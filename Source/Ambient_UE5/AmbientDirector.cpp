// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientDirector.h"

#include "AmbientCandidateMarker.h"
#include "AmbientEncounterDefinitionData.h"
#include "AmbientEncounterPoint.h"
#include "AmbientPlaceholderEncounter.h"
#include "AmbientRegionVolume.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AAmbientDirector::AAmbientDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	CandidateMarkerClass = AAmbientCandidateMarker::StaticClass();
	PrototypeEncounterDefinition.EncounterClass = AAmbientPlaceholderEncounter::StaticClass();
}

void AAmbientDirector::BeginPlay()
{
	Super::BeginPlay();

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
	DestroyCandidateMarker();
	DestroyPrototypeEncounter();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorldStateTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AAmbientDirector::UpdateWorldState()
{
	CurrentWorldState = FAmbientWorldState();
	CurrentRegion = nullptr;
	SelectedEncounterPoint = nullptr;
	SelectedEncounterDefinitionAsset = nullptr;
	bHasSelectedEncounterDefinition = false;
	SelectedEncounterScore = 0.0f;
	SelectedEncounterReason = TEXT("No encounter selected");
	LastSelectionDebugEntries.Reset();

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
		UpdateCandidateLocation(PlayerPawn);
		SelectEncounterDefinitionAndPoint();
		EvaluatePrototypeEncounterCondition();
	}
	else
	{
		CurrentWorldState.PrototypeEncounterBlockReason = TEXT("No player pawn");
		CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("No player pawn");
	}

	UpdateCandidateMarker();
	UpdatePrototypeEncounter();
	SyncPrototypeRuntimeWorldState();
	
	if (bDrawRegionDebug)
	{
		DrawRegionDebug();
	}

	if (bDrawCandidateDebug)
	{
		DrawCandidateDebug();
	}

	if (bDrawEncounterPointDebug)
	{
		DrawEncounterPointDebug();
	}

	if (bDrawEncounterRuntimeDebug)
	{
		DrawEncounterRuntimeDebug();
	}

	if (bPrintDebug)
	{
		PrintWorldStateDebug();
		PrintSelectionDebug();
		PrintEncounterDebug();
		PrintEncounterHistoryDebug();
	}
}

