// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "AEDTravelAheadEnvQueryContext.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class AMBIENT_UE5_API UAEDTravelAheadEnvQueryContext : public UEnvQueryContext
{
	GENERATED_BODY()
	
public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|EQS|Travel Ahead",
		meta = (ClampMin = "0.0", Units = "cm"))
	float BaseLeadDistance = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|EQS|Travel Ahead",
		meta = (ClampMin = "0.0", Units = "s"))
	float LookAheadSeconds = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|EQS|Travel Ahead",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumLeadDistance = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|EQS|Travel Ahead",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumLeadDistance = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|EQS|Travel Ahead",
		meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinimumSpeedForVelocityDirection = 100.0f;
};
