
#include "AmbientDirector.h"

#include "Ambient_UE5.h"
#include "AmbientCandidateMarker.h"
#include "AmbientDirectorSaveGame.h"
#include "AmbientEncounterDefinitionData.h"
#include "AmbientEncounterPoint.h"
#include "AmbientEncounterRuntimeInterface.h"
#include "AmbientPlaceholderEncounter.h"
#include "AmbientRegionVolume.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
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
	DestroyCandidateMarker();
	DestroyPrototypeEncounter();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorldStateTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AAmbientDirector::SaveDirectorStateToSlot()
{
	FString Reason;
	const bool bSaved = SaveDirectorStateInternal(Reason);

	PrintSaveDebugMessage(
		FString::Printf(
			TEXT("SaveDirectorStateToSlot | %s"),
			*Reason
		),
		bSaved
	);
}

void AAmbientDirector::LoadDirectorStateFromSlot()
{
	FString Reason;
	const bool bLoaded = LoadDirectorStateInternal(Reason);

	PrintSaveDebugMessage(
		FString::Printf(
			TEXT("LoadDirectorStateFromSlot | %s"),
			*Reason
		),
		bLoaded
	);
}

void AAmbientDirector::ClearDirectorSaveSlot()
{
	FString Reason;
	const bool bCleared = ClearDirectorSaveInternal(Reason);

	PrintSaveDebugMessage(
		FString::Printf(
			TEXT("ClearDirectorSaveSlot | %s"),
			*Reason
		),
		bCleared
	);
}

