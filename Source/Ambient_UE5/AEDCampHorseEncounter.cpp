// Fill out your copyright notice in the Description page of Project Settings.


#include "AEDCampHorseEncounter.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogAEDCampHorseEncounter, Log, All);

AAEDCampHorseEncounter::AAEDCampHorseEncounter()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AAEDCampHorseEncounter::InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context)
{
	RuntimeContext = Context;
	
	if (!TryResolveTargetHorse())
	{
		PrintDebugMessage(
			FString::Printf(
				TEXT("Initialize failed | Horse Actor Tag=%s"),
				*HorseActorTag.ToString()
			), true);

		return;
	}

	PrintDebugMessage(
		FString::Printf(
			TEXT("Initialized | Horse reference cached | Horse=%s"),
			*GetNameSafe(TargetHorse)
		), false);
}

void AAEDCampHorseEncounter::OnAmbientEncounterWaiting_Implementation()
{
	const bool bHasValidHorse = TryResolveTargetHorse();

	PrintDebugMessage(
		FString::Printf(
			TEXT("Waiting | Horse=%s | Horse state unchanged"),
			*GetNameSafe(TargetHorse)
		),
		!bHasValidHorse);
}

void AAEDCampHorseEncounter::OnAmbientEncounterActivated_Implementation()
{
	if (!TryResolveTargetHorse())
	{
		PrintDebugMessage(
			FString::Printf(
				TEXT("Activation failed | Horse Actor Tag=%s"),
				*HorseActorTag.ToString()
			), true);

		return;
	}

	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Activated | Horse reference validated | "
				"Horse visibility, transform and collision unchanged | Horse=%s"
			),
			*GetNameSafe(TargetHorse)
		), false);
}

void AAEDCampHorseEncounter::OnAmbientEncounterCleanup_Implementation(const FString& Reason)
{
	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Cleanup | Reason=%s | "
				"MountPermissionGranted=%s | Horse state unchanged"
			),
			*Reason,
			bMountPermissionGranted ? TEXT("Yes") : TEXT("No")
		),
		false
	);
}

void AAEDCampHorseEncounter::OnAmbientEncounterFinished_Implementation(const FString& Reason)
{
	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Finished | Reason=%s | "
				"MountPermissionGranted=%s | Horse state unchanged"
			),
			*Reason,
			bMountPermissionGranted ? TEXT("Yes") : TEXT("No")
		),
		false
	);
}

bool AAEDCampHorseEncounter::GrantMountPermission()
{
	if (bMountPermissionGranted)
	{
		PrintDebugMessage(
			FString::Printf(
				TEXT("Mount permission already granted | Horse=%s"),
				*GetNameSafe(TargetHorse)
			), false);

		return true;
	}

	if (!TryResolveTargetHorse())
	{
		PrintDebugMessage(
			FString::Printf(
				TEXT(
					"GrantMountPermission failed | "
					"No valid horse with Actor Tag=%s"
				),
				*HorseActorTag.ToString()
			), true);

		return false;
	}

	UActorComponent* HorseRuntimeComponent = FindHorseRuntimeComponent();

	if (!IsValid(HorseRuntimeComponent))
	{
		PrintDebugMessage(
			FString::Printf(
				TEXT(
					"GrantMountPermission failed | "
					"Horse Runtime component not found | "
					"Horse=%s | Component Tag=%s"
				),
				*GetNameSafe(TargetHorse),
				*HorseRuntimeComponentTag.ToString()
			), true);

		return false;
	}

	bMountPermissionGranted = true;

	HorseRuntimeComponent->Activate(true);

	if (!HorseRuntimeComponent->IsActive())
	{
		bMountPermissionGranted = false;

		PrintDebugMessage(
			FString::Printf(
				TEXT(
					"GrantMountPermission failed | "
					"Horse Runtime component did not activate | Runtime=%s"
				),
				*GetNameSafe(HorseRuntimeComponent)
			), true);

		return false;
	}

	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Mount permission granted | "
				"Horse=%s | Runtime=%s"
			),
			*GetNameSafe(TargetHorse),
			*GetNameSafe(HorseRuntimeComponent)
		), false);

	return true;
}

bool AAEDCampHorseEncounter::TryResolveTargetHorse()
{
	if (IsValid(TargetHorse))
	{
		return true;
	}

	TargetHorse = FindTargetHorse();

	return IsValid(TargetHorse);
}

APawn* AAEDCampHorseEncounter::FindTargetHorse() const
{
	if (HorseActorTag.IsNone() || GetWorld() == nullptr)
	{
		return nullptr;
	}

	TArray<AActor*> TaggedActors;

	UGameplayStatics::GetAllActorsWithTag(GetWorld(), HorseActorTag, TaggedActors);

	APawn* ClosestHorse = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();

	for (AActor* TaggedActor : TaggedActors)
	{
		APawn* CandidateHorse = Cast<APawn>(TaggedActor);

		if (!IsValid(CandidateHorse))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), CandidateHorse->GetActorLocation());

		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestHorse = CandidateHorse;
		}
	}

	return ClosestHorse;
}

UActorComponent* AAEDCampHorseEncounter::FindHorseRuntimeComponent() const
{
	if (!IsValid(TargetHorse) || HorseRuntimeComponentTag.IsNone())
	{
		return nullptr;
	}

	const TArray<UActorComponent*> RuntimeComponents =
		TargetHorse->GetComponentsByTag(
			UActorComponent::StaticClass(),
			HorseRuntimeComponentTag
		);

	for (UActorComponent* RuntimeComponent : RuntimeComponents)
	{
		if (IsValid(RuntimeComponent))
		{
			return RuntimeComponent;
		}
	}

	return nullptr;
}

void AAEDCampHorseEncounter::PrintDebugMessage(const FString& Message, bool bError) const
{
	if (!bPrintDebug)
	{
		return;
	}

	const FString FullMessage = FString::Printf(
		TEXT("[AED CAMP HORSE] %s"),
		*Message
	);

	if (bError)
	{
		UE_LOG(
			LogAEDCampHorseEncounter,
			Error,
			TEXT("%s"),
			*FullMessage
		);
	}
	else
	{
		UE_LOG(
			LogAEDCampHorseEncounter,
			Display,
			TEXT("%s"),
			*FullMessage
		);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			3501,
			3.0f,
			bError ? FColor::Red : FColor::Green,
			FullMessage
		);
	}
}



