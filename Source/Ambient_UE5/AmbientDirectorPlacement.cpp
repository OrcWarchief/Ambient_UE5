#include "AmbientDirector.h"

#include "AmbientEncounterPoint.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"

bool AAmbientDirector::FindSpawnTransformForDefinition(
	const FAmbientEncounterDefinition& Definition,
	FTransform& OutSpawnTransform,
	AAmbientEncounterPoint*& OutBestPoint,
	float& OutDistanceToLocation,
	FString& OutReason
) const
{
	OutSpawnTransform = FTransform::Identity;
	OutBestPoint = nullptr;
	OutDistanceToLocation = 0.0f;
	OutReason = TEXT("No location source evaluated");

	switch (Definition.LocationSource)
	{
	case EAmbientEncounterLocationSource::AuthoredPoint:
		return FindAuthoredPointSpawnTransformForDefinition(
			Definition,
			OutSpawnTransform,
			OutBestPoint,
			OutDistanceToLocation,
			OutReason
		);

	case EAmbientEncounterLocationSource::EnvironmentQuery:
		return FindEQSSpawnTransformForDefinition(
			Definition,
			OutSpawnTransform,
			OutDistanceToLocation,
			OutReason
		);

	default:
		OutReason = TEXT("Rejected: unknown encounter location source");
		return false;
	}
}

bool AAmbientDirector::FindAuthoredPointSpawnTransformForDefinition(
	const FAmbientEncounterDefinition& Definition,
	FTransform& OutSpawnTransform,
	AAmbientEncounterPoint*& OutBestPoint,
	float& OutDistanceToPoint,
	FString& OutReason
) const
{
	OutSpawnTransform = FTransform::Identity;
	OutBestPoint = nullptr;
	OutDistanceToPoint = 0.0f;
	OutReason = TEXT("No authored point evaluated");

	UWorld* World = GetWorld();

	if (!World)
	{
		OutReason = TEXT("Rejected: no world");
		return false;
	}

	const float MinDistance = MinimumSpawnDistance;
	const float MaxDistance = FMath::Min(
		Definition.EncounterPointSearchRadius,
		MaximumSpawnDistance
	);

	if (MaxDistance < MinDistance)
	{
		OutReason = FString::Printf(
			TEXT("Rejected: invalid authored-point distance range Min=%.0f Max=%.0f"),
			MinDistance,
			MaxDistance
		);
		return false;
	}

	const float MinDistanceSq = FMath::Square(MinDistance);
	const float MaxDistanceSq = FMath::Square(MaxDistance);

	float BestDistanceSq = TNumericLimits<float>::Max();
	AAmbientEncounterPoint* BestPoint = nullptr;

	for (TActorIterator<AAmbientEncounterPoint> PointIt(World); PointIt; ++PointIt)
	{
		AAmbientEncounterPoint* Point = *PointIt;

		if (!IsValid(Point) || !Point->IsPointEnabled())
		{
			continue;
		}

		if (!DoesEncounterPointMatchDefinition(Point, Definition))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared2D(
			CurrentWorldState.PlayerLocation,
			Point->GetActorLocation()
		);

		if (DistanceSq < MinDistanceSq || DistanceSq > MaxDistanceSq)
		{
			continue;
		}

		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestPoint = Point;
		}
	}

	if (!IsValid(BestPoint))
	{
		OutReason = FString::Printf(
			TEXT("Rejected: no authored point matched tags and distance %.0f-%.0f cm"),
			MinDistance,
			MaxDistance
		);
		return false;
	}

	OutBestPoint = BestPoint;
	OutDistanceToPoint = FMath::Sqrt(BestDistanceSq);
	OutSpawnTransform = BestPoint->GetEncounterSpawnTransform();

	OutReason = FString::Printf(
		TEXT("AuthoredPoint accepted | Point=%s Distance=%.0f cm"),
		*BestPoint->GetPointName().ToString(),
		OutDistanceToPoint
	);

	return true;
}

float AAmbientDirector::GetAutomaticEQSSpawnHeightOffset(const FAmbientEncounterDefinition& Definition) const
{
	if (!Definition.EncounterClass)
	{
		return 0.0f;
	}

	const ACharacter* CharacterDefaultObject = Cast<ACharacter>(Definition.EncounterClass->GetDefaultObject());

	if (!IsValid(CharacterDefaultObject))
	{
		return 0.0f;
	}

	const UCapsuleComponent* Capsule = CharacterDefaultObject->GetCapsuleComponent();

	if (!IsValid(Capsule))
	{
		return 0.0f;
	}

	return Capsule->GetScaledCapsuleHalfHeight();
}

