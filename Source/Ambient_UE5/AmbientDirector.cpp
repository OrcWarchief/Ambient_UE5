// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientDirector.h"

#include "AmbientCandidateMarker.h"
#include "AmbientRegionVolume.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AAmbientDirector::AAmbientDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	CandidateMarkerClass = AAmbientCandidateMarker::StaticClass();
}

void AAmbientDirector::BeginPlay()
{
	Super::BeginPlay();

	UpdateWorldState();

	if (UWorld* World = GetWorld())
	{
		const float SafeUpdateInterval = FMath::Max(0.1f, UpdateInterval);

		World->GetTimerManager().SetTimer(
			WorldStateTimerHandle,
			this,
			&AAmbientDirector::UpdateWorldState,
			SafeUpdateInterval,
			true
		);
	}
}

void AAmbientDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyCandidateMarker();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WorldStateTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AAmbientDirector::UpdateWorldState()
{
	CurrentWorldState = FAmbientWorldState();
	CurrentRegion = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		if (bPrintDebug)
		{
			PrintWorldStateDebug();
		}

		return;
	}

	CurrentWorldState.GameTimeSeconds = World->GetTimeSeconds();

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	CurrentWorldState.bHasPlayerPawn = IsValid(PlayerPawn);

	if (CurrentWorldState.bHasPlayerPawn)
	{
		CurrentWorldState.PlayerLocation = PlayerPawn->GetActorLocation();
		CurrentWorldState.PlayerSpeed2D = PlayerPawn->GetVelocity().Size2D();

		UpdateCurrentRegion(PlayerPawn);
		UpdateCandidateLocation(PlayerPawn);
	}

	UpdateCandidateMarker();
	
	if (bDrawRegionDebug)
	{
		DrawRegionDebug();
	}

	if (bDrawCandidateDebug)
	{
		DrawCandidateDebug();
	}

	if (bPrintDebug)
	{
		PrintWorldStateDebug();
	}
}

void AAmbientDirector::UpdateCandidateLocation(const APawn* PlayerPawn)
{
	if (!IsValid(PlayerPawn))
	{
		RejectCandidate(TEXT("No player pawn"));
		return;
	}

	if (MaximumSpawnDistance < MinimumSpawnDistance)
	{
		RejectCandidate(TEXT("Invalid distance settings: MaximumSpawnDistance is smaller than MinimumSpawnDistance"));
		return;
	}

	CurrentWorldState.RequestedCandidateDistance = CandidateDistance;
	CurrentWorldState.UsedCandidateDistance = FMath::Clamp(
		CandidateDistance,
		MinimumSpawnDistance,
		MaximumSpawnDistance
	);

	FVector CandidateDirection = PlayerPawn->GetActorForwardVector(); // TODO: 전방을 콘앵글 씌우고 랜덤한 값으로 변경...?
	CandidateDirection.Z = 0.0f;
	CandidateDirection = CandidateDirection.GetSafeNormal();
	
	if (CandidateDirection.IsNearlyZero())
	{
		CandidateDirection = FVector::ForwardVector;
	}

	CurrentWorldState.RawCandidateLocation = CurrentWorldState.PlayerLocation + CandidateDirection * CurrentWorldState.UsedCandidateDistance;
	CurrentWorldState.CandidateLocation = CurrentWorldState.RawCandidateLocation;
	CurrentWorldState.bHasCandidateLocation = true;

	FVector GroundLocation = FVector::ZeroVector;
	FHitResult GroundHit;

	if (!ProjectPointToGround(CurrentWorldState.RawCandidateLocation, PlayerPawn, GroundLocation, GroundHit))
	{
		RejectCandidate(TEXT("No ground found under candidate"));
		return;
	}

	if (GroundHit.ImpactNormal.Z < MinimumGroundNormalZ)
	{
		const FString Reason = FString::Printf(
			TEXT("Ground too steep: NormalZ=%.2f"),
			GroundHit.ImpactNormal.Z
		);

		RejectCandidate(Reason);
		return;
	}

	CurrentWorldState.CandidateLocation = GroundLocation;
	CurrentWorldState.bCandidateProjectedToGround = true;
	CurrentWorldState.CandidateDistance2D = FVector::Dist2D(CurrentWorldState.PlayerLocation, CurrentWorldState.CandidateLocation);

	if (CurrentWorldState.CandidateDistance2D < MinimumSpawnDistance)
	{
		const FString Reason = FString::Printf(
			TEXT("Too close: Dist=%.0f Min=%.0f"),
			CurrentWorldState.CandidateDistance2D,
			MinimumSpawnDistance
		);

		RejectCandidate(Reason);
		return;
	}

	if (CurrentWorldState.CandidateDistance2D > MaximumSpawnDistance)
	{
		const FString Reason = FString::Printf(
			TEXT("Too far: Dist=%.0f Max=%.0f"),
			CurrentWorldState.CandidateDistance2D,
			MaximumSpawnDistance
		);

		RejectCandidate(Reason);
		return;
	}

	FHitResult PathBlockHit;

	if (IsPathToCandidateBlocked(PlayerPawn, CurrentWorldState.CandidateLocation, PathBlockHit))
	{
		const FString Reason = FString::Printf(
			TEXT("Blocked path: %s"),
			*GetNameSafe(PathBlockHit.GetActor())
		);

		RejectCandidate(Reason);
		return;
	}

	FHitResult AreaBlockHit;

	if (IsCandidateAreaBlocked(PlayerPawn, CurrentWorldState.CandidateLocation, AreaBlockHit))
	{
		const FString Reason = FString::Printf(
			TEXT("Candidate area occupied: %s"),
			*GetNameSafe(AreaBlockHit.GetActor())
		);

		RejectCandidate(Reason);
		return;
	}

	AcceptCandidate();
}

