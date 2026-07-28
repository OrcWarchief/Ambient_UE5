#include "AEDHorseTraversalBridge.h"

#include "AEDMountedPawnInterface.h"
#include "AmbientDirector.h"
#include "AmbientDirectorTypes.h"

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogAEDHorseTraversalBridge, Log, All);

AAEDHorseTraversalBridge::AAEDHorseTraversalBridge()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AAEDHorseTraversalBridge::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsValid(Director))
	{
		UE_LOG(
			LogAEDHorseTraversalBridge,
			Error,
			TEXT(
				"[%s] Director is not assigned. "
				"Select this bridge instance and assign "
				"AD_Director_Main."
			),
			*GetName()
		);

		return;
	}

	ObservedPlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (!IsValid(ObservedPlayerController))
	{
		UE_LOG(
			LogAEDHorseTraversalBridge,
			Error,
			TEXT(
				"[%s] PlayerController at Player Index 0 "
				"was not found."
			),
			*GetName()
		);

		return;
	}

	ObservedPlayerController->OnPossessedPawnChanged.AddDynamic(
		this, 
		&AAEDHorseTraversalBridge::HandlePossessedPawnChanged
	);

	SyncTraversalFromPawn(ObservedPlayerController->GetPawn());
}

void AAEDHorseTraversalBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ObservedPlayerController))
	{
		ObservedPlayerController->OnPossessedPawnChanged.RemoveDynamic(
			this,
			&AAEDHorseTraversalBridge::
			HandlePossessedPawnChanged
		);
	}

	ObservedPlayerController = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AAEDHorseTraversalBridge::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (bPrintBridgeDebug)
	{
		UE_LOG(
			LogAEDHorseTraversalBridge,
			Log,
			TEXT(
				"[AED HORSE BRIDGE] "
				"Possession Changed Old=%s New=%s"
			),
			*GetNameSafe(OldPawn),
			*GetNameSafe(NewPawn)
		);
	}

	if (!IsValid(NewPawn))
	{
		if (bPrintBridgeDebug)
		{
			UE_LOG(
				LogAEDHorseTraversalBridge,
				Log,
				TEXT(
					"[AED HORSE BRIDGE] "
					"NewPawn=None ignored"
				)
			);
		}

		return;
	}

	SyncTraversalFromPawn(NewPawn);
}

void AAEDHorseTraversalBridge::SyncTraversalFromPawn(APawn* ObservedPawn)
{
	if (!IsValid(Director))
	{
		UE_LOG(
			LogAEDHorseTraversalBridge,
			Warning,
			TEXT(
				"[%s] Traversal synchronization failed: "
				"Director is invalid."
			),
			*GetName()
		);

		return;
	}

	if (!IsValid(ObservedPawn))
	{
		return;
	}

	const bool bMounted = ObservedPawn->GetClass()->ImplementsInterface(
		UAEDMountedPawnInterface::StaticClass()
	);

	const EAmbientTraversalState NewTraversalState =
		bMounted
		? EAmbientTraversalState::Mounted
		: EAmbientTraversalState::OnFoot;

	AActor* NewTraversalActor =
		bMounted
		? ObservedPawn
		: nullptr;

	Director->SetTraversalState(NewTraversalState, NewTraversalActor);

	PrintBridgeDebug(ObservedPawn, bMounted);
}

void AAEDHorseTraversalBridge::PrintBridgeDebug(const APawn* ObservedPawn, const bool bMounted) const
{
	if (!bPrintBridgeDebug)
	{
		return;
	}

	const TCHAR* TraversalText =
		bMounted
		? TEXT("Mounted")
		: TEXT("OnFoot");

	const FString Message = FString::Printf(
		TEXT(
			"[AED HORSE BRIDGE] "
			"Pawn=%s Traversal=%s"
		),
		*GetNameSafe(ObservedPawn),
		TraversalText
	);

	UE_LOG(
		LogAEDHorseTraversalBridge,
		Display,
		TEXT("%s"),
		*Message
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Cyan,
			Message
		);
	}
}