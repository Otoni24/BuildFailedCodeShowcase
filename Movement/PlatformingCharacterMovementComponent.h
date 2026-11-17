#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterNetworkMoveDataContainer_Platforming.h"
#include "PlatformingMovementTypes.h"
#include "ExternalForceContext.h"
#include "PlatformingCharacterMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FCollectExternalForces, const FExternalForceContext&, FVector&);

UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class PLAYER_API UPlatformingCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Construction
	UPlatformingCharacterMovementComponent();

	// Directional action API
	void RequestDirectionalAction();
	void RequestFinishDirectionalAction(EDirectionalAction Action, bool bCompleted = true);
	void RequestCancelDirectionalAction();
	void SetRollSprintInputHeld(bool bHeld);
	void ClearPendingDirectionalActionRequest();

	EDirectionalAction GetActiveDirectionalAction() const { return ActiveDirectionalAction; }
	bool IsDashing() const { return ActiveDirectionalAction == EDirectionalAction::Dash; }
	bool IsRolling() const { return ActiveDirectionalAction == EDirectionalAction::Roll; }
	bool IsDashAvailable() const { return DashAvailability == EDashAvailability::Available; }

	// Sprint API
	UFUNCTION(BlueprintPure, Category = "Platforming|Movement|Sprint")
	float GetMaxSprintSpeed() const { return SprintSpeed; }

	virtual float GetMaxSpeed() const override;

	// Jump API
	void RequestBufferedJump();
	void ClearPendingJumpRequest();
	bool CanUseCoyoteJump() const;
	void ConsumeCoyoteJump();
	void RequestJumpCut();
	void NotifyJumpStarted(bool bIsSecondJump);
	virtual bool CanAttemptJump() const override;

	UFUNCTION(BlueprintPure, Category = "Platforming|Movement|Jump")
	bool IsSecondJumpActive() const { return bIsDoubleJumping; }

	FCollectExternalForces OnCollectExternalForces;

protected:
	// UCharacterMovementComponent
	virtual float GetGravityZ() const override;
	virtual FVector ScaleInputAcceleration(const FVector& InputAcceleration) const override;
	virtual float ComputeAnalogInputModifier() const override;
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	void SetRollRootMotionScale(float NewScale);

	virtual bool CanStartDash() const;
	virtual bool CanStartRoll() const;

	// Network prediction
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;

	virtual void UpdateBasedMovement(float DeltaSeconds) override;

	virtual void ApplyAccumulatedForces(float DeltaSecond) override;
private:

	// Network prediction
	friend class FSavedMove_Platforming;
	FCharacterNetworkMoveDataContainer_Platforming NetworkMoveDataContainer;

	// Directional actions
	void TryStartDirectionalAction();
	void StartDash(const FVector& DashDirection);
	void StartRoll(const FVector& RollDirection);
	void UpdateDash(float DeltaSeconds);
	void FinishDirectionalAction(EDirectionalAction FinishedAction, bool bCompleted = true);

	FVector GetDirectionalActionDirection() const;

	void CacheAndSetGravityScale(float NewGravityScale);
	void RestoreCachedGravityScale();

	// Sprint
	bool IsSprinting() const;
	void UpdateSprintCancellation(float DeltaSeconds);
	void ClampHorizontalVelocityToWalkSpeed();

	// Jump
	void UpdateCoyoteTime(float DeltaSeconds);
	void UpdateJumpBuffer(float DeltaSeconds);
	void ApplyJumpCut();

	// Directional action state
	bool bWantsToStartDirectionalAction = false;
	bool bPendingFinishCompleted = true;

	EDirectionalAction ActiveDirectionalAction = EDirectionalAction::None;
	EDirectionalAction PendingFinishAction = EDirectionalAction::None;
	EDashAvailability DashAvailability = EDashAvailability::Available;

	float DashElapsedTime = 0.0f;
	float CachedGravityScale = 1.0f;

	// Sprint state
	bool bRollSprintInputHeld = false;
	bool bSprintUnlockedForCurrentHold = false;

	float TimeBelowSprintCancelSpeed = 0.0f;

	// Jump state
	bool bWantsToApplyJumpCut = false;
	bool bIsJumping = false;
	bool bIsDoubleJumping = false;
	bool bWantsBufferedJump = false;

	float CoyoteTimeRemaining = 0.0f;
	float JumpBufferRemaining = 0.0f;

	// Analog input state
	mutable float InputTargetSpeedMultiplier = 0.0f;

	// Rotation settings
	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Rotation", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinFacingSpeed = 20.0f;

	// Dash settings
	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Dash", meta = (ClampMin = "0.0", Units = "cm/s"))
	float DashImpulseStrength = 1000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Dash", meta = (ClampMin = "0.0", Units = "s"))
	float DashDuration = 0.2f;

	// Roll settings
	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Roll", meta = (ClampMin = "0.0"))
	float RollRootMotionScale = 1.0f;

	// Sprint settings
	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Sprint", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SprintSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Sprint", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SprintCancelSpeed = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Sprint", meta = (ClampMin = "0.0", Units = "s"))
	float SprintCancelGraceTime = 0.3f;

	// Jump settings
	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Jump", meta = (ClampMin = "0.0"))
	float RisingGravityMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Jump", meta = (ClampMin = "0.0"))
	float ApexGravityMultiplier = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Jump", meta = (ClampMin = "0.0"))
	float FallingGravityMultiplier = 1.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Jump", meta = (ClampMin = "1.0", Units = "cm/s"))
	float ApexVelocityRange = 140.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Jump", meta = (ClampMin = "0.0", Units = "s"))
	float CoyoteTimeDuration = 0.16f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Jump", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JumpCutMultiplier = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Jump", meta = (ClampMin = "0.0", Units = "s"))
	float JumpBufferDuration = 0.12f;

	// Second jump settings
	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Jump|Second Jump", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SecondJumpZVelocity = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Platforming|Movement|Jump|Second Jump", meta = (ClampMin = "0.0"))
	float SecondJumpGravityMultiplier = 1.0f;
};
