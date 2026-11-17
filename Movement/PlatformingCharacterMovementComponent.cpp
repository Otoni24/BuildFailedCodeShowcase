#include "PlatformingCharacterMovementComponent.h"

#include "PlatformingCharacter.h"
#include "PlatformingPlayerState.h"
#include "GameFramework/Character.h"
#include "NetworkPredictionData_Client_Platforming.h"

#include "EngineUtils.h"


UPlatformingCharacterMovementComponent::UPlatformingCharacterMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetNetworkMoveDataContainer(NetworkMoveDataContainer);
}


void UPlatformingCharacterMovementComponent::RequestDirectionalAction()
{
	if (CharacterOwner && CharacterOwner->IsMoveInputIgnored())
	{
		return;
	}
	bWantsToStartDirectionalAction = true;
}

void UPlatformingCharacterMovementComponent::ClearPendingDirectionalActionRequest()
{
	bWantsToStartDirectionalAction = false;
}


void UPlatformingCharacterMovementComponent::RequestFinishDirectionalAction(EDirectionalAction Action, bool bCompleted)
{
	if (Action == EDirectionalAction::None || ActiveDirectionalAction != Action)
	{
		return;
	}

	if (PendingFinishAction == Action)
	{
		bPendingFinishCompleted = bPendingFinishCompleted && bCompleted;
		return;
	}

	PendingFinishAction = Action;
	bPendingFinishCompleted = bCompleted;
}


void UPlatformingCharacterMovementComponent::RequestCancelDirectionalAction()
{
	const EDirectionalAction ActionToCancel = ActiveDirectionalAction;

	if (ActionToCancel == EDirectionalAction::None)
	{
		return;
	}

	PendingFinishAction = EDirectionalAction::None;
	bPendingFinishCompleted = true;

	FinishDirectionalAction(ActionToCancel, false);

	if (APlatformingCharacter* Character = Cast<APlatformingCharacter>(GetCharacterOwner()))
	{
		Character->StopDirectionalActionMontage(ActionToCancel);
	}
}


void UPlatformingCharacterMovementComponent::SetRollSprintInputHeld(bool bHeld)
{
	bRollSprintInputHeld = bHeld;

	if (!bRollSprintInputHeld)
	{
		bSprintUnlockedForCurrentHold = false;
	}
}


float UPlatformingCharacterMovementComponent::GetMaxSpeed() const
{
	if (IsSprinting())
	{
		return SprintSpeed;
	}

	return Super::GetMaxSpeed();
}


void UPlatformingCharacterMovementComponent::RequestBufferedJump()
{
	if (CharacterOwner && CharacterOwner->IsMoveInputIgnored())
	{
		return;
	}
	bWantsBufferedJump = true;
}

void UPlatformingCharacterMovementComponent::ClearPendingJumpRequest()
{
	bWantsBufferedJump = false;
	JumpBufferRemaining = 0.0f;
}


bool UPlatformingCharacterMovementComponent::CanAttemptJump() const
{
	const APlatformingCharacter* Player = Cast<APlatformingCharacter>(CharacterOwner);
	if (Player && !Player->IsTutorialControlAvailable(ETutorialControl::Jump))
	{
		return false;
	}
	return CharacterOwner && !CharacterOwner->IsMoveInputIgnored() && Super::CanAttemptJump();
}


bool UPlatformingCharacterMovementComponent::CanUseCoyoteJump() const
{
	return CoyoteTimeRemaining > 0.0f && IsFalling();
}


void UPlatformingCharacterMovementComponent::ConsumeCoyoteJump()
{
	CoyoteTimeRemaining = 0.0f;
}


void UPlatformingCharacterMovementComponent::RequestJumpCut()
{
	if (CharacterOwner && CharacterOwner->IsMoveInputIgnored())
	{
		return;
	}
	if (bIsDoubleJumping)
	{
		return;
	}

	bWantsToApplyJumpCut = true;
}


