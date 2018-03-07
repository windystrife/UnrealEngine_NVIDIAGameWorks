// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneKeyStruct.generated.h"

struct FPropertyChangedEvent;

/**
 * Base class for movie scene section key structs that need to manually
 * have their changes propagated to key values.
 */
USTRUCT()
struct FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/**
	 * Propagate changes from this key structure to the corresponding key values.
	 *
	 * @param ChangeEvent The property change event.
	 */
	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) { };

	virtual ~FMovieSceneKeyStruct() {};
};
