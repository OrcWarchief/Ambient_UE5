// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmbientEncounterPoint.generated.h"

class USceneComponent;
class UArrowComponent;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAmbientEncounterPoint : public AActor
{
	GENERATED_BODY()
	
public:
	AAmbientEncounterPoint();

	bool IsPointEnabled() const { return bEnabled; }

	FName GetPointName() const { return PointName; }

	FName GetRegionName() const { return RegionName; }

	float GetDebugRadius() const { return DebugRadius; }

	FTransform GetEncounterSpawnTransform() const { return GetActorTransform(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	TObjectPtr<UArrowComponent> FacingArrow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	FName PointName = TEXT("EP.Showroom.01");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	FName RegionName = TEXT("Region.Showroom");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point", meta = (ClampMin = "10.0", Units = "cm"))
	float DebugRadius = 80.0f;
};
