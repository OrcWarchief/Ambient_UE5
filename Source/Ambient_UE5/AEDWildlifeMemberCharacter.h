
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AEDWildlifeMemberCharacter.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class AMBIENT_UE5_API AAEDWildlifeMemberCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAEDWildlifeMemberCharacter();

	void PrepareForAmbientFlee(float NewFleeSpeed);

	UFUNCTION(BlueprintCallable, Category = "AED|Wildlife Member|Reaction")
	void SetAmbientAlerted(bool bNewAlerted);

	UFUNCTION(BlueprintPure, Category = "AED|Wildlife Member|Reaction")
	bool IsAmbientAlerted() const;

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient,
		Category = "AED|Wildlife Member|Reaction")
	bool bAmbientAlerted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Wildlife Member|Debug")
	TObjectPtr<UStaticMeshComponent> DebugBodyMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Wildlife Member|Debug")
	TObjectPtr<UStaticMeshComponent> DebugHeadMesh = nullptr;
};
