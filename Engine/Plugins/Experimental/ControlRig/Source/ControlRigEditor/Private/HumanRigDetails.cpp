	// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HumanRigDetails.h"
#include "UObject/Class.h"
#include "HumanRig.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Framework/Commands/GenericCommands.h"
#include "ScopedTransaction.h"
#include "SButton.h"
#include "STreeView.h"
#include "SSearchBox.h"
#include "Framework/Commands/UICommandList.h"
#include "MultiBoxBuilder.h"
#include "ControlRigEditMode.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "HumanRigDetails"
#define MAXSPINE 5
#define MAXNODEINPUT	MAXSPINE
//////////////////////////////
// FControlRigNodeCommand
//////////////////////////////
void FHumanRigNodeCommand::RegisterCommands() 
{
	UI_COMMAND(AddNode, "Add Node", "Add new node", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AddManipulator, "Add Widget", "Add widget to the selected node.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SetupSpine, "Set Up Spine", "Set Up Spline", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SetupLimb, "Set Up Limb", "Set Up Limb", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SetupFingers, "Set Up Fingers", "Set Up Fingers", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ClearRotation, "Clear Rotation", "Clear Rotation for all nodes", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SetTranslation, "Set Translation", "Set Translation", EUserInterfaceActionType::Button, FInputChord());
	
	UI_COMMAND(AddFKNode, "Add FK Node", "Add FK Node", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Reparent, "Change Parent", "Change Parent", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(RenameNode, "Rename Node", "Rename Node", EUserInterfaceActionType::Button, FInputChord());
}
//////////////////////////////
TSharedRef<IDetailCustomization> FHumanRigDetails::MakeInstance()
{
	return MakeShareable( new FHumanRigDetails );
}

FHumanRigDetails::~FHumanRigDetails()
{
	if (FControlRigEditMode* ControlRigEditMode = GLevelEditorModeTools().GetActiveModeTyped<FControlRigEditMode>(FControlRigEditMode::ModeName))
	{
		ControlRigEditMode->OnNodesSelected().RemoveAll(this);
	}
}

void FHumanRigDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TArray< TWeakObjectPtr<UObject> > Objects;
	
	// initialize input boxes
	NodeInputs.Reset(MAXNODEINPUT);
	NodeInputs.AddDefaulted(MAXNODEINPUT);

	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() == 0 || Objects.Num() > 1)
	{
		// for now no support on multi selection
		return;
	}

	CurrentlySelectedObject = Cast<class UHumanRig>(Objects[0].Get());

	CurrentEditMode = CurrentlySelectedObject->IsTemplate()? EControlRigEditoMode::EditMode : EControlRigEditoMode::InputMode;

	if (CurrentEditMode == EControlRigEditoMode::InputMode)
	{
		if (FControlRigEditMode* ControlRigEditMode = GLevelEditorModeTools().GetActiveModeTyped<FControlRigEditMode>(FControlRigEditMode::ModeName))
		{
			ControlRigEditMode->OnNodesSelected().AddSP(this, &FHumanRigDetails::HandleNodesSelected);
		}
	}

	// property visibility check happens in the ControlRigEditMode

	TSharedPtr<SHorizontalBox> ImportMeshBox;
	SAssignNew(ImportMeshBox, SHorizontalBox);

	if (CurrentEditMode == EControlRigEditoMode::EditMode)
	{
		ImportMeshBox->AddSlot()
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(USkeletalMesh::StaticClass())
				.OnObjectChanged(this, &FHumanRigDetails::SetCurrentMesh)
				.ObjectPath(this, &FHumanRigDetails::GetCurrentlySelectedMesh)
			];

		ImportMeshBox->AddSlot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("HumanRig_ImportMeshButton", "Import Selected Mesh.."))
				.ToolTipText(LOCTEXT("HumanRig_ImportMeshTooltip", "Import Mesh from the SkeletalMesh. This will clear all the existing nodes and restart."))
				.OnClicked(this, &FHumanRigDetails::ImportMesh)
			];

		// build tree view
		SAssignNew(TreeView, STreeView<TSharedPtr<FNodeNameInfo>>)
			.TreeItemsSource(&SkeletonTreeInfo)
			.OnGenerateRow(this, &FHumanRigDetails::MakeTreeRowWidget)
			.OnGetChildren(this, &FHumanRigDetails::GetChildrenForInfo)
			.OnSelectionChanged(this, &FHumanRigDetails::OnSelectionChanged)
			.OnContextMenuOpening(this, &FHumanRigDetails::OnContextMenuOpening)
			.SelectionMode(ESelectionMode::Multi);
	}
	else
	{
		// build tree view for input mode
		SAssignNew(TreeView, STreeView<TSharedPtr<FNodeNameInfo>>)
			.TreeItemsSource(&SkeletonTreeInfo)
			.OnGenerateRow(this, &FHumanRigDetails::MakeTreeRowWidget)
			.OnGetChildren(this, &FHumanRigDetails::GetChildrenForInfo)
			.OnSelectionChanged(this, &FHumanRigDetails::OnSelectionChanged);
	}

	if(CurrentEditMode == EControlRigEditoMode::EditMode)
	{
		// technically this should only allow to customize in 
		// add to set up category
		IDetailCategoryBuilder& SetUpCategory = DetailBuilder.EditCategory("Nodes");
		SetUpCategory.AddCustomRow(LOCTEXT("HumanRig_ImportMesh", "Mesh"))
		.WholeRowWidget
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSearchBox)
				.SelectAllTextWhenFocused(true)
				.OnTextChanged(this, &FHumanRigDetails::OnFilterTextChanged)
				.HintText(LOCTEXT("HumanRig_SearchNode", "Search..."))
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(500)
				.MinDesiredHeight(100)
				.MaxDesiredHeight(500)
				.Content()
				[
					TreeView->AsShared()
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
			]

			+ SVerticalBox::Slot()
			.Padding(2)
			.AutoHeight()
			[
				ImportMeshBox.ToSharedRef()
			]
		];

		RebuildTree();

		// add update constraint sections
		IDetailCategoryBuilder& ConstraintsCategory = DetailBuilder.EditCategory("Constraints");
		ConstraintsCategory.AddCustomRow(LOCTEXT("HumanRig_ConstraintsRow", "UpdateAction"))
		[
			SNew(SButton)
			.Text(LOCTEXT("HumanRig_Constraints_UpdateButton", "Update Constraints"))
			.OnClicked(this, &FHumanRigDetails::UpdateConstraintsClicked)
		];
	}

	CreateCommandList();
}

