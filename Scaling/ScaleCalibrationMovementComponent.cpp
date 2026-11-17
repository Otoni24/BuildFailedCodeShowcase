#include "ScaleCalibrationMovementComponent.h"
#include "ScaleCalibrationCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"



UScaleCalibrationMovementComponent::UScaleCalibrationMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetNetworkMoveDataContainer(ScaleNetworkMoveDataContainer);
}

void UScaleCalibrationMovementComponent::RequestScaleChange(const EScaleState NewScaleState)
{
	if (bIsChangingScale || bWantsToStartScaleChange)
	{
		return;
	}
	
	AuthoritativeScaleState.Reset();
	bWantsToStartScaleChange = true;
	PendingScaleState = NewScaleState;
}

void UScaleCalibrationMovementComponent::RequestReplicatedScaleChange(const EScaleState NewScaleState)
{
	RequestScaleChange(NewScaleState);
	AuthoritativeScaleState = NewScaleState;
}

void UScaleCalibrationMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	
	DefaultMass = Mass;
	
	if (!CharacterOwner)
	{
		return;
	}

	if (const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent())
	{
		BaseCapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
		BaseCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	}

	if (const USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh())
	{
		BaseMeshRelativeScale = Mesh->GetRelativeScale3D();
		BaseMeshRelativeLocation = Mesh->GetRelativeLocation();
	}
}

void UScaleCalibrationMovementComponent::UpdateCharacterStateBeforeMovement(const float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	
	if (AuthoritativeScaleState.IsSet() && CharacterOwner && !CharacterOwner->bClientUpdating && !bIsChangingScale)
	{
		PendingScaleState = AuthoritativeScaleState.GetValue();
		bWantsToStartScaleChange = CurrentScaleState != PendingScaleState;
	}

	if (bWantsToStartScaleChange && !bIsChangingScale && !IsRolling())
	{
		bWantsToStartScaleChange = false;
		TryStartScaleChange(PendingScaleState);
	}
	
	if (bIsChangingScale)
	{
		UpdateScaling(DeltaSeconds);
	}
}

bool UScaleCalibrationMovementComponent::CanStartRoll() const
{
	if (bIsChangingScale)
	{
		return false;
	}
	
	return Super::CanStartRoll();
}

void UScaleCalibrationMovementComponent::TryStartScaleChange(const EScaleState NewScaleState)
{
	AScaleCalibrationCharacter* Character = GetScaleCalibrationCharacter();

	if (!Character || !CanStartScaleChange(NewScaleState))
	{
		return;
	}

	TargetCapsuleScale = GetScaleForState(NewScaleState);
	ScaleChangeStartScale = CurrentCapsuleScale;
	ScaleChangeElapsedTime = 0.0f;
	PendingScaleState = NewScaleState;
	bIsChangingScale = true;

	if (Character->HasAuthority())
	{
		Character->SetReplicatedScaleState(NewScaleState);
	}

	Character->StartScaleChange(TargetCapsuleScale > CurrentCapsuleScale);

	Character->HandleScaling();
}

bool UScaleCalibrationMovementComponent::CanStartScaleChange(const EScaleState NewScaleState) const
{
	const APlatformingCharacter* Player = Cast<APlatformingCharacter>(CharacterOwner);
	if (Player && !Player->IsTutorialControlAvailable(ETutorialControl::ScaleSwitch))
	{
		return false;
	}

	return CurrentScaleState != NewScaleState && !IsRolling() && !bIsChangingScale;
}

float UScaleCalibrationMovementComponent::GetScaleForState(const EScaleState State) const
{
	switch (State)
	{
	case EScaleState::Large:
		return LargeScale;

	case EScaleState::Small:
		return SmallScale;

	default:
		return 1.0f;
	}
}

void UScaleCalibrationMovementComponent::UpdateScaling(const float DeltaSeconds)
{
	ScaleChangeElapsedTime += DeltaSeconds;
	
	const float SafeDuration = FMath::Max(ScaleChangeDuration, UE_SMALL_NUMBER);
	const float LinearAlpha = FMath::Clamp(ScaleChangeElapsedTime / SafeDuration, 0.0f, 1.0f);
	
	const float CurveAlpha = ScaleCurve
		? FMath::Clamp(ScaleCurve->GetFloatValue(LinearAlpha), 0.0f, 1.0f)
		: LinearAlpha;
	
	const float NewScale = FMath::Lerp(ScaleChangeStartScale, TargetCapsuleScale, CurveAlpha);
	
	ApplyCapsuleScale(NewScale);
	
	if (LinearAlpha >= 1.0f)
	{
		ApplyCapsuleScale(TargetCapsuleScale);
		FinishScaleChange(TargetCapsuleScale);
	}
}


