// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientNPCEncounterCharacter.h"

#include "Components/AudioComponent.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogAmbientNPCEncounter, Log, All);

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
	DebugBodyMesh->SetRelativeLocation(FVector::ZeroVector);
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
	FloatingText->SetRelativeLocation(FVector(0.0f, 0.0f, 190.0f));
	FloatingText->SetWorldSize(36.0f);
	FloatingText->SetTextRenderColor(FColor::White);
	FloatingText->SetText(FText::FromString(TEXT("NPC")));
}

void AAmbientNPCEncounterCharacter::InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context)
{
	StopBark();

	RuntimeContext = Context;
	bHasPlayedBark = false;
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

void AAmbientNPCEncounterCharacter::OnAmbientEncounterCleanup_Implementation(const FString& Reason)
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("CleanupText"), CleanupText);
	Arguments.Add(TEXT("Reason"), FText::AsCultureInvariant(Reason));

	const FText CleanupMessage = FText::Format(
		NSLOCTEXT(
			"AmbientNPCEncounter",
			"CleanupMessageFormat",
			"{CleanupText}\nReason: {Reason}"),
		Arguments);

	SetFloatingText(CleanupMessage);
}

void AAmbientNPCEncounterCharacter::OnAmbientEncounterFinished_Implementation(const FString& Reason)
{
	StopBark();

	UE_LOG(LogAmbientNPCEncounter, Log, TEXT("Finished notification | Encounter=%s | Reason=%s"), *RuntimeContext.EncounterId.ToString(), *Reason);
}

void AAmbientNPCEncounterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBark();

	Super::EndPlay(EndPlayReason);
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
		ActiveBarkAudioComponent = UGameplayStatics::SpawnSoundAtLocation(this, BarkSound, GetActorLocation());
	}

	const FString Message = FString::Printf(TEXT("[AD] NPC Bark | %s | %s"),
		*RuntimeContext.EncounterId.ToString(),*BarkText.ToString());

	UE_LOG(LogAmbientNPCEncounter, Log, TEXT("%s"), *Message);

	if (bPrintBarkToScreen && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			2001,
			2.5f,
			FColor::White,
			Message
		);
	}
}

void AAmbientNPCEncounterCharacter::StopBark()
{
	if (IsValid(ActiveBarkAudioComponent))
	{
		ActiveBarkAudioComponent->Stop();
	}

	ActiveBarkAudioComponent = nullptr;
}
