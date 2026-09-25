
#include "AmbientDirector.h"

#include "AmbientEncounterDefinitionData.h"
#include "AmbientEncounterPoint.h"
#include "AmbientEncounterRuntimeInterface.h"
#include "AmbientPlaceholderEncounter.h"
#include "AmbientRegionVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "TimerManager.h"

AAmbientDirector::AAmbientDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	PrototypeEncounterDefinition.EncounterClass = AAmbientPlaceholderEncounter::StaticClass();
}

void AAmbientDirector::SetTraversalState(EAmbientTraversalState NewTraversalState, AActor* NewTraversalActor)
{
	if (NewTraversalState == EAmbientTraversalState::Mounted && !IsValid(NewTraversalActor))
	{
		ensureMsgf(false,
			TEXT(
				"Mounted traversal requires "
				"a valid traversal actor"));

		NewTraversalState = EAmbientTraversalState::OnFoot;
		NewTraversalActor = nullptr;
	}
	AActor* SanitizedTraversalActor = 
		NewTraversalState == EAmbientTraversalState::Mounted 
		? NewTraversalActor 
		: nullptr;

	const bool bStateUnchanged = 
		TraversalState == NewTraversalState && 
		TraversalActor.Get() == SanitizedTraversalActor;

	if (bStateUnchanged)
	{
		return;
	}

	TraversalState = NewTraversalState;
	TraversalActor = SanitizedTraversalActor;

	SyncTraversalWorldState();

	if (EncounterRuntimeState ==  EAmbientEncounterRuntimeState::Waiting &&
		bHasRuntimeEncounterDefinition)
	{
		FString TraversalMatchReason;

		if (!DoesDefinitionMatchTraversal(RuntimeEncounterDefinition, TraversalMatchReason))
		{
			RemoveWaitingPrototypeEncounter(
				FString::Printf(
					TEXT("Waiting encounter removed because traversal changed: %s"),
					*TraversalMatchReason
				)
			);
		}
	}

	if (HasActorBegunPlay())
	{
		UpdateWorldState();
	}
}

bool AAmbientDirector::RequestActiveEncounterResolution(AActor* RequestingEncounter, const FString& FinishReason)
{
	if (!IsValid(RequestingEncounter))
	{
		return false;
	}

	if (EncounterRuntimeState != EAmbientEncounterRuntimeState::Active)
	{
		return false;
	}

	if (!IsValid(ActivePrototypeEncounter) || 
		ActivePrototypeEncounter.Get() != RequestingEncounter)
	{
		return false;
	}

	FString SafeFinishReason = FinishReason;
	SafeFinishReason.TrimStartAndEndInline();

	if (SafeFinishReason.IsEmpty())
	{
		SafeFinishReason = TEXT("Resolved by active encounter");
	}

	BeginPrototypeCleanup(SafeFinishReason);

	return true;
}

bool AAmbientDirector::IsEncounterRuntimeClear() const
{
	return EncounterRuntimeState == EAmbientEncounterRuntimeState::Waiting &&
		!IsValid(ActivePrototypeEncounter.Get()) &&
		!bHasRuntimeEncounterDefinition &&
		GetCurrentEncounterBudgetUse() == 0;
}

void AAmbientDirector::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoLoadDirectorSaveOnBeginPlay)
	{
		LoadDirectorStateFromSlot();
	}

	UpdateWorldState();

	if (UWorld* World = GetWorld())
	{
		const float SafeUpdateInterval = FMath::Max(0.1f, UpdateInterval);

		World->GetTimerManager().SetTimer(
			WorldStateTimerHandle,
			this,
			&AAmbientDirector::UpdateWorldState,
			SafeUpdateInterval,
			true
		);
	}
}

void AAmbientDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyPrototypeEncounter();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorldStateTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AAmbientDirector::UpdateWorldState()
{
	if (bIsUpdatingWorldState)
	{
		return;
	}

	// Prevent callbacks from restarting the update while it is in progress.
	TGuardValue<bool> UpdateGuard(bIsUpdatingWorldState, true);

	// Temporary re-entry test. Remove after verification.
	UpdateWorldState();

	TRACE_CPUPROFILER_EVENT_SCOPE(AED_UpdateWorldState);

	CurrentWorldState = FAmbientWorldState();
	CurrentWorldState.CurrentEncounterBudgetUse = GetCurrentEncounterBudgetUse();
	CurrentWorldState.MaxEncounterBudget = MaxSimultaneousPrototypeEncounters;

	SyncTraversalWorldState();

	CurrentRegion = nullptr;

	// 후보 평가를 건너뛰어도 이전 선택 결과가 남지 않도록 먼저 초기화.
	SelectedEncounterPoint = nullptr;
	SelectedEncounterDefinitionAsset = nullptr;

	bHasSelectedEncounterDefinition = false;
	SelectedEncounterDefinition = FAmbientEncounterDefinition();
	SelectedEncounterScore = 0.0f;
	SelectedEncounterReason = TEXT("No encounter selected");
	LastSelectionDebugEntries.Reset();

	bHasSelectedEncounterSpawnTransform = false;
	SelectedEncounterSpawnTransform = FTransform::Identity;
	SelectedEncounterLocationReason = TEXT("No selected encounter spawn transform");

	UWorld* World = GetWorld();
	if (!World)
	{
		if (bPrintDebug)
		{
			PrintWorldStateDebug();
		}

		return;
	}

	CurrentWorldState.GameTimeSeconds = World->GetTimeSeconds();
	CurrentWorldState.GlobalPacingRemainingSeconds = GetGlobalPacingRemaining();

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	CurrentWorldState.bHasPlayerPawn = IsValid(PlayerPawn);

	if (CurrentWorldState.bHasPlayerPawn)
	{
		CurrentWorldState.PlayerLocation = PlayerPawn->GetActorLocation();
		CurrentWorldState.PlayerSpeed2D	= PlayerPawn->GetVelocity().Size2D();

		UpdateCurrentRegion(CurrentWorldState.PlayerLocation);
		SelectEncounterDefinitionAndPoint();
		EvaluatePrototypeEncounterCondition();
	}
	else
	{
		CurrentWorldState.EncounterBlockReason = TEXT("No player pawn");
		CurrentWorldState.EncounterRuntimeReason = TEXT("No player pawn");
	}

	// Pawn이 없어도 Cleanup과 Cooldown의 만료는 처리한다.
	UpdatePrototypeEncounter();
	SyncPrototypeRuntimeWorldState();
	
	if (ShouldDrawPlacementDebug())
	{
		if (bDrawRegionDebug)
		{
			DrawRegionDebug();
		}
	}

	if (ShouldDrawSelectionDebug())
	{
		if (bDrawEncounterPointDebug)
		{
			DrawEncounterPointDebug();
		}

		if (bDrawSelectedEncounterLocationDebug)
		{
			DrawSelectedEncounterLocationDebug();
		}
	}

	if (ShouldDrawRuntimeDebug())
	{
		if (bDrawEncounterRuntimeDebug)
		{
			DrawEncounterRuntimeDebug();
		}
	}

	if (bPrintDebug)
	{
		if (bPrintCompactDebugDashboard)
		{
			PrintDirectorDashboardDebug();
		}

		if (bPrintDetailedDebugLines)
		{
			PrintWorldStateDebug();
			PrintSelectionDebug();
			PrintEncounterDebug();
			PrintEncounterHistoryDebug();
		}
	}
}

void AAmbientDirector::UpdateCurrentRegion(const FVector& QueryLocation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AED_UpdateCurrentRegion);

	CurrentRegion = nullptr;
	CurrentWorldState.bHasCurrentRegion = false;
	CurrentWorldState.CurrentRegionName = NAME_None;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AAmbientRegionVolume* BestRegion = nullptr;

	for (TActorIterator<AAmbientRegionVolume> RegionIt(World); RegionIt; ++RegionIt)
	{
		AAmbientRegionVolume* Region = *RegionIt;

		if (!IsValid(Region) || !Region->ContainsWorldLocation(QueryLocation))
		{
			continue;
		}

		if (!BestRegion || Region->IsPreferredOver(*BestRegion))
		{
			BestRegion = Region;
		}
	}

	if (!BestRegion)
	{
		return;
	}

	CurrentRegion = BestRegion;
	CurrentWorldState.bHasCurrentRegion = true;
	CurrentWorldState.CurrentRegionName = BestRegion->GetRegionName();

	const FGameplayTag RegionTag = BestRegion->GetRegionTag();
	if (RegionTag.IsValid())
	{
		CurrentWorldState.WorldTags.AddTag(RegionTag);
	}
}

bool AAmbientDirector::ShouldDrawPlacementDebug() const
{
	return DebugVisualizationMode == EAmbientDirectorDebugVisualizationMode::Placement ||
		DebugVisualizationMode == EAmbientDirectorDebugVisualizationMode::Full;
}

bool AAmbientDirector::ShouldDrawSelectionDebug() const
{
	return DebugVisualizationMode == EAmbientDirectorDebugVisualizationMode::Selection ||
		DebugVisualizationMode == EAmbientDirectorDebugVisualizationMode::Full;
}

bool AAmbientDirector::ShouldDrawRuntimeDebug() const
{
	return DebugVisualizationMode == EAmbientDirectorDebugVisualizationMode::Runtime ||
		DebugVisualizationMode == EAmbientDirectorDebugVisualizationMode::Full;
}