FReply FHumanRigDetails::UpdateConstraintsClicked()
{
	if (CurrentlySelectedObject.IsValid())
	{
		const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_UpdateConstraints", "Update Constraints"));
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		ControlRig->Modify();
		ControlRig->UpdateConstraints();
		// this is for constraint update in the future
		RebuildTree();
	}

	return FReply::Handled();
}

FReply FHumanRigDetails::ImportMesh()
{
	if (CurrentlySelectedAssetData.IsValid())
	{
		// warn users for overriding data
		USkeletalMesh* SelectedMesh = CastChecked<USkeletalMesh>(CurrentlySelectedAssetData.GetAsset());
		
		if (CurrentlySelectedObject.IsValid())
		{
			const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_ImportMesh", "ImportMesh"));
			UHumanRig* ControlRig = CurrentlySelectedObject.Get();
			ControlRig->Modify();
			ControlRig->BuildHierarchyFromSkeletalMesh(SelectedMesh);
		}

		RebuildTree();
	}

	return FReply::Handled();
}

void FHumanRigDetails::SetCurrentMesh(const FAssetData& InAssetData)
{
	CurrentlySelectedAssetData = InAssetData;
}

FString FHumanRigDetails::GetCurrentlySelectedMesh() const
{ 
	return CurrentlySelectedAssetData.ObjectPath.ToString();  
}

TSharedRef<ITableRow> FHumanRigDetails::MakeTreeRowWidget(TSharedPtr<FNodeNameInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FNodeNameInfo>>, OwnerTable)
		.Content()
		[
			SNew(STextBlock)
			.HighlightText(FilterText)
			.Text(InInfo->DisplayName)
		];
}

void FHumanRigDetails::GetChildrenForInfo(TSharedPtr<FNodeNameInfo> InInfo, TArray< TSharedPtr<FNodeNameInfo> >& OutChildren)
{
	OutChildren = InInfo->Children;
}

void FHumanRigDetails::OnFilterTextChanged(const FText& InFilterText)
{
	FilterText = InFilterText;

	RebuildTree();
}

void FHumanRigDetails::OnSelectionChanged(TSharedPtr<FNodeNameInfo> NodeInfo, ESelectInfo::Type SelectInfo)
{
	//Because we recreate all our items on tree refresh we will get a spurious null selection event initially.
	if (NodeInfo.IsValid() && CurrentlySelectedObject.IsValid())
	{
		 // only when inputmode
		if ( EControlRigEditoMode::InputMode == CurrentEditMode )
		{
			if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
			{
				ControlRigEditMode->SetNodeSelection(NodeInfo->NodeName, TreeView->IsItemSelected(NodeInfo));
			}
		}
	}
}

