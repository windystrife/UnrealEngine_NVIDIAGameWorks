// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Application/SlateWindowHelper.h"

class SEditableTextBox;

/**
 * A directory path box, with a button for picking a new path.
 */
class DESKTOPWIDGETS_API SDirectoryPicker : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnDirectoryChanged, const FString& /*Directory*/);

	SLATE_BEGIN_ARGS(SDirectoryPicker)
		 : _IsEnabled(true)

		{}
		SLATE_ARGUMENT(FString, Directory)
		SLATE_ARGUMENT(FString, File)
		SLATE_ARGUMENT(FText, Message)
		SLATE_ATTRIBUTE(bool, IsEnabled)

		/** Called when a path has been picked or modified. */
		SLATE_EVENT(FOnDirectoryChanged, OnDirectoryChanged)
	SLATE_END_ARGS()
	
public:

	void Construct(const FArguments& InArgs);
	
	FString GetFilePath() const;

	FString GetDirectory() const { return Directory; }

	/**
	* Declares a delegate that is executed when a file was picked in the SFilePathPicker widget.
	*
	* The first parameter will contain the path to the picked file.
	*/
	DECLARE_DELEGATE_OneParam(FOnDirectoryPicked, const FString& /*PickedPath*/);
	
private:
	void OnDirectoryTextChanged(const FText& InDirectoryPath);
	void OnDirectoryTextCommited(const FText& InText, ETextCommit::Type InCommitType);

	FText GetFilePathText() const;

	bool OpenPlatformDirectoryPicker(FString& OutDirectory, const FString& DefaultPath);

	FReply BrowseForDirectory();

private:
	FString File;
	FString Directory;
	FText Message;

	/** Holds a delegate that is executed when a file was picked. */
	FOnDirectoryChanged OnDirectoryChanged;

	TSharedPtr<SEditableTextBox> EditableTextBox;
};
