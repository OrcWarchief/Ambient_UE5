#include "AEDWildlifeEncounter.h"

#include "AEDWildlifeMemberCharacter.h"
#include "AmbientDirector.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#include "NavigationPath.h"
#include "NavigationSystem.h"

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
	bReactionStarted = false;
	bFleeStarted = false;
	bFleeStartSequenceFinalized = false;
	bOutcomeSubmitted = false;

	ClearFleeStartTimer();
	ClearMemberFleeStartTimers();
	ClearFleeResolutionTimer();
	DestroyWildlifeMembers();

	const bool bSpawnedMembers = SpawnWildlifeMembers();

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Initialized | Director=%s | Encounter=%s | Members=%d"),
			*GetNameSafe(CachedDirector),
			*Context.EncounterId.ToString(),
			SpawnedWildlifeMembers.Num()),
		!IsValid(CachedDirector) || !bSpawnedMembers);
}

void AAEDWildlifeEncounter::OnAmbientEncounterWaiting_Implementation()
{
	bEncounterActive = false;
	bReactionStarted = false;
	bFleeStarted = false;
	bFleeStartSequenceFinalized = false;
	bOutcomeSubmitted = false;

	ClearFleeStartTimer();
	ClearMemberFleeStartTimers();
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
	bReactionStarted = false;
	bOutcomeSubmitted = false;

	BeginWildlifeReaction();
}

void AAEDWildlifeEncounter::OnAmbientEncounterCleanup_Implementation(const FString& Reason)
{
	bEncounterActive = false;
	bReactionStarted = false;

	SetAllWildlifeMembersAlerted(false);
	ResetFleeTracking();

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Cleanup | Reason=%s | Active flee moves continue; pending member starts cancelled"),
			*Reason),
		false);
}

void AAEDWildlifeEncounter::OnAmbientEncounterFinished_Implementation(const FString& Reason)
{
	bEncounterActive = false;
	bReactionStarted = false;

	SetAllWildlifeMembersAlerted(false);
	ResetFleeTracking();

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Finished | Reason=%s | Destroying wildlife members"),
			*Reason),
		false);

	DestroyWildlifeMembers();
}

void AAEDWildlifeEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetFleeTracking();
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

float AAEDWildlifeEncounter::GetMemberFleeStartDelay(int32 MemberIndex) const
{
	const int32 SafeMemberIndex = FMath::Max(0, MemberIndex);
	const float SafeDelayStep = FMath::Max(0.0f, MemberFleeStartDelayStep);

	return static_cast<float>(SafeMemberIndex) * SafeDelayStep;
}

void AAEDWildlifeEncounter::SetWildlifeMemberAlerted(APawn* WildlifeMember, bool bNewAlerted) const
{
	AAEDWildlifeMemberCharacter* AEDWildlifeMember =
		Cast<AAEDWildlifeMemberCharacter>(WildlifeMember);

	if (!IsValid(AEDWildlifeMember))
	{
		return;
	}

	AEDWildlifeMember->SetAmbientAlerted(bNewAlerted);
}

void AAEDWildlifeEncounter::SetAllWildlifeMembersAlerted(const bool bNewAlerted) const
{
	for (const TObjectPtr<APawn>& WildlifeMember : SpawnedWildlifeMembers)
	{
		SetWildlifeMemberAlerted(WildlifeMember, bNewAlerted);
	}
}