void UScaleCalibrationMovementComponent::ApplyCapsuleScale(const float NewScale, const bool bMaintainBaseLocation)
{
	if (!CharacterOwner)
	{
		return;
	}

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();

	if (!Capsule)
	{
		return;
	}

	const float OldScaledHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	const float NewRadius = BaseCapsuleRadius * NewScale;
	const float NewHalfHeight = BaseCapsuleHalfHeight * NewScale;

	const float CurrentUnscaledHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float ShapeScale = CurrentUnscaledHalfHeight > UE_SMALL_NUMBER ? OldScaledHalfHeight / CurrentUnscaledHalfHeight : 1.0f;

	const float NewScaledHalfHeight = NewHalfHeight * ShapeScale;

	const bool bIsGrowing = NewScaledHalfHeight > OldScaledHalfHeight;

	if (bIsGrowing && bMaintainBaseLocation)
	{
		MaintainCapsuleBaseLocation(OldScaledHalfHeight, NewScaledHalfHeight);
		Capsule->SetCapsuleSize(NewRadius, NewHalfHeight, true);
	}
	else
	{
		Capsule->SetCapsuleSize(NewRadius, NewHalfHeight, true);
		if (bMaintainBaseLocation)
		{
			MaintainCapsuleBaseLocation(OldScaledHalfHeight, NewScaledHalfHeight);
		}
	}

	ApplyVisualScale(NewScale);

	CurrentCapsuleScale = NewScale;
}


void UScaleCalibrationMovementComponent::ApplyVisualScale(const float NewScale)
{
	if (!CharacterOwner)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();

	if (!Mesh)
	{
		return;
	}

	const FVector NewMeshScale = BaseMeshRelativeScale * NewScale;
	const FVector NewMeshLocation = BaseMeshRelativeLocation * NewScale;

	Mesh->SetRelativeScale3D(NewMeshScale);
	Mesh->SetRelativeLocation(NewMeshLocation);

	if (AScaleCalibrationCharacter* Character = GetScaleCalibrationCharacter())
	{
		Character->SetMeshBaseTranslationOffset(NewMeshLocation);
	}
}


void UScaleCalibrationMovementComponent::MaintainCapsuleBaseLocation(const float OldHalfHeight, const float NewHalfHeight)
{
	if (!UpdatedComponent)
	{
		return;
	}

	const float DeltaHalfHeight = NewHalfHeight - OldHalfHeight;
	
	FHitResult Hit{};
	
	SafeMoveUpdatedComponent(FVector(0.0f, 0.0f, DeltaHalfHeight),
		UpdatedComponent->GetComponentQuat(), true, Hit);
}

void UScaleCalibrationMovementComponent::FinishScaleChange(const float NewScale)
{
	SetRollRootMotionScale(1.0f / NewScale);
	CurrentScaleState = PendingScaleState;

	switch (PendingScaleState)
	{
	case EScaleState::Large:
		Mass = LargeMass;
		break;

	case EScaleState::Small:
		Mass = SmallMass;
		break;

	case EScaleState::Default:
		Mass = DefaultMass;
		break;
	}

	bIsChangingScale = false;

	if (AScaleCalibrationCharacter* Character = GetScaleCalibrationCharacter())
	{
		Character->FinishScaleChange(NewScale);
	}
}

AScaleCalibrationCharacter* UScaleCalibrationMovementComponent::GetScaleCalibrationCharacter() const
{
	return Cast<AScaleCalibrationCharacter>(CharacterOwner);
}

void UScaleCalibrationMovementComponent::RestoreScaleState(const EScaleState NewScaleState, const bool bMaintainBaseLocation)
{
	const float NewScale = GetScaleForState(NewScaleState);

	PendingScaleState = NewScaleState;

	bWantsToStartScaleChange = false;
	bIsChangingScale = false;
	ScaleChangeElapsedTime = 0.0f;

	TargetCapsuleScale = NewScale;
	ScaleChangeStartScale = NewScale;

	if (AScaleCalibrationCharacter* Character = GetScaleCalibrationCharacter())
	{
		if (Character->HasAuthority())
		{
			Character->SetReplicatedScaleState(NewScaleState);
		}
		else
		{
			AuthoritativeScaleState = NewScaleState;
		}
	}

	ApplyCapsuleScale(NewScale, bMaintainBaseLocation);
	FinishScaleChange(NewScale);
}




// Network Prediction

void FSavedMove_ScaleCalibration::Clear()
{
	FSavedMove_Platforming::Clear();

	bSavedWantsToStartScaleChange = false;
	bSavedIsChangingScale = false; 
	SavedCurrentScaleState = EScaleState::Default;
	SavedPendingScaleState = EScaleState::Default;
	SavedScaleChangeElapsedTime = 0.0f;
	SavedScaleChangeStartScale = 1.0f;
	SavedCurrentCapsuleScale = 1.0f;
	SavedTargetCapsuleScale = 1.0f;

}


