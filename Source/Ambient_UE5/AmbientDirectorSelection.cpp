#include "AmbientDirector.h"

#include "AmbientEncounterDefinitionData.h"
#include "AmbientEncounterPoint.h"
#include "GameplayTagContainer.h"

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
				BestScore = DebugEntry.Score;
				BestPoint = CandidatePoint;
				BestAsset = DefinitionAsset;
				BestDefinition = Definition;
				BestSpawnTransform = CandidateSpawnTransform;
				BestLocationReason = DebugEntry.LocationReason;
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
	OutBestPoint = nullptr;
	OutSpawnTransform = FTransform::Identity;

	OutDebugEntry.bAccepted = false;
	OutDebugEntry.EncounterId = Definition.EncounterId;
	OutDebugEntry.PointName = NAME_None;
	OutDebugEntry.Score = 0.0f;
	OutDebugEntry.DistanceToPoint = 0.0f;
	OutDebugEntry.Reason = TEXT("Not evaluated");
	OutDebugEntry.LocationSource =
		Definition.LocationSource == EAmbientEncounterLocationSource::EnvironmentQuery
		? TEXT("EQS")
		: TEXT("AuthoredPoint");
	OutDebugEntry.SelectedLocation = FVector::ZeroVector;
	OutDebugEntry.LocationReason = TEXT("No location evaluated");

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

	OutDebugEntry.bAccepted = true;
	OutDebugEntry.Score = FinalScore;
	OutDebugEntry.DistanceToPoint = DistanceToLocation;
	OutDebugEntry.SelectedLocation = OutSpawnTransform.GetLocation();
	OutDebugEntry.LocationReason = LocationReason;

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