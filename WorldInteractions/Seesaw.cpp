#include "Seesaw.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "TimerManager.h"

#include "PlatformingCharacter.h"


ASeesaw::ASeesaw()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PlankMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlankMesh"));
	PlankMesh->SetupAttachment(Root);
	PlankMesh->SetMobility(EComponentMobility::Movable);
	PlankMesh->SetIsReplicated(true);
	
	LeftZone = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftZone"));
	LeftZone->SetupAttachment(PlankMesh);
	
	RightZone = CreateDefaultSubobject<UBoxComponent>(TEXT("RightZone"));
	RightZone->SetupAttachment(PlankMesh);

	PhysicsConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("PhysicsConstraint"));
	PhysicsConstraint->SetupAttachment(Root);
	
	LeftZone->OnComponentBeginOverlap.AddDynamic(this, &ASeesaw::OnZoneBeginOverlap);
	RightZone->OnComponentBeginOverlap.AddDynamic(this, &ASeesaw::OnZoneBeginOverlap);
}

void ASeesaw::BeginPlay()
{
	Super::BeginPlay();
	
	SetReplicateMovement(false);
	PlankMesh->SetIsReplicated(true);
	PlankMesh->SetSimulatePhysics(HasAuthority());

	if (HasAuthority())
	{
		SetReplicates(true);
		PhysicsConstraint->SetConstrainedComponents(PlankMesh, NAME_None, nullptr, NAME_None);
	}
	else
	{
		PhysicsConstraint->BreakConstraint();
	}
}

void ASeesaw::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!HasAuthority() || !OtherActor || OtherActor == this)
    {
        return;
    }

    ESeesawSide Side;

    if (OverlappedComponent == LeftZone)
    {
        Side = ESeesawSide::Left;
    }
    else if (OverlappedComponent == RightZone)
    {
        Side = ESeesawSide::Right;
    }
    else
    {
        return;
    }

    HandleActorEnteredZone(OtherActor, Side);
}

void ASeesaw::HandleActorEnteredZone(AActor* ImpactingActor, const ESeesawSide Side)
{
	TryTriggerCatapult(ImpactingActor, Side);
}

void ASeesaw::TryTriggerCatapult(const AActor* ImpactingActor, const ESeesawSide ImpactSide)
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	if (CurrentTime - LastImpactTime < ImpactCooldown)
	{
		return;
	}

	const float ImpactStrength = CalculateImpactStrength(ImpactingActor);
	const FSeesawImpactTier* ImpactTier = FindImpactTier(ImpactStrength);

	if (!ImpactTier)
	{
		return;
	}

	LastImpactTime = CurrentTime;
	TriggerCatapult(ImpactSide, *ImpactTier);
}

float ASeesaw::CalculateImpactStrength(const AActor* ImpactingActor)
{
	float Mass = 0.f;
	float VelocityZ = 0.f;

	if (const APlatformingCharacter* ImpactingCharacter = Cast<APlatformingCharacter>(ImpactingActor))
	{
		if (ImpactingCharacter->GetCharacterMovement())
		{
			Mass = ImpactingCharacter->GetCharacterMovement()->Mass;
		}
		VelocityZ = ImpactingActor->GetVelocity().Z;
	}
	else if (const UStaticMeshComponent* Mesh = ImpactingActor->GetComponentByClass<UStaticMeshComponent>())
	{
		Mass = Mesh->GetMass();
		VelocityZ = Mesh->GetComponentVelocity().Z;
	}
	const float DownwardVelocity = FMath::Max(0.0f, -VelocityZ);

	return Mass * DownwardVelocity;
}

const FSeesawImpactTier* ASeesaw::FindImpactTier(const float ImpactStrength) const
{
	const FSeesawImpactTier* BestTier = nullptr;
	float BestThreshold = -1.0f;

	for (const FSeesawImpactTier& Tier : ImpactTiers)
	{
		if (ImpactStrength >= Tier.MinImpactStrength && Tier.MinImpactStrength > BestThreshold)
		{
			BestThreshold = Tier.MinImpactStrength;
			BestTier = &Tier;
		}
	}

	return BestTier;
}

APlatformingCharacter* ASeesaw::GetCharacterOnSide(const ESeesawSide Side) const
{
	const UBoxComponent* Zone = (Side == ESeesawSide::Left) ? LeftZone : RightZone;
	
	if (!Zone)
	{
		return nullptr;
	}
	
	TArray<AActor*> OverlappingActors;
	Zone->GetOverlappingActors(OverlappingActors, APlatformingCharacter::StaticClass());
	
	for (AActor* Actor : OverlappingActors)
	{
		if (APlatformingCharacter* Character = Cast<APlatformingCharacter>(Actor))
		{
			return Character;
		}
	}
	
	return nullptr;
}

ESeesawSide ASeesaw::GetOppositeSide(const ESeesawSide Side)
{
	return (Side == ESeesawSide::Left) ? ESeesawSide::Right : ESeesawSide::Left;
}

void ASeesaw::TriggerCatapult(const ESeesawSide ImpactSide, const FSeesawImpactTier& ImpactTier)
{
	const ESeesawSide OppositeSide = GetOppositeSide(ImpactSide);
	APlatformingCharacter* LaunchedCharacter = GetCharacterOnSide(OppositeSide);

	if (LaunchedCharacter)
	{
		LaunchCharacter(LaunchedCharacter, ImpactTier);
	}
	
	if (SeesawKickDelay <= 0.0f)
	{
		ApplySeesawKick(ImpactSide, ImpactTier);
		return;
	}

	FTimerHandle TimerHandle;

	GetWorldTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(this, [this, ImpactSide, ImpactTier]()
	{
		ApplySeesawKick(ImpactSide, ImpactTier);
	}), SeesawKickDelay, false);
}

void ASeesaw::ApplySeesawKick(const ESeesawSide ImpactSide, const FSeesawImpactTier& ImpactTier)
{
	const float Direction = ImpactSide == ESeesawSide::Left ? -1.0f : 1.0f;
	const FVector TwistAxis = PhysicsConstraint->GetComponentQuat().GetAxisX();
	const FVector AngularVelocity = TwistAxis * ImpactTier.SeesawKickAngularVelocity * Direction;
	
	PlankMesh->SetPhysicsAngularVelocityInDegrees(AngularVelocity, false);
}

void ASeesaw::LaunchCharacter(APlatformingCharacter* Character, const FSeesawImpactTier& ImpactTier)
{
	Character->LaunchCharacter(FVector(0, 0, ImpactTier.LaunchVelocity), false, true);
}
