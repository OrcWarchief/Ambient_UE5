// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "AmbientEncounterDefinitionTypes.h"
#include "AmbientDirectorTypes.h"
#include "AmbientDirector.generated.h"

class APawn;
class AAmbientCandidateMarker;
class AAmbientRegionVolume;
class AAmbientEncounterPoint;
class UAmbientEncounterDefinitionData;
struct FAmbientDirectorSaveSnapshot;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAmbientDirector : public AActor
{
	GENERATED_BODY()
	
public:
	AAmbientDirector();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Ambient Director|Save")
	void SaveDirectorStateToSlot();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Ambient Director|Save")
	void LoadDirectorStateFromSlot();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Ambient Director|Save")
	void ClearDirectorSaveSlot();

	// ===== Debug Properties =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	float UpdateInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bPrintDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bPrintCompactDebugDashboard = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bPrintDetailedDebugLines = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	EAmbientDirectorDebugVisualizationMode DebugVisualizationMode =
		EAmbientDirectorDebugVisualizationMode::Full;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bDrawCandidateDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bDrawRegionDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bDrawEncounterPointDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bDrawEncounterRuntimeDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Debug")
	bool bDrawSelectedEncounterLocationDebug = true;

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

	// ===== Encounter Pacing =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Pacing")
	bool bEnableDirectorPacing = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Pacing", meta = (ClampMin = "0"))
	int32 MaxSimultaneousPrototypeEncounters = 1;
	
	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Pacing")
	float LastAnyEncounterStartTimeSeconds = -999999.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Pacing", meta = (ClampMin = "0.0", Units = "s"))
	float MinimumSecondsBetweenEncounterStarts = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Pacing", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumDistanceFromRecentEncounterLocations = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Pacing")
	bool bUseRecentEncounterSpacing = true;


	// ===== World State =====
	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FAmbientWorldState CurrentWorldState;

	// ===== World State References =====
	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	TObjectPtr<AAmbientRegionVolume> CurrentRegion = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Prototype")
	TObjectPtr<AAmbientEncounterPoint> SelectedEncounterPoint = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Prototype")
	TObjectPtr<AActor> ActivePrototypeEncounter = nullptr;

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

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FTransform SelectedEncounterSpawnTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	bool bHasSelectedEncounterSpawnTransform = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FString SelectedEncounterLocationReason = TEXT("No selected encounter spawn transform");

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

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	FVector RuntimeEncounterLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Encounter Runtime")
	FString RuntimeEncounterLocationSource = TEXT("Unknown");

	// ===== Save System =====

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Save")
	bool bAutoLoadDirectorSaveOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Save")
	bool bAutoSaveDirectorStateOnRuntimeChange = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Save")
	bool bPrintSaveDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Save")
	FString DirectorSaveSlotName = TEXT("AmbientDirector_Debug");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Director|Save")
	int32 DirectorSaveUserIndex = 0;

private:
	UFUNCTION()
	void UpdateWorldState();

	void UpdateCandidateLocation(const APawn* PlayerPawn);

	void UpdateCurrentRegion(const APawn* PlayerPawn);

	void SelectEncounterDefinitionAndPoint();

	bool EvaluateEncounterDefinitionCandidate(
		const FAmbientEncounterDefinition& Definition,
		FAmbientEncounterSelectionDebugEntry& OutDebugEntry,
		AAmbientEncounterPoint*& OutBestPoint,
		FTransform& OutSpawnTransform
	);

	bool FindSpawnTransformForDefinition(
		const FAmbientEncounterDefinition& Definition,
		FTransform& OutSpawnTransform,
		AAmbientEncounterPoint*& OutBestPoint,
		float& OutDistanceToLocation,
		FString& OutReason
	) const;

	bool FindAuthoredPointSpawnTransformForDefinition(
		const FAmbientEncounterDefinition& Definition,
		FTransform& OutSpawnTransform,
		AAmbientEncounterPoint*& OutBestPoint,
		float& OutDistanceToPoint,
		FString& OutReason
	) const;

	bool FindEQSSpawnTransformForDefinition(
		const FAmbientEncounterDefinition& Definition,
		FTransform& OutSpawnTransform,
		float& OutDistanceToLocation,
		FString& OutReason
	) const;

	bool ValidateEQSLocationCandidate(
		const FVector& RawLocation,
		const APawn* PlayerPawn,
		FVector& OutValidatedLocation,
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

	bool DoesCandidatePassDirectorPacing(
		const FTransform& CandidateSpawnTransform,
		FString& OutReason,
		float& OutGlobalPacingRemaining,
		float& OutNearestHistoryDistance
	) const;

	int32 GetCurrentEncounterBudgetUse() const;

	float GetGlobalPacingRemaining() const;

	float GetNearestRecentEncounterDistance(const FVector& CandidateLocation) const;

	bool ShouldDrawPlacementDebug() const;

	bool ShouldDrawSelectionDebug() const;

	bool ShouldDrawRuntimeDebug() const;

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

	// ===== Save Game =====

	bool SaveDirectorStateInternal(FString& OutReason) const;

	bool LoadDirectorStateInternal(FString& OutReason);

	bool ClearDirectorSaveInternal(FString& OutReason) const;

	void BuildDirectorSaveSnapshot(
		FAmbientDirectorSaveSnapshot& OutSnapshot
	) const;

	bool ApplyDirectorSaveSnapshot(
		const FAmbientDirectorSaveSnapshot& Snapshot,
		FString& OutReason
	);

	bool RestoreRuntimeEncounterFromSave(
		const FAmbientDirectorSaveSnapshot& Snapshot,
		const FAmbientEncounterDefinition& RestoredDefinition,
		FString& OutReason
	);

	bool TryFindEncounterDefinitionById(
		FName EncounterId,
		FAmbientEncounterDefinition& OutDefinition
	) const;

	void PrintSaveDebugMessage(
		const FString& Message,
		bool bSuccess
	) const;

	// ===== Debug Properties =====
	void PrintWorldStateDebug() const;

	void PrintEncounterDebug() const;

	void PrintEncounterHistoryDebug() const;

	void PrintSelectionDebug() const;

	void PrintDirectorDashboardDebug() const;

	void DrawCandidateDebug() const;

	void DrawRegionDebug() const;

	void DrawEncounterPointDebug() const;

	void DrawEncounterRuntimeDebug() const;

	void DrawSelectedEncounterLocationDebug() const;
};
