// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SRigWindow.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "ReferenceSkeleton.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SButton.h"
#include "AssetNotifications.h"
#include "Animation/Rig.h"
#include "BoneSelectionWidget.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "SRigPicker.h"
#include "BoneMappingHelper.h"
#include "SSkeletonWidget.h"
#include "IEditableSkeleton.h"

class FPersona;

#define LOCTEXT_NAMESPACE "SRigWindow"

DECLARE_DELEGATE_TwoParams(FOnBoneMappingChanged, FName /** NodeName */, FName /** BoneName **/);
DECLARE_DELEGATE_RetVal_OneParam(FName, FOnGetBoneMapping, FName /** Node Name **/);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// SRigWindow

void SRigWindow::Construct(const FArguments& InArgs, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnPostUndo)
{
	EditableSkeletonPtr = InEditableSkeleton;
	bDisplayAdvanced = false;

	InEditableSkeleton->RefreshRigConfig();

	ChildSlot
	[
		SNew( SVerticalBox )

		// first add rig asset picker
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RigNameLabel", "Select Rig "))
				.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.BoldFont"))
			]

			+SHorizontalBox::Slot()
			[
				SAssignNew( AssetComboButton, SComboButton )
				//.ToolTipText( this, &SPropertyEditorAsset::OnGetToolTip )
				.ButtonStyle( FEditorStyle::Get(), "PropertyEditor.AssetComboStyle" )
				.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent( this, &SRigWindow::MakeRigPickerWithMenu )
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					// Show the name of the asset or actor
					SNew(STextBlock)
					.TextStyle( FEditorStyle::Get(), "PropertyEditor.AssetClass" )
					.Font( FEditorStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
					.Text(this,&SRigWindow::GetAssetName)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(5, 5)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(5, 0)
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnAutoMapping))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("AutoMapping_Title", "Auto  Mapping"))
				.ToolTipText(LOCTEXT("AutoMapping_Tooltip", "Automatically map the best matching bones"))
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(5, 0)
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnClearMapping))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("ClearMapping_Title", "Clear Mapping"))
				.ToolTipText(LOCTEXT("ClearMapping_Tooltip", "Clear currently mapping bones"))
			]

// 			+ SHorizontalBox::Slot()
// 			.HAlign(HAlign_Right)
// 			.Padding(5, 0)
// 			[
// 				SNew(SButton)
// 				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnToggleView))
// 				.HAlign(HAlign_Center)
// 				.VAlign(VAlign_Center)
// 				.Text(LOCTEXT("HierarchyView_Title", "Show Hierarchy"))
// 				.ToolTipText(LOCTEXT("HierarchyView_Tooltip", "Show Hierarchy View"))
// 			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(5, 0)
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateSP(this, &SRigWindow::OnToggleAdvanced))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(this, &SRigWindow::GetAdvancedButtonText)
				.ToolTipText(LOCTEXT("ToggleAdvanced_Tooltip", "Toggle Base/Advanced configuration"))
			]
		]

		// now show bone mapping
		+ SVerticalBox::Slot()
		.FillHeight(1)
		.Padding(0,2)
		[
			SAssignNew(BoneMappingWidget, SBoneMappingBase, InOnPostUndo)
			.OnBoneMappingChanged(this, &SRigWindow::OnBoneMappingChanged)
			.OnGetBoneMapping(this, &SRigWindow::GetBoneMapping)
			.OnCreateBoneMapping(this, &SRigWindow::CreateBoneMappingList)
			.OnGetReferenceSkeleton(this, &SRigWindow::GetReferenceSkeleton)
		]
	];
}

void SRigWindow::CreateBoneMappingList( const FString& SearchText, TArray< TSharedPtr<FDisplayedBoneMappingInfo> >& BoneMappingList)
{
	BoneMappingList.Empty();

	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();
	const URig* Rig = Skeleton.GetRig();

	if ( Rig )
	{
		bool bDoFiltering = !SearchText.IsEmpty();
		const TArray<FNode>& Nodes = Rig->GetNodes();

		for ( const auto Node : Nodes )
		{
			const FName& Name = Node.Name;
			const FString& DisplayName = Node.DisplayName;
			const FName& BoneName = Skeleton.GetRigBoneMapping(Name);

			if (Node.bAdvanced == bDisplayAdvanced)
			{
				if(bDoFiltering)
				{
					// make sure it doens't fit any of them
					if(!Name.ToString().Contains(SearchText) && !DisplayName.Contains(SearchText) && !BoneName.ToString().Contains(SearchText))
					{
						continue; // Skip items that don't match our filter
					}
				}

				TSharedRef<FDisplayedBoneMappingInfo> Info = FDisplayedBoneMappingInfo::Make(Name, DisplayName);

				BoneMappingList.Add(Info);
			}
		}
	}
}


void SRigWindow::OnAssetSelected(UObject* Object)
{
	AssetComboButton->SetIsOpen(false);

	EditableSkeletonPtr.Pin()->SetRigConfig(Cast<URig>(Object));

	BoneMappingWidget.Get()->RefreshBoneMappingList();

	FAssetNotifications::SkeletonNeedsToBeSaved(&EditableSkeletonPtr.Pin()->GetSkeleton());
}

/** Returns true if the asset shouldn't show  */
bool SRigWindow::ShouldFilterAsset(const struct FAssetData& AssetData)
{
	return (AssetData.GetAsset() == GetRigObject());
}

URig* SRigWindow::GetRigObject() const
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();
	return Skeleton.GetRig();
}

