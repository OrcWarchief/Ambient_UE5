#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "AmbientEncounterDefinitionTypes.generated.h"

class AAmbientPlaceholderEncounter;

USTRUCT(BlueprintType)
struct FAmbientEncounterDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition")
	FName EncounterId = TEXT("Encounter.Prototype.Showroom");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition")
	FName RequiredRegionName = TEXT("Region.Showroom");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "cm"))
	float EncounterPointSearchRadius = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxPlayerSpeed = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "cm"))
	float PlayerEngageDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "cm"))
	float PlayerLeaveDistance = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "s"))
	float CleanupDelaySeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownDurationSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition")
	TSubclassOf<AAmbientPlaceholderEncounter> EncounterClass;
};