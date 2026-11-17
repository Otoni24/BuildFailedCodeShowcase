#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlatformingCameraComponent.generated.h"


class APlatformingCharacter;
class APawn;
class USpringArmComponent;


UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class PLAYER_API UPlatformingCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Construction
	UPlatformingCameraComponent();

	// Input
	void HandleLookInput(float Yaw, float Pitch);
	
	// Safe before or after BeginPlay; an invalid target falls back to the owner at BeginPlay.
	void SetTargetCharacter(APlatformingCharacter* Character);
	// Initialize once with a boom belonging to this component's owner.
	void SetCameraBoom(USpringArmComponent* Boom);

protected:
	// UActorComponent
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Automatic rotation
	void UpdateAutomaticRotation(float DeltaTime);

	// Vertical tracking
	void UpdateVerticalPosition(float DeltaTime);

	// Springarm offset
	void UpdateCollisionOffset(float DeltaTime);

	// Automatic rotation state
	float TimeSinceLastLookInput = 0.0f;
	bool bIsAutoRotationActive = true;

	// Vertical tracking state
	bool bResetVerticalTracking = true;
	FVector DefaultTargetOffset = FVector::ZeroVector;
	FVector DefaultSocketOffset = FVector::ZeroVector;

	float SmoothedVerticalTargetZ = 0.0f;
	bool bFollowVerticalTargetInAir = false;

	// Component references
	TWeakObjectPtr<APlatformingCharacter> TargetCharacter;
	TWeakObjectPtr<APawn> OwnerPawn;
	TWeakObjectPtr<USpringArmComponent> CameraBoom;

	// Automatic rotation settings
	UPROPERTY(EditDefaultsOnly, Category="Platforming|Camera|Auto Rotation")
	bool bAutoRotationEnabled = true;

	UPROPERTY(EditDefaultsOnly, Category="Platforming|Camera|Auto Rotation", meta=(ClampMin="0.0", Units="s"))
	float AutoRotationDelay = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Platforming|Camera|Auto Rotation", meta=(ClampMin="0.0", Units="deg/s"))
	float AutoRotationSpeed = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category="Platforming|Camera|Auto Rotation", meta=(ClampMin="0.0"))
	float LookInputDeadZone = 0.01f;

	UPROPERTY(EditDefaultsOnly, Category="Platforming|Camera|Auto Rotation", meta=(ClampMin="0.0", ClampMax="180.0", Units="deg"))
	float AutoRotationAngleThreshold = 20.0f;

	// Vertical tracking settings
	UPROPERTY(EditDefaultsOnly, Category="Platforming|Camera|Vertical Tracking", meta=(ClampMin="0.0", Units="cm"))
	float MaxVerticalDistance = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category="Platforming|Camera|Vertical Tracking", meta=(ClampMin="0.0"))
	float GroundVerticalInterpSpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="Platforming|Camera|Vertical Tracking", meta=(ClampMin="0.0"))
	float AirVerticalUpwardInterpSpeed = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="Platforming|Camera|Vertical Tracking", meta=(ClampMin="0.0"))
	float AirVerticalDownwardInterpSpeed = 5.0f;

	// Springarm offset settings
	UPROPERTY(EditAnywhere, Category="Camera|Collision")
	TObjectPtr<UCurveFloat> CollisionOffsetCurve;

	UPROPERTY(EditAnywhere, Category="Camera|Collision")
	float CollisionOffsetCollapseSpeed = 12.0f;

	UPROPERTY(EditAnywhere, Category="Camera|Collision")
	float CollisionOffsetRestoreSpeed = 5.0f;

	float CurrentCollisionOffsetScale = 1.0f;
};