void AAEDWildlifeEncounter::BeginWildlifeReaction()
{
	if (bReactionStarted || bFleeStarted || bOutcomeSubmitted || !bEncounterActive)
	{
		return;
	}

	bReactionStarted = true;

	if (bUseMemberAlertReaction)
	{
		SetAllWildlifeMembersAlerted(true);
	}
	else
	{
		SetAllWildlifeMembersAlerted(false);
	}

	// debug
	int32 CompatibleMemberCount = 0;
	int32 AlertedMemberCount = 0;

	for (const TObjectPtr<APawn>& WildlifeMember : SpawnedWildlifeMembers)
	{
		const AAEDWildlifeMemberCharacter* AEDWildlifeMember =
			Cast<AAEDWildlifeMemberCharacter>(WildlifeMember);

		if (!IsValid(AEDWildlifeMember))
		{
			continue;
		}

		++CompatibleMemberCount;

		if (AEDWildlifeMember->IsAmbientAlerted())
		{
			++AlertedMemberCount;
		}
	}
	// debug

	PrintWildlifeDebug(
		FString::Printf(
			TEXT(
				"Reaction alert state | "
				"UseAlert=%s | Compatible=%d/%d | Alerted=%d/%d"
			),
			bUseMemberAlertReaction ? TEXT("Yes") : TEXT("No"),
			CompatibleMemberCount,
			SpawnedWildlifeMembers.Num(),
			AlertedMemberCount,
			SpawnedWildlifeMembers.Num()
		),
		false
	);

	if (StartleSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			StartleSound,
			GetActorLocation());
	}

	const float SafeReactionDelay = FMath::Max(0.0f, ReactionDelayBeforeFlee);

	PrintWildlifeDebug(
		FString::Printf(
			TEXT(
				"Reaction started | "
				"DelayBeforeFlee=%.2f s | Members=%d"
			),
			SafeReactionDelay,
			SpawnedWildlifeMembers.Num()
		),
		false
	);

	if (SafeReactionDelay <= KINDA_SMALL_NUMBER)
	{
		StartWildlifeFlee();
		return;
	}

	if (!GetWorld())
	{
		PrintWildlifeDebug(
			TEXT(
				"Reaction timer unavailable because World is invalid; "
				"starting flee immediately"
			),
			true
		);

		StartWildlifeFlee();
		return;
	}

	GetWorldTimerManager().SetTimer(
		FleeStartTimerHandle,
		this,
		&AAEDWildlifeEncounter::StartWildlifeFlee,
		SafeReactionDelay,
		false
	);
}

void AAEDWildlifeEncounter::StartWildlifeFlee()
{
	ClearFleeStartTimer();
	ClearMemberFleeStartTimers();

	if (bFleeStarted || bOutcomeSubmitted || !bEncounterActive)
	{
		return;
	}

	if (SpawnedWildlifeMembers.IsEmpty())
	{
		SpawnWildlifeMembers();
	}

	if (SpawnedWildlifeMembers.IsEmpty())
	{
		PrintWildlifeDebug(TEXT("Flee failed | No wildlife members are available"), true);
		SubmitWildlifeResolution(TEXT("Wildlife failed to flee"));

		return;
	}

	bFleeStarted = true;
	bFleeStartSequenceFinalized = false;

	AcceptedFleeStartLocations.Reset();

	const FVector BaseFleeDirection = CalculateFleeDirection().GetSafeNormal2D();
	const float SearchDirectionSign = FMath::RandBool() ? 1.0f : -1.0f;

	float PlannedYawOffsetDegrees = 0.0f;
	float PlannedDistanceScale = FMath::Clamp(MinimumFleeDistanceScale, 0.25f, 1.0f);

	const bool bFoundFleePlan = FindReachableHerdFleePlan(
		BaseFleeDirection,
		SearchDirectionSign,
		PlannedYawOffsetDegrees,
		PlannedDistanceScale);

	const int32 MemberCount = SpawnedWildlifeMembers.Num();

	PendingMemberFleeStartCount = MemberCount;
	MemberFleeStartTimerHandles.SetNum(MemberCount);

	const float MaximumMemberStartDelay = GetMemberFleeStartDelay(MemberCount - 1);
	const FVector PlannedDirection = BaseFleeDirection
		.RotateAngleAxis(PlannedYawOffsetDegrees, FVector::UpVector)
		.GetSafeNormal2D();

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Flee sequence scheduled | Members=%d | StartDelayStep=%.2f s | MaxStartDelay=%.2f s | "
				"Plan=%s | YawOffset=%.0f | DistanceScale=%.2f | Direction=(%.2f, %.2f)"),
			MemberCount,
			FMath::Max(0.0f, MemberFleeStartDelayStep),
			MaximumMemberStartDelay,
			bFoundFleePlan ? TEXT("Found") : TEXT("Fallback"),
			PlannedYawOffsetDegrees,
			PlannedDistanceScale,
			PlannedDirection.X,
			PlannedDirection.Y),
		false);

	UWorld* World = GetWorld();

	for (int32 MemberIndex = 0; MemberIndex < MemberCount; ++MemberIndex)
	{
		const float MemberStartDelay = GetMemberFleeStartDelay(MemberIndex);

		if (MemberStartDelay <= KINDA_SMALL_NUMBER || !IsValid(World))
		{
			StartWildlifeMemberFlee(
				MemberIndex,
				BaseFleeDirection,
				PlannedYawOffsetDegrees,
				PlannedDistanceScale,
				SearchDirectionSign);

			continue;
		}

		FTimerDelegate MemberStartDelegate;

		MemberStartDelegate.BindUObject(
			this,
			&AAEDWildlifeEncounter::StartWildlifeMemberFlee,
			MemberIndex,
			BaseFleeDirection,
			PlannedYawOffsetDegrees,
			PlannedDistanceScale,
			SearchDirectionSign);

		World->GetTimerManager().SetTimer(
			MemberFleeStartTimerHandles[MemberIndex],
			MemberStartDelegate,
			MemberStartDelay,
			false);
	}
}