void AAmbientDirector::UpdateCurrentRegion(const APawn* PlayerPawn)
{
	CurrentRegion = nullptr;
	CurrentWorldState.bHasCurrentRegion = false;
	CurrentWorldState.CurrentRegionName = NAME_None;
	CurrentWorldState.CurrentRegionPriority = 0;

	UWorld* World = GetWorld();

	if (!World || !IsValid(PlayerPawn))
	{
		return;
	}

	const FVector QueryLocation = CurrentWorldState.PlayerLocation;

	bool bFoundAnyRegion = false;
	int32 BestPriority = 0;

	for (TActorIterator<AAmbientRegionVolume> RegionIt(World); RegionIt; ++RegionIt)
	{
		AAmbientRegionVolume* Region = *RegionIt;

		if (!IsValid(Region))
		{
			continue;
		}

		if (!Region->ContainsWorldLocation(QueryLocation))
		{
			continue;
		}

		if (!bFoundAnyRegion || Region->GetPriority() > BestPriority)
		{
			bFoundAnyRegion = true;
			BestPriority = Region->GetPriority();
			CurrentRegion = Region;
		}
	}

	if (IsValid(CurrentRegion))
	{
		CurrentWorldState.bHasCurrentRegion = true;
		CurrentWorldState.CurrentRegionName = CurrentRegion->GetRegionName();
		CurrentWorldState.CurrentRegionPriority = CurrentRegion->GetPriority();
	}
}

bool AAmbientDirector::ProjectPointToGround(const FVector& Point, const APawn* PlayerPawn, FVector& OutGroundLocation, FHitResult& OutGroundHit) const
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	const FVector TraceStart = Point + FVector::UpVector * GroundTraceUpDistance;
	const FVector TraceEnd = Point - FVector::UpVector * GroundTraceDownDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(PlayerPawn);

	const bool bHitGround = World->LineTraceSingleByChannel(
		OutGroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	if (!bHitGround || !OutGroundHit.bBlockingHit)
	{
		return false;
	}

	OutGroundLocation = OutGroundHit.ImpactPoint;
	return true;
}

bool AAmbientDirector::IsPathToCandidateBlocked(const APawn* PlayerPawn, const FVector& GroundLocation, FHitResult& OutBlockHit) const
{
	UWorld* World = GetWorld();

	if (!World || !IsValid(PlayerPawn))
	{
		return false;
	}

	const FVector TraceStart = CurrentWorldState.PlayerLocation;
	const FVector TraceEnd = GroundLocation + FVector::UpVector * ObstructionCheckHeight;

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(PlayerPawn);

	const bool bBlocked = World->LineTraceSingleByChannel(
		OutBlockHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	return bBlocked && OutBlockHit.bBlockingHit;
}

bool AAmbientDirector::IsCandidateAreaBlocked(const APawn* PlayerPawn, const FVector& GroundLocation, FHitResult& OutBlockHit) const
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	const FVector SweepStart = GroundLocation + FVector::UpVector * (CandidateClearanceRadius + 5.0f);

	const FVector SweepEnd = SweepStart + FVector::UpVector * 1.0f;

	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(CandidateClearanceRadius);

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bFindInitialOverlaps = true;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(PlayerPawn);

	const bool bBlocked = World->SweepSingleByChannel(
		OutBlockHit,
		SweepStart,
		SweepEnd,
		FQuat::Identity,
		ECC_Visibility,
		SweepShape,
		QueryParams
	);

	return bBlocked && OutBlockHit.bBlockingHit;
}