void AAmbientDirector::UpdateWorldState()
{
	CurrentWorldState = FAmbientWorldState();
	CurrentWorldState.CurrentEncounterBudgetUse = GetCurrentEncounterBudgetUse();
	CurrentWorldState.MaxEncounterBudget = MaxSimultaneousPrototypeEncounters;
	CurrentWorldState.GlobalPacingRemaining = GetGlobalPacingRemaining();
	CurrentWorldState.NearestRecentEncounterDistance = 0.0f;
	CurrentWorldState.bPacingAllowsNewEncounter = true;
	CurrentWorldState.PacingBlockReason = TEXT("Pacing not evaluated");

	CurrentRegion = nullptr;
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
	
	if (ShouldDrawPlacementDebug())
	{
		if (bDrawRegionDebug)
		{
			DrawRegionDebug();
		}

		if (bDrawCandidateDebug)
		{
			DrawCandidateDebug();
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

	bHasSelectedEncounterSpawnTransform = false;
	SelectedEncounterSpawnTransform = FTransform::Identity;
	SelectedEncounterLocationReason = TEXT("No selected encounter location");

	CurrentWorldState.bHasSelectedEncounterDefinition = false;
	CurrentWorldState.SelectedEncounterDefinitionId = NAME_None;
	CurrentWorldState.SelectedEncounterDefinitionScore = 0.0f;
	CurrentWorldState.SelectedEncounterDefinitionReason = TEXT("No encounter definition selected");

	CurrentWorldState.bHasSelectedEncounterPoint = false;
	CurrentWorldState.SelectedEncounterPointName = NAME_None;
	CurrentWorldState.SelectedEncounterPointLocation = FVector::ZeroVector;
	CurrentWorldState.SelectedEncounterPointReason = TEXT("No encounter point evaluated");

	CurrentWorldState.bHasSelectedEncounterLocation = false;
	CurrentWorldState.SelectedEncounterLocation = FVector::ZeroVector;
	CurrentWorldState.SelectedEncounterRotation = FRotator::ZeroRotator;
	CurrentWorldState.SelectedEncounterLocationSource = TEXT("None");
	CurrentWorldState.SelectedEncounterLocationReason = TEXT("No encounter location selected");

	bool bFoundBestCandidate = false;
	float BestScore = 0.0f;
	AAmbientEncounterPoint* BestPoint = nullptr;
	const UAmbientEncounterDefinitionData* BestAsset = nullptr;
	FAmbientEncounterDefinition BestDefinition;
	FTransform BestSpawnTransform = FTransform::Identity;
	FString BestLocationReason = TEXT("No location");

	auto EvaluateDefinition = [&](const UAmbientEncounterDefinitionData* DefinitionAsset, const FAmbientEncounterDefinition& Definition)
	{
		FAmbientEncounterSelectionDebugEntry DebugEntry;
		AAmbientEncounterPoint* CandidatePoint = nullptr;
		FTransform CandidateSpawnTransform = FTransform::Identity;

		const bool bAccepted = EvaluateEncounterDefinitionCandidate(
			Definition,
			DebugEntry,
			CandidatePoint,
			CandidateSpawnTransform
		);

		LastSelectionDebugEntries.Add(DebugEntry);

		if (!bAccepted)
		{
			return;
		}

		if (!bFoundBestCandidate || DebugEntry.Score > BestScore)
		{
			bFoundBestCandidate = true;
			BestScore			= DebugEntry.Score;
			BestPoint			= CandidatePoint;
			BestAsset			= DefinitionAsset;
			BestDefinition		= Definition;
			BestSpawnTransform	= CandidateSpawnTransform;
			BestLocationReason	= DebugEntry.LocationReason;
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
			DebugEntry.LocationReason = TEXT("No asset");
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

	if (!BestLocationReason.IsEmpty())
	{
		SelectedEncounterLocationReason = BestLocationReason;
	}

	if (!bFoundBestCandidate)
	{
		SelectedEncounterReason = TEXT("No accepted encounter definition candidate");

		CurrentWorldState.SelectedEncounterDefinitionReason = SelectedEncounterReason;
		CurrentWorldState.SelectedEncounterPointReason =
			TEXT("No point selected because no definition candidate won");
		CurrentWorldState.SelectedEncounterLocationReason =
			TEXT("No location selected because no definition candidate won");
		return;
	}

	SelectedEncounterPoint = BestPoint;
	SelectedEncounterDefinitionAsset = const_cast<UAmbientEncounterDefinitionData*>(BestAsset);
	SelectedEncounterDefinition = BestDefinition;
	bHasSelectedEncounterDefinition = true;
	SelectedEncounterScore = BestScore;
	SelectedEncounterSpawnTransform = BestSpawnTransform;
	bHasSelectedEncounterSpawnTransform = true;

	SelectedEncounterReason = FString::Printf(
		TEXT("Selected highest score candidate: %.1f"),
		BestScore
	);

	CurrentWorldState.bHasSelectedEncounterDefinition = true;
	CurrentWorldState.SelectedEncounterDefinitionId = BestDefinition.EncounterId;
	CurrentWorldState.SelectedEncounterDefinitionScore = BestScore;
	CurrentWorldState.SelectedEncounterDefinitionReason = SelectedEncounterReason;

	if (IsValid(BestPoint))
	{
		CurrentWorldState.bHasSelectedEncounterPoint = true;
		CurrentWorldState.SelectedEncounterPointName = BestPoint->GetPointName();
		CurrentWorldState.SelectedEncounterPointLocation = BestPoint->GetActorLocation();
		CurrentWorldState.SelectedEncounterPointReason =
			TEXT("Selected authored point from winning encounter definition");
	}
	else
	{
		CurrentWorldState.bHasSelectedEncounterPoint = false;
		CurrentWorldState.SelectedEncounterPointName = NAME_None;
		CurrentWorldState.SelectedEncounterPointLocation = FVector::ZeroVector;
		CurrentWorldState.SelectedEncounterPointReason =
			TEXT("Winning encounter used non-authored location source");
	}

	CurrentWorldState.bHasSelectedEncounterLocation = true;
	CurrentWorldState.SelectedEncounterLocation =
		BestSpawnTransform.GetLocation();
	CurrentWorldState.SelectedEncounterRotation =
		BestSpawnTransform.GetRotation().Rotator();

	CurrentWorldState.SelectedEncounterLocationSource =
		BestDefinition.LocationSource == EAmbientEncounterLocationSource::EnvironmentQuery
		? TEXT("EQS")
		: TEXT("AuthoredPoint");

	CurrentWorldState.SelectedEncounterLocationReason = BestLocationReason;
}

bool AAmbientDirector::EvaluateEncounterDefinitionCandidate(
	const FAmbientEncounterDefinition& Definition,
	FAmbientEncounterSelectionDebugEntry& OutDebugEntry,
	AAmbientEncounterPoint*& OutBestPoint,
	FTransform& OutSpawnTransform
)
{
	OutBestPoint		= nullptr;
	OutSpawnTransform	= FTransform::Identity;

	OutDebugEntry.bAccepted			= false;
	OutDebugEntry.EncounterId		= Definition.EncounterId;
	OutDebugEntry.PointName			= NAME_None;
	OutDebugEntry.Score = 0.0f;
	OutDebugEntry.DistanceToPoint	= 0.0f;
	OutDebugEntry.Reason			= TEXT("Not evaluated");
	OutDebugEntry.LocationSource	=
		Definition.LocationSource  == EAmbientEncounterLocationSource::EnvironmentQuery
		? TEXT("EQS")
		: TEXT("AuthoredPoint");
	OutDebugEntry.SelectedLocation	= FVector::ZeroVector;
	OutDebugEntry.LocationReason	= TEXT("No location evaluated");

	FString WorldMatchReason;

	if (!DoesEncounterDefinitionMatchCurrentWorld(Definition, WorldMatchReason))
	{
		OutDebugEntry.Reason = WorldMatchReason;
		OutDebugEntry.LocationReason = TEXT("World conditions rejected before location search");
		return false;
	}

	float DistanceToLocation = 0.0f;
	FString LocationReason;

	if (!FindSpawnTransformForDefinition(
		Definition,
		OutSpawnTransform,
		OutBestPoint,
		DistanceToLocation,
		LocationReason
	))
	{
		OutDebugEntry.Reason = LocationReason;
		OutDebugEntry.LocationReason = LocationReason;
		return false;
	}

	FString PacingReason;
	float GlobalPacingRemaining = 0.0f;
	float NearestHistoryDistance = 0.0f;

	if (!DoesCandidatePassDirectorPacing(
		OutSpawnTransform,
		PacingReason,
		GlobalPacingRemaining,
		NearestHistoryDistance
	))
	{
		OutDebugEntry.Reason = FString::Printf(
			TEXT("Rejected by Director pacing: %s"),
			*PacingReason
		);

		OutDebugEntry.LocationReason = LocationReason;
		OutDebugEntry.SelectedLocation = OutSpawnTransform.GetLocation();
		OutDebugEntry.DistanceToPoint = DistanceToLocation;

		CurrentWorldState.bPacingAllowsNewEncounter = false;
		CurrentWorldState.PacingBlockReason = PacingReason;
		CurrentWorldState.GlobalPacingRemaining = GlobalPacingRemaining;
		CurrentWorldState.NearestRecentEncounterDistance = NearestHistoryDistance;

		return false;
	}

	const float MinDistance = MinimumSpawnDistance;
	const float MaxDistance = FMath::Min(
		Definition.EncounterPointSearchRadius,
		MaximumSpawnDistance
	);

	const float DistanceRange = FMath::Max(1.0f, MaxDistance - MinDistance);
	const float DistanceAlpha = FMath::Clamp(
		(DistanceToLocation - MinDistance) / DistanceRange,
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

	OutDebugEntry.bAccepted			= true;
	OutDebugEntry.Score				= FinalScore;
	OutDebugEntry.DistanceToPoint	= DistanceToLocation;
	OutDebugEntry.SelectedLocation	= OutSpawnTransform.GetLocation();
	OutDebugEntry.LocationReason	= LocationReason;

	CurrentWorldState.bPacingAllowsNewEncounter = true;
	CurrentWorldState.PacingBlockReason = TEXT("Pacing passed");
	CurrentWorldState.GlobalPacingRemaining = GlobalPacingRemaining;
	CurrentWorldState.NearestRecentEncounterDistance = NearestHistoryDistance;

	if (IsValid(OutBestPoint))
	{
		OutDebugEntry.PointName = OutBestPoint->GetPointName();
	}

	OutDebugEntry.Reason = FString::Printf(
		TEXT("Accepted | Base=%.1f DistanceBonus=%.1f HistoryPenalty=%.1f | %s"),
		Definition.BaseSelectionScore,
		DistanceBonus,
		HistoryPenalty,
		*LocationReason
	);


	return true;
}

bool AAmbientDirector::FindSpawnTransformForDefinition(
	const FAmbientEncounterDefinition& Definition,
	FTransform& OutSpawnTransform,
	AAmbientEncounterPoint*& OutBestPoint,
	float& OutDistanceToLocation,
	FString& OutReason
) const
{
	OutSpawnTransform		= FTransform::Identity;
	OutBestPoint			= nullptr;
	OutDistanceToLocation	= 0.0f;
	OutReason				= TEXT("No location source evaluated");

	switch (Definition.LocationSource)
	{
	case EAmbientEncounterLocationSource::AuthoredPoint:
		return FindAuthoredPointSpawnTransformForDefinition(
			Definition,
			OutSpawnTransform,
			OutBestPoint,
			OutDistanceToLocation,
			OutReason
		);

	case EAmbientEncounterLocationSource::EnvironmentQuery:
		return FindEQSSpawnTransformForDefinition(
			Definition,
			OutSpawnTransform,
			OutDistanceToLocation,
			OutReason
		);

	default:
		OutReason = TEXT("Rejected: unknown encounter location source");
		return false;
	}
}

bool AAmbientDirector::FindAuthoredPointSpawnTransformForDefinition(
	const FAmbientEncounterDefinition& Definition,
	FTransform& OutSpawnTransform,
	AAmbientEncounterPoint*& OutBestPoint,
	float& OutDistanceToPoint,
	FString& OutReason
) const
{
	OutSpawnTransform	= FTransform::Identity;
	OutBestPoint		= nullptr;
	OutDistanceToPoint	= 0.0f;
	OutReason			= TEXT("No authored point evaluated");

	UWorld* World = GetWorld();

	if (!World)
	{
		OutReason = TEXT("Rejected: no world");
		return false;
	}

	const float MinDistance = MinimumSpawnDistance;
	const float MaxDistance = FMath::Min(
		Definition.EncounterPointSearchRadius,
		MaximumSpawnDistance
	);

	if (MaxDistance < MinDistance)
	{
		OutReason = FString::Printf(
			TEXT("Rejected: invalid authored-point distance range Min=%.0f Max=%.0f"),
			MinDistance,
			MaxDistance
		);
		return false;
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
		return false;
	}

	OutBestPoint		= BestPoint;
	OutDistanceToPoint	= FMath::Sqrt(BestDistanceSq);
	OutSpawnTransform	= BestPoint->GetEncounterSpawnTransform();

	OutReason = FString::Printf(
		TEXT("AuthoredPoint accepted | Point=%s Distance=%.0f cm"),
		*BestPoint->GetPointName().ToString(),
		OutDistanceToPoint
	);

	return true;
}

bool AAmbientDirector::FindEQSSpawnTransformForDefinition(
	const FAmbientEncounterDefinition& Definition, 
	FTransform& OutSpawnTransform, 
	float& OutDistanceToLocation, 
	FString& OutReason
) const
{
	OutSpawnTransform = FTransform::Identity;
	OutDistanceToLocation = 0.0f;
	OutReason = TEXT("No EQS query evaluated");

	if (!Definition.LocationQuery)
	{
		OutReason = TEXT("Rejected: no EQS query defined");
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutReason = TEXT("Rejected: no world for EQS Query");
		return false;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!IsValid(PlayerPawn))
	{
		OutReason = TEXT("Rejected: no player pawn for EQS Query");
		return false;
	}

	UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(World);

	if (!QueryManager)
	{
		OutReason = TEXT("Rejected: no EQS QueryManager");
		return false;
	}
	
	FEnvQueryRequest QueryRequest(Definition.LocationQuery, PlayerPawn);

	const TSharedPtr<FEnvQueryResult> QueryResult =
		QueryManager->RunInstantQuery(
			QueryRequest,
			Definition.EQSRunMode.GetValue()
		);

	if (!QueryResult.IsValid())
	{
		OutReason = TEXT("Rejected: EQS returned invalid result object");
		return false;
	}

	if (!QueryResult->IsSuccessful() || QueryResult->Items.Num() == 0)
	{
		OutReason = TEXT("Rejected: EQS query did not return a successful location result");
		return false;
	}

	const FVector RawEQSLocation = QueryResult->GetItemAsLocation(0);

	FVector FinalLocation = RawEQSLocation;
	FString ValidationReason = TEXT("EQS location accepted without Director Validation");

	if (Definition.bValidateEQSLocationWithDirectorRules)
	{
		if (!ValidateEQSLocationCandidate(RawEQSLocation, PlayerPawn, FinalLocation, ValidationReason))
		{
			OutReason = FString::Printf(
				TEXT("Rejected: EQS location failed Director validation | Reason=%s"),
				*ValidationReason
			);
			return false;
		}
	}

	FVector ToPlayer = CurrentWorldState.PlayerLocation - FinalLocation;
	ToPlayer.Z = 0.0f;

	const FRotator SpawnRotation =
		ToPlayer.IsNearlyZero()
		? FRotator::ZeroRotator
		: ToPlayer.Rotation();

	OutSpawnTransform = FTransform(SpawnRotation, FinalLocation, FVector::OneVector);
	OutDistanceToLocation = FVector::Dist2D(CurrentWorldState.PlayerLocation, FinalLocation);

	OutReason = FString::Printf(
		TEXT("EQS accepted | Raw=(X=%.0f Y=%.0f Z=%.0f) Final=(X=%.0f Y=%.0f Z=%.0f) Distance=%.0f cm | %s"),
		RawEQSLocation.X,
		RawEQSLocation.Y,
		RawEQSLocation.Z,
		FinalLocation.X,
		FinalLocation.Y,
		FinalLocation.Z,
		OutDistanceToLocation,
		*ValidationReason
	);

	return true;
}

bool AAmbientDirector::ValidateEQSLocationCandidate(
	const FVector& RawLocation, 
	const APawn* PlayerPawn, 
	FVector& OutValidatedLocation, 
	FString& OutReason
) const
{
	OutValidatedLocation = RawLocation;
	OutReason = TEXT("EQS location not validated");

	if (!IsValid(PlayerPawn))
	{
		OutReason = TEXT("No player pawn");
		return false;
	}

	FVector GroundLocation = FVector::ZeroVector;
	FHitResult GroundHit;

	if (!ProjectPointToGround(RawLocation, PlayerPawn, GroundLocation, GroundHit))
	{
		OutReason = TEXT("No ground found under EQS location");
		return false;
	}

	if (GroundHit.ImpactNormal.Z < MinimumGroundNormalZ)
	{
		OutReason = FString::Printf(
			TEXT("Ground too steep under EQS location: NormalZ=%.2f"),
			GroundHit.ImpactNormal.Z
		);
		return false;
	}

	const float Distance2D = FVector::Dist2D(
		CurrentWorldState.PlayerLocation, 
		GroundLocation
	);

	if (Distance2D < MinimumSpawnDistance)
	{
		OutReason = FString::Printf(
			TEXT("EQS location too close: Dist=%.0f Min=%.0f"),
			Distance2D,
			MinimumSpawnDistance
		);
		return false;
	}

	if (Distance2D > MaximumSpawnDistance)
	{
		OutReason = FString::Printf(
			TEXT("EQS location too far: Dist=%.0f Max=%.0f"),
			Distance2D,
			MaximumSpawnDistance
		);
		return false;
	}

	FHitResult PathBlockHit;

	if (IsPathToCandidateBlocked(PlayerPawn, GroundLocation, PathBlockHit))
	{
		OutReason = FString::Printf(
			TEXT("Blocked path to EQS location: %s"),
			*GetNameSafe(PathBlockHit.GetActor())
		);
		return false;
	}

	FHitResult AreaBlockHit;

	if (IsCandidateAreaBlocked(PlayerPawn, GroundLocation, AreaBlockHit))
	{
		OutReason = FString::Printf(
			TEXT("EQS location area occupied: %s"),
			*GetNameSafe(AreaBlockHit.GetActor())
		);
		return false;
	}

	OutValidatedLocation = GroundLocation;

	OutReason = FString::Printf(
		TEXT("Director validation passed | Grounded=true Dist=%.0f"),
		Distance2D
	);

	return true;
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

bool AAmbientDirector::DoesCandidatePassDirectorPacing(
	const FTransform& CandidateSpawnTransform, 
	FString& OutReason, 
	float& OutGlobalPacingRemaining, 
	float& OutNearestHistoryDistance
)const
{
	OutReason = TEXT("Pacing passed");
	OutGlobalPacingRemaining = 0.0f;
	OutNearestHistoryDistance = TNumericLimits<float>::Max();

	if (!bEnableDirectorPacing)
	{
		OutReason = TEXT("Director pacing disabled");
		return true;
	}

	const int32 SafeMaxBudget		= FMath::Max(0, MaxSimultaneousPrototypeEncounters);
	const int32 CurrentBudgetUse	= GetCurrentEncounterBudgetUse();

	if (CurrentBudgetUse >= SafeMaxBudget)
	{
		OutReason = FString::Printf(
			TEXT("Rejected: budget exceeded %d >= %d"),
			CurrentBudgetUse,
			SafeMaxBudget
		);
		return false;
	}

	OutGlobalPacingRemaining = GetGlobalPacingRemaining();

	if (OutGlobalPacingRemaining > 0.0f)
	{
		OutReason = FString::Printf(
			TEXT("Rejected: global pacing cooldown %.1f sec remaining"),
			OutGlobalPacingRemaining
		);
		return false;
	}

	const FVector CandidateLocation = CandidateSpawnTransform.GetLocation();
	OutNearestHistoryDistance		= GetNearestRecentEncounterDistance(CandidateLocation);

	if (bUseRecentEncounterSpacing &&
		MinimumDistanceFromRecentEncounterLocations > 0.0f &&
		PrototypeEncounterHistory.Num() > 0 &&
		OutNearestHistoryDistance < MinimumDistanceFromRecentEncounterLocations)
	{
		OutReason = FString::Printf(
			TEXT("Rejected: too close to recent encounter %.0f < %.0f cm"),
			OutNearestHistoryDistance,
			MinimumDistanceFromRecentEncounterLocations
		);
		return false;
	}

	OutReason = TEXT("Pacing passed");
	return true;
}

int32 AAmbientDirector::GetCurrentEncounterBudgetUse() const
{
	// Slice 31 supports one prototype encounter actor.
	// Future multi-encounter support should count all active runtime encounters here.
	return IsValid(ActivePrototypeEncounter) ? 1 : 0;
}

float AAmbientDirector::GetGlobalPacingRemaining() const
{
	if (!bEnableDirectorPacing)
	{
		return 0.0f;
	}

	if (MinimumSecondsBetweenEncounterStarts <= 0.0f)
	{
		return 0.0f;
	}

	const float Now		= CurrentWorldState.GameTimeSeconds;
	const float Elapsed = Now - LastAnyEncounterStartTimeSeconds;

	return FMath::Max(0.0f, MinimumSecondsBetweenEncounterStarts - Elapsed);
}

float AAmbientDirector::GetNearestRecentEncounterDistance(const FVector& CandidateLocation) const
{
	if (PrototypeEncounterHistory.Num() == 0)
	{
		return TNumericLimits<float>::Max();
	}

	float NearestDistance = TNumericLimits<float>::Max();

	for (const FAmbientEncounterHistoryEntry& Entry : PrototypeEncounterHistory)
	{
		const float Distance = FVector::Dist2D(CandidateLocation, Entry.EncounterLocation);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
		}
	}

	return NearestDistance;
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

		RuntimeEncounterDefinition = SelectedEncounterDefinition;
		bHasRuntimeEncounterDefinition = true;

		const FTransform SpawnTransform = SelectedEncounterSpawnTransform;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ActivePrototypeEncounter = World->SpawnActor<AActor>(
			RuntimeEncounterDefinition.EncounterClass,
			SpawnTransform,
			SpawnParams
		);

		if (!IsValid(ActivePrototypeEncounter))
		{
			CurrentWorldState.PrototypeEncounterBlockReason = TEXT("Spawn failed");
			return false;
		}

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
	const FRotator MarkerRotation = FRotator::ZeroRotator;

	if (!IsValid(ActiveCandidateMarker))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ActiveCandidateMarker = World->SpawnActor<AAmbientCandidateMarker>(
			CandidateMarkerClass,
			MarkerLocation,
			MarkerRotation,
			SpawnParams
		);
	}
	else
	{
		ActiveCandidateMarker->SetActorLocation(MarkerLocation);
		ActiveCandidateMarker->SetActorRotation(MarkerRotation);
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

bool AAmbientDirector::SaveDirectorStateInternal(FString& OutReason) const
{
	if (DirectorSaveSlotName.IsEmpty())
	{
		OutReason = TEXT("Save slot name is empty");
		return false;
	}

	USaveGame* RawSaveObject = UGameplayStatics::CreateSaveGameObject(
		UAmbientDirectorSaveGame::StaticClass()
	);

	UAmbientDirectorSaveGame* SaveGameObject = Cast<UAmbientDirectorSaveGame>(RawSaveObject);

	if (!SaveGameObject)
	{
		OutReason = TEXT("Failed to create AmbientDirectorSaveGame object");
		return false;
	}

	BuildDirectorSaveSnapshot(SaveGameObject->DirectorSnapshot);

	const bool bSaved = UGameplayStatics::SaveGameToSlot(
		SaveGameObject,
		DirectorSaveSlotName,
		DirectorSaveUserIndex
	);

	OutReason = bSaved
		? FString::Printf(
			TEXT("Saved slot '%s'"),
			*DirectorSaveSlotName
		)
		: FString::Printf(
			TEXT("Failed to save slot '%s'"),
			*DirectorSaveSlotName
		);

	return bSaved;
}

bool AAmbientDirector::LoadDirectorStateInternal(FString& OutReason)
{
	if (DirectorSaveSlotName.IsEmpty())
	{
		OutReason = TEXT("Save slot name is empty");
		return false;
	}

	const bool bSaveExists = UGameplayStatics::DoesSaveGameExist(
		DirectorSaveSlotName,
		DirectorSaveUserIndex
	);

	if (!bSaveExists)
	{
		OutReason = FString::Printf(
			TEXT("No save exists in slot '%s'"),
			*DirectorSaveSlotName
		);
		return false;
	}

	USaveGame* RawSaveObject = UGameplayStatics::LoadGameFromSlot(
		DirectorSaveSlotName,
		DirectorSaveUserIndex
	);

	UAmbientDirectorSaveGame* LoadedSaveGame =
		Cast<UAmbientDirectorSaveGame>(RawSaveObject);

	if (!LoadedSaveGame)
	{
		OutReason = FString::Printf(
			TEXT("Slot '%s' did not contain AmbientDirectorSaveGame data"),
			*DirectorSaveSlotName
		);
		return false;
	}

	return ApplyDirectorSaveSnapshot(LoadedSaveGame->DirectorSnapshot, OutReason);
}

bool AAmbientDirector::ClearDirectorSaveInternal(FString& OutReason) const
{
	if (DirectorSaveSlotName.IsEmpty())
	{
		OutReason = TEXT("Save slot name is empty");
		return false;
	}

	const bool bSaveExists = UGameplayStatics::DoesSaveGameExist(
		DirectorSaveSlotName,
		DirectorSaveUserIndex
	);

	if (!bSaveExists)
	{
		OutReason = FString::Printf(
			TEXT("No save exists in slot '%s'"),
			*DirectorSaveSlotName
		);
		return false;
	}

	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(
		DirectorSaveSlotName,
		DirectorSaveUserIndex
	);

	OutReason = bDeleted
		? FString::Printf(
			TEXT("Deleted save slot '%s'"),
			*DirectorSaveSlotName
		)
		: FString::Printf(
			TEXT("Failed to delete save slot '%s'"),
			*DirectorSaveSlotName
		);

	return bDeleted;
}

void AAmbientDirector::BuildDirectorSaveSnapshot(FAmbientDirectorSaveSnapshot& OutSnapshot) const
{
	const float Now = CurrentWorldState.GameTimeSeconds;

	OutSnapshot.SaveVersion				= 1;
	OutSnapshot.SavedAtGameTimeSeconds	= Now;

	OutSnapshot.RuntimeState						= PrototypeEncounterState;
	OutSnapshot.bHadRuntimeEncounterActor			= IsValid(ActivePrototypeEncounter);
	OutSnapshot.bHadRuntimeEncounterDefinition		= bHasRuntimeEncounterDefinition;

	const FAmbientEncounterDefinition& Definition	= GetPrototypeEncounterDefinition();

	OutSnapshot.RuntimeEncounterId	= Definition.EncounterId;
	OutSnapshot.RuntimeRegionName	= RuntimeEncounterRegionName;
	OutSnapshot.RuntimePointName	= RuntimeEncounterPointName;

	if (IsValid(ActivePrototypeEncounter))
	{
		OutSnapshot.RuntimeEncounterLocation =
			ActivePrototypeEncounter->GetActorLocation();

		OutSnapshot.RuntimeEncounterRotation =
			ActivePrototypeEncounter->GetActorRotation();
	}
	else
	{
		OutSnapshot.RuntimeEncounterLocation = RuntimeEncounterLocation;
		OutSnapshot.RuntimeEncounterRotation = FRotator::ZeroRotator;
	}

	OutSnapshot.RuntimeEncounterLocationSource = RuntimeEncounterLocationSource;

	if (RuntimeEncounterStartedAtTimeSeconds > 0.0f)
	{
		OutSnapshot.RuntimeEncounterElapsedSeconds =
			FMath::Max(0.0f, Now - RuntimeEncounterStartedAtTimeSeconds);
	}
	else
	{
		OutSnapshot.RuntimeEncounterElapsedSeconds = 0.0f;
	}

	OutSnapshot.CleanupRemainingSeconds		= 0.0f;
	OutSnapshot.CooldownRemainingSeconds	= 0.0f;

	if (PrototypeEncounterState == EAmbientEncounterRuntimeState::Cleanup)
	{
		OutSnapshot.CleanupRemainingSeconds =
			FMath::Max(0.0f, PrototypeCleanupEndTimeSeconds - Now);
	}

	if (PrototypeEncounterState == EAmbientEncounterRuntimeState::Cooldown)
	{
		OutSnapshot.CooldownRemainingSeconds =
			FMath::Max(0.0f, PrototypeCooldownEndTimeSeconds - Now);
	}

	OutSnapshot.GlobalPacingRemainingSeconds	= GetGlobalPacingRemaining();
	OutSnapshot.PendingFinishReason				= PendingPrototypeFinishReason;
	OutSnapshot.PrototypeEncounterStartCount	= PrototypeEncounterStartCount;
	OutSnapshot.PrototypeEncounterFinishCount	= PrototypeEncounterFinishCount;
	OutSnapshot.PrototypeEncounterHistory		= PrototypeEncounterHistory;
}

bool AAmbientDirector::ApplyDirectorSaveSnapshot(
	const FAmbientDirectorSaveSnapshot& Snapshot, 
	FString& OutReason
)
{
	if (Snapshot.SaveVersion != 1)
	{
		OutReason = FString::Printf(
			TEXT("Unsupported save version: %d"),
			Snapshot.SaveVersion
		);
		return false;
	}

	DestroyPrototypeEncounter();
	PrototypeEncounterHistory		= Snapshot.PrototypeEncounterHistory;
	PrototypeEncounterStartCount	= Snapshot.PrototypeEncounterStartCount;
	PrototypeEncounterFinishCount	= Snapshot.PrototypeEncounterFinishCount;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (Snapshot.GlobalPacingRemainingSeconds > 0.0f &&
		MinimumSecondsBetweenEncounterStarts > 0.0f)
	{
		LastAnyEncounterStartTimeSeconds =
			Now - FMath::Max(
				0.0f,
				MinimumSecondsBetweenEncounterStarts -
				Snapshot.GlobalPacingRemainingSeconds
			);
	}
	else
	{
		LastAnyEncounterStartTimeSeconds = -999999.0f;
	}

	PrototypeCleanupEndTimeSeconds	= 0.0f;
	PrototypeCooldownEndTimeSeconds = 0.0f;
	PendingPrototypeFinishReason	= Snapshot.PendingFinishReason;

	RuntimeEncounterStartedAtTimeSeconds	= 0.0f;
	RuntimeEncounterRegionName				= Snapshot.RuntimeRegionName;
	RuntimeEncounterPointName				= Snapshot.RuntimePointName;
	RuntimeEncounterLocation				= Snapshot.RuntimeEncounterLocation;
	RuntimeEncounterLocationSource			= Snapshot.RuntimeEncounterLocationSource;

	RuntimeEncounterDefinition		= FAmbientEncounterDefinition();
	bHasRuntimeEncounterDefinition	= false;

	if (Snapshot.RuntimeEncounterId == NAME_None)
	{
		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;
		OutReason = TEXT("Loaded save with history only; no runtime encounter ID");
		return true;
	}

	FAmbientEncounterDefinition RestoredDefinition;

	if (!TryFindEncounterDefinitionById(Snapshot.RuntimeEncounterId, RestoredDefinition))
	{
		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;
		OutReason = FString::Printf(
			TEXT("Loaded history, but could not find encounter definition '%s'"),
			*Snapshot.RuntimeEncounterId.ToString()
		);

		return false;
	}

	RuntimeEncounterDefinition		= RestoredDefinition;
	bHasRuntimeEncounterDefinition	= true;

	switch (Snapshot.RuntimeState)
	{
	case EAmbientEncounterRuntimeState::Waiting:
	case EAmbientEncounterRuntimeState::Active:
	case EAmbientEncounterRuntimeState::Cleanup:
	{
		return RestoreRuntimeEncounterFromSave(
			Snapshot,
			RestoredDefinition,
			OutReason
		);
	}

	case EAmbientEncounterRuntimeState::Cooldown:
	{
		DestroyPrototypeEncounter();

		if (Snapshot.CooldownRemainingSeconds > 0.0f)
		{
			PrototypeEncounterState			= EAmbientEncounterRuntimeState::Cooldown;
			PrototypeCooldownEndTimeSeconds = Now + Snapshot.CooldownRemainingSeconds;

			OutReason = FString::Printf(
				TEXT("Loaded cooldown state for %s with %.1fs remaining"),
				*Snapshot.RuntimeEncounterId.ToString(),
				Snapshot.CooldownRemainingSeconds
			);
		}
		else
		{
			PrototypeEncounterState			= EAmbientEncounterRuntimeState::Waiting;
			PrototypeCooldownEndTimeSeconds = 0.0f;
			bHasRuntimeEncounterDefinition	= false;

			OutReason = FString::Printf(
				TEXT("Loaded expired cooldown for %s; returning to Waiting"),
				*Snapshot.RuntimeEncounterId.ToString()
			);
		}

		return true;
	}

	default:
	{
		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;

		OutReason = TEXT("Loaded unknown runtime state; corrected to Waiting");
		return false;
	}
	}
}

bool AAmbientDirector::RestoreRuntimeEncounterFromSave(
	const FAmbientDirectorSaveSnapshot& Snapshot, 
	const FAmbientEncounterDefinition& RestoredDefinition, 
	FString& OutReason
)
{
	UWorld* World = GetWorld();

	if (!World)
	{
		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;
		OutReason = TEXT("Cannot restore runtime encounter: no world");
		return false;
	}

	if (!RestoredDefinition.EncounterClass)
	{
		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;
		OutReason = TEXT("Cannot restore runtime encounter: EncounterClass missing");
		return false;
	}

	if (!RestoredDefinition.EncounterClass->ImplementsInterface(
		UAmbientEncounterRuntimeInterface::StaticClass()
	))
	{
		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;

		OutReason = FString::Printf(
			TEXT("Cannot restore: class %s does not implement AmbientEncounterRuntimeInterface"),
			*GetNameSafe(RestoredDefinition.EncounterClass.Get())
		);

		return false;
	}

	const FTransform RestoreTransform(
		Snapshot.RuntimeEncounterRotation,
		Snapshot.RuntimeEncounterLocation,
		FVector::OneVector
	);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActivePrototypeEncounter = World->SpawnActor<AActor>(
		RestoredDefinition.EncounterClass,
		RestoreTransform,
		SpawnParams
	);

	if (!IsValid(ActivePrototypeEncounter))
	{
		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;
		OutReason = TEXT("Failed to spawn restored runtime encounter Actor");
		return false;
	}

	const float Now = World->GetTimeSeconds();
	RuntimeEncounterStartedAtTimeSeconds	= Now - FMath::Max(0.0f, Snapshot.RuntimeEncounterElapsedSeconds);
	RuntimeEncounterRegionName				= Snapshot.RuntimeRegionName;
	RuntimeEncounterPointName				= Snapshot.RuntimePointName;
	RuntimeEncounterLocation				= Snapshot.RuntimeEncounterLocation;
	RuntimeEncounterLocationSource			= Snapshot.RuntimeEncounterLocationSource;

	FAmbientEncounterRuntimeContext RuntimeContext;
	RuntimeContext.DirectorActor		= this;
	RuntimeContext.EncounterId			= RestoredDefinition.EncounterId;
	RuntimeContext.RegionName			= RuntimeEncounterRegionName;
	RuntimeContext.SourcePointName		= RuntimeEncounterPointName;
	RuntimeContext.SpawnLocation		= Snapshot.RuntimeEncounterLocation;
	RuntimeContext.StartedAtTimeSeconds = RuntimeEncounterStartedAtTimeSeconds;
	RuntimeContext.EncounterTags		= RestoredDefinition.EncounterTags;

	IAmbientEncounterRuntimeInterface::Execute_InitializeAmbientEncounter(
		ActivePrototypeEncounter,
		RuntimeContext
	);

	PrototypeEncounterState = Snapshot.RuntimeState;

	switch (PrototypeEncounterState)
	{
	case EAmbientEncounterRuntimeState::Waiting:
	{
		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterWaiting(
			ActivePrototypeEncounter
		);

		OutReason = FString::Printf(
			TEXT("Restored Waiting encounter %s"),
			*RestoredDefinition.EncounterId.ToString()
		);

		return true;
	}

	case EAmbientEncounterRuntimeState::Active:
	{
		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterActivated(
			ActivePrototypeEncounter
		);

		OutReason = FString::Printf(
			TEXT("Restored Active encounter %s"),
			*RestoredDefinition.EncounterId.ToString()
		);

		return true;
	}

	case EAmbientEncounterRuntimeState::Cleanup:
	{
		PendingPrototypeFinishReason = Snapshot.PendingFinishReason;

		PrototypeCleanupEndTimeSeconds =
			Now + FMath::Max(0.0f, Snapshot.CleanupRemainingSeconds);

		IAmbientEncounterRuntimeInterface::Execute_OnAmbientEncounterCleanup(
			ActivePrototypeEncounter,
			PendingPrototypeFinishReason
		);

		OutReason = FString::Printf(
			TEXT("Restored Cleanup encounter %s with %.1fs remaining"),
			*RestoredDefinition.EncounterId.ToString(),
			Snapshot.CleanupRemainingSeconds
		);

		return true;
	}

	default:
	{
		DestroyPrototypeEncounter();
		PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;

		OutReason = TEXT("Restore runtime called for unsupported state");
		return false;
	}
	}
}

bool AAmbientDirector::TryFindEncounterDefinitionById(
	FName EncounterId, 
	FAmbientEncounterDefinition& OutDefinition
) const
{
	if (EncounterId == NAME_None)
	{
		return false;
	}

	for (const TObjectPtr<UAmbientEncounterDefinitionData>& DefinitionAsset
		: EncounterDefinitionAssets)
	{
		if (!IsValid(DefinitionAsset))
		{
			continue;
		}

		if (DefinitionAsset->Definition.EncounterId == EncounterId)
		{
			OutDefinition = DefinitionAsset->Definition;
			return true;
		}
	}

	if (IsValid(PrototypeEncounterDefinitionAsset) &&
		PrototypeEncounterDefinitionAsset->Definition.EncounterId == EncounterId)
	{
		OutDefinition = PrototypeEncounterDefinitionAsset->Definition;
		return true;
	}

	if (PrototypeEncounterDefinition.EncounterId == EncounterId)
	{
		OutDefinition = PrototypeEncounterDefinition;
		return true;
	}

	return false;
}

void AAmbientDirector::PrintSaveDebugMessage(const FString& Message, bool bSuccess) const
{
	if (!bPrintSaveDebug)
	{
		return;
	}

	const FString FullMessage = FString::Printf(
		TEXT("[AD Save] %s"),
		*Message
	);

	UE_LOG(LogAmbient_UE5, Log, TEXT("%s"), *FullMessage);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			1006,
			2.5f,
			bSuccess ? FColor::Green : FColor::Red,
			FullMessage
		);
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

void AAmbientDirector::DrawSelectedEncounterLocationDebug() const
{
	if (!bHasSelectedEncounterSpawnTransform)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float DebugLifeTime = FMath::Max(0.05f, UpdateInterval * 0.9f);

	const FVector Location =
		SelectedEncounterSpawnTransform.GetLocation();

	const bool bIsEQS =
		CurrentWorldState.SelectedEncounterLocationSource == TEXT("EQS");

	const FColor LocationColor =
		bIsEQS ? FColor::Purple : FColor::Cyan;

	const FVector MarkerLocation =
		Location + FVector::UpVector * 100.0f;

	DrawDebugSphere(
		World,
		MarkerLocation,
		90.0f,
		20,
		LocationColor,
		false,
		DebugLifeTime,
		0,
		3.0f
	);

	DrawDebugDirectionalArrow(
		World,
		MarkerLocation,
		MarkerLocation + SelectedEncounterSpawnTransform.GetRotation().GetForwardVector() * 220.0f,
		60.0f,
		LocationColor,
		false,
		DebugLifeTime,
		0,
		3.0f
	);

	const FString Label = FString::Printf(
		TEXT("Selected Location: %s\n%s"),
		*CurrentWorldState.SelectedEncounterLocationSource,
		*CurrentWorldState.SelectedEncounterLocationReason
	);

	DrawDebugString(
		World,
		MarkerLocation + FVector::UpVector * 130.0f,
		Label,
		nullptr,
		LocationColor,
		DebugLifeTime,
		true
	);
}