void AAEDWildlifeEncounter::StartWildlifeMemberFlee(int32 MemberIndex, FVector BaseFleeDirection, 
	float PlannedYawOffsetDegrees, float PlannedDistanceScale, float SearchDirectionSign)
{
	if (!bEncounterActive || bOutcomeSubmitted || !bFleeStarted)
	{
		return;
	}

	const float MemberStartDelay = GetMemberFleeStartDelay(MemberIndex);

	bool bMoveAccepted = false;

	if (SpawnedWildlifeMembers.IsValidIndex(MemberIndex))
	{
		APawn* WildlifeMember = SpawnedWildlifeMembers[MemberIndex];

		const FVector StartLocation = IsValid(WildlifeMember)
			? WildlifeMember->GetActorLocation()
			: FVector::ZeroVector;

		bMoveAccepted = IssueFleeMove(
			WildlifeMember,
			MemberIndex,
			BaseFleeDirection,
			PlannedYawOffsetDegrees,
			PlannedDistanceScale,
			SearchDirectionSign);

		if (bMoveAccepted)
		{
			SetWildlifeMemberAlerted(WildlifeMember, false);
		}
		else
		{
			SetWildlifeMemberAlerted(WildlifeMember, bUseMemberAlertReaction);
		}

		if (bMoveAccepted && IsValid(WildlifeMember))
		{
			AcceptedFleeStartLocations.Add(
				TWeakObjectPtr<APawn>(WildlifeMember),
				StartLocation);
		}
	}
	else
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT("Member flee start failed | Invalid MemberIndex=%d"),
				MemberIndex),
			true);
	}

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Member flee start | Index=%d | Delay=%.2f s | Actor=%s | MoveAccepted=%s"),
			MemberIndex,
			MemberStartDelay,
			SpawnedWildlifeMembers.IsValidIndex(MemberIndex)
			? *GetNameSafe(SpawnedWildlifeMembers[MemberIndex])
			: TEXT("Invalid"),
			bMoveAccepted ? TEXT("Yes") : TEXT("No")),
		!bMoveAccepted);

	PendingMemberFleeStartCount = FMath::Max(0, PendingMemberFleeStartCount - 1);

	if (PendingMemberFleeStartCount == 0)
	{
		FinalizeWildlifeFleeStartSequence();
	}
}

