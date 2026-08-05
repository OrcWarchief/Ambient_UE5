
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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Wildlife Member|Debug")
	TObjectPtr<UStaticMeshComponent> DebugBodyMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AED|Wildlife Member|Debug")
	TObjectPtr<UStaticMeshComponent> DebugHeadMesh = nullptr;
};
