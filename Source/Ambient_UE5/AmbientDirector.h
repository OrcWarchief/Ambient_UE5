// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "AmbientEncounterDefinitionTypes.h"
#include "AmbientDirector.generated.h"

class APawn;
class AAmbientCandidateMarker;
class AAmbientRegionVolume;
class AAmbientEncounterPoint;
class AAmbientPlaceholderEncounter;
class UAmbientEncounterDefinitionData;

UENUM(BlueprintType)
enum class EAmbientEncounterRuntimeState : uint8
{
	Waiting UMETA(DisplayName = "Waiting"),
	Active UMETA(DisplayName = "Active"),
	Cleanup UMETA(DisplayName = "Cleanup"),
	Cooldown UMETA(DisplayName = "Cooldown")
};

USTRUCT(BlueprintType)
struct FAmbientEncounterHistoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	FName EncounterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	FName RegionName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	FName SourcePointName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	float StartedAtTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	float FinishedAtTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	FString FinishReason = TEXT("None");
};

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
	bool bHasCurrentRegionTag = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FGameplayTag CurrentRegionTag;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FGameplayTagContainer WorldTags;

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

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bHasSelectedEncounterPoint = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FName SelectedEncounterPointName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FVector SelectedEncounterPointLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FString SelectedEncounterPointReason = TEXT("No encounter point evaluated");

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bHasSelectedEncounterDefinition = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FName SelectedEncounterDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float SelectedEncounterDefinitionScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FString SelectedEncounterDefinitionReason = TEXT("No encounter definition selected");

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bPrototypeEncounterConditionMet = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bHasActivePrototypeEncounter = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FString PrototypeEncounterBlockReason = TEXT("Prototype condition not evaluated");

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	EAmbientEncounterRuntimeState PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FString PrototypeEncounterRuntimeReason = TEXT("Runtime not evaluated");

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float DistanceToPrototypeEncounter = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float PrototypeCleanupRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float PrototypeCooldownRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	int32 PrototypeEncounterStartCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	int32 PrototypeEncounterFinishCount = 0;
};

USTRUCT(BlueprintType) // debug only data
struct FAmbientEncounterSelectionDebugEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FName EncounterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FName PointName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	float Score = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	float DistanceToPoint = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FString Reason = TEXT("Not evaluated");
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bDrawEncounterPointDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bDrawEncounterRuntimeDebug = true;

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

	// ===== Encounter Definition =====

	// Slice 22 이후 권장 방식
	// Director가 여러 Encounter 정의 에셋을 평가 후 최적의 에셋 선택
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Encounter Definition")
	TArray<TObjectPtr<UAmbientEncounterDefinitionData>> EncounterDefinitionAssets;

	// 기존 단일 Encounter 정의 에셋 fallback
	// 배열이 비어 있을 때 이전 설정 호환용으로 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Encounter Definition")
	TObjectPtr<UAmbientEncounterDefinitionData> PrototypeEncounterDefinitionAsset = nullptr;

	// Data Asset이 없을 때 사용할 인라인 Encounter 정의값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Encounter Definition")
	FAmbientEncounterDefinition PrototypeEncounterDefinition;

	// ===== Encounter Runtime =====

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime", meta = (ClampMin = "1"))
	int32 MaxHistoryEntries = 8;


	// ===== World State =====
	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FAmbientWorldState CurrentWorldState;

	// ===== World State References =====
	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	TObjectPtr<AAmbientRegionVolume> CurrentRegion = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Prototype")
	TObjectPtr<AAmbientEncounterPoint> SelectedEncounterPoint = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Prototype")
	TObjectPtr<AAmbientPlaceholderEncounter> ActivePrototypeEncounter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	TObjectPtr<UAmbientEncounterDefinitionData> SelectedEncounterDefinitionAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FAmbientEncounterDefinition SelectedEncounterDefinition;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	bool bHasSelectedEncounterDefinition = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	float SelectedEncounterScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FString SelectedEncounterReason = TEXT("No encounter selected");

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	TArray<FAmbientEncounterSelectionDebugEntry> LastSelectionDebugEntries;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	FAmbientEncounterDefinition RuntimeEncounterDefinition;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	bool bHasRuntimeEncounterDefinition = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	EAmbientEncounterRuntimeState PrototypeEncounterState = EAmbientEncounterRuntimeState::Waiting;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	TArray<FAmbientEncounterHistoryEntry> PrototypeEncounterHistory;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	int32 PrototypeEncounterStartCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	int32 PrototypeEncounterFinishCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	FName RuntimeEncounterRegionName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	FName RuntimeEncounterPointName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	float RuntimeEncounterStartedAtTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	float PrototypeCleanupEndTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	float PrototypeCooldownEndTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	FString PendingPrototypeFinishReason = TEXT("None");


private:
	UFUNCTION()
	void UpdateWorldState();

	void UpdateCandidateLocation(const APawn* PlayerPawn);

	void UpdateCurrentRegion(const APawn* PlayerPawn);

	void SelectEncounterDefinitionAndPoint();

	bool EvaluateEncounterDefinitionCandidate(
		const FAmbientEncounterDefinition& Definition,
		FAmbientEncounterSelectionDebugEntry& OutDebugEntry,
		AAmbientEncounterPoint*& OutBestPoint
	) const;

	AAmbientEncounterPoint* FindBestEncounterPointForDefinition(
		const FAmbientEncounterDefinition& Definition,
		float& OutDistanceToPoint,
		FString& OutReason
	) const;

	bool DoesEncounterDefinitionMatchCurrentWorld(
		const FAmbientEncounterDefinition& Definition,
		FString& OutReason
	) const;

	bool DoesEncounterPointMatchDefinition(
		const AAmbientEncounterPoint* Point,
		const FAmbientEncounterDefinition& Definition
	) const;

	bool HasRecentlyFinishedEncounter(FName EncounterId) const;

	void EvaluatePrototypeEncounterCondition();

	void UpdatePrototypeEncounter();

	const FAmbientEncounterDefinition& GetPrototypeEncounterDefinition() const;

	bool TrySpawnOrUpdatePrototypeEncounter();

	void StartPrototypeEncounter();

	void BeginPrototypeCleanup(const FString& Reason);

	void FinishPrototypeEncounter(const FString& Reason);

	void StartPrototypeCooldown();

	void DestroyPrototypeEncounter();

	void AddPrototypeHistoryEntry(float FinishedAtTimeSeconds, const FString& FinishReason);

	void SyncPrototypeRuntimeWorldState();

	float GetDistanceFromPlayerToPrototypeEncounter() const;

	FString GetPrototypeRuntimeStateString() const;

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

	void PrintEncounterDebug() const;

	void PrintEncounterHistoryDebug() const;

	void PrintSelectionDebug() const;

	void DrawCandidateDebug() const;

	void DrawRegionDebug() const;

	void DrawEncounterPointDebug() const;

	void DrawEncounterRuntimeDebug() const;

};
