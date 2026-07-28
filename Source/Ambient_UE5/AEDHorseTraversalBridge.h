// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AEDHorseTraversalBridge.generated.h"

class AAmbientDirector;
class APawn;
class APlayerController;
class USceneComponent;

UCLASS(BlueprintType, Blueprintable)
class AMBIENT_UE5_API AAEDHorseTraversalBridge : public AActor
{
	GENERATED_BODY()
	
public:	
	AAEDHorseTraversalBridge();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Horse Traversal")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AED|Horse Traversal")
	TObjectPtr<AAmbientDirector> Director = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "AED|Horse Traversal")
	TObjectPtr<APlayerController> ObservedPlayerController = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AED|Horse Traversal|Debug")
	bool bPrintBridgeDebug = true;

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void SyncTraversalFromPawn(APawn* ObservedPawn);

	void PrintBridgeDebug(const APawn* ObservedPawn, bool bMounted) const;
};
