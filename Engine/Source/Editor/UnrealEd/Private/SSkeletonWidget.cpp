// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSkeletonWidget.h"
#include "Modules/ModuleManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Editor.h"
#include "Animation/AnimSet.h"
#include "Interfaces/IMainFrameModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "IDocumentation.h"
#include "Animation/Rig.h"
#include "AnimPreviewInstance.h"
#include "AssetRegistryModule.h"
#include "AnimationRuntime.h"
#include "Settings/SkeletalMeshEditorSettings.h"

#define LOCTEXT_NAMESPACE "SkeletonWidget"

void SSkeletonListWidget::Construct(const FArguments& InArgs)
{
	CurSelectedSkeleton = nullptr;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SSkeletonListWidget::SkeletonSelectionChanged);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
	AssetPickerConfig.bShowPathInColumnView = true;
	AssetPickerConfig.bShowTypeInColumnView = false;

	this->ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectSkeletonLabel", "Select Skeleton: "))
			]

			+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
				[
					SNew(SBorder)
					.Content()
					[
						ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
					]
				]

			+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
				.Expose(BoneListSlot)

		];

	// Construct the BoneListSlot by clearing the skeleton selection. 
	SkeletonSelectionChanged(FAssetData());
}

void SSkeletonListWidget::SkeletonSelectionChanged(const FAssetData& AssetData)
{
	BoneList.Empty();
	CurSelectedSkeleton = Cast<USkeleton>(AssetData.GetAsset());

	if (CurSelectedSkeleton != nullptr)
	{
		const FReferenceSkeleton& RefSkeleton = CurSelectedSkeleton->GetReferenceSkeleton();

		for (int32 I=0; I<RefSkeleton.GetNum(); ++I)
		{
			BoneList.Add( MakeShareable(new FName(RefSkeleton.GetBoneName(I))) ) ;
		}

		(*BoneListSlot)
			[
				SNew(SBorder) .Padding(2)
				.Content()
				[
					SNew(SListView< TSharedPtr<FName> >)
					.OnGenerateRow(this, &SSkeletonListWidget::GenerateSkeletonBoneRow)
					.ListItemsSource(&BoneList)
					.HeaderRow
					(
					SNew(SHeaderRow)
					+SHeaderRow::Column(TEXT("Bone Name"))
					.DefaultLabel(NSLOCTEXT("SkeletonWidget", "BoneName", "Bone Name"))
					)
				]
			];
	}
	else
	{
		(*BoneListSlot)
			[
				SNew(SBorder) .Padding(2)
				.Content()
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SkeletonWidget", "NoSkeletonIsSelected", "No skeleton is selected!"))
				]
			];
	}
}

void SSkeletonCompareWidget::Construct(const FArguments& InArgs)
{
	const UObject* Object = InArgs._Object;

	CurSelectedSkeleton = nullptr;
	BoneNames = *InArgs._BoneNames;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SSkeletonCompareWidget::SkeletonSelectionChanged);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
	AssetPickerConfig.bShowPathInColumnView = true;
	AssetPickerConfig.bShowTypeInColumnView = false;

	TSharedPtr<SToolTip> SkeletonTooltip = IDocumentation::Get()->CreateToolTip(FText::FromString("Pick a skeleton for this mesh"), nullptr, FString("Shared/Editors/Persona"), FString("Skeleton"));

	this->ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight() 
				.Padding(2) 
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
				 
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CurrentlySelectedSkeletonLabel_SelectSkeleton", "Select Skeleton"))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
						.ToolTip(SkeletonTooltip)
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						IDocumentation::Get()->CreateAnchor(FString("Engine/Animation/Skeleton"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight() 
				.Padding(2, 10)
				.HAlign(HAlign_Fill)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]

				+SVerticalBox::Slot()
				.AutoHeight() 
				.Padding(2) 
				.HAlign(HAlign_Fill)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CurrentlySelectedSkeletonLabel", "Currently Selected : "))
				]
				+SVerticalBox::Slot()
				.AutoHeight() 
				.Padding(2) 
				.HAlign(HAlign_Fill)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Object->GetFullName()))
				]
			]

			+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
				[
					SNew(SBorder)
					.Content()
					[
						ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
					]
				]

			+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
				.Expose(BonePairSlot)
		];

	// Construct the BonePairSlot by clearing the skeleton selection. 
	SkeletonSelectionChanged(FAssetData());
}

void SSkeletonCompareWidget::SkeletonSelectionChanged(const FAssetData& AssetData)
{
	BonePairList.Empty();
	CurSelectedSkeleton = Cast<USkeleton>(AssetData.GetAsset());

	if (CurSelectedSkeleton != nullptr)
	{
		for (int32 I=0; I<BoneNames.Num(); ++I)
		{
			if ( CurSelectedSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneNames[I]) != INDEX_NONE )
			{
				BonePairList.Add( MakeShareable(new FBoneTrackPair(BoneNames[I], BoneNames[I])) );
			}
			else
			{
				BonePairList.Add( MakeShareable(new FBoneTrackPair(BoneNames[I], TEXT(""))) );
			}
		}

		(*BonePairSlot)
			[
				SNew(SBorder) .Padding(2)
				.Content()
				[
					SNew(SListView< TSharedPtr<FBoneTrackPair> >)
					.OnGenerateRow(this, &SSkeletonCompareWidget::GenerateBonePairRow)
					.ListItemsSource(&BonePairList)
					.HeaderRow
					(
					SNew(SHeaderRow)
					+SHeaderRow::Column(TEXT("Curretly Selected"))
					.DefaultLabel(NSLOCTEXT("SkeletonWidget", "CurrentlySelected", "Currently Selected"))
					+SHeaderRow::Column(TEXT("Target Skeleton Bone"))
					.DefaultLabel(NSLOCTEXT("SkeletonWidget", "TargetSkeletonBone", "Target Skeleton Bone"))
					)
				]
			];
	}
	else
	{
		(*BonePairSlot)
			[
				SNew(SBorder) .Padding(2)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoSkeletonSelectedLabel", "No skeleton is selected!"))
				]
			];
	}
}

