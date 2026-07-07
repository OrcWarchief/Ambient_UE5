// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientNPCEncounterCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h" 
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AAmbientNPCEncounterCharacter::AAmbientNPCEncounterCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}

	DebugBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugBodyMesh"));
	DebugBodyMesh->SetupAttachment(GetRootComponent());
	DebugBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugBodyMesh->SetGenerateOverlapEvents(false);
	DebugBodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	DebugBodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultBodyMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
	);

	if (DefaultBodyMesh.Succeeded())
	{
		DebugBodyMesh->SetStaticMesh(DefaultBodyMesh.Object);
	}

	FloatingText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FloatingText"));
	FloatingText->SetupAttachment(GetRootComponent());
	FloatingText->SetRelativeLocation(FVector(0.0f, 0.0f, 230.0f));
	FloatingText->SetWorldSize(36.0f);
	FloatingText->SetTextRenderColor(FColor::White);
	FloatingText->SetText(FText::FromString(TEXT("NPC")));
}

void AAmbientNPCEncounterCharacter::InitializeAmbientEncounter_Implementation(
	const FAmbientEncounterRuntimeContext& Context
)
{
	RuntimeContext = Context;
	bHasPlayedBark = false;

	SetFloatingText(FText::FromString(FString::Printf(
		TEXT("NPC Waiting\n%s\n%s"),
		*RuntimeContext.EncounterId.ToString(),
		*RuntimeContext.SourcePointName.ToString()
	)));
}

void AAmbientNPCEncounterCharacter::OnAmbientEncounterWaiting_Implementation()
{
	SetFloatingText(WaitingText);
}

void AAmbientNPCEncounterCharacter::OnAmbientEncounterActivated_Implementation()
{
	FacePlayer();
	PlayBark();
}

void AAmbientNPCEncounterCharacter::OnAmbientEncounterCleanup_Implementation(
	const FString& Reason
)
{
	SetFloatingText(FText::FromString(FString::Printf(
		TEXT("%s\nReason: %s"),
		*CleanupText.ToString(),
		*Reason
	)));
}

void AAmbientNPCEncounterCharacter::OnAmbientEncounterFinished_Implementation(
	const FString& Reason
)
{
	SetFloatingText(FText::FromString(FString::Printf(
		TEXT("NPC Finished\n%s"),
		*Reason
	)));
}

void AAmbientNPCEncounterCharacter::SetFloatingText(const FText& NewText)
{
	if (FloatingText)
	{
		FloatingText->SetText(NewText);
	}
}

void AAmbientNPCEncounterCharacter::FacePlayer()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!IsValid(PlayerPawn))
	{
		return;
	}

	FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.0f;

	if (ToPlayer.IsNearlyZero())
	{
		return;
	}

	SetActorRotation(ToPlayer.Rotation());
}

void AAmbientNPCEncounterCharacter::PlayBark()
{
	if (bHasPlayedBark)
	{
		return;
	}

	bHasPlayedBark = true;

	SetFloatingText(BarkText);

	if (BarkSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			BarkSound,
			GetActorLocation()
		);
	}

	if (bPrintBarkToScreen && GEngine)
	{
		const FString Message = FString::Printf(
			TEXT("[AD] NPC Bark | %s | %s"),
			*RuntimeContext.EncounterId.ToString(),
			*BarkText.ToString()
		);

		GEngine->AddOnScreenDebugMessage(
			2001,
			2.5f,
			FColor::White,
			Message
		);

		UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
	}
}