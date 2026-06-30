// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientCandidateMarker.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAmbientCandidateMarker::AAmbientCandidateMarker()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(SceneRoot);

	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetGenerateOverlapEvents(false);

	MarkerMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	MarkerMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultSphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere")
	);

	if (DefaultSphereMesh.Succeeded())
	{
		MarkerMesh->SetStaticMesh(DefaultSphereMesh.Object);
	}
}

void AAmbientCandidateMarker::SetMarkerActive(const bool bNewActive)
{
	SetActorHiddenInGame(!bNewActive);
	SetActorEnableCollision(false);

	if (MarkerMesh)
	{
		MarkerMesh->SetVisibility(bNewActive, true);
	}
}
