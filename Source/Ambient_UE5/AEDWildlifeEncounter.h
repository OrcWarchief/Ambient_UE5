// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AmbientEncounterRuntimeInterface.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "AEDWildlifeEncounter.generated.h"

class AAIController;
class AAmbientDirector;
class APawn;
class USceneComponent;
class USoundBase;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAEDWildlifeEncounter : public AActor, public IAmbientEncounterRuntimeInterface
{
	GENERATED_BODY()
	
public:
	AAEDWildlifeEncounter();

	virtual void InitializeAmbientEncounter_Implementation(
		const FAmbientEncounterRuntimeContext& Context) override;

	virtual void OnAmbientEncounterWaiting_Implementation() override;
	virtual void OnAmbientEncounterActivated_Implementation() override;
	virtual void OnAmbientEncounterCleanup_Implementation(const FString& Reason) override;
	virtual void OnAmbientEncounterFinished_Implementation(const FString& Reason) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Wildlife")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Members")
	TArray<TSubclassOf<APawn>> WildlifeMemberClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Members",
		meta = (ClampMin = "1", ClampMax = "8"))
	int32 WildlifeMemberCount = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Members")
	TArray<FVector> MemberLocalOffsets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Members",
		meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnHeightPadding = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "100.0", Units = "cm"))
	float FleeDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "100.0", Units = "cm/s"))
	float FleeSpeed = 750.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MemberLateralSpacing = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "-180.0", ClampMax = "180.0", Units = "deg"))
	float FleeDirectionYawOffsetDegrees = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "1.0", ClampMax = "45.0", Units = "deg"))
	float FleeDirectionSearchStepDegrees = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float MaxFleeDirectionSearchAngleDegrees = 90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "0.25", ClampMax = "1.0"))
	float MinimumFleeDistanceScale = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float FleeDistanceScaleStep = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee")
	FVector FleeNavigationProjectionExtent = FVector(250.0f, 250.0f, 300.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MaxFleeDestinationHeightDelta = 225.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Flee",
		meta = (ClampMin = "0.1", Units = "s"))
	float FleeDurationBeforeResolution = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Audio")
	TObjectPtr<USoundBase> StartleSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Debug")
	bool bPrintWildlifeDebug = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Wildlife|Debug")
	bool bDrawFleeDebug = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AED|Wildlife|Runtime")
	TObjectPtr<AAmbientDirector> CachedDirector = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AED|Wildlife|Runtime")
	TArray<TObjectPtr<APawn>> SpawnedWildlifeMembers;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AED|Wildlife|Runtime")
	FAmbientEncounterRuntimeContext RuntimeContext;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AED|Wildlife|Runtime")
	bool bEncounterActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AED|Wildlife|Runtime")
	bool bFleeStarted = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AED|Wildlife|Runtime")
	bool bOutcomeSubmitted = false;

private:
	bool SpawnWildlifeMembers();

	APawn* SpawnWildlifeMember(int32 MemberIndex);

	TSubclassOf<APawn> GetWildlifeMemberClassForIndex(int32 MemberIndex) const;

	FVector GetWildlifeMemberOffset(int32 MemberIndex) const;
	FVector CalculateFleeDirection() const;

	void StartWildlifeFlee();

	AAIController* PrepareWildlifeMemberForFlee(APawn* WildlifeMember) const;

	APawn* GetFleePlanReferenceMember(int32& OutMemberIndex) const;

	bool TryFindReachableFleeDestination(
		APawn* WildlifeMember,
		int32 MemberIndex,
		const FVector& FleeDirection,
		float DistanceScale,
		float LateralScale,
		FVector& OutDestination
	) const;

	bool FindReachableHerdFleePlan(
		const FVector& BaseFleeDirection,
		float SearchDirectionSign,
		float& OutYawOffsetDegrees,
		float& OutDistanceScale
	) const;

	bool IssueFleeMove(
		APawn* WildlifeMember,
		int32 MemberIndex,
		const FVector& BaseFleeDirection,
		float PlannedYawOffsetDegrees,
		float PlannedDistanceScale,
		float SearchDirectionSign
	);

	void ResolveWildlifeFlee();
	void ClearFleeResolutionTimer();
	void DestroyWildlifeMembers();

	void PrintWildlifeDebug(const FString& Message, bool bError) const;

	FTimerHandle FleeResolutionTimerHandle;
};
