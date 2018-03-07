// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "IContentBrowserSingleton.h"

class SPathView;

/**
 * A sources view designed for path picking
 */
class SPathPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPathPicker ){}

		/** A struct containing details about how the path picker should behave */
		SLATE_ARGUMENT(FPathPickerConfig, PathPickerConfig)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Sets the selected paths in this picker */
	void SetPaths(const TArray<FString>& NewPaths);

	/** Return the selected paths in this picker */
	TArray<FString> GetPaths() const;

	/** Return the associated SPathView */
	const TSharedPtr<SPathView>& GetPathView() const;

	/** Handler for creating a new folder in the path picker */
	void CreateNewFolder(FString FolderPath, FOnCreateNewFolder InOnCreateNewFolder);

private:

	/** Handler for the context menu for folder items */
	TSharedPtr<SWidget> GetFolderContextMenu(const TArray<FString>& SelectedPaths, FContentBrowserMenuExtender_SelectedPaths InMenuExtender, FOnCreateNewFolder InOnCreateNewFolder);

private:

	/** The path view in this picker */
	TSharedPtr<SPathView> PathViewPtr;
};
