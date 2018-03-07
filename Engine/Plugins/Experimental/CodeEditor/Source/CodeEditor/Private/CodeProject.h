// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "CodeProjectItem.h"
#include "CodeProject.generated.h"

UCLASS()
class UCodeProject : public UCodeProjectItem
{
	GENERATED_UCLASS_BODY()

	// @TODO: This class should probably be mostly config/settings stuff, with a details panel allowing editing somewhere
};
