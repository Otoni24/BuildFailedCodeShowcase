#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "PlatformingPlayerState.h"
#include "PlatformingCharacter.generated.h"


enum class EDirectionalAction : uint8;

struct FInputActionValue;

class UAnimMontage;
class UCameraComponent;
class UInputAction;
class UPlatformingCameraComponent;
class UPlatformingCharacterMovementComponent;
class UPrimitiveComponent;
class USpringArmComponent;
class UBuild_FailedVFXComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDeath);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInitialSpawnReady, APlatformingCharacter*);

UCLASS(Abstract)
class PLAYER_API APlatformingCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Construction
	explicit APlatformingCharacter(const FObjectInitializer& ObjectInitializer);

	// Gameplay API
	UFUNCTION(BlueprintCallable, Category="Events|Player")
	void Respawn();

	// Animation API
	UFUNCTION(BlueprintPure, Category="Movement|Jump")
	bool IsSecondJumpActive() const;

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	bool IsTutorialControlAvailable(ETutorialControl Control) const;
	bool IsDialogueMovementBlocked() const;

	// Component access
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera.Get(); }

	// Events
	UPROPERTY(BlueprintAssignable, Category="Events|Player")
	FOnPlayerDeath OnPlayerDeath;

	// RPC
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDashMontage();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayRollMontage();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Respawn
	FOnInitialSpawnReady OnInitialSpawnReady;
	void StartRespawnInputLock();
	void HandleRespawned();

	// Death
	void HandleDeath();

protected:
	virtual TFunction<void(APawn*)> CaptureRespawnState() const;

	virtual void BeginPlay() override;

	// ACharacter
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;
	virtual void OnJumped_Implementation() override;
	virtual bool CanJumpWhileFalling() const override;
	void ProcessJumpPressed();
	void ProcessJumpReleased();
	void ProcessDirectionalActionPressed();
	void ProcessDirectionalActionReleased();

	// Respawn
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Respawn")
	bool bRespawnLocked = false;

	void EndRespawnInputLock();

	FTimerHandle RespawnInputLockTimerHandle;

	// Client RPC Respawn
	UFUNCTION(Client, Reliable)
	void ClientSetRespawnInputLock(bool bLocked);

	// Initial Spawn Ready
	UFUNCTION(Server, Reliable)
	void ServerNotifyInitialSpawnReady();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	void TryNotifyInitialSpawnReady();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	TObjectPtr<UBuild_FailedVFXComponent> VFXComponent;

private:
	friend class UPlatformingCharacterMovementComponent;

	bool bRespawnStarted = false;

	// Input
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);
	void HandleDirectionalActionStartInput();
	void HandleDirectionalActionReleaseInput();
	void HandleJumpStartInput();
	void HandleJumpEndInput();

	// Movement input helpers
	void ApplyMoveInput(float Right, float Forward);
	void ApplyLookInput(float Yaw, float Pitch);

	// Movement helpers
	void CancelDirectionalAction();
	UPlatformingCharacterMovementComponent* GetPlatformingMovement() const;

	// Directional action presentation
	void PlayDashMontage();
	bool PlayRollMontage();
	void PlayRollMontageCosmetic();
	void StopDirectionalActionMontage(EDirectionalAction Action);
	void HandleRollMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// Respawn
	void FinishRespawn();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastHandleDeathVisuals();

	// Collision
	UFUNCTION()
	void HandleDamageHitboxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPlatformingCameraComponent> PlatformingCameraComponent;

	// Input assets
	UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
	TObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
	TObjectPtr<UInputAction> RollDashAction;

	// Animation assets
	UPROPERTY(EditDefaultsOnly, Category="Movement|Dash")
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditDefaultsOnly, Category="Movement|Roll")
	TObjectPtr<UAnimMontage> RollMontage;

	// Animation delegates
	FOnMontageEnded OnRollMontageEnded;

	// Initial Spawn
	bool bInitialSpawnReadySent = false;
	FTimerHandle RespawnTimerHandle;

	bool bIsRespawning = false;

	UPROPERTY(EditDefaultsOnly, Category = "Respawn")
	float RespawnDelay = 1.0f;
};
