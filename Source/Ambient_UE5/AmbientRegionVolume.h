// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmbientRegionVolume.generated.h"

class USceneComponent;
class UBoxComponent;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAmbientRegionVolume : public AActor
{
	GENERATED_BODY()
	
public:
	AAmbientRegionVolume();

	bool ContainsWorldLocation(const FVector& WorldLocation) const;

	FName GetRegionName() const { return RegionName; }

	int32 GetPriority() const { return Priority; }

	FLinearColor GetRegionDebugColor() const { return RegionDebugColor; }

	const UBoxComponent* GetRegionBounds() const { return RegionBounds; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	TObjectPtr<UBoxComponent> RegionBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	FName RegionName = TEXT("Region.Showroom");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	FLinearColor RegionDebugColor = FLinearColor(0.1f, 0.5f, 1.0f, 1.0f);
};