void UPlatformingCharacterMovementComponent::NotifyJumpStarted(const bool bIsSecondJump)
{
	bIsJumping = true;
	bIsDoubleJumping = bIsSecondJump;
	bWantsToApplyJumpCut = false;

	if (bIsDoubleJumping)
	{
		Velocity.Z = SecondJumpZVelocity;
	}

	bSprintUnlockedForCurrentHold = false;

	ClampHorizontalVelocityToWalkSpeed();
}


float UPlatformingCharacterMovementComponent::GetGravityZ() const
{
	const float BaseGravityZ = Super::GetGravityZ();

	if (!IsFalling() || FMath::IsNearlyZero(BaseGravityZ))
	{
		return BaseGravityZ;
	}

	if (bIsDoubleJumping)
	{
		return BaseGravityZ * SecondJumpGravityMultiplier;
	}

	const float DistanceFromApex = FMath::Abs(Velocity.Z);
	const float RawAlpha = FMath::Clamp(DistanceFromApex / ApexVelocityRange, 0.0f, 1.0f);
	const float BlendAlpha = FMath::SmoothStep(0.0f, 1.0f, RawAlpha);

	const float TargetMultiplier = Velocity.Z >= 0.0f
		? RisingGravityMultiplier
		: FallingGravityMultiplier;

	const float GravityMultiplier = FMath::Lerp(ApexGravityMultiplier, TargetMultiplier, BlendAlpha);

	return BaseGravityZ * GravityMultiplier;
}


FVector UPlatformingCharacterMovementComponent::ScaleInputAcceleration(const FVector& InputAcceleration) const
{
	const APlatformingCharacter* Character = Cast<APlatformingCharacter>(GetCharacterOwner());
	if (Character && Character->bRespawnLocked)
	{
		InputTargetSpeedMultiplier = 0.0f;
		return FVector::ZeroVector;
	}

	const FVector ClampedInput = InputAcceleration.GetClampedToMaxSize(1.0f);
	InputTargetSpeedMultiplier = ClampedInput.Size();

	if (InputTargetSpeedMultiplier <= UE_KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	return ClampedInput.GetSafeNormal() * GetMaxAcceleration();
}


float UPlatformingCharacterMovementComponent::ComputeAnalogInputModifier() const
{
	return FMath::Clamp(InputTargetSpeedMultiplier, 0.0f, 1.0f);
}


FRotator UPlatformingCharacterMovementComponent::ComputeOrientToMovementRotation(
	const FRotator& CurrentRotation,
	float DeltaTime,
	FRotator& DeltaRotation
) const
{
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);

	if (HorizontalVelocity.SizeSquared() < FMath::Square(MinFacingSpeed))
	{
		return CurrentRotation;
	}

	return HorizontalVelocity.Rotation();
}


void UPlatformingCharacterMovementComponent::UpdateCharacterStateBeforeMovement(const float DeltaSeconds)
{
	if (const APlatformingCharacter* Player = Cast<APlatformingCharacter>(CharacterOwner))
	{
		if (!Player->IsTutorialControlAvailable(ETutorialControl::Movement))
		{
			Acceleration = FVector::ZeroVector;
			Velocity.X = 0.0f;
			Velocity.Y = 0.0f;
			RequestCancelDirectionalAction();
			ClearPendingDirectionalActionRequest();
			ClearPendingJumpRequest();
			SetRollSprintInputHeld(false);
		}
		if (!Player->IsTutorialControlAvailable(ETutorialControl::Jump))
		{
			ClearPendingJumpRequest();
		}
	}
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	if (ActiveDirectionalAction == EDirectionalAction::Dash)
	{
		UpdateDash(DeltaSeconds);
	}

	if (PendingFinishAction != EDirectionalAction::None)
	{
		const EDirectionalAction FinishedAction = PendingFinishAction;
		const bool bCompleted = bPendingFinishCompleted;

		PendingFinishAction = EDirectionalAction::None;
		bPendingFinishCompleted = true;

		FinishDirectionalAction(FinishedAction, bCompleted);
	}

	if (bWantsToStartDirectionalAction)
	{
		bWantsToStartDirectionalAction = false;
		TryStartDirectionalAction();
	}

	if (bWantsToApplyJumpCut)
	{
		ApplyJumpCut();
	}

	if (bWantsBufferedJump)
	{
		bWantsBufferedJump = false;
		JumpBufferRemaining = JumpBufferDuration;
	}

	UpdateCoyoteTime(DeltaSeconds);

	UpdateJumpBuffer(DeltaSeconds);

	UpdateSprintCancellation(DeltaSeconds);
}


