#include "PlatformingPlayerState.h"
#include "Net/UnrealNetwork.h"


APlatformingPlayerState::APlatformingPlayerState()
{
	PlayerIdentity = EPlatformingPlayerIdentity::None;
}



void APlatformingPlayerState::SetPlayerIdentity(EPlatformingPlayerIdentity NewIdentity)
{
	PlayerIdentity = NewIdentity;
}

bool APlatformingPlayerState::IsTutorialControlEnabled(ETutorialControl Control) const
{
	return !bTutorialControlsRestricted || (TutorialControls & static_cast<int32>(Control)) != 0;
}

void APlatformingPlayerState::InitializeTutorialControls(int32 InitialControls)
{
	if (!HasAuthority() || bTutorialControlsRestricted)
	{
		return;
	}
	bTutorialControlsRestricted = true;
	TutorialControls = InitialControls & 127;
	ForceNetUpdate();
}

void APlatformingPlayerState::UnlockTutorialControls(int32 Controls)
{
	if (!HasAuthority())
	{
		return;
	}
	TutorialControls |= Controls & 127;
	ForceNetUpdate();
}


void APlatformingPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
    Super::CopyProperties(NewPlayerState);

    APlatformingPlayerState* NewPlatformingPlayerState = Cast<APlatformingPlayerState>(NewPlayerState);
    if (!NewPlatformingPlayerState)
    {
        return;
    }

    NewPlatformingPlayerState->SetPlayerIdentity(PlayerIdentity);
	NewPlatformingPlayerState->bTutorialControlsRestricted = bTutorialControlsRestricted;
	NewPlatformingPlayerState->TutorialControls = TutorialControls;
}



void APlatformingPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APlatformingPlayerState, PlayerIdentity);
	DOREPLIFETIME(APlatformingPlayerState, bTutorialControlsRestricted);
	DOREPLIFETIME(APlatformingPlayerState, TutorialControls);
	DOREPLIFETIME(APlatformingPlayerState, bDialogueMovementLocked);
}

