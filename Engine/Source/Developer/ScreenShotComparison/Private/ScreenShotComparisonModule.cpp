// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotComparisonModule.cpp: Implements the FScreenShotComparisonModule class.
=============================================================================*/

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Interfaces/IScreenShotComparisonModule.h"
#include "Widgets/SScreenShotBrowser.h"

static const FName ScreenShotComparisonName("ScreenShotComparison");


/**
 * Implements the ScreenShotComparison module.
 */
class FScreenShotComparisonModule
	: public IScreenShotComparisonModule
{
public:
	virtual TSharedRef<class SWidget> CreateScreenShotComparison( const IScreenShotManagerRef& ScreenShotManager ) override
	{
		return SNew(SScreenShotBrowser, ScreenShotManager);
	}

};


IMPLEMENT_MODULE(FScreenShotComparisonModule, ScreenShotComparison);
