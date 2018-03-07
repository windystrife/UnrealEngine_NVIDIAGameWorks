// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SComboButton.h"
#include "SBoneMappingBase.h"

class IEditableSkeleton;
class URig;
class USkeleton;
//////////////////////////////////////////////////////////////////////////
// SRigWindow

class SRigWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SRigWindow )
	{}

	SLATE_END_ARGS()

	/**
	* Slate construction function
	*
	* @param InArgs - Arguments passed from Slate
	*
	*/
	void Construct( const FArguments& InArgs, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnPostUndo );

private:

	/**
	* Clears and rebuilds the table, according to an optional search string
	*
	* @param SearchText - Optional search string
	*
	*/
	void CreateBoneMappingList( const FString& SearchText, TArray< TSharedPtr<FDisplayedBoneMappingInfo> >& BoneMappingList );

	/** Pointer back to the Persona that owns us */
	TWeakPtr<class IEditableSkeleton> EditableSkeletonPtr;

	/** show advanced? */
	bool bDisplayAdvanced;

	/** rig combo button */
	TSharedPtr< class SComboButton > AssetComboButton;

	/**
	 * Callback for asset picker
	 */
	/* Set rig set combo box*/
	void OnAssetSelected(UObject* Object);
	FText GetAssetName() const;
	void CloseComboButton();
	TSharedRef<SWidget> MakeRigPickerWithMenu();

	/** Returns true if the asset shouldn't show  */
	bool ShouldFilterAsset(const struct FAssetData& AssetData);

	URig* GetRigObject() const;
	
	void OnBoneMappingChanged( FName NodeName, FName BoneName );
	FName GetBoneMapping( FName NodeName );
	const struct FReferenceSkeleton& GetReferenceSkeleton() const;

	FReply OnAutoMapping();
	FReply OnClearMapping();
	FReply OnToggleView();

	FReply OnToggleAdvanced();
	FText GetAdvancedButtonText() const;

	bool SelectSourceReferenceSkeleton(URig* Rig) const;
	bool OnTargetSkeletonSelected(USkeleton* SelectedSkeleton, URig*  Rig) const;

	// bone mapping widget
	TSharedPtr<SBoneMappingBase> BoneMappingWidget;
};