void AAmbientDirector::RejectCandidate(const FString& Reason)
{
	CurrentWorldState.bCandidateValid = false;
	CurrentWorldState.CandidateRejectReason = Reason;
}

void AAmbientDirector::AcceptCandidate()
{
	CurrentWorldState.bCandidateValid = true;
	CurrentWorldState.CandidateRejectReason = TEXT("Accepted");
}

void AAmbientDirector::UpdateCandidateMarker()
{
	const bool bShouldShowMarker =
		bUseVisibleCandidateMarker &&
		CurrentWorldState.bHasCandidateLocation &&
		CurrentWorldState.bCandidateValid;

	if (!bShouldShowMarker)
	{
		DestroyCandidateMarker();
		return;
	}

	if (!CandidateMarkerClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector  MarkerLocation = CurrentWorldState.CandidateLocation;
	const FRotator MakrerRotation = FRotator::ZeroRotator;

	if (!IsValid(ActiveCandidateMarker))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ActiveCandidateMarker = World->SpawnActor<AAmbientCandidateMarker>(
			CandidateMarkerClass,
			MarkerLocation,
			MakrerRotation,
			SpawnParams
		);
	}
	else
	{
		ActiveCandidateMarker->SetActorLocation(MarkerLocation);
		ActiveCandidateMarker->SetActorRotation(MakrerRotation);
	}

	if (IsValid(ActiveCandidateMarker))
	{
		ActiveCandidateMarker->SetMarkerActive(true);
	}
}

void AAmbientDirector::DestroyCandidateMarker()
{
	if (IsValid(ActiveCandidateMarker))
	{
		ActiveCandidateMarker->Destroy();
	}

	ActiveCandidateMarker = nullptr;
}

void AAmbientDirector::PrintWorldStateDebug() const
{
	if (!GEngine)
	{
		return;
	}

	FString Message;

	if (CurrentWorldState.bHasPlayerPawn && CurrentWorldState.bHasCandidateLocation)
	{
		const auto State = CurrentWorldState;

		const FString CandidateStatus = CurrentWorldState.bCandidateValid
			? TEXT("ACCEPTED")
			: TEXT("REJECTED");

		const FString MarkerStatus = IsValid(ActiveCandidateMarker)
			? TEXT("Spawned")
			: TEXT("None");

		const FString RegionNameString = CurrentWorldState.bHasCurrentRegion
			? CurrentWorldState.CurrentRegionName.ToString()
			: TEXT("None");

		const FString SummaryLine = FString::Printf(
			TEXT("[AD] Candidate %s | Region=%s | Time=%.1fs | Marker=%s | Reason=%s"),
			*CandidateStatus,
			*RegionNameString,
			State.GameTimeSeconds,
			*MarkerStatus,
			*State.CandidateRejectReason
		);

		const FString DetailLine = FString::Printf(
			TEXT("     CandidateLoc=(X=%.0f Y=%.0f Z=%.0f) | PlayerLoc=(X=%.0f Y=%.0f Z=%.0f) | Speed2D=%.0f | RequestedDist=%.0f | UsedDist=%.0f | Dist2D=%.0f | Grounded=%s"),
			State.CandidateLocation.X,
			State.CandidateLocation.Y,
			State.CandidateLocation.Z,

			State.PlayerLocation.X,
			State.PlayerLocation.Y,
			State.PlayerLocation.Z,

			State.PlayerSpeed2D,
			State.RequestedCandidateDistance,
			State.UsedCandidateDistance,
			State.CandidateDistance2D,
			State.bCandidateProjectedToGround ? TEXT("true") : TEXT("false")
		);

		Message = SummaryLine + LINE_TERMINATOR + DetailLine;
	}
	else if (CurrentWorldState.bHasPlayerPawn)
	{
		const FString RegionNameString = CurrentWorldState.bHasCurrentRegion
			? CurrentWorldState.CurrentRegionName.ToString()
			: TEXT("None");

		Message = FString::Printf(
			TEXT("[AD] World State | Region=%s | Time=%.1fs | Player found | No candidate location | Reason=%s"),
			*RegionNameString,
			CurrentWorldState.GameTimeSeconds,
			*CurrentWorldState.CandidateRejectReason
		);
	}
	else
	{
		Message = FString::Printf(
			TEXT("[AD] World State | Time=%.1fs | No player pawn found"),
			CurrentWorldState.GameTimeSeconds
		);
	}

	GEngine->AddOnScreenDebugMessage(
		1001,
		UpdateInterval * 0.85f,
		CurrentWorldState.bCandidateValid ? FColor::Green : FColor::Red,
		Message
	);

	UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
}

