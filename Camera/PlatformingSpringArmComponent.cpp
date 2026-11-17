#include "PlatformingSpringArmComponent.h"

#include "Engine/World.h"
#include "Math/RotationMatrix.h"


void UPlatformingSpringArmComponent::SetCollisionOffsetScale(const float InCollisionOffsetScale)
{
	CollisionOffsetScale = FMath::Clamp(InCollisionOffsetScale, 0.0f, 1.0f);
}


void UPlatformingSpringArmComponent::UpdateDesiredArmLocation(const bool bDoTrace, const bool bDoLocationLag,
	const bool bDoRotationLag, const float DeltaTime)
{
	const FVector BaseTargetOffset = TargetOffset;
	const FVector BaseArmOrigin = GetComponentLocation() + BaseTargetOffset;
	const FRotator BaseDesiredRotation = GetTargetRotation();
	FVector BaseDesiredLocation = BaseArmOrigin - BaseDesiredRotation.Vector() * TargetArmLength;
	BaseDesiredLocation += FRotationMatrix(BaseDesiredRotation).TransformVector(SocketOffset);

	ArmCompressionRatio = 1.0f;

	if (bDoTrace && TargetArmLength != 0.0f)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlatformingSpringArm), false, GetOwner());
		FHitResult HitResult;
		GetWorld()->SweepSingleByChannel(HitResult, BaseArmOrigin, BaseDesiredLocation, FQuat::Identity, ProbeChannel,
			FCollisionShape::MakeSphere(ProbeSize), QueryParams);

		if (HitResult.bBlockingHit)
		{
			const float DesiredDistance = FVector::Distance(BaseArmOrigin, BaseDesiredLocation);
			const float ActualDistance = FVector::Distance(BaseArmOrigin, HitResult.Location);

			if (DesiredDistance > KINDA_SMALL_NUMBER)
			{
				ArmCompressionRatio = FMath::Clamp(ActualDistance / DesiredDistance, 0.0f, 1.0f);
			}
		}
	}

	TargetOffset.Z = BaseTargetOffset.Z * CollisionOffsetScale;

	if (bDoLocationLag)
	{
		const float CurrentArmOriginZ = (GetComponentLocation() + TargetOffset).Z;
		PreviousDesiredLoc.Z = CurrentArmOriginZ;
		PreviousArmOrigin.Z = CurrentArmOriginZ;
	}

	Super::UpdateDesiredArmLocation(bDoTrace, bDoLocationLag, bDoRotationLag, DeltaTime);

	TargetOffset = BaseTargetOffset;
}


FVector UPlatformingSpringArmComponent::BlendLocations(const FVector& DesiredArmLocation,
	const FVector& TraceHitLocation, const bool bHitSomething, const float DeltaTime)
{
	const FVector DesiredArmVector = DesiredArmLocation - PreviousArmOrigin;
	const float DesiredArmLength = DesiredArmVector.Size();
	const float TargetCollisionArmLength = bHitSomething
		? FVector::Distance(PreviousArmOrigin, TraceHitLocation)
		: DesiredArmLength;

	if (!bIsArmLengthInitialized)
	{
		InterpolatedArmLength = DesiredArmLength;
		bIsArmLengthInitialized = true;
	}

	InterpolatedArmLength = FMath::FInterpTo(InterpolatedArmLength, TargetCollisionArmLength, DeltaTime,
		CollisionArmInterpSpeed);
	InterpolatedArmLength = FMath::Clamp(InterpolatedArmLength, 0.0f, DesiredArmLength);

	return PreviousArmOrigin + DesiredArmVector.GetSafeNormal() * InterpolatedArmLength;
}