void AAmbientDirector::UpdateCandidateLocation(const APawn* PlayerPawn)
{
	if (!IsValid(PlayerPawn))
	{
		RejectCandidate(TEXT("No player pawn"));
		return;
	}

	if (MaximumSpawnDistance < MinimumSpawnDistance)
	{
		RejectCandidate(TEXT("Invalid distance settings: MaximumSpawnDistance is smaller than MinimumSpawnDistance"));
		return;
	}

	CurrentWorldState.RequestedCandidateDistance = CandidateDistance;
	CurrentWorldState.UsedCandidateDistance = FMath::Clamp(
		CandidateDistance,
		MinimumSpawnDistance,
		MaximumSpawnDistance
	);

	FVector CandidateDirection = PlayerPawn->GetActorForwardVector(); // TODO: 전방을 콘앵글 씌우고 랜덤한 값으로 변경...?
	CandidateDirection.Z = 0.0f;
	CandidateDirection = CandidateDirection.GetSafeNormal();
	
	if (CandidateDirection.IsNearlyZero())
	{
		CandidateDirection = FVector::ForwardVector;
	}

	CurrentWorldState.RawCandidateLocation = CurrentWorldState.PlayerLocation + CandidateDirection * CurrentWorldState.UsedCandidateDistance;
	CurrentWorldState.CandidateLocation = CurrentWorldState.RawCandidateLocation;
	CurrentWorldState.bHasCandidateLocation = true;

	FVector GroundLocation = FVector::ZeroVector;
	FHitResult GroundHit;

	if (!ProjectPointToGround(CurrentWorldState.RawCandidateLocation, PlayerPawn, GroundLocation, GroundHit))
	{
		RejectCandidate(TEXT("No ground found under candidate"));
		return;
	}

	if (GroundHit.ImpactNormal.Z < MinimumGroundNormalZ)
	{
		const FString Reason = FString::Printf(
			TEXT("Ground too steep: NormalZ=%.2f"),
			GroundHit.ImpactNormal.Z
		);

		RejectCandidate(Reason);
		return;
	}

	CurrentWorldState.CandidateLocation = GroundLocation;
	CurrentWorldState.bCandidateProjectedToGround = true;
	CurrentWorldState.CandidateDistance2D = FVector::Dist2D(CurrentWorldState.PlayerLocation, CurrentWorldState.CandidateLocation);

	if (CurrentWorldState.CandidateDistance2D < MinimumSpawnDistance)
	{
		const FString Reason = FString::Printf(
			TEXT("Too close: Dist=%.0f Min=%.0f"),
			CurrentWorldState.CandidateDistance2D,
			MinimumSpawnDistance
		);

		RejectCandidate(Reason);
		return;
	}

	if (CurrentWorldState.CandidateDistance2D > MaximumSpawnDistance)
	{
		const FString Reason = FString::Printf(
			TEXT("Too far: Dist=%.0f Max=%.0f"),
			CurrentWorldState.CandidateDistance2D,
			MaximumSpawnDistance
		);

		RejectCandidate(Reason);
		return;
	}

	FHitResult PathBlockHit;

	if (IsPathToCandidateBlocked(PlayerPawn, CurrentWorldState.CandidateLocation, PathBlockHit))
	{
		const FString Reason = FString::Printf(
			TEXT("Blocked path: %s"),
			*GetNameSafe(PathBlockHit.GetActor())
		);

		RejectCandidate(Reason);
		return;
	}

	FHitResult AreaBlockHit;

	if (IsCandidateAreaBlocked(PlayerPawn, CurrentWorldState.CandidateLocation, AreaBlockHit))
	{
		const FString Reason = FString::Printf(
			TEXT("Candidate area occupied: %s"),
			*GetNameSafe(AreaBlockHit.GetActor())
		);

		RejectCandidate(Reason);
		return;
	}

	AcceptCandidate();
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

void AAmbientDirector::SelectEncounterDefinitionAndPoint()
{
	SelectedEncounterPoint = nullptr;
	SelectedEncounterDefinitionAsset = nullptr;
	bHasSelectedEncounterDefinition = false;
	SelectedEncounterDefinition = FAmbientEncounterDefinition();
	SelectedEncounterScore = 0.0f;
	SelectedEncounterReason = TEXT("No encounter definition selected");

	CurrentWorldState.bHasSelectedEncounterDefinition = false;
	CurrentWorldState.SelectedEncounterDefinitionId = NAME_None;
	CurrentWorldState.SelectedEncounterDefinitionScore = 0.0f;
	CurrentWorldState.SelectedEncounterDefinitionReason = TEXT("No encounter definition selected");

	CurrentWorldState.bHasSelectedEncounterPoint = false;
	CurrentWorldState.SelectedEncounterPointName = NAME_None;
	CurrentWorldState.SelectedEncounterPointLocation = FVector::ZeroVector;
	CurrentWorldState.SelectedEncounterPointReason = TEXT("No encounter point evaluated");

	float BestScore = -TNumericLimits<float>::Max();
	AAmbientEncounterPoint* BestPoint = nullptr;
	const UAmbientEncounterDefinitionData* BestAsset = nullptr;
	FAmbientEncounterDefinition BestDefinition;

	auto EvaluateDefinition = [&](const UAmbientEncounterDefinitionData* DefinitionAsset, const FAmbientEncounterDefinition& Definition)
	{
		FAmbientEncounterSelectionDebugEntry DebugEntry;
		AAmbientEncounterPoint* CandidatePoint = nullptr;

		const bool bAccepted = EvaluateEncounterDefinitionCandidate(
			Definition,
			DebugEntry,
			CandidatePoint
		);

		LastSelectionDebugEntries.Add(DebugEntry);

		if (!bAccepted || !IsValid(CandidatePoint))
		{
			return;
		}

		if (DebugEntry.Score > BestScore)
		{
			BestScore = DebugEntry.Score;
			BestPoint = CandidatePoint;
			BestAsset = DefinitionAsset;
			BestDefinition = Definition;
		}
	};

	// 우선 방식:
	// 여러 Encounter 정의 에셋을 후보로 평가
	for (const TObjectPtr<UAmbientEncounterDefinitionData>& DefinitionAsset : EncounterDefinitionAssets)
	{
		if (!IsValid(DefinitionAsset))
		{
			FAmbientEncounterSelectionDebugEntry DebugEntry;
			DebugEntry.bAccepted = false;
			DebugEntry.EncounterId = NAME_None;
			DebugEntry.Reason = TEXT("Null encounter definition asset in array");
			LastSelectionDebugEntries.Add(DebugEntry);
			continue;
		}

		EvaluateDefinition(DefinitionAsset.Get(), DefinitionAsset->Definition);
	}

	// 기존 단일 Encounter 정의 에셋 fallback
	// 새 배열이 비어 있을 때만 사용
	if (EncounterDefinitionAssets.Num() == 0 && IsValid(PrototypeEncounterDefinitionAsset))
	{
		EvaluateDefinition(PrototypeEncounterDefinitionAsset.Get(), PrototypeEncounterDefinitionAsset->Definition);
	}

	// 인라인 Encounter 정의값 fallback
	// 설정된 에셋이 없을 때만 사용
	if (EncounterDefinitionAssets.Num() == 0 && !IsValid(PrototypeEncounterDefinitionAsset))
	{
		EvaluateDefinition(nullptr, PrototypeEncounterDefinition);
	}

	if (!IsValid(BestPoint))
	{
		SelectedEncounterReason = TEXT("No accepted encounter definition candidate");

		CurrentWorldState.SelectedEncounterDefinitionReason = SelectedEncounterReason;
		CurrentWorldState.SelectedEncounterPointReason =
			TEXT("No point selected because no definition candidate won");
		return;
	}

	SelectedEncounterPoint = BestPoint;
	SelectedEncounterDefinitionAsset = const_cast<UAmbientEncounterDefinitionData*>(BestAsset);
	SelectedEncounterDefinition = BestDefinition;
	bHasSelectedEncounterDefinition = true;
	SelectedEncounterScore = BestScore;

	SelectedEncounterReason = FString::Printf(
		TEXT("Selected highest score candidate: %.1f"),
		BestScore
	);

	CurrentWorldState.bHasSelectedEncounterDefinition = true;
	CurrentWorldState.SelectedEncounterDefinitionId = BestDefinition.EncounterId;
	CurrentWorldState.SelectedEncounterDefinitionScore = BestScore;
	CurrentWorldState.SelectedEncounterDefinitionReason = SelectedEncounterReason;

	CurrentWorldState.bHasSelectedEncounterPoint = true;
	CurrentWorldState.SelectedEncounterPointName = BestPoint->GetPointName();
	CurrentWorldState.SelectedEncounterPointLocation = BestPoint->GetActorLocation();
	CurrentWorldState.SelectedEncounterPointReason =
		TEXT("Selected point from winning encounter definition");
}

bool AAmbientDirector::EvaluateEncounterDefinitionCandidate(
	const FAmbientEncounterDefinition& Definition, 
	FAmbientEncounterSelectionDebugEntry& OutDebugEntry, 
	AAmbientEncounterPoint*& OutBestPoint
) const
{
	OutBestPoint = nullptr;

	OutDebugEntry.bAccepted = false;
	OutDebugEntry.EncounterId = Definition.EncounterId;
	OutDebugEntry.PointName = NAME_None;
	OutDebugEntry.Score = 0.0f;
	OutDebugEntry.DistanceToPoint = 0.0f;
	OutDebugEntry.Reason = TEXT("Not evaluated");

	FString WorldMatchReason;

	if (!DoesEncounterDefinitionMatchCurrentWorld(Definition, WorldMatchReason))
	{
		OutDebugEntry.Reason = WorldMatchReason;
		return false;
	}

	float DistanceToPoint = 0.0f;
	FString PointReason;

	AAmbientEncounterPoint* BestPoint = FindBestEncounterPointForDefinition(
		Definition,
		DistanceToPoint,
		PointReason
	);

	if (!IsValid(BestPoint))
	{
		OutDebugEntry.Reason = PointReason;
		return false;
	}

	const float MinDistance = MinimumSpawnDistance;
	const float MaxDistance = FMath::Min(
		Definition.EncounterPointSearchRadius,
		MaximumSpawnDistance
	);

	const float DistanceRange = FMath::Max(1.0f, MaxDistance - MinDistance);
	const float DistanceAlpha = FMath::Clamp(
		(DistanceToPoint - MinDistance) / DistanceRange,
		0.0f,
		1.0f
	);

	// 가까울수록 보너스
	const float DistanceBonus = (1.0f - DistanceAlpha) * Definition.DistanceScoreWeight;

	const bool bRecentlyCompleted =
		HasRecentlyFinishedEncounter(Definition.EncounterId);

	// 최근에 했으면 마이너스
	const float HistoryPenalty = bRecentlyCompleted
		? Definition.RecentlyCompletedPenalty
		: 0.0f;

	const float FinalScore =
		Definition.BaseSelectionScore + DistanceBonus - HistoryPenalty;

	OutBestPoint = BestPoint;

	OutDebugEntry.bAccepted = true;
	OutDebugEntry.PointName = BestPoint->GetPointName();
	OutDebugEntry.Score = FinalScore;
	OutDebugEntry.DistanceToPoint = DistanceToPoint;

	OutDebugEntry.Reason = FString::Printf(
		TEXT("Accepted | Base=%.1f DistanceBonus=%.1f HistoryPenalty=%.1f | %s"),
		Definition.BaseSelectionScore,
		DistanceBonus,
		HistoryPenalty,
		*PointReason
	);

	return true;
}

AAmbientEncounterPoint* AAmbientDirector::FindBestEncounterPointForDefinition(
	const FAmbientEncounterDefinition& Definition, 
	float& OutDistanceToPoint, 
	FString& OutReason
) const
{
	OutDistanceToPoint = 0.0f;
	OutReason = TEXT("No point evaluated");

	UWorld* World = GetWorld();

	if (!World)
	{
		OutReason = TEXT("Rejected: no world");
		return nullptr;
	}

	const float MinDistance = MinimumSpawnDistance;
	const float MaxDistance = FMath::Min(
		Definition.EncounterPointSearchRadius,
		MaximumSpawnDistance
	);

	if (MaxDistance < MinDistance)
	{
		OutReason = FString::Printf(
			TEXT("Rejected: invalid distance range Min=%.0f Max=%.0f"),
			MinDistance,
			MaxDistance
		);
		return nullptr;
	}

	const float MinDistanceSq = FMath::Square(MinDistance);
	const float MaxDistanceSq = FMath::Square(MaxDistance);

	float BestDistanceSq = TNumericLimits<float>::Max();
	AAmbientEncounterPoint* BestPoint = nullptr;

	for (TActorIterator<AAmbientEncounterPoint> PointIt(World); PointIt; ++PointIt)
	{
		AAmbientEncounterPoint* Point = *PointIt;

		if (!IsValid(Point) || !Point->IsPointEnabled())
		{
			continue;
		}

		if (!DoesEncounterPointMatchDefinition(Point, Definition))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared2D(
			CurrentWorldState.PlayerLocation,
			Point->GetActorLocation()
		);

		if (DistanceSq < MinDistanceSq || DistanceSq > MaxDistanceSq)
		{
			continue;
		}

		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestPoint = Point;
		}
	}

	if (!IsValid(BestPoint))
	{
		OutReason = FString::Printf(
			TEXT("Rejected: no authored point matched tags and distance %.0f-%.0f cm"),
			MinDistance,
			MaxDistance
		);
		return nullptr;
	}

	OutDistanceToPoint = FMath::Sqrt(BestDistanceSq);

	OutReason = FString::Printf(
		TEXT("Point=%s Distance=%.0f cm"),
		*BestPoint->GetPointName().ToString(),
		OutDistanceToPoint
	);

	return BestPoint;
}

