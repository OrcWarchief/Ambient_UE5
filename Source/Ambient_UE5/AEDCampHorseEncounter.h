// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AmbientEncounterRuntimeInterface.h"
#include "GameFramework/Actor.h"
#include "AEDCampHorseEncounter.generated.h"

class APawn;
class USceneComponent;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAEDCampHorseEncounter : public AActor, public IAmbientEncounterRuntimeInterface
{
	GENERATED_BODY()
	
public:	
	AAEDCampHorseEncounter();

	virtual void InitializeAmbientEncounter_Implementation(
		const FAmbientEncounterRuntimeContext& Context
	) override;

	virtual void OnAmbientEncounterWaiting_Implementation() override;

	virtual void OnAmbientEncounterActivated_Implementation() override;

	virtual void OnAmbientEncounterCleanup_Implementation(const FString& Reason) override;

	virtual void OnAmbientEncounterFinished_Implementation(const FString& Reason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Camp Horse")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Horse")
	FName HorseActorTag = TEXT("AED.CampHorse.Primary");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Horse")
	bool bSnapHorseToAuthoredPointOnFirstRelease = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Horse|Debug")
	bool bPrintDebug = true;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Horse|Runtime")
	TObjectPtr<APawn> TargetHorse = nullptr;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Horse|Runtime")
	bool bHorseReleased = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Horse|Runtime")
	FAmbientEncounterRuntimeContext RuntimeContext;

private:
	APawn* FindTargetHorse() const;

	bool ReleaseHorse();
	void PrintDebugMessage(const FString& Message, bool bError) const;
};