void SSkeletonSelectorWindow::Construct(const FArguments& InArgs)
{
	UObject* Object = InArgs._Object;
	WidgetWindow = InArgs._WidgetWindow;
	SelectedSkeleton = nullptr;
	if (Object == nullptr)
	{
		ConstructWindow();
	}
	else if (Object->IsA(USkeletalMesh::StaticClass()))
	{
		ConstructWindowFromMesh(CastChecked<USkeletalMesh>(Object));
	}
	else if (Object->IsA(UAnimSet::StaticClass()))
	{
		ConstructWindowFromAnimSet(CastChecked<UAnimSet>(Object));
	}
}

void SSkeletonSelectorWindow::ConstructWindowFromAnimSet(UAnimSet* InAnimSet)
{
	TArray<FName>  *TrackNames = &InAnimSet->TrackBoneNames;

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		+SVerticalBox::Slot() 
		.FillHeight(1) 
		.Padding(2)
		[
			SAssignNew(SkeletonWidget, SSkeletonCompareWidget)
			.Object(InAnimSet)
			.BoneNames(TrackNames)	
		];

	ConstructButtons(ContentBox);

	ChildSlot
		[
			ContentBox
		];
}

void SSkeletonSelectorWindow::ConstructWindowFromMesh(USkeletalMesh* InSkeletalMesh)
{
	TArray<FName>  BoneNames;

	for (int32 I=0; I<InSkeletalMesh->RefSkeleton.GetRawBoneNum(); ++I)
	{
		BoneNames.Add(InSkeletalMesh->RefSkeleton.GetBoneName(I));
	}

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
		[
			SAssignNew(SkeletonWidget, SSkeletonCompareWidget)
			.Object(InSkeletalMesh)
			.BoneNames(&BoneNames)
		];

	ConstructButtons(ContentBox);

	ChildSlot
		[
			ContentBox
		];
}

void SSkeletonSelectorWindow::ConstructWindow()
{
	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		+SVerticalBox::Slot() .FillHeight(1) .Padding(2)
		[
			SAssignNew(SkeletonWidget, SSkeletonListWidget)
		];

	ConstructButtons(ContentBox);

	ChildSlot
		[
			ContentBox
		];
}

TSharedPtr<SWindow> SAnimationRemapSkeleton::DialogWindow;

bool SAnimationRemapSkeleton::OnShouldFilterAsset(const struct FAssetData& AssetData)
{
	USkeleton* AssetSkeleton = nullptr;
	if (AssetData.IsAssetLoaded())
	{
		AssetSkeleton = Cast<USkeleton>(AssetData.GetAsset());
	}

	// do not show same skeleton
	if (OldSkeleton && OldSkeleton == AssetSkeleton)
	{
		return true;
	}

	if (bShowOnlyCompatibleSkeletons)
	{
		if(OldSkeleton && OldSkeleton->GetRig())
		{
			URig * Rig = OldSkeleton->GetRig();

			const FString Value = AssetData.GetTagValueRef<FString>(USkeleton::RigTag);

			if(Rig->GetFullName() == Value)
			{
				return false;
			}

			// if loaded, check to see if it has same rig
			if (AssetData.IsAssetLoaded())
			{
				USkeleton* LoadedSkeleton = Cast<USkeleton>(AssetData.GetAsset());

				if (LoadedSkeleton && LoadedSkeleton->GetRig() == Rig)
				{
					return false;
				}
			}
		}

		return true;
	}

	return false;
}

void SAnimationRemapSkeleton::UpdateAssetPicker()
{
	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAnimationRemapSkeleton::OnAssetSelectedFromPicker);
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SAnimationRemapSkeleton::OnShouldFilterAsset);
	AssetPickerConfig.bShowPathInColumnView = true;
	AssetPickerConfig.bShowTypeInColumnView = false;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	if (AssetPickerBox.IsValid())
	{
		AssetPickerBox->SetContent(
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		);
	}
}

