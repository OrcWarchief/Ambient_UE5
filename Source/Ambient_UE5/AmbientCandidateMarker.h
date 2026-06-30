// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmbientCandidateMarker.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AMBIENT_UE5_API AAmbientCandidateMarker : public AActor
{
	GENERATED_BODY()
	
public:
	AAmbientCandidateMarker();

	UFUNCTION(BlueprintCallable, Category = "Ambient Candidate Marker")
	void SetMarkerActive(bool bNewActive);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Candidate Marker")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Candidate Marker")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;
};