bool AAmbientDirector::FindEQSSpawnTransformForDefinition(
	const FAmbientEncounterDefinition& Definition,
	FTransform& OutSpawnTransform,
	float& OutDistanceToLocation,
	FString& OutReason
) const
{
	OutSpawnTransform = FTransform::Identity;
	OutDistanceToLocation = 0.0f;
	OutReason = TEXT("No EQS query evaluated");

	if (!Definition.LocationQuery)
	{
		OutReason = TEXT("Rejected: no EQS query defined");
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutReason = TEXT("Rejected: no world for EQS Query");
		return false;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!IsValid(PlayerPawn))
	{
		OutReason = TEXT("Rejected: no player pawn for EQS Query");
		return false;
	}

	UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(World);

	if (!QueryManager)
	{
		OutReason = TEXT("Rejected: no EQS QueryManager");
		return false;
	}

	FEnvQueryRequest QueryRequest(Definition.LocationQuery, PlayerPawn);

	EEnvQueryRunMode::Type QueryRunMode = Definition.EQSRunMode.GetValue();

	if (Definition.bValidateEQSLocationWithDirectorRules)
	{
		QueryRunMode = EEnvQueryRunMode::AllMatching;
	}

	const TSharedPtr<FEnvQueryResult> QueryResult =
		QueryManager->RunInstantQuery(
			QueryRequest,
			QueryRunMode
		);

	if (!QueryResult.IsValid())
	{
		OutReason = TEXT("Rejected: EQS returned invalid result object");
		return false;
	}

	if (!QueryResult->IsSuccessful() || QueryResult->Items.Num() == 0)
	{
		OutReason = TEXT("Rejected: EQS query did not return a successful location result");
		return false;
	}

	constexpr int32 MaxCandidateChecks = 8;
	const int32 TotalCandidateCount = QueryResult->Items.Num();
	const int32 CandidateCount = FMath::Min(TotalCandidateCount, MaxCandidateChecks);

	const bool bHasClearanceOverride = Definition.EQSClearanceRadiusOverride > 0.0f;

	const float EffectiveClearanceRadius =
		FMath::Max(1.0f, bHasClearanceOverride ? Definition.EQSClearanceRadiusOverride : CandidateClearanceRadius);

	FString FirstValidationFailure;
	FString LastValidationFailure;

	for (int32 ItemIndex = 0; ItemIndex < CandidateCount; ++ItemIndex)
	{
		const FVector RawEQSLocation = QueryResult->GetItemAsLocation(ItemIndex);
		FVector FinalLocation = RawEQSLocation;

		FString ValidationReason = TEXT("EQS location accepted without Director validation");

		if (Definition.bValidateEQSLocationWithDirectorRules)
		{
			const bool bLocationPassedValidation =
				ValidateEQSLocationCandidate(
					RawEQSLocation,
					PlayerPawn,
					EffectiveClearanceRadius,
					FinalLocation,
					ValidationReason
				);

			if (!bLocationPassedValidation)
			{
				const FString IndexedFailureReason =
					FString::Printf(TEXT("Item %d: %s"), ItemIndex + 1, *ValidationReason);

				if (FirstValidationFailure.IsEmpty())
				{
					FirstValidationFailure = IndexedFailureReason;
				}

				LastValidationFailure = IndexedFailureReason;

				continue;
			}
		}

		FVector ToPlayer = CurrentWorldState.PlayerLocation - FinalLocation;
		ToPlayer.Z = 0.0f;

		const FRotator SpawnRotation =
			ToPlayer.IsNearlyZero()
			? FRotator::ZeroRotator
			: ToPlayer.Rotation();

		const float AutomaticHeightOffset = GetAutomaticEQSSpawnHeightOffset(Definition);
		const FVector ActorSpawnLocation = FinalLocation + FVector::UpVector * AutomaticHeightOffset;

		OutSpawnTransform = FTransform(SpawnRotation, ActorSpawnLocation, FVector::OneVector);
		OutDistanceToLocation = FVector::Dist2D(CurrentWorldState.PlayerLocation, FinalLocation);

		OutReason =
			FString::Printf(
				TEXT(
					"EQS accepted | "
					"Item=%d/%d TotalItems=%d | "
					"Raw=(X=%.0f Y=%.0f Z=%.0f) "
					"Ground=(X=%.0f Y=%.0f Z=%.0f) "
					"Spawn=(X=%.0f Y=%.0f Z=%.0f) "
					"HeightOffset=%.0f "
					"Distance=%.0f cm | %s"
				),
				ItemIndex + 1,
				CandidateCount,
				TotalCandidateCount,

				RawEQSLocation.X,
				RawEQSLocation.Y,
				RawEQSLocation.Z,

				FinalLocation.X,
				FinalLocation.Y,
				FinalLocation.Z,

				ActorSpawnLocation.X,
				ActorSpawnLocation.Y,
				ActorSpawnLocation.Z,

				AutomaticHeightOffset,
				OutDistanceToLocation,
				*ValidationReason
			);

		return true;
	}
	OutReason =
		FString::Printf(
			TEXT(
				"Rejected: all %d checked EQS locations "
				"failed Director validation | "
				"TotalItems=%d | First=%s | Last=%s"
			),
			CandidateCount,
			TotalCandidateCount,
			FirstValidationFailure.IsEmpty()
			? TEXT("None")
			: *FirstValidationFailure,
			LastValidationFailure.IsEmpty()
			? TEXT("None")
			: *LastValidationFailure
		);

	return false;
}

