
#include "AEDCampHorseEncounter.h"

#include "Components/ActorComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogAEDCampHorseEncounter, Log, All);

AAEDCampHorseEncounter::AAEDCampHorseEncounter()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CampNpc = CreateDefaultSubobject<UChildActorComponent>(TEXT("CampNpc"));
	CampNpc->SetupAttachment(SceneRoot);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->InitSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AAEDCampHorseEncounter::HandleInteractionRangeBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AAEDCampHorseEncounter::HandleInteractionRangeEndOverlap);

	InteractionPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IntearctionPrompt"));
	InteractionPrompt->SetupAttachment(SceneRoot);
	InteractionPrompt->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
	InteractionPrompt->SetHorizontalAlignment(EHTA_Center);
	InteractionPrompt->SetWorldSize(28.0f);
	InteractionPrompt->SetTextRenderColor(FColor::Yellow);
	InteractionPrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPrompt->SetText(InteractionPromptText);
	InteractionPrompt->SetVisibility(false);
}

void AAEDCampHorseEncounter::InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context)
{
	RuntimeContext = Context;

	bEncounterActive = false;
	bPlayerInsideInteractionRange = false;
	bArrivalBarkStarted = false;
	bArrivalBarkCompleted = false;
	bConversationStarted = false;
	bConversationCompleted = false;
	bMountPermissionGranted = false;

	const bool bHasHorse = TryResolveTargetHorse();
	const bool bHasSarah = TryResolveSarah();

	PrintDebugMessage(
		FString::Printf(
			TEXT("Initialized | Horse=%s | Sarah=%s"),
			*GetNameSafe(TargetHorse),
			*GetNameSafe(TargetSarah)
		),
		!bHasHorse || !bHasSarah
	);
}

void AAEDCampHorseEncounter::OnAmbientEncounterWaiting_Implementation()
{
	bEncounterActive = false;
	bPlayerInsideInteractionRange = false;

	StopActiveVoice();
	DisableInteractionInput();

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UpdateInteractionPrompt();

	const bool bHasHorse = TryResolveTargetHorse();
	const bool bHasSarah = TryResolveSarah();

	PrintDebugMessage(
		FString::Printf(
			TEXT("Waiting | Horse=%s | Sarah=%s | Horse state unchanged"),
			*GetNameSafe(TargetHorse),
			*GetNameSafe(TargetSarah)
		),
		!bHasHorse || !bHasSarah
	);
}

void AAEDCampHorseEncounter::OnAmbientEncounterActivated_Implementation()
{
	const bool bHasHorse = TryResolveTargetHorse();
	const bool bHasSarah = TryResolveSarah();

	if (!bHasHorse || !bHasSarah)
	{
		PrintDebugMessage(
			FString::Printf(
				TEXT("Activation incomplete | Horse=%s | Sarah=%s"),
				*GetNameSafe(TargetHorse),
				*GetNameSafe(TargetSarah)
			),
			true
		);
	}

	bEncounterActive = true;

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		InteractionSphere->UpdateOverlaps();
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	bPlayerInsideInteractionRange =
		IsValid(PlayerPawn) &&
		IsValid(InteractionSphere) &&
		InteractionSphere->IsOverlappingActor(PlayerPawn);

	PlayArrivalBark();

	PrintDebugMessage(
		TEXT(
			"Activated | Sarah bark started | "
			"Horse visibility, transform and collision unchanged"
		),
		false
	);
}

void AAEDCampHorseEncounter::OnAmbientEncounterCleanup_Implementation(const FString& Reason)
{
	bEncounterActive = false;
	bPlayerInsideInteractionRange = false;

	StopActiveVoice();
	DisableInteractionInput();

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UpdateInteractionPrompt();

	PrintDebugMessage(
		FString::Printf(
			TEXT("Cleanup | Reason=%s | MountPermissionGranted=%s"),
			*Reason,
			bMountPermissionGranted ? TEXT("Yes") : TEXT("No")
		),
		false
	);
}

void AAEDCampHorseEncounter::OnAmbientEncounterFinished_Implementation(const FString& Reason)
{
	bEncounterActive = false;
	bPlayerInsideInteractionRange = false;

	StopActiveVoice();
	DisableInteractionInput();

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UpdateInteractionPrompt();

	PrintDebugMessage(
		FString::Printf(
			TEXT("Finished | Reason=%s | MountPermissionGranted=%s"),
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
			),
			true
		);

		return false;
	}

	bMountPermissionGranted = true;

	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Mount permission granted | Horse=%s | Runtime=%s"
			),
			*GetNameSafe(TargetHorse),
			*GetNameSafe(HorseRuntimeComponent)
		), false);

	return true;
}

