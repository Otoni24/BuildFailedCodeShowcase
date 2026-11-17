#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PlatformingPlayerState.generated.h"


UENUM(BlueprintType)
enum class EPlatformingPlayerIdentity : uint8
{
	None,
	Player1,
	Player2
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETutorialControl : uint8
{
	None = 0 UMETA(Hidden),
	Camera = 1 UMETA(DisplayName = "Camera"),
	Movement = 2 UMETA(DisplayName = "Movimento"),
	Jump = 4 UMETA(DisplayName = "Salto"),
	Roll = 8 UMETA(DisplayName = "Roll"),
	Dash = 16 UMETA(DisplayName = "Dash"),
	Sprint = 32 UMETA(DisplayName = "Sprint"),
	ScaleSwitch = 64 UMETA(DisplayName = "Cambio dimensione")
};
ENUM_CLASS_FLAGS(ETutorialControl);


UCLASS()
class PLAYER_API APlatformingPlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	APlatformingPlayerState();

	// Stable identity assigned by Server when Player joins.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Player Identity")
	EPlatformingPlayerIdentity PlayerIdentity;

	void SetPlayerIdentity(EPlatformingPlayerIdentity NewIdentity);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Tutorial")
	bool bTutorialControlsRestricted = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Tutorial", meta = (Bitmask, BitmaskEnum = "/Script/Player.ETutorialControl"))
	int32 TutorialControls = 127;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dialogue")
	bool bDialogueMovementLocked = false;

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	bool IsTutorialControlEnabled(ETutorialControl Control) const;

	void InitializeTutorialControls(int32 InitialControls);
	void UnlockTutorialControls(int32 Controls);

	// to transfer custom data from an old PlayerState to the new one during Seamless Travel
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
