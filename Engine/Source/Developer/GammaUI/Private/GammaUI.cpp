// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GammaUI.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "GammaUIPanel.h"

IMPLEMENT_MODULE( FGammaUI, GammaUI );

void FGammaUI::StartupModule()
{
}

void FGammaUI::ShutdownModule()
{
}

/** @return a pointer to the Gamma manager panel; invalid pointer if one has already been allocated */
TSharedPtr< SWidget > FGammaUI::GetGammaUIPanel()
{
	return SNew(SGammaUIPanel);
}
