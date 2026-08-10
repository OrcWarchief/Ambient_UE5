#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "AEDVistaFinaleTrigger.generated.h"

class AAmbientDirector;
class ALevelSequenceActor;
class APlayerController;
class UBoxComponent;
class ULevelSequencePlayer;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAEDVistaFinaleTrigger : public AActor
{
	GENERATED_BODY()
	
public:
	AAEDVistaFinaleTrigger();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Vista Finale")
	TObjectPtr<UBoxComponent> TriggerBox = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AED|Vista Finale")
	TObjectPtr<AAmbientDirector> Director = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AED|Vista Finale")
	TObjectPtr<ALevelSequenceActor> VistaSequenceActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AED|Vista Finale")
	bool bRequireMounted = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AED|Vista Finale|Input")
	bool bLockPlayerInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AED|Vista Finale|Input")
	bool bRestoreInputAfterSequence = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AED|Vista Finale",
		meta = (ClampMin = "0.05", Units = "s"))
	float RuntimeCheckIntervalSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AED|Vista Finale|Debug")
	bool bPrintDebug = true;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Vista Finale|Runtime")
	bool bPlayerInsideTrigger = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Vista Finale|Runtime")
	bool bFinaleStarted = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Vista Finale|Runtime")
	bool bFinaleCompleted = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Vista Finale|Runtime")
	TObjectPtr<ULevelSequencePlayer> CachedSequencePlayer = nullptr;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AED|Vista Finale|Runtime")
	TObjectPtr<APlayerController> CachedPlayerController = nullptr;

private:
	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UFUNCTION()
	void HandleSequenceFinished();

	void BeginRuntimeChecks();
	void ClearRuntimeChecks();
	void TryStartFinale();
	void StartFinale();
	void SetPlayerInputLocked(bool bLocked);

	bool CanStartFinale(FString& OutReason) const;
	bool IsCurrentPlayerActor(const AActor* Actor) const;

	void PrintDebugMessage(const FString& Message, bool bError) const;

	FTimerHandle RuntimeCheckTimerHandle;
	FString LastStartBlockReason;
	bool bInputLocked = false;
};