void SRigWindow::OnBoneMappingChanged(FName NodeName, FName BoneName)
{
	EditableSkeletonPtr.Pin()->SetRigBoneMapping(NodeName, BoneName);
}

FName SRigWindow::GetBoneMapping(FName NodeName)
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();
	return Skeleton.GetRigBoneMapping(NodeName);
}

FReply SRigWindow::OnToggleAdvanced()
{
	bDisplayAdvanced = !bDisplayAdvanced;

	BoneMappingWidget.Get()->RefreshBoneMappingList();

	return FReply::Handled();
}

FText SRigWindow::GetAdvancedButtonText() const
{
	if (bDisplayAdvanced)
	{
		return LOCTEXT("ShowBase", "Show Base");
	}

	return LOCTEXT("ShowAdvanced", "Show Advanced");
}

TSharedRef<SWidget> SRigWindow::MakeRigPickerWithMenu()
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();

	// rig asset picker
	return	
		SNew(SRigPicker)
		.InitialObject(Skeleton.GetRig())
		.OnShouldFilterAsset(this, &SRigWindow::ShouldFilterAsset)
		.OnSetReference(this, &SRigWindow::OnAssetSelected)
		.OnClose(this, &SRigWindow::CloseComboButton );
}

void SRigWindow::CloseComboButton()
{
	AssetComboButton->SetIsOpen(false);
}

FText SRigWindow::GetAssetName() const
{
	URig* Rig = GetRigObject();
	if (Rig)
	{
		return FText::FromString(Rig->GetName());
	}

	return LOCTEXT("None", "None");
}

const struct FReferenceSkeleton& SRigWindow::GetReferenceSkeleton() const
{
	return EditableSkeletonPtr.Pin()->GetSkeleton().GetReferenceSkeleton();
}

bool SRigWindow::OnTargetSkeletonSelected(USkeleton* SelectedSkeleton, URig*  Rig) const
{
	if (SelectedSkeleton)
	{
		// make sure the skeleton contains all the rig node names
		const FReferenceSkeleton& RefSkeleton = SelectedSkeleton->GetReferenceSkeleton();

		if (RefSkeleton.GetNum() > 0)
		{
			const TArray<FNode> RigNodes = Rig->GetNodes();
			int32 BoneMatched = 0;

			for (const auto& RigNode : RigNodes)
			{
				if (RefSkeleton.FindBoneIndex(RigNode.Name) != INDEX_NONE)
				{
					++BoneMatched;
				}
			}

			float BoneMatchedPercentage = (float)(BoneMatched) / RefSkeleton.GetNum();
			if (BoneMatchedPercentage > 0.5f)
			{
				Rig->SetSourceReferenceSkeleton(RefSkeleton);

				return true;
			}
		}
	}

	return false;
}

bool SRigWindow::SelectSourceReferenceSkeleton(URig* Rig) const
{
	TSharedRef<SWindow> WidgetWindow = SNew(SWindow)
		.Title(LOCTEXT("SelectSourceSkeletonForRig", "Select Source Skeleton for the Rig"))
		.ClientSize(FVector2D(500, 600));

	TSharedRef<SSkeletonSelectorWindow> SkeletonSelectorWindow = SNew(SSkeletonSelectorWindow).WidgetWindow(WidgetWindow);

	WidgetWindow->SetContent(SkeletonSelectorWindow);

	GEditor->EditorAddModalWindow(WidgetWindow);
	USkeleton* RigSkeleton = SkeletonSelectorWindow->GetSelectedSkeleton();
	if (RigSkeleton)
	{
		return OnTargetSkeletonSelected(RigSkeleton, Rig);
	}

	return false;
}


FReply SRigWindow::OnAutoMapping()
{
	URig* Rig = GetRigObject();
	if (Rig)
	{
		if (!Rig->IsSourceReferenceSkeletonAvailable())
		{
			//ask if they want to set up source skeleton
			EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("TheRigNeedsSkeleton", 
				"In order to attempt to auto-map bones, the rig should have the source skeleton. However, the current rig is missing the source skeleton. Would you like to choose one? It's best to select the skeleton this rig is from."));

			if (Response == EAppReturnType::No)
			{
				return FReply::Handled();
			}

			if (!SelectSourceReferenceSkeleton(Rig))
			{
				return FReply::Handled();
			}
		}

		FReferenceSkeleton RigReferenceSkeleton = Rig->GetSourceReferenceSkeleton();
		const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();
		FBoneMappingHelper Helper(RigReferenceSkeleton, Skeleton.GetReferenceSkeleton());
		TMap<FName, FName> BestMatches;
		Helper.TryMatch(BestMatches);

		EditableSkeletonPtr.Pin()->SetRigBoneMappings(BestMatches);
		// refresh the list
		BoneMappingWidget->RefreshBoneMappingList();
	}

	return FReply::Handled();
}

FReply SRigWindow::OnClearMapping()
{
	URig* Rig = GetRigObject();
	if (Rig)
	{
		const TArray<FNode>& Nodes = Rig->GetNodes();
		TMap<FName, FName> Mappings;
		for (const auto& Node : Nodes)
		{
			Mappings.Add(Node.Name, NAME_None);
		}

		EditableSkeletonPtr.Pin()->SetRigBoneMappings(Mappings);

		// refresh the list
		BoneMappingWidget->RefreshBoneMappingList();
	}
	return FReply::Handled();
}

FReply SRigWindow::OnToggleView()
{
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

