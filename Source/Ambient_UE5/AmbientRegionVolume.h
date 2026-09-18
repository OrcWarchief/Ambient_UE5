// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "AmbientRegionVolume.generated.h"

class USceneComponent;
class UBoxComponent;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAmbientRegionVolume : public AActor
{
	GENERATED_BODY()
	
public:
	AAmbientRegionVolume();

	// 월드 공간의 점을 테스트합니다. 경계 위의 점은 내부에 있는 것으로 간주됩니다.
	bool ContainsWorldLocation(const FVector& WorldLocation) const;

	FName GetRegionName() const { return RegionName; }

	int32 GetPriority() const { return Priority; }

	FLinearColor GetRegionDebugColor() const { return RegionDebugColor; }

	const UBoxComponent* GetRegionBounds() const { return RegionBounds; }

	FGameplayTag GetRegionTag() const { return RegionTag; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	TObjectPtr<UBoxComponent> RegionBounds;

	// Legacy 이름 매칭, 런타임 컨텍스트 및 디버그 레이블.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	FName RegionName = TEXT("Region.Showroom");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region", meta = (Categories = "Region"))
	FGameplayTag RegionTag;

	// 우선 순위가 높은 RegionVolume이 낮은 우선 순위의 RegionVolume보다 먼저 평가됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	FLinearColor RegionDebugColor = FLinearColor(0.1f, 0.5f, 1.0f, 1.0f);
};
