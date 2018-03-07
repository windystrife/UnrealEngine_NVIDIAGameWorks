// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

class FExtender;
class FMenuBuilder;
class FTranslationEditor;

/**
 * Translation Editor menu
 */
class TRANSLATIONEDITOR_API FTranslationEditorMenu
{
public:
	static void SetupTranslationEditorMenu( TSharedPtr< FExtender > Extender, FTranslationEditor& TranslationEditor);
	static void SetupTranslationEditorToolbar( TSharedPtr< FExtender > Extender, FTranslationEditor& TranslationEditor);
	
protected:

	static void FillTranslationMenu( FMenuBuilder& MenuBuilder/*, FTranslationEditor& TranslationEditor*/ );
};


class FTranslationEditorCommands : public TCommands<FTranslationEditorCommands>
{
public:
	/** Constructor */
	FTranslationEditorCommands() 
		: TCommands<FTranslationEditorCommands>("TranslationEditor", NSLOCTEXT("Contexts", "TranslationEditor", "Translation Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	/** Switch fonts */
	TSharedPtr<FUICommandInfo> ChangeSourceFont;
	TSharedPtr<FUICommandInfo> ChangeTranslationTargetFont;

	/** Save the translations to file */
	TSharedPtr<FUICommandInfo> SaveTranslations;

	/** Save the translations to file */
	TSharedPtr<FUICommandInfo> PreviewAllTranslationsInEditor;
	
	/** Download and import the latest translations from the active Translation Service */
	TSharedPtr<FUICommandInfo> ImportLatestFromLocalizationService;

	/** Export to PortableObject format (.po) */
	TSharedPtr<FUICommandInfo> ExportToPortableObjectFormat;

	/** Import from PortableObject format (.po) */
	TSharedPtr<FUICommandInfo> ImportFromPortableObjectFormat;

	/** Open the tab for searching */
	TSharedPtr<FUICommandInfo> OpenSearchTab;

	/** Open the translation picker */
	TSharedPtr<FUICommandInfo> OpenTranslationPicker;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

