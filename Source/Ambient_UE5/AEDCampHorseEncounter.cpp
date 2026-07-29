// Fill out your copyright notice in the Description page of Project Settings.


#include "AEDCampHorseEncounter.h"

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
	SetActorEnableCollision(false);
}

void AAEDCampHorseEncounter::InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context)
{
	RuntimeContext = Context;
	TargetHorse = FindTargetHorse();

	if (!IsValid(TargetHorse))
	{
		PrintDebugMessage(
			FString::Printf(
				TEXT(
					"Initialize failed to find a horse with Actor Tag=%s"
				),
				*HorseActorTag.ToString()
			),
			true
		);

		return;
	}

	bHorseReleased = !TargetHorse->IsHidden();

	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Initialized | Horse=%s | AlreadyReleased=%s"
			),
			*GetNameSafe(TargetHorse),
			bHorseReleased ? TEXT("Yes") : TEXT("No")
		),
		false
	);
}

void AAEDCampHorseEncounter::OnAmbientEncounterWaiting_Implementation()
{
	if (!IsValid(TargetHorse))
	{
		TargetHorse = FindTargetHorse();
	}

	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Waiting | Horse=%s | Player must approach the authored point"
			),
			*GetNameSafe(TargetHorse)
		),
		!IsValid(TargetHorse)
	);
	return;
}

void AAEDCampHorseEncounter::OnAmbientEncounterActivated_Implementation()
{
	if (!ReleaseHorse())
	{
		PrintDebugMessage(
			FString::Printf(
				TEXT(
					"Activation failed | No valid horse with Actor Tag=%s"
				),
				*HorseActorTag.ToString()
			),
			true
		);

		return;
	}

	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Horse released | Horse=%s | Player may now mount"
			),
			*GetNameSafe(TargetHorse)
		),
		false
	);
}

void AAEDCampHorseEncounter::OnAmbientEncounterCleanup_Implementation(const FString& Reason)
{
	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Cleanup | Reason=%s | Horse remains available"
			),
			*Reason
		),
		false
	);
}

void AAEDCampHorseEncounter::OnAmbientEncounterFinished_Implementation(const FString& Reason)
{
	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Finished | Reason=%s | Horse remains available"
			),
			*Reason
		),
		false
	);
}

APawn* AAEDCampHorseEncounter::FindTargetHorse() const
{
	if (HorseActorTag.IsNone())
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

bool AAEDCampHorseEncounter::ReleaseHorse()
{
	if (!IsValid(TargetHorse))
	{
		TargetHorse = FindTargetHorse();
	}

	if (!IsValid(TargetHorse))
	{
		return false;
	}

	const bool bHorseWasHidden = TargetHorse->IsHidden();

	if (bHorseWasHidden && bSnapHorseToAuthoredPointOnFirstRelease)
	{
		FTransform HandoffTransform = GetActorTransform();

		HandoffTransform.SetScale3D(TargetHorse->GetActorScale3D());
		
		TargetHorse->SetActorTransform(
			HandoffTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
	}

	TargetHorse->SetActorHiddenInGame(false);
	TargetHorse->SetActorEnableCollision(true);

	bHorseReleased = true;

	return true;
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



