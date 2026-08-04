// Fill out your copyright notice in the Description page of Project Settings.


#include "AEDMountedRoadsideEncounter.h"

#include "AmbientDirector.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAEDMountedRoadside, Log, All);

AAEDMountedRoadsideEncounter::AAEDMountedRoadsideEncounter()
{
	PrimaryActorTick.bCanEverTick = false;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetGenerateOverlapEvents(false);
	}

	StopEvaluationSphere = CreateDefaultSubobject<USphereComponent>(TEXT("StopEvaluationSphere"));
	StopEvaluationSphere->SetupAttachment(GetRootComponent());
	StopEvaluationSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StopEvaluationSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	StopEvaluationSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	StopEvaluationSphere->SetGenerateOverlapEvents(true);

	StopEvaluationSphere->OnComponentBeginOverlap.AddDynamic(
		this,
		&AAEDMountedRoadsideEncounter::HandleStopRangeBeginOverlap);
	StopEvaluationSphere->OnComponentEndOverlap.AddDynamic(
		this,
		&AAEDMountedRoadsideEncounter::HandleStopRangeEndOverlap);

	ResolutionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ResolutionSphere"));
	ResolutionSphere->SetupAttachment(GetRootComponent());
	ResolutionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ResolutionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ResolutionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ResolutionSphere->SetGenerateOverlapEvents(true);

	ResolutionSphere->OnComponentBeginOverlap.AddDynamic(
		this,
		&AAEDMountedRoadsideEncounter::HandleResolutionRangeBeginOverlap);
	ResolutionSphere->OnComponentEndOverlap.AddDynamic(
		this,
		&AAEDMountedRoadsideEncounter::HandleResolutionRangeEndOverlap);
}

void AAEDMountedRoadsideEncounter::InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context)
{
	Super::InitializeAmbientEncounter_Implementation(Context);

	CachedDirector = Cast<AAmbientDirector>(Context.DirectorActor);

	bEncounterActive = false;
	bPlayerInsideStopRange = false;
	bPlayerInsideResolutionRange = false;
	bPlayerEnteredResolutionRange = false;
	bPlayerStoppedToListen = false;
	bOutcomeSubmitted = false;
	AccumulatedStoppedSeconds = 0.0f;

	StopStopEvaluation(true);

	PrintMountedDebug(
		FString::Printf(
			TEXT("Initialized | Director=%s | Encounter=%s"),
			*GetNameSafe(CachedDirector),
			*Context.EncounterId.ToString()),
		!IsValid(CachedDirector));
}

void AAEDMountedRoadsideEncounter::OnAmbientEncounterWaiting_Implementation()
{
	Super::OnAmbientEncounterWaiting_Implementation();

	bEncounterActive = false;
	bPlayerInsideStopRange = false;
	bPlayerInsideResolutionRange = false;
	bPlayerEnteredResolutionRange = false;
	bPlayerStoppedToListen = false;
	bOutcomeSubmitted = false;

	StopStopEvaluation(true);

	PrintMountedDebug(
		TEXT("Waiting | Mounted rider has not entered the engage distance"),
		false
	);
}

void AAEDMountedRoadsideEncounter::OnAmbientEncounterActivated_Implementation()
{
	Super::OnAmbientEncounterActivated_Implementation();

	bEncounterActive = true;
	bPlayerStoppedToListen = false;
	bOutcomeSubmitted = false;
	AccumulatedStoppedSeconds = 0.0f;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	bPlayerInsideStopRange =
		IsValid(PlayerPawn) &&
		IsValid(StopEvaluationSphere) &&
		StopEvaluationSphere->IsOverlappingActor(PlayerPawn);

	bPlayerInsideResolutionRange =
		IsValid(PlayerPawn) &&
		IsValid(ResolutionSphere) &&
		ResolutionSphere->IsOverlappingActor(PlayerPawn);

	bPlayerEnteredResolutionRange = bPlayerInsideResolutionRange;

	if (bPlayerInsideStopRange)
	{
		StartStopEvaluation();
	}

	PlayWarningGesture();

	PrintMountedDebug(
		FString::Printf(
			TEXT("Activated | Pawn=%s | InsideStop=%s | InsideResolution=%s"),
			*GetNameSafe(PlayerPawn),
			bPlayerInsideStopRange ? TEXT("Yes") : TEXT("No"),
			bPlayerInsideResolutionRange ? TEXT("Yes") : TEXT("No")
		),
		false
	);
}