bool AAmbientDirector::ValidateEQSLocationCandidate(
	const FVector& RawLocation,
	const APawn* PlayerPawn,
	float ClearanceRadius,
	FVector& OutValidatedLocation,
	FString& OutReason
) const
{
	OutValidatedLocation = RawLocation;
	OutReason = TEXT("EQS location not validated");

	if (!IsValid(PlayerPawn))
	{
		OutReason = TEXT("No player pawn");
		return false;
	}

	const float SafeClearanceRadius = FMath::Max(1.0f, ClearanceRadius);
	FVector GroundLocation = FVector::ZeroVector;
	FHitResult GroundHit;

	if (!ProjectPointToGround(RawLocation, PlayerPawn, GroundLocation, GroundHit))
	{
		OutReason = TEXT("No ground found under EQS location");
		return false;
	}

	if (GroundHit.ImpactNormal.Z < MinimumGroundNormalZ)
	{
		OutReason = FString::Printf(
			TEXT("Ground too steep under EQS location: NormalZ=%.2f"),
			GroundHit.ImpactNormal.Z
		);
		return false;
	}

	const float Distance2D = FVector::Dist2D(
		CurrentWorldState.PlayerLocation,
		GroundLocation
	);

	if (Distance2D < MinimumSpawnDistance)
	{
		OutReason = FString::Printf(
			TEXT("EQS location too close: Dist=%.0f Min=%.0f"),
			Distance2D,
			MinimumSpawnDistance
		);
		return false;
	}

	if (Distance2D > MaximumSpawnDistance)
	{
		OutReason = FString::Printf(
			TEXT("EQS location too far: Dist=%.0f Max=%.0f"),
			Distance2D,
			MaximumSpawnDistance
		);
		return false;
	}

	FHitResult PathBlockHit;

	if (IsPathToCandidateBlocked(PlayerPawn, GroundLocation, PathBlockHit))
	{
		OutReason = FString::Printf(
			TEXT("Blocked path to EQS location: %s"),
			*GetNameSafe(PathBlockHit.GetActor())
		);
		return false;
	}

	FHitResult AreaBlockHit;

	if (IsCandidateAreaBlocked(PlayerPawn, GroundLocation, SafeClearanceRadius, AreaBlockHit))
	{
		OutReason = FString::Printf(
			TEXT("EQS location area occupied: %s"),
			*GetNameSafe(AreaBlockHit.GetActor())
		);
		return false;
	}

	OutValidatedLocation = GroundLocation;

	OutReason = FString::Printf(
		TEXT("Director validation passed | Grounded=true Dist=%.0f ClearanceRadius=%.0f cm"),
		Distance2D, SafeClearanceRadius);

	return true;
}

bool AAmbientDirector::DoesEncounterPointMatchDefinition(
	const AAmbientEncounterPoint* Point,
	const FAmbientEncounterDefinition& Definition
) const
{
	if (!IsValid(Point))
	{
		return false;
	}

	if (Definition.RequiredRegionTag.IsValid())
	{
		const FGameplayTag PointRegionTag = Point->GetRegionTag();

		if (!PointRegionTag.IsValid())
		{
			return false;
		}

		if (!PointRegionTag.MatchesTagExact(Definition.RequiredRegionTag))
		{
			return false;
		}
	}
	else if (Definition.RequiredRegionName != NAME_None)
	{
		if (Point->GetRegionName() != Definition.RequiredRegionName)
		{
			return false;
		}
	}

	if (!Definition.RequiredPointTags.IsEmpty())
	{
		if (!Point->GetPointTags().HasAllExact(Definition.RequiredPointTags))
		{
			return false;
		}
	}

	return true;
}

bool AAmbientDirector::ProjectPointToGround(
	const FVector& Point, 
	const APawn* PlayerPawn, 
	FVector& OutGroundLocation, 
	FHitResult& OutGroundHit
) const
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

bool AAmbientDirector::IsPathToCandidateBlocked(
	const APawn* PlayerPawn, 
	const FVector& GroundLocation, 
	FHitResult& OutBlockHit
) const
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

bool AAmbientDirector::IsCandidateAreaBlocked(const APawn* PlayerPawn, const FVector& GroundLocation, float ClearanceRadius, FHitResult& OutBlockHit) const
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	const float SafeClearanceRadius = FMath::Max(1.0f, ClearanceRadius);
	constexpr float GroundSeparationPadding = 5.0f;

	const FVector SweepStart = GroundLocation + FVector::UpVector * (SafeClearanceRadius + GroundSeparationPadding);
	const FVector SweepEnd = SweepStart + FVector::UpVector * 1.0f;

	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(SafeClearanceRadius);

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
