// Fill out your copyright notice in the Description page of Project Settings.


#include "AEDWildlifeMemberCharacter.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AAEDWildlifeMemberCharacter::AAEDWildlifeMemberCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();
	bUseControllerRotationYaw = false;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->InitCapsuleSize(38.0f, 55.0f);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 420.0f, 0.0f);
		Movement->MaxWalkSpeed = 700.0f;
		Movement->BrakingDecelerationWalking = 600.0f;

		DebugBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugBodyMesh"));
		DebugBodyMesh->SetupAttachment(GetRootComponent());
		DebugBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DebugBodyMesh->SetGenerateOverlapEvents(false);
		DebugBodyMesh->SetCanEverAffectNavigation(false);
		DebugBodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -10.0f));
		DebugBodyMesh->SetRelativeScale3D(FVector(1.05f, 0.45f, 0.55f));

		DebugHeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugHeadMesh"));
		DebugHeadMesh->SetupAttachment(GetRootComponent());
		DebugHeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DebugHeadMesh->SetGenerateOverlapEvents(false);
		DebugHeadMesh->SetCanEverAffectNavigation(false);
		DebugHeadMesh->SetRelativeLocation(FVector(65.0f, 0.0f, 20.0f));
		DebugHeadMesh->SetRelativeScale3D(FVector(0.32f, 0.30f, 0.38f));

		static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
			TEXT("/Engine/BasicShapes/Sphere.Sphere"));

		if (SphereMesh.Succeeded())
		{
			DebugBodyMesh->SetStaticMesh(SphereMesh.Object);
			DebugHeadMesh->SetStaticMesh(SphereMesh.Object);
		}
	}
}

void AAEDWildlifeMemberCharacter::PrepareForAmbientFlee(float NewFleeSpeed)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = FMath::Max(100.0f, NewFleeSpeed);
		Movement->bOrientRotationToMovement = true;
	}

	if (!IsValid(GetController()))
	{
		SpawnDefaultController();
	}
}

void AAEDWildlifeMemberCharacter::SetAmbientAlerted(const bool bNewAlerted)
{
	bAmbientAlerted = bNewAlerted;
}

bool AAEDWildlifeMemberCharacter::IsAmbientAlerted() const
{
	return bAmbientAlerted;
}