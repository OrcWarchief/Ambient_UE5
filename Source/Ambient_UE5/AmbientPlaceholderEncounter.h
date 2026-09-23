// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AmbientEncounterRuntimeInterface.h"
#include "GameFramework/Actor.h"
#include "AmbientPlaceholderEncounter.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class AMBIENT_UE5_API AAmbientPlaceholderEncounter : public AActor, public IAmbientEncounterRuntimeInterface
{
	GENERATED_BODY()
	
public:
	AAmbientPlaceholderEncounter();

	UFUNCTION(BlueprintPure, Category = "Ambient Placeholder Encounter")
	FName GetEncounterId() const { return RuntimeContext.EncounterId; }

	virtual void InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context) override;

	virtual void OnAmbientEncounterWaiting_Implementation() override;

	virtual void OnAmbientEncounterActivated_Implementation() override;

	virtual void OnAmbientEncounterCleanup_Implementation(const FString& Reason) override;

	virtual void OnAmbientEncounterFinished_Implementation(const FString& Reason) override;


protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Placeholder Encounter")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Placeholder Encounter")
	TObjectPtr<UStaticMeshComponent> EncounterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Placeholder Encounter")
	TObjectPtr<UTextRenderComponent> StateText;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Ambient Placeholder Encounter")
	FAmbientEncounterRuntimeContext RuntimeContext;

	void SetStateText(const FString& Text);
};
