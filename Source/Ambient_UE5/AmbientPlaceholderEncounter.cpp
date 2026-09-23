// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientPlaceholderEncounter.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogAmbientPlaceholderEncounter, Log, All);

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

	StateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StateText"));
	StateText->SetupAttachment(SceneRoot);
	StateText->SetRelativeLocation(FVector(0.0f, 0.0f, 230.0f));
	StateText->SetWorldSize(36.0f);
	StateText->SetTextRenderColor(FColor::White);
	StateText->SetText(FText::FromString(TEXT("Placeholder")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultCubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube")
	);

	if (DefaultCubeMesh.Succeeded())
	{
		EncounterMesh->SetStaticMesh(DefaultCubeMesh.Object);
	}
}

void AAmbientPlaceholderEncounter::InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context)
{
	RuntimeContext = Context;

	SetStateText(FString::Printf(TEXT("Placeholder\n%s\n%s"),
		*RuntimeContext.EncounterId.ToString(), *RuntimeContext.SourcePointName.ToString()));
}

void AAmbientPlaceholderEncounter::OnAmbientEncounterWaiting_Implementation()
{
	SetStateText(FString::Printf(TEXT("Placeholder Waiting\n%s"),
		*RuntimeContext.EncounterId.ToString()));
}

void AAmbientPlaceholderEncounter::OnAmbientEncounterActivated_Implementation()
{
	SetStateText(FString::Printf(TEXT("Placeholder Active\n%s"),
		*RuntimeContext.EncounterId.ToString()));
}

void AAmbientPlaceholderEncounter::OnAmbientEncounterCleanup_Implementation(const FString& Reason)
{
	SetStateText(FString::Printf(TEXT("Placeholder Cleanup\n%s"),*Reason));
}

void AAmbientPlaceholderEncounter::OnAmbientEncounterFinished_Implementation(const FString& Reason)
{
	UE_LOG(LogAmbientPlaceholderEncounter, Log, TEXT("Finished notification | Encounter=%s | Reason=%s"), *RuntimeContext.EncounterId.ToString(), *Reason);
}

void AAmbientPlaceholderEncounter::SetStateText(const FString& Text)
{
	if (StateText)
	{
		StateText->SetText(FText::FromString(Text));
	}
}
