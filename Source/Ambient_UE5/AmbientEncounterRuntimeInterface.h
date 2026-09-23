#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "AmbientEncounterRuntimeInterface.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct FAmbientEncounterRuntimeContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	TObjectPtr<AActor> DirectorActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FName EncounterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FName RegionName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FName SourcePointName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambient Encounter Runtime")
	FGameplayTagContainer EncounterTags;
};

UINTERFACE(BlueprintType)
class AMBIENT_UE5_API UAmbientEncounterRuntimeInterface : public UInterface
{
	GENERATED_BODY()
};

// Director가 Encounter 액터에 보내는 런타임 알림.
//
// Waiting 중 취소되거나 월드가 종료되면 Cleanup과 Finished가 호출되지 않을 수 있다.
// 구현 클래스는 EndPlay에서도 자원을 정리해야 한다.
class AMBIENT_UE5_API IAmbientEncounterRuntimeInterface
{
	GENERATED_BODY()

public:
	// 생성된 Encounter에 실행 정보를 전달한다. Active 진입은 별도로 알린다.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void InitializeAmbientEncounter(const FAmbientEncounterRuntimeContext& Context);

	// 플레이어의 접근을 기다리기 시작할 때 호출한다. 매 갱신마다 호출하지는 않는다.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void OnAmbientEncounterWaiting();

	// Director가 Active로 전환한 뒤 호출한다. 구현 클래스는 여기서 참여 동작을 시작한다.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void OnAmbientEncounterActivated();

	// 정리 구간에 들어갈 때 호출한다. 정리 지연이 0이면 Finished가 바로 이어질 수 있다.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void OnAmbientEncounterCleanup(const FString& Reason);

	// Director가 종료 이력을 기록하고 액터를 제거하기 전에 호출한다.
	// 액터가 바로 제거될 수 있어, 여기서 표시한 내용이 화면에 보인다는 보장은 없다.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ambient Encounter Runtime")
	void OnAmbientEncounterFinished(const FString& Reason);
};
