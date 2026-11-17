// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "PlatformingCharacterMovementComponent.h"
#include "SavedMove_Platforming.h"
#include "NetworkPredictionData_Client_Platforming.h"
#include "CharacterNetworkMoveData_Platforming.h"
#include "ScaleCalibrationMovementComponent.generated.h"

class AScaleCalibrationCharacter;
class UCurveFloat;

enum class EScaleState : uint8;


class FSavedMove_ScaleCalibration : public FSavedMove_Platforming
{
public:
	// FSavedMove_Character
	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual void PrepMoveFor(ACharacter* Character) override;

	bool bSavedWantsToStartScaleChange = false;
	bool bSavedIsChangingScale = false;
	EScaleState SavedCurrentScaleState{};
	EScaleState SavedPendingScaleState{};
	float SavedScaleChangeElapsedTime = 0.0f;
	float SavedScaleChangeStartScale = 1.0f;
	float SavedCurrentCapsuleScale = 1.0f;
	float SavedTargetCapsuleScale = 1.0f;

};

// Makes Unreal allocate our custom SavedMove
class FNetworkPredictionData_Client_ScaleCalibration : public FNetworkPredictionData_Client_Platforming
{
public:
	FNetworkPredictionData_Client_ScaleCalibration(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};


// Carries custom data from the SavedMove through the network
struct FCharacterNetworkMoveData_ScaleCalibration : public FCharacterNetworkMoveData_Platforming
{
	// FCharacterNetworkMoveData
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;

	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	// Custom network data
	bool bWantsToStartScaleChange = false;
	EScaleState PendingScaleState{};
};

// Provides custom New, Pending and Old network moves
struct FCharacterNetworkMoveDataContainer_ScaleCalibration : public FCharacterNetworkMoveDataContainer
{
	FCharacterNetworkMoveDataContainer_ScaleCalibration();

	FCharacterNetworkMoveData_ScaleCalibration MoveData[3];
};




UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BUILD_FAILED_API UScaleCalibrationMovementComponent : public UPlatformingCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Construction
	UScaleCalibrationMovementComponent();

	// Scaling API
	void RequestScaleChange(EScaleState NewScaleState);
	void RequestReplicatedScaleChange(EScaleState NewScaleState);
	EScaleState GetScaleState() const { return CurrentScaleState; }
	EScaleState GetScaleStateForRespawn() const { return bIsChangingScale ? PendingScaleState : CurrentScaleState; }
	void RestoreScaleState(EScaleState NewScaleState, bool bMaintainBaseLocation = true);

protected:
	// UCharacterMovementComponent
	virtual void BeginPlay() override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	// UPlatformingCharacterMovementComponent
	virtual bool CanStartRoll() const override;

	// Network prediction
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void MoveAutonomous(float ClientTimeStamp,	float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;


private:
	// Scaling
	void TryStartScaleChange(EScaleState NewScaleState);
	bool CanStartScaleChange(EScaleState NewScaleState) const;
	float GetScaleForState(EScaleState State) const;
	void UpdateScaling(float DeltaSeconds);
	void ApplyCapsuleScale(float NewScale, bool bMaintainBaseLocation = true);
	void ApplyVisualScale(float NewScale);
	void MaintainCapsuleBaseLocation(float OldHalfHeight, float NewHalfHeight);
	void FinishScaleChange(float NewScale);

	AScaleCalibrationCharacter* GetScaleCalibrationCharacter() const;

	// Scaling state
	EScaleState CurrentScaleState{};
	EScaleState PendingScaleState{};
	TOptional<EScaleState> AuthoritativeScaleState;

	bool bIsChangingScale = false;
	bool bWantsToStartScaleChange = false;

	float ScaleChangeElapsedTime = 0.0f;
	float ScaleChangeStartScale = 1.0f;
	float CurrentCapsuleScale = 1.0f;
	float TargetCapsuleScale = 1.0f;

	float DefaultMass = 80.0f;
	float BaseCapsuleRadius = 0.0f;
	float BaseCapsuleHalfHeight = 0.0f;
	FVector BaseMeshRelativeScale = FVector::OneVector;
	FVector BaseMeshRelativeLocation = FVector::ZeroVector;

	// Network prediction
	friend class FSavedMove_ScaleCalibration;
	FCharacterNetworkMoveDataContainer_ScaleCalibration ScaleNetworkMoveDataContainer;

	// Scale settings
	UPROPERTY(EditDefaultsOnly, Category="Scaling", meta=(ClampMin="0.001"))
	float SmallScale = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Scaling", meta=(ClampMin="0.001"))
	float LargeScale = 2.0f;
	
	UPROPERTY(EditDefaultsOnly, Category="Scaling", meta=(ClampMin="0.0"))
	float SmallMass = 10.0f;
	
	UPROPERTY(EditDefaultsOnly, Category="Scaling", meta=(ClampMin="0.0"))
	float LargeMass = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category="Scaling", meta=(ClampMin="0.001"))
	float ScaleChangeDuration = 0.2f;
	
	UPROPERTY(EditDefaultsOnly, Category="Scaling")
	TObjectPtr<UCurveFloat> ScaleCurve;
};
