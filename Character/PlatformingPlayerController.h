#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlatformingPlayerController.generated.h"


class APlatformingCharacter;
class UInputMappingContext;
class UInputAction;


UCLASS(Abstract)
class PLAYER_API APlatformingPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Construction
	APlatformingPlayerController();

	// Pause Menu request
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void RequestResume();

	UFUNCTION(BlueprintCallable, Category = "Pause")
	void RequestExit();

	// Called by Server after Exit state has been registered 
	UFUNCTION(Client, Reliable)
	void ClientConfirmExit();

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void RequestDialogueReady(FName DialogueId);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void RequestDialogueStarted(FName DialogueId);

	// Authority owns the tokens; owning clients receive the resulting state on PlayerState.
	void SetDialogueMovementLock(FName SequenceId, bool bLocked);
	virtual void PlayerTick(float DeltaTime) override;

	// Banner input acknowledgements; called only by the owning client.
	UFUNCTION(BlueprintCallable, Category = "Banner")
	void RequestBannerShown(FName BannerId, bool bSharedBanner = true);

	UFUNCTION(BlueprintCallable, Category = "Banner")
	void RequestBannerInputReady(FName BannerId);

protected:
	UFUNCTION(Server, Reliable)
	void ServerRegisterBannerShown(FName BannerId, bool bSharedBanner);

	UFUNCTION(Server, Reliable)
	void ServerConfirmBannerInput(FName BannerId);

	// APlayerController
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// Blueprint extension points
	UFUNCTION(BlueprintImplementableEvent, Category="Events|Player",
		meta=(DisplayName="On Controlled Character Changed"))
	void OnControlledCharacterChanged(APlatformingCharacter* NewCharacter);

	// Global Pause System 
	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	TObjectPtr<UInputAction> PauseAction;

	void OnPausePressed();

	UFUNCTION(Server, Reliable)
	void ServerConfirmResume();

	UFUNCTION(Server, Reliable)
	void ServerRequestExit();

	UFUNCTION(BlueprintImplementableEvent, Category = "Pause")
	void OnExitConfirmed();

	UFUNCTION(Server, Reliable)
	void ServerConfirmDialogueReady(FName DialogueId);
	UFUNCTION(Server, Reliable)
	void ServerNotifyDialogueStarted(FName DialogueId);


private:
	// Controlled character
	void NotifyControlledCharacterChanged(APawn* NewPawn);

	TWeakObjectPtr<APlatformingCharacter> LastNotifiedCharacter;
	TSet<FName> DialogueMovementLocks;
	bool bAppliedDialogueMovementLock = false;

	// Input assets
	UPROPERTY(EditDefaultsOnly, Category="Input|Mappings")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;
};
