#pragma once

#include "CoreMinimal.h"


enum class EDirectionalAction : uint8
{
	None,
	Dash,
	Roll
};

enum class EDashAvailability : uint8
{
	Available,
	Consumed
};