void AAmbientDirector::DrawCandidateDebug() const
{
	if (!CurrentWorldState.bHasCandidateLocation)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float DebugLifeTime = FMath::Max(0.05f, UpdateInterval * 0.9f);
	const FColor CandidateColor = CurrentWorldState.bCandidateValid
		? FColor::Green
		: FColor::Red;

	const FVector MarkerLocation = CurrentWorldState.bCandidateProjectedToGround
		? CurrentWorldState.CandidateLocation + FVector::UpVector * CandidateDebugRadius
		: CurrentWorldState.CandidateLocation;

	DrawDebugSphere(
		World,
		MarkerLocation,
		CandidateDebugRadius,
		16,
		CandidateColor,
		false,
		DebugLifeTime,
		0,
		2.0f
	);

	DrawDebugLine(
		World,
		CurrentWorldState.PlayerLocation,
		MarkerLocation,
		CandidateColor,
		false,
		DebugLifeTime,
		0,
		1.5f
	);

	DrawDebugSphere(
		World,
		CurrentWorldState.RawCandidateLocation,
		20.0f,
		8,
		FColor::Yellow,
		false,
		DebugLifeTime,
		0,
		1.0f
	);

	const FString WorldLabel = CurrentWorldState.bCandidateValid
		? TEXT("Candidate ACCEPTED")
		: FString::Printf(TEXT("Candidate REJECTED\n%s"), *CurrentWorldState.CandidateRejectReason);

	DrawDebugString(
		World,
		MarkerLocation + FVector::UpVector * 90.0f,
		WorldLabel,
		nullptr,
		CandidateColor,
		DebugLifeTime,
		true
	);
}

void AAmbientDirector::DrawRegionDebug() const
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float DebugLifeTime = FMath::Max(0.05f, UpdateInterval * 0.9f);

	for (TActorIterator<AAmbientRegionVolume> RegionIt(World); RegionIt; ++RegionIt)
	{
		const AAmbientRegionVolume* Region = *RegionIt;

		if (!IsValid(Region))
		{
			continue;
		}

		const UBoxComponent* RegionBounds = Region->GetRegionBounds();

		if (!RegionBounds)
		{
			continue;
		}

		const bool bIsCurrentRegion = Region == CurrentRegion;

		const FColor RegionColor = bIsCurrentRegion
			? Region->GetRegionDebugColor().ToFColor(true)
			: FColor(80, 80, 80);

		const float Thickness = bIsCurrentRegion ? 4.0f : 1.0f;

		DrawDebugBox(
			World,
			RegionBounds->GetComponentLocation(),
			RegionBounds->GetScaledBoxExtent(),
			RegionBounds->GetComponentQuat(),
			RegionColor,
			false,
			DebugLifeTime,
			0,
			Thickness
		);

		if (bIsCurrentRegion)
		{
			const FVector LabelLocation =
				RegionBounds->GetComponentLocation()
				+ FVector::UpVector * (RegionBounds->GetScaledBoxExtent().Z + 80.0f);

			const FString Label = FString::Printf(
				TEXT("Current Region: %s"),
				*Region->GetRegionName().ToString()
			);

			DrawDebugString(
				World,
				LabelLocation,
				Label,
				nullptr,
				RegionColor,
				DebugLifeTime,
				true
			);
		}
	}

}
