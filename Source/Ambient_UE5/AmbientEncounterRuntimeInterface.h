#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "AmbientEncounterRuntimeInterface.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct FAmbientEncounterRuntimeContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	TObjectPtr<AActor> DirectorActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FName EncounterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FName RegionName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FName SourcePointName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	float StartedAtTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FGameplayTagContainer EncounterTags;
};

UINTERFACE(BlueprintType)
class AMBIENT_UE5_API UAmbientEncounterRuntimeInterface : public UInterface
{
	GENERATED_BODY()
};

class AMBIENT_UE5_API IAmbientEncounterRuntimeInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void InitializeAmbientEncounter(const FAmbientEncounterRuntimeContext& Context);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void OnAmbientEncounterWaiting();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void OnAmbientEncounterActivated();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void OnAmbientEncounterCleanup(const FString& Reason);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void OnAmbientEncounterFinished(const FString& Reason);
};