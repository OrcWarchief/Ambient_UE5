// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AmbientEncounterRuntimeInterface.h"
#include "GameFramework/Character.h"
#include "AmbientNPCEncounterCharacter.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USoundBase;

UCLASS()
class AMBIENT_UE5_API AAmbientNPCEncounterCharacter : public ACharacter, public IAmbientEncounterRuntimeInterface
{
	GENERATED_BODY()

public:
	AAmbientNPCEncounterCharacter();

	virtual void InitializeAmbientEncounter_Implementation(
		const FAmbientEncounterRuntimeContext& Context
	) override;

	virtual void OnAmbientEncounterWaiting_Implementation() override;

	virtual void OnAmbientEncounterActivated_Implementation() override;

	virtual void OnAmbientEncounterCleanup_Implementation(
		const FString& Reason
	) override;

	virtual void OnAmbientEncounterFinished_Implementation(
		const FString& Reason
	) override;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter")
	TObjectPtr<UStaticMeshComponent> DebugBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter")
	TObjectPtr<UTextRenderComponent> FloatingText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	FText WaitingText = FText::FromString(TEXT("..."));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	FText BarkText = FText::FromString(TEXT("Hey there, traveler."));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	FText CleanupText = FText::FromString(TEXT("NPC encounter cleanup."));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	TObjectPtr<USoundBase> BarkSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient NPC Encounter|Bark")
	bool bPrintBarkToScreen = true;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient NPC Encounter|Runtime")
	FAmbientEncounterRuntimeContext RuntimeContext;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient NPC Encounter|Runtime")
	bool bHasPlayedBark = false;

	void SetFloatingText(const FText& NewText);

	void FacePlayer();

	void PlayBark();
};
