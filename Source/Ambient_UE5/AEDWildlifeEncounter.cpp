#include "AEDWildlifeEncounter.h"

#include "AEDWildlifeMemberCharacter.h"
#include "AmbientDirector.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAEDWildlifeEncounter, Log, All);

AAEDWildlifeEncounter::AAEDWildlifeEncounter()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));

	SetRootComponent(SceneRoot);
	SetActorEnableCollision(false);

	MemberLocalOffsets =
	{
		FVector(-140.0f, -100.0f, 0.0f),
		FVector(60.0f, 110.0f, 0.0f),
		FVector(170.0f, -60.0f, 0.0f),
		FVector(-40.0f, 210.0f, 0.0f)
	};

}

void AAEDWildlifeEncounter::InitializeAmbientEncounter_Implementation(const FAmbientEncounterRuntimeContext& Context)
{
	RuntimeContext = Context;
	CachedDirector = Cast<AAmbientDirector>(Context.DirectorActor);

	bEncounterActive = false;
	bFleeStarted = false;
	bOutcomeSubmitted = false;

	ClearFleeResolutionTimer();
	DestroyWildlifeMembers();

	const bool bSpawnedMembers = SpawnWildlifeMembers();

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Initialized | Director=%s | Encounter=%s | Members=%d"),
			*GetNameSafe(CachedDirector),
			*Context.EncounterId.ToString(),
			SpawnedWildlifeMembers.Num()),
		!IsValid(CachedDirector) || !bSpawnedMembers
	);
}

void AAEDWildlifeEncounter::OnAmbientEncounterWaiting_Implementation()
{
	bEncounterActive = false;
	bFleeStarted = false;
	bOutcomeSubmitted = false;

	ClearFleeResolutionTimer();

	if (SpawnedWildlifeMembers.IsEmpty())
	{
		SpawnWildlifeMembers();
	}

	PrintWildlifeDebug(TEXT("Waiting | Wildlife group is idle"), false);
}

void AAEDWildlifeEncounter::OnAmbientEncounterActivated_Implementation()
{
	bEncounterActive = true;
	bOutcomeSubmitted = false;
	StartWildlifeFlee();
}

void AAEDWildlifeEncounter::OnAmbientEncounterCleanup_Implementation(const FString& Reason)
{
	bEncounterActive = false;
	ClearFleeResolutionTimer();

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Cleanup | Reason=%s | Members continue fleeing"), *Reason), false);
}

void AAEDWildlifeEncounter::OnAmbientEncounterFinished_Implementation(const FString& Reason)
{
	bEncounterActive = false;
	ClearFleeResolutionTimer();

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Finished | Reason=%s | Destroying wildlife members"), *Reason), false);
	DestroyWildlifeMembers();
}

void AAEDWildlifeEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFleeResolutionTimer();
	DestroyWildlifeMembers();

	Super::EndPlay(EndPlayReason);
}

bool AAEDWildlifeEncounter::SpawnWildlifeMembers()
{
	const int32 SafeMemberCount = FMath::Clamp(WildlifeMemberCount, 1, 8);

	int32 ExistingValidMemberCount = 0;
	for (const TObjectPtr<APawn>& ExistingMember : SpawnedWildlifeMembers)		
	{
		if (IsValid(ExistingMember))
		{
			++ExistingValidMemberCount;
		}
	}

	if (ExistingValidMemberCount == SafeMemberCount)
	{
		return true;
	}

	if (ExistingValidMemberCount > 0)
	{
		DestroyWildlifeMembers();
	}

	SpawnedWildlifeMembers.Reset();

	for (int32 MemberIndex = 0; MemberIndex < SafeMemberCount; ++MemberIndex)
	{
		if (APawn* NewMember = SpawnWildlifeMember(MemberIndex))
		{
			SpawnedWildlifeMembers.Add(NewMember);
		}
	}

	return SpawnedWildlifeMembers.Num() == SafeMemberCount;
}