void AAEDWildlifeEncounter::FinalizeWildlifeFleeStartSequence()
{
	if (bFleeStartSequenceFinalized || !bEncounterActive || bOutcomeSubmitted)
	{
		return;
	}

	bFleeStartSequenceFinalized = true;

	MemberFleeStartTimerHandles.Reset();

	const int32 SuccessfulMoveRequestCount = AcceptedFleeStartLocations.Num();
	const int32 SafeMemberCount = FMath::Clamp(WildlifeMemberCount, 1, 8);
	const int32 RequiredMoveRequestCount = FMath::Clamp(
		MinimumSuccessfulFleeMemberCount,
		1,
		SafeMemberCount);

	if (SuccessfulMoveRequestCount < RequiredMoveRequestCount)
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT("Flee start sequence failed | MoveRequests=%d/%d | Required=%d"),
				SuccessfulMoveRequestCount,
				SpawnedWildlifeMembers.Num(),
				RequiredMoveRequestCount),
			true);

		SubmitWildlifeResolution(TEXT("Wildlife failed to flee"));

		return;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		PrintWildlifeDebug(TEXT("Flee start sequence failed | World became invalid"), true);
		SubmitWildlifeResolution(TEXT("Wildlife failed to flee"));

		return;
	}

	const float SafeResolutionDelay = FMath::Max(0.1f, FleeDurationBeforeResolution);

	World->GetTimerManager().SetTimer(
		FleeResolutionTimerHandle,
		this,
		&AAEDWildlifeEncounter::ResolveWildlifeFlee,
		SafeResolutionDelay,
		false);

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("Flee start sequence complete | MoveRequests=%d/%d | Required=%d | ResolutionIn=%.2f s"),
			SuccessfulMoveRequestCount,
			SpawnedWildlifeMembers.Num(),
			RequiredMoveRequestCount,
			SafeResolutionDelay),
		false);
}

AAIController* AAEDWildlifeEncounter::PrepareWildlifeMemberForFlee(APawn* WildlifeMember) const
{
	if (!IsValid(WildlifeMember))
	{
		return nullptr;
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
	}

	if (!IsValid(WildlifeMember->GetController()))
	{
		WildlifeMember->SpawnDefaultController();
	}

	return Cast<AAIController>(WildlifeMember->GetController());
}

APawn* AAEDWildlifeEncounter::GetFleePlanReferenceMember(int32& OutMemberIndex) const
{
	OutMemberIndex = INDEX_NONE;

	APawn* ReferenceMember = nullptr;
	float LargestAgentRadius = -1.0f;

	for (int32 MemberIndex = 0; MemberIndex < SpawnedWildlifeMembers.Num(); ++MemberIndex)
	{
		APawn* WildlifeMember = SpawnedWildlifeMembers[MemberIndex];

		AAIController* AIController = PrepareWildlifeMemberForFlee(WildlifeMember);

		if (!IsValid(AIController) || AIController->GetPawn() != WildlifeMember)
		{
			continue;
		}

		const float AgentRadius = AIController->GetNavAgentPropertiesRef().AgentRadius;

		if (!IsValid(ReferenceMember) || AgentRadius > LargestAgentRadius)
		{
			ReferenceMember = WildlifeMember;
			OutMemberIndex = MemberIndex;
			LargestAgentRadius = AgentRadius;
		}
	}

	return ReferenceMember;
}

