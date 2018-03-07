// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Framework/Commands/Commands.h"
#include "Reply.h"
#include "AssetData.h"
#include "UObject/WeakObjectPtr.h"
#include "Styling/CoreStyle.h"
#include "STreeView.h"
#include "Constraint.h"

class ITableRow;
class UHumanRig;
class STableViewBase;
class FUICommandList;

class FHumanRigNodeCommand : public TCommands<FHumanRigNodeCommand>
{
public:

	FHumanRigNodeCommand()
		: TCommands<FHumanRigNodeCommand>(TEXT("TreeNodeCommand"), NSLOCTEXT("TreeNodeCommands", "TreeNodeCommands", "Node Commands"), NAME_None, FCoreStyle::Get().GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> AddManipulator;
	TSharedPtr<FUICommandInfo> SetupLimb;
	TSharedPtr<FUICommandInfo> SetupSpine;
	TSharedPtr<FUICommandInfo> SetupFingers;
	TSharedPtr<FUICommandInfo> AddNode;
	TSharedPtr<FUICommandInfo> AddFKNode;
	TSharedPtr<FUICommandInfo> Reparent;
	TSharedPtr<FUICommandInfo> ClearRotation;
	TSharedPtr<FUICommandInfo> SetTranslation;
	TSharedPtr<FUICommandInfo> RenameNode;
};
//////////////////////////////////////////


class FHumanRigDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	~FHumanRigDetails();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	enum class EControlRigEditoMode : uint8
	{
		None,
		EditMode, // editing hierarchy and so on 
		InputMode, // editing inputs of the system
	};

private:

	// edit mode
	EControlRigEditoMode CurrentEditMode;

	// import mesh related
	FReply ImportMesh();

	void SetCurrentMesh(const FAssetData& InAssetData);
	FString GetCurrentlySelectedMesh() const; 

	FAssetData CurrentlySelectedAssetData;

	// no support on multi selection
	TWeakObjectPtr<class UHumanRig> CurrentlySelectedObject;

	// Storage object for bone hierarchy
	struct FNodeNameInfo
	{
		FNodeNameInfo(FName InName, const FText& InDisplayName) : NodeName(InName), DisplayName(InDisplayName), bIsConstraint(false) {}
		FNodeNameInfo(FName InName, const struct FTransformConstraint& InCachedConstraint);

		FName NodeName;
		FText DisplayName;
		// this is better to be enum later? For now, it just marks constraint or not
		bool bIsConstraint;
		struct FTransformConstraint CachedConstraint;
		// select node?
		TArray<TSharedPtr<FNodeNameInfo>> Children;
	};

	// Using the current filter, repopulate the tree view
	void RebuildTree();

	// Make a single tree row widget
	TSharedRef<ITableRow> MakeTreeRowWidget(TSharedPtr<FNodeNameInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);

	// Get the children for the provided bone info
	void GetChildrenForInfo(TSharedPtr<FNodeNameInfo> InInfo, TArray< TSharedPtr<FNodeNameInfo> >& OutChildren);

	// Called when the user changes the search filter
	void OnFilterTextChanged(const FText& InFilterText);

	void OnSelectionChanged(TSharedPtr<FNodeNameInfo> BoneInfo, ESelectInfo::Type SelectInfo);

	// Tree info entries for bone picker
	TArray<TSharedPtr<FNodeNameInfo>> SkeletonTreeInfo;
	// Mirror of SkeletonTreeInfo but flattened for searching
	TArray<TSharedPtr<FNodeNameInfo>> SkeletonTreeInfoFlat;

	// Text to filter bone tree with
	FText FilterText;

	// Tree view used in the button menu
	TSharedPtr<STreeView<TSharedPtr<FNodeNameInfo>>> TreeView;

	// context menu for tree view
	TSharedPtr<SWidget> OnContextMenuOpening() const;

	TSharedPtr<FUICommandList> CommandList;

	void CreateCommandList();

	void OnDeleteNodeSelected();
	void OnSetupLimb();
	void OnSetupSpine();
	void OnSetupFingers();
	void OnAddManipulator();
	void OnClearRotation();
	void OnAddNode();
	void OnAddFKNode();
	void OnReparent();
	void OnSetTranslation();
	void OnRenameNode();
	FName GetFirstSelectedNodeName() const;

	FReply UpdateConstraintsClicked();
	FReply AddNodeButtonClicked();
	FReply SetupLimbButtonClicked();
	FReply SetupSpineButtonClicked();
	FReply ReparentButtonClicked();
	FReply SetTranslationButtonClicked();
	FReply RenameNodeButtonClicked();

	void AddNode(FName ParentName, FName NewNodeName, const FVector& Translation);
	void SetupLimb(FName PropertyName, FName Upper, FName Middle, FName Lower);
	void SetupSpine(FName TopNode, FName EndNode);
	void SetTranslation(FName NodeName, const FVector& Translation);
	void RenameNode(const FName& OldName, const FName& NewName);

	void HandleNodesSelected(const TArray<FName>& NodeNames);

	// brute force text boxes and inputs
	TSharedRef<SWidget> CreateAddNodeMenu() const;
	TSharedRef<SWidget> CreateSetupLimbMenu() const;
	TSharedRef<SWidget> CreateSetupSpineMenu() const;
	TSharedRef<SWidget> CreateAddFKNodeMenu() const;
	TSharedRef<SWidget> CreateReparentMenu() const;
	TSharedRef<SWidget> CreateSetTranslation() const;
	TSharedRef<SWidget> CreateRenameNode() const;

	// node inputs
	TArray<FText> NodeInputs;
	FText OnGetNodeInputText(int32 Index) const
	{
		return NodeInputs[Index];
	}
	void OnNodeInputTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, int32 Index)
	{
		NodeInputs[Index] = NewText;
	}

	FText LimbProperty;
	FText OnGetLimbPropertyText() const
	{
		return LimbProperty;
	}
	void OnLimbPropertyTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
	{
		LimbProperty = NewText;
	}

	FVector InputTranslation;
	void On_TransX_EntryBoxChanged(float NewValue)
	{
		InputTranslation.X = NewValue;
	}

	TOptional<float> OnGet_TransX_EntryBoxValue() const
	{
		return InputTranslation.X;
	}

	void On_TransY_EntryBoxChanged(float NewValue)
	{
		InputTranslation.Y = NewValue;
	}

	TOptional<float> OnGet_TransY_EntryBoxValue() const
	{
		return InputTranslation.Y;
	}

	void On_TransZ_EntryBoxChanged(float NewValue)
	{
		InputTranslation.Z = NewValue;
	}

	TOptional<float> OnGet_TransZ_EntryBoxValue() const
	{
		return InputTranslation.Z;
	}
};
