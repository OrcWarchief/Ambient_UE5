#include "AEDVistaFinaleTrigger.h"

#include "AmbientDirector.h"
#include "AmbientDirectorTypes.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAEDVistaFinale, Log, All);

AAEDVistaFinaleTrigger::AAEDVistaFinaleTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(FVector(600.0f, 600.0f, 250.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(
		this,
		&AAEDVistaFinaleTrigger::HandleTriggerBeginOverlap);

	TriggerBox->OnComponentEndOverlap.AddDynamic(
		this,
		&AAEDVistaFinaleTrigger::HandleTriggerEndOverlap);
}

void AAEDVistaFinaleTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(Director))
	{
		PrintDebugMessage(
			TEXT("Director is not assigned. Assign AD_Director_Main on the placed trigger."),
			true);
	}

	if (!IsValid(VistaSequenceActor))
	{
		PrintDebugMessage(
			TEXT("VistaSequenceActor is not assigned. Assign AD_Vista_Sequence on the placed trigger."),
			true);
	}
}

void AAEDVistaFinaleTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRuntimeChecks();

	if (IsValid(CachedSequencePlayer))
	{
		CachedSequencePlayer->OnFinished.RemoveDynamic(
			this,
			&AAEDVistaFinaleTrigger::HandleSequenceFinished);
	}

	if (bInputLocked)
	{
		SetPlayerInputLocked(false);
	}

	CachedSequencePlayer = nullptr;
	CachedPlayerController = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AAEDVistaFinaleTrigger::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComponent, 
	int32 OtherBodyIndex, 
	bool bFromSweep, 
	const FHitResult& SweepResult)
{
	if (!IsCurrentPlayerActor(OtherActor) || bFinaleStarted)
	{
		return;
	}

	bPlayerInsideTrigger = true;
	LastStartBlockReason.Reset();

	PrintDebugMessage(TEXT("Player entered Vista finale trigger."), false);

	TryStartFinale();

	if (!bFinaleStarted && !bFinaleStartFailed)
	{
		BeginRuntimeChecks();
	}
}

void AAEDVistaFinaleTrigger::HandleTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComponent, 
	int32 OtherBodyIndex)
{
	if (!IsCurrentPlayerActor(OtherActor))
	{
		return;
	}

	bPlayerInsideTrigger = false;

	if (!bFinaleStarted)
	{
		ClearRuntimeChecks();
		PrintDebugMessage(TEXT("Player left Vista trigger before finale began."), false);
	}
}

void AAEDVistaFinaleTrigger::HandleSequenceFinished()
{
	bFinaleCompleted = true;

	if (bRestoreInputAfterSequence)
	{
		SetPlayerInputLocked(false);
	}

	PrintDebugMessage(TEXT("Vista finale sequence finished."), false);
}

void AAEDVistaFinaleTrigger::BeginRuntimeChecks()
{
	if (!GetWorld() || bFinaleStarted || bFinaleCompleted || bFinaleStartFailed)
	{
		return;
	}

	const float SafeInterval = FMath::Max(RuntimeCheckIntervalSeconds, 0.05f);

	GetWorldTimerManager().SetTimer(
		RuntimeCheckTimerHandle,
		this,
		&AAEDVistaFinaleTrigger::TryStartFinale,
		SafeInterval,
		true);
}

void AAEDVistaFinaleTrigger::ClearRuntimeChecks()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(RuntimeCheckTimerHandle);
	}
}

void AAEDVistaFinaleTrigger::TryStartFinale()
{
	if (bFinaleStarted || bFinaleCompleted ||
		bFinaleStartFailed || !bPlayerInsideTrigger)
	{
		return;
	}

	FString BlockReason;

	if (!CanStartFinale(BlockReason))
	{
		if (BlockReason != LastStartBlockReason)
		{
			LastStartBlockReason = BlockReason;
			PrintDebugMessage(
				FString::Printf(TEXT("Finale waiting | %s"), *BlockReason),
				false);
		}

		return;
	}

	StartFinale();
}