bool AAmbientDirector::DoesEncounterDefinitionMatchCurrentWorld(
	const FAmbientEncounterDefinition& Definition, 
	FString& OutReason
) const
{
	if (!Definition.bEnabled)
	{
		OutReason = TEXT("Rejected: definition disabled");
		return false;
	}

	if (!CurrentWorldState.bHasPlayerPawn)
	{
		OutReason = TEXT("Rejected: no player pawn");
		return false;
	}

	if (Definition.RequiredRegionTag.IsValid())
	{
		if (!CurrentWorldState.WorldTags.HasTagExact(Definition.RequiredRegionTag))
		{
			OutReason = FString::Printf(
				TEXT("Rejected: missing required region tag %s"),
				*Definition.RequiredRegionTag.ToString()
			);
			return false;
		}
	}
	else if (Definition.RequiredRegionName != NAME_None)
	{
		if (!CurrentWorldState.bHasCurrentRegion)
		{
			OutReason = FString::Printf(
				TEXT("Rejected: requires region %s but player is in None"),
				*Definition.RequiredRegionName.ToString()
			);
			return false;
		}
		if (CurrentWorldState.CurrentRegionName != Definition.RequiredRegionName)
		{
			OutReason = FString::Printf(
				TEXT("Rejected: requires region %s but player is in %s"),
				*Definition.RequiredRegionName.ToString(),
				*CurrentWorldState.CurrentRegionName.ToString()
			);
			return false;
		}
	}

	if (!Definition.RequiredWorldTags.IsEmpty())
	{
		if (!CurrentWorldState.WorldTags.HasAllExact(Definition.RequiredWorldTags))
		{
			OutReason = FString::Printf(
				TEXT("Rejected: missing required world tags. Required=%s Current=%s"),
				*Definition.RequiredWorldTags.ToStringSimple(),
				*CurrentWorldState.WorldTags.ToStringSimple()
			);
			return false;
		}
	}

	if (CurrentWorldState.PlayerSpeed2D > Definition.MaxPlayerSpeed)
	{
		OutReason = FString::Printf(
			TEXT("Rejected: player moving too fast %.0f > %.0f cm/s"),
			CurrentWorldState.PlayerSpeed2D,
			Definition.MaxPlayerSpeed
		);
		return false;
	}

	OutReason = TEXT("World conditions matched");
	return true;
}