bool AAEDWildlifeEncounter::TryFindReachableFleeDestination(
	APawn* WildlifeMember, 
	int32 MemberIndex, 
	const FVector& FleeDirection, 
	float DistanceScale, 
	float LateralScale, 
	FVector& OutDestination) const
{
	OutDestination = FVector::ZeroVector;

	UWorld* World = GetWorld();

	if (!World || !IsValid(WildlifeMember))
	{
		return false;
	}

	AAIController* AIController = Cast<AAIController>(WildlifeMember->GetController());

	if (!IsValid(AIController))
	{
		return false;
	}

	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (!IsValid(NavigationSystem))
	{
		return false;
	}

	const FVector SafeFleeDirection = FleeDirection.GetSafeNormal2D();

	if (SafeFleeDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector ProjectionExtent(
		FMath::Max(1.0f, FMath::Abs(FleeNavigationProjectionExtent.X)),
		FMath::Max(1.0f, FMath::Abs(FleeNavigationProjectionExtent.Y)),
		FMath::Max(1.0f, FMath::Abs(FleeNavigationProjectionExtent.Z)));

	const FNavAgentProperties& AgentProperties = AIController->GetNavAgentPropertiesRef();

	FNavLocation ProjectedStart;

	if (!NavigationSystem->ProjectPointToNavigation(
		WildlifeMember->GetNavAgentLocation(),
		ProjectedStart,
		ProjectionExtent,
		&AgentProperties))
	{
		return false;
	}

	const int32 SafeMemberCount = FMath::Max(1, SpawnedWildlifeMembers.Num());

	const float CenteredIndex = 
		static_cast<float>(MemberIndex) - static_cast<float>(SafeMemberCount - 1) * 0.5f;

	const float DistanceVariation = 1.0f + 0.05f * static_cast<float>(MemberIndex % 3);

	const float SafeDistanceScale = FMath::Clamp(DistanceScale, 0.25f, 1.0f);

	const float SafeLateralScale = FMath::Clamp(LateralScale, 0.0f, 1.0f);

	const FVector RightDirection =
		FVector::CrossProduct(FVector::UpVector, SafeFleeDirection).GetSafeNormal();

	const FVector DesiredDestination =
		WildlifeMember->GetActorLocation() +
		SafeFleeDirection *
		FMath::Max(100.0f, FleeDistance) *
		DistanceVariation *
		SafeDistanceScale +
		RightDirection *
		CenteredIndex *
		FMath::Max(0.0f, MemberLateralSpacing) *
		SafeLateralScale;

	FNavLocation ProjectedDestination;

	if (!NavigationSystem->ProjectPointToNavigation(
		DesiredDestination,
		ProjectedDestination,
		ProjectionExtent,
		&AgentProperties))
	{
		return false;
	}

	const float ProjectionHeightDelta = 
		FMath::Abs(ProjectedDestination.Location.Z - DesiredDestination.Z);

	if (MaxFleeDestinationHeightDelta > 0.0f &&
		ProjectionHeightDelta > MaxFleeDestinationHeightDelta)
	{
		return false;
	}

	UNavigationPath* NavigationPath = 
		UNavigationSystemV1::FindPathToLocationSynchronously(
			World,
			ProjectedStart.Location,
			ProjectedDestination.Location,
			AIController);

	if (!IsValid(NavigationPath) ||
		!NavigationPath->IsValid() ||
		NavigationPath->IsPartial() ||
		NavigationPath->PathPoints.Num() < 2)
	{
		return false;
	}

	OutDestination = ProjectedDestination.Location;
	return true;
}

bool AAEDWildlifeEncounter::FindReachableHerdFleePlan(
	const FVector& BaseFleeDirection, 
	float SearchDirectionSign, 
	float& OutYawOffsetDegrees,
	float& OutDistanceScale) const
{
	OutYawOffsetDegrees = 0.0f;
	OutDistanceScale = FMath::Clamp(MinimumFleeDistanceScale, 0.25f, 1.0f);

	int32 ReferenceMemberIndex = INDEX_NONE;
	APawn* ReferenceMember = GetFleePlanReferenceMember(ReferenceMemberIndex);

	if (!IsValid(ReferenceMember))
	{
		PrintWildlifeDebug(TEXT("No valid reference member for flee planning"), true);
		return false;
	}

	const FVector SafeBaseDirection = BaseFleeDirection.GetSafeNormal2D();

	if (SafeBaseDirection.IsNearlyZero())
	{
		return false;
	}

	const float PreferredSign = SearchDirectionSign < 0.0f ? -1.0f : 1.0f;
	const float AngleStep = FMath::Clamp(FleeDirectionSearchStepDegrees, 1.0f, 45.0f);
	const float MaxSearchAngle = FMath::Clamp(MaxFleeDirectionSearchAngleDegrees, 0.0f, 90.0f);

	TArray<float, TInlineAllocator<16>> YawOffsets;
	YawOffsets.Add(0.0f);

	const int32 AngleStepCount = FMath::CeilToInt(MaxSearchAngle / AngleStep);

	for (int32 AngleIndex = 1; AngleIndex <= AngleStepCount; ++AngleIndex)
	{
		const float Angle = FMath::Min(static_cast<float>(AngleIndex) * AngleStep, MaxSearchAngle);

		YawOffsets.AddUnique(PreferredSign * Angle);
		YawOffsets.AddUnique(-PreferredSign * Angle);
	}

	const float MinimumDistanceScale = FMath::Clamp(MinimumFleeDistanceScale, 0.25f, 1.0f);
	const float DistanceScaleStep = FMath::Clamp(FleeDistanceScaleStep, 0.05f, 0.5f);

	TArray<float, TInlineAllocator<4>> DistanceScales;

	for (float Scale = 1.0f; Scale > MinimumDistanceScale + KINDA_SMALL_NUMBER; Scale -= DistanceScaleStep)
	{
		DistanceScales.Add(Scale);
	}

	DistanceScales.Add(MinimumDistanceScale);

	for (const float DistanceScale : DistanceScales)
	{
		for (const float YawOffsetDegrees : YawOffsets)
		{
			const FVector CandidateDirection =
				SafeBaseDirection
				.RotateAngleAxis(YawOffsetDegrees, FVector::UpVector)
				.GetSafeNormal2D();

			FVector CandidateDestination;

			if (!TryFindReachableFleeDestination(
				ReferenceMember,
				ReferenceMemberIndex,
				CandidateDirection,
				DistanceScale,
				0.0f,
				CandidateDestination))
			{
				continue;
			}

			OutYawOffsetDegrees = YawOffsetDegrees;
			OutDistanceScale = DistanceScale;

			return true;
		}
	}

	PrintWildlifeDebug(
		FString::Printf(
			TEXT("No reachable herd flee plan | Reference=%s"),
			*GetNameSafe(ReferenceMember)),
		true);

	return false;
}

bool AAEDWildlifeEncounter::IssueFleeMove(APawn* WildlifeMember, int32 MemberIndex, const FVector& BaseFleeDirection, float PlannedYawOffsetDegrees, float PlannedDistanceScale, float SearchDirectionSign)
{
	if (!IsValid(WildlifeMember))
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT("Member has no AIController | Member=%s"),
				*GetNameSafe(WildlifeMember)),
			true);

		return false;
	}

	if (AAEDWildlifeMemberCharacter* AEDWildlifeMember =
		Cast<AAEDWildlifeMemberCharacter>(WildlifeMember))
	{
		AEDWildlifeMember->PrepareForAmbientFlee(FleeSpeed);
	}
	else if (ACharacter* Character = Cast<ACharacter>(WildlifeMember))
	{
		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
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

	AAIController* WildlifeController =
		Cast<AAIController>(WildlifeMember->GetController());

	if (!IsValid(WildlifeController))
	{
		return false;
	}

	const bool bControllerPossessesMember = WildlifeController->GetPawn() == WildlifeMember;

	UPathFollowingComponent* PathFollowingComponent = WildlifeController->GetPathFollowingComponent();

	if (!bControllerPossessesMember || !IsValid(PathFollowingComponent))
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT(
					"Controller precheck failed | "
					"Member=%s | Controller=%s | "
					"Possessed=%s | PathFollowing=%s"),
				*GetNameSafe(WildlifeMember),
				*GetNameSafe(WildlifeController),
				bControllerPossessesMember ? TEXT("Yes") : TEXT("No"),
				*GetNameSafe(PathFollowingComponent)),
			true);


		return false;
	}

	const FVector SafeBaseFleeDirection = BaseFleeDirection.GetSafeNormal2D();

	if (SafeBaseFleeDirection.IsNearlyZero())
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT("Invalid base flee direction | Member=%s"),
				*GetNameSafe(WildlifeMember)),
			true);

		return false;
	}

	const float MaxSearchAngle = FMath::Clamp(MaxFleeDirectionSearchAngleDegrees, 0.0f, 90.0f);
	const float SearchAngleStep = FMath::Clamp(FleeDirectionSearchStepDegrees, 1.0f, 45.0f);
	const float PreferredSearchSign = SearchDirectionSign < 0.0f ? -1.0f : 1.0f;
	const float MinimumDistanceScale = FMath::Clamp(MinimumFleeDistanceScale, 0.25f, 1.0f);
	const float SafePlannedYawOffset = FMath::Clamp(PlannedYawOffsetDegrees, -MaxSearchAngle, MaxSearchAngle);
	const float SafePlannedDistanceScale = FMath::Clamp(PlannedDistanceScale, MinimumDistanceScale, 1.0f);

	auto TryMove =
		[
			this,
			WildlifeMember,
			MemberIndex,
			WildlifeController,
			SafeBaseFleeDirection
		](
			const float YawOffsetDegrees,
			const float DistanceScale,
			const float LateralScale)
		{
			const FVector CandidateDirection =
				SafeBaseFleeDirection
				.RotateAngleAxis(YawOffsetDegrees, FVector::UpVector)
				.GetSafeNormal2D();

			if (CandidateDirection.IsNearlyZero())
			{
				return false;
			}

			FVector CandidateDestination = FVector::ZeroVector;

			if (!TryFindReachableFleeDestination(
				WildlifeMember,
				MemberIndex,
				CandidateDirection,
				DistanceScale,
				LateralScale,
				CandidateDestination))
			{
				return false;
			}

			const EPathFollowingRequestResult::Type MoveRequestResult =
				WildlifeController->MoveToLocation(
					CandidateDestination,
					100.0f,
					false,
					true,
					false,
					false,
					nullptr,
					false);

			if (MoveRequestResult != EPathFollowingRequestResult::RequestSuccessful)
			{
				return false;
			}

			WildlifeMember->SetActorRotation(CandidateDirection.Rotation());

			if (bDrawFleeDebug && GetWorld())
			{
				DrawDebugLine(
					GetWorld(),
					WildlifeMember->GetActorLocation(),
					CandidateDestination,
					FColor::Cyan,
					false,
					6.0f,
					0,
					3.0f);

				DrawDebugSphere(
					GetWorld(),
					CandidateDestination,
					60.0f,
					12,
					FColor::Yellow,
					false,
					6.0f,
					0,
					2.0f);
			}

			return true;
		};

	if (TryMove(SafePlannedYawOffset, SafePlannedDistanceScale, 1.0f))
	{
		return true;
	}

	if (TryMove(SafePlannedYawOffset, SafePlannedDistanceScale, 0.0f))
	{
		return true;
	}

	const float PreferredAlternativeYaw = FMath::Clamp(
		SafePlannedYawOffset +
		PreferredSearchSign * SearchAngleStep,
		-MaxSearchAngle,
		MaxSearchAngle);

	const float OppositeAlternativeYaw = FMath::Clamp(
		SafePlannedYawOffset -
		PreferredSearchSign * SearchAngleStep,
		-MaxSearchAngle,
		MaxSearchAngle);

	if (!FMath::IsNearlyEqual(PreferredAlternativeYaw, SafePlannedYawOffset) &&
		TryMove(PreferredAlternativeYaw, SafePlannedDistanceScale, 0.0f))
	{
		return true;
	}

	if (!FMath::IsNearlyEqual(OppositeAlternativeYaw, SafePlannedYawOffset) &&
		!FMath::IsNearlyEqual(OppositeAlternativeYaw, PreferredAlternativeYaw) &&
		TryMove(OppositeAlternativeYaw, SafePlannedDistanceScale, 0.0f))
	{
		return true;
	}

	const float DistanceScaleStep = FMath::Clamp(FleeDistanceScaleStep, 0.05f, 0.5f);

	const float ReducedDistanceScale = FMath::Max(MinimumDistanceScale, SafePlannedDistanceScale - DistanceScaleStep);

	if (ReducedDistanceScale < SafePlannedDistanceScale - KINDA_SMALL_NUMBER)
	{
		if (TryMove(SafePlannedYawOffset, ReducedDistanceScale, 0.0f))
		{
			return true;
		}

		if (!FMath::IsNearlyEqual(PreferredAlternativeYaw, SafePlannedYawOffset) &&
			TryMove(PreferredAlternativeYaw, ReducedDistanceScale, 0.0f))
		{
			return true;
		}

		if (!FMath::IsNearlyEqual(OppositeAlternativeYaw, SafePlannedYawOffset) &&
			!FMath::IsNearlyEqual(OppositeAlternativeYaw, PreferredAlternativeYaw) &&
			TryMove(OppositeAlternativeYaw, ReducedDistanceScale, 0.0f))
		{
			return true;
		}
	}
	PrintWildlifeDebug(
		FString::Printf(
			TEXT(
				"No reachable flee move | "
				"Member=%s | PlannedYaw=%.0f | "
				"PlannedDistanceScale=%.2f"),
			*GetNameSafe(WildlifeMember),
			SafePlannedYawOffset,
			SafePlannedDistanceScale),
		true);


	return false;
}