void SAnimationRemapSkeleton::Construct( const FArguments& InArgs )
{
	OldSkeleton = InArgs._CurrentSkeleton;
	NewSkeleton = nullptr;
	WidgetWindow = InArgs._WidgetWindow;
	bRemapReferencedAssets = true;
	bConvertSpaces = false;
	bShowOnlyCompatibleSkeletons = false;
	OnRetargetAnimationDelegate = InArgs._OnRetargetDelegate;
	bShowDuplicateAssetOption = InArgs._ShowDuplicateAssetOption;

	TSharedRef<SVerticalBox> RetargetWidget = SNew(SVerticalBox);

	RetargetWidget->AddSlot()
	[
		SNew(STextBlock)
		.AutoWrapText(true)
		.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.SmallBoldFont"))
		.Text(LOCTEXT("RetargetBasePose_OptionLabel", "Retarget Options"))
	];

	if(InArgs._ShowRemapOption)
	{
		RetargetWidget->AddSlot()
		[
			SNew(SCheckBox)
			.IsChecked(this, &SAnimationRemapSkeleton::IsRemappingReferencedAssets)
			.OnCheckStateChanged(this, &SAnimationRemapSkeleton::OnRemappingReferencedAssetsChanged)
			[
				SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_RemapAssets", "Remap referenced assets "))
			]
		];

		bRemapReferencedAssets = true;

		if(InArgs._ShowExistingRemapOption)
		{
			RetargetWidget->AddSlot()
				[
					SNew(SCheckBox)
					.IsChecked(this, &SAnimationRemapSkeleton::IsRemappingToExistingAssetsChecked)
					.IsEnabled(this, &SAnimationRemapSkeleton::IsRemappingToExistingAssetsEnabled)
					.OnCheckStateChanged(this, &SAnimationRemapSkeleton::OnRemappingToExistingAssetsChanged)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RemapSkeleton_RemapToExisting", "Allow remapping to existing assets"))
					]
				];

			// Not by default, user must specify
			bAllowRemappingToExistingAssets = false;
		}
	}

	if (InArgs._ShowConvertSpacesOption)
	{
		TSharedPtr<SToolTip> ConvertSpaceTooltip = IDocumentation::Get()->CreateToolTip(FText::FromString("Check if you'd like to convert animation data to new skeleton space. If this is false, it won't convert any animation data to new space."),
			nullptr, FString("Shared/Editors/Persona"), FString("AnimRemapSkeleton_ConvertSpace"));
		RetargetWidget->AddSlot()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SAnimationRemapSkeleton::IsConvertSpacesChecked)
				.OnCheckStateChanged(this, &SAnimationRemapSkeleton::OnConvertSpacesCheckChanged)
				[
					SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_ConvertSpaces", "Convert Spaces to new Skeleton"))
					.ToolTip(ConvertSpaceTooltip)
				]
			];

		bConvertSpaces = true;
	}

	if(InArgs._ShowCompatibleDisplayOption)
	{
		TSharedPtr<SToolTip> ConvertSpaceTooltip = IDocumentation::Get()->CreateToolTip(FText::FromString("Check if you'd like to show only the skeleton that uses the same rig."),
			nullptr, FString("Shared/Editors/Persona"), FString("AnimRemapSkeleton_ShowCompatbielSkeletons")); // @todo add tooltip
		RetargetWidget->AddSlot()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SAnimationRemapSkeleton::IsShowOnlyCompatibleSkeletonsChecked)
				.IsEnabled(this, &SAnimationRemapSkeleton::IsShowOnlyCompatibleSkeletonsEnabled)
				.OnCheckStateChanged(this, &SAnimationRemapSkeleton::OnShowOnlyCompatibleSkeletonsCheckChanged)
				[
					SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_ShowCompatible", "Show Only Compatible Skeletons"))
					.ToolTip(ConvertSpaceTooltip)
				]
			];

		bShowOnlyCompatibleSkeletons = true;
	}

	TSharedRef<SHorizontalBox> OptionWidget = SNew(SHorizontalBox);
	OptionWidget->AddSlot()
		[
			RetargetWidget
		];

	if (bShowDuplicateAssetOption)
	{
		TSharedRef<SVerticalBox> NameOptionWidget = SNew(SVerticalBox);

		NameOptionWidget->AddSlot()
		[	
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2, 3)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.SmallBoldFont"))
				.Text(LOCTEXT("RetargetBasePose_RenameLable", "New Asset Name"))
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2, 1)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_DupeName_Prefix", "Prefix"))
				]

				+SHorizontalBox::Slot()
				[
					SNew(SEditableTextBox)
						.Text(this, &SAnimationRemapSkeleton::GetPrefixName)
						.MinDesiredWidth(100)
						.OnTextChanged(this, &SAnimationRemapSkeleton::SetPrefixName)
						.IsReadOnly(false)
						.RevertTextOnEscape(true)
				]

			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2, 1)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_DupeName_Suffix", "Suffix"))
				]

				+SHorizontalBox::Slot()
				[
					SNew(SEditableTextBox)
						.Text(this, &SAnimationRemapSkeleton::GetSuffixName)
						.MinDesiredWidth(100)
						.OnTextChanged(this, &SAnimationRemapSkeleton::SetSuffixName)
						.IsReadOnly(false)
						.RevertTextOnEscape(true)
				]

			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2, 1)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_DupeName_ReplaceFrom", "Replace "))
				]

				+SHorizontalBox::Slot()
				[
					SNew(SEditableTextBox)
						.Text(this, &SAnimationRemapSkeleton::GetReplaceFrom)
						.MinDesiredWidth(50)
						.OnTextChanged(this, &SAnimationRemapSkeleton::SetReplaceFrom)
						.IsReadOnly(false)
						.RevertTextOnEscape(true)
				]

				+SHorizontalBox::Slot()
				.Padding(5, 0)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("RemapSkeleton_DupeName_ReplaceTo", "with "))
				]

				+SHorizontalBox::Slot()
				[
					SNew(SEditableTextBox)
						.Text(this, &SAnimationRemapSkeleton::GetReplaceTo)
						.MinDesiredWidth(50)
						.OnTextChanged(this, &SAnimationRemapSkeleton::SetReplaceTo)
						.IsReadOnly(false)
						.RevertTextOnEscape(true)
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2, 3)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(5, 5)
				[
					SNew(STextBlock).Text(this,  &SAnimationRemapSkeleton::GetExampleText)
						.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.ItalicFont"))
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2, 3)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RemapSkeleton_DupeName_Folder", "Folder "))
						.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.SmallBoldFont"))
				]

				+SHorizontalBox::Slot()
				.FillWidth(1)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(this, &SAnimationRemapSkeleton::GetFolderPath)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("RemapSkeleton_DupeName_ShowFolderOption", "Change..."))
					.OnClicked(this, &SAnimationRemapSkeleton::ShowFolderOption)
				]
			]
		];

		OptionWidget->AddSlot()
		[
			NameOptionWidget
		];
	}

	TSharedPtr<SToolTip> SkeletonTooltip = IDocumentation::Get()->CreateToolTip(FText::FromString("Pick a skeleton for this mesh"), nullptr, FString("Shared/Editors/Persona"), FString("Skeleton"));

	this->ChildSlot
		[
			SNew (SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2)
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CurrentlySelectedSkeletonLabel_SelectSkeleton", "Select Skeleton"))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
						.ToolTip(SkeletonTooltip)
					]
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						IDocumentation::Get()->CreateAnchor(FString("Engine/Animation/Skeleton"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.FilterFont"))
					.Text(InArgs._WarningMessage)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SSeparator)
				]

				+SVerticalBox::Slot()
				.MaxHeight(500)
				[
					SAssignNew(AssetPickerBox, SBox)
					.WidthOverride(400)
					.HeightOverride(300)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SSeparator)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton).HAlign(HAlign_Center)
						.Text(LOCTEXT("RemapSkeleton_Apply", "Retarget"))
						.IsEnabled(this, &SAnimationRemapSkeleton::CanApply)
						.OnClicked(this, &SAnimationRemapSkeleton::OnApply)
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					]
					+SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton).HAlign(HAlign_Center)
						.Text(LOCTEXT("RemapSkeleton_Cancel", "Cancel"))
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SAnimationRemapSkeleton::OnCancel)
					]
				]
			]

			+SHorizontalBox::Slot()
			.Padding(2)
			.AutoWidth()
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5, 5)
				[
					// put nice message here
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.FilterFont"))
					.ColorAndOpacity(FLinearColor::Red)
					.Text(LOCTEXT("RetargetBasePose_WarningMessage", "*Make sure you have the similar retarget base pose. \nIf they don't look alike here, you can edit your base pose in the Retarget Manager window."))
				]

				+SVerticalBox::Slot()
				.FillHeight(1)
				.Padding(0, 5)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SourceSkeleteonTitle", "[Source]"))
							.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.BoldFont"))
							.AutoWrapText(true)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(5, 5)
						[
							SAssignNew(SourceViewport, SBasePoseViewport)
							.Skeleton(OldSkeleton)
						]
					]

					+SHorizontalBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TargetSkeleteonTitle", "[Target]"))
							.Font(FEditorStyle::GetFontStyle("Persona.RetargetManager.BoldFont"))
							.AutoWrapText(true)
						]

						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(5, 5)
						[
							SAssignNew(TargetViewport, SBasePoseViewport)
							.Skeleton(nullptr)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5, 5)
					[
						OptionWidget
					]
				]
			]
		];

	UpdateAssetPicker();
	UpdateExampleText();
}