void AAEDCampHorseEncounter::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(
			FMath::Max(50.0f, InteractionRadius)
		);
	}

	if (InteractionPrompt)
	{
		InteractionPrompt->SetText(InteractionPromptText);
		InteractionPrompt->SetVisibility(false);
	}

	TryResolveSarah();
}

void AAEDCampHorseEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopActiveVoice();
	DisableInteractionInput();

	Super::EndPlay(EndPlayReason);
}
void AAEDCampHorseEncounter::HandleInteractionRangeBeginOverlap(
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
	RefreshInteractionAvailability();
}

void AAEDCampHorseEncounter::HandleInteractionRangeEndOverlap(
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
	RefreshInteractionAvailability();
}

void AAEDCampHorseEncounter::HandleInteractPressed()
{
	if (!CanPlayerInteract())
	{
		return;
	}

	bConversationStarted = true;

	DisableInteractionInput();
	FaceSarahTowardPlayer();
	PlayVoice(InteractionLineSound, ECampVoicePhase::InteractionLine);

	PrintDebugMessage(TEXT("Interaction accepted | Sarah horse-offer line started"), false);
}

void AAEDCampHorseEncounter::PlayArrivalBark()
{
	if (bArrivalBarkStarted)
	{
		RefreshInteractionAvailability();
		return;
	}

	bArrivalBarkStarted = true;

	FaceSarahTowardPlayer();
	PlayVoice(ArrivalBarkSound, ECampVoicePhase::ArrivalBark);
}

void AAEDCampHorseEncounter::PlayVoice(USoundBase* Sound, ECampVoicePhase VoicePhase)
{
	StopActiveVoice();

	ActiveVoicePhase = VoicePhase;
	UpdateInteractionPrompt();

	if (!IsValid(Sound))
	{
		PrintDebugMessage(
			VoicePhase == ECampVoicePhase::ArrivalBark
			? TEXT("Arrival bark sound is not assigned; completing bark immediately")
			: TEXT("Interaction line sound is not assigned; completing line immediately"),
			true
		);

		HandleActiveVoiceFinished();
		return;
	}

	USceneComponent* AttachComponent = nullptr;

	if (IsValid(TargetSarah))
	{
		AttachComponent = TargetSarah->GetRootComponent();
	}

	if (!IsValid(AttachComponent))
	{
		AttachComponent = CampNpc;
	}

	if (!IsValid(AttachComponent))
	{
		PrintDebugMessage(TEXT("Cannot play Sarah voice: no valid attach component"), true);
		HandleActiveVoiceFinished();
		return;
	}

	ActiveVoiceComponent = UGameplayStatics::SpawnSoundAttached(
		Sound,
		AttachComponent,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true,
		1.0f,
		1.0f,
		0.0f,
		VoiceAttenuation,
		nullptr,
		true
	);

	if (!IsValid(ActiveVoiceComponent))
	{
		PrintDebugMessage(TEXT("SpawnSoundAttached failed for Sarah voice"), true);
		HandleActiveVoiceFinished();
		return;
	}

	ActiveVoiceComponent->OnAudioFinished.AddDynamic(
		this,
		&AAEDCampHorseEncounter::HandleActiveVoiceFinished
	);
}

void AAEDCampHorseEncounter::HandleActiveVoiceFinished()
{
	const ECampVoicePhase FinishedVoicePhase = ActiveVoicePhase;

	if (IsValid(ActiveVoiceComponent))
	{
		ActiveVoiceComponent->OnAudioFinished.RemoveDynamic(
			this,
			&AAEDCampHorseEncounter::HandleActiveVoiceFinished
		);
	}

	ActiveVoiceComponent = nullptr;
	ActiveVoicePhase = ECampVoicePhase::None;

	switch (FinishedVoicePhase)
	{
	case ECampVoicePhase::ArrivalBark:
		bArrivalBarkCompleted = true;
		RefreshInteractionAvailability();

		PrintDebugMessage(
			FString::Printf(
				TEXT(
					"Arrival bark completed | "
					"InsideInteractionRange=%s | "
					"InteractionAvailable=%s"
				),
				bPlayerInsideInteractionRange
				? TEXT("Yes")
				: TEXT("No"),
				CanPlayerInteract()
				? TEXT("Yes")
				: TEXT("No")
			),
			false
		);
		break;

	case ECampVoicePhase::InteractionLine:
		CompleteCampConversation();
		break;

	default:
		UpdateInteractionPrompt();
		break;
	}
}