void AAEDWildlifeEncounter::ResolveWildlifeFlee()
{
	if (!bEncounterActive || bOutcomeSubmitted)
	{
		return;
	}
	
	const int32 SafeMemberCount = FMath::Clamp(WildlifeMemberCount, 1, 8);
	const int32 RequiredSuccessfulMemberCount = FMath::Clamp(MinimumSuccessfulFleeMemberCount, 1, SafeMemberCount);
	const int32 SuccessfulMemberCount = GetSuccessfulFleeMemberCount();

	if (SuccessfulMemberCount < RequiredSuccessfulMemberCount)
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT(
					"Flee failed | Moved=%d/%d | "
					"Required=%d"
				),
				SuccessfulMemberCount,
				SpawnedWildlifeMembers.Num(),
				RequiredSuccessfulMemberCount
			),
			true
		);

		SubmitWildlifeResolution(TEXT("Wildlife failed to flee"));
		return;
	}

	PrintWildlifeDebug(
		FString::Printf(
			TEXT(
				"Flee succeeded | Moved=%d/%d | "
				"Required=%d"
			),
			SuccessfulMemberCount,
			SpawnedWildlifeMembers.Num(),
			RequiredSuccessfulMemberCount
		),
		false
	);

	SubmitWildlifeResolution(TEXT("Fled from rider"));
}

