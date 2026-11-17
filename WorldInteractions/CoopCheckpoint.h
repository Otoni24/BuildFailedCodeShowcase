#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoopCheckpoint.generated.h"

class APlayerStart;
class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;

UCLASS(Blueprintable)
class BUILD_FAILED_API ACoopCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	ACoopCheckpoint();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Checkpoint", meta = (ClampMin = "0", UIMin = "0"))
	int32 CheckpointIndex = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<APlayerStart> Player1Start;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<APlayerStart> Player2Start;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> ActivationBox;

private:
	UFUNCTION()
	void OnActivationBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