void AAEDMountedRoadsideEncounter::OnAmbientEncounterCleanup_Implementation(const FString& Reason)
{
	bEncounterActive = false;
	StopStopEvaluation(true);

	Super::OnAmbientEncounterCleanup_Implementation(Reason);

	if (Reason.Equals(TEXT("Stopped to listen"), ESearchCase::IgnoreCase))
	{
		SetFloatingText(StoppedCleanupText);
	}
	else if (Reason.Equals(TEXT("Passed by"), ESearchCase::IgnoreCase))
	{
		SetFloatingText(PassedCleanupText);
	}

	PrintMountedDebug(FString::Printf(TEXT("Cleanup | Outcome=%s"), *Reason), false);
}

void AAEDMountedRoadsideEncounter::OnAmbientEncounterFinished_Implementation(const FString& Reason)
{
	StopStopEvaluation(true);

	Super::OnAmbientEncounterFinished_Implementation(Reason);

	PrintMountedDebug(FString::Printf(TEXT("Finished | Outcome=%s"), *Reason), false);
}

void AAEDMountedRoadsideEncounter::BeginPlay()
{
	Super::BeginPlay();

	if (StopEvaluationSphere)
	{
		StopEvaluationSphere->SetSphereRadius(FMath::Max(100.0f, StopEvaluationRadius));
	}

	if (ResolutionSphere)
	{
		const float SafeResolutionRadius = 
			FMath::Max(StopEvaluationRadius + 100.0f, ResolutionRadius);

		ResolutionSphere->SetSphereRadius(SafeResolutionRadius);
	}
}

void AAEDMountedRoadsideEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopStopEvaluation(true);
	Super::EndPlay(EndPlayReason);
}

void AAEDMountedRoadsideEncounter::HandleStopRangeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComponent, 
	int32 OtherBodyIndex, 
	bool bFromSweep, 
	const FHitResult& SweepResult)
{
	if (!IsCurrentPlayerActor(OtherActor))
	{
		return;
	}

	bPlayerInsideStopRange = true;

	if (bEncounterActive &&
		!bOutcomeSubmitted &&
		!bPlayerStoppedToListen)
	{
		StartStopEvaluation();
	}

	PrintMountedDebug(TEXT("Player entered stop-evaluation range"), false);
}

void AAEDMountedRoadsideEncounter::HandleStopRangeEndOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComponent, 
	int32 OtherBodyIndex)
{
	if (!IsCurrentPlayerActor(OtherActor))
	{
		return;
	}

	bPlayerInsideStopRange = false;

	if (!bPlayerStoppedToListen)
	{
		StopStopEvaluation(true);
	}

	PrintMountedDebug(TEXT("Player left stop-evaluation range"), false);
}

void AAEDMountedRoadsideEncounter::HandleResolutionRangeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComponent, 
	int32 OtherBodyIndex, 
	bool bFromSweep, 
	const FHitResult& SweepResult)
{
	if (!IsCurrentPlayerActor(OtherActor))
	{
		return;
	}

	bPlayerInsideResolutionRange = true;

	if (bEncounterActive)
	{
		bPlayerEnteredResolutionRange = true;
	}

	PrintMountedDebug(TEXT("Player entered resolution range"), false);
}

void AAEDMountedRoadsideEncounter::HandleResolutionRangeEndOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComponent, 
	int32 OtherBodyIndex)
{
	if (!IsCurrentPlayerActor(OtherActor))
	{
		return;
	}

	bPlayerInsideResolutionRange = false;

	if (!bEncounterActive ||
		!bPlayerEnteredResolutionRange ||
		bOutcomeSubmitted)
	{
		return;
	}

	const FString OutcomeReason = bPlayerStoppedToListen
		? TEXT("Stopped to listen")
		: TEXT("Passed by");

	SubmitOutcomeAndRequestCleanup(OutcomeReason);
}

void AAEDMountedRoadsideEncounter::StartStopEvaluation()
{
	if (!bEncounterActive ||
		!bPlayerInsideStopRange ||
		bOutcomeSubmitted ||
		bPlayerStoppedToListen)
	{
		return;
	}

	AccumulatedStoppedSeconds = 0.0f;

	const float SafeSampleInterval = FMath::Max(0.05f, StopSampleIntervalSeconds);

	GetWorldTimerManager().SetTimer(
		StopEvaluationTimerHandle,
		this,
		&AAEDMountedRoadsideEncounter::EvaluateStopState,
		SafeSampleInterval,
		true,
		0.0f
	);
}

void AAEDMountedRoadsideEncounter::StopStopEvaluation(bool bResetAccumulatedTime)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(StopEvaluationTimerHandle);
	}

	if (bResetAccumulatedTime)
	{
		AccumulatedStoppedSeconds = 0.0f;
	}
}

void AAEDMountedRoadsideEncounter::EvaluateStopState()
{
	if (!bEncounterActive ||
		!bPlayerInsideStopRange ||
		bOutcomeSubmitted ||
		bPlayerStoppedToListen)
	{
		StopStopEvaluation(!bPlayerStoppedToListen);
		return;
	}

	if (IsValid(CachedDirector) &&
		CachedDirector->GetTraversalState() != EAmbientTraversalState::Mounted)
	{
		AccumulatedStoppedSeconds = 0.0f;
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!IsValid(PlayerPawn))
	{
		StopStopEvaluation(true);
		return;
	}

	const float PlayerSpeed2D = PlayerPawn->GetVelocity().Size2D();
	const float SafeSampleInterval = FMath::Max(0.05f, StopSampleIntervalSeconds);

	if (PlayerSpeed2D <= StopSpeedThreshold)
	{
		AccumulatedStoppedSeconds += SafeSampleInterval;
	}
	else
	{
		AccumulatedStoppedSeconds = 0.0f;
	}

	PrintMountedDebug(
		FString::Printf(
			TEXT("Stop sample | Speed=%.0f | Accumulated=%.2f/%.2f"),
			PlayerSpeed2D,
			AccumulatedStoppedSeconds,
			RequiredStoppedDurationSeconds
		),
		false
	);

	if (AccumulatedStoppedSeconds < FMath::Max(0.1f, RequiredStoppedDurationSeconds))
	{
		return;
	}

	bPlayerStoppedToListen = true;

	StopStopEvaluation(false);
	SetFloatingText(StoppedResponseText);

	if (StoppedResponseSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, StoppedResponseSound, GetActorLocation());
	}

	PrintMountedDebug(
		TEXT("Mounted player stopped long enough | Outcome marked as Stopped to listen"),
		false
	);
}

bool AAEDMountedRoadsideEncounter::IsCurrentPlayerActor(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return IsValid(PlayerPawn) && Actor == PlayerPawn;
}

void AAEDMountedRoadsideEncounter::SubmitOutcomeAndRequestCleanup(const FString& OutcomeReason)
{
	if (bOutcomeSubmitted)
	{
		return;
	}

	bOutcomeSubmitted = true;
	StopStopEvaluation(true);

	if (!IsValid(CachedDirector))
	{
		bOutcomeSubmitted = false;

		PrintMountedDebug(
			FString::Printf(
				TEXT("Cannot submit outcome %s: Director is invalid"),
				*OutcomeReason
			),
			true
		);

		return;
	}

	const bool bAccepted =
		CachedDirector->RequestActiveEncounterResolution(this, OutcomeReason);

	if (!bAccepted)
	{
		bOutcomeSubmitted = false;

		PrintMountedDebug(
			FString::Printf(
				TEXT("Director rejected resolution request | Outcome=%s"),
				*OutcomeReason
			),
			true
		);

		return;
	}

	PrintMountedDebug(
		FString::Printf(TEXT("Resolution accepted | Outcome=%s"), *OutcomeReason),
		false
	);
}

void AAEDMountedRoadsideEncounter::PlayWarningGesture()
{
	if (!WarningGestureMontage || !GetMesh())
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (!IsValid(AnimInstance))
	{
		return;
	}

	AnimInstance->Montage_Play(
		WarningGestureMontage,
		FMath::Max(0.1f, WarningGesturePlayRate)
	);
}

void AAEDMountedRoadsideEncounter::PrintMountedDebug(const FString& Message, bool bError) const
{
	if (!bPrintMountedDebug)
	{
		return;
	}

	const FString FullMessage =
		FString::Printf(TEXT("[AED MOUNTED ROADSIDE] %s"), *Message);

	if (bError)
	{
		UE_LOG(LogAEDMountedRoadside, Error, TEXT("%s"), *FullMessage);
	}
	else
	{
		UE_LOG(LogAEDMountedRoadside, Display, TEXT("%s"), *FullMessage);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			4001,
			2.5f,
			bError ? FColor::Red : FColor::Cyan,
			FullMessage
		);
	}
}
