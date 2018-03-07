// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Commands/Commands.h"

class IMenu;
class SWindow;

// Global editor common commands
// Note: There is no real global command concept, so these must still be registered in each editor
class UNREALED_API FGlobalEditorCommonCommands : public TCommands< FGlobalEditorCommonCommands >
{
public:
	FGlobalEditorCommonCommands();

	virtual void RegisterCommands() override;

	static void MapActions(TSharedRef<FUICommandList>& ToolkitCommands);

protected:
	static void OnPressedCtrlTab(TSharedPtr<FUICommandInfo> TriggeringCommand);
	static void OnSummonedAssetPicker();
	static void OnSummonedConsoleCommandBox();

	static TSharedPtr<IMenu> OpenPopupMenu(TSharedRef<SWidget> WindowContents, const FVector2D& PopupDesiredSize);
public:
	TSharedPtr<FUICommandInfo> FindInContentBrowser;
	TSharedPtr<FUICommandInfo> SummonControlTabNavigation;
	TSharedPtr<FUICommandInfo> SummonControlTabNavigationAlternate;
	TSharedPtr<FUICommandInfo> SummonOpenAssetDialog;
	TSharedPtr<FUICommandInfo> SummonOpenAssetDialogAlternate;
	TSharedPtr<FUICommandInfo> OpenDocumentation;
	TSharedPtr<FUICommandInfo> ViewReferences;
	TSharedPtr<FUICommandInfo> ViewSizeMap;
	TSharedPtr<FUICommandInfo> OpenConsoleCommandBox;
};

