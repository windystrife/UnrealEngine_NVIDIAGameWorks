// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor/UnrealEd/Public/SViewportToolBar.h"

// This is the interface that the host of a SCommonEditorViewportToolbarBase must implement
class ICommonEditorViewportToolbarInfoProvider
{
public:
	// Get the viewport widget
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() = 0;

// 	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
// 	TSharedPtr<FExtender> LevelEditorExtenders = LevelEditorModule.GetMenuExtensibilityManager()->GetAllExtenders();
	virtual TSharedPtr<FExtender> GetExtenders() const = 0;

	// Called to inform the host that a button was clicked (typically used to focus on a particular viewport in a multi-viewport setup)
	virtual void OnFloatingButtonClicked() = 0;
};

namespace CommonEditorViewportUtils
{
	struct FShowMenuCommand
	{
		TSharedPtr<FUICommandInfo> ShowMenuItem;
		FText LabelOverride;

		FShowMenuCommand(TSharedPtr<FUICommandInfo> InShowMenuItem, const FText& InLabelOverride)
			: ShowMenuItem(InShowMenuItem)
			, LabelOverride(InLabelOverride)
		{
		}

		FShowMenuCommand(TSharedPtr<FUICommandInfo> InShowMenuItem)
			: ShowMenuItem(InShowMenuItem)
		{
		}
	};

	static inline void FillShowMenu(class FMenuBuilder& MenuBuilder, TArray<FShowMenuCommand> MenuCommands, int32 EntryOffset)
	{
		// Generate entries for the standard show flags
		// Assumption: the first 'n' entries types like 'Show All' and 'Hide All' buttons, so insert a separator after them
		for (int32 EntryIndex = 0; EntryIndex < MenuCommands.Num(); ++EntryIndex)
		{
			MenuBuilder.AddMenuEntry(MenuCommands[EntryIndex].ShowMenuItem, NAME_None, MenuCommands[EntryIndex].LabelOverride);
			if (EntryIndex == EntryOffset - 1)
			{
				MenuBuilder.AddMenuSeparator();
			}
		}
	}
}

/**
 * A viewport toolbar widget for an asset or level editor that is placed in a viewport
 */
class UNREALED_API SCommonEditorViewportToolbarBase : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SCommonEditorViewportToolbarBase){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider);

private:
	/**
	 * Returns the label for the "Camera" tool bar menu, which changes depending on the viewport type
	 *
	 * @return	Label to use for this menu label
	 */
	FText GetCameraMenuLabel() const;

	/**
	 * Returns the label icon for the "Camera" tool bar menu, which changes depending on the viewport type
	 *
	 * @return	Label icon to use for this menu label
	 */
	const FSlateBrush* GetCameraMenuLabelIcon() const;

	/**
	 * Returns the label for the "View" tool bar menu, which changes depending on viewport show flags
	 *
	 * @return	Label to use for this menu label
	 */
	FText GetViewMenuLabel() const;

	/**
	 * Returns the label icon for the "View" tool bar menu, which changes depending on viewport show flags
	 *
	 * @return	Label icon to use for this menu label
	 */
	const FSlateBrush* GetViewMenuLabelIcon() const;

	/**
	 * Generates the toolbar options menu content 
	 *
	 * @return The widget containing the options menu content
	 */
	TSharedRef<SWidget> GenerateOptionsMenu() const;

	/**
	 * Generates the toolbar camera menu content 
	 *
	 * @return The widget containing the view menu content
	 */
	TSharedRef<SWidget> GenerateCameraMenu() const;

	/**
	 * Generates the toolbar view menu content 
	 *
	 * @return The widget containing the view menu content
	 */
	TSharedRef<SWidget> GenerateViewMenu() const;

	/**
	 * Generates the toolbar show menu content 
	 *
	 * @return The widget containing the show menu content
	 */
	virtual TSharedRef<SWidget> GenerateShowMenu() const;


	/**
	 * Returns the initial visibility of the view mode options widget 
	 *
	 * @return The visibility value
	 */
	EVisibility GetViewModeOptionsVisibility() const;

	/**
	 * Generates the toolbar view param menu content 
	 *
	 * @return The widget containing the show menu content
	 */
	TSharedRef<SWidget> GenerateViewModeOptionsMenu() const;

	/**
	 * @return The widget containing the perspective only FOV window.
	 */
	TSharedRef<SWidget> GenerateFOVMenu() const;

	/** Called by the FOV slider in the perspective viewport to get the FOV value */
	float OnGetFOVValue() const;

	/**
	 * @return The widget containing the far view plane slider.
	 */
	TSharedRef<SWidget> GenerateFarViewPlaneMenu() const;

	/** Called by the far view plane slider in the perspective viewport to get the far view plane value */
	float OnGetFarViewPlaneValue() const;

	/** Called when the far view plane slider is adjusted in the perspective viewport */
	void OnFarViewPlaneValueChanged( float NewValue );

protected:
	// Merges the extender list from the host with the specified extender and returns the results
	TSharedPtr<FExtender> GetCombinedExtenderList(TSharedRef<FExtender> MenuExtender) const;

	/** Gets the extender for the view menu */
	virtual TSharedPtr<FExtender> GetViewMenuExtender() const;

	void CreateViewMenuExtensions(FMenuBuilder& MenuBuilder);

protected:
	// Returns the info provider for this viewport
	ICommonEditorViewportToolbarInfoProvider& GetInfoProvider() const;

	// Get the viewport client
	class FEditorViewportClient& GetViewportClient() const;

protected:
	// Creates the view menu widget (override point for children)
	virtual TSharedRef<class SEditorViewportViewMenu> MakeViewMenu();

	/** Called when the FOV slider is adjusted in the perspective viewport */
	virtual void OnFOVValueChanged(float NewValue) const;

private:
	/** The viewport that we are in */
	TWeakPtr<class ICommonEditorViewportToolbarInfoProvider> InfoProviderPtr;
};

