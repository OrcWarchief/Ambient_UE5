// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AmbientEncounterDefinitionTypes.h"
#include "AmbientEncounterDefinitionData.generated.h"

UCLASS(BlueprintType)
class AMBIENT_UE5_API UAmbientEncounterDefinitionData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Encounter Definition")
	FAmbientEncounterDefinition Definition;
};
