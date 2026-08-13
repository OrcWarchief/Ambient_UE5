// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AmbientEncounterRuntimeInterface.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "AEDCampHorseEncounter.generated.h"

class APawn;
class UActorComponent;
class UAudioComponent;
class UChildActorComponent;
class UPrimitiveComponent;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USphereComponent;
class UTextRenderComponent;

enum class ECampVoicePhase : uint8
{
	None,
	ArrivalBark,
	InteractionLine
};

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

	UFUNCTION(BlueprintCallable, Category = "AED|Camp Horse")
	bool GrantMountPermission();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "AED|Camp Conversation")
	void BP_OnCampConversationCompleted();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Camp Horse")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Camp|NPC")
	TObjectPtr<UChildActorComponent> CampNpc = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Camp Conversation|Interaction")
	TObjectPtr<USphereComponent> InteractionSphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Camp Conversation|Interaction")
	TObjectPtr<UTextRenderComponent> InteractionPrompt = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Horse")
	FName HorseActorTag = TEXT("AED.CampHorse.Primary");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Horse")
	FName HorseRuntimeComponentTag = TEXT("AED.Horse.Runtime");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Conversation|Interaction",
		meta = (ClampMin = "50.0", Units = "cm"))
	float InteractionRadius = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Conversation|Interaction")
	FKey InteractKey = EKeys::E;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Conversation|Presentation")
	FText InteractionPromptText = FText::FromString(TEXT("[E] Talk to Sarah"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Conversation|Presentation")
	FText ArrivalBarkText = FText::FromString(TEXT("Hey, traveler. Come here a moment."));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Conversation|Audio")
	TObjectPtr<USoundBase> ArrivalBarkSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Conversation|Presentation")
	FText InteractionLineText = FText::FromString(
		TEXT(
			"Headed for the ridge?\n"
			"Take the horse by the fire.\n"
			"It'll get you there."
		)
	);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Conversation|Audio")
	TObjectPtr<USoundBase> InteractionLineSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Conversation|Audio")
	TObjectPtr<USoundAttenuation> VoiceAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Conversation|Presentation")
	bool bShowSpokenLineAsWorldText = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Camp Horse|Debug")
	bool bPrintDebug = true;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Horse|Runtime")
	TObjectPtr<APawn> TargetHorse = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "AED|Camp Horse|Runtime")
	bool bMountPermissionGranted = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Conversation|Runtime")
	TObjectPtr<AActor> TargetSarah = nullptr;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Conversation|Runtime")
	TObjectPtr<UAudioComponent> ActiveVoiceComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Conversation|Runtime")
	bool bEncounterActive = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Conversation|Runtime")
	bool bPlayerInsideInteractionRange = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Conversation|Runtime")
	bool bArrivalBarkStarted = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Conversation|Runtime")
	bool bArrivalBarkCompleted = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Conversation|Runtime")
	bool bConversationStarted = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Conversation|Runtime")
	bool bConversationCompleted = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Camp Horse|Runtime")
	FAmbientEncounterRuntimeContext RuntimeContext;


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
	void HandleActiveVoiceFinished();

	void HandleInteractPressed();
	void PlayArrivalBark();
	void PlayVoice(USoundBase* Sound, ECampVoicePhase VoicePhase);
	void StopActiveVoice();
	void CompleteCampConversation();
	void RefreshInteractionAvailability();
	void UpdateInteractionPrompt();
	void EnableInteractionInput();
	void DisableInteractionInput();
	bool CanPlayerInteract() const;
	bool IsCurrentPlayerActor(const AActor* Actor) const;
	bool TryResolveSarah();
	void FaceSarahTowardPlayer();
	bool TryResolveTargetHorse();
	APawn* FindTargetHorse() const;
	UActorComponent* FindHorseRuntimeComponent() const;
	void PrintDebugMessage(const FString& Message, bool bError) const;

	ECampVoicePhase ActiveVoicePhase = ECampVoicePhase::None;
	bool bInteractionInputBound = false;
};
