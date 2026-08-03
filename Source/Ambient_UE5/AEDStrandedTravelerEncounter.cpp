// Fill out your copyright notice in the Description page of Project Settings.


#include "AEDStrandedTravelerEncounter.h"
#include "AmbientDirector.h"

#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogAEDStrandedTraveler, Log,All);

AAEDStrandedTravelerEncounter::AAEDStrandedTravelerEncounter()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(GetRootComponent());
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AAEDStrandedTravelerEncounter::HandleInteractionRangeBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AAEDStrandedTravelerEncounter::HandleInteractionRangeEndOverlap);

	ResolutionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ResolutionSphere"));
	ResolutionSphere->SetupAttachment(GetRootComponent());
	ResolutionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ResolutionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ResolutionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ResolutionSphere->SetGenerateOverlapEvents(true);
	ResolutionSphere->OnComponentEndOverlap.AddDynamic(this, &AAEDStrandedTravelerEncounter::HandleResolutionRangeEndOverlap);

	InteractionPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("InteractionPrompt"));
	InteractionPrompt->SetupAttachment(GetRootComponent());
	InteractionPrompt->SetRelativeLocation(FVector(0.f, 0.f, 285.f));
	InteractionPrompt->SetWorldSize(28.0f);
	InteractionPrompt->SetTextRenderColor(FColor::Yellow);
	InteractionPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPrompt->SetText(InteractionPromptText);
	InteractionPrompt->SetVisibility(false);
}

void AAEDStrandedTravelerEncounter::InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context)
{
	Super::InitializeAmbientEncounter_Implementation(Context);

	CachedDirector = Cast<AAmbientDirector>(Context.DirectorActor);

	bEncounterActive = false;
	bPlayerInsideInteractionRange = false;
	bHelpAccepted = false;
	bOutcomeSubmitted = false;

	UpdateInteractionPrompt();

	PrintInteractionDebug(
		FString::Printf(
			TEXT(
				"Initialized | Director=%s | Encounter=%s"
			),
			*GetNameSafe(CachedDirector),
			*Context.EncounterId.ToString()
		),
		!IsValid(CachedDirector)
	);
}

void AAEDStrandedTravelerEncounter::OnAmbientEncounterWaiting_Implementation()
{
	Super::OnAmbientEncounterWaiting_Implementation();

	bEncounterActive = false;
	bPlayerInsideInteractionRange = false;
	bHelpAccepted = false;
	bOutcomeSubmitted = false;

	UpdateInteractionPrompt();

	PrintInteractionDebug(
		TEXT("Waiting for player approach"),
		false
	);
}

void AAEDStrandedTravelerEncounter::OnAmbientEncounterActivated_Implementation()
{
	Super::OnAmbientEncounterActivated_Implementation();

	bEncounterActive = true;
	bOutcomeSubmitted = false;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	bPlayerInsideInteractionRange =
		IsValid(PlayerPawn) &&
		IsValid(InteractionSphere) &&
		InteractionSphere->IsOverlappingActor(
			PlayerPawn
		);

	EnableInteractionInput();
	UpdateInteractionPrompt();

	PrintInteractionDebug(
		TEXT(
			"Activated | Player may help inside interaction range"
		),
		false
	);
}

void AAEDStrandedTravelerEncounter::OnAmbientEncounterCleanup_Implementation(const FString& Reason)
{
	bEncounterActive = false;

	DisableInteractionInput();
	UpdateInteractionPrompt();

	Super::OnAmbientEncounterCleanup_Implementation(Reason);

	if (Reason.Equals(TEXT("Helped"), ESearchCase::IgnoreCase))
	{
		SetFloatingText(HelpedCleanupText);
	}
	else if (Reason.Equals(TEXT("Ignored"), ESearchCase::IgnoreCase))
	{
		SetFloatingText(IgnoredCleanupText);

		if (IgnoredSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				IgnoredSound,
				GetActorLocation()
			);
		}
	}

	PrintInteractionDebug(
		FString::Printf(
			TEXT("Cleanup | Outcome=%s"),
			*Reason
		),
		false
	);
}

void AAEDStrandedTravelerEncounter::OnAmbientEncounterFinished_Implementation(const FString& Reason)
{
	DisableInteractionInput();

	Super::OnAmbientEncounterFinished_Implementation(Reason);

	PrintInteractionDebug(
		FString::Printf(
			TEXT("Finished | Outcome=%s"),
			*Reason
		),
		false
	);
}

void AAEDStrandedTravelerEncounter::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(FMath::Max(50.0f, InteractionRadius));
	}

	if (ResolutionSphere)
	{
		ResolutionSphere->SetSphereRadius(FMath::Max(InteractionRadius + 50.0f, ResolutionRadius));
	}

	if (InteractionPrompt)
	{
		InteractionPrompt->SetText(InteractionPromptText);
		InteractionPrompt->SetVisibility(false);
	}
}

void AAEDStrandedTravelerEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DisableInteractionInput();

	Super::EndPlay(EndPlayReason);
}

