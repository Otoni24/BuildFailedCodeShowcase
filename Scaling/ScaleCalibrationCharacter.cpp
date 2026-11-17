#include "ScaleCalibrationCharacter.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "ScaleCalibrationGameMode.h"
#include "ScaleCalibrationMovementComponent.h"
#include "VFX/Build_FailedVFXComponent.h"
#include "VFX/BuildFailedVFXGameplayTags.h"

AScaleCalibrationCharacter::AScaleCalibrationCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UScaleCalibrationMovementComponent>(CharacterMovementComponentName))
{
	CeilingCheck = CreateDefaultSubobject<USphereComponent>(TEXT("CeilingCheck"));
	CeilingCheck->SetupAttachment(RootComponent);
	CeilingCheck->OnComponentBeginOverlap.AddDynamic(this, &AScaleCalibrationCharacter::OnCeilingCheckBeginOverlap);
	CeilingCheck->SetGenerateOverlapEvents(true);
	CeilingCheck->SetCollisionResponseToAllChannels(ECR_Ignore);
	CeilingCheck->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
}

void AScaleCalibrationCharacter::HandleScaling()
{
	if (IsValid(VFXComponent))
	{
		VFXComponent->PlayVFX(BuildFailedVFXGameplayTags::VFX_Player_Scaling);
	}
}

EScaleState AScaleCalibrationCharacter::GetCurrentState() const
{
	if (const UScaleCalibrationMovementComponent* Movement = GetScaleCalibrationMovement())
	{
		return Movement->GetScaleState();
	}
	return EScaleState::Default;
}

void AScaleCalibrationCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	CeilingCheck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ApplyInitialReplicatedScale();
}

TFunction<void(APawn*)> AScaleCalibrationCharacter::CaptureRespawnState() const
{
	const UScaleCalibrationMovementComponent* Movement = GetScaleCalibrationMovement();
	if (!Movement)
	{
		return {};
	}

	const EScaleState RespawnScaleState = Movement->GetScaleStateForRespawn();
	return [RespawnScaleState](APawn* NewPawn)
	{
		AScaleCalibrationCharacter* NewCharacter = Cast<AScaleCalibrationCharacter>(NewPawn);
		UScaleCalibrationMovementComponent* NewMovement = NewCharacter ? NewCharacter->GetScaleCalibrationMovement() : nullptr;
		if (!NewMovement || !NewMovement->HasBegunPlay())
		{
			UE_LOG(LogTemp, Warning, TEXT("Respawn scale restore failed: new Pawn has no initialized scale movement component."));
			return;
		}

		NewMovement->RestoreScaleState(RespawnScaleState);
	};
}

void AScaleCalibrationCharacter::PostNetInit()
{
	bInitialReplicationReceived = true;
	Super::PostNetInit();
	ApplyInitialReplicatedScale();
}

void AScaleCalibrationCharacter::ApplyInitialReplicatedScale()
{
	if (HasAuthority() || !bInitialReplicationReceived || bInitialScaleApplied)
	{
		return;
	}

	UScaleCalibrationMovementComponent* Movement = GetScaleCalibrationMovement();
	if (!Movement || !Movement->HasBegunPlay())
	{
		return;
	}

	bInitialScaleApplied = true;
	Movement->RestoreScaleState(ReplicatedScaleState, false);
}

void AScaleCalibrationCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (!EnhancedInputComponent)
	{
		return;
	}

	EnhancedInputComponent->BindAction(ScaleSwitchAction, ETriggerEvent::Started, this,
		&AScaleCalibrationCharacter::HandleScaleSwitchInput);
}

void AScaleCalibrationCharacter::HandleScaleSwitchInput()
{
	if (!IsTutorialControlAvailable(ETutorialControl::ScaleSwitch))
	{
		return;
	}

	RequestScaleChange(EScaleState::Large);

	ServerRequestScaleSwitch();
}

void AScaleCalibrationCharacter::RequestScaleChange(const EScaleState NewScaleState)
{
	if (UScaleCalibrationMovementComponent* Movement = GetScaleCalibrationMovement())
	{
		Movement->RequestScaleChange(NewScaleState);
	}
}

void AScaleCalibrationCharacter::StartScaleChange(const bool bCheckCeiling)
{
	CeilingCheck->SetCollisionEnabled(bCheckCeiling
		? ECollisionEnabled::QueryOnly
		: ECollisionEnabled::NoCollision);
}

void AScaleCalibrationCharacter::FinishScaleChange(const float NewScale)
{
	CeilingCheck->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OnScaleSwitch.Broadcast();
	OnScaleUpdated(NewScale);
}

UScaleCalibrationMovementComponent* AScaleCalibrationCharacter::GetScaleCalibrationMovement() const
{
	return Cast<UScaleCalibrationMovementComponent>(GetCharacterMovement());
}

void AScaleCalibrationCharacter::OnCeilingCheckBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	Respawn();
}

void AScaleCalibrationCharacter::SetMeshBaseTranslationOffset(const FVector& NewOffset)
{
	BaseTranslationOffset = NewOffset;
}

void AScaleCalibrationCharacter::SetReplicatedScaleState(const EScaleState NewScaleState)
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedScaleState = NewScaleState;
}

void AScaleCalibrationCharacter::OnRep_ScaleState()
{
	if (!bInitialScaleApplied && !IsNetStartupActor())
	{
		return;
	}

	if (UScaleCalibrationMovementComponent* Movement = GetScaleCalibrationMovement())
	{
		Movement->RequestReplicatedScaleChange(ReplicatedScaleState);
	}
}


void AScaleCalibrationCharacter::ServerRequestScaleSwitch_Implementation()
{
	if (!IsTutorialControlAvailable(ETutorialControl::ScaleSwitch))
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Controller);

	if (!Controller)
	{
		return;
	}

	AScaleCalibrationGameMode* GameMode = GetWorld()->GetAuthGameMode<AScaleCalibrationGameMode>();

	if (!GameMode)
	{
		return;
	}

	GameMode->HandleScaleSwitchRequest(PlayerController);
}

void AScaleCalibrationCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AScaleCalibrationCharacter, ReplicatedScaleState);
}