bool AAmbientDirector::DoesEncounterPointMatchDefinition(
	const AAmbientEncounterPoint* Point, 
	const FAmbientEncounterDefinition& Definition
) const
{
	if (!IsValid(Point))
	{
		return false;
	}

	if (Definition.RequiredRegionTag.IsValid())
	{
		const FGameplayTag PointRegionTag = Point->GetRegionTag();

		if (!PointRegionTag.IsValid())
		{
			return false;
		}

		if (!PointRegionTag.MatchesTagExact(Definition.RequiredRegionTag))
		{
			return false;
		}
	}
	else if (Definition.RequiredRegionName != NAME_None)
	{
		if (Point->GetRegionName() != Definition.RequiredRegionName)
		{
			return false;
		}
	}

	if (!Definition.RequiredPointTags.IsEmpty())
	{
		if (!Point->GetPointTags().HasAllExact(Definition.RequiredPointTags))
		{
			return false;
		}
	}

	return true;
}

bool AAmbientDirector::HasRecentlyFinishedEncounter(FName EncounterId) const
{
	if (EncounterId == NAME_None)
	{
		return false;
	}

	for (const FAmbientEncounterHistoryEntry& HistoryEntry : PrototypeEncounterHistory)
	{
		if (HistoryEntry.EncounterId == EncounterId)
		{
			return true;
		}
	}

	return false;
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

	if (!CurrentWorldState.bHasSelectedEncounterPoint && !IsValid(ActivePrototypeEncounter))
	{
		CurrentWorldState.PrototypeEncounterBlockReason =
			CurrentWorldState.SelectedEncounterPointReason;
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
			// Encounter 액터가 이미 있는 상태
			// 특정 Region에서만 유지되어야 하는 Encounter인지 확인

			if (Definition.RequiredRegionName != NAME_None)
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
					DestroyPrototypeEncounter();

					RuntimeEncounterDefinition = FAmbientEncounterDefinition();
					bHasRuntimeEncounterDefinition = false;
					RuntimeEncounterRegionName = NAME_None;
					RuntimeEncounterPointName = NAME_None;
					RuntimeEncounterStartedAtTimeSeconds = 0.0f;

					CurrentWorldState.PrototypeEncounterRuntimeReason =
						TEXT("Waiting encounter removed because player left required region");
					return;
				}
			}
		}

		// Encounter는 준비됨
		// 플레이어 접근 대기 상태
		CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("Waiting for player approach");

		const float DistanceToEncounter = GetDistanceFromPlayerToPrototypeEncounter();

		// 플레이어가 Engage 거리 안으로 들어오면 Encounter 시작
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

	if (!IsValid(SelectedEncounterPoint) && !IsValid(ActivePrototypeEncounter))
	{
		CurrentWorldState.bPrototypeEncounterConditionMet = false;
		CurrentWorldState.PrototypeEncounterBlockReason =
			TEXT("Selected encounter point is invalid");
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
		if (!IsValid(SelectedEncounterPoint))
		{
			CurrentWorldState.PrototypeEncounterBlockReason =
				TEXT("Cannot spawn because selected point is missing");
			return false;
		}

		RuntimeEncounterDefinition = SelectedEncounterDefinition;
		bHasRuntimeEncounterDefinition = true;

		const FTransform SpawnTransform = SelectedEncounterPoint->GetEncounterSpawnTransform();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ActivePrototypeEncounter = World->SpawnActor<AAmbientPlaceholderEncounter>(
			RuntimeEncounterDefinition.EncounterClass,
			SpawnTransform,
			SpawnParams
		);

		if (!IsValid(ActivePrototypeEncounter))
		{
			CurrentWorldState.PrototypeEncounterBlockReason = TEXT("Spawn failed");
			return false;
		}

		RuntimeEncounterRegionName = CurrentWorldState.CurrentRegionName;
		RuntimeEncounterPointName  = CurrentWorldState.SelectedEncounterPointName;

		ActivePrototypeEncounter->InitializePrototypeEncounter(
			RuntimeEncounterDefinition.EncounterId,
			RuntimeEncounterRegionName,
			RuntimeEncounterPointName
		);
	}
	else if (IsValid(SelectedEncounterPoint))
	{
		const FTransform SpawnTransform =
			SelectedEncounterPoint->GetEncounterSpawnTransform();

		ActivePrototypeEncounter->SetActorTransform(SpawnTransform);
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

	CurrentWorldState.PrototypeEncounterRuntimeReason = TEXT("Player is involved in encounter");
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
}

