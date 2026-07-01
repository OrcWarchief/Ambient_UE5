// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientEncounterPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

AAmbientEncounterPoint::AAmbientEncounterPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FacingArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FacingArrow"));
	FacingArrow->SetupAttachment(SceneRoot);
	FacingArrow->SetRelativeScale3D(FVector(2.0f, 2.0f, 2.0f));
}