FReply SAnimationRemapSkeleton::ShowFolderOption()
{
	TSharedRef<SSelectFolderDlg> NewAnimDlg =
	SNew(SSelectFolderDlg);

	if(NewAnimDlg->ShowModal() != EAppReturnType::Cancel)
	{
		NameDuplicateRule.FolderPath = NewAnimDlg->GetAssetPath();
	}

	if (WidgetWindow.IsValid())
	{
		WidgetWindow.Pin()->BringToFront(true);
	}

	return FReply::Handled();
}

void SAnimationRemapSkeleton::UpdateExampleText()
{
	FString ReplaceFrom = FString::Printf(TEXT("Old Name : ###%s###"), *NameDuplicateRule.ReplaceFrom);
	FString ReplaceTo = FString::Printf(TEXT("New Name : %s###%s###%s"), *NameDuplicateRule.Prefix, *NameDuplicateRule.ReplaceTo, *NameDuplicateRule.Suffix);

	ExampleText = FText::FromString(FString::Printf(TEXT("%s\n%s"), *ReplaceFrom, *ReplaceTo));
}

ECheckBoxState SAnimationRemapSkeleton::IsRemappingReferencedAssets() const
{
	return bRemapReferencedAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimationRemapSkeleton::OnRemappingReferencedAssetsChanged(ECheckBoxState InNewRadioState)
{
	bRemapReferencedAssets = (InNewRadioState == ECheckBoxState::Checked);
}

ECheckBoxState SAnimationRemapSkeleton::IsRemappingToExistingAssetsChecked() const
{
	return bAllowRemappingToExistingAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SAnimationRemapSkeleton::IsRemappingToExistingAssetsEnabled() const
{
	return bRemapReferencedAssets;
}

void SAnimationRemapSkeleton::OnRemappingToExistingAssetsChanged(ECheckBoxState InNewRadioState)
{
	bAllowRemappingToExistingAssets = (InNewRadioState == ECheckBoxState::Checked);
}

ECheckBoxState SAnimationRemapSkeleton::IsConvertSpacesChecked() const
{
	return bConvertSpaces ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimationRemapSkeleton::OnConvertSpacesCheckChanged(ECheckBoxState InNewRadioState)
{
	bConvertSpaces = (InNewRadioState == ECheckBoxState::Checked);
}

bool SAnimationRemapSkeleton::IsShowOnlyCompatibleSkeletonsEnabled() const
{
	// if convert space is false, compatible skeletons won't matter either. 
	return (bConvertSpaces == true);
}

ECheckBoxState SAnimationRemapSkeleton::IsShowOnlyCompatibleSkeletonsChecked() const
{
	return bShowOnlyCompatibleSkeletons ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimationRemapSkeleton::OnShowOnlyCompatibleSkeletonsCheckChanged(ECheckBoxState InNewRadioState)
{
	bShowOnlyCompatibleSkeletons = (InNewRadioState == ECheckBoxState::Checked);

	UpdateAssetPicker();
}

bool SAnimationRemapSkeleton::CanApply() const
{
	return (NewSkeleton!=nullptr && NewSkeleton!=OldSkeleton);
}

void SAnimationRemapSkeleton::OnAssetSelectedFromPicker(const FAssetData& AssetData)
{
	if (AssetData.GetAsset())
	{
		NewSkeleton = Cast<USkeleton>(AssetData.GetAsset());

		TargetViewport->SetSkeleton(NewSkeleton);
	}
}

FReply SAnimationRemapSkeleton::OnApply()
{
	if (OnRetargetAnimationDelegate.IsBound())
	{
		OnRetargetAnimationDelegate.Execute(OldSkeleton, NewSkeleton, bRemapReferencedAssets, bAllowRemappingToExistingAssets, bConvertSpaces, bShowDuplicateAssetOption ? &NameDuplicateRule : nullptr);
	}

	CloseWindow();
	return FReply::Handled();
}

FReply SAnimationRemapSkeleton::OnCancel()
{
	NewSkeleton = nullptr;
	CloseWindow();
	return FReply::Handled();
}

void SAnimationRemapSkeleton::OnRemapDialogClosed(const TSharedRef<SWindow>& Window)
{
	NewSkeleton = nullptr;
	DialogWindow = nullptr;
}

void SAnimationRemapSkeleton::CloseWindow()
{
	if ( WidgetWindow.IsValid() )
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
}

void SAnimationRemapSkeleton::ShowWindow(USkeleton* OldSkeleton, const FText& WarningMessage, bool bDuplicateAssets, FOnRetargetAnimation RetargetDelegate)
{
	if(DialogWindow.IsValid())
	{
		FSlateApplication::Get().DestroyWindowImmediately(DialogWindow.ToSharedRef());
	}

	DialogWindow = SNew(SWindow)
		.Title( LOCTEXT("RemapSkeleton", "Select Skeleton") )
		.SupportsMinimize(false) .SupportsMaximize(false)
		.SizingRule( ESizingRule::Autosized );

	TSharedPtr<class SAnimationRemapSkeleton> DialogWidget;

	TSharedPtr<SBorder> DialogWrapper = 
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SAssignNew(DialogWidget, SAnimationRemapSkeleton)
			.CurrentSkeleton(OldSkeleton)
			.WidgetWindow(DialogWindow)
			.WarningMessage(WarningMessage)
			.ShowRemapOption(true)
			.ShowExistingRemapOption(true)
			.ShowConvertSpacesOption(OldSkeleton != nullptr)
			.ShowCompatibleDisplayOption(OldSkeleton != nullptr)
			.ShowDuplicateAssetOption(bDuplicateAssets)
			.OnRetargetDelegate(RetargetDelegate)
		];

	DialogWindow->SetOnWindowClosed(FRequestDestroyWindowOverride::CreateSP(DialogWidget.Get(), &SAnimationRemapSkeleton::OnRemapDialogClosed));
	DialogWindow->SetContent(DialogWrapper.ToSharedRef());

	FSlateApplication::Get().AddWindow(DialogWindow.ToSharedRef());
}

////////////////////////////////////////////////////

FDlgRemapSkeleton::FDlgRemapSkeleton( USkeleton* Skeleton )
{
	if (FSlateApplication::IsInitialized())
	{
		DialogWindow = SNew(SWindow)
			.Title( LOCTEXT("RemapSkeleton", "Select Skeleton") )
			.SupportsMinimize(false) .SupportsMaximize(false)
			.SizingRule( ESizingRule::Autosized );

		TSharedPtr<SBorder> DialogWrapper = 
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(DialogWidget, SAnimationRemapSkeleton)
				.CurrentSkeleton(Skeleton)
				.WidgetWindow(DialogWindow)
			];

		DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	}
}

bool FDlgRemapSkeleton::ShowModal()
{
	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());

	NewSkeleton = DialogWidget.Get()->NewSkeleton;

	return (NewSkeleton != nullptr && NewSkeleton != DialogWidget.Get()->OldSkeleton);
}

//////////////////////////////////////////////////////////////
void SRemapFailures::Construct( const FArguments& InArgs )
{
	for ( auto RemapIt = InArgs._FailedRemaps.CreateConstIterator(); RemapIt; ++RemapIt )
	{
		FailedRemaps.Add( MakeShareable( new FText(*RemapIt) ) );
	}

	ChildSlot
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("Docking.Tab.ContentAreaBrush") )
			.Padding(FMargin(4, 8, 4, 4))
			[
				SNew(SVerticalBox)

				// Title text
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock) .Text( LOCTEXT("RemapFailureTitle", "The following assets could not be Remaped.") )
				]

				// Failure list
				+SVerticalBox::Slot()
				.Padding(0, 8)
				.FillHeight(1.f)
				[
					SNew(SBorder)
					.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
					[
						SNew(SListView<TSharedRef<FText>>)
						.ListItemsSource(&FailedRemaps)
						.SelectionMode(ESelectionMode::None)
						.OnGenerateRow(this, &SRemapFailures::MakeListViewWidget)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0,0)
					[
						SNew(SButton) 
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.Text(LOCTEXT("RemapFailuresCloseButton", "Close"))
						.OnClicked(this, &SRemapFailures::CloseClicked)
					]
				]
			]
		];
}

