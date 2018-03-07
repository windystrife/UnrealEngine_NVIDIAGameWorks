// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableTextBox.h"

class SSubversionSourceControlSettings : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SSubversionSourceControlSettings) {}
	
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);

	/** Get the currently entered password */
	static FString GetPassword();

private:

	/** Delegate to get repository text from settings */
	FText GetRepositoryText() const;

	/** Delegate to commit repository text to settings */
	void OnRepositoryTextCommited(const FText& InText, ETextCommit::Type InCommitType) const;

	/** Delegate to get user name text from settings */
	FText GetUserNameText() const;

	/** Delegate to commit user name text to settings */
	void OnUserNameTextCommited(const FText& InText, ETextCommit::Type InCommitType) const;

	/** Delegate to get labels root text from settings */
	FText GetLabelsRootText() const;

	/** Delegate to commit labels root text to settings */
	void OnLabelsRootTextCommited(const FText& InText, ETextCommit::Type InCommitType) const;

private:

	/** Pointer to the password text box widget */
	static TWeakPtr<class SEditableTextBox> PasswordTextBox;
};