void AAmbientDirector::EvaluatePrototypeEncounterCondition()
{
	CurrentWorldState.bEncounterConditionsMet = false;
	CurrentWorldState.EncounterBlockReason = TEXT("Prototype condition not evaluated");

	if (!bHasSelectedEncounterDefinition && !bHasRuntimeEncounterDefinition)
	{
		CurrentWorldState.EncounterBlockReason =
			TEXT("No selected encounter definition");
		return;
	}

	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();

	if (!Definition.bEnabled)
	{
		CurrentWorldState.EncounterBlockReason =
			TEXT("Encounter definition is disabled");
		return;
	}

	if (!CurrentWorldState.bHasPlayerPawn)
	{
		CurrentWorldState.EncounterBlockReason = TEXT("No player pawn");
		return;
	}

	if (!CurrentWorldState.bHasSelectedEncounterLocation && !IsValid(ActivePrototypeEncounter))
	{
		CurrentWorldState.EncounterBlockReason =
			CurrentWorldState.SelectedEncounterLocationReason;
		return;
	}

	if (CurrentWorldState.PlayerSpeed2D > Definition.MaxPlayerSpeed2D)
	{
		CurrentWorldState.EncounterBlockReason = FString::Printf(
			TEXT("Player moving too fast: %.0f > %.0f cm/s"),
			CurrentWorldState.PlayerSpeed2D,
			Definition.MaxPlayerSpeed2D
		);
		return;
	}

	CurrentWorldState.bEncounterConditionsMet = true;
	CurrentWorldState.EncounterBlockReason =
		TEXT("Definition condition passed");
}

