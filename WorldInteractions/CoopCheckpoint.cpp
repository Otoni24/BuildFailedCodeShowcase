#include "LevelElements/CoopCheckpoint.h"

#include "Build_FailedGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PlatformingPlayerState.h"

ACoopCheckpoint::ACoopCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ActivationBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ActivationBox"));
	ActivationBox->SetupAttachment(SceneRoot);
	ActivationBox->InitBoxExtent(FVector(100.0f));
	ActivationBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ActivationBox->SetCollisionObjectType(ECC_WorldDynamic);
	ActivationBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ActivationBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ActivationBox->SetGenerateOverlapEvents(true);
	ActivationBox->SetCanEverAffectNavigation(false);
	ActivationBox->OnComponentBeginOverlap.AddDynamic(this, &ACoopCheckpoint::OnActivationBoxBeginOverlap);
}

void ACoopCheckpoint::OnActivationBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || GetNetMode() == NM_Client)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed())
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	if (!IsValid(PlayerController) || PlayerController->GetPawn() != Pawn)
	{
		return;
	}

	const APlatformingPlayerState* PlayerState = PlayerController->GetPlayerState<APlatformingPlayerState>();
	if (!IsValid(PlayerState) ||
		(PlayerState->PlayerIdentity != EPlatformingPlayerIdentity::Player1 &&
		 PlayerState->PlayerIdentity != EPlatformingPlayerIdentity::Player2))
	{
		return;
	}

	if (ABuild_FailedGameMode* GameMode = GetWorld()->GetAuthGameMode<ABuild_FailedGameMode>())
	{
		GameMode->TryActivateCheckpoint(this);
	}
}
