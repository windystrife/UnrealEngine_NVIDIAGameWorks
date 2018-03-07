// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/ReflectionCapture.h"
#include "BoxReflectionCapture.generated.h"


/** 
 * Actor used to capture the scene for reflection in a box shape 
 * @see https://docs.unrealengine.com/latest/INT/Resources/ContentExamples/Reflections/1_3/index.html
 */
UCLASS(hidecategories=(Collision, Attachment, Actor), MinimalAPI)
class ABoxReflectionCapture
	: public AReflectionCapture
{
	GENERATED_UCLASS_BODY()
};