void AAmbientDirector::UpdatePrototypeEncounter()
{
	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();
	// 현재 게임 시간. Cleanup / Cooldown 종료 시점 계산에 사용
	const float Now = CurrentWorldState.GameTimeSeconds;

	switch (EncounterRuntimeState)
	{
	case EAmbientEncounterRuntimeState::Waiting:
	{
		// Waiting 상태:
		// 아직 플레이어가 Encounter에 진입X
		// Enocunter 액터가 없으면 조건 확인 후 생성 (AmbientPlaceholderEncounter)
		// Enocunter 액가 있으면 플레이어 접근 여부 검사
		if (!IsValid(ActivePrototypeEncounter))
		{
			if (!CurrentWorldState.bEncounterConditionsMet)
			{
				// Encounter 액터가 없는 상태
				// 현재 Encounter 발생 조건을 만족하지 않으면 정리 후 대기
				DestroyPrototypeEncounter();
				CurrentWorldState.EncounterRuntimeReason =
					CurrentWorldState.EncounterBlockReason;
				return;
			}

			const EAmbientEncounterPreparationResult PreparationResult = TrySpawnOrUpdatePrototypeEncounter();

			if (PreparationResult != EAmbientEncounterPreparationResult::Ready)
			{
				if (PreparationResult == EAmbientEncounterPreparationResult::Failed)
				{
					CurrentWorldState.EncounterRuntimeReason = CurrentWorldState.EncounterBlockReason;
				}
				else if (EncounterRuntimeState == EAmbientEncounterRuntimeState::Waiting &&
					bHasRuntimeEncounterDefinition && !IsValid(ActivePrototypeEncounter))
				{
					// Cancel orphaned preparation data without recording a completion.
					RemoveWaitingPrototypeEncounter(TEXT("Encounter actor became invalid during preparation"));
				}

				// Preserve any state transition or replacement made by the callback.
				return;
			}
		}
		else
		{
			const bool bHasRequiredRegion =
				Definition.RequiredRegionTag.IsValid() ||
				Definition.RequiredRegionName != NAME_None;

			if (bHasRequiredRegion)
			{
				// 플레이어가 필수 Region을 벗어난 경우
				// Waiting 중이던 Encounter 제거
				bool bWrongRegion = false;

				if (Definition.RequiredRegionTag.IsValid())
				{
					bWrongRegion =
						!CurrentWorldState.WorldTags.HasTagExact(Definition.RequiredRegionTag);
				}
				else if (Definition.RequiredRegionName != NAME_None)
				{
					bWrongRegion =
						!CurrentWorldState.bHasCurrentRegion ||
						CurrentWorldState.CurrentRegionName != Definition.RequiredRegionName;
				}

				if (bWrongRegion)
				{
					RemoveWaitingPrototypeEncounter(
						TEXT("Waiting encounter removed because player left required region")
					);

					return;
				}
			}
		}

		float DistanceToEncounter = 0.0f;

		if (!TryGetDistanceFromPlayerToPrototypeEncounter(DistanceToEncounter))
		{
			CurrentWorldState.EncounterRuntimeReason =
				TEXT("Waiting paused because player pawn is unavailable");
			break;
		}

		if (Definition.WaitingAbandonDistance > 0.0f)
		{
			// 플레이어가 Abandon 거리 밖으로 나가면 Waiting 제거
			const float MaximumInitialSpawnDistance = FMath::Min(Definition.SpawnSearchRadius, MaximumSpawnDistance);
			const float SafeWaitingAbandonDistance = FMath::Max(Definition.WaitingAbandonDistance, MaximumInitialSpawnDistance + 100.f);

			if (DistanceToEncounter >= SafeWaitingAbandonDistance)
			{
				RemoveWaitingPrototypeEncounter(
					FString::Printf(
						TEXT("Waiting encounter removed because player left abandon distance (%.0f >= %.0f)"),
						DistanceToEncounter,
						SafeWaitingAbandonDistance
					)
				);

				return;
			}
		}

		// Encounter는 준비됨
		// 플레이어가 Engage 거리 안으로 들어오면 Encounter 시작
		CurrentWorldState.EncounterRuntimeReason = TEXT("Waiting for player approach");
		if (DistanceToEncounter <= Definition.PlayerEngageDistance)
		{
			StartPrototypeEncounter();
		}

		break;
	}

	case EAmbientEncounterRuntimeState::Active:
	{
		// Active 상태:
		// 플레이어가 Encounter에 참여 중인 상태
		// Encounter 액터 유효성 검사
		// 플레이어가 Encounter 반경을 벗어났는지 검사
		if (!IsValid(ActivePrototypeEncounter))
		{
			// Active 상태에서 Encounter 액터가 유효하지 않으면 Cleanup 진입
			BeginPrototypeCleanup(TEXT("Active encounter actor became invalid"));
			break;
		}

		float DistanceToEncounter = 0.0f;

		if (!TryGetDistanceFromPlayerToPrototypeEncounter(DistanceToEncounter))
		{
			CurrentWorldState.EncounterRuntimeReason =
				TEXT("Active encounter paused because player pawn is unavailable");
			break;
		}

		// 플레이어가 Leave 거리 밖으로 나가면 Cleanup 진입
		if (DistanceToEncounter >= Definition.PlayerLeaveDistance)
		{
			BeginPrototypeCleanup(TEXT("Player left encounter radius"));
			break;
		}

		// Encounter가 정상 진행 중인 상태
		CurrentWorldState.EncounterRuntimeReason = TEXT("Player is involved in encounter");

		break;
	}

	case EAmbientEncounterRuntimeState::Cleanup:
	{
		// Cleanup 상태:
		// Encounter 종료 후 정리 중인 상태
		// Cleanup 시간이 끝나면 Encounter 완전 종료
		const float Remaining = PrototypeCleanupEndTimeSeconds - Now;

		// Cleanup 시간이 끝난 경우
		if (Remaining <= 0.0f)
		{
			const FString FinishReason = PendingPrototypeFinishReason.IsEmpty()
				? TEXT("Cleanup finished")
				: PendingPrototypeFinishReason;

			FinishPrototypeEncounter(FinishReason);
			break;
		}

		// Cleanup 진행 중인 이유 기록
		CurrentWorldState.EncounterRuntimeReason = FString::Printf(
			TEXT("Cleaning up: %s"),
			*PendingPrototypeFinishReason
		);

		break;
	}

	case EAmbientEncounterRuntimeState::Cooldown:
	{
		// Cooldown 상태:
		// Encounter 종료 후 재생성 방지 대기 상태
		// Cooldown 중에는 Encounter 액터가 남아 있지 않도록 제거
		DestroyPrototypeEncounter();

		const float Remaining = PrototypeCooldownEndTimeSeconds - Now;

		// Cooldown 시간이 끝나면 Waiting 상태로 복귀
		if (Remaining <= 0.0f)
		{
			EncounterRuntimeState = EAmbientEncounterRuntimeState::Waiting;
			PrototypeCooldownEndTimeSeconds = 0.0f;

			RuntimeEncounterDefinition = FAmbientEncounterDefinition();
			bHasRuntimeEncounterDefinition = false;

			CurrentWorldState.EncounterRuntimeReason =
				TEXT(
					"Cooldown complete; next update will "
					"perform a fresh selection"
				);

			if (bAutoSaveDirectorStateOnRuntimeChange)
			{
				SaveDirectorStateToSlot();
			}

			break;
		}

		// 아직 Cooldown 대기 중
		CurrentWorldState.EncounterRuntimeReason = TEXT("Cooldown remaining");

		break;
	}

	default:
	{
		// 알 수 없는 상태
		// 안전하게 Waiting 상태로 복구

		EncounterRuntimeState = EAmbientEncounterRuntimeState::Waiting;
		CurrentWorldState.EncounterRuntimeReason = TEXT("Unknown state corrected to Waiting");
		break;
	}
	}
}

const FAmbientEncounterDefinition& AAmbientDirector::GetPrototypeEncounterDefinition() const
{
	// Spawn 이후 상태에서는 Spawn 시점에 고정된 Runtime 정의값 사용
	if (bHasRuntimeEncounterDefinition)
	{
		return RuntimeEncounterDefinition;
	}

	// 후보 평가 중에는 현재 선택된 Encounter 정의값 사용
	if (bHasSelectedEncounterDefinition)
	{
		return SelectedEncounterDefinition;
	}

	// 기존 단일 Encounter 정의 에셋 fallback
	if (IsValid(PrototypeEncounterDefinitionAsset))
	{
		return PrototypeEncounterDefinitionAsset->Definition;
	}

	// 최종 인라인 fallback 정의값
	return PrototypeEncounterDefinition;
}

