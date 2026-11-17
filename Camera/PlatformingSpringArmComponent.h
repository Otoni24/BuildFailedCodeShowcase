#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "PlatformingSpringArmComponent.generated.h"

UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class PLAYER_API UPlatformingSpringArmComponent : public USpringArmComponent
{
	GENERATED_BODY()

public:
	FORCEINLINE float GetArmCompressionRatio() const { return ArmCompressionRatio; }
	void SetCollisionOffsetScale(float InCollisionOffsetScale);

protected:
	virtual void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime) override;
	virtual FVector BlendLocations(const FVector& DesiredArmLocation, const FVector& TraceHitLocation,
		bool bHitSomething, float DeltaTime) override;

private:
	UPROPERTY(EditDefaultsOnly, Category="Camera|Collision", meta=(ClampMin="0.0"))
	float CollisionArmInterpSpeed = 10.0f;

	float ArmCompressionRatio = 1.0f;
	float CollisionOffsetScale = 1.0f;
	float InterpolatedArmLength = 0.0f;
	bool bIsArmLengthInitialized = false;
};
