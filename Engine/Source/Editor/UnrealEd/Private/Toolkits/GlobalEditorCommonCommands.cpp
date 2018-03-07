// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/GlobalEditorCommonCommands.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "OutputLogModule.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Toolkits/SGlobalOpenAssetDialog.h"
#include "Toolkits/SGlobalTabSwitchingDialog.h"

#define LOCTEXT_NAMESPACE "GlobalEditorCommonCommands"

//////////////////////////////////////////////////////////////////////////
// FGlobalEditorCommonCommands

FGlobalEditorCommonCommands::FGlobalEditorCommonCommands()
	: TCommands<FGlobalEditorCommonCommands>(TEXT("SystemWideCommands"), NSLOCTEXT("Contexts", "SystemWideCommands", "System-wide"), NAME_None, FEditorStyle::GetStyleSetName())
{
}

void FGlobalEditorCommonCommands::RegisterCommands()
{
	UI_COMMAND( SummonControlTabNavigation, "Tab Navigation", "Summons a list of open assets and tabs", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::Tab) );
	UI_COMMAND( SummonControlTabNavigationAlternate, "Tab Navigation", "Summons a list of open assets and tabs", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Command, EKeys::Tab) );

	UI_COMMAND( SummonOpenAssetDialog, "Open Asset...", "Summons an asset picker", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::P) );
	UI_COMMAND( SummonOpenAssetDialogAlternate, "Open Asset...", "Summons an asset picker", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::O));
	UI_COMMAND( FindInContentBrowser, "Browse to Asset", "Browses to the associated asset and selects it in the most recently used Content Browser (summoning one if necessary)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::B));
	UI_COMMAND( ViewReferences, "Reference Viewer...", "Launches the reference viewer showing the selected assets' references", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::R));
	UI_COMMAND( ViewSizeMap, "Size Map...", "Displays an interactive map showing the approximate size of this asset and everything it references", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::M));	// @todo sizemap: Make sure key is not used already
	
	UI_COMMAND( OpenConsoleCommandBox, "Open Console Command Box", "Opens an edit box where you can type in a console command", EUserInterfaceActionType::Button, FInputChord(EKeys::Tilde));

	UI_COMMAND( OpenDocumentation, "Open Documentation...", "Opens documentation for this tool", EUserInterfaceActionType::Button, FInputChord(EKeys::F1) );
}

void FGlobalEditorCommonCommands::MapActions(TSharedRef<FUICommandList>& ToolkitCommands)
{
	Register();

 	ToolkitCommands->MapAction(
 		Get().SummonControlTabNavigation,
 		FExecuteAction::CreateStatic( &FGlobalEditorCommonCommands::OnPressedCtrlTab, Get().SummonControlTabNavigation ) );

 	ToolkitCommands->MapAction(
 		Get().SummonControlTabNavigationAlternate,
 		FExecuteAction::CreateStatic( &FGlobalEditorCommonCommands::OnPressedCtrlTab, Get().SummonControlTabNavigationAlternate ) );

	ToolkitCommands->MapAction(
		Get().SummonOpenAssetDialog,
		FExecuteAction::CreateStatic( &FGlobalEditorCommonCommands::OnSummonedAssetPicker ) );

	ToolkitCommands->MapAction(
		Get().SummonOpenAssetDialogAlternate,
		FExecuteAction::CreateStatic(&FGlobalEditorCommonCommands::OnSummonedAssetPicker));

	ToolkitCommands->MapAction(
		Get().OpenConsoleCommandBox,
		FExecuteAction::CreateStatic( &FGlobalEditorCommonCommands::OnSummonedConsoleCommandBox ) );
}

void FGlobalEditorCommonCommands::OnPressedCtrlTab(TSharedPtr<FUICommandInfo> TriggeringCommand)
{
	if (!SGlobalTabSwitchingDialog::IsAlreadyOpen())
	{
		const FVector2D TabListSize(700.0f, 486.0f);

		// Create the contents of the popup
		TSharedRef<SWidget> ActualWidget = SNew(SGlobalTabSwitchingDialog, TabListSize, *TriggeringCommand->GetFirstValidChord());

		OpenPopupMenu(ActualWidget, TabListSize);
	}
}

void FGlobalEditorCommonCommands::OnSummonedAssetPicker()
{
	const FVector2D AssetPickerSize(600.0f, 586.0f);

	// Create the contents of the popup
	TSharedRef<SWidget> ActualWidget = SNew(SGlobalOpenAssetDialog, AssetPickerSize);

	// Wrap the picker widget in a multibox-style menu body
	FMenuBuilder MenuBuilder(/*BShouldCloseAfterSelection=*/ false, /*CommandList=*/ nullptr);
	MenuBuilder.BeginSection("AssetPickerOpenAsset", NSLOCTEXT("GlobalAssetPicker", "WindowTitle", "Open Asset"));
	MenuBuilder.AddWidget(ActualWidget, FText::GetEmpty(), /*bNoIndent=*/ true);
	MenuBuilder.EndSection();

	OpenPopupMenu(MenuBuilder.MakeWidget(), AssetPickerSize);
}

TSharedPtr<IMenu> FGlobalEditorCommonCommands::OpenPopupMenu(TSharedRef<SWidget> WindowContents, const FVector2D& PopupDesiredSize)
{
	// Determine where the pop-up should open
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	FVector2D WindowPosition = FSlateApplication::Get().GetCursorPos();
	if (!ParentWindow.IsValid())
	{
		TSharedPtr<SDockTab> LevelEditorTab = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorTab();
		ParentWindow = LevelEditorTab->GetParentWindow();
		check(ParentWindow.IsValid());
	}

	if (ParentWindow.IsValid())
	{
		FSlateRect ParentMonitorRect = ParentWindow->GetFullScreenInfo();
		const FVector2D MonitorCenter((ParentMonitorRect.Right + ParentMonitorRect.Left) * 0.5f, (ParentMonitorRect.Top + ParentMonitorRect.Bottom) * 0.5f);
		WindowPosition = MonitorCenter - PopupDesiredSize * 0.5f;

		// Open the pop-up
		FPopupTransitionEffect TransitionEffect(FPopupTransitionEffect::None);
		return FSlateApplication::Get().PushMenu(ParentWindow.ToSharedRef(), FWidgetPath(), WindowContents, WindowPosition, TransitionEffect, /*bFocusImmediately=*/ true);
	}

	return TSharedPtr<IMenu>();
}

static void CloseDebugConsole()
{
	FOutputLogModule& OutputLogModule = FModuleManager::LoadModuleChecked< FOutputLogModule >(TEXT("OutputLog"));
	OutputLogModule.CloseDebugConsole();
}

void FGlobalEditorCommonCommands::OnSummonedConsoleCommandBox()
{
	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	
	if( ParentWindow.IsValid() && ParentWindow->GetType() == EWindowType::Normal )
	{
		TSharedRef<SWindow> WindowRef = ParentWindow.ToSharedRef();
		FOutputLogModule& OutputLogModule = FModuleManager::LoadModuleChecked< FOutputLogModule >(TEXT("OutputLog"));

		FDebugConsoleDelegates Delegates;
		Delegates.OnConsoleCommandExecuted = FSimpleDelegate::CreateStatic( &CloseDebugConsole );

		OutputLogModule.ToggleDebugConsoleForWindow(WindowRef, EDebugConsoleStyle::Compact, Delegates);
	}
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
