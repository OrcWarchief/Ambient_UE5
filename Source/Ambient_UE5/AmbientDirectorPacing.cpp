#include "AmbientDirector.h"

bool AAmbientDirector::DoesCandidatePassDirectorPacing(
	const FAmbientEncounterDefinition& Definition,
	const FTransform& CandidateSpawnTransform,
	FString& OutReason,
	float& OutGlobalPacingRemaining,
	float& OutNearestHistoryDistance
) const
{
	OutReason = TEXT("Pacing passed");
	OutGlobalPacingRemaining = 0.0f;
	OutNearestHistoryDistance = TNumericLimits<float>::Max();

	if (!bEnableDirectorPacing)
	{
		OutReason = TEXT("Director pacing disabled");
		return true;
	}

	const int32 SafeMaxBudget = FMath::Max(0, MaxSimultaneousPrototypeEncounters);
	const int32 CurrentBudgetUse = GetCurrentEncounterBudgetUse();

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

	const bool bShouldApplyRecentLocationSpacing =
		bUseRecentEncounterSpacing &&
		Definition.LocationSource == EAmbientEncounterLocationSource::EnvironmentQuery;

	if (bShouldApplyRecentLocationSpacing)
	{
		const FVector CandidateLocation = CandidateSpawnTransform.GetLocation();

		OutNearestHistoryDistance = GetNearestRecentEncounterDistance(CandidateLocation);

		if (
			MinimumDistanceFromRecentEncounterLocations > 0.0f &&
			OutNearestHistoryDistance < MinimumDistanceFromRecentEncounterLocations
			)
		{
			OutReason = FString::Printf(
				TEXT("Rejected: EQS location too close to recent EQS encounter %.0f < %.0f cm"),
				OutNearestHistoryDistance,
				MinimumDistanceFromRecentEncounterLocations
			);

			return false;
		}
	}
	else
	{
		OutNearestHistoryDistance = 0.0f;
	}

	OutReason = bShouldApplyRecentLocationSpacing
		? TEXT("Pacing passed")
		: TEXT("Pacing passed; recent-location spacing not applicable to Authored Point");

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

	const float Now = CurrentWorldState.GameTimeSeconds;
	const float Elapsed = Now - LastAnyEncounterStartTimeSeconds;

	return FMath::Max(0.0f, MinimumSecondsBetweenEncounterStarts - Elapsed);
}

float AAmbientDirector::GetNearestRecentEncounterDistance(const FVector& CandidateLocation) const
{
	if (PrototypeEncounterHistory.Num() == 0)	//debug 반드시 필요한가? 없어도 될 듯?
	{
		return TNumericLimits<float>::Max();
	}

	float NearestDistance = TNumericLimits<float>::Max();

	for (const FAmbientEncounterHistoryEntry& Entry : PrototypeEncounterHistory)
	{
		if (!Entry.LocationSource.Equals(TEXT("EQS"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const float Distance = FVector::Dist2D(CandidateLocation, Entry.EncounterLocation);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
		}
	}

	return NearestDistance;
}