EAmbientEncounterPreparationResult AAmbientDirector::TrySpawnOrUpdatePrototypeEncounter()
{
	if (EncounterRuntimeState != EAmbientEncounterRuntimeState::Waiting)
	{
		return EAmbientEncounterPreparationResult::Interrupted;
	}

	if (!bHasSelectedEncounterDefinition && !bHasRuntimeEncounterDefinition)
	{
		CurrentWorldState.bEncounterConditionsMet = false;
		CurrentWorldState.EncounterBlockReason = TEXT("No selected encounter definition");
		return EAmbientEncounterPreparationResult::Failed;
	}

	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();

	if (!Definition.EncounterClass)
	{
		CurrentWorldState.bEncounterConditionsMet = false;
		CurrentWorldState.EncounterBlockReason = TEXT("EncounterClass is not assigned in definition");
		return EAmbientEncounterPreparationResult::Failed;
	}

	if (!Definition.EncounterClass->ImplementsInterface(UAmbientEncounterRuntimeInterface::StaticClass()))
	{
		CurrentWorldState.bEncounterConditionsMet = false;
		CurrentWorldState.EncounterBlockReason = FString::Printf(
			TEXT("EncounterClass %s does not implement AmbientEncounterRuntimeInterface"),
			*GetNameSafe(Definition.EncounterClass.Get()));
		return EAmbientEncounterPreparationResult::Failed;
	}

	const bool bNeedsSpawn = !IsValid(ActivePrototypeEncounter);

	if (bNeedsSpawn && !bHasSelectedEncounterSpawnTransform)
	{
		CurrentWorldState.bEncounterConditionsMet = false;
		CurrentWorldState.EncounterBlockReason = TEXT("Selected encounter spawn transform is invalid");
		return EAmbientEncounterPreparationResult::Failed;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		CurrentWorldState.bEncounterConditionsMet = false;
		CurrentWorldState.EncounterBlockReason = TEXT("No world");
		return EAmbientEncounterPreparationResult::Failed;
	}

	TWeakObjectPtr<AActor> PreparedEncounter(ActivePrototypeEncounter.Get());

	if (bNeedsSpawn)
	{
		const FAmbientEncounterDefinition PendingRuntimeDefinition = Definition;
		const FTransform SpawnTransform = SelectedEncounterSpawnTransform;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* const SpawnedEncounter = World->SpawnActor<AActor>(
			PendingRuntimeDefinition.EncounterClass, SpawnTransform, SpawnParams);

		if (!IsValid(SpawnedEncounter))
		{
			ActivePrototypeEncounter = nullptr;
			RuntimeEncounterDefinition = FAmbientEncounterDefinition();
			bHasRuntimeEncounterDefinition = false;
			CurrentWorldState.bEncounterConditionsMet = false;
			CurrentWorldState.EncounterBlockReason = TEXT("Spawn failed");
			return EAmbientEncounterPreparationResult::Failed;
		}

		ActivePrototypeEncounter = SpawnedEncounter;
		PreparedEncounter = SpawnedEncounter;
		RuntimeEncounterDefinition = PendingRuntimeDefinition;
		bHasRuntimeEncounterDefinition = true;

		RuntimeEncounterRegionName = CurrentWorldState.CurrentRegionName;
		RuntimeEncounterPointName = IsValid(SelectedEncounterPoint)
			? SelectedEncounterPoint->GetPointName()
			: FName(TEXT("Location.EQS"));
		RuntimeEncounterLocation = SpawnTransform.GetLocation();
		RuntimeEncounterLocationSource = CurrentWorldState.SelectedEncounterLocationSource.IsEmpty()
			? TEXT("Unknown")
			: CurrentWorldState.SelectedEncounterLocationSource;

		FAmbientEncounterRuntimeContext RuntimeContext;
		RuntimeContext.DirectorActor = this;
		RuntimeContext.EncounterId = RuntimeEncounterDefinition.EncounterId;
		RuntimeContext.RegionName = RuntimeEncounterRegionName;
		RuntimeContext.SourcePointName = RuntimeEncounterPointName;
		RuntimeContext.SpawnLocation = SpawnTransform.GetLocation();
		RuntimeContext.EncounterTags = RuntimeEncounterDefinition.EncounterTags;

		TRACE_BOOKMARK(TEXT("AED.Init | Encounter=%s | Class=%s"),
			*RuntimeContext.EncounterId.ToString(), *GetNameSafe(SpawnedEncounter->GetClass()));

		IAmbientEncounterRuntimeInterface::Execute_InitializeAmbientEncounter(SpawnedEncounter, RuntimeContext);

		// Initialization may remove the actor, replace it, or advance the state.
		AActor* const InitializedEncounter = PreparedEncounter.Get();
		if (!InitializedEncounter || ActivePrototypeEncounter.Get() != InitializedEncounter ||
			EncounterRuntimeState != EAmbientEncounterRuntimeState::Waiting || !bHasRuntimeEncounterDefinition)
		{
			return EAmbientEncounterPreparationResult::Interrupted;
		}

		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterWaiting(InitializedEncounter);
	}
	else if (bHasSelectedEncounterSpawnTransform && IsValid(SelectedEncounterPoint))
	{
		// Preserve the existing authored-point update behavior.
		if (AActor* EncounterActor = PreparedEncounter.Get())
		{
			EncounterActor->SetActorTransform(SelectedEncounterSpawnTransform);
		}
	}

	// Only the same valid Waiting encounter counts as a completed preparation.
	AActor* const EncounterActor = PreparedEncounter.Get();
	if (!EncounterActor || ActivePrototypeEncounter.Get() != EncounterActor ||
		EncounterRuntimeState != EAmbientEncounterRuntimeState::Waiting || !bHasRuntimeEncounterDefinition)
	{
		return EAmbientEncounterPreparationResult::Interrupted;
	}

	if (bNeedsSpawn && bAutoSaveDirectorStateOnRuntimeChange)
	{
		SaveDirectorStateToSlot();
	}

	return EAmbientEncounterPreparationResult::Ready;
}

