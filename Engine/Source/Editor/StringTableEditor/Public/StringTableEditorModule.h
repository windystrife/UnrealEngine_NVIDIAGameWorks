// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"

class IStringTableEditor;
class UStringTable;

/** String Table Editor module */
class STRINGTABLEEDITOR_API FStringTableEditorModule : public IModuleInterface, public IHasMenuExtensibility
{
public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Creates an instance of string table editor object.
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	StringTable				The string table to start editing
	 *
	 * @return	Interface to the new string table editor
	 */
	TSharedRef<IStringTableEditor> CreateStringTableEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UStringTable* StringTable);

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }

	/** StringTable Editor app identifier string */
	static const FName StringTableEditorAppIdentifier;

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
};