void SRemapFailures::OpenRemapFailuresDialog(const TArray<FText>& InFailedRemaps)
{
	TSharedRef<SWindow> RemapWindow = SNew(SWindow)
		.Title(LOCTEXT("FailedRemapsDialog", "Failed Remaps"))
		.ClientSize(FVector2D(800,400))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SRemapFailures).FailedRemaps(InFailedRemaps)
		];

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));

	if ( MainFrameModule.GetParentWindow().IsValid() )
	{
		FSlateApplication::Get().AddWindowAsNativeChild(RemapWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(RemapWindow);
	}
}

TSharedRef<ITableRow> SRemapFailures::MakeListViewWidget(TSharedRef<FText> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(STableRow< TSharedRef<FText> >, OwnerTable)
		[
			SNew(STextBlock) .Text( Item.Get() )
		];
}

FReply SRemapFailures::CloseClicked()
{
	FWidgetPath WidgetPath;
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared(), WidgetPath);

	if ( Window.IsValid() )
	{
		Window->RequestDestroyWindow();
	}

	return FReply::Handled();
}

void SSkeletonBoneRemoval::Construct( const FArguments& InArgs )
{
	bShouldContinue = false;
	WidgetWindow = InArgs._WidgetWindow;

	for (int32 I=0; I<InArgs._BonesToRemove.Num(); ++I)
	{
		BoneNames.Add( MakeShareable(new FName(InArgs._BonesToRemove[I])) ) ;
	}

	this->ChildSlot
	[
		SNew (SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight() 
		.Padding(2) 
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			+SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BoneRemovalLabel", "Bone Removal"))
				.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight() 
		.Padding(2, 10)
		.HAlign(HAlign_Fill)
		[
			SNew(SSeparator)
			.Orientation(Orient_Horizontal)
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(STextBlock)
			.WrapTextAt(400.f)
			.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 ) )
			.Text( InArgs._WarningMessage )
		]

		+SVerticalBox::Slot()
		.AutoHeight() 
		.Padding(5)
		[
			SNew(SSeparator)
		]

		+SVerticalBox::Slot()
		.MaxHeight(300)
		.Padding(5)
		[
			SNew(SListView< TSharedPtr<FName> >)
			.OnGenerateRow(this, &SSkeletonBoneRemoval::GenerateSkeletonBoneRow)
			.ListItemsSource(&BoneNames)
			.HeaderRow
			(
			SNew(SHeaderRow)
			+SHeaderRow::Column(TEXT("Bone Name"))
			.DefaultLabel(NSLOCTEXT("SkeletonWidget", "BoneName", "Bone Name"))
			)
		]

		+SVerticalBox::Slot()
		.AutoHeight() 
		.Padding(5)
		[
			SNew(SSeparator)
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+SUniformGridPanel::Slot(0,0)
			[
				SNew(SButton) .HAlign(HAlign_Center)
				.Text(LOCTEXT("BoneRemoval_Ok", "Ok"))
				.OnClicked(this, &SSkeletonBoneRemoval::OnOk)
				.HAlign(HAlign_Center)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
			]
			+SUniformGridPanel::Slot(1,0)
			[
				SNew(SButton) .HAlign(HAlign_Center)
				.Text(LOCTEXT("BoneRemoval_Cancel", "Cancel"))
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SSkeletonBoneRemoval::OnCancel)
			]
		]
	];
}

