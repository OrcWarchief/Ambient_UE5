#include "AmbientDirector.h"

#include "Ambient_UE5.h"
#include "AmbientDirectorSaveGame.h"
#include "AmbientEncounterDefinitionData.h"
#include "AmbientEncounterRuntimeInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

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

	OutSnapshot.SaveVersion = 1;
	OutSnapshot.SavedAtGameTimeSeconds = Now;

	OutSnapshot.RuntimeState = PrototypeEncounterState;
	OutSnapshot.bHadRuntimeEncounterActor = IsValid(ActivePrototypeEncounter);
	OutSnapshot.bHadRuntimeEncounterDefinition = bHasRuntimeEncounterDefinition;

	const FAmbientEncounterDefinition& Definition = GetPrototypeEncounterDefinition();

	OutSnapshot.RuntimeEncounterId = Definition.EncounterId;
	OutSnapshot.RuntimeRegionName = RuntimeEncounterRegionName;
	OutSnapshot.RuntimePointName = RuntimeEncounterPointName;

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

	OutSnapshot.CleanupRemainingSeconds = 0.0f;
	OutSnapshot.CooldownRemainingSeconds = 0.0f;

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

	OutSnapshot.GlobalPacingRemainingSeconds = GetGlobalPacingRemaining();
	OutSnapshot.PendingFinishReason = PendingPrototypeFinishReason;
	OutSnapshot.PrototypeEncounterStartCount = PrototypeEncounterStartCount;
	OutSnapshot.PrototypeEncounterFinishCount = PrototypeEncounterFinishCount;
	OutSnapshot.PrototypeEncounterHistory = PrototypeEncounterHistory;
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
	PrototypeEncounterHistory = Snapshot.PrototypeEncounterHistory;
	PrototypeEncounterStartCount = Snapshot.PrototypeEncounterStartCount;
	PrototypeEncounterFinishCount = Snapshot.PrototypeEncounterFinishCount;

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

	PrototypeCleanupEndTimeSeconds = 0.0f;
	PrototypeCooldownEndTimeSeconds = 0.0f;
	PendingPrototypeFinishReason = Snapshot.PendingFinishReason;

	RuntimeEncounterStartedAtTimeSeconds = 0.0f;
	RuntimeEncounterRegionName = Snapshot.RuntimeRegionName;
	RuntimeEncounterPointName = Snapshot.RuntimePointName;
	RuntimeEncounterLocation = Snapshot.RuntimeEncounterLocation;
	RuntimeEncounterLocationSource = Snapshot.RuntimeEncounterLocationSource;

	RuntimeEncounterDefinition = FAmbientEncounterDefinition();
	bHasRuntimeEncounterDefinition = false;

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

	RuntimeEncounterDefinition = RestoredDefinition;
	bHasRuntimeEncounterDefinition = true;

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
			PrototypeEncounterState = EAmbientEncounterRuntimeState::Cooldown;
			PrototypeCooldownEndTimeSeconds = Now + Snapshot.CooldownRemainingSeconds;

			OutReason = FString::Printf(
				TEXT("Loaded cooldown state for %s with %.1fs remaining"),
				*Snapshot.RuntimeEncounterId.ToString(),
				Snapshot.CooldownRemainingSeconds
			);
		}
		else
		{
			PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;
			PrototypeCooldownEndTimeSeconds = 0.0f;
			bHasRuntimeEncounterDefinition = false;

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
	RuntimeEncounterStartedAtTimeSeconds = Now - FMath::Max(0.0f, Snapshot.RuntimeEncounterElapsedSeconds);
	RuntimeEncounterRegionName = Snapshot.RuntimeRegionName;
	RuntimeEncounterPointName = Snapshot.RuntimePointName;
	RuntimeEncounterLocation = Snapshot.RuntimeEncounterLocation;
	RuntimeEncounterLocationSource = Snapshot.RuntimeEncounterLocationSource;

	FAmbientEncounterRuntimeContext RuntimeContext;
	RuntimeContext.DirectorActor = this;
	RuntimeContext.EncounterId = RestoredDefinition.EncounterId;
	RuntimeContext.RegionName = RuntimeEncounterRegionName;
	RuntimeContext.SourcePointName = RuntimeEncounterPointName;
	RuntimeContext.SpawnLocation = Snapshot.RuntimeEncounterLocation;
	RuntimeContext.StartedAtTimeSeconds = RuntimeEncounterStartedAtTimeSeconds;
	RuntimeContext.EncounterTags = RestoredDefinition.EncounterTags;

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