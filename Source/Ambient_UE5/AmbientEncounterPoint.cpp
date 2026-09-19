// Fill out your copyright notice in the Description page of Project Settings.


#include "AmbientEncounterPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/UObjectToken.h"
#endif

AAmbientEncounterPoint::AAmbientEncounterPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FacingArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FacingArrow"));
	FacingArrow->SetupAttachment(SceneRoot);
	FacingArrow->SetRelativeScale3D(FVector(2.0f, 2.0f, 2.0f));

	RegionTag = FGameplayTag::RequestGameplayTag(TEXT("Region.Showroom"), false);
	PointTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Point.Showroom"), false));
}

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "AmbientEncounterPoint"

void AAmbientEncounterPoint::CheckForErrors()
{
	Super::CheckForErrors();

	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld() || IsTemplate())
	{
		return;
	}

	FMessageLog MapCheck(TEXT("MapCheck"));
	const FTransform& PointTransform = GetActorTransform();

	if (PointTransform.ContainsNaN() || !PointTransform.IsRotationNormalized())
	{
		MapCheck.Error(LOCTEXT("InvalidTransform",
			"Point의 Transform에 비정상 값이 있거나 회전이 정규화되어 있지 않습니다."))
			->AddToken(FUObjectToken::Create(this));
		return;
	}

	const FVector Scale = PointTransform.GetScale3D();

	if (Scale.GetAbs().GetMin() <= UE_SMALL_NUMBER)
	{
		MapCheck.Warning(LOCTEXT("NearlyZeroScale",
			"Point의 스케일이 0에 가깝습니다. 스폰되는 Encounter의 크기를 확인하세요."))
			->AddToken(FUObjectToken::Create(this));
	}
	else if (!Scale.Equals(FVector::OneVector))
	{
		MapCheck.Info(LOCTEXT("NonUnitScale",
			"Point의 스케일이 Encounter 스폰에 전달됩니다. 의도한 크기인지 확인하세요."))
			->AddToken(FUObjectToken::Create(this));
	}

	if (!IsValid(FacingArrow.Get()))
	{
		MapCheck.Warning(LOCTEXT("MissingFacingArrow", "FacingArrow 컴포넌트가 없습니다."))
			->AddToken(FUObjectToken::Create(this));
	}
	else if (!FacingArrow->GetForwardVector().Equals(GetActorForwardVector()))
	{
		MapCheck.Warning(LOCTEXT("FacingMismatch",
			"FacingArrow 방향이 Actor 방향과 다릅니다. 스폰에는 Actor Transform을 사용합니다."))
			->AddToken(FUObjectToken::Create(this));
	}

	if (PointName.IsNone())
	{
		MapCheck.Warning(LOCTEXT("MissingPointName",
			"PointName이 비어 있어 로그와 이력에서 출처를 구별하기 어렵습니다."))
			->AddToken(FUObjectToken::Create(this));
		return;
	}

	const FString ThisPath = GetPathName();

	for (TActorIterator<AAmbientEncounterPoint> PointIt(World); PointIt; ++PointIt)
	{
		const AAmbientEncounterPoint* Other = *PointIt;
		if (!IsValid(Other) || Other == this || Other->GetPointName() != PointName)
		{
			continue;
		}

		// 같은 쌍의 경고를 양쪽 액터에서 반복하지 않는다.
		if (ThisPath.Compare(Other->GetPathName(), ESearchCase::CaseSensitive) >= 0)
		{
			continue;
		}

		MapCheck.Warning(LOCTEXT("DuplicatePointName",
			"로드된 Point끼리 PointName이 중복됩니다. 로그와 이력의 출처를 확인하세요."))
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FUObjectToken::Create(Other));
		break;
	}
}

#undef LOCTEXT_NAMESPACE

#endif