void AAmbientDirector::StartPrototypeEncounter()
{
	// 이미 다른 단계라면 활성화 처리를 반복하지 않는다.
	if (EncounterRuntimeState != EAmbientEncounterRuntimeState::Waiting)
	{
		return;
	}

	AActor* const EncounterActor = ActivePrototypeEncounter.Get();

	if (!IsValid(EncounterActor))
	{
		CurrentWorldState.EncounterRuntimeReason = TEXT("Cannot start encounter because actor is missing");
		return;
	}

	const float StartTimeSeconds = CurrentWorldState.GameTimeSeconds;

	EncounterRuntimeState = EAmbientEncounterRuntimeState::Active;
	RuntimeEncounterStartedAtTimeSeconds = StartTimeSeconds;

	if (RuntimeEncounterRegionName == NAME_None)
	{
		RuntimeEncounterRegionName = CurrentWorldState.CurrentRegionName;
	}

	if (RuntimeEncounterPointName == NAME_None)
	{
		RuntimeEncounterPointName = CurrentWorldState.SelectedEncounterPointName;
	}

	++EncounterStartCount;
	LastAnyEncounterStartTimeSeconds = StartTimeSeconds;

	CurrentWorldState.EncounterRuntimeReason = TEXT("Player is involved in encounter");

	// 콜백에서 Director에 종료를 요청할 수 있으므로,
	// 상태와 시작 관련 기록을 모두 확정한 뒤 알린다.
	if (EncounterActor->GetClass()->ImplementsInterface(UAmbientEncounterRuntimeInterface::StaticClass()))
	{
		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterActivated(EncounterActor);
	}

	// 콜백 안에서 Cleanup이나 Cooldown으로 넘어갔을 수 있다.
	// 여기서는 Active용 상태나 설명을 다시 기록하지 않는다.
	if (bAutoSaveDirectorStateOnRuntimeChange)
	{
		SaveDirectorStateToSlot();
	}
}

void AAmbientDirector::RemoveWaitingPrototypeEncounter(const FString& Reason)
{
	if (EncounterRuntimeState != EAmbientEncounterRuntimeState::Waiting)
	{
		return;
	}

	// Preserve the reason before resetting any runtime data it may reference.
	CurrentWorldState.EncounterRuntimeReason = Reason;

	// Cancel the preparation without changing completion history or pacing.
	RuntimeEncounterDefinition = FAmbientEncounterDefinition();
	bHasRuntimeEncounterDefinition = false;
	RuntimeEncounterRegionName = NAME_None;
	RuntimeEncounterPointName = NAME_None;
	RuntimeEncounterStartedAtTimeSeconds = 0.0f;
	RuntimeEncounterLocation = FVector::ZeroVector;
	RuntimeEncounterLocationSource = TEXT("Unknown");

	PrototypeCleanupEndTimeSeconds = 0.0f;
	PrototypeCooldownEndTimeSeconds = 0.0f;
	PendingPrototypeFinishReason = TEXT("None");
	CurrentWorldState.DistanceToEncounter = 0.0f;

	// Runtime data must be cleared before destruction callbacks can re-enter.
	DestroyPrototypeEncounter();

	if (bAutoSaveDirectorStateOnRuntimeChange)
	{
		SaveDirectorStateToSlot();
	}
}

