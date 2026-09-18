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

	// 월드 좌표로 받은 점이 볼륨 안에 있는지 확인, 경계에 있는 점도 내부로 처리한다.
	bool ContainsWorldLocation(const FVector& WorldLocation) const;

	bool IsPreferredOver(const AAmbientRegionVolume& Other) const;

	FName GetRegionName() const { return RegionName; }

	int32 GetPriority() const { return Priority; }

	FLinearColor GetRegionDebugColor() const { return RegionDebugColor; }

	const UBoxComponent* GetRegionBounds() const { return RegionBounds; }

	FGameplayTag GetRegionTag() const { return RegionTag; }

#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	TObjectPtr<UBoxComponent> RegionBounds;

	// 리전을 이름으로 비교하는 기존 코드에서 사용, 런타임 컨텍스트와 디버그 표시에도 이 이름을 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	FName RegionName = TEXT("Region.Showroom");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region", meta = (Categories = "Region"))
	FGameplayTag RegionTag;

	// 플레이어를 포함하는 리전 중 Priority가 가장 높은 것을 선택한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Region")
	FLinearColor RegionDebugColor = FLinearColor(0.1f, 0.5f, 1.0f, 1.0f);

private:
	// Region으로 사용할 수 있는 박스 크기인지 확인한다.
	bool HasUsableBounds() const;
};