bool AAEDVistaFinaleTrigger::CanStartFinale(FString& OutReason) const
{
	if (!bPlayerInsideTrigger)
	{
		OutReason = TEXT("Player is outside trigger");
		return false;
	}

	if (bFinaleStarted)
	{
		OutReason = TEXT("Finale already started");
		return false;
	}

	if (bFinaleStartFailed)
	{
		OutReason = TEXT("Finale start previously failed");
		return false;
	}

	if (!IsValid(Director))
	{
		OutReason = TEXT("Director is invalid");
		return false;
	}

	if (!IsValid(VistaSequenceActor))
	{
		OutReason = TEXT("Level Sequence Actor is invalid");
		return false;
	}

	if (!IsValid(RiderMark))
	{
		OutReason = TEXT("RiderMark is invalid");
		return false;
	}

	if (bRequireMounted &&
		Director->GetTraversalState() != EAmbientTraversalState::Mounted)
	{
		OutReason = TEXT("Traversal must be Mounted");
		return false;
	}

	if (!IsValid(ResolveFinalePawn()))
	{
		OutReason = TEXT("Finale Pawn is invalid or does not match TraversalActor");

		return false;
	}

	if (!Director->IsEncounterRuntimeClear())
	{
		OutReason = TEXT("Ambient Director runtime is not clear");
		return false;
	}

	OutReason = TEXT("Ready");
	return true;
}

void AAEDVistaFinaleTrigger::StartFinale()
{
	if (!IsValid(VistaSequenceActor))
	{
		PrintDebugMessage(TEXT("Cannot start: Sequence Actor is invalid."), true);
		return;
	}

	CachedSequencePlayer = VistaSequenceActor->GetSequencePlayer();

	if (!IsValid(CachedSequencePlayer))
	{
		PrintDebugMessage(TEXT("Cannot start: Sequence Player is invalid."), true);
		return;
	}

	APawn* FinalePawn = ResolveFinalePawn();

	if (!IsValid(FinalePawn))
	{
		PrintDebugMessage(TEXT("Cannot start: Finale Pawn is invalid."), true);
		return;
	}

	bFinaleStarted = true;
	bFinaleCompleted = false;
	LastStartBlockReason.Reset();

	ClearRuntimeChecks();
	SetPlayerInputLocked(true);

	if (!AlignFinalePawn(FinalePawn))
	{
		SetPlayerInputLocked(false);

		bFinaleStarted = false;
		bFinaleCompleted = false;
		bFinaleStartFailed = true;

		ClearRuntimeChecks();

		PrintDebugMessage(
			TEXT("Finale start aborted because Pawn alignment failed. "), true);

		return;
	}

	CachedSequencePlayer->OnFinished.RemoveDynamic(
		this,
		&AAEDVistaFinaleTrigger::HandleSequenceFinished);

	CachedSequencePlayer->OnFinished.AddDynamic(
		this,
		&AAEDVistaFinaleTrigger::HandleSequenceFinished);

	CachedSequencePlayer->Play();

	PrintDebugMessage(TEXT("Vista finale sequence started."), false);
}