void FHumanRigDetails::RebuildTree()
{
	SkeletonTreeInfo.Empty();
	SkeletonTreeInfoFlat.Empty();

	if (CurrentlySelectedObject.IsValid())
	{
		UHumanRig* HumanRig = CurrentlySelectedObject.Get();

		const FAnimationHierarchy& Hierarchy = HumanRig->GetHierarchy();
		const int32 MaxNode = Hierarchy.GetNum();

		// we have to do this in two pass since there is no guarantee that it will have its parent yet
		// just add without inserting as child, 
		// in the second pass, it will add to parent if exists
		for (int32 NodeIdx = 0; NodeIdx < MaxNode; ++NodeIdx)
		{
			const FName NodeName = Hierarchy.GetNodeName(NodeIdx);
			const UControlManipulator* Manipulator = HumanRig->FindManipulator(NodeName);
			const bool bHasManipulator = Manipulator != nullptr;
			// we don't want to see manipulator displayname when editing trees
			const FText DisplayName = (Manipulator && CurrentEditMode == EControlRigEditoMode::InputMode) ? Manipulator->DisplayName : FText::FromName(NodeName);
			TSharedRef<FNodeNameInfo> NodeInfo = MakeShareable(new FNodeNameInfo(NodeName, DisplayName));

			// Filter if Necessary
			if (!FilterText.IsEmpty() && !NodeInfo->NodeName.ToString().Contains(FilterText.ToString()))
			{
				continue;
			}

			if (CurrentEditMode == EControlRigEditoMode::InputMode && bHasManipulator == false)
			{
				// in the input mode, only display the ones that have manipulator, it's too confusing to see everything
				continue;
			}

			SkeletonTreeInfoFlat.Add(NodeInfo);
			TreeView->SetItemExpansion(NodeInfo, true);
		}

		// second pass where it adds to parent
		// have to save this because we're adding new nodes inside of this loop (constraints)
		int32 TotalNode = SkeletonTreeInfoFlat.Num();
		for (int32 TreeIndex = 0; TreeIndex < TotalNode; ++TreeIndex)
		{
			TSharedPtr<FNodeNameInfo> NodeInfo = SkeletonTreeInfoFlat[TreeIndex];
			FName NodeName = NodeInfo->NodeName;
			int32 NodeIndex = Hierarchy.GetNodeIndex(NodeName);
			FName ParentName = Hierarchy.GetParentName(NodeName);
			int32 ParentIndex = INDEX_NONE;

			if (ParentName != NAME_None && FilterText.IsEmpty())
			{
				// We have a parent, search for it in the flat list
				for (int32 FlatListIdx = 0; FlatListIdx < SkeletonTreeInfoFlat.Num(); ++FlatListIdx)
				{
					TSharedPtr<FNodeNameInfo> InfoEntry = SkeletonTreeInfoFlat[FlatListIdx];
					if (InfoEntry->NodeName == ParentName)
					{
						ParentIndex = FlatListIdx;
						break;
					}
				}

				if (ParentIndex!=INDEX_NONE)
				{
					SkeletonTreeInfoFlat[ParentIndex]->Children.Add(NodeInfo);
				}
				else
				{
					SkeletonTreeInfo.Add(NodeInfo);
				}
			}
			else
			{
				SkeletonTreeInfo.Add(NodeInfo);
			}

			const FConstraintNodeData& NodeData = Hierarchy.GetNodeData<FConstraintNodeData>(NodeIndex);
			const TArray<FTransformConstraint>& Constraints = NodeData.GetConstraints();
			for (int32 ConstraintId = 0; ConstraintId < Constraints.Num(); ++ConstraintId)
			{
				TSharedRef<FNodeNameInfo> ConstraintNodeInfo = MakeShareable(new FNodeNameInfo(NodeName, Constraints[ConstraintId]));
				SkeletonTreeInfoFlat.Add(ConstraintNodeInfo);

				// add to children
				SkeletonTreeInfoFlat[TreeIndex]->Children.Add(ConstraintNodeInfo);
			}
		}
	}

	TreeView->RequestTreeRefresh();
}

void FHumanRigDetails::CreateCommandList()
{
	const FHumanRigNodeCommand& Commands = FHumanRigNodeCommand::Get();
	CommandList = MakeShareable(new FUICommandList);

	CommandList->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnDeleteNodeSelected)
	);

	CommandList->MapAction(
		Commands.AddManipulator,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnAddManipulator)
		);

	CommandList->MapAction(
		Commands.SetupLimb,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnSetupLimb)
	);

	CommandList->MapAction(
		Commands.SetupSpine,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnSetupSpine)
	);

	CommandList->MapAction(
		Commands.SetupFingers,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnSetupFingers)
	);

	CommandList->MapAction(
		Commands.AddNode,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnAddNode)
		);

	CommandList->MapAction(
		Commands.AddFKNode,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnAddFKNode)
	);

	CommandList->MapAction(
		Commands.Reparent,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnReparent)
	);

	CommandList->MapAction(
		Commands.ClearRotation,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnClearRotation)
	);

	CommandList->MapAction(
		Commands.SetTranslation,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnSetTranslation)
	);

	CommandList->MapAction(
		Commands.RenameNode,
		FExecuteAction::CreateSP(this, &FHumanRigDetails::OnRenameNode)
	);
}

