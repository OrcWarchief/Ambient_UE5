// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmbientPlaceholderEncounter.generated.h"

UCLASS()
class AMBIENT_UE5_API AAmbientPlaceholderEncounter : public AActor
{
	GENERATED_BODY()
	
public:
	AAmbientPlaceholderEncounter();

	UFUNCTION(BlueprintCallable, Category = "Ambient Placeholder Encounter")
	void InitializePrototypeEncounter(
		FName InEncounterId,
		FName InRegionName,
		FName InSourcePointName
	);

	UFUNCTION(BlueprintPure, Category = "Ambient Placeholder Encounter")
	FName GetEncounterId() const { return EncounterId; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Placeholder Encounter")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Placeholder Encounter")
	TObjectPtr<UStaticMeshComponent> EncounterMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Placeholder Encounter")
	FName EncounterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Placeholder Encounter")
	FName SpawnRegionName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Placeholder Encounter")
	FName SourcePointName = NAME_None;
};