void AAmbientDirector::BeginPrototypeCleanup(const FString& Reason)
{
	if (EncounterRuntimeState != EAmbientEncounterRuntimeState::Active)
	{
		return;
	}

	const float CleanupStartTimeSeconds = CurrentWorldState.GameTimeSeconds;
	const float SafeCleanupDelay = FMath::Max(0.0f, GetPrototypeEncounterDefinition().CleanupDelaySeconds);

	AActor* const EncounterActor = ActivePrototypeEncounter.Get();

	// Complete the cleanup state before callbacks can re-enter the Director.
	PendingPrototypeFinishReason = Reason;
	PrototypeCleanupEndTimeSeconds = CleanupStartTimeSeconds + SafeCleanupDelay;

	CurrentWorldState.EncounterRuntimeReason = FString::Printf(TEXT("Cleaning up: %s"), *Reason);

	EncounterRuntimeState = EAmbientEncounterRuntimeState::Cleanup;

	if (IsValid(EncounterActor) && EncounterActor->GetClass()->ImplementsInterface(UAmbientEncounterRuntimeInterface::StaticClass()))
	{
		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterCleanup(EncounterActor, Reason);
	}

	// The callback may have already advanced the runtime state.
	if (EncounterRuntimeState != EAmbientEncounterRuntimeState::Cleanup)
	{
		return;
	}

	if (SafeCleanupDelay <= 0.0f)
	{
		FinishPrototypeEncounter(Reason);
		return;
	}

	if (bAutoSaveDirectorStateOnRuntimeChange)
	{
		SaveDirectorStateToSlot();
	}
}

void AAmbientDirector::FinishPrototypeEncounter(const FString& Reason)
{
	if (EncounterRuntimeState != EAmbientEncounterRuntimeState::Cleanup)
	{
		return;
	}

	const float FinishTimeSeconds = CurrentWorldState.GameTimeSeconds;
	const FString FinishReason = Reason;
	const TWeakObjectPtr<AActor> FinishedEncounter(ActivePrototypeEncounter.Get());
	const FAmbientEncounterDefinition& FinishedDefinition = GetPrototypeEncounterDefinition();

	// Record completion while the finished encounter's runtime data is intact.
	if (FinishedDefinition.bOneShot && FinishedDefinition.EncounterId != NAME_None)
	{
		CompletedEncounterIds.Add(FinishedDefinition.EncounterId);
	}

	AddPrototypeHistoryEntry(FinishTimeSeconds, FinishReason);
	++EncounterFinishCount;

	// Complete the state transition before notifying external code.
	ActivePrototypeEncounter = nullptr;
	RuntimeEncounterStartedAtTimeSeconds = 0.0f;
	RuntimeEncounterRegionName = NAME_None;
	RuntimeEncounterPointName = NAME_None;
	RuntimeEncounterLocation = FVector::ZeroVector;
	RuntimeEncounterLocationSource = TEXT("Unknown");
	PendingPrototypeFinishReason = TEXT("None");
	PrototypeCleanupEndTimeSeconds = 0.0f;

	StartPrototypeCooldown();
	CurrentWorldState.EncounterRuntimeReason = FString::Printf(TEXT("Finished encounter: %s"), *FinishReason);

	if (AActor* EncounterActor = FinishedEncounter.Get())
	{
		if (EncounterActor->GetClass()->ImplementsInterface(UAmbientEncounterRuntimeInterface::StaticClass()))
		{
			IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterFinished(EncounterActor, FinishReason);
		}
	}

	// The callback may have destroyed the actor, so resolve it again.
	if (AActor* EncounterActor = FinishedEncounter.Get())
	{
		EncounterActor->Destroy();
	}

	if (bAutoSaveDirectorStateOnRuntimeChange)
	{
		SaveDirectorStateToSlot();
	}
}

void AAmbientDirector::StartPrototypeCooldown()
{
	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();

	const float Now = CurrentWorldState.GameTimeSeconds;

	const float SafeCooldownDuration =
		FMath::Max(0.0f, Definition.CooldownDurationSeconds);

	if (SafeCooldownDuration <= 0.0f)
	{
		PrototypeCooldownEndTimeSeconds = 0.0f;
		EncounterRuntimeState = EAmbientEncounterRuntimeState::Waiting;

		RuntimeEncounterDefinition = FAmbientEncounterDefinition();
		bHasRuntimeEncounterDefinition = false;

		return;
	}

	PrototypeCooldownEndTimeSeconds = Now + SafeCooldownDuration;
	EncounterRuntimeState = EAmbientEncounterRuntimeState::Cooldown;
}

void AAmbientDirector::DestroyPrototypeEncounter()
{
	AActor* const EncounterToDestroy = ActivePrototypeEncounter.Get();

	// Detach before destruction callbacks can re-enter the Director.
	ActivePrototypeEncounter = nullptr;

	if (IsValid(EncounterToDestroy))
	{
		EncounterToDestroy->Destroy();
	}
}

void AAmbientDirector::AddPrototypeHistoryEntry(float FinishedAtTimeSeconds, const FString& FinishReason)
{
	FAmbientEncounterHistoryEntry NewEntry;

	NewEntry.EncounterId			= GetPrototypeEncounterDefinition().EncounterId;
	NewEntry.RegionName				= RuntimeEncounterRegionName;
	NewEntry.SourcePointName		= RuntimeEncounterPointName;
	NewEntry.EncounterLocation		= RuntimeEncounterLocation;
	NewEntry.LocationSource			= RuntimeEncounterLocationSource;
	NewEntry.StartedAtTimeSeconds	= RuntimeEncounterStartedAtTimeSeconds;
	NewEntry.FinishedAtTimeSeconds	= FinishedAtTimeSeconds;
	NewEntry.FinishReason			= FinishReason;

	PrototypeEncounterHistory.Insert(NewEntry, 0);

	const int32 SafeMaxHistoryEntries = FMath::Max(1, MaxHistoryEntries);

	while (PrototypeEncounterHistory.Num() > SafeMaxHistoryEntries)
	{
		PrototypeEncounterHistory.RemoveAt(PrototypeEncounterHistory.Num() - 1);
	}
}