void FHumanRigDetails::OnClearRotation()
{
	if (CurrentlySelectedObject.IsValid())
	{
		const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_ClearRotation", "Clear Rotation"));
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		ControlRig->Modify();

		FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
		TArray<TSharedPtr<FNodeNameInfo>> SelectedItems = TreeView->GetSelectedItems();
		for (int32 Index = 0; Index < SelectedItems.Num(); ++Index)
		{
			FName NodeName = SelectedItems[Index]->NodeName;
			int32 NodeIndex = Hierarchy.GetNodeIndex(NodeName);
			FTransform Transform = Hierarchy.GetGlobalTransform(NodeIndex);
			Transform.SetRotation(FQuat::Identity);
			Hierarchy.SetGlobalTransform(NodeIndex, Transform);
		}

		RebuildTree();
	}
}

void FHumanRigDetails::OnSetTranslation()
{
	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (Parent.IsValid() && CurrentlySelectedObject.IsValid())
	{
		FName NodeName = GetFirstSelectedNodeName();
		NodeInputs[0] = FText::FromName(NodeName);
	
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
		if (Hierarchy.Contains(NodeName))
		{
			InputTranslation = Hierarchy.GetGlobalTransformByName(NodeName).GetTranslation();

			FSlateApplication::Get().PushMenu(
				Parent.ToSharedRef(),
				FWidgetPath(),
				CreateSetTranslation(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
		}
	}
}

void FHumanRigDetails::SetTranslation(FName NodeName, const FVector& Translation)
{
	if (CurrentlySelectedObject.IsValid())
	{
		const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_SetTranslation", "Set Translation"));
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		ControlRig->Modify();

		FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
		if (Hierarchy.Contains(NodeName))
		{
			FTransform CurrentTransform = Hierarchy.GetGlobalTransformByName(NodeName);
			CurrentTransform.SetTranslation(Translation);

			Hierarchy.SetGlobalTransformByName(NodeName, CurrentTransform);
		}
	}
}

void FHumanRigDetails::OnRenameNode()
{
	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (Parent.IsValid() && CurrentlySelectedObject.IsValid())
	{
		FName NodeName = GetFirstSelectedNodeName();
		NodeInputs[0] = FText::FromName(NodeName);
		NodeInputs[1] = FText::FromName(NodeName);

		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
		if (Hierarchy.Contains(NodeName))
		{
			FSlateApplication::Get().PushMenu(
				Parent.ToSharedRef(),
				FWidgetPath(),
				CreateRenameNode(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
		}
	}
}

void FHumanRigDetails::RenameNode(const FName& OldName, const FName& NewName)
{
	if (CurrentlySelectedObject.IsValid() && OldName != NewName)
	{
		const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_RenameNode", "Rename Node"));
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		ControlRig->Modify();
		if (ControlRig->RenameNode(OldName, NewName))
		{
			RebuildTree();
		}
	}
}

void FHumanRigDetails::OnAddManipulator()
{
	if (CurrentlySelectedObject.IsValid())
	{
		FName CurrentlySelectedNodeName = GetFirstSelectedNodeName();
		if (CurrentlySelectedNodeName != NAME_None)
		{
			const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_AddManipulator", "Add Manipulator"));
			UHumanRig* ControlRig = CurrentlySelectedObject.Get();
			ControlRig->Modify();

			FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();

			// @FIXME: allow differnet classes on creation
			ControlRig->AddManipulator(USphereManipulator::StaticClass(), FText::FromName(CurrentlySelectedNodeName), CurrentlySelectedNodeName, NAME_None);

			RebuildTree();
		}
	}
}

void FHumanRigDetails::OnDeleteNodeSelected()
{
	if (CurrentlySelectedObject.IsValid())
	{
		const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_DeleteNode", "Delete Node(s)"));
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		ControlRig->Modify();

		FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
		TArray<TSharedPtr<FNodeNameInfo>> SelectedItems = TreeView->GetSelectedItems();
		for (int32 Index = 0; Index < SelectedItems.Num(); ++Index)
		{
			FName NodeName = SelectedItems[Index]->NodeName;
			if (SelectedItems[Index]->bIsConstraint)
			{
				ControlRig->DeleteConstraint(NodeName, SelectedItems[Index]->CachedConstraint.TargetNode);
			}
			else
			{
				// @todo ideally you should removed linked node also or optional
				ControlRig->DeleteNode(NodeName);
			}
		}

		RebuildTree();
	}
}

TSharedRef<SWidget> FHumanRigDetails::CreateSetupLimbMenu() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	// get the curve data
	if (CurrentlySelectedObject.IsValid())
	{
		MenuBuilder.BeginSection("HumanRigDetails_SetupLimbLabel", LOCTEXT("SetupLimb_Heading", "Setup Limb"));
		{
			MenuBuilder.AddWidget(
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SetupLimb_Property", "Property"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetLimbPropertyText)
						.OnTextCommitted(this, &FHumanRigDetails::OnLimbPropertyTextCommitted)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SetupLimb_Upper", "Upper Part"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 0)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 0)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SetupLimb_Middle", "Middle Part"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 1)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 1)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SetupLimb_Lower", "Lower Part"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 2)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 2)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SButton)
					.Text(LOCTEXT("HumanRig_SetupLimbButton", "Setup"))
					.OnClicked(this, &FHumanRigDetails::SetupLimbButtonClicked)
				]
				,
				FText()
				);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FHumanRigDetails::CreateAddNodeMenu() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	// get the curve data
	if (CurrentlySelectedObject.IsValid())
	{
		MenuBuilder.BeginSection("HumanRigDetails_AddNodeLabel", LOCTEXT("AddNode_Heading", "Add New Node"));
		{
			MenuBuilder.AddWidget(
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AddNode_Parent", "Parent"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 0)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 0)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AddNode_New", "New"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 1)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 1)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AddNode_Translation", "Translation"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.Padding(2, 0)
						[
							SNew(SNumericEntryBox<float>)
							.AllowSpin(true)
							.Value(this, &FHumanRigDetails::OnGet_TransX_EntryBoxValue)
							.OnValueChanged(this, &FHumanRigDetails::On_TransX_EntryBoxChanged)
						]

						+ SHorizontalBox::Slot()
						.Padding(2, 0)
						[
							SNew(SNumericEntryBox<float>)
							.AllowSpin(true)
							.Value(this, &FHumanRigDetails::OnGet_TransY_EntryBoxValue)
							.OnValueChanged(this, &FHumanRigDetails::On_TransY_EntryBoxChanged)
						]

						+ SHorizontalBox::Slot()
						.Padding(2, 0)
						[
							SNew(SNumericEntryBox<float>)
							.AllowSpin(true)
							.Value(this, &FHumanRigDetails::OnGet_TransZ_EntryBoxValue)
							.OnValueChanged(this, &FHumanRigDetails::On_TransZ_EntryBoxChanged)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SButton)
					.Text(LOCTEXT("HumanRig_AddNodeButton", "Add"))
					.OnClicked(this, &FHumanRigDetails::AddNodeButtonClicked)
				]
				,
				FText()
			);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FHumanRigDetails::CreateSetTranslation() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	// get the curve data
	if (CurrentlySelectedObject.IsValid())
	{
		MenuBuilder.BeginSection("HumanRigDetails_SetTranslationLabel", LOCTEXT("SetTranslation_Heading", "Add New Node"));
		{
			MenuBuilder.AddWidget(
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SetTranslation_Node", "Node"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 0)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 0)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SetTranslation_Translation", "Translation"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.Padding(2, 0)
						[
							SNew(SNumericEntryBox<float>)
							.AllowSpin(true)
							.Value(this, &FHumanRigDetails::OnGet_TransX_EntryBoxValue)
							.OnValueChanged(this, &FHumanRigDetails::On_TransX_EntryBoxChanged)
						]

						+ SHorizontalBox::Slot()
						.Padding(2, 0)
						[
							SNew(SNumericEntryBox<float>)
							.AllowSpin(true)
							.Value(this, &FHumanRigDetails::OnGet_TransY_EntryBoxValue)
							.OnValueChanged(this, &FHumanRigDetails::On_TransY_EntryBoxChanged)
						]

						+ SHorizontalBox::Slot()
						.Padding(2, 0)
						[
							SNew(SNumericEntryBox<float>)
							.AllowSpin(true)
							.Value(this, &FHumanRigDetails::OnGet_TransZ_EntryBoxValue)
							.OnValueChanged(this, &FHumanRigDetails::On_TransZ_EntryBoxChanged)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SButton)
					.Text(LOCTEXT("HumanRig_SetTranslationButton", "Set"))
					.OnClicked(this, &FHumanRigDetails::SetTranslationButtonClicked)
				]
				,
				FText()
			);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FHumanRigDetails::CreateRenameNode() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	// get the curve data
	if (CurrentlySelectedObject.IsValid())
	{
		MenuBuilder.BeginSection("HumanRigDetails_RenameNodeLabel", LOCTEXT("RenameNode_Heading", "Add New Node"));
		{
			MenuBuilder.AddWidget(
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RenameNode_Node", "Old Name"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 0)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 0)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RenameNode_Node", "New Name"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 1)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 1)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SButton)
					.Text(LOCTEXT("HumanRig_RenameNodeButton", "Rename"))
					.OnClicked(this, &FHumanRigDetails::RenameNodeButtonClicked)
				]
				,
				FText()
			);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}