FReply SSkeletonBoneRemoval::OnOk()
{
	bShouldContinue = true;
	CloseWindow();
	return FReply::Handled();
}

FReply SSkeletonBoneRemoval::OnCancel()
{
	CloseWindow();
	return FReply::Handled();
}

void SSkeletonBoneRemoval::CloseWindow()
{
	if ( WidgetWindow.IsValid() )
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
}

bool SSkeletonBoneRemoval::ShowModal(const TArray<FName> BonesToRemove, const FText& WarningMessage)
{
	TSharedPtr<class SSkeletonBoneRemoval> DialogWidget;

	TSharedPtr<SWindow> DialogWindow = SNew(SWindow)
		.Title( LOCTEXT("RemapSkeleton", "Select Skeleton") )
		.SupportsMinimize(false) .SupportsMaximize(false)
		.SizingRule( ESizingRule::Autosized );

	TSharedPtr<SBorder> DialogWrapper = 
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SAssignNew(DialogWidget, SSkeletonBoneRemoval)
			.WidgetWindow(DialogWindow)
			.BonesToRemove(BonesToRemove)
			.WarningMessage(WarningMessage)
		];

	DialogWindow->SetContent(DialogWrapper.ToSharedRef());

	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());

	return DialogWidget.Get()->bShouldContinue;
}

////////////////////////////////////////

class FBasePoseViewportClient: public FEditorViewportClient
{
public:
	FBasePoseViewportClient(FPreviewScene& InPreviewScene, const TSharedRef<SBasePoseViewport>& InBasePoseViewport)
		: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InBasePoseViewport))
	{
		SetViewMode(VMI_Lit);

		// Always composite editor objects after post processing in the editor
		EngineShowFlags.SetCompositeEditorPrimitives(true);
		EngineShowFlags.DisableAdvancedFeatures();

		UpdateLighting();

		// Setup defaults for the common draw helper.
		DrawHelper.bDrawPivot = false;
		DrawHelper.bDrawWorldBox = false;
		DrawHelper.bDrawKillZ = false;
		DrawHelper.bDrawGrid = true;
		DrawHelper.GridColorAxis = FColor(70, 70, 70);
		DrawHelper.GridColorMajor = FColor(40, 40, 40);
		DrawHelper.GridColorMinor =  FColor(20, 20, 20);
		DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;

		bDisableInput = true;
	}


	// FEditorViewportClient interface
	virtual void Tick(float DeltaTime) override
	{
		if (PreviewScene)
		{
			PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaTime);
		}
	}

	virtual FSceneInterface* GetScene() const override
	{
		return PreviewScene->GetScene();
	}

	virtual FLinearColor GetBackgroundColor() const override 
	{ 
		return FLinearColor::White; 
	}

	// End of FEditorViewportClient

	void UpdateLighting()
	{
		const USkeletalMeshEditorSettings* Options = GetDefault<USkeletalMeshEditorSettings>();

		PreviewScene->SetLightDirection(Options->AnimPreviewLightingDirection);
		PreviewScene->SetLightColor(Options->AnimPreviewDirectionalColor);
		PreviewScene->SetLightBrightness(Options->AnimPreviewLightBrightness);
	}
};

////////////////////////////////
// SBasePoseViewport
void SBasePoseViewport::Construct(const FArguments& InArgs)
{
	SEditorViewport::Construct(SEditorViewport::FArguments());

	PreviewComponent = NewObject<UDebugSkelMeshComponent>();
	PreviewComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	PreviewScene.AddComponent(PreviewComponent, FTransform::Identity);

	SetSkeleton(InArgs._Skeleton);
}

void SBasePoseViewport::SetSkeleton(USkeleton* Skeleton)
{
	if(Skeleton != TargetSkeleton)
	{
		TargetSkeleton = Skeleton;

		if(TargetSkeleton)
		{
			USkeletalMesh* PreviewSkeletalMesh = Skeleton->GetPreviewMesh();
			if(PreviewSkeletalMesh)
			{
				PreviewComponent->SetSkeletalMesh(PreviewSkeletalMesh);
				PreviewComponent->EnablePreview(true, nullptr);
//				PreviewComponent->AnimScriptInstance = PreviewComponent->PreviewInstance;
				PreviewComponent->PreviewInstance->SetForceRetargetBasePose(true);
				PreviewComponent->RefreshBoneTransforms(nullptr);

				//Place the camera at a good viewer position
				FVector NewPosition = Client->GetViewLocation();
				NewPosition.Normalize();
				NewPosition *= (PreviewSkeletalMesh->GetImportedBounds().SphereRadius*1.5f);
				Client->SetViewLocation(NewPosition);
			}
			else
			{
				PreviewComponent->SetSkeletalMesh(nullptr);
			}
		}
		else
		{
			PreviewComponent->SetSkeletalMesh(nullptr);
		}

		Client->Invalidate();
	}
}

SBasePoseViewport::SBasePoseViewport()
: PreviewScene(FPreviewScene::ConstructionValues())
{
}

bool SBasePoseViewport::IsVisible() const
{
	return true;
}

