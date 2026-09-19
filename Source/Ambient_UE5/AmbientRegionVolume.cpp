// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientRegionVolume.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/UObjectToken.h"
#endif


// Sets default values
AAmbientRegionVolume::AAmbientRegionVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	RegionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RegionBounds"));
	RegionBounds->SetupAttachment(SceneRoot);
	RegionBounds->InitBoxExtent(FVector(1000.0f, 1000.0f, 500.0f));

	// Director가 경계를 폴링합니다. 충돌 이벤트는 사용되지 않습니다.
	RegionBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RegionBounds->SetGenerateOverlapEvents(false);

	RegionTag = FGameplayTag::RequestGameplayTag(TEXT("Region.Showroom"), false);
}

bool AAmbientRegionVolume::ContainsWorldLocation(const FVector& WorldLocation) const
{
	if (!HasUsableBounds())
	{
		return false;
	}

	const FTransform& RegionTransform	= RegionBounds->GetComponentTransform();
	const FVector    LocalLocation		= RegionTransform.InverseTransformPosition(WorldLocation);
	const FVector    LocalExtent		= RegionBounds->GetUnscaledBoxExtent();

	// 역변환에 스케일이 포함되므로 Unscaled Extent와 비교한다.
	return FMath::Abs(LocalLocation.X) <= LocalExtent.X
		&& FMath::Abs(LocalLocation.Y) <= LocalExtent.Y
		&& FMath::Abs(LocalLocation.Z) <= LocalExtent.Z;
}

bool AAmbientRegionVolume::IsPreferredOver(const AAmbientRegionVolume& Other) const
{
	if (Priority != Other.Priority)
	{
		return Priority > Other.Priority;
	}

	// 같은 후보와 경로가 주어졌을 때 순회 순서에 의존하지 않도록 한다.
	return GetPathName().Compare(Other.GetPathName(), ESearchCase::CaseSensitive) < 0;
}

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "AmbientRegionVolume"

void AAmbientRegionVolume::CheckForErrors()
{
	Super::CheckForErrors();

	const UWorld* World = GetWorld();
	if (!World || World->IsGameWorld() || IsTemplate())
	{
		return;
	}

	FMessageLog MapCheck(TEXT("MapCheck"));

	if (!IsValid(RegionBounds.Get()))
	{
		MapCheck.Error(LOCTEXT("MissingBounds", "RegionBounds 컴포넌트가 없습니다."))
			->AddToken(FUObjectToken::Create(this));
		return;
	}

	const FTransform& BoundsTransform = RegionBounds->GetComponentTransform();
	const FVector Extent = RegionBounds->GetUnscaledBoxExtent();

	if (BoundsTransform.ContainsNaN() || !BoundsTransform.IsRotationNormalized() || Extent.ContainsNaN())
	{
		MapCheck.Error(LOCTEXT("InvalidBoundsTransform",
			"RegionBounds의 Transform 또는 Extent에 비정상 값이 있습니다."))
			->AddToken(FUObjectToken::Create(this));
		return;
	}

	if (!HasUsableBounds())
	{
		MapCheck.Error(LOCTEXT("InvalidBoundsSize",
			"Region의 스케일이 0에 가깝거나 Extent가 0 이하입니다. 박스 크기를 확인하세요."))
			->AddToken(FUObjectToken::Create(this));
	}
}

#undef LOCTEXT_NAMESPACE

#endif

bool AAmbientRegionVolume::HasUsableBounds() const
{
	if (!IsValid(RegionBounds.Get()))
	{
		return false;
	}

	const FVector Scale = RegionBounds->GetComponentTransform().GetScale3D();

	// 한 축이라도 스케일이 0에 가까우면 이 볼륨은 영역 검사에서 제외한다.
	if (Scale.GetAbs().GetMin() <= UE_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Extent = RegionBounds->GetUnscaledBoxExtent();
	return Extent.GetMin() > 0.0;
}

