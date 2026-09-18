// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "AmbientEncounterPoint.generated.h"

class USceneComponent;
class UArrowComponent;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAmbientEncounterPoint : public AActor
{
	GENERATED_BODY()
	
public:
	AAmbientEncounterPoint();

	bool IsPointEnabled() const { return bEnabled; }

	FName GetPointName() const { return PointName; }

	FName GetRegionName() const { return RegionName; }

	float GetDebugRadius() const { return DebugRadius; }

	// 화살표 컴포넌트의 상대 트랜스폼이 아니라 액터 트랜스폼을 사용.
	FTransform GetEncounterSpawnTransform() const { return GetActorTransform(); }

	FGameplayTag GetRegionTag() const { return RegionTag; }

	const FGameplayTagContainer& GetPointTags() const { return PointTags; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	TObjectPtr<UArrowComponent> FacingArrow;

	// 후보 선택 가능 여부를 제어. 이미 존재하는 Encounter의 수명은 제어하지 않음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	bool bEnabled = true;

	//직접 지정한 라벨. 진단 출력과 Encounter 이력에 사용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	FName PointName = TEXT("EP.Showroom.01");

	// Definition에 유효한 RequiredRegionTag가 없을 때만 사용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point")
	FName RegionName = TEXT("Region.Showroom");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point|Tags", meta = (Categories = "Region"))
	FGameplayTag RegionTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point|Tags", meta = (Categories = "Point"))
	FGameplayTagContainer PointTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Point", meta = (ClampMin = "10.0", Units = "cm"))
	float DebugRadius = 80.0f;
};