TSharedRef<FEditorViewportClient> SBasePoseViewport::MakeEditorViewportClient()
{
	TSharedPtr<FEditorViewportClient> EditorViewportClient = MakeShareable(new FBasePoseViewportClient(PreviewScene, SharedThis(this)));

	EditorViewportClient->ViewportType = LVT_Perspective;
	EditorViewportClient->bSetListenerPosition = false;
	EditorViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	EditorViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

	EditorViewportClient->SetRealtime(false);
	EditorViewportClient->VisibilityDelegate.BindSP(this, &SBasePoseViewport::IsVisible);
	EditorViewportClient->SetViewMode(VMI_Lit);

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SBasePoseViewport::MakeViewportToolbar()
{
	return nullptr;
}


/////////////////////////////////////////////////
// select folder dialog
//////////////////////////////////////////////////
void SSelectFolderDlg::Construct(const FArguments& InArgs)
{
	AssetPath = FText::FromString(FPackageName::GetLongPackagePath(InArgs._DefaultAssetPath.ToString()));

	if(AssetPath.IsEmpty())
	{
		AssetPath = FText::FromString(TEXT("/Game"));
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath.ToString();
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SSelectFolderDlg::OnPathChange);
	PathPickerConfig.bAddDefaultPath = true;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("SSelectFolderDlg_Title", "Create New Animation Object"))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		//.SizingRule( ESizingRule::Autosized )
		.ClientSize(FVector2D(450, 450))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot() // Add user input block
			.Padding(2)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectPath", "Select Path"))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14))
					]

					+SVerticalBox::Slot()
					.FillHeight(1)
					.Padding(3)
					[
						ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
					]
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(5)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("OK", "OK"))
					.OnClicked(this, &SSelectFolderDlg::OnButtonClick, EAppReturnType::Ok)
				]
				+SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SSelectFolderDlg::OnButtonClick, EAppReturnType::Cancel)
				]
			]
		]);
}

void SSelectFolderDlg::OnPathChange(const FString& NewPath)
{
	AssetPath = FText::FromString(NewPath);
}

FReply SSelectFolderDlg::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	RequestDestroyWindow();

	return FReply::Handled();
}

EAppReturnType::Type SSelectFolderDlg::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

FString SSelectFolderDlg::GetAssetPath()
{
	return AssetPath.ToString();
}

//////////////////////////////////////////////////////////////////////////
// Window for remapping assets when retargetting
TSharedPtr<SWindow> SAnimationRemapAssets::DialogWindow;

void SAnimationRemapAssets::Construct(const FArguments& InArgs)
{
	NewSkeleton = InArgs._NewSkeleton;
	RetargetContext = InArgs._RetargetContext;
	
	TSharedPtr<SVerticalBox> ListBox;

	TArray<UObject*> Duplicates = RetargetContext->GetAllDuplicates();

	for(UObject* Asset : Duplicates)
	{
		// We don't want to add anim blueprints here, just animation assets
		if(Asset->GetClass() != UAnimBlueprint::StaticClass())
		{
			AssetListInfo.Add(FDisplayedAssetEntryInfo::Make(Asset, NewSkeleton));
		}
	}

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(5.0f)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("RemapAssetsDescription", "The assets shown below need to be duplicated or remapped for the new blueprint. Select a new animation to use in the new animation blueprint for each asset or leave blank to duplicate the existing asset."))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.Padding(5.0f)
		.AutoHeight()
		.MaxHeight(500.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			[
				SAssignNew(ListWidget, SRemapAssetEntryList)
				.ItemHeight(20.0f)
				.ListItemsSource(&AssetListInfo)
				.OnGenerateRow(this, &SAnimationRemapAssets::OnGenerateMontageReferenceRow)
				.SelectionMode(ESelectionMode::None)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("AssetName")
					.DefaultLabel(LOCTEXT("ColumnLabel_RemapAssetName", "Asset Name"))
					+ SHeaderRow::Column("AssetType")
					.DefaultLabel(LOCTEXT("ColumnLabel_RemapAssetType", "Asset Type"))
					+ SHeaderRow::Column("AssetRemap")
					.DefaultLabel(LOCTEXT("ColumnLabel_RemapAssetRemap", "Remapped Asset"))
				)
			]
		]
		+ SVerticalBox::Slot()
		.Padding(5.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.FillWidth(0.2f)
			[
				SNew(SButton)
				.ContentPadding(2.0f)
				.OnClicked(this, &SAnimationRemapAssets::OnBestGuessClicked)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BestGuessButton", "Auto-Fill Using Best Guess"))
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT("BestGuessDescription", "Auto-Fill will look at the names of all compatible assets for the new skeleton and look for something similar to use for the remapped asset."))
			]
		]

		+ SVerticalBox::Slot()
		.Padding(5.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Content()
			[
				SNew(SButton)
				.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				.OnClicked(this, &SAnimationRemapAssets::OnOkClicked)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OkButton", "OK"))
				]
			]
		]
	];
}

void SAnimationRemapAssets::ShowWindow(FAnimationRetargetContext& RetargetContext, USkeleton* RetargetToSkeleton)
{
	if(DialogWindow.IsValid())
	{
		FSlateApplication::Get().DestroyWindowImmediately(DialogWindow.ToSharedRef());
	}
	
	DialogWindow = SNew(SWindow)
		.Title(LOCTEXT("RemapAssets", "Choose Assets to Remap"))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.HasCloseButton(false)
		.MaxWidth(1024.0f)
		.SizingRule(ESizingRule::Autosized);
	
	TSharedPtr<class SAnimationRemapAssets> DialogWidget;
	
	TSharedPtr<SBorder> DialogWrapper =
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SAssignNew(DialogWidget, SAnimationRemapAssets)
				.NewSkeleton(RetargetToSkeleton)
				.RetargetContext(&RetargetContext)
		];
	
	DialogWindow->SetOnWindowClosed(FRequestDestroyWindowOverride::CreateSP(DialogWidget.Get(), &SAnimationRemapAssets::OnDialogClosed));
	DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	
	FSlateApplication::Get().AddModalWindow(DialogWindow.ToSharedRef(), nullptr);
}

TSharedRef<ITableRow> SAnimationRemapAssets::OnGenerateMontageReferenceRow(TSharedPtr<FDisplayedAssetEntryInfo> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SAssetEntryRow, OwnerTable)
		.DisplayedInfo(Item);
}

