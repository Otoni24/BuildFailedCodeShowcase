#pragma once

#include "CoreMinimal.h"
#include "PlatformingCharacter.h"
#include "ScaleCalibrationCharacter.generated.h"

class UScaleCalibrationMovementComponent;
class USphereComponent;

UENUM(BlueprintType)
enum class EScaleState : uint8
{
	Default,
	Large,
	Small
};


DECLARE_MULTICAST_DELEGATE(FOnScaleSwitch);


UCLASS()
class BUILD_FAILED_API AScaleCalibrationCharacter : public APlatformingCharacter
{
	GENERATED_BODY()

public:
	// Construction
	explicit AScaleCalibrationCharacter(const FObjectInitializer& ObjectInitializer);

	void HandleScaling();

	// Events
	FOnScaleSwitch OnScaleSwitch;
	
	//Getters
	EScaleState GetCurrentState() const;

protected:
	virtual TFunction<void(APawn*)> CaptureRespawnState() const override;
	virtual void PostNetInit() override;

	// ACharacter
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Scaling presentation
	UFUNCTION(BlueprintImplementableEvent, Category="Scaling")
	void OnScaleUpdated(float NewScale);


	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


private:
	friend class UScaleCalibrationMovementComponent;

	void ApplyInitialReplicatedScale();
	bool bInitialReplicationReceived = false;
	bool bInitialScaleApplied = false;

	// Input
	void HandleScaleSwitchInput();

	// Scaling lifecycle
	void RequestScaleChange(EScaleState NewScaleState);
	void StartScaleChange(bool bCheckCeiling);
	void FinishScaleChange(float NewScale);

	UScaleCalibrationMovementComponent* GetScaleCalibrationMovement() const;

	void SetMeshBaseTranslationOffset(const FVector& NewOffset);

	// Collision
	UFUNCTION()
	void OnCeilingCheckBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scaling", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> CeilingCheck;

	// Input assets
	UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
	TObjectPtr<UInputAction> ScaleSwitchAction;

	// Replicated Scale State
	UPROPERTY(ReplicatedUsing = OnRep_ScaleState)
	EScaleState ReplicatedScaleState = EScaleState::Default;

	// RepNotify callback for Scale State changes
	UFUNCTION()
	void OnRep_ScaleState();

	// RPC Scale switch request
	UFUNCTION(Server, Reliable)
	void ServerRequestScaleSwitch();

	void SetReplicatedScaleState(EScaleState NewScaleState);

};
