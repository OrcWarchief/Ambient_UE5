// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AmbientDirectorTypes.h"
#include "AmbientDirectorSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FAmbientDirectorSaveSnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	int32 SaveVersion = 1;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	float SavedAtGameTimeSeconds = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	EAmbientEncounterRuntimeState RuntimeState = EAmbientEncounterRuntimeState::Waiting;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	bool bHadRuntimeEncounterActor = false;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	bool bHadRuntimeEncounterDefinition = false;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	FName RuntimeEncounterId = NAME_None;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	FName RuntimeRegionName = NAME_None;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	FName RuntimePointName = NAME_None;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	FVector RuntimeEncounterLocation = FVector::ZeroVector;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	FRotator RuntimeEncounterRotation = FRotator::ZeroRotator;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	FString RuntimeEncounterLocationSource = TEXT("Unknown");

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	float RuntimeEncounterElapsedSeconds = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	float CleanupRemainingSeconds = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	float CooldownRemainingSeconds = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	float GlobalPacingRemainingSeconds = 0.0f;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	FString PendingFinishReason = TEXT("None");

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	int32 PrototypeEncounterStartCount = 0;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	int32 PrototypeEncounterFinishCount = 0;

	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	TArray<FAmbientEncounterHistoryEntry> PrototypeEncounterHistory;
};

UCLASS()
class AMBIENT_UE5_API UAmbientDirectorSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Ambient Director Save")
	FAmbientDirectorSaveSnapshot DirectorSnapshot;
};