void AAEDCampHorseEncounter::StopActiveVoice()
{
	UAudioComponent* VoiceComponent = ActiveVoiceComponent.Get();

	ActiveVoiceComponent = nullptr;
	ActiveVoicePhase = ECampVoicePhase::None;

	if (!IsValid(VoiceComponent))
	{
		return;
	}

	VoiceComponent->OnAudioFinished.RemoveDynamic(
		this,
		&AAEDCampHorseEncounter::HandleActiveVoiceFinished
	);

	VoiceComponent->Stop();
}

void AAEDCampHorseEncounter::CompleteCampConversation()
{
	if (bConversationCompleted)
	{
		return;
	}

	if (!GrantMountPermission())
	{
		bConversationStarted = false;
		RefreshInteractionAvailability();

		PrintDebugMessage(
			TEXT(
				"Conversation could not complete because "
				"horse unlock failed; interaction can be retried"
			),
			true
		);

		return;
	}

	bConversationCompleted = true;

	BP_OnCampConversationCompleted();

	DisableInteractionInput();
	UpdateInteractionPrompt();

	PrintDebugMessage(
		TEXT(
			"Conversation completed | Horse unlocked | "
			"Camp encounter remains active until the player leaves"
		),
		false
	);
}

void AAEDCampHorseEncounter::RefreshInteractionAvailability()
{
	if (CanPlayerInteract())
	{
		EnableInteractionInput();
	}
	else
	{
		DisableInteractionInput();
	}

	UpdateInteractionPrompt();
}

void AAEDCampHorseEncounter::UpdateInteractionPrompt()
{
	if (!InteractionPrompt)
	{
		return;
	}

	if (!bEncounterActive)
	{
		InteractionPrompt->SetVisibility(false);
		return;
	}

	if (bShowSpokenLineAsWorldText &&
		ActiveVoicePhase == ECampVoicePhase::ArrivalBark)
	{
		InteractionPrompt->SetText(ArrivalBarkText);
		InteractionPrompt->SetVisibility(true);
		return;
	}

	if (bShowSpokenLineAsWorldText &&
		ActiveVoicePhase == ECampVoicePhase::InteractionLine)
	{
		InteractionPrompt->SetText(InteractionLineText);
		InteractionPrompt->SetVisibility(true);
		return;
	}

	if (CanPlayerInteract())
	{
		InteractionPrompt->SetText(InteractionPromptText);
		InteractionPrompt->SetVisibility(true);
		return;
	}

	InteractionPrompt->SetVisibility(false);
}

void AAEDCampHorseEncounter::EnableInteractionInput()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (!IsValid(PlayerController))
	{
		PrintDebugMessage(TEXT("Cannot enable Sarah interaction input: invalid PlayerController"), true);
		return;
	}

	AActor::EnableInput(PlayerController);

	if (InputComponent && !bInteractionInputBound)
	{
		InputComponent->Priority = 10;

		FInputKeyBinding& KeyBinding = InputComponent->BindKey(
			InteractKey,
			IE_Pressed,
			this,
			&AAEDCampHorseEncounter::HandleInteractPressed
		);

		KeyBinding.bConsumeInput = true;
		bInteractionInputBound = true;
	}
}

void AAEDCampHorseEncounter::DisableInteractionInput()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (IsValid(PlayerController))
	{
		AActor::DisableInput(PlayerController);
	}
}

bool AAEDCampHorseEncounter::CanPlayerInteract() const
{
	return
		bEncounterActive &&
		bPlayerInsideInteractionRange &&
		bArrivalBarkCompleted &&
		!bConversationStarted &&
		!bConversationCompleted;
}

bool AAEDCampHorseEncounter::IsCurrentPlayerActor(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	return IsValid(PlayerPawn) && Actor == PlayerPawn;
}

bool AAEDCampHorseEncounter::TryResolveSarah()
{
	if (IsValid(TargetSarah))
	{
		return true;
	}

	if (!IsValid(CampNpc))
	{
		return false;
	}

	TargetSarah = CampNpc->GetChildActor();

	return IsValid(TargetSarah);
}

void AAEDCampHorseEncounter::FaceSarahTowardPlayer()
{
	if (!TryResolveSarah())
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!IsValid(PlayerPawn))
	{
		return;
	}

	FVector ToPlayer = PlayerPawn->GetActorLocation() - TargetSarah->GetActorLocation();
	ToPlayer.Z = 0.0f;

	if (ToPlayer.IsNearlyZero())
	{
		return;
	}

	TargetSarah->SetActorRotation(ToPlayer.Rotation());
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
		UE_LOG(LogAEDCampHorseEncounter, Error, TEXT("%s"), *FullMessage);
	}
	else
	{
		UE_LOG(LogAEDCampHorseEncounter, Display, TEXT("%s"), *FullMessage);
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



