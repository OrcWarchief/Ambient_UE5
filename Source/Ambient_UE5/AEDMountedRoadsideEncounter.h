// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AmbientNPCEncounterCharacter.h"
#include "TimerManager.h"
#include "AEDMountedRoadsideEncounter.generated.h"

class AAmbientDirector;
class UAnimMontage;
class UPrimitiveComponent;
class USoundBase;
class USphereComponent;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAEDMountedRoadsideEncounter : public AAmbientNPCEncounterCharacter
{
	GENERATED_BODY()
public:
	AAEDMountedRoadsideEncounter();

	virtual void InitializeAmbientEncounter_Implementation(
		const FAmbientEncounterRuntimeContext& Context
	) override;

	virtual void OnAmbientEncounterWaiting_Implementation() override;

	virtual void OnAmbientEncounterActivated_Implementation() override;

	virtual void OnAmbientEncounterCleanup_Implementation(const FString& Reason) override;

	virtual void OnAmbientEncounterFinished_Implementation(const FString& Reason) override;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Mounted Roadside|Detection")
	TObjectPtr<USphereComponent> StopEvaluationSphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Mounted Roadside|Detection")
	TObjectPtr<USphereComponent> ResolutionSphere = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Detection",
		meta = (ClampMin = "100.0", Units = "cm"))
	float StopEvaluationRadius = 550.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Detection",
		meta = (ClampMin = "200.0", Units = "cm"))
	float ResolutionRadius = 1100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Stop Evaluation",
		meta = (ClampMin = "0.0", Units = "cm/s"))
	float StopSpeedThreshold = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Stop Evaluation",
		meta = (ClampMin = "0.1", Units = "s"))
	float RequiredStoppedDurationSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Stop Evaluation",
		meta = (ClampMin = "0.05", Units = "s"))
	float StopSampleIntervalSeconds = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Presentation")
	FText StoppedResponseText =
		FText::FromString(TEXT("Take it slow. The road narrows beyond the ridge."));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Presentation")
	FText StoppedCleanupText =
		FText::FromString(TEXT("The rider stopped to listen."));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Presentation")
	FText PassedCleanupText =
		FText::FromString(TEXT("The rider passed without stopping."));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Audio")
	TObjectPtr<USoundBase> StoppedResponseSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Animation")
	TObjectPtr<UAnimMontage> WarningGestureMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Animation",
		meta = (ClampMin = "0.1"))
	float WarningGesturePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AED|Mounted Roadside|Debug")
	bool bPrintMountedDebug = true;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Mounted Roadside|Runtime")
	TObjectPtr<AAmbientDirector> CachedDirector = nullptr;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Mounted Roadside|Runtime")
	bool bEncounterActive = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Mounted Roadside|Runtime")
	bool bPlayerInsideStopRange = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Mounted Roadside|Runtime")
	bool bPlayerInsideResolutionRange = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Mounted Roadside|Runtime")
	bool bPlayerEnteredResolutionRange = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Mounted Roadside|Runtime")
	bool bPlayerStoppedToListen = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Mounted Roadside|Runtime")
	bool bOutcomeSubmitted = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Mounted Roadside|Runtime")
	float AccumulatedStoppedSeconds = 0.0f;

private:
	UFUNCTION()
	void HandleStopRangeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleStopRangeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UFUNCTION()
	void HandleResolutionRangeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleResolutionRangeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void StartStopEvaluation();
	void StopStopEvaluation(bool bResetAccumulatedTime);
	void EvaluateStopState();

	bool IsCurrentPlayerActor(const AActor* Actor) const;

	void SubmitOutcomeAndRequestCleanup(const FString& OutcomeReason);
	void PlayWarningGesture();
	void PrintMountedDebug(const FString& Message, bool bError) const;

	FTimerHandle StopEvaluationTimerHandle;
};