void AAmbientDirector::SyncPrototypeRuntimeWorldState()
{
	const float Now = CurrentWorldState.GameTimeSeconds;

	CurrentWorldState.DistanceToEncounter = 0.0f;
	TryGetDistanceFromPlayerToPrototypeEncounter(CurrentWorldState.DistanceToEncounter);

	CurrentWorldState.EncounterCleanupRemainingSeconds		= 0.0f;
	CurrentWorldState.EncounterCooldownRemainingSeconds	= 0.0f;

	if (EncounterRuntimeState == EAmbientEncounterRuntimeState::Cleanup)
	{
		CurrentWorldState.EncounterCleanupRemainingSeconds = FMath::Max(0.0f, PrototypeCleanupEndTimeSeconds - Now);
	}

	if (EncounterRuntimeState == EAmbientEncounterRuntimeState::Cooldown)
	{
		CurrentWorldState.EncounterCooldownRemainingSeconds = FMath::Max(0.0f, PrototypeCooldownEndTimeSeconds - Now);
	}
}

bool AAmbientDirector::TryGetDistanceFromPlayerToPrototypeEncounter(float& OutDistance) const
{
	OutDistance = 0.0f;
	if (!CurrentWorldState.bHasPlayerPawn || !IsValid(ActivePrototypeEncounter))
	{
		return false;
	}

	OutDistance = FVector::Dist2D(CurrentWorldState.PlayerLocation, ActivePrototypeEncounter->GetActorLocation());

	return true;
}

FString AAmbientDirector::GetPrototypeRuntimeStateString() const
{
	switch (EncounterRuntimeState)
	{
	case EAmbientEncounterRuntimeState::Waiting:
		return TEXT("Waiting");

	case EAmbientEncounterRuntimeState::Active:
		return TEXT("Active");

	case EAmbientEncounterRuntimeState::Cleanup:
		return TEXT("Cleanup");

	case EAmbientEncounterRuntimeState::Cooldown:
		return TEXT("Cooldown");

	default:
		return TEXT("Unknown");
	}
}


void AAmbientDirector::SyncTraversalWorldState()
{
	CurrentWorldState.TraversalState = TraversalState;

	const FGameplayTag OnFootTag = GetTraversalGameplayTag(EAmbientTraversalState::OnFoot);
	const FGameplayTag MountedTag = GetTraversalGameplayTag(EAmbientTraversalState::Mounted);

	if (OnFootTag.IsValid())
	{
		CurrentWorldState.WorldTags.RemoveTag(OnFootTag);
	}

	if (MountedTag.IsValid())
	{
		CurrentWorldState.WorldTags.RemoveTag(MountedTag);
	}

	const FGameplayTag CurrentTraversalTag = GetTraversalGameplayTag(TraversalState);

	if (CurrentTraversalTag.IsValid())
	{
		CurrentWorldState.WorldTags.AddTag(CurrentTraversalTag);
	}
}

bool AAmbientDirector::DoesDefinitionMatchTraversal(const FAmbientEncounterDefinition& Definition, FString& OutReason) const
{
	const FGameplayTag OnFootTag = GetTraversalGameplayTag(EAmbientTraversalState::OnFoot);
	const FGameplayTag MountedTag = GetTraversalGameplayTag(EAmbientTraversalState::Mounted);

	if (OnFootTag.IsValid() &&
		Definition.RequiredWorldTags.HasTagExact(OnFootTag) &&
		TraversalState != EAmbientTraversalState::OnFoot)
	{
		OutReason = TEXT("Requires Traversal.OnFoot");

		return false;
	}

	if (MountedTag.IsValid() &&
		Definition.RequiredWorldTags.HasTagExact(MountedTag) &&
		TraversalState != EAmbientTraversalState::Mounted)
	{
		OutReason = TEXT("Requires Traversal.Mounted");

		return false;
	}

	OutReason = TEXT("Traversal requirement matched");
	return true;
}

FString AAmbientDirector::GetTraversalStateString(EAmbientTraversalState State)
{
	switch (State)
	{
	case EAmbientTraversalState::OnFoot:
		return TEXT("OnFoot");

	case EAmbientTraversalState::Mounted:
		return TEXT("Mounted");

	default:
		return TEXT("Unknown");
	}
}

FGameplayTag AAmbientDirector::GetTraversalGameplayTag(EAmbientTraversalState State)
{
	switch (State)
	{
	case EAmbientTraversalState::OnFoot:
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Traversal.OnFoot")), false);

	case EAmbientTraversalState::Mounted:
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Traversal.Mounted")), false);

	default:
		return FGameplayTag();
	}
}
