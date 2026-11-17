#include "PlatformingCameraComponent.h"

#include "Curves/CurveFloat.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

#include "PlatformingCharacter.h"
#include "PlatformingSpringArmComponent.h"


UPlatformingCameraComponent::UPlatformingCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UPlatformingCameraComponent::HandleLookInput(const float Yaw, const float Pitch)
{
	APawn* Pawn = OwnerPawn.Get();
	if (!Pawn || !Pawn->IsLocallyControlled() || FVector2D(Yaw, Pitch).IsNearlyZero(LookInputDeadZone))
	{
		return;
	}

	TimeSinceLastLookInput = 0.0f;
	bIsAutoRotationActive = false;

	Pawn->AddControllerYawInput(Yaw);
	Pawn->AddControllerPitchInput(Pitch);
}

void UPlatformingCameraComponent::SetTargetCharacter(APlatformingCharacter* Character)
{
	if (TargetCharacter.Get() == Character)
	{
		return;
	}
	// Camera pawns order movement before their own follow tick instead.
	if (Cast<APlatformingCharacter>(GetOwner()))
	{
		if (APlatformingCharacter* PreviousTarget = TargetCharacter.Get())
		{
			RemoveTickPrerequisiteComponent(PreviousTarget->GetCharacterMovement());
		}
		if (IsValid(Character))
		{
			AddTickPrerequisiteComponent(Character->GetCharacterMovement());
		}
	}
	TargetCharacter = IsValid(Character) ? Character : nullptr;
	// Reset after the camera owner has followed the new target.
	bResetVerticalTracking = true;
}

void UPlatformingCameraComponent::SetCameraBoom(USpringArmComponent* Boom)
{
	if (CameraBoom.IsValid() || !IsValid(Boom) || Boom->GetOwner() != GetOwner())
	{
		return;
	}
	CameraBoom = Boom;
	DefaultTargetOffset = Boom->TargetOffset;
	DefaultSocketOffset = Boom->SocketOffset;
	Boom->AddTickPrerequisiteComponent(this);
	bResetVerticalTracking = true;
}

void UPlatformingCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPawn = Cast<APawn>(GetOwner());

	APlatformingCharacter* PlatformingCharacter = Cast<APlatformingCharacter>(GetOwner());
	if (PlatformingCharacter)
	{
		if (!TargetCharacter.IsValid())
		{
			SetTargetCharacter(PlatformingCharacter);
		}
		if (!CameraBoom.IsValid())
		{
			SetCameraBoom(PlatformingCharacter->GetCameraBoom());
		}
	}
}


void UPlatformingCameraComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerPawn.IsValid() || !OwnerPawn->IsLocallyControlled() ||
		!CameraBoom.IsValid() || !TargetCharacter.IsValid())
	{
		return;
	}
	if (bResetVerticalTracking)
	{
		CameraBoom->SocketOffset = DefaultSocketOffset;
		SmoothedVerticalTargetZ = CameraBoom->GetComponentLocation().Z + DefaultTargetOffset.Z;
		bFollowVerticalTargetInAir = false;
		bResetVerticalTracking = false;
	}

	UpdateCollisionOffset(DeltaTime);
	UpdateVerticalPosition(DeltaTime);

	if (!bAutoRotationEnabled)
	{
		return;
	}

	TimeSinceLastLookInput += DeltaTime;

	if (bIsAutoRotationActive)
	{
		UpdateAutomaticRotation(DeltaTime);
	}
	else if (TimeSinceLastLookInput >= AutoRotationDelay)
	{
		bIsAutoRotationActive = true;
	}
}