void AAEDStrandedTravelerEncounter::HandleInteractionRangeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComponent, 
	int32 OtherBodyIndex, 
	bool bFromSweep, 
	const FHitResult& SweepResult
)
{
	if (!IsCurrentPlayerActor(OtherActor))
	{
		return;
	}

	bPlayerInsideInteractionRange = true;

	UpdateInteractionPrompt();
}

void AAEDStrandedTravelerEncounter::HandleInteractionRangeEndOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComponent, 
	int32 OtherBodyIndex
)
{
	if (!IsCurrentPlayerActor(OtherActor))
	{
		return;
	}

	bPlayerInsideInteractionRange = false;

	UpdateInteractionPrompt();
}

void AAEDStrandedTravelerEncounter::HandleResolutionRangeEndOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComponent, 
	int32 OtherBodyIndex
)
{
	if (!IsCurrentPlayerActor(OtherActor))
	{
		return;
	}

	if (!bEncounterActive || bOutcomeSubmitted)
	{
		return;
	}

	const FString OutcomeReason =
		bHelpAccepted
		? TEXT("Helped")
		: TEXT("Ignored");

	SubmitOutcomeAndRequestCleanup(OutcomeReason);
}

void AAEDStrandedTravelerEncounter::HandleInteractPressed()
{
	if (
		!bEncounterActive ||
		bOutcomeSubmitted ||
		bHelpAccepted ||
		!bPlayerInsideInteractionRange
		)
	{
		return;
	}

	bHelpAccepted = true;

	SetFloatingText(HelpAcceptedText);
	UpdateInteractionPrompt();

	if (HelpAcceptedSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			HelpAcceptedSound,
			GetActorLocation()
		);
	}

	PrintInteractionDebug(
		TEXT(
			"Player accepted help request | "
			"Outcome will resolve after leaving"
		),
		false
	);
}

bool AAEDStrandedTravelerEncounter::IsCurrentPlayerActor(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	return IsValid(PlayerPawn) && Actor == PlayerPawn;
}

void AAEDStrandedTravelerEncounter::EnableInteractionInput()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (!IsValid(PlayerController))
	{
		PrintInteractionDebug(
			TEXT("Cannot enable interaction input: PlayerController 0 is invalid"),
			true
		);

		return;
	}

	AActor::EnableInput(PlayerController);

	if (InputComponent && !bInteractionInputBound)
	{
		InputComponent->Priority = 10;

		FInputKeyBinding& KeyBinding =
			InputComponent->BindKey(
				InteractKey,
				IE_Pressed,
				this,
				&AAEDStrandedTravelerEncounter::
				HandleInteractPressed
			);

		KeyBinding.bConsumeInput = true;

		bInteractionInputBound = true;
	}
}

void AAEDStrandedTravelerEncounter::DisableInteractionInput()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (IsValid(PlayerController))
	{
		AActor::DisableInput(PlayerController);
	}
}

void AAEDStrandedTravelerEncounter::UpdateInteractionPrompt()
{
	if (!InteractionPrompt)
	{
		return;
	}

	const bool bShouldShowPrompt =
		bEncounterActive &&
		bPlayerInsideInteractionRange &&
		!bHelpAccepted &&
		!bOutcomeSubmitted;

	InteractionPrompt->SetText(InteractionPromptText);
	InteractionPrompt->SetVisibility(bShouldShowPrompt);
}

void AAEDStrandedTravelerEncounter::SubmitOutcomeAndRequestCleanup(const FString& OutcomeReason)
{
	if (bOutcomeSubmitted)
	{
		return;
	}

	bOutcomeSubmitted = true;
	UpdateInteractionPrompt();

	if (!IsValid(CachedDirector))
	{
		PrintInteractionDebug(
			FString::Printf(
				TEXT(
					"Cannot submit outcome %s: "
					"Director is invalid"
				),
				*OutcomeReason
			),
			true
		);

		return;
	}

	const bool bAccepted =
		CachedDirector->
		RequestActiveEncounterResolution(
			this,
			OutcomeReason
		);

	if (!bAccepted)
	{
		PrintInteractionDebug(
			FString::Printf(
				TEXT(
					"Director rejected resolution request | "
					"Outcome=%s"
				),
				*OutcomeReason
			),
			true
		);

		return;
	}

	PrintInteractionDebug(
		FString::Printf(
			TEXT(
				"Resolution accepted | Outcome=%s"
			),
			*OutcomeReason
		),
		false
	);
}

void AAEDStrandedTravelerEncounter::PrintInteractionDebug(const FString& Message, bool bError) const
{
	if (!bPrintInteractionDebug)
	{
		return;
	}

	const FString FullMessage =
		FString::Printf(
			TEXT("[AED STRANDED TRAVELER] %s"),
			*Message
		);

	if (bError)
	{
		UE_LOG(
			LogAEDStrandedTraveler,
			Error,
			TEXT("%s"),
			*FullMessage
		);
	}
	else
	{
		UE_LOG(
			LogAEDStrandedTraveler,
			Display,
			TEXT("%s"),
			*FullMessage
		);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			3901,
			2.5f,
			bError
			? FColor::Red
			: FColor::Cyan,
			FullMessage
		);
	}
}