TSharedRef<SWidget> FHumanRigDetails::CreateReparentMenu() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	// get the curve data
	if (CurrentlySelectedObject.IsValid())
	{
		MenuBuilder.BeginSection("HumanRigDetails_ReparentLabel", LOCTEXT("Reparent_Heading", "Change Parent"));
		{
			MenuBuilder.AddWidget(
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Reparent_NodeName", "Node Name"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 0)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
						.IsReadOnly(true)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Reparent_Parent", "New Parent"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 1)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 1)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SButton)
					.Text(LOCTEXT("HumanRig_ReparentButton", "Change"))
					.OnClicked(this, &FHumanRigDetails::ReparentButtonClicked)
				]
				,
				FText()
				);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}
TSharedRef<SWidget> FHumanRigDetails::CreateSetupSpineMenu() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	// get the curve data
	if (CurrentlySelectedObject.IsValid())
	{
		MenuBuilder.BeginSection("HumanRigDetails_SetupSpineLabel", LOCTEXT("SetupSpine_Heading", "Add Spine"));
		{
			MenuBuilder.AddWidget(
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SetupSpine_0", "Top Node"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 0)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 0)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SetupSpine_1", "End Node"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(SEditableTextBox)
						.Text(this, &FHumanRigDetails::OnGetNodeInputText, 1)
						.OnTextCommitted(this, &FHumanRigDetails::OnNodeInputTextCommitted, 1)
						.SelectAllTextWhenFocused(true)
						.RevertTextOnEscape(true)
						.MinDesiredWidth(30.f)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3)
				[
					SNew(SButton)
					.Text(LOCTEXT("HumanRig_SetupSpineButton", "Setup"))
					.OnClicked(this, &FHumanRigDetails::SetupSpineButtonClicked)
				]
				,
				FText()
				);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

FReply FHumanRigDetails::RenameNodeButtonClicked()
{
	RenameNode(FName(*NodeInputs[0].ToString()), FName(*NodeInputs[1].ToString()));
	FSlateApplication::Get().DismissAllMenus();
	return FReply::Handled();
}

FReply FHumanRigDetails::SetTranslationButtonClicked()
{
	SetTranslation(FName(*NodeInputs[0].ToString()), InputTranslation);
	FSlateApplication::Get().DismissAllMenus();
	return FReply::Handled();
}

FReply FHumanRigDetails::AddNodeButtonClicked()
{
	AddNode(FName(*NodeInputs[0].ToString()), FName(*NodeInputs[1].ToString()), InputTranslation);
	FSlateApplication::Get().DismissAllMenus();
	return FReply::Handled();
}

FReply FHumanRigDetails::SetupLimbButtonClicked()
{
	SetupLimb(FName(*LimbProperty.ToString()), FName(*NodeInputs[0].ToString()), FName(*NodeInputs[1].ToString()), FName(*NodeInputs[2].ToString()));
	FSlateApplication::Get().DismissAllMenus();
	return FReply::Handled();
}

FReply FHumanRigDetails::SetupSpineButtonClicked()
{
	SetupSpine(FName(*NodeInputs[0].ToString()), FName(*NodeInputs[1].ToString()));
	FSlateApplication::Get().DismissAllMenus();
	return FReply::Handled();
}

FReply FHumanRigDetails::ReparentButtonClicked()
{
	if (CurrentlySelectedObject.IsValid())
	{
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
		FName NodeName = FName(*NodeInputs[0].ToString());
		FName ParentName = FName(*NodeInputs[1].ToString());

		if (Hierarchy.Contains(NodeName) && (ParentName == NAME_None || Hierarchy.Contains(ParentName)))
		{
			ControlRig->SetParent(NodeName, ParentName);
			RebuildTree();
		}
	}

	FSlateApplication::Get().DismissAllMenus();
	return FReply::Handled();
}

void FHumanRigDetails::OnSetupLimb()
{
	// Create context menu
	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (Parent.IsValid())
	{
		NodeInputs[0] = FText::FromName(GetFirstSelectedNodeName());
		NodeInputs[1] = FText::GetEmpty();
		NodeInputs[2] = FText::GetEmpty();
		FSlateApplication::Get().PushMenu(
			Parent.ToSharedRef(),
			FWidgetPath(),
			CreateSetupLimbMenu(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
	}
}

void FHumanRigDetails::OnSetupSpine()
{
	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (Parent.IsValid())
	{
		NodeInputs[0] = FText::FromName(GetFirstSelectedNodeName());
		NodeInputs[1] = FText::GetEmpty();

		FSlateApplication::Get().PushMenu(
			Parent.ToSharedRef(),
			FWidgetPath(),
			CreateSetupSpineMenu(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
	}
}

void FHumanRigDetails::OnSetupFingers()
{
	if (CurrentlySelectedObject.IsValid())
	{
		FName SourceNodeName = GetFirstSelectedNodeName();
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();

		// finger names
		const TArray<FFingerDescription>& FingerDescription = ControlRig->FingerDescription;

		auto SetupFingers = [this](FPoseKey& InNewKey, const FFingerDescription& InFingerDescription, const FAnimationHierarchy& InHierarchy)
		{
			TArray<FName> NodeNames = InFingerDescription.GetNodeNames();

			for (int32 Index = 0; Index < NodeNames.Num(); ++Index)
			{
				FName NodeName = NodeNames[Index];
				if (InHierarchy.Contains(NodeName))
				{
					InNewKey.TransformKeys.Add(NodeName);
				}
			}
		};

		const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_SetUpFingers", "Set up Fingers"));
		ControlRig->Modify();
		const FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();

		// add pose container data - it's a lot of data
		// this is almost hard coded
		// add 3 joints per each finger, and _l, and _r for the index
		// index_01_l for the first left index finger
		for (int32 FingerIndex = 0; FingerIndex < FingerDescription.Num(); ++FingerIndex)
		{
			const FFingerDescription& Finger = FingerDescription[FingerIndex];
			FPoseKey& NewKey = ControlRig->KeyedPoses.FindOrAdd(Finger.PoseName);
			NewKey.TransformKeys.Reset();
			SetupFingers(NewKey, Finger, Hierarchy);
		}
	}
}

void FHumanRigDetails::OnAddNode()
{
	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (Parent.IsValid())
	{
		NodeInputs[0] = FText::FromName(GetFirstSelectedNodeName());
		NodeInputs[1] = FText::GetEmpty();
		InputTranslation = FVector::ZeroVector;

		FSlateApplication::Get().PushMenu(
			Parent.ToSharedRef(),
			FWidgetPath(),
			CreateAddNodeMenu(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
	}
}

void FHumanRigDetails::OnAddFKNode()
{
	if (CurrentlySelectedObject.IsValid())
	{
		FName SourceNodeName = GetFirstSelectedNodeName();
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
		if (Hierarchy.Contains(SourceNodeName))
		{
			const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_AddFKNode", "Add FK Node"));
			ControlRig->Modify();
			// @todo for now we only add to identity
			FName NewGroupName = FName(*FString(SourceNodeName.ToString() + TEXT("_FKGrp")));
			FName NewCtrlName;

			FTransform NewNodeTransform = ControlRig->GetGlobalTransform(SourceNodeName);
			ControlRig->AddCtrlGroupNode(NewGroupName, NewCtrlName, NAME_None, NewNodeTransform, SourceNodeName);

			RebuildTree();
		}
	}
}

void FHumanRigDetails::OnReparent()
{
	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (Parent.IsValid())
	{
		FName NodeName = GetFirstSelectedNodeName();
		FName ParentName = CurrentlySelectedObject->GetHierarchy().GetParentName(NodeName);

		NodeInputs[0] = FText::FromName(NodeName);
		NodeInputs[1] = FText::FromName(ParentName);

		FSlateApplication::Get().PushMenu(
			Parent.ToSharedRef(),
			FWidgetPath(),
			CreateReparentMenu(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
	}
}

void FHumanRigDetails::AddNode(FName ParentName, FName NewNodeName, const FVector& Translation)
{
	if (CurrentlySelectedObject.IsValid())
	{
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
		if (!Hierarchy.Contains(NewNodeName) && (ParentName == NAME_None || Hierarchy.Contains(ParentName)))
		{
			const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_AddNode", "Add Node"));
			ControlRig->Modify();
			// @todo for now we only add to identity
			ControlRig->AddNode(NewNodeName, ParentName, FTransform(Translation));
			RebuildTree();
		}
		else
		{
			// failed
		}
	}
}

void FHumanRigDetails::SetupSpine(FName TopNode, FName EndNode)
{
	if (CurrentlySelectedObject.IsValid())
	{
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
 		FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
 		if (Hierarchy.Contains(TopNode) && Hierarchy.Contains(EndNode))
 		{
 			const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_SetupSpine", "Set up Spine"));
 			ControlRig->Modify();
 			ControlRig->SetupSpine(TopNode, EndNode);
 			RebuildTree();
 		}
 		else
 		{
 			// failed
 		}
	}
}


void FHumanRigDetails::SetupLimb(FName PropertyName, FName Upper, FName Middle, FName Lower)
{
	if (CurrentlySelectedObject.IsValid())
	{
		UHumanRig* ControlRig = CurrentlySelectedObject.Get();
		UProperty* Property = ControlRig->GetClass()->FindPropertyByName(PropertyName);
		if (Property)
		{
			FLimbControl& LimbControl = *Property->ContainerPtrToValuePtr<FLimbControl>(ControlRig);

			FAnimationHierarchy& Hierarchy = ControlRig->GetHierarchy();
			if (Hierarchy.Contains(Upper) && Hierarchy.Contains(Middle) && Hierarchy.Contains(Lower))
			{
				// if only contains, give users warning
				const FScopedTransaction Transaction(LOCTEXT("HumanRigDetail_SetupLimb", "Setup Limb"));
				ControlRig->Modify();
				ControlRig->SetupLimb(LimbControl, Upper, Middle, Lower);
				RebuildTree();
			}
		}

	}
}

FName FHumanRigDetails::GetFirstSelectedNodeName() const
{
	TArray<TSharedPtr<FNodeNameInfo>> SelectedItems = TreeView->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		return SelectedItems[0]->NodeName;
	}

	return NAME_None;
}

TSharedPtr<SWidget> FHumanRigDetails::OnContextMenuOpening() const
{
	FMenuBuilder MenuBuilder(true, CommandList.ToSharedRef());

	if (CurrentEditMode == EControlRigEditoMode::EditMode)
	{
		bool bAnySelected = TreeView->GetSelectedItems().Num() > 0;
		bool bOneSelected = TreeView->GetSelectedItems().Num() == 1;

		MenuBuilder.BeginSection("Edit", LOCTEXT("Edit", "Edit"));
		{
			MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().AddNode);
			if (bAnySelected)
			{
				MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().AddFKNode);
				MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().Reparent);
				MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().AddManipulator);
			}

			MenuBuilder.AddMenuSeparator();
			MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().SetupLimb);
			MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().SetupSpine);
			MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().SetupFingers);

			if (bAnySelected)
			{
				MenuBuilder.AddMenuSeparator();
				MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
			}

			MenuBuilder.AddMenuSeparator();
			MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().ClearRotation);

			if (bAnySelected)
			{
				MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().SetTranslation);
			}

			if (bOneSelected)
			{
				MenuBuilder.AddMenuEntry(FHumanRigNodeCommand::Get().RenameNode);
			}
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void FHumanRigDetails::HandleNodesSelected(const TArray<FName>& NodeNames)
{
	TreeView->ClearSelection();

	for (const FName& NodeName : NodeNames)
	{
		for (TSharedPtr<FNodeNameInfo> NodeNameInfo : SkeletonTreeInfoFlat)
		{
			if (NodeNameInfo->NodeName == NodeName)
			{
				TreeView->SetItemSelection(NodeNameInfo, true);
				break;
			}
		}
	}
}

FHumanRigDetails::FNodeNameInfo::FNodeNameInfo(FName InName, const FTransformConstraint& InCachedConstraint) 
	: NodeName(InName)
	, bIsConstraint(true)
{
	CachedConstraint = InCachedConstraint;
	TArray<FStringFormatArg> Args;
	Args.Add(CachedConstraint.TargetNode.ToString());
	Args.Add(LexicalConversion::ToString(CachedConstraint.Weight));
	FString DisplayString = FString::Format(TEXT("--[Target node] {0} : W({1}) : "), Args);

	if (CachedConstraint.Operator.bParent)
	{
		DisplayString += TEXT("P");
	}
	else
	{
		if (CachedConstraint.Operator.bTranslation)
		{
			DisplayString += TEXT("T");
		}

		if (CachedConstraint.Operator.bRotation)
		{
			DisplayString += TEXT("R");
		}

		if (CachedConstraint.Operator.bScale)
		{
			DisplayString += TEXT("S");
		}
	}

	DisplayName = FText::FromString(DisplayString);
}
#undef LOCTEXT_NAMESPACE