APawn* AAEDWildlifeEncounter::SpawnWildlifeMember(int32 MemberIndex)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const TSubclassOf<APawn> MemberClass = GetWildlifeMemberClassForIndex(MemberIndex);
	if (!MemberClass)
	{
		PrintWildlifeDebug(
			FString::Printf(TEXT("Member %d has no valid class"), MemberIndex), true);
		return nullptr;
	}

	const FVector LocalOffset = GetWildlifeMemberOffset(MemberIndex);
	const FVector RotatedWorldOffset = GetActorTransform().TransformVectorNoScale(LocalOffset);
	FVector SpawnLocation = GetActorLocation() + RotatedWorldOffset;

	float DefaultHalfHeight = 50.0f;
	if (const APawn* MemberCDO = MemberClass.GetDefaultObject())
	{
		DefaultHalfHeight = FMath::Max(0.0f, MemberCDO->GetDefaultHalfHeight());
	}

	SpawnLocation.Z += DefaultHalfHeight + FMath::Max(0.0f, SpawnHeightPadding);

	const int32 SafeMemberCount = FMath::Max(1, WildlifeMemberCount);
	const float CenteredIndex = static_cast<float>(MemberIndex) - 
		(static_cast<float>(SafeMemberCount - 1) * 0.5f);

	FRotator SpawnRotation = GetActorRotation();
	SpawnRotation.Yaw += CenteredIndex * 8.0f;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* NewMember =
		World->SpawnActor<APawn>(
			MemberClass,
			SpawnLocation,
			SpawnRotation,
			SpawnParameters);

	if (!IsValid(NewMember))
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT("Failed to spawn member %d | Class=%s"),
				MemberIndex,
				*GetNameSafe(MemberClass)),
			true);
		return nullptr;
	}

	NewMember->Tags.AddUnique(FName(TEXT("AED.Wildlife.Member")));

	if (ACharacter* Character = Cast<ACharacter>(NewMember))
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			Capsule->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
		}
	}

	if (!IsValid(NewMember->GetController()))
	{
		NewMember->SpawnDefaultController();
	}

	if (bDrawFleeDebug)
	{
		DrawDebugSphere(
			World,
			NewMember->GetActorLocation(),
			45.0f,
			12,
			FColor::Green,
			false,
			5.0f,
			0,
			2.0f
		);
	}

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Spawned member %d | Actor=%s | Controller=%s"),
			MemberIndex,
			*GetNameSafe(NewMember),
			*GetNameSafe(NewMember->GetController())),
		false);

	return NewMember;
}

TSubclassOf<APawn> AAEDWildlifeEncounter::GetWildlifeMemberClassForIndex(int32 MemberIndex) const
{
	if (!WildlifeMemberClasses.IsEmpty())
	{
		const int32 WrappedIndex = MemberIndex % WildlifeMemberClasses.Num();
		if (WildlifeMemberClasses[WrappedIndex])
		{
			return WildlifeMemberClasses[WrappedIndex];
		}

		for (const TSubclassOf<APawn>&MemberClass : WildlifeMemberClasses)
		{
			if (MemberClass)
			{
				return MemberClass;
			}
		}
	}

	return AAEDWildlifeMemberCharacter::StaticClass();
}

FVector AAEDWildlifeEncounter::GetWildlifeMemberOffset(int32 MemberIndex) const
{
	if (!MemberLocalOffsets.IsEmpty())
	{
		return MemberLocalOffsets[MemberIndex % MemberLocalOffsets.Num()];
	}

	const int32 SafeCount = FMath::Max(1, WildlifeMemberCount);

	const float AngleDegrees = (360.0f / static_cast<float>(SafeCount)) * 
		static_cast<float>(MemberIndex);

	const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);

	return FVector(
		FMath::Cos(AngleRadians) * 160.0f,
		FMath::Sin(AngleRadians) * 160.0f,
		0.0f);
}

FVector AAEDWildlifeEncounter::CalculateFleeDirection() const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	FVector FleeDirection = IsValid(PlayerPawn)
		? GetActorLocation() - PlayerPawn->GetActorLocation()
		: -GetActorForwardVector();

	FleeDirection.Z = 0.0f;

	if (!FleeDirection.Normalize())
	{
		FleeDirection = -GetActorForwardVector();
		FleeDirection.Z = 0.0f;
		FleeDirection.Normalize();
	}

	return FleeDirection.RotateAngleAxis(
		FleeDirectionYawOffsetDegrees,
		FVector::UpVector);
}

void AAEDWildlifeEncounter::StartWildlifeFlee()
{
	if (bFleeStarted || bOutcomeSubmitted)
	{
		return;
	}

	if (!bEncounterActive)
	{
		return;
	}

	if (SpawnedWildlifeMembers.IsEmpty())
	{
		SpawnWildlifeMembers();
	}

	bFleeStarted = true;

	const FVector FleeDirection = CalculateFleeDirection();
	int32 SuccessfulMoveRequestCount = 0;

	for (int32 MemberIndex = 0; MemberIndex < SpawnedWildlifeMembers.Num(); ++MemberIndex)
	{
		APawn* WildlifeMember = SpawnedWildlifeMembers[MemberIndex];
		if (IssueFleeMove(WildlifeMember, MemberIndex, FleeDirection))
		{
			++SuccessfulMoveRequestCount;
		}
	}

	if (StartleSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, StartleSound, GetActorLocation());
	}

	const float SafeResolutionDelay = FMath::Max(0.1f, FleeDurationBeforeResolution);
	GetWorldTimerManager().SetTimer(
		FleeResolutionTimerHandle,
		this,
		&AAEDWildlifeEncounter::
		ResolveWildlifeFlee,
		SafeResolutionDelay,
		false);

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Flee started | MoveRequests=%d/%d | Direction=(%.2f, %.2f)"),
			SuccessfulMoveRequestCount,
			SpawnedWildlifeMembers.Num(),
			FleeDirection.X,
			FleeDirection.Y),
		SuccessfulMoveRequestCount == 0
	);
}

