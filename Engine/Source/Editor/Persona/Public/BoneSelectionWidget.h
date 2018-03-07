// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

class IEditableSkeleton;
class SComboButton;

DECLARE_DELEGATE_OneParam(FOnBoneSelectionChanged, FName);
DECLARE_DELEGATE_RetVal_OneParam(FName, FGetSelectedBone, bool& /*bMultipleValues*/);
DECLARE_DELEGATE_RetVal(const struct FReferenceSkeleton&, FGetReferenceSkeleton);
DECLARE_DELEGATE_RetVal(const TArray<class USkeletalMeshSocket*>&, FGetSocketList);

class PERSONA_API SBoneTreeMenu : public SCompoundWidget
{
public:
	// Storage object for bone hierarchy
	struct FBoneNameInfo
	{
		FBoneNameInfo(FName Name) : BoneName(Name) {}

		FName BoneName;
		TArray<TSharedPtr<FBoneNameInfo>> Children;
	};

	SLATE_BEGIN_ARGS(SBoneTreeMenu)
		: _bShowVirtualBones(true)
		, _bShowSocket(false)
		, _OnGetReferenceSkeleton()
		, _OnBoneSelectionChanged()
		, _OnGetSocketList()
		{}

		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(bool, bShowVirtualBones)
		SLATE_ARGUMENT(bool, bShowSocket)
		SLATE_ARGUMENT(FName, SelectedBone)
		SLATE_EVENT(FGetReferenceSkeleton, OnGetReferenceSkeleton)
		SLATE_EVENT(FOnBoneSelectionChanged, OnBoneSelectionChanged)
		SLATE_EVENT(FGetSocketList, OnGetSocketList)

	SLATE_END_ARGS();

	/**
	* Construct this widget
	*
	* @param	InArgs	The declaration data for this widget
	*/
	void Construct(const FArguments& InArgs);

	//Filter text widget
	TSharedPtr<SSearchBox> FilterTextWidget;

private:

	// Using the current filter, repopulate the tree view
	void RebuildBoneList(const FName& SelectedBone);

	// Make a single tree row widget
	TSharedRef<ITableRow> MakeTreeRowWidget(TSharedPtr<FBoneNameInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);

	// Get the children for the provided bone info
	void GetChildrenForInfo(TSharedPtr<FBoneNameInfo> InInfo, TArray< TSharedPtr<FBoneNameInfo> >& OutChildren);

	// Called when the user changes the search filter
	void OnFilterTextChanged(const FText& InFilterText);

	void OnSelectionChanged(TSharedPtr<SBoneTreeMenu::FBoneNameInfo> BoneInfo, ESelectInfo::Type SelectInfo);

	// Tree info entries for bone picker
	TArray<TSharedPtr<FBoneNameInfo>> SkeletonTreeInfo;
	// Mirror of SkeletonTreeInfo but flattened for searching
	TArray<TSharedPtr<FBoneNameInfo>> SkeletonTreeInfoFlat;

	// Text to filter bone tree with
	FText FilterText;


	// Tree view used in the button menu
	TSharedPtr<STreeView<TSharedPtr<FBoneNameInfo>>> TreeView;

	FOnBoneSelectionChanged OnSelectionChangedDelegate;
	FGetReferenceSkeleton	OnGetReferenceSkeletonDelegate;
	FGetSocketList			OnGetSocketListDelegate;

	bool bShowVirtualBones;
	bool bShowSocket;
};

class PERSONA_API SBoneSelectionWidget : public SCompoundWidget
{
public: 

	SLATE_BEGIN_ARGS( SBoneSelectionWidget )
		: _bShowSocket(false)
		, _OnBoneSelectionChanged()
		, _OnGetSelectedBone()
		, _OnGetReferenceSkeleton()
		, _OnGetSocketList()
	{}

		SLATE_ARGUMENT(bool, bShowSocket)
		/** set selected bone name */
		SLATE_EVENT(FOnBoneSelectionChanged, OnBoneSelectionChanged);

		/** get selected bone name **/
		SLATE_EVENT(FGetSelectedBone, OnGetSelectedBone);

		/** Get Reference skeleton */
		SLATE_EVENT(FGetReferenceSkeleton, OnGetReferenceSkeleton)

		/** Get Socket List */
		SLATE_EVENT(FGetSocketList, OnGetSocketList)

	SLATE_END_ARGS();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

private: 

	// Creates the combo button menu when clicked
	TSharedRef<SWidget> CreateSkeletonWidgetMenu();
	// Called when the user selects a bone name
	void OnSelectionChanged(FName BoneName);

	// Gets the current bone name, used to get the right name for the combo button
	FText GetCurrentBoneName() const;

	FText GetFinalToolTip() const;

	// Base combo button 
	TSharedPtr<SComboButton> BonePickerButton;

	// delegates
	FOnBoneSelectionChanged OnBoneSelectionChanged;
	FGetSelectedBone		OnGetSelectedBone;
	FGetReferenceSkeleton	OnGetReferenceSkeleton;
	FGetSocketList			OnGetSocketList;
	bool bShowSocket;

	// Cache supplied tooltip
	FText SuppliedToolTip;
};
