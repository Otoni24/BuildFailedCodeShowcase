#include "PlatformingCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

#include "PlatformingCameraComponent.h"
#include "PlatformingCharacterMovementComponent.h"
#include "PlatformingSpringArmComponent.h"
#include "PlatformingPlayerState.h"

#include "VFX/Build_FailedVFXComponent.h"
#include "VFX/BuildFailedVFXGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

APlatformingCharacter::APlatformingCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPlatformingCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	OnRollMontageEnded.BindUObject(this, &APlatformingCharacter::HandleRollMontageEnded);

	JumpMaxCount = 2;
	JumpMaxHoldTime = 0.0f;

	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &APlatformingCharacter::HandleDamageHitboxBeginOverlap);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	CameraBoom = CreateDefaultSubobject<UPlatformingSpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	PlatformingCameraComponent = CreateDefaultSubobject<UPlatformingCameraComponent>(TEXT("PlatformingCameraComponent"));

	VFXComponent = CreateDefaultSubobject<UBuild_FailedVFXComponent>(TEXT("VFXComponent"));
}


void APlatformingCharacter::Respawn()
{
	if (!HasAuthority() || bRespawnStarted || IsActorBeingDestroyed())
	{
		return;
	}

	if (!HasAuthority() || bIsRespawning) return;

	AController* PlayerController = GetController();

	if (!IsValid(PlayerController))
	{
		return;
	}

	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode<AGameModeBase>() : nullptr;
	bIsRespawning = true;

	if (!IsValid(GameMode))
	{
		return;
	}

	bRespawnStarted = true;
	OnPlayerDeath.Broadcast();
	
	HandleDeath();

	GetWorldTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&APlatformingCharacter::FinishRespawn,
		RespawnDelay,
		false
	);
}

TFunction<void(APawn*)> APlatformingCharacter::CaptureRespawnState() const
{
	return {};
}


bool APlatformingCharacter::IsSecondJumpActive() const
{
	const UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement();

	return Movement && Movement->IsSecondJumpActive();
}

bool APlatformingCharacter::IsDialogueMovementBlocked() const
{
	const APlatformingPlayerState* PS = GetPlayerState<APlatformingPlayerState>();
	return IsMoveInputIgnored() || (PS && PS->bDialogueMovementLocked);
}

bool APlatformingCharacter::IsTutorialControlAvailable(ETutorialControl Control) const
{
	const APlatformingPlayerState* PS = GetPlayerState<APlatformingPlayerState>();
	return (!PS || PS->IsTutorialControlEnabled(Control))
		&& (Control == ETutorialControl::Camera || !IsDialogueMovementBlocked());
}


void APlatformingCharacter::PlayDashMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (!IsValid(AnimInstance) || !IsValid(DashMontage))
	{
		return;
	}

	VFXComponent->PlayVFX(BuildFailedVFXGameplayTags::VFX_Player_Dash);

	AnimInstance->Montage_Play(DashMontage, 1.0f, EMontagePlayReturnType::MontageLength,
		0.0f, true);
}


bool APlatformingCharacter::PlayRollMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (!IsValid(AnimInstance) || !IsValid(RollMontage))
	{
		return false;
	}

	const float MontageLength = AnimInstance->Montage_Play(RollMontage, 1.0f,
		EMontagePlayReturnType::MontageLength, 0.0f, true);

	if (MontageLength <= 0.0f)
	{
		return false;
	}

	AnimInstance->Montage_SetEndDelegate(OnRollMontageEnded, RollMontage);

	return true;
}


void APlatformingCharacter::PlayRollMontageCosmetic()
{
	if (!RollMontage) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (AnimInstance)
	{
		AnimInstance->Montage_Play(RollMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);
	}
}


void APlatformingCharacter::StopDirectionalActionMontage(const EDirectionalAction Action)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (!IsValid(AnimInstance))
	{
		return;
	}

	UAnimMontage* MontageToStop;

	switch (Action)
	{
	case EDirectionalAction::Dash:
		MontageToStop = DashMontage.Get();
		break;

	case EDirectionalAction::Roll:
		MontageToStop = RollMontage.Get();
		break;

	default:
		return;
	}

	if (IsValid(MontageToStop))
	{
		AnimInstance->Montage_Stop(0.1f, MontageToStop);
	}
}


void APlatformingCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	if (!EnhancedInputComponent)
	{
		return;
	}

	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlatformingCharacter::HandleJumpStartInput);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlatformingCharacter::HandleJumpEndInput);

	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlatformingCharacter::HandleMoveInput);
	EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &APlatformingCharacter::HandleLookInput);
	EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlatformingCharacter::HandleLookInput);

	EnhancedInputComponent->BindAction(RollDashAction, ETriggerEvent::Started, this, &APlatformingCharacter::HandleDirectionalActionStartInput);
	EnhancedInputComponent->BindAction(RollDashAction, ETriggerEvent::Completed, this, &APlatformingCharacter::HandleDirectionalActionReleaseInput);
	EnhancedInputComponent->BindAction(RollDashAction, ETriggerEvent::Canceled, this, &APlatformingCharacter::HandleDirectionalActionReleaseInput);
}


void APlatformingCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	Respawn();
}


void APlatformingCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	CancelDirectionalAction();

	UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement();

	if (!Movement)
	{
		return;
	}

	const bool bIsCoyoteJump = Movement->CanUseCoyoteJump();
	const bool bIsSecondJump = !bIsCoyoteJump && JumpCurrentCount >= 2;

	if (bIsCoyoteJump)
	{
		Movement->ConsumeCoyoteJump();
	}

	Movement->NotifyJumpStarted(bIsSecondJump);
}


bool APlatformingCharacter::CanJumpWhileFalling() const
{
	const UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement();

	return Movement && Movement->CanUseCoyoteJump();
}


void APlatformingCharacter::HandleMoveInput(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	ApplyMoveInput(MovementVector.X, MovementVector.Y);
}


void APlatformingCharacter::HandleLookInput(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	ApplyLookInput(LookAxisVector.X, LookAxisVector.Y);
}


void APlatformingCharacter::HandleDirectionalActionStartInput()
{
	ProcessDirectionalActionPressed();
}


void APlatformingCharacter::ProcessDirectionalActionPressed()
{
	if (bRespawnLocked)
	{
		return;
	}

	if (IsDialogueMovementBlocked())
	{
		return;
	}
	if (UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement())
	{
		Movement->SetRollSprintInputHeld(true);
		Movement->RequestDirectionalAction();
	}
}


void APlatformingCharacter::HandleDirectionalActionReleaseInput()
{
	ProcessDirectionalActionReleased();
}


void APlatformingCharacter::ProcessDirectionalActionReleased()
{
	if (UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement())
	{
		Movement->SetRollSprintInputHeld(false);
	}
}


void APlatformingCharacter::HandleJumpStartInput()
{
	ProcessJumpPressed();
}


void APlatformingCharacter::ProcessJumpPressed()
{
	if (bRespawnLocked)
	{
		return;
	}

	if (!IsTutorialControlAvailable(ETutorialControl::Jump))
	{
		return;
	}
	UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement();

	if (!Movement)
	{
		return;
	}

	if (!CanJump())
	{
		Movement->RequestBufferedJump();
		return;
	}

	Jump();
}


void APlatformingCharacter::HandleJumpEndInput()
{
	ProcessJumpReleased();
}


void APlatformingCharacter::ProcessJumpReleased()
{
	StopJumping();

	if (UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement())
	{
		Movement->RequestJumpCut();
	}
}


void APlatformingCharacter::ApplyMoveInput(const float Right, const float Forward)
{
	if (!IsTutorialControlAvailable(ETutorialControl::Movement))
	{
		return;
	}
	if (bRespawnLocked)
	{
		return;
	}

	const AController* PlayerController = GetController();

	if (!PlayerController)
	{
		return;
	}

	const FRotator ControlRotation = PlayerController->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}


void APlatformingCharacter::ApplyLookInput(const float Yaw, const float Pitch)
{
	if (!IsTutorialControlAvailable(ETutorialControl::Camera))
	{
		return;
	}
	if (PlatformingCameraComponent)
	{
		PlatformingCameraComponent->HandleLookInput(Yaw, Pitch);
	}
}


void APlatformingCharacter::CancelDirectionalAction()
{
	if (UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement())
	{
		Movement->RequestCancelDirectionalAction();
	}
}


UPlatformingCharacterMovementComponent* APlatformingCharacter::GetPlatformingMovement() const
{
	return Cast<UPlatformingCharacterMovementComponent>(GetCharacterMovement());
}


void APlatformingCharacter::HandleRollMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement())
	{
		Movement->RequestFinishDirectionalAction(EDirectionalAction::Roll, !bInterrupted);
	}
}

