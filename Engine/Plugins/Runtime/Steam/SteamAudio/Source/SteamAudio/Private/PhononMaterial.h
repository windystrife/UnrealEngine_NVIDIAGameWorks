//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "phonon.h"

UENUM()
enum class EPhononMaterial : uint8
{
	GENERIC = 0 UMETA(DisplayName = "Generic"),
	BRICK = 1 UMETA(DisplayName = "Brick"),
	CONCRETE = 2 UMETA(DisplayName = "Concrete"),
	CERAMIC = 3 UMETA(DisplayName = "Ceramic"),
	GRAVEL = 4 UMETA(DisplayName = "Gravel"),
	CARPET = 5 UMETA(DisplayName = "Carpet"),
	GLASS = 6 UMETA(DisplayName = "Glass"),
	PLASTER = 7 UMETA(DisplayName = "Plaster"),
	WOOD = 8 UMETA(DisplayName = "Wood"),
	METAL = 9 UMETA(DisplayName = "Metal"),
	ROCK = 10 UMETA(DisplayName = "Rock"),
	CUSTOM = 11 UMETA(DisplayName = "Custom")
};

namespace SteamAudio
{
	extern TMap<EPhononMaterial, IPLMaterial> MaterialPresets;
}