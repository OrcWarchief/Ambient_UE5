#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AmbientDirectorTypes.generated.h"


UENUM(BlueprintType)
enum class EAmbientEncounterRuntimeState : uint8
{
	Waiting UMETA(DisplayName = "Waiting"),
	Active UMETA(DisplayName = "Active"),
	Cleanup UMETA(DisplayName = "Cleanup"),
	Cooldown UMETA(DisplayName = "Cooldown")
};

UENUM(BlueprintType)
enum class EAmbientDirectorDebugVisualizationMode : uint8
{
	Off UMETA(DisplayName = "Off"),
	Placement UMETA(DisplayName = "Placement"),
	Selection UMETA(DisplayName = "Selection"),
	Runtime UMETA(DisplayName = "Runtime"),
	Full UMETA(DisplayName = "Full")
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
	FVector EncounterLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	FString LocationSource = TEXT("Unknown");

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	float StartedAtTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	float FinishedAtTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|History")
	FString FinishReason = TEXT("None");
};

UENUM(BlueprintType)
enum class EAmbientTraversalState : uint8
{
	OnFoot UMETA(DisplayName = "On Foot"),
	Mounted UMETA(DisplayName = "Mounted")
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
	bool bHasSelectedEncounterLocation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FVector SelectedEncounterLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FRotator SelectedEncounterRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FString SelectedEncounterLocationSource = TEXT("None");

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FString SelectedEncounterLocationReason = TEXT("No encounter location selected");

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

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	bool bPacingAllowsNewEncounter = true;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	FString PacingBlockReason = TEXT("Pacing not evaluated");

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	int32 CurrentEncounterBudgetUse = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	int32 MaxEncounterBudget = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float GlobalPacingRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State")
	float NearestRecentEncounterDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State|Traversal")
	EAmbientTraversalState TraversalState = EAmbientTraversalState::OnFoot;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State|Traversal")
	bool bIsMounted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|World State|Traversal")
	TObjectPtr<AActor> TraversalActor = nullptr;
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

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FString LocationSource = TEXT("None");

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FVector SelectedLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Director|Selection")
	FString LocationReason = TEXT("No location evaluated");
};