void UPlatformingCharacterMovementComponent::OnMovementModeChanged(const EMovementMode PreviousMovementMode,
	const uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	const bool bJustLanded = PreviousMovementMode == MOVE_Falling && IsMovingOnGround();

	if (bJustLanded)
	{
		DashAvailability = EDashAvailability::Available;
		CoyoteTimeRemaining = 0.0f;
		bIsJumping = false;
		bIsDoubleJumping = false;
		bWantsToApplyJumpCut = false;

		if (JumpBufferRemaining > 0.0f && CharacterOwner && CharacterOwner->CanJump())
		{
			JumpBufferRemaining = 0.0f;
			CharacterOwner->Jump();
		}

		return;
	}

	const bool bWasMovingOnGround = PreviousMovementMode == MOVE_Walking || PreviousMovementMode == MOVE_NavWalking;
	const bool bJustStartedFalling = bWasMovingOnGround && MovementMode == MOVE_Falling;

	if (!bJustStartedFalling)
	{
		return;
	}

	if (Velocity.Z <= 0.0f)
	{
		CoyoteTimeRemaining = CoyoteTimeDuration;
	}
}

void UPlatformingCharacterMovementComponent::SetRollRootMotionScale(const float NewScale)
{
	RollRootMotionScale = NewScale;
}


void UPlatformingCharacterMovementComponent::TryStartDirectionalAction()
{
	if (ActiveDirectionalAction != EDirectionalAction::None)
	{
		return;
	}

	const FVector ActionDirection = GetDirectionalActionDirection();

	if (ActionDirection.IsNearlyZero())
	{
		return;
	}

	if (CanStartRoll())
	{
		StartRoll(ActionDirection);
	}
	else if (CanStartDash())
	{
		StartDash(ActionDirection);
	}
}


void UPlatformingCharacterMovementComponent::StartDash(const FVector& DashDirection)
{
	APlatformingCharacter* PlatformingCharacter = Cast<APlatformingCharacter>(CharacterOwner);

	if (!PlatformingCharacter)
	{
		return;
	}

	ActiveDirectionalAction = EDirectionalAction::Dash;
	DashAvailability = EDashAvailability::Consumed;

	DashElapsedTime = 0.0f;
	Velocity.Z = 0.0f;
	CacheAndSetGravityScale(0.0f);

	CharacterOwner->StopJumping();
	CharacterOwner->SetActorRotation(FRotator(0.0f, DashDirection.Rotation().Yaw, 0.0f));
	CharacterOwner->LaunchCharacter(DashDirection * DashImpulseStrength, true, true);

	PlatformingCharacter->PlayDashMontage();

	if (PlatformingCharacter->HasAuthority())
	{
		PlatformingCharacter->MulticastPlayDashMontage();
	}
}


void UPlatformingCharacterMovementComponent::StartRoll(const FVector& RollDirection)
{
	APlatformingCharacter* PlatformingCharacter = Cast<APlatformingCharacter>(CharacterOwner);

	if (!PlatformingCharacter)
	{
		return;
	}

	bSprintUnlockedForCurrentHold = false;
	ActiveDirectionalAction = EDirectionalAction::Roll;

	CharacterOwner->StopJumping();
	CharacterOwner->SetActorRotation(FRotator(0.0f, RollDirection.Rotation().Yaw, 0.0f));

	PlatformingCharacter->SetAnimRootMotionTranslationScale(RollRootMotionScale);

	if (!PlatformingCharacter->PlayRollMontage())
	{
		PlatformingCharacter->SetAnimRootMotionTranslationScale(1.0f);
		FinishDirectionalAction(EDirectionalAction::Roll, false);
	}

	if (PlatformingCharacter->HasAuthority())
	{
		PlatformingCharacter->MulticastPlayRollMontage();
	}
}


