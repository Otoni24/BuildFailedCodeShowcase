#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Seesaw.generated.h"

class APlatformingCharacter;
class UBoxComponent;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;


UENUM()
enum class ESeesawSide : uint8
{
	Left,
	Right
};


USTRUCT(BlueprintType)
struct FSeesawImpactTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0"))
	float MinImpactStrength = 0.0f;

	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0"))
	float LaunchVelocity = 0.0f;

	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0"))
	float SeesawKickAngularVelocity = 0.0f;
};


UCLASS()
class BUILD_FAILED_API ASeesaw : public AActor
{
	GENERATED_BODY()

public:
	// Construction
	ASeesaw();

protected:
	// AActor
	virtual void BeginPlay() override;

private:
	// Collision
	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void HandleActorEnteredZone(AActor* ImpactingActor, ESeesawSide Side);

	// Catapult
	void TryTriggerCatapult(const AActor* ImpactingActor, ESeesawSide ImpactSide);
	static float CalculateImpactStrength(const AActor* ImpactingActor);
	const FSeesawImpactTier* FindImpactTier(float ImpactStrength) const;
	APlatformingCharacter* GetCharacterOnSide(ESeesawSide Side) const;
	static ESeesawSide GetOppositeSide(ESeesawSide Side);
	void TriggerCatapult(ESeesawSide ImpactSide, const FSeesawImpactTier& ImpactTier);
	void ApplySeesawKick(ESeesawSide ImpactSide, const FSeesawImpactTier& ImpactTier);
	static void LaunchCharacter(APlatformingCharacter* Character, const FSeesawImpactTier& ImpactTier);

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> PlankMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> LeftZone;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> RightZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPhysicsConstraintComponent> PhysicsConstraint;

	// Catapult settings
	UPROPERTY(EditAnywhere, Category="Seesaw|Catapult", meta=(TitleProperty="MinImpactStrength"))
	TArray<FSeesawImpactTier> ImpactTiers;
	
	UPROPERTY(EditAnywhere, Category="Seesaw|Catapult", meta=(ClampMin="0.0"))
	float ImpactCooldown = 0.35f;
	
	UPROPERTY(EditAnywhere, Category="Seesaw|Catapult", meta=(ClampMin="0.0"))
	float SeesawKickDelay = 0.1f;

	// Catapult state
	float LastImpactTime = -BIG_NUMBER;
};
