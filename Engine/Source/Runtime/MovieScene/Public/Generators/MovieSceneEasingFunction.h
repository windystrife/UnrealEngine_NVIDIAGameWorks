// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneEasingFunction.generated.h"


UINTERFACE(Category="Sequencer", Blueprintable, meta=(DisplayName = "Easing Function"))
class MOVIESCENE_API UMovieSceneEasingFunction : public UInterface
{
public:
	GENERATED_BODY()
};

class MOVIESCENE_API IMovieSceneEasingFunction
{
public:
	GENERATED_BODY()

	/**
	 * Evaluate using the specified script interface. Handles both native and k2 implemented interfaces.
	 */
	static float EvaluateWith(const TScriptInterface<IMovieSceneEasingFunction>& ScriptInterface, float Time);

protected:

	/** Evaluate the easing with an interpolation value between 0 and 1 */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Sequencer", meta=(CallInEditor="true"))
	float OnEvaluate(float Interp) const;

	/** Evaluate the easing with an interpolation value between 0 and 1 */
	virtual float Evaluate(float Interp) const { return 0.f; }
};