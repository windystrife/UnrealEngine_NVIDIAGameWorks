// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ITranslationEditor.h"

#define LOCTEXT_NAMESPACE "FMainFrameTranslationEditorMenu"


/**
 * Static helper class for populating the "Cook Content" menu.
 */
class FMainFrameTranslationEditorMenu
{
public:

	// Handles clicking a menu entry.
	static void HandleOpenTranslationPicker()
	{
		FModuleManager::LoadModuleChecked<ITranslationEditor>("TranslationEditor").OpenTranslationPicker();
	}

};


#undef LOCTEXT_NAMESPACE
