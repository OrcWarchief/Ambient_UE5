#include "AEDVistaFinaleTrigger.h"

#include "AmbientDirector.h"
#include "AmbientDirectorTypes.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
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
	Super::EndPlay(EndPlayReason);

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

	if (!bFinaleStarted)
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
	if (!GetWorld() || bFinaleStarted)
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
	if (bFinaleStarted || !bPlayerInsideTrigger)
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

	if (bRequireMounted &&
		Director->GetTraversalState() != EAmbientTraversalState::Mounted)
	{
		OutReason = TEXT("Traversal must be Mounted");
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

	bFinaleStarted = true;
	bFinaleCompleted = false;
	LastStartBlockReason.Reset();

	ClearRuntimeChecks();

	CachedSequencePlayer->OnFinished.RemoveDynamic(
		this,
		&AAEDVistaFinaleTrigger::HandleSequenceFinished);

	CachedSequencePlayer->OnFinished.AddDynamic(
		this,
		&AAEDVistaFinaleTrigger::HandleSequenceFinished);

	SetPlayerInputLocked(true);
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