void APlatformingCharacter::FinishRespawn()
{
	UE_LOG(LogTemp, Warning, TEXT("[Respawn] Timer completed"));

	if (!HasAuthority()) return;

	AController* PlayerController = GetController();
	if (!IsValid(PlayerController)) return;

	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AGameModeBase>();
	if (!IsValid(GameMode)) return;
	
	const TFunction<void(APawn*)> RestoreRespawnState = CaptureRespawnState();

	if (PlayerController->GetPawn() == this)
	{
		PlayerController->UnPossess();
	}

	if (IsValid(PlayerController) && IsValid(GameMode) && PlayerController->GetPawn() == nullptr)
	{
		GameMode->RestartPlayer(PlayerController);

		APawn* NewPawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
		if (!IsValid(NewPawn))
		{
			UE_LOG(LogTemp, Warning, TEXT("Respawn failed: RestartPlayer did not produce a valid Pawn."));
			return;
		}

		if (RestoreRespawnState)
		{
			RestoreRespawnState(NewPawn);
		}
	}

	GameMode->RestartPlayer(PlayerController);

	Destroy();
}

void APlatformingCharacter::MulticastHandleDeathVisuals_Implementation()
{
	if (IsValid(GetMesh()))
	{
		GetMesh()->SetVisibility(false, true);
	}
}


void APlatformingCharacter::HandleDamageHitboxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// TO DO: Death Mechanic
}



void APlatformingCharacter::MulticastPlayDashMontage_Implementation()
{
	if (HasAuthority() || IsLocallyControlled())
	{
		return;
	}

	PlayDashMontage();
}



void APlatformingCharacter::MulticastPlayRollMontage_Implementation()
{
	if (HasAuthority() || IsLocallyControlled())
	{
		return;
	}

	PlayRollMontageCosmetic();
}


void APlatformingCharacter::StartRespawnInputLock()
{
	if (!HasAuthority())
	{
		return;
	}

	bRespawnLocked = true;

	StopJumping();
	CancelDirectionalAction();

	if (UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement())
	{
		Movement->ClearPendingDirectionalActionRequest();
		Movement->SetRollSprintInputHeld(false);
		Movement->ClearPendingJumpRequest();
	}

	ClientSetRespawnInputLock(true);
	GetWorldTimerManager().SetTimer(RespawnInputLockTimerHandle, this, &APlatformingCharacter::EndRespawnInputLock, 1.0f, false);
}

void APlatformingCharacter::HandleRespawned()
{
	if (!HasAuthority()) return;

	// Stop character movement
	if (UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement())
	{
		Movement->StopMovementImmediately();
	}

	// Lock player input
	StartRespawnInputLock();

	// Play VFX
	if (IsValid(VFXComponent))
	{
		VFXComponent->PlayVFX(BuildFailedVFXGameplayTags::VFX_Player_Respawn);
	}
}

void APlatformingCharacter::HandleDeath()
{
	if (!HasAuthority()) return;

	// Stop character movement
	if (UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement())
	{
		Movement->StopMovementImmediately();
	}

	// Lock player input
	StartRespawnInputLock();

	// Play VFX
	if (IsValid(VFXComponent))
	{
		VFXComponent->PlayVFX(BuildFailedVFXGameplayTags::VFX_Player_Death);
	}

	// Hide mesh on all clients
	MulticastHandleDeathVisuals();
}

void APlatformingCharacter::BeginPlay()
{
	Super::BeginPlay();

	HandleRespawned();
	TryNotifyInitialSpawnReady();
}

void APlatformingCharacter::EndRespawnInputLock()
{
	if (!HasAuthority())
	{
		return;
	}

	bRespawnLocked = false;
	ClientSetRespawnInputLock(false);
}

void APlatformingCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	TryNotifyInitialSpawnReady();
}

void APlatformingCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	TryNotifyInitialSpawnReady();
}

void APlatformingCharacter::TryNotifyInitialSpawnReady()
{
	if (bInitialSpawnReadySent || !HasActorBegunPlay() || !IsLocallyControlled() || !IsValid(GetController()) || !IsValid(VFXComponent)) return;

	bInitialSpawnReadySent = true;

	ServerNotifyInitialSpawnReady();
}

void APlatformingCharacter::ServerNotifyInitialSpawnReady_Implementation()
{
	if (!HasAuthority()) return;

	OnInitialSpawnReady.Broadcast(this);
}

void APlatformingCharacter::ClientSetRespawnInputLock_Implementation(bool bLocked)
{
	bRespawnLocked = bLocked;

	if (!bLocked)
	{
		return;
	}

	StopJumping();
	CancelDirectionalAction();

	if (UPlatformingCharacterMovementComponent* Movement = GetPlatformingMovement())
	{
		Movement->StopMovementImmediately();
		Movement->ClearPendingDirectionalActionRequest();
		Movement->SetRollSprintInputHeld(false);
		Movement->ClearPendingJumpRequest();
	}
}



void APlatformingCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APlatformingCharacter, bRespawnLocked);
}