void AAEDVistaFinaleTrigger::SetPlayerInputLocked(const bool bLocked)
{
	if (!bLockPlayerInput)
	{
		return;
	}

	if (!IsValid(CachedPlayerController))
	{
		CachedPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	}

	if (!IsValid(CachedPlayerController))
	{
		PrintDebugMessage(TEXT("Cannot change input lock: PlayerController is invalid."), true);
		return;
	}

	CachedPlayerController->SetIgnoreMoveInput(bLocked);
	CachedPlayerController->SetIgnoreLookInput(bLocked);
	bInputLocked = bLocked;

	if (!bLocked)
	{
		return;
	}

	APawn* PlayerPawn = CachedPlayerController->GetPawn();

	if (IsValid(PlayerPawn))
	{
		if (UPawnMovementComponent* Movement = PlayerPawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
	}
}

bool AAEDVistaFinaleTrigger::IsCurrentPlayerActor(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	return IsValid(PlayerPawn) && Actor == PlayerPawn;
}

APawn* AAEDVistaFinaleTrigger::ResolveFinalePawn() const
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (!IsValid(PlayerController))
	{
		return nullptr;
	}

	APawn* ControlledPawn = PlayerController->GetPawn();

	if (!IsValid(ControlledPawn))
	{
		return nullptr;
	}

	if (!bRequireMounted)
	{
		return ControlledPawn;
	}

	if (!IsValid(Director) || Director->GetTraversalState() != EAmbientTraversalState::Mounted)
	{
		return nullptr;
	}

	APawn* TraversalPawn = Cast<APawn>(Director->GetTraversalActor());

	if (!IsValid(TraversalPawn) || TraversalPawn != ControlledPawn)
	{
		return nullptr;
	}

	return TraversalPawn;
}

bool AAEDVistaFinaleTrigger::AlignFinalePawn(APawn* FinalePawn) const
{
	if (!IsValid(FinalePawn) || !IsValid(RiderMark))
	{
		return false;
	}

	UPawnMovementComponent* Movement = FinalePawn->GetMovementComponent();
	FinalePawn->ConsumeMovementInputVector();

	if (IsValid(Movement))
	{
		Movement->StopMovementImmediately();
	}

	const FVector SourceLocation = FinalePawn->GetActorLocation();
	const FVector TargetLocation = RiderMark->GetActorLocation();
	FRotator TargetRotation = RiderMark->GetActorRotation();

	TargetRotation.Pitch = 0.0f;
	TargetRotation.Roll = 0.0f;

	const bool bMoved =
		FinalePawn->SetActorLocationAndRotation(
			TargetLocation,
			TargetRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

	FinalePawn->ConsumeMovementInputVector();

	if (IsValid(Movement))
	{
		Movement->StopMovementImmediately();
	}

	const float LocationError =
		FVector::Dist(FinalePawn->GetActorLocation(), TargetLocation);

	if (!bMoved || LocationError > 5.0f)
	{
		PrintDebugMessage(
			FString::Printf(
				TEXT(
					"Failed to align finale Pawn %s. "
					"Marker=%s MarkerClass=%s "
					"From=%s Target=%s Actual=%s "
					"Error=%.1f cm"),
				*GetNameSafe(FinalePawn),
				*GetNameSafe(RiderMark),
				*GetNameSafe(RiderMark->GetClass()),
				*SourceLocation.ToCompactString(),
				*TargetLocation.ToCompactString(),
				*FinalePawn->GetActorLocation().ToCompactString(),
				LocationError),
			true);

		return false;
	}

	if (AController* Controller = FinalePawn->GetController())
	{
		Controller->SetControlRotation(TargetRotation);
	}

	PrintDebugMessage(
		FString::Printf(
			TEXT(
				"Finale Pawn aligned. "
				"Pawn=%s Marker=%s "
				"Target=%s Yaw=%.1f"),
			*GetNameSafe(FinalePawn),
			*GetNameSafe(RiderMark),
			*TargetLocation.ToCompactString(),
			TargetRotation.Yaw),
		false);

	return true;
}

void AAEDVistaFinaleTrigger::PrintDebugMessage(
	const FString& Message,
	const bool bError) const
{
	if (!bPrintDebug)
	{
		return;
	}

	const FString FullMessage = FString::Printf(
		TEXT("[AED VISTA FINALE] %s"),
		*Message);

	if (bError)
	{
		UE_LOG(LogAEDVistaFinale, Error, TEXT("%s"), *FullMessage);
	}
	else
	{
		UE_LOG(LogAEDVistaFinale, Display, TEXT("%s"), *FullMessage);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			4201,
			3.0f,
			bError ? FColor::Red : FColor::Cyan,
			FullMessage);
	}
}