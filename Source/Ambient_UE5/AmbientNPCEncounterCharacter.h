// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AmbientEncounterRuntimeInterface.h"
#include "GameFramework/Character.h"
#include "AmbientNPCEncounterCharacter.generated.h"

class UAudioComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USoundBase;

UCLASS()
class AMBIENT_UE5_API AAmbientNPCEncounterCharacter : public ACharacter, public IAmbientEncounterRuntimeInterface
{
	GENERATED_BODY()

public:
	AAmbientNPCEncounterCharacter();

	virtual void InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context) override;

	virtual void OnAmbientEncounterWaiting_Implementation() override;

	virtual void OnAmbientEncounterActivated_Implementation() override;

	virtual void OnAmbientEncounterCleanup_Implementation(const FString& Reason) override;

	virtual void OnAmbientEncounterFinished_Implementation(const FString& Reason) override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter")
	TObjectPtr<UStaticMeshComponent> DebugBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter")
	TObjectPtr<UTextRenderComponent> FloatingText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	FText WaitingText = NSLOCTEXT("AmbientNPCEncounter", "DefaultWaitingText", "...");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	FText BarkText = NSLOCTEXT("AmbientNPCEncounter", "DefaultBarkText", "Hey there, traveler.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	FText CleanupText = NSLOCTEXT("AmbientNPCEncounter", "DefaultCleanupText", "NPC encounter cleanup.");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	TObjectPtr<USoundBase> BarkSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	bool bPrintBarkToScreen = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Ambient NPC Encounter|Runtime")
	FAmbientEncounterRuntimeContext RuntimeContext;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Ambient NPC Encounter|Runtime")
	bool bHasPlayedBark = false;

	void SetFloatingText(const FText& NewText);

	void FacePlayer();

	void PlayBark();

private:
	void StopBark();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveBarkAudioComponent = nullptr;
};