void AAmbientDirector::FinishPrototypeEncounter(const FString& Reason)
{
	const float FinishTime = CurrentWorldState.GameTimeSeconds;

	AddPrototypeHistoryEntry(FinishTime, Reason);

	PrototypeEncounterFinishCount++;

	DestroyPrototypeEncounter();

	RuntimeEncounterStartedAtTimeSeconds = 0.0f;
	RuntimeEncounterRegionName = NAME_None;
	RuntimeEncounterPointName = NAME_None;
	PendingPrototypeFinishReason = TEXT("None");
	PrototypeCleanupEndTimeSeconds = 0.0f;

	StartPrototypeCooldown();

	CurrentWorldState.PrototypeEncounterRuntimeReason = FString::Printf(TEXT("Finished encounter: %s"), *Reason);
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

bool AAmbientDirector::ProjectPointToGround(const FVector& Point, const APawn* PlayerPawn, FVector& OutGroundLocation, FHitResult& OutGroundHit) const
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	const FVector TraceStart = Point + FVector::UpVector * GroundTraceUpDistance;
	const FVector TraceEnd = Point - FVector::UpVector * GroundTraceDownDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(PlayerPawn);

	const bool bHitGround = World->LineTraceSingleByChannel(
		OutGroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	if (!bHitGround || !OutGroundHit.bBlockingHit)
	{
		return false;
	}

	OutGroundLocation = OutGroundHit.ImpactPoint;
	return true;
}

bool AAmbientDirector::IsPathToCandidateBlocked(const APawn* PlayerPawn, const FVector& GroundLocation, FHitResult& OutBlockHit) const
{
	UWorld* World = GetWorld();

	if (!World || !IsValid(PlayerPawn))
	{
		return false;
	}

	const FVector TraceStart = CurrentWorldState.PlayerLocation;
	const FVector TraceEnd = GroundLocation + FVector::UpVector * ObstructionCheckHeight;

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(PlayerPawn);

	const bool bBlocked = World->LineTraceSingleByChannel(
		OutBlockHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	return bBlocked && OutBlockHit.bBlockingHit;
}

bool AAmbientDirector::IsCandidateAreaBlocked(const APawn* PlayerPawn, const FVector& GroundLocation, FHitResult& OutBlockHit) const
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	const FVector SweepStart = GroundLocation + FVector::UpVector * (CandidateClearanceRadius + 5.0f);

	const FVector SweepEnd = SweepStart + FVector::UpVector * 1.0f;

	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(CandidateClearanceRadius);

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bFindInitialOverlaps = true;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(PlayerPawn);

	const bool bBlocked = World->SweepSingleByChannel(
		OutBlockHit,
		SweepStart,
		SweepEnd,
		FQuat::Identity,
		ECC_Visibility,
		SweepShape,
		QueryParams
	);

	return bBlocked && OutBlockHit.bBlockingHit;
}

void AAmbientDirector::RejectCandidate(const FString& Reason)
{
	CurrentWorldState.bCandidateValid = false;
	CurrentWorldState.CandidateRejectReason = Reason;
}

void AAmbientDirector::AcceptCandidate()
{
	CurrentWorldState.bCandidateValid = true;
	CurrentWorldState.CandidateRejectReason = TEXT("Accepted");
}

void AAmbientDirector::UpdateCandidateMarker()
{
	const bool bShouldShowMarker =
		bUseVisibleCandidateMarker &&
		CurrentWorldState.bHasCandidateLocation &&
		CurrentWorldState.bCandidateValid;

	if (!bShouldShowMarker)
	{
		DestroyCandidateMarker();
		return;
	}

	if (!CandidateMarkerClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector  MarkerLocation = CurrentWorldState.CandidateLocation;
	const FRotator MakrerRotation = FRotator::ZeroRotator;

	if (!IsValid(ActiveCandidateMarker))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ActiveCandidateMarker = World->SpawnActor<AAmbientCandidateMarker>(
			CandidateMarkerClass,
			MarkerLocation,
			MakrerRotation,
			SpawnParams
		);
	}
	else
	{
		ActiveCandidateMarker->SetActorLocation(MarkerLocation);
		ActiveCandidateMarker->SetActorRotation(MakrerRotation);
	}

	if (IsValid(ActiveCandidateMarker))
	{
		ActiveCandidateMarker->SetMarkerActive(true);
	}
}

void AAmbientDirector::DestroyCandidateMarker()
{
	if (IsValid(ActiveCandidateMarker))
	{
		ActiveCandidateMarker->Destroy();
	}

	ActiveCandidateMarker = nullptr;
}

void AAmbientDirector::PrintWorldStateDebug() const
{
	if (!GEngine)
	{
		return;
	}

	FString Message;

	if (CurrentWorldState.bHasPlayerPawn && CurrentWorldState.bHasCandidateLocation)
	{
		const auto State = CurrentWorldState;

		const FString CandidateStatus = CurrentWorldState.bCandidateValid
			? TEXT("ACCEPTED")
			: TEXT("REJECTED");

		const FString MarkerStatus = IsValid(ActiveCandidateMarker)
			? TEXT("Spawned")
			: TEXT("None");

		const FString RegionNameString = CurrentWorldState.bHasCurrentRegion
			? CurrentWorldState.CurrentRegionName.ToString()
			: TEXT("None");

		const FString SummaryLine = FString::Printf(
			TEXT("[AD] Candidate %s | Region=%s | Time=%.1fs | Marker=%s | Reason=%s"),
			*CandidateStatus,
			*RegionNameString,
			State.GameTimeSeconds,
			*MarkerStatus,
			*State.CandidateRejectReason
		);

		const FString DetailLine = FString::Printf(
			TEXT("     CandidateLoc=(X=%.0f Y=%.0f Z=%.0f) | PlayerLoc=(X=%.0f Y=%.0f Z=%.0f) | Speed2D=%.0f | RequestedDist=%.0f | UsedDist=%.0f | Dist2D=%.0f | Grounded=%s"),
			State.CandidateLocation.X,
			State.CandidateLocation.Y,
			State.CandidateLocation.Z,

			State.PlayerLocation.X,
			State.PlayerLocation.Y,
			State.PlayerLocation.Z,

			State.PlayerSpeed2D,
			State.RequestedCandidateDistance,
			State.UsedCandidateDistance,
			State.CandidateDistance2D,
			State.bCandidateProjectedToGround ? TEXT("true") : TEXT("false")
		);

		Message = SummaryLine + LINE_TERMINATOR + DetailLine;
	}
	else if (CurrentWorldState.bHasPlayerPawn)
	{
		const FString RegionNameString = CurrentWorldState.bHasCurrentRegion
			? CurrentWorldState.CurrentRegionName.ToString()
			: TEXT("None");

		Message = FString::Printf(
			TEXT("[AD] World State | Region=%s | Time=%.1fs | Player found | No candidate location | Reason=%s"),
			*RegionNameString,
			CurrentWorldState.GameTimeSeconds,
			*CurrentWorldState.CandidateRejectReason
		);
	}
	else
	{
		Message = FString::Printf(
			TEXT("[AD] World State | Time=%.1fs | No player pawn found"),
			CurrentWorldState.GameTimeSeconds
		);
	}

	GEngine->AddOnScreenDebugMessage(
		1001,
		UpdateInterval * 0.85f,
		CurrentWorldState.bCandidateValid ? FColor::Green : FColor::Red,
		Message
	);

	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
}

void AAmbientDirector::PrintEncounterDebug() const
{
	if (!GEngine)
	{
		return;
	}

	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();

	const FString RegionNameString = CurrentWorldState.bHasCurrentRegion
		? CurrentWorldState.CurrentRegionName.ToString()
		: TEXT("None");

	const FString PointNameString = CurrentWorldState.bHasSelectedEncounterPoint
		? CurrentWorldState.SelectedEncounterPointName.ToString()
		: RuntimeEncounterPointName != NAME_None
		? RuntimeEncounterPointName.ToString()
		: TEXT("None");

	const FString ActorString = IsValid(ActivePrototypeEncounter)
		? TEXT("Yes")
		: TEXT("No");

	const FString Message = FString::Printf(
		TEXT("[AD] Encounter | Def=%s | State=%s | Region=%s | Point=%s | Actor=%s | Dist=%.0f | Cleanup=%.1fs | Cooldown=%.1fs | Starts=%d | Finishes=%d | Reason=%s"),
		*Definition.EncounterId.ToString(),
		*GetPrototypeRuntimeStateString(),
		*RegionNameString,
		*PointNameString,
		*ActorString,
		CurrentWorldState.DistanceToPrototypeEncounter,
		CurrentWorldState.PrototypeCleanupRemaining,
		CurrentWorldState.PrototypeCooldownRemaining,
		PrototypeEncounterStartCount,
		PrototypeEncounterFinishCount,
		*CurrentWorldState.PrototypeEncounterRuntimeReason
	);

	FColor MessageColor = FColor::Cyan;

	switch (PrototypeEncounterState)
	{
	case EAmbientEncounterRuntimeState::Waiting:
		MessageColor = FColor::Yellow;
		break;

	case EAmbientEncounterRuntimeState::Active:
		MessageColor = FColor::Green;
		break;

	case EAmbientEncounterRuntimeState::Cleanup:
		MessageColor = FColor::Orange;
		break;

	case EAmbientEncounterRuntimeState::Cooldown:
		MessageColor = FColor::Red;
		break;

	default:
		MessageColor = FColor::Cyan;
		break;
	}

	GEngine->AddOnScreenDebugMessage(
		1002,
		UpdateInterval * 0.85f,
		MessageColor,
		Message
	);

	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
}

void AAmbientDirector::PrintEncounterHistoryDebug() const
{
	if (!GEngine)
	{
		return;
	}

	FString LastHistoryString = TEXT("None");

	if (PrototypeEncounterHistory.Num() > 0)
	{
		const FAmbientEncounterHistoryEntry& LastEntry =
			PrototypeEncounterHistory[0];

		LastHistoryString = FString::Printf(
			TEXT("%s finished at %.1fs from %s"),
			*LastEntry.EncounterId.ToString(),
			LastEntry.FinishedAtTimeSeconds,
			*LastEntry.SourcePointName.ToString()
		);
	}

	const FString Message = FString::Printf(
		TEXT("[AD] History | Starts=%d | Finishes=%d | Entries=%d | Last=%s"),
		PrototypeEncounterStartCount,
		PrototypeEncounterFinishCount,
		PrototypeEncounterHistory.Num(),
		*LastHistoryString
	);

	GEngine->AddOnScreenDebugMessage(
		1003,
		UpdateInterval * 0.85f,
		FColor::Silver,
		Message
	);

	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
}

void AAmbientDirector::PrintSelectionDebug() const
{
	if (!GEngine)
	{
		return;
	}

	FString WinnerString = TEXT("None");

	if (bHasSelectedEncounterDefinition)
	{
		WinnerString = SelectedEncounterDefinition.EncounterId.ToString();
	}

	FString Message = FString::Printf(
		TEXT("[AD] Selection | Candidates=%d | Winner=%s | Score=%.1f | Reason=%s"),
		LastSelectionDebugEntries.Num(),
		*WinnerString,
		SelectedEncounterScore,
		*SelectedEncounterReason
	);

	if (bHasSelectedEncounterDefinition)
	{
		for (const FAmbientEncounterSelectionDebugEntry& Entry : LastSelectionDebugEntries)
		{
			if (Entry.bAccepted && Entry.EncounterId == SelectedEncounterDefinition.EncounterId)
			{
				Message += FString::Printf(
					TEXT(" | Detail=%s"),
					*Entry.Reason
				);
				break;
			}
		}
	}

	GEngine->AddOnScreenDebugMessage(
		1004,
		UpdateInterval * 0.85f,
		bHasSelectedEncounterDefinition ? FColor::Emerald : FColor::Red,
		Message
	);

	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);

	for (const FAmbientEncounterSelectionDebugEntry& Entry : LastSelectionDebugEntries)
	{
		const FString EntryMessage = FString::Printf(
			TEXT("[AD] SelectionCandidate | Accepted=%s | Encounter=%s | Point=%s | Score=%.1f | Dist=%.0f | Reason=%s"),
			Entry.bAccepted ? TEXT("true") : TEXT("false"),
			*Entry.EncounterId.ToString(),
			*Entry.PointName.ToString(),
			Entry.Score,
			Entry.DistanceToPoint,
			*Entry.Reason
		);

		UE_LOG(LogTemp, Log, TEXT("%s"), *EntryMessage);
	}
}

