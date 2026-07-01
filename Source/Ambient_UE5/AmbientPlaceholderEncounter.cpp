// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientPlaceholderEncounter.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAmbientPlaceholderEncounter::AAmbientPlaceholderEncounter()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	EncounterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EncounterMesh"));
	EncounterMesh->SetupAttachment(SceneRoot);

	EncounterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EncounterMesh->SetGenerateOverlapEvents(false);

	EncounterMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
	EncounterMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultCubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube")
	);

	if (DefaultCubeMesh.Succeeded())
	{
		EncounterMesh->SetStaticMesh(DefaultCubeMesh.Object);
	}
}

void AAmbientPlaceholderEncounter::InitializePrototypeEncounter(
	const FName InEncounterId,
	const FName InRegionName,
	const FName InSourcePointName
)
{
	EncounterId = InEncounterId;
	SpawnRegionName = InRegionName;
	SourcePointName = InSourcePointName;
}