// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientRegionVolume.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"


// Sets default values
AAmbientRegionVolume::AAmbientRegionVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RegionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RegionBounds"));
	RegionBounds->SetupAttachment(SceneRoot);

	RegionBounds->InitBoxExtent(FVector(1000.0f, 1000.0f, 500.0f));

	RegionBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBounds->SetGenerateOverlapEvents(false);
}

bool AAmbientRegionVolume::ContainsWorldLocation(const FVector& WorldLocation) const
{
	if (!RegionBounds)
	{
		return false;
	}

	const FTransform RegionTransform = RegionBounds->GetComponentTransform();
	const FVector    LocalLocation   = RegionTransform.InverseTransformPosition(WorldLocation);
	const FVector    LocalExtent     = RegionBounds->GetUnscaledBoxExtent();

	return FMath::Abs(LocalLocation.X) <= LocalExtent.X
		&& FMath::Abs(LocalLocation.Y) <= LocalExtent.Y
		&& FMath::Abs(LocalLocation.Z) <= LocalExtent.Z;
}