void AAmbientDirector::DrawCandidateDebug() const
{
	if (!CurrentWorldState.bHasCandidateLocation)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float DebugLifeTime = FMath::Max(0.05f, UpdateInterval * 0.9f);
	const FColor CandidateColor = CurrentWorldState.bCandidateValid
		? FColor::Green
		: FColor::Red;

	const FVector MarkerLocation = CurrentWorldState.bCandidateProjectedToGround
		? CurrentWorldState.CandidateLocation + FVector::UpVector * CandidateDebugRadius
		: CurrentWorldState.CandidateLocation;

	DrawDebugSphere(
		World,
		MarkerLocation,
		CandidateDebugRadius,
		16,
		CandidateColor,
		false,
		DebugLifeTime,
		0,
		2.0f
	);

	DrawDebugLine(
		World,
		CurrentWorldState.PlayerLocation,
		MarkerLocation,
		CandidateColor,
		false,
		DebugLifeTime,
		0,
		1.5f
	);

	DrawDebugSphere(
		World,
		CurrentWorldState.RawCandidateLocation,
		20.0f,
		8,
		FColor::Yellow,
		false,
		DebugLifeTime,
		0,
		1.0f
	);

	const FString WorldLabel = CurrentWorldState.bCandidateValid
		? TEXT("Candidate ACCEPTED")
		: FString::Printf(TEXT("Candidate REJECTED\n%s"), *CurrentWorldState.CandidateRejectReason);

	DrawDebugString(
		World,
		MarkerLocation + FVector::UpVector * 90.0f,
		WorldLabel,
		nullptr,
		CandidateColor,
		DebugLifeTime,
		true
	);
}