void SAnimationRemapAssets::OnDialogClosed(const TSharedRef<SWindow>& Window)
{
	DialogWindow = nullptr;
}

FReply SAnimationRemapAssets::OnOkClicked()
{
	for(TSharedPtr<FDisplayedAssetEntryInfo> AssetInfo : AssetListInfo)
	{
		if(AssetInfo->RemapAsset)
		{
			RetargetContext->AddRemappedAsset(Cast<UAnimationAsset>(AssetInfo->AnimAsset), Cast<UAnimationAsset>(AssetInfo->RemapAsset));
		}
	}

	DialogWindow->RequestDestroyWindow();

	return FReply::Handled();
}

FReply SAnimationRemapAssets::OnBestGuessClicked()
{
	// collect all compatible assets
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FString SkeletonName = FAssetData(NewSkeleton).GetExportTextName();

	TArray<FAssetData> CompatibleAssets;
	TArray<FAssetData> AssetDataList;

	AssetRegistryModule.Get().GetAssetsByClass(UAnimationAsset::StaticClass()->GetFName(), AssetDataList, true);

	for(const FAssetData& Data : AssetDataList)
	{
		if(Data.GetTagValueRef<FString>("Skeleton") == SkeletonName)
		{
			CompatibleAssets.Add(Data);
		}
	}

	if(CompatibleAssets.Num() > 0)
	{
		// Do best guess analysis for the assets based on name.
		for(TSharedPtr<FDisplayedAssetEntryInfo>& Info : AssetListInfo)
		{
			const FAssetData* BestMatchData = FindBestGuessMatch(FAssetData(Info->AnimAsset), CompatibleAssets);
			Info->RemapAsset = BestMatchData ? BestMatchData->GetAsset() : nullptr;
		}
	}

	ListWidget->RequestListRefresh();

	return FReply::Handled();
}

const FAssetData* SAnimationRemapAssets::FindBestGuessMatch(const FAssetData& AssetData, const TArray<FAssetData>& PossibleAssets) const
{
	int32 LowestScore = MAX_int32;
	int32 FoundIndex = INDEX_NONE;

	for(int32 Idx = 0 ; Idx < PossibleAssets.Num() ; ++Idx)
	{
		const FAssetData& Data = PossibleAssets[Idx];

		if(Data.AssetClass == AssetData.AssetClass)
		{
			int32 Distance = FAnimationRuntime::GetStringDistance(AssetData.AssetName.ToString(), Data.AssetName.ToString());

			if(Distance < LowestScore)
			{
				LowestScore = Distance;
				FoundIndex = Idx;
			}
		}
	}

	if(FoundIndex != INDEX_NONE)
	{
		return &PossibleAssets[FoundIndex];
	}

	return nullptr;
}

void SAssetEntryRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	check(InArgs._DisplayedInfo.IsValid());
	DisplayedInfo = InArgs._DisplayedInfo;

	SkeletonExportName = FAssetData(DisplayedInfo->NewSkeleton).GetExportTextName();

	SMultiColumnTableRow<TSharedPtr<FDisplayedAssetEntryInfo>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SAssetEntryRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if(ColumnName == TEXT("AssetName"))
	{
		return SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("AssetNameEntry", "{0}"), FText::FromString(DisplayedInfo->AnimAsset->GetName())));
	}
	else if(ColumnName == TEXT("AssetType"))
	{
		return SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("AssetTypeEntry", "{0}"), FText::FromString(DisplayedInfo->AnimAsset->GetClass()->GetName())));
	}
	else if(ColumnName == TEXT("AssetRemap"))
	{
		return SNew(SBox)
			.Padding(2.0f)
			[
				SNew(SComboButton)
				.ToolTipText(LOCTEXT("AssetRemapTooltip", "Select compatible asset to remap to."))
				.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent(this, &SAssetEntryRow::GetRemapMenuContent)
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FEditorStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.Text(this, &SAssetEntryRow::GetRemapMenuButtonText)
				]
			];
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SAssetEntryRow::GetRemapMenuContent()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	
	FAssetPickerConfig PickerConfig;
	PickerConfig.SelectionMode = ESelectionMode::Single;
	PickerConfig.Filter.ClassNames.Add(DisplayedInfo->AnimAsset->GetClass()->GetFName());
	PickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAssetEntryRow::OnAssetSelected);
	PickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SAssetEntryRow::OnShouldFilterAsset);
	PickerConfig.bAllowNullSelection = true;
	
	return SNew(SBox)
		.WidthOverride(384)
		.HeightOverride(768)
		[
			ContentBrowserModule.Get().CreateAssetPicker(PickerConfig)
		];
}

FText SAssetEntryRow::GetRemapMenuButtonText() const
{
	FText NameText = (DisplayedInfo->RemapAsset) ? FText::FromString(*DisplayedInfo->RemapAsset->GetName()) : LOCTEXT("AssetRemapNone", "None");

	return FText::Format(LOCTEXT("RemapButtonText", "{0}"), NameText);
}

void SAssetEntryRow::OnAssetSelected(const FAssetData& AssetData)
{
	// Close the asset picker menu
	FSlateApplication::Get().DismissAllMenus();

	RemapAsset = AssetData.GetAsset();
	DisplayedInfo->RemapAsset = RemapAsset.Get();
}

bool SAssetEntryRow::OnShouldFilterAsset(const FAssetData& AssetData) const
{
	if(AssetData.GetTagValueRef<FString>("Skeleton") == SkeletonExportName)
	{
		return false;
	}

	return true;
}

TSharedRef<FDisplayedAssetEntryInfo> FDisplayedAssetEntryInfo::Make(UObject* InAsset, USkeleton* InNewSkeleton)
{
	return MakeShareable(new FDisplayedAssetEntryInfo(InAsset, InNewSkeleton));
}

FDisplayedAssetEntryInfo::FDisplayedAssetEntryInfo(UObject* InAsset, USkeleton* InNewSkeleton)
	: NewSkeleton(InNewSkeleton)
	, AnimAsset(InAsset)
	, RemapAsset(nullptr)
{

}

#undef LOCTEXT_NAMESPACE 
