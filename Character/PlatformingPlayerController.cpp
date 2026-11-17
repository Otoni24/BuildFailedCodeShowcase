#include "PlatformingPlayerController.h"
#include "BannerCoordinatorInterface.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "DialogueCoordinatorInterface.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameModeBase.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "PauseCoordinatorInterface.h"
#include "PlatformingCharacter.h"
#include "PlatformingPlayerState.h"





APlatformingPlayerController::APlatformingPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}



void APlatformingPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController())
	{
		return;
	}
	
	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	SetInputMode(InputMode);

	SetIgnoreLookInput(false);
}



void APlatformingPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);

	if (!EnhancedInputComponent)
	{
		return;
	}

	EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Started, this, &APlatformingPlayerController::OnPausePressed);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	if (!Subsystem)
	{
		return;
	}

	for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
	{
		if (IsValid(CurrentContext))
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}
	}
}



void APlatformingPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	if (IsLocalController())
	{
		NotifyControlledCharacterChanged(InPawn);
	}
}



void APlatformingPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	
	NotifyControlledCharacterChanged(P);
}



void APlatformingPlayerController::OnUnPossess()
{
	Super::OnUnPossess();

	NotifyControlledCharacterChanged(nullptr);
}



void APlatformingPlayerController::NotifyControlledCharacterChanged(APawn* NewPawn)
{
	APlatformingCharacter* NewCharacter = Cast<APlatformingCharacter>(NewPawn);

	if (LastNotifiedCharacter.Get() == NewCharacter)
	{
		return;
	}

	LastNotifiedCharacter = NewCharacter;
	OnControlledCharacterChanged(NewCharacter);
}



void APlatformingPlayerController::OnPausePressed()
{
	const bool bIsPaused = UGameplayStatics::IsGamePaused(this);

	if (bIsPaused)
	{
		ServerConfirmResume();
		return;
	}

	Pause();
}



void APlatformingPlayerController::RequestResume()
{
	ServerConfirmResume();
}



void APlatformingPlayerController::ServerConfirmResume_Implementation()
{

	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	
	if (!GameMode)
	{
		return;
	}

	IPauseCoordinatorInterface* PauseCoordinator = Cast<IPauseCoordinatorInterface>(GameMode);

	if (!PauseCoordinator)
	{
		return;
	}

	// Notifies which Player requested the resume
	PauseCoordinator->ConfirmPlayerResume(this);
}



void APlatformingPlayerController::RequestExit()
{
	ServerRequestExit();
}



void APlatformingPlayerController::ServerRequestExit_Implementation()
{
	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();

	if (!GameMode)
	{
		return;
	}

	IPauseCoordinatorInterface* PauseCoordinator = Cast<IPauseCoordinatorInterface>(GameMode);

	if (!PauseCoordinator)
	{
		return;
	}

	PauseCoordinator->ConfirmPlayerExit(this);
}



void APlatformingPlayerController::ClientConfirmExit_Implementation()
{
	OnExitConfirmed();
}


void APlatformingPlayerController::RequestDialogueReady(FName DialogueId)
{
	if (DialogueId.IsNone())
	{
		return;
	}

	ServerConfirmDialogueReady(DialogueId);
}

void APlatformingPlayerController::RequestDialogueStarted(FName DialogueId)
{
	if (IsLocalController() && !DialogueId.IsNone())
	{
		ServerNotifyDialogueStarted(DialogueId);
	}
}

void APlatformingPlayerController::ServerNotifyDialogueStarted_Implementation(FName DialogueId)
{
	if (IDialogueCoordinatorInterface* Coordinator = Cast<IDialogueCoordinatorInterface>(GetWorld()->GetGameState()))
	{
		Coordinator->NotifyDialogueStarted(this, DialogueId);
	}
}

void APlatformingPlayerController::SetDialogueMovementLock(FName SequenceId, bool bLocked)
{
	if (!HasAuthority() || SequenceId.IsNone())
	{
		return;
	}
	if (bLocked)
	{
		DialogueMovementLocks.Add(SequenceId);
	}
	else
	{
		DialogueMovementLocks.Remove(SequenceId);
	}
	if (APlatformingPlayerState* PS = GetPlayerState<APlatformingPlayerState>())
	{
		const bool bDesired = !DialogueMovementLocks.IsEmpty();
		if (PS->bDialogueMovementLocked != bDesired)
		{
			PS->bDialogueMovementLocked = bDesired;
			PS->ForceNetUpdate();
		}
	}
}

void APlatformingPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	const APlatformingPlayerState* PS = GetPlayerState<APlatformingPlayerState>();
	const bool bDesired = PS && PS->bDialogueMovementLocked;
	if (bDesired != bAppliedDialogueMovementLock)
	{
		SetIgnoreMoveInput(bDesired);
		bAppliedDialogueMovementLock = bDesired;
	}
	else if (bDesired && !IsMoveInputIgnored())
	{
		// Other input modes/possession must not release an active dialogue lock.
		SetIgnoreMoveInput(true);
	}
}


void APlatformingPlayerController::ServerConfirmDialogueReady_Implementation(FName DialogueId)
{
	if (DialogueId.IsNone())
	{
		return;
	}

	AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!IsValid(GameState))
	{
		return;
	}

	IDialogueCoordinatorInterface* DialogueCoordinator = Cast<IDialogueCoordinatorInterface>(GameState);
	if (!DialogueCoordinator)
	{
		return;
	}

	DialogueCoordinator->ConfirmDialogueReady(this, DialogueId);
}

// Reliable controller RPCs preserve shown-before-input order.
void APlatformingPlayerController::RequestBannerShown(FName BannerId, bool bSharedBanner)
{
	if (!IsLocalController() || BannerId.IsNone())
	{
		return;
	}
	ServerRegisterBannerShown(BannerId, bSharedBanner);
}

void APlatformingPlayerController::RequestBannerInputReady(FName BannerId)
{
	if (!IsLocalController() || BannerId.IsNone())
	{
		return;
	}
	ServerConfirmBannerInput(BannerId);
}

void APlatformingPlayerController::ServerRegisterBannerShown_Implementation(FName BannerId, bool bSharedBanner)
{
	if (BannerId.IsNone() || !GetWorld())
	{
		return;
	}
	AGameStateBase* State = GetWorld()->GetGameState();
	if (!IsValid(State))
	{
		return;
	}
	if (IBannerCoordinatorInterface* Coordinator = Cast<IBannerCoordinatorInterface>(State))
	{
		Coordinator->RegisterBannerShown(this, BannerId, bSharedBanner);
	}
}

void APlatformingPlayerController::ServerConfirmBannerInput_Implementation(FName BannerId)
{
	if (BannerId.IsNone() || !GetWorld())
	{
		return;
	}
	AGameStateBase* State = GetWorld()->GetGameState();
	if (!IsValid(State))
	{
		return;
	}
	if (IBannerCoordinatorInterface* Coordinator = Cast<IBannerCoordinatorInterface>(State))
	{
		Coordinator->ConfirmBannerInput(this, BannerId);
	}
}