void FSavedMove_ScaleCalibration::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Platforming::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	UScaleCalibrationMovementComponent* Movement = Cast<UScaleCalibrationMovementComponent>(Character->GetCharacterMovement());
	if (!Movement) return;

	bSavedWantsToStartScaleChange = Movement->bWantsToStartScaleChange;
	bSavedIsChangingScale = Movement->bIsChangingScale;
	SavedCurrentScaleState = Movement->CurrentScaleState;
	SavedPendingScaleState = Movement->PendingScaleState;
	SavedScaleChangeElapsedTime = Movement->ScaleChangeElapsedTime;
	SavedScaleChangeStartScale = Movement->ScaleChangeStartScale ;
	SavedCurrentCapsuleScale = Movement->CurrentCapsuleScale;
	SavedTargetCapsuleScale = Movement->TargetCapsuleScale;
}


uint8 FSavedMove_ScaleCalibration::GetCompressedFlags() const
{
	uint8 Flags = FSavedMove_Platforming::GetCompressedFlags();

	return Flags;
}


bool FSavedMove_ScaleCalibration::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	const FSavedMove_ScaleCalibration* NewScaleMove = static_cast<const FSavedMove_ScaleCalibration*>(NewMove.Get());
	if (!NewScaleMove) return false;

	if (bSavedWantsToStartScaleChange != NewScaleMove->bSavedWantsToStartScaleChange) return false;
	if (bSavedIsChangingScale != NewScaleMove->bSavedIsChangingScale) return false;
	if (SavedCurrentScaleState != NewScaleMove->SavedCurrentScaleState) return false;
	if (SavedPendingScaleState != NewScaleMove->SavedPendingScaleState)	return false;

	return FSavedMove_Platforming::CanCombineWith(NewMove, Character, MaxDelta);
}


void FSavedMove_ScaleCalibration::PrepMoveFor(ACharacter* Character)
{
	FSavedMove_Platforming::PrepMoveFor(Character);

	UScaleCalibrationMovementComponent* Movement = Cast<UScaleCalibrationMovementComponent>(Character->GetCharacterMovement());
	if (!Movement) return;

	Movement->bWantsToStartScaleChange = bSavedWantsToStartScaleChange;
	Movement->bIsChangingScale = bSavedIsChangingScale;
	Movement->CurrentScaleState = SavedCurrentScaleState;
	Movement->PendingScaleState = SavedPendingScaleState;
	Movement->ScaleChangeElapsedTime = SavedScaleChangeElapsedTime;
	Movement->ScaleChangeStartScale = SavedScaleChangeStartScale;
	Movement->ApplyCapsuleScale(SavedCurrentCapsuleScale);
	Movement->TargetCapsuleScale = SavedTargetCapsuleScale;

}


FNetworkPredictionData_Client_ScaleCalibration::FNetworkPredictionData_Client_ScaleCalibration(const UCharacterMovementComponent& ClientMovement) 
	: FNetworkPredictionData_Client_Platforming(ClientMovement)
{
}


FSavedMovePtr FNetworkPredictionData_Client_ScaleCalibration::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_ScaleCalibration());
}


FNetworkPredictionData_Client* UScaleCalibrationMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UScaleCalibrationMovementComponent* MutableThis = const_cast<UScaleCalibrationMovementComponent*>(this);

		MutableThis->ClientPredictionData =	new FNetworkPredictionData_Client_ScaleCalibration(*this);
	}

	return ClientPredictionData;
}


void FCharacterNetworkMoveData_ScaleCalibration::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	FCharacterNetworkMoveData_Platforming::ClientFillNetworkMoveData(ClientMove, MoveType);

	const FSavedMove_ScaleCalibration& ScaleMove = static_cast<const FSavedMove_ScaleCalibration&>(ClientMove);

	bWantsToStartScaleChange = ScaleMove.bSavedWantsToStartScaleChange;
	PendingScaleState = ScaleMove.SavedPendingScaleState;
}


bool FCharacterNetworkMoveData_ScaleCalibration::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	bool bSuccess = FCharacterNetworkMoveData_Platforming::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	Ar << bWantsToStartScaleChange;

	uint8 ScaleStateByte = static_cast<uint8>(PendingScaleState);
	Ar << ScaleStateByte;

	if (Ar.IsLoading())
	{
		PendingScaleState = static_cast<EScaleState>(ScaleStateByte);
	}

	return bSuccess && !Ar.IsError();
}


FCharacterNetworkMoveDataContainer_ScaleCalibration::FCharacterNetworkMoveDataContainer_ScaleCalibration()
{
	NewMoveData = &MoveData[0];
	PendingMoveData = &MoveData[1];
	OldMoveData = &MoveData[2];
}


void UScaleCalibrationMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	FCharacterNetworkMoveData* CurrentMoveData = GetCurrentNetworkMoveData();
	if (CurrentMoveData)
	{
		FCharacterNetworkMoveData_ScaleCalibration* ScaleMoveData = static_cast<FCharacterNetworkMoveData_ScaleCalibration*>(CurrentMoveData);

		if (!bWantsToStartScaleChange && !bIsChangingScale && ScaleMoveData->bWantsToStartScaleChange)
		{
			bWantsToStartScaleChange = true;
			PendingScaleState = ScaleMoveData->PendingScaleState;
		}
	}

	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}
