// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AmbientNPCEncounterCharacter.h"
#include "InputCoreTypes.h"
#include "AEDStrandedTravelerEncounter.generated.h"

class AAmbientDirector;
class APlayerController;
class UPrimitiveComponent;
class USoundBase;
class USphereComponent;
class UTextRenderComponent;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAEDStrandedTravelerEncounter : public AAmbientNPCEncounterCharacter
{
	GENERATED_BODY()
	
public:
	AAEDStrandedTravelerEncounter();

	virtual void InitializeAmbientEncounter_Implementation(
		const FAmbientEncounterRuntimeContext& Context
	) override;

	virtual void OnAmbientEncounterWaiting_Implementation() override;

	virtual void OnAmbientEncounterActivated_Implementation() override;

	virtual void OnAmbientEncounterCleanup_Implementation(const FString& Reason) override;

	virtual void OnAmbientEncounterFinished_Implementation(const FString& Reason) override;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Stranded Traveler|Interaction")
	TObjectPtr<USphereComponent> InteractionSphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Stranded Traveler|Interaction")
	TObjectPtr<USphereComponent> ResolutionSphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Stranded Traveler|Interaction")
	TObjectPtr<UTextRenderComponent> InteractionPrompt = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Interaction", meta = (ClampMin = "50.0", Units = "cm"))
	float InteractionRadius = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Interaction", meta = (ClampMin = "100.0", Units = "cm"))
	float ResolutionRadius = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Interaction")
	FKey InteractKey = EKeys::E;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Presentation")
	FText InteractionPromptText = FText::FromString(TEXT("[E] Help the traveler"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Presentation")
	FText HelpAcceptedText = FText::FromString(TEXT("Thank you. I can manage from here."));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Presentation")
	FText HelpedCleanupText = FText::FromString(TEXT("Traveler helped"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Presentation")
	FText IgnoredCleanupText = FText::FromString(TEXT("Traveler ignored"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Audio")
	TObjectPtr<USoundBase> HelpAcceptedSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Audio")
	TObjectPtr<USoundBase> IgnoredSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Stranded Traveler|Debug")
	bool bPrintInteractionDebug = true;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Stranded Traveler|Runtime")
	TObjectPtr<AAmbientDirector> CachedDirector = nullptr;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Stranded Traveler|Runtime")
	bool bEncounterActive = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Stranded Traveler|Runtime")
	bool bPlayerInsideInteractionRange = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Stranded Traveler|Runtime")
	bool bHelpAccepted = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Stranded Traveler|Runtime")
	bool bOutcomeSubmitted = false;

private:
	UFUNCTION()
	void HandleInteractionRangeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleInteractionRangeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex
	);

	UFUNCTION()
	void HandleResolutionRangeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex
	);

	void HandleInteractPressed();

	bool IsCurrentPlayerActor(const AActor* Actor) const;

	void EnableInteractionInput();

	void DisableInteractionInput();

	void UpdateInteractionPrompt();

	void SubmitOutcomeAndRequestCleanup(const FString& OutcomeReason);

	void PrintInteractionDebug(const FString& Message, bool bError) const;

	bool bInteractionInputBound = false;
};
