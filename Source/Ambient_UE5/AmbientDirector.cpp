
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
#include "TimerManager.h"

AAmbientDirector::AAmbientDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	PrototypeEncounterDefinition.EncounterClass = AAmbientPlaceholderEncounter::StaticClass();
}

void AAmbientDirector::SetTraversalState(EAmbientTraversalState NewTraversalState, AActor* NewTraversalActor)
{
	if (NewTraversalState == EAmbientTraversalState::Mounted &&
		!IsValid(NewTraversalActor))
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

	if (PrototypeEncounterState ==  EAmbientEncounterRuntimeState::Waiting &&
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

	if (PrototypeEncounterState != EAmbientEncounterRuntimeState::Active)
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
	return PrototypeEncounterState == EAmbientEncounterRuntimeState::Waiting &&
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
	CurrentWorldState									= FAmbientWorldState();
	CurrentWorldState.CurrentEncounterBudgetUse			= GetCurrentEncounterBudgetUse();
	CurrentWorldState.MaxEncounterBudget				= MaxSimultaneousPrototypeEncounters;
	CurrentWorldState.GlobalPacingRemaining				= GetGlobalPacingRemaining();
	CurrentWorldState.NearestRecentEncounterDistance	= 0.0f;
	CurrentWorldState.bPacingAllowsNewEncounter			= true;
	CurrentWorldState.PacingBlockReason					= TEXT("Pacing not evaluated");

	SyncTraversalWorldState();

	CurrentRegion						= nullptr;
	SelectedEncounterPoint				= nullptr;
	SelectedEncounterDefinitionAsset	= nullptr;

	bHasSelectedEncounterDefinition		= false;
	SelectedEncounterDefinition			= FAmbientEncounterDefinition();
	SelectedEncounterScore				= 0.0f;
	SelectedEncounterReason				= TEXT("No encounter selected");
	LastSelectionDebugEntries.Reset();

	bHasSelectedEncounterSpawnTransform = false;
	SelectedEncounterSpawnTransform		= FTransform::Identity;
	SelectedEncounterLocationReason		= TEXT("No selected encounter spawn transform");

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

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	CurrentWorldState.bHasPlayerPawn = IsValid(PlayerPawn);

	if (CurrentWorldState.bHasPlayerPawn)
	{
		CurrentWorldState.PlayerLocation = PlayerPawn->GetActorLocation();
		CurrentWorldState.PlayerSpeed2D = PlayerPawn->GetVelocity().Size2D();

		UpdateCurrentRegion(PlayerPawn);
		SelectEncounterDefinitionAndPoint();
		EvaluatePrototypeEncounterCondition();
	}
	else
	{
		CurrentWorldState.PrototypeEncounterBlockReason		= TEXT("No player pawn");
		CurrentWorldState.PrototypeEncounterRuntimeReason	= TEXT("No player pawn");
	}

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

void AAmbientDirector::UpdateCurrentRegion(const APawn* PlayerPawn)
{
	CurrentRegion = nullptr;
	CurrentWorldState.bHasCurrentRegion = false;
	CurrentWorldState.CurrentRegionName = NAME_None;
	CurrentWorldState.CurrentRegionPriority = 0;

	UWorld* World = GetWorld();

	if (!World || !IsValid(PlayerPawn))
	{
		return;
	}

	const FVector QueryLocation = CurrentWorldState.PlayerLocation;

	bool bFoundAnyRegion = false;
	int32 BestPriority = 0;

	for (TActorIterator<AAmbientRegionVolume> RegionIt(World); RegionIt; ++RegionIt)
	{
		AAmbientRegionVolume* Region = *RegionIt;

		if (!IsValid(Region))
		{
			continue;
		}

		if (!Region->ContainsWorldLocation(QueryLocation))
		{
			continue;
		}

		if (!bFoundAnyRegion || Region->GetPriority() > BestPriority)
		{
			bFoundAnyRegion = true;
			BestPriority = Region->GetPriority();
			CurrentRegion = Region;
		}
	}

	if (IsValid(CurrentRegion))
	{
		CurrentWorldState.bHasCurrentRegion = true;
		CurrentWorldState.CurrentRegionName = CurrentRegion->GetRegionName();
		CurrentWorldState.CurrentRegionPriority = CurrentRegion->GetPriority();

		const FGameplayTag RegionTag = CurrentRegion->GetRegionTag();

		if (RegionTag.IsValid())
		{
			CurrentWorldState.bHasCurrentRegionTag = true;
			CurrentWorldState.CurrentRegionTag = RegionTag;
			CurrentWorldState.WorldTags.AddTag(RegionTag);
		}
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
	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();

	CurrentWorldState.bPrototypeEncounterConditionMet = false;
	CurrentWorldState.PrototypeEncounterBlockReason = TEXT("Prototype condition not evaluated");

	if (!bHasSelectedEncounterDefinition && !bHasRuntimeEncounterDefinition)
	{
		CurrentWorldState.PrototypeEncounterBlockReason =
			TEXT("No selected encounter definition");
		return;
	}

	if (!Definition.bEnabled)
	{
		CurrentWorldState.PrototypeEncounterBlockReason =
			TEXT("Encounter definition is disabled");
		return;
	}

	if (!CurrentWorldState.bHasPlayerPawn)
	{
		CurrentWorldState.PrototypeEncounterBlockReason = TEXT("No player pawn");
		return;
	}

	if (!CurrentWorldState.bHasSelectedEncounterLocation && !IsValid(ActivePrototypeEncounter))
	{
		CurrentWorldState.PrototypeEncounterBlockReason =
			CurrentWorldState.SelectedEncounterLocationReason;
		return;
	}

	if (CurrentWorldState.PlayerSpeed2D > Definition.MaxPlayerSpeed)
	{
		CurrentWorldState.PrototypeEncounterBlockReason = FString::Printf(
			TEXT("Player moving too fast: %.0f > %.0f cm/s"),
			CurrentWorldState.PlayerSpeed2D,
			Definition.MaxPlayerSpeed
		);
		return;
	}

	CurrentWorldState.bPrototypeEncounterConditionMet = true;
	CurrentWorldState.PrototypeEncounterBlockReason =
		TEXT("Definition condition passed");
}

void AAmbientDirector::UpdatePrototypeEncounter()
{
	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();
	// 현재 게임 시간. Cleanup / Cooldown 종료 시점 계산에 사용
	const float Now = CurrentWorldState.GameTimeSeconds;

	switch (PrototypeEncounterState)
	{
	case EAmbientEncounterRuntimeState::Waiting:
	{
		// Waiting 상태:
		// 아직 플레이어가 Encounter에 진입X
		// Enocunter 액터가 없으면 조건 확인 후 생성 (AmbientPlaceholderEncounter)
		// Enocunter 액가 있으면 플레이어 접근 여부 검사
		if (!IsValid(ActivePrototypeEncounter))
		{
			if (!CurrentWorldState.bPrototypeEncounterConditionMet)
			{
				// Encounter 액터가 없는 상태
				// 현재 Encounter 발생 조건을 만족하지 않으면 정리 후 대기
				DestroyPrototypeEncounter();
				CurrentWorldState.PrototypeEncounterRuntimeReason =
					CurrentWorldState.PrototypeEncounterBlockReason;
				return;
			}

			// Encounter 발생 조건은 만족
			// Spawn 또는 Update 실패 시 RuntimeReason 기록 후 종료
			if (!TrySpawnOrUpdatePrototypeEncounter())
			{
				CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("Failed to spawn or update prototype encounter");
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

		const float DistanceToEncounter = GetDistanceFromPlayerToPrototypeEncounter();

		if (Definition.WaitingAbandonDistance > 0.0f)
		{
			// 플레이어가 Abandon 거리 밖으로 나가면 Waiting 제거
			const float MaximumInitialSpawnDistance = FMath::Min(Definition.EncounterPointSearchRadius, MaximumSpawnDistance);
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
		CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("Waiting for player approach");
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

		const float DistanceToEncounter = GetDistanceFromPlayerToPrototypeEncounter();

		// 플레이어가 Leave 거리 밖으로 나가면 Cleanup 진입
		if (DistanceToEncounter >= Definition.PlayerLeaveDistance)
		{
			BeginPrototypeCleanup(TEXT("Player left encounter radius"));
			break;
		}

		// Encounter가 정상 진행 중인 상태
		CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("Player is involved in encounter");

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
		CurrentWorldState.PrototypeEncounterRuntimeReason = FString::Printf(
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
			PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;
			RuntimeEncounterDefinition = FAmbientEncounterDefinition();
			bHasRuntimeEncounterDefinition = false;

			// 이 틱에서 이미 선택된 Winner를 바로 고정하고 스폰
			if (bHasSelectedEncounterDefinition && bHasSelectedEncounterSpawnTransform)
			{
				if (!TrySpawnOrUpdatePrototypeEncounter())
				{
					CurrentWorldState.PrototypeEncounterRuntimeReason =
						TEXT("Cooldown complete, but selected encounter failed to spawn");
					break;
				}

				CurrentWorldState.PrototypeEncounterRuntimeReason =
					TEXT("Cooldown complete; selected encounter spawned");
				break;
			}

			CurrentWorldState.PrototypeEncounterRuntimeReason =
				TEXT("Cooldown complete; returning to Waiting");
			break;
		}

		// 아직 Cooldown 대기 중
		CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("Cooldown remaining");

		break;
	}

	default:
	{
		// 알 수 없는 상태
		// 안전하게 Waiting 상태로 복구

		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;
		CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("Unknown state corrected to Waiting");
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

bool AAmbientDirector::TrySpawnOrUpdatePrototypeEncounter()
{
	if (!bHasSelectedEncounterDefinition && !bHasRuntimeEncounterDefinition)
	{
		CurrentWorldState.bPrototypeEncounterConditionMet = false;
		CurrentWorldState.PrototypeEncounterBlockReason =
			TEXT("No selected encounter definition");
		return false;
	}

	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();

	if (!Definition.EncounterClass)
	{
		CurrentWorldState.bPrototypeEncounterConditionMet = false;
		CurrentWorldState.PrototypeEncounterBlockReason =
			TEXT("EncounterClass is not assigned in definition");
		return false;
	}

	if (!Definition.EncounterClass->ImplementsInterface(
		UAmbientEncounterRuntimeInterface::StaticClass()
	))
	{
		CurrentWorldState.bPrototypeEncounterConditionMet = false;
		CurrentWorldState.PrototypeEncounterBlockReason = FString::Printf(
			TEXT("EncounterClass %s does not implement AmbientEncounterRuntimeInterface"),
			*GetNameSafe(Definition.EncounterClass.Get())
		);
		return false;
	}

	if (!bHasSelectedEncounterSpawnTransform && !IsValid(ActivePrototypeEncounter))
	{
		CurrentWorldState.bPrototypeEncounterConditionMet = false;
		CurrentWorldState.PrototypeEncounterBlockReason =
			TEXT("Selected encounter spawn transform is invalid");
		return false;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		CurrentWorldState.bPrototypeEncounterConditionMet = false;
		CurrentWorldState.PrototypeEncounterBlockReason = TEXT("No world");
		return false;
	}


	if (!IsValid(ActivePrototypeEncounter))
	{
		if (!bHasSelectedEncounterSpawnTransform)
		{
			CurrentWorldState.PrototypeEncounterBlockReason =
				TEXT("Cannot spawn because selected spawn transform is missing");
			return false;
		}

		const FAmbientEncounterDefinition PendingRuntimeDefinition = Definition;
		const FTransform SpawnTransform = SelectedEncounterSpawnTransform;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* SpawnedEncounter = World->SpawnActor<AActor>(
			PendingRuntimeDefinition.EncounterClass,
			SpawnTransform, SpawnParams);

		if (!IsValid(SpawnedEncounter))
		{
			ActivePrototypeEncounter = nullptr;
			RuntimeEncounterDefinition = FAmbientEncounterDefinition();
			bHasRuntimeEncounterDefinition = false;
			CurrentWorldState.PrototypeEncounterBlockReason = TEXT("Spawn failed");
			return false;
		}

		ActivePrototypeEncounter = SpawnedEncounter;
		RuntimeEncounterDefinition = PendingRuntimeDefinition;
		bHasRuntimeEncounterDefinition = true;

		RuntimeEncounterRegionName		= CurrentWorldState.CurrentRegionName;
		RuntimeEncounterPointName		= IsValid(SelectedEncounterPoint)
			? SelectedEncounterPoint->GetPointName()
			: FName(TEXT("Location.EQS"));

		RuntimeEncounterLocation		= SpawnTransform.GetLocation();
		RuntimeEncounterLocationSource	=
			CurrentWorldState.SelectedEncounterLocationSource.IsEmpty()
			? TEXT("Unknown")
			: CurrentWorldState.SelectedEncounterLocationSource;

		FAmbientEncounterRuntimeContext RuntimeContext;
		RuntimeContext.DirectorActor		= this;
		RuntimeContext.EncounterId			= RuntimeEncounterDefinition.EncounterId;
		RuntimeContext.RegionName			= RuntimeEncounterRegionName;
		RuntimeContext.SourcePointName		= RuntimeEncounterPointName;
		RuntimeContext.SpawnLocation		= SpawnTransform.GetLocation();
		RuntimeContext.StartedAtTimeSeconds = CurrentWorldState.GameTimeSeconds;
		RuntimeContext.EncounterTags		= RuntimeEncounterDefinition.EncounterTags;

		IAmbientEncounterRuntimeInterface::Execute_InitializeAmbientEncounter(
			ActivePrototypeEncounter,
			RuntimeContext
		);

		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterWaiting(
			ActivePrototypeEncounter
		);

		if (bAutoSaveDirectorStateOnRuntimeChange)
		{
			SaveDirectorStateToSlot();
		}
	}
	else if (bHasSelectedEncounterSpawnTransform && IsValid(SelectedEncounterPoint))
	{
		// EQS Encounter는 스폰 뒤 위치 이동 X, Point가 있는 Authored 방식일 때만 위치 이동 
		ActivePrototypeEncounter->SetActorTransform(SelectedEncounterSpawnTransform);
	}

	return IsValid(ActivePrototypeEncounter);
}

void AAmbientDirector::StartPrototypeEncounter()
{
	if (!IsValid(ActivePrototypeEncounter))
	{
		CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("Cannot start encounter because actor is missing");
		return;
	}

	PrototypeEncounterState = EAmbientEncounterRuntimeState::Active;

	RuntimeEncounterStartedAtTimeSeconds = CurrentWorldState.GameTimeSeconds;

	if (RuntimeEncounterRegionName == NAME_None)
	{
		RuntimeEncounterRegionName = CurrentWorldState.CurrentRegionName;
	}

	if (RuntimeEncounterPointName == NAME_None)
	{
		RuntimeEncounterPointName = CurrentWorldState.SelectedEncounterPointName;
	}

	PrototypeEncounterStartCount++;

	LastAnyEncounterStartTimeSeconds = CurrentWorldState.GameTimeSeconds;

	if (ActivePrototypeEncounter->GetClass()->ImplementsInterface(
		UAmbientEncounterRuntimeInterface::StaticClass()
	))
	{
		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterActivated(
			ActivePrototypeEncounter
		);
	}

	CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("Player is involved in encounter");

	if (bAutoSaveDirectorStateOnRuntimeChange)
	{
		SaveDirectorStateToSlot();
	}
}

void AAmbientDirector::RemoveWaitingPrototypeEncounter(const FString& Reason)
{
	if (PrototypeEncounterState != EAmbientEncounterRuntimeState::Waiting)
	{
		return;
	}

	DestroyPrototypeEncounter();

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

	CurrentWorldState.bHasActivePrototypeEncounter = false;
	CurrentWorldState.DistanceToPrototypeEncounter = 0.0f;
	CurrentWorldState.PrototypeEncounterRuntimeReason = Reason;

	if (bAutoSaveDirectorStateOnRuntimeChange)
	{
		SaveDirectorStateToSlot();
	}
}

void AAmbientDirector::BeginPrototypeCleanup(const FString& Reason)
{
	if (PrototypeEncounterState == EAmbientEncounterRuntimeState::Cleanup)
	{
		return;
	}

	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();

	PrototypeEncounterState = EAmbientEncounterRuntimeState::Cleanup;

	PendingPrototypeFinishReason = Reason;

	if (IsValid(ActivePrototypeEncounter) &&
		ActivePrototypeEncounter->GetClass()->ImplementsInterface(
			UAmbientEncounterRuntimeInterface::StaticClass()
		))
	{
		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterCleanup(
			ActivePrototypeEncounter,
			Reason
		);
	}

	const float Now = CurrentWorldState.GameTimeSeconds;
	const float SafeCleanupDelay = FMath::Max(0.0f, Definition.CleanupDelaySeconds);

	PrototypeCleanupEndTimeSeconds = Now + SafeCleanupDelay;

	if (SafeCleanupDelay <= 0.0f)
	{
		FinishPrototypeEncounter(Reason);
		return;
	}

	CurrentWorldState.PrototypeEncounterRuntimeReason = FString::Printf(
		TEXT("Cleaning up: %s"),
		*Reason
	);

	if (bAutoSaveDirectorStateOnRuntimeChange)
	{
		SaveDirectorStateToSlot();
	}
}

void AAmbientDirector::FinishPrototypeEncounter(const FString& Reason)
{
	const float FinishTime = CurrentWorldState.GameTimeSeconds;

	if (IsValid(ActivePrototypeEncounter) &&
		ActivePrototypeEncounter->GetClass()->ImplementsInterface(
			UAmbientEncounterRuntimeInterface::StaticClass()
		))
	{
		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterFinished(
			ActivePrototypeEncounter,
			Reason
		);
	}

	AddPrototypeHistoryEntry(FinishTime, Reason);

	PrototypeEncounterFinishCount++;

	DestroyPrototypeEncounter();

	RuntimeEncounterStartedAtTimeSeconds = 0.0f;
	RuntimeEncounterRegionName = NAME_None;
	RuntimeEncounterPointName = NAME_None;
	RuntimeEncounterLocation = FVector::ZeroVector;
	RuntimeEncounterLocationSource = TEXT("Unknown");
	PendingPrototypeFinishReason = TEXT("None");
	PrototypeCleanupEndTimeSeconds = 0.0f;

	StartPrototypeCooldown();

	CurrentWorldState.PrototypeEncounterRuntimeReason = FString::Printf(TEXT("Finished encounter: %s"), *Reason);

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
		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;

		RuntimeEncounterDefinition = FAmbientEncounterDefinition();
		bHasRuntimeEncounterDefinition = false;

		return;
	}

	PrototypeCooldownEndTimeSeconds = Now + SafeCooldownDuration;
	PrototypeEncounterState = EAmbientEncounterRuntimeState::Cooldown;
}

void AAmbientDirector::DestroyPrototypeEncounter()
{
	if (IsValid(ActivePrototypeEncounter))
	{
		ActivePrototypeEncounter->Destroy();
	}

	ActivePrototypeEncounter = nullptr;
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

	CurrentWorldState.PrototypeEncounterState		= PrototypeEncounterState;
	CurrentWorldState.bHasActivePrototypeEncounter	= IsValid(ActivePrototypeEncounter);

	CurrentWorldState.DistanceToPrototypeEncounter	= GetDistanceFromPlayerToPrototypeEncounter();

	CurrentWorldState.PrototypeCleanupRemaining		= 0.0f;
	CurrentWorldState.PrototypeCooldownRemaining	= 0.0f;

	if (PrototypeEncounterState == EAmbientEncounterRuntimeState::Cleanup)
	{
		CurrentWorldState.PrototypeCleanupRemaining = FMath::Max(0.0f, PrototypeCleanupEndTimeSeconds - Now);
	}

	if (PrototypeEncounterState == EAmbientEncounterRuntimeState::Cooldown)
	{
		CurrentWorldState.PrototypeCooldownRemaining = FMath::Max(0.0f, PrototypeCooldownEndTimeSeconds - Now);
	}

	CurrentWorldState.PrototypeEncounterStartCount  = PrototypeEncounterStartCount;
	CurrentWorldState.PrototypeEncounterFinishCount = PrototypeEncounterFinishCount;
}

float AAmbientDirector::GetDistanceFromPlayerToPrototypeEncounter() const
{
	if (!CurrentWorldState.bHasPlayerPawn || !IsValid(ActivePrototypeEncounter))
	{
		return 0.0f;
	}

	return FVector::Dist2D(CurrentWorldState.PlayerLocation, ActivePrototypeEncounter->GetActorLocation());
}

FString AAmbientDirector::GetPrototypeRuntimeStateString() const
{
	switch (PrototypeEncounterState)
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
	CurrentWorldState.bIsMounted = TraversalState == EAmbientTraversalState::Mounted;

	CurrentWorldState.TraversalActor =
		IsValid(TraversalActor.Get())
		? TraversalActor.Get()
		: nullptr;

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