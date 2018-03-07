// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FunctionalTest.h"
#include "ScreenshotFunctionalTest.h"
#include "EditorFunctionalTest.generated.h"

/** Editor-only version of functional test actor, tests that call editor-only blueprints must inherit from this class */
UCLASS(Blueprintable, MinimalAPI)
class AEditorFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

	virtual bool IsEditorOnly() const override
	{
		return true;
	}
};

/** Editor-only version of screenshot functional test */
UCLASS(Blueprintable, MinimalAPI)
class AEditorScreenshotFunctionalTest : public AScreenshotFunctionalTest
{
	GENERATED_BODY()

	virtual bool IsEditorOnly() const override
	{
		return true;
	}
};