bool AAEDWildlifeEncounter::IssueFleeMove(APawn* WildlifeMember, int32 MemberIndex, const FVector& FleeDirection)
{
	if (!IsValid(WildlifeMember))
	{
		return false;
	}

	if (AAEDWildlifeMemberCharacter* AEDWildlifeMember =
			Cast<AAEDWildlifeMemberCharacter>(WildlifeMember))
	{
		AEDWildlifeMember->PrepareForAmbientFlee(FleeSpeed);
	}
	else if (ACharacter* Character = Cast<ACharacter>(WildlifeMember))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = FMath::Max(100.0f, FleeSpeed);
			Movement->bOrientRotationToMovement = true;
		}

		if (!IsValid(Character->GetController()))
		{
			Character->SpawnDefaultController();
		}
	}
	else if (!IsValid(WildlifeMember->GetController()))
	{
		WildlifeMember->SpawnDefaultController();
	}

	AAIController* WildLifeController = Cast<AAIController>(WildlifeMember->GetController());
	if (!IsValid(WildLifeController))
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT("Member has no AIController | Member=%s"), *GetNameSafe(WildlifeMember)),
			true);
		return false;
	}

	const FVector RightDirection = FVector::CrossProduct(
		FVector::UpVector, 
		FleeDirection).GetSafeNormal();

	const int32 SafeMemberCount = FMath::Max(1, SpawnedWildlifeMembers.Num());
	const float CenteredIndex = static_cast<float>(MemberIndex) -
		(static_cast<float>(SafeMemberCount - 1) * 0.5f);
	const float DistanceVariation = 1.0f + (0.05f * static_cast<float>(MemberIndex % 3));

	const FVector Destination = WildlifeMember->GetActorLocation() +
		(FleeDirection * FMath::Max(100.0f, FleeDistance) * DistanceVariation) +
		(RightDirection * CenteredIndex * MemberLateralSpacing);

	WildlifeMember->SetActorRotation(FleeDirection.Rotation());

	const EPathFollowingRequestResult::Type MoveRequestResult =
		WildLifeController->MoveToLocation(
			Destination,
			100.0f,
			false,
			true,
			true,
			false,
			nullptr,
			true);

	if (bDrawFleeDebug && GetWorld())
	{
		DrawDebugLine(
			GetWorld(),
			WildlifeMember->
			GetActorLocation(),
			Destination,
			FColor::Cyan,
			false,
			6.0f,
			0,
			3.0f);

		DrawDebugSphere(
			GetWorld(),
			Destination,
			60.0f,
			12,
			FColor::Yellow,
			false,
			6.0f,
			0,
			2.0f);
	}

	if (MoveRequestResult == EPathFollowingRequestResult::Failed)
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT("Move request failed | Member=%s"), *GetNameSafe(WildlifeMember)),
			true);
		return false;
	}

	if (MoveRequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT("Move request projected to current position | Member=%s"), 
				*GetNameSafe(WildlifeMember)), true);
		return false;
	}

	return true;
}

void AAEDWildlifeEncounter::ResolveWildlifeFlee()
{
	if (!bEncounterActive || bOutcomeSubmitted)
	{
		return;
	}

	if (!IsValid(CachedDirector))
	{
		PrintWildlifeDebug(
			TEXT("Cannot resolve wildlife encounter: Director is invalid"),
			true);
		return;
	}

	bOutcomeSubmitted = true;

	const bool bResolutionAccepted = CachedDirector->RequestActiveEncounterResolution(
			this,
			TEXT("Fled from rider"));

	if (!bResolutionAccepted)
	{
		bOutcomeSubmitted = false;

		PrintWildlifeDebug(
			TEXT("Director rejected wildlife resolution request"), true);
		return;
	}

	PrintWildlifeDebug(
		TEXT("Resolution accepted | Outcome=Fled from rider"),
		false);
}

void AAEDWildlifeEncounter::ClearFleeResolutionTimer()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(FleeResolutionTimerHandle);
	}
}

void AAEDWildlifeEncounter::DestroyWildlifeMembers()
{
	for (TObjectPtr<APawn>& WildlifeMember : SpawnedWildlifeMembers)
	{
		if (!IsValid(WildlifeMember))
		{
			continue;
		}

		if (AAIController* AIController =
				Cast<AAIController>(WildlifeMember->GetController()))
		{
			AIController->StopMovement();
			AIController->UnPossess();
			AIController->Destroy();
		}

		WildlifeMember->Destroy();
	}

	SpawnedWildlifeMembers.Reset();
}

void AAEDWildlifeEncounter::PrintWildlifeDebug(const FString& Message, bool bError) const
{
	if (!bPrintWildlifeDebug)
	{
		return;
	}

	const FString FullMessage = FString::Printf(TEXT("[AED WILDLIFE] %s"), *Message);

	if (bError)
	{
		UE_LOG(LogAEDWildlifeEncounter, Error, TEXT("%s"), *FullMessage);
	}
	else
	{
		UE_LOG(LogAEDWildlifeEncounter, Display, TEXT("%s"), *FullMessage);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			4101,
			2.5f,
			bError ? FColor::Red : FColor::Green,
			FullMessage);
	}
}