void AAmbientDirector::DrawRegionDebug() const
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float DebugLifeTime = FMath::Max(0.05f, UpdateInterval * 0.9f);

	for (TActorIterator<AAmbientRegionVolume> RegionIt(World); RegionIt; ++RegionIt)
	{
		const AAmbientRegionVolume* Region = *RegionIt;

		if (!IsValid(Region))
		{
			continue;
		}

		const UBoxComponent* RegionBounds = Region->GetRegionBounds();

		if (!RegionBounds)
		{
			continue;
		}

		const bool bIsCurrentRegion = Region == CurrentRegion;

		const FColor RegionColor = bIsCurrentRegion
			? Region->GetRegionDebugColor().ToFColor(true)
			: FColor(80, 80, 80);

		const float Thickness = bIsCurrentRegion ? 4.0f : 1.0f;

		DrawDebugBox(
			World,
			RegionBounds->GetComponentLocation(),
			RegionBounds->GetScaledBoxExtent(),
			RegionBounds->GetComponentQuat(),
			RegionColor,
			false,
			DebugLifeTime,
			0,
			Thickness
		);

		if (bIsCurrentRegion)
		{
			const FVector LabelLocation =
				RegionBounds->GetComponentLocation()
				+ FVector::UpVector * (RegionBounds->GetScaledBoxExtent().Z + 80.0f);

			const FString Label = FString::Printf(
				TEXT("Current Region: %s"),
				*Region->GetRegionName().ToString()
			);

			DrawDebugString(
				World,
				LabelLocation,
				Label,
				nullptr,
				RegionColor,
				DebugLifeTime,
				true
			);
		}
	}

}