void UPlatformingCharacterMovementComponent::UpdateDash(const float DeltaSeconds)
{
	DashElapsedTime += DeltaSeconds;

	if (DashElapsedTime >= DashDuration)
	{
		FinishDirectionalAction(EDirectionalAction::Dash, true);
	}
}


void UPlatformingCharacterMovementComponent::FinishDirectionalAction(
	const EDirectionalAction FinishedAction,
	const bool bCompleted)
{
	if (FinishedAction == EDirectionalAction::None || ActiveDirectionalAction != FinishedAction)
	{
		return;
	}

	if (FinishedAction == EDirectionalAction::Dash)
	{
		RestoreCachedGravityScale();
		DashElapsedTime = 0.0f;
	}
	else if (FinishedAction == EDirectionalAction::Roll)
	{
		bSprintUnlockedForCurrentHold = bCompleted && bRollSprintInputHeld;

		if (bSprintUnlockedForCurrentHold && CharacterOwner)
		{
			const FVector SprintDirection = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

			const float ExitSpeed = FMath::Max(Velocity.Size2D(), SprintSpeed);

			Velocity.X = SprintDirection.X * ExitSpeed;
			Velocity.Y = SprintDirection.Y * ExitSpeed;
		}
	}

	ActiveDirectionalAction = EDirectionalAction::None;
}


bool UPlatformingCharacterMovementComponent::CanStartDash() const
{
	const APlatformingCharacter* Player = Cast<APlatformingCharacter>(CharacterOwner);
	if (Player && !Player->IsTutorialControlAvailable(ETutorialControl::Dash))
	{
		return false;
	}
	return CharacterOwner && !CharacterOwner->IsMoveInputIgnored() && IsFalling() &&
		ActiveDirectionalAction == EDirectionalAction::None &&
		DashAvailability == EDashAvailability::Available;
}


bool UPlatformingCharacterMovementComponent::CanStartRoll() const
{
	const APlatformingCharacter* Player = Cast<APlatformingCharacter>(CharacterOwner);
	if (Player && !Player->IsTutorialControlAvailable(ETutorialControl::Roll))
	{
		return false;
	}
	return CharacterOwner && !CharacterOwner->IsMoveInputIgnored() &&
		IsMovingOnGround() && ActiveDirectionalAction == EDirectionalAction::None;
}


FVector UPlatformingCharacterMovementComponent::GetDirectionalActionDirection() const
{
	if (!CharacterOwner)
	{
		return FVector::ZeroVector;
	}

	FVector ActionDirection = Acceleration.GetSafeNormal2D();

	if (ActionDirection.IsNearlyZero())
	{
		ActionDirection = CharacterOwner->GetLastMovementInputVector().GetSafeNormal2D();
	}

	if (ActionDirection.IsNearlyZero())
	{
		ActionDirection = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	}

	return ActionDirection;
}


void UPlatformingCharacterMovementComponent::CacheAndSetGravityScale(float NewGravityScale)
{
	CachedGravityScale = GravityScale;
	GravityScale = NewGravityScale;
}


void UPlatformingCharacterMovementComponent::RestoreCachedGravityScale()
{
	GravityScale = CachedGravityScale;
}


bool UPlatformingCharacterMovementComponent::IsSprinting() const
{
	const APlatformingCharacter* Player = Cast<APlatformingCharacter>(CharacterOwner);
	if (Player && !Player->IsTutorialControlAvailable(ETutorialControl::Sprint))
	{
		return false;
	}
	return
		bRollSprintInputHeld &&
		bSprintUnlockedForCurrentHold &&
		IsMovingOnGround() &&
		ActiveDirectionalAction == EDirectionalAction::None;
}


