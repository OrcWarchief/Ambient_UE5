#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "AmbientEncounterDefinitionTypes.generated.h"

class AActor;
class UEnvQuery;

UENUM(BlueprintType)
enum class EAmbientEncounterLocationSource : uint8
{
	AuthoredPoint UMETA(DisplayName = "Authored Point"),
	EnvironmentQuery UMETA(DisplayName = "Environment Query")
};

USTRUCT(BlueprintType)
struct FAmbientEncounterDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition")
	FName EncounterId = TEXT("Encounter.Prototype.Showroom");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition")
	FText DisplayName = FText::FromString(TEXT("Prototype Showroom Encounter"));

	// Legacy Region 이름
	// 이전 버전 에셋 호환용 fallback 값
	// 마이그레이션 중 기존 에셋이 깨지지 않도록 유지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Legacy")
	FName RequiredRegionName = TEXT("Region.Showroom");

	// Tag 기반 Required Region
	// RequiredRegionTag가 지정되어 있으면 RequiredRegionName보다 우선 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Tags", meta = (Categories = "Region"))
	FGameplayTag RequiredRegionTag;

	// 이 Encounter가 어떤 성격인지 설명하는 태그들
	// 예: Showroom, Tutorial, Combat, Prototype 등
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Tags", meta = (Categories = "Encounter"))
	FGameplayTagContainer EncounterTags;

	// Director의 현재 WorldState에 반드시 있어야 하는 태그들
	// 조건을 만족하지 않으면 이 Encounter는 생성 대상에서 제외
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Tags")
	FGameplayTagContainer RequiredWorldTags;

	// 배치된 Encounter Point에 반드시 있어야 하는 태그들
	// 특정 Point 유형에서만 이 Encounter가 선택되도록 제한
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Tags", meta = (Categories = "Point"))
	FGameplayTagContainer RequiredPointTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Location")
	EAmbientEncounterLocationSource LocationSource = EAmbientEncounterLocationSource::AuthoredPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Location", 
		meta = (EditCondition = "LocationSource == EAmbientEncounterLocationSource::EnvironmentQuery", EditConditionHides))
	TObjectPtr<UEnvQuery> LocationQuery = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Location", 
		meta = (EditCondition = "LocationSource == EAmbientEncounterLocationSource::EnvironmentQuery", EditConditionHides))
	TEnumAsByte<EEnvQueryRunMode::Type> EQSRunMode = EEnvQueryRunMode::SingleResult;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Location", 
		meta = (EditCondition = "LocationSource == EAmbientEncounterLocationSource::EnvironmentQuery", EditConditionHides))
	bool bValidateEQSLocationWithDirectorRules = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Selection", meta = (ClampMin = "0.0"))
	float BaseSelectionScore = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Selection", meta = (ClampMin = "0.0"))
	float DistanceScoreWeight = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Selection", meta = (ClampMin = "0.0"))
	float RecentlyCompletedPenalty = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition|Selection")
	bool bOneShotPerHistory = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "cm"))
	float EncounterPointSearchRadius = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxPlayerSpeed = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "cm"))
	float PlayerEngageDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "cm"))
	float PlayerLeaveDistance = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "cm"))
	float WaitingAbandonDistance = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "s"))
	float CleanupDelaySeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownDurationSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition")
	TSubclassOf<AActor> EncounterClass;
};