// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IEditableSkeleton.h"
#include "Widgets/SCompoundWidget.h"
#include "EditorUndoClient.h"
#include "BlendProfilePicker.h"

class UBlendProfile;

// Picker for UBlendProfile instances inside a USkeleton
class SBlendProfilePicker : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SBlendProfilePicker)
		: _InitialProfile(nullptr)
		, _OnBlendProfileSelected()
		, _AllowNew(true)
		, _AllowClear(true)
	{}
		// Initial blend profile selected
		SLATE_ARGUMENT(UBlendProfile*, InitialProfile)
		// Delegate to call when the picker selection is changed
		SLATE_EVENT(FOnBlendProfileSelected, OnBlendProfileSelected)
		// Allow the option to create new profiles in the picker
		SLATE_ARGUMENT(bool, AllowNew)
		// Allow the option to clear the profile selection
		SLATE_ARGUMENT(bool, AllowClear)
		// Is this a standalone blend profile picker?
		SLATE_ARGUMENT(bool, Standalone)
	SLATE_END_ARGS()

	~SBlendProfilePicker();

	void Construct(const FArguments& InArgs, TSharedRef<class IEditableSkeleton> InEditableSkeleton);

	/** Set the selected profile externally 
	 *  @param InProfile New Profile to set
	 *  @param bBroadcast Whether or not to broadcast this selection
	 */
	void SetSelectedProfile(UBlendProfile* InProfile, bool bBroadcast = true);

	/** Get the currently selected blend profile */
	UBlendProfile* const GetSelectedBlendProfile() const;

	/** Get the currently selected blend profile name */
	FName GetSelectedBlendProfileName() const;

private:

	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	void OnClearSelection();
	void OnCreateNewProfile();
	void OnCreateNewProfileComitted(const FText& NewName, ETextCommit::Type CommitType);

	void OnProfileSelected(FName InBlendProfileName);
	void OnProfileRemoved(FName InBlendProfileName);

	FText GetSelectedProfileName() const;

	TSharedRef<SWidget> GetMenuContent();

	bool bShowNewOption;
	bool bShowClearOption;
	bool bIsStandalone;

	FName SelectedProfileName;

	TWeakPtr<class IEditableSkeleton> EditableSkeleton;

	FOnBlendProfileSelected BlendProfileSelectedDelegate;

};
