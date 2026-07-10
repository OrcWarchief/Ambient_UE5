#include "AmbientDirector.h"

#include "Ambient_UE5.h"
#include "AmbientCandidateMarker.h"
#include "AmbientEncounterPoint.h"
#include "AmbientRegionVolume.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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

	UE_LOG(LogAmbient_UE5, Log, TEXT("%s"), *Message);
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

	UE_LOG(LogAmbient_UE5, Log, TEXT("%s"), *Message);
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

	UE_LOG(LogAmbient_UE5, Log, TEXT("%s"), *Message);
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

	const FString LocationSourceString =
		CurrentWorldState.bHasSelectedEncounterLocation
		? CurrentWorldState.SelectedEncounterLocationSource
		: TEXT("None");

	FString Message = FString::Printf(
		TEXT("[AD] Selection | Candidates=%d | Winner=%s | Score=%.1f | Location=%s | Reason=%s"),
		LastSelectionDebugEntries.Num(),
		*WinnerString,
		SelectedEncounterScore,
		*LocationSourceString,
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

	UE_LOG(LogAmbient_UE5, Log, TEXT("%s"), *Message);

	for (const FAmbientEncounterSelectionDebugEntry& Entry : LastSelectionDebugEntries)
	{
		const FString EntryMessage = FString::Printf(
			TEXT("[AD] SelectionCandidate | Accepted=%s | Encounter=%s | Point=%s | LocationSource=%s | Score=%.1f | Dist=%.0f | Location=(X=%.0f Y=%.0f Z=%.0f) | Reason=%s | LocationReason=%s"),
			Entry.bAccepted ? TEXT("true") : TEXT("false"),
			*Entry.EncounterId.ToString(),
			*Entry.PointName.ToString(),
			*Entry.LocationSource,
			Entry.Score,
			Entry.DistanceToPoint,
			Entry.SelectedLocation.X,
			Entry.SelectedLocation.Y,
			Entry.SelectedLocation.Z,
			*Entry.Reason,
			*Entry.LocationReason
		);

		UE_LOG(LogAmbient_UE5, Log, TEXT("%s"), *EntryMessage);
	}
}

void AAmbientDirector::PrintDirectorDashboardDebug() const
{
	if (!GEngine)
	{
		return;
	}

	const FString StateString = GetPrototypeRuntimeStateString();

	const FAmbientEncounterDefinition& Definition =
		GetPrototypeEncounterDefinition();

	const FString DefinitionIdString =
		Definition.EncounterId != NAME_None
		? Definition.EncounterId.ToString()
		: TEXT("None");

	const FString RegionString =
		CurrentWorldState.bHasCurrentRegion
		? CurrentWorldState.CurrentRegionName.ToString()
		: TEXT("None");

	const FString SelectionString =
		bHasSelectedEncounterDefinition
		? SelectedEncounterDefinition.EncounterId.ToString()
		: TEXT("None");

	const FString LocationSourceString =
		CurrentWorldState.bHasSelectedEncounterLocation
		? CurrentWorldState.SelectedEncounterLocationSource
		: RuntimeEncounterLocationSource;

	const FString ActorString =
		IsValid(ActivePrototypeEncounter)
		? TEXT("Yes")
		: TEXT("No");

	const FString PacingString =
		CurrentWorldState.bPacingAllowsNewEncounter
		? TEXT("OK")
		: FString::Printf(
			TEXT("Blocked: %s"),
			*CurrentWorldState.PacingBlockReason
		);

	FString LastHistoryString = TEXT("None");

	if (PrototypeEncounterHistory.Num() > 0)
	{
		const FAmbientEncounterHistoryEntry& LastEntry =
			PrototypeEncounterHistory[0];

		LastHistoryString = FString::Printf(
			TEXT("%s @ %s"),
			*LastEntry.EncounterId.ToString(),
			*LastEntry.LocationSource
		);
	}

	const FString Dashboard = FString::Printf(
		TEXT("[AD DASHBOARD]\n")
		TEXT("State=%s | Def=%s | Region=%s | Location=%s | Actor=%s\n")
		TEXT("Selection=%s | Score=%.1f | Candidates=%d\n")
		TEXT("Pacing=%s | Budget=%d/%d | StartGap=%.1fs | RecentDist=%.0f\n")
		TEXT("Runtime=Dist=%.0f | Cleanup=%.1fs | Cooldown=%.1fs\n")
		TEXT("History=Starts=%d | Finishes=%d | Entries=%d | Last=%s\n")
		TEXT("Save=Slot=%s | AutoLoad=%s | AutoSave=%s"),
		*StateString,
		*DefinitionIdString,
		*RegionString,
		*LocationSourceString,
		*ActorString,

		*SelectionString,
		SelectedEncounterScore,
		LastSelectionDebugEntries.Num(),

		*PacingString,
		CurrentWorldState.CurrentEncounterBudgetUse,
		CurrentWorldState.MaxEncounterBudget,
		CurrentWorldState.GlobalPacingRemaining,
		CurrentWorldState.NearestRecentEncounterDistance,

		CurrentWorldState.DistanceToPrototypeEncounter,
		CurrentWorldState.PrototypeCleanupRemaining,
		CurrentWorldState.PrototypeCooldownRemaining,

		PrototypeEncounterStartCount,
		PrototypeEncounterFinishCount,
		PrototypeEncounterHistory.Num(),
		*LastHistoryString,

		*DirectorSaveSlotName,
		bAutoLoadDirectorSaveOnBeginPlay ? TEXT("On") : TEXT("Off"),
		bAutoSaveDirectorStateOnRuntimeChange ? TEXT("On") : TEXT("Off")
	);

	FColor DashboardColor = FColor(180, 220, 255);

	if (!CurrentWorldState.bPacingAllowsNewEncounter)
	{
		DashboardColor = FColor::Orange;
	}
	else if (PrototypeEncounterState == EAmbientEncounterRuntimeState::Active)
	{
		DashboardColor = FColor::Green;
	}
	else if (PrototypeEncounterState == EAmbientEncounterRuntimeState::Cooldown)
	{
		DashboardColor = FColor::Red;
	}

	GEngine->AddOnScreenDebugMessage(
		1000,
		UpdateInterval * 0.85f,
		DashboardColor,
		Dashboard
	);

	UE_LOG(LogAmbient_UE5, Log, TEXT("%s"), *Dashboard);
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