void AAmbientDirector::DrawEncounterPointDebug() const
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float DebugLifeTime = FMath::Max(0.05f, UpdateInterval * 0.9f);

	for (TActorIterator<AAmbientEncounterPoint> PointIt(World); PointIt; ++PointIt)
	{
		const AAmbientEncounterPoint* Point = *PointIt;

		if (!IsValid(Point))
		{
			continue;
		}

		const bool bIsSelected = Point == SelectedEncounterPoint;

		const FColor PointColor = !Point->IsPointEnabled()
			? FColor(80, 80, 80)
			: bIsSelected
			? FColor::Orange
			: FColor::Blue;

		const float Thickness = bIsSelected ? 4.0f : 1.5f;

		const FVector PointLocation = Point->GetActorLocation();
		const FVector MarkerLocation = PointLocation + FVector::UpVector * 80.0f;
		const FVector ArrowStart = MarkerLocation;
		const FVector ArrowEnd =
			MarkerLocation + Point->GetActorForwardVector() * 220.0f;

		DrawDebugSphere(
			World,
			MarkerLocation,
			Point->GetDebugRadius(),
			16,
			PointColor,
			false,
			DebugLifeTime,
			0,
			Thickness
		);

		DrawDebugDirectionalArrow(
			World,
			ArrowStart,
			ArrowEnd,
			60.0f,
			PointColor,
			false,
			DebugLifeTime,
			0,
			Thickness
		);

		if (bIsSelected)
		{
			const FString Label = FString::Printf(
				TEXT("Selected EP: %s"),
				*Point->GetPointName().ToString()
			);

			DrawDebugString(
				World,
				MarkerLocation + FVector::UpVector * 120.0f,
				Label,
				nullptr,
				PointColor,
				DebugLifeTime,
				true
			);
		}
	}
}

void AAmbientDirector::DrawEncounterRuntimeDebug() const
{
	if (!IsValid(ActivePrototypeEncounter))
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float DebugLifeTime = FMath::Max(0.05f, UpdateInterval * 0.9f);
	const FVector EncounterLocation = ActivePrototypeEncounter->GetActorLocation();

	FColor StateColor = FColor::White;

	switch (PrototypeEncounterState)
	{
	case EAmbientEncounterRuntimeState::Waiting:
		StateColor = FColor::Yellow;
		break;

	case EAmbientEncounterRuntimeState::Active:
		StateColor = FColor::Green;
		break;

	case EAmbientEncounterRuntimeState::Cleanup:
		StateColor = FColor::Orange;
		break;

	case EAmbientEncounterRuntimeState::Cooldown:
		StateColor = FColor::Red;
		break;

	default:
		StateColor = FColor::White;
		break;
	}

	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();

	DrawDebugSphere(
		World,
		EncounterLocation,
		Definition.PlayerEngageDistance,
		24,
		FColor::Green,
		false,
		DebugLifeTime,
		0,
		1.5f
	);

	DrawDebugSphere(
		World,
		EncounterLocation,
		Definition.PlayerLeaveDistance,
		32,
		FColor::Red,
		false,
		DebugLifeTime,
		0,
		1.5f
	);

	const FString Label = FString::Printf(
		TEXT("Encounter State: %s\nDef: %s\nEngage <= %.0f cm | Leave >= %.0f cm"),
		*GetPrototypeRuntimeStateString(),
		*Definition.EncounterId.ToString(),
		Definition.PlayerEngageDistance,
		Definition.PlayerLeaveDistance
	);

	DrawDebugString(
		World,
		EncounterLocation + FVector::UpVector * 220.0f,
		Label,
		nullptr,
		StateColor,
		DebugLifeTime,
		true
	);
}