int32 AAEDWildlifeEncounter::GetSuccessfulFleeMemberCount() const
{
	const float RequiredDistance = FMath::Max(1.0f, MinimumSuccessfulFleeDisplacement);
	const float RequiredDistanceSquared = FMath::Square(RequiredDistance);

	int32 SuccessfulMemberCount = 0;

	for (const TPair<TWeakObjectPtr<APawn>, FVector>& Entry : AcceptedFleeStartLocations)
	{
		const APawn* WildlifeMember = Entry.Key.Get();

		if (!IsValid(WildlifeMember))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(
			Entry.Value, WildlifeMember->GetActorLocation());

		if (DistanceSquared >= RequiredDistanceSquared)
		{
			++SuccessfulMemberCount;
		}
	}

	return SuccessfulMemberCount;
}
void AAEDWildlifeEncounter::SubmitWildlifeResolution(
	const FString& OutcomeReason)
{
	if (!bEncounterActive || bOutcomeSubmitted)
	{
		return;
	}

	if (!IsValid(CachedDirector))
	{
		PrintWildlifeDebug(
			FString::Printf(
				TEXT(
					"Cannot submit wildlife resolution: "
					"Director is invalid | Outcome=%s"),
				*OutcomeReason),
			true);

		return;
	}

	bOutcomeSubmitted = true;

	const bool bResolutionAccepted =
		CachedDirector->RequestActiveEncounterResolution(
			this,
			OutcomeReason);

	if (!bResolutionAccepted)
	{
		bOutcomeSubmitted = false;

		PrintWildlifeDebug(
			FString::Printf(
				TEXT(
					"Director rejected wildlife "
					"resolution | Outcome=%s"),
				*OutcomeReason),
			true);

		return;
	}

	PrintWildlifeDebug(
		FString::Printf(
			TEXT(
				"Resolution accepted | Outcome=%s"),
			*OutcomeReason),
		false);
}

void AAEDWildlifeEncounter::ResetFleeTracking()
{
	ClearFleeStartTimer();
	ClearMemberFleeStartTimers();
	ClearFleeResolutionTimer();

	bFleeStartSequenceFinalized = false;

	AcceptedFleeStartLocations.Reset();
}

void AAEDWildlifeEncounter::ClearFleeStartTimer()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(FleeStartTimerHandle);
	}
}

void AAEDWildlifeEncounter::ClearMemberFleeStartTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& TimerHandle : MemberFleeStartTimerHandles)
		{
			World->GetTimerManager().ClearTimer(TimerHandle);
		}
	}

	MemberFleeStartTimerHandles.Reset();
	PendingMemberFleeStartCount = 0;
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

		SetWildlifeMemberAlerted(WildlifeMember, false);

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
	AcceptedFleeStartLocations.Reset();
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
		return;
	}

	UE_LOG(LogAEDWildlifeEncounter, Display, TEXT("%s"), *FullMessage);
}