void UPlatformingCharacterMovementComponent::UpdateSprintCancellation(float DeltaSeconds)
{
	if (!bSprintUnlockedForCurrentHold || ActiveDirectionalAction != EDirectionalAction::None || !IsMovingOnGround())
	{
		TimeBelowSprintCancelSpeed = 0.0f;
		return;
	}

	const bool bBelowCancelSpeed = Velocity.SizeSquared2D() < FMath::Square(SprintCancelSpeed);

	if (!bBelowCancelSpeed)
	{
		TimeBelowSprintCancelSpeed = 0.0f;
		return;
	}

	TimeBelowSprintCancelSpeed += DeltaSeconds;

	if (TimeBelowSprintCancelSpeed >= SprintCancelGraceTime)
	{
		bSprintUnlockedForCurrentHold = false;
		TimeBelowSprintCancelSpeed = 0.0f;
	}
}


void UPlatformingCharacterMovementComponent::ClampHorizontalVelocityToWalkSpeed()
{
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
	const FVector ClampedHorizontalVelocity = HorizontalVelocity.GetClampedToMaxSize(MaxWalkSpeed);

	Velocity.X = ClampedHorizontalVelocity.X;
	Velocity.Y = ClampedHorizontalVelocity.Y;
}


void UPlatformingCharacterMovementComponent::UpdateCoyoteTime(const float DeltaSeconds)
{
	if (CoyoteTimeRemaining == 0.0f) return;

	CoyoteTimeRemaining = FMath::Max(0.0f, CoyoteTimeRemaining - DeltaSeconds);
}


void UPlatformingCharacterMovementComponent::UpdateJumpBuffer(const float DeltaSeconds)
{
	if (JumpBufferRemaining <= 0.0f)
	{
		return;
	}

	JumpBufferRemaining = FMath::Max(0.0f, JumpBufferRemaining - DeltaSeconds);
}


void UPlatformingCharacterMovementComponent::ApplyJumpCut()
{
	bWantsToApplyJumpCut = false;

	if (IsFalling() && Velocity.Z > 0.0f)
	{
		Velocity.Z *= JumpCutMultiplier;
	}
}

// UPlatformingCharacterMovementComponent - Network Prediction

FNetworkPredictionData_Client* UPlatformingCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UPlatformingCharacterMovementComponent* MutableThis = const_cast<UPlatformingCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Platforming(*this);
	}

	return ClientPredictionData;
}

void UPlatformingCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToStartDirectionalAction = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToApplyJumpCut = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bWantsBufferedJump = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
	SetRollSprintInputHeld((Flags & FSavedMove_Character::FLAG_Custom_3) != 0);
}


void UPlatformingCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	FCharacterNetworkMoveData* CurrentMoveData = GetCurrentNetworkMoveData();
	if (CurrentMoveData)
	{
		FCharacterNetworkMoveData_Platforming* PlatformingMoveData = static_cast<FCharacterNetworkMoveData_Platforming*>(CurrentMoveData);
		InputTargetSpeedMultiplier = PlatformingMoveData->InputTargetSpeedMultiplier;
	}

	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

void UPlatformingCharacterMovementComponent::UpdateBasedMovement(float DeltaSeconds) {}

void UPlatformingCharacterMovementComponent::ApplyAccumulatedForces(float DeltaSecond)
{
	FExternalForceContext Context;

	Context.Character = CharacterOwner;
	if (!IsValid(UpdatedComponent)) return;
	Context.Location = UpdatedComponent->GetComponentLocation();
	Context.Mass = Mass;
	Context.DeltaTime = DeltaSecond;
	Context.GravityZ = GetGravityZ();

	FVector ExternalForce = FVector::ZeroVector;

	OnCollectExternalForces.Broadcast(Context, ExternalForce);

	if (!ExternalForce.IsNearlyZero())
	{
		AddForce(ExternalForce);
	}

	Super::ApplyAccumulatedForces(DeltaSecond);
}