void UPlatformingCameraComponent::UpdateAutomaticRotation(const float DeltaTime)
{
	const APlatformingCharacter* Character = TargetCharacter.Get();

	if (!Character)
	{
		return;
	}

	AController* Controller = OwnerPawn.IsValid() ? OwnerPawn->GetController() : nullptr;

	if (!Controller)
	{
		return;
	}

	const FVector HorizontalVelocity = Character->GetVelocity().GetSafeNormal2D();

	if (HorizontalVelocity.IsNearlyZero())
	{
		return;
	}

	const float ControlRotationYaw = Controller->GetControlRotation().Yaw;
	float AxisDeltaAngle = FMath::FindDeltaAngleDegrees(ControlRotationYaw, HorizontalVelocity.Rotation().Yaw);

	if (AxisDeltaAngle > 90.0f)
	{
		AxisDeltaAngle = 180.0f - AxisDeltaAngle;
	}
	else if (AxisDeltaAngle < -90.0f)
	{
		AxisDeltaAngle = -180.0f - AxisDeltaAngle;
	}

	if (FMath::Abs(AxisDeltaAngle) <= AutoRotationAngleThreshold)
	{
		return;
	}

	FRotator ControlRotation = Controller->GetControlRotation();
	ControlRotation.Yaw = FMath::FixedTurn(ControlRotation.Yaw, ControlRotationYaw + AxisDeltaAngle,
		AutoRotationSpeed * DeltaTime);

	Controller->SetControlRotation(ControlRotation);
}


void UPlatformingCameraComponent::UpdateVerticalPosition(const float DeltaTime)
{
	const APlatformingCharacter* Character = TargetCharacter.Get();
	USpringArmComponent* Boom = CameraBoom.Get();

	if (!Character || !Boom)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

	if (!Movement)
	{
		return;
	}

	const float NaturalTargetZ = Boom->GetComponentLocation().Z + DefaultTargetOffset.Z;

	if (Movement->IsMovingOnGround())
	{
		bFollowVerticalTargetInAir = false;
		SmoothedVerticalTargetZ = FMath::FInterpTo(SmoothedVerticalTargetZ, NaturalTargetZ, DeltaTime, GroundVerticalInterpSpeed);
	}
	else
	{
		const float VerticalDistance = FMath::Abs(NaturalTargetZ - SmoothedVerticalTargetZ);

		if (VerticalDistance > MaxVerticalDistance)
		{
			bFollowVerticalTargetInAir = true;
		}

		if (bFollowVerticalTargetInAir)
		{
			const float InterpSpeed = NaturalTargetZ > SmoothedVerticalTargetZ ? AirVerticalUpwardInterpSpeed : AirVerticalDownwardInterpSpeed;
			SmoothedVerticalTargetZ = FMath::FInterpTo(SmoothedVerticalTargetZ, NaturalTargetZ, DeltaTime, InterpSpeed);
		}
	}

	const FVector WorldVerticalCorrection(0.0f, 0.0f, SmoothedVerticalTargetZ - NaturalTargetZ);
	const FVector LocalVerticalCorrection = Boom->GetTargetRotation().Quaternion().UnrotateVector(WorldVerticalCorrection);

	Boom->SocketOffset = DefaultSocketOffset + LocalVerticalCorrection;
}

void UPlatformingCameraComponent::UpdateCollisionOffset(const float DeltaTime)
{
	UPlatformingSpringArmComponent* Boom = Cast<UPlatformingSpringArmComponent>(CameraBoom.Get());

	if (!Boom)
	{
		CurrentCollisionOffsetScale = 1.0f;
		return;
	}

	const float ArmRatio = Boom->GetArmCompressionRatio();
	float TargetOffsetScale = ArmRatio;

	if (CollisionOffsetCurve)
	{
		TargetOffsetScale = CollisionOffsetCurve->GetFloatValue(ArmRatio);
	}

	TargetOffsetScale = FMath::Clamp(TargetOffsetScale, 0.0f, 1.0f);
	const float InterpSpeed = TargetOffsetScale < CurrentCollisionOffsetScale ? CollisionOffsetCollapseSpeed : CollisionOffsetRestoreSpeed;

	CurrentCollisionOffsetScale = FMath::FInterpTo(CurrentCollisionOffsetScale, TargetOffsetScale, DeltaTime, InterpSpeed);
	Boom->SetCollisionOffsetScale(CurrentCollisionOffsetScale);
}
