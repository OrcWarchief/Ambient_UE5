// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "AmbientDirector.generated.h"

class APawn;
class AAmbientCandidateMarker;
class AAmbientRegionVolume;

USTRUCT(BlueprintType)
struct FAmbientWorldState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bHasPlayerPawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float PlayerSpeed2D = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float GameTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bHasCurrentRegion = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FName CurrentRegionName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	int32 CurrentRegionPriority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bHasCandidateLocation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FVector RawCandidateLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FVector CandidateLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float RequestedCandidateDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float UsedCandidateDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float CandidateDistance2D = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bCandidateProjectedToGround = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bCandidateValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FString CandidateRejectReason = TEXT("No candidate evaluated");
};

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAmbientDirector : public AActor
{
	GENERATED_BODY()
	
public:
	AAmbientDirector();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== Debug Properties =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	float UpdateInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bPrintDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bDrawCandidateDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bDrawRegionDebug = true;

	// ===== Debug Marker =====

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Marker")
	bool bUseVisibleCandidateMarker = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Marker")
	TSubclassOf<AAmbientCandidateMarker> CandidateMarkerClass;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Marker")
	TObjectPtr<AAmbientCandidateMarker> ActiveCandidateMarker = nullptr;

	// ===== Spawn Prototype =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Spawn Prototype", meta = (ClampMin = "0.0", Units = "cm"))
	float CandidateDistance = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Spawn Prototype", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumSpawnDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Spawn Prototype", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumSpawnDistance = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Spawn Prototype", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundTraceUpDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Spawn Prototype", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundTraceDownDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Spawn Prototype", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumGroundNormalZ = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Spawn Prototype", meta = (ClampMin = "10.0", Units = "cm"))
	float CandidateClearanceRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Spawn Prototype", meta = (ClampMin = "0.0", Units = "cm"))
	float ObstructionCheckHeight = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Spawn Prototype", meta = (ClampMin = "10.0", Units = "cm"))
	float CandidateDebugRadius = 50.f;

	// ===== World State =====
	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FAmbientWorldState CurrentWorldState;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	TObjectPtr<AAmbientRegionVolume> CurrentRegion = nullptr;

private:
	UFUNCTION()
	void UpdateWorldState();

	void UpdateCandidateLocation(const APawn* PlayerPawn);

	void UpdateCurrentRegion(const APawn* PlayerPawn);

	bool ProjectPointToGround(
		const FVector& Point,
		const APawn* PlayerPawn,
		FVector& OutGroundLocation,
		FHitResult& OutGroundHit
	) const;

	bool IsPathToCandidateBlocked(
		const APawn* PlayerPawn,
		const FVector& GroundLocation,
		FHitResult& OutBlockHit
	) const;

	bool IsCandidateAreaBlocked(
		const APawn* PlayerPawn,
		const FVector& GroundLocation,
		FHitResult& OutBlockHit
	) const;

	void RejectCandidate(const FString& Reason);

	void AcceptCandidate();

	void UpdateCandidateMarker();

	void DestroyCandidateMarker();

	FTimerHandle WorldStateTimerHandle;

	// ===== Debug Properties =====
	void PrintWorldStateDebug() const;

	void DrawCandidateDebug() const;

	void DrawRegionDebug() const;
};
