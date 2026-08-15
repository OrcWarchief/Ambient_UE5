
#include "AEDTravelAheadEnvQueryContext.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "GameFramework/Pawn.h"

void UAEDTravelAheadEnvQueryContext::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const UObject* QueryOwner = QueryInstance.Owner.Get();
	const APawn* QueryPawn = Cast<APawn>(QueryOwner);

	if (!IsValid(QueryPawn))
	{
		return;
	}

	FVector HorizontalVelocity = QueryPawn->GetVelocity();
	HorizontalVelocity.Z = 0.0f;

	const float Speed2D = HorizontalVelocity.Size();

	FVector TravelDirection;

	const float SafeDirectionSpeed = FMath::Max(1.0f, MinimumSpeedForVelocityDirection);

	if (Speed2D >= SafeDirectionSpeed)
	{
		TravelDirection = HorizontalVelocity / Speed2D;
	}
	else
	{
		TravelDirection = QueryPawn->GetActorForwardVector().GetSafeNormal2D();
	}

	if (TravelDirection.IsNearlyZero())
	{
		TravelDirection = FVector::ForwardVector;
	}

	const float SafeMinimumLeadDistance = FMath::Max(0.0f, MinimumLeadDistance);
	const float SafeMaximumLeadDistance = FMath::Max(SafeMinimumLeadDistance, MaximumLeadDistance);
	const float RawLeadDistance = FMath::Max(0.0f, BaseLeadDistance) + Speed2D * FMath::Max(0.0f, LookAheadSeconds);
	const float LeadDistance = FMath::Clamp(RawLeadDistance, SafeMinimumLeadDistance, SafeMaximumLeadDistance);

	const FVector TravelAheadLocation = QueryPawn->GetActorLocation() + TravelDirection * LeadDistance;

	UEnvQueryItemType_Point::SetContextHelper(ContextData, TravelAheadLocation);
}
