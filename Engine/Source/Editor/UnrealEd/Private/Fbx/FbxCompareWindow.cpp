// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "FbxCompareWindow.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "EditorStyleSet.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "IDocumentation.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Factories/FbxSceneImportFactory.h"
#include "Toolkits/AssetEditorManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/SkeletalMesh.h"


#define LOCTEXT_NAMESPACE "FBXOption"

void SFbxCompareWindow::Construct(const FArguments& InArgs)
{
	CurrentDisplayOption = FMaterialCompareData::All;
	bShowSectionFlag[EFBXCompareSection_General] = true;
	bShowSectionFlag[EFBXCompareSection_Materials] = true;
	bShowSectionFlag[EFBXCompareSection_Skeleton] = true;

	WidgetWindow = InArgs._WidgetWindow;
	FullFbxPath = InArgs._FullFbxPath.ToString();
	FbxSceneInfo = InArgs._FbxSceneInfo;
	FbxGeneralInfo = InArgs._FbxGeneralInfo;
	if (InArgs._AssetReferencingSkeleton != nullptr)
	{
		//Copy the aray
		AssetReferencingSkeleton = *(InArgs._AssetReferencingSkeleton);
	}
	CurrentMeshData = InArgs._CurrentMeshData;
	FbxMeshData = InArgs._FbxMeshData;
	PreviewObject = InArgs._PreviewObject;

	FillGeneralListItem();
	FillMaterialListItem();
	if (PreviewObject->IsA(USkeletalMesh::StaticClass()))
	{
		FilSkeletonTreeItem();
	}

	SetMatchJointInfo();

	// Material comparison
	TSharedPtr<SWidget> MaterialCompareSection = ConstructMaterialComparison();
	// Skeleton comparison
	TSharedPtr<SWidget> SkeletonCompareSection = ConstructSkeletonComparison();
	// General section
	TSharedPtr<SWidget> GeneralInfoSection = ConstructGeneralInfo();

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(EOrientation::Orient_Vertical)
		.AlwaysShowScrollbar(false);

	this->ChildSlot
		[
		SNew(SBox)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							// Header with the file path
							SNew(SBorder)
							.Padding(FMargin(3))
							.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(STextBlock)
									.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
									.Text(LOCTEXT("Import_CurrentFileTitle", "Current File: "))
								]
								+SHorizontalBox::Slot()
								.Padding(5, 0, 0, 0)
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
									.Text(InArgs._FullFbxPath)
								]
							]
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						.Padding(2)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(2)
							[
								// Material Compare section
								MaterialCompareSection.ToSharedRef()
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(2)
							[
								// Skeleton Compare section
								SkeletonCompareSection.ToSharedRef()
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(2)
							[
								GeneralInfoSection.ToSharedRef()
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(2)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("SFbxCompareWindow_Preview_Done", "Done"))
				.OnClicked(this, &SFbxCompareWindow::OnDone)
			]
		]
	];	
}

FReply SFbxCompareWindow::SetSectionVisible(EFBXCompareSection SectionIndex)
{
	bShowSectionFlag[SectionIndex] = !bShowSectionFlag[SectionIndex];
	return FReply::Handled();
}

EVisibility SFbxCompareWindow::IsSectionVisible(EFBXCompareSection SectionIndex)
{
	return bShowSectionFlag[SectionIndex] ? EVisibility::All : EVisibility::Collapsed;
}

const FSlateBrush* SFbxCompareWindow::GetCollapsableArrow(EFBXCompareSection SectionIndex) const
{
	return bShowSectionFlag[SectionIndex] ? FEditorStyle::GetBrush("Symbols.DownArrow") : FEditorStyle::GetBrush("Symbols.RightArrow");
}

TSharedPtr<SWidget> SFbxCompareWindow::ConstructGeneralInfo()
{
	return SNew(SBox)
	.MaxDesiredHeight(205)
	[
		SNew(SBorder)
		.Padding(FMargin(3))
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.IsFocusable(false)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.OnClicked(this, &SFbxCompareWindow::SetSectionVisible, EFBXCompareSection_General)
					[
						SNew(SImage).Image(this, &SFbxCompareWindow::GetCollapsableArrow, EFBXCompareSection_General)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.Text(LOCTEXT("SFbxCompareWindow_GeneralInfoHeader", "Fbx File Information"))
				]
			
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SFbxCompareWindow::IsSectionVisible, EFBXCompareSection_General)))
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[
						//Show the general fbx information
						SNew(SListView<TSharedPtr<FString>>)
						.ListItemsSource(&GeneralListItem)
						.OnGenerateRow(this, &SFbxCompareWindow::OnGenerateRowGeneralFbxInfo)
					]
				]
			]
		]
	];
}

TSharedPtr<SWidget> SFbxCompareWindow::ConstructMaterialComparison()
{
	return SNew(SBox)
	.MaxDesiredHeight(500)
	[
		SNew(SBorder)
		.Padding(FMargin(3))
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.IsFocusable(false)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.OnClicked(this, &SFbxCompareWindow::SetSectionVisible, EFBXCompareSection_Materials)
					[
						SNew(SImage).Image(this, &SFbxCompareWindow::GetCollapsableArrow, EFBXCompareSection_Materials)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.Text(LOCTEXT("SFbxCompareWindow_MaterialCompareHeader", "Materials"))
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SFbxCompareWindow::IsSectionVisible, EFBXCompareSection_Materials)))
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(2)
					[
						//Show the Comparison of the meshes
						SNew(SListView< TSharedPtr<FMaterialCompareData> >)
						.ItemHeight(24)
						.ListItemsSource(&CompareMaterialListItem)
						.OnGenerateRow(this, &SFbxCompareWindow::OnGenerateRowForCompareMaterialList)
						.HeaderRow
						(
							SNew(SHeaderRow)
							+ SHeaderRow::Column("RowIndex")
								.DefaultLabel(LOCTEXT("SFbxCompareWindow_RowIndex_ColumnHeader", ""))
								.FixedWidth(25)
							+ SHeaderRow::Column("Current")
								.DefaultLabel(LOCTEXT("SFbxCompareWindow_Current_ColumnHeader", "Current Asset"))
								.FillWidth(0.5f)
							+ SHeaderRow::Column("Fbx")
								.DefaultLabel(LOCTEXT("SFbxCompareWindow_Fbx_ColumnHeader", "Reimport Asset (Preview)"))
								.FillWidth(0.5f)
						)
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2)
					[
						//Show the toggle button to display different re-import problem
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SFbxCompareWindow::ToggleMaterialDisplay, FMaterialCompareData::EMaterialCompareDisplayOption::All)
							.IsChecked(this, &SFbxCompareWindow::IsToggleMaterialDisplayChecked, FMaterialCompareData::EMaterialCompareDisplayOption::All)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SFbxCompareWindow_Display_Option_All", "All"))
							]
							
						]
						+SHorizontalBox::Slot()
						.Padding(5, 0, 0, 0)
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SFbxCompareWindow::ToggleMaterialDisplay, FMaterialCompareData::EMaterialCompareDisplayOption::NoMatch)
							.IsChecked(this, &SFbxCompareWindow::IsToggleMaterialDisplayChecked, FMaterialCompareData::EMaterialCompareDisplayOption::NoMatch)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SFbxCompareWindow_Display_Option_NoMatch", "No Match"))
								.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.3f, 0.0f)))
								.ToolTipText(LOCTEXT("SFbxCompareWindow_Display_Option_NoMatch_tooltip", "Can impact gameplay code using material slot name."))
							]
						]
						+SHorizontalBox::Slot()
						.Padding(5, 0, 0, 0)
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SFbxCompareWindow::ToggleMaterialDisplay, FMaterialCompareData::EMaterialCompareDisplayOption::IndexChanged)
							.IsChecked(this, &SFbxCompareWindow::IsToggleMaterialDisplayChecked, FMaterialCompareData::EMaterialCompareDisplayOption::IndexChanged)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SFbxCompareWindow_Display_Option_IndexChanged", "Index Changed"))
								.ColorAndOpacity(FSlateColor(FLinearColor::Yellow))
								.ToolTipText(LOCTEXT("SFbxCompareWindow_Display_Option_IndexChanged_tooltip", "Can impact gameplay code using index base material."))
							]
						]
						+SHorizontalBox::Slot()
						.Padding(5, 0, 0, 0)
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SFbxCompareWindow::ToggleMaterialDisplay, FMaterialCompareData::EMaterialCompareDisplayOption::SkinxxError)
							.IsChecked(this, &SFbxCompareWindow::IsToggleMaterialDisplayChecked, FMaterialCompareData::EMaterialCompareDisplayOption::SkinxxError)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SFbxCompareWindow_Display_Option_SkinxxError", "SkinXX Error"))
								.ColorAndOpacity(FSlateColor(FLinearColor::Red))
								.ToolTipText(LOCTEXT("SFbxCompareWindow_Display_Option_SkinxxError_tooltip", "The list of materials will not be re-order correctly."))
							]
						]
					]
				]
			]
		]
	];
}

void SFbxCompareWindow::ToggleMaterialDisplay(ECheckBoxState InNewValue, FMaterialCompareData::EMaterialCompareDisplayOption InDisplayOption)
{
	//Cannot uncheck a radio button
	if (InNewValue != ECheckBoxState::Checked)
	{
		return;
	}
	CurrentDisplayOption = InDisplayOption;
	for (TSharedPtr<FMaterialCompareData> CompareMaterial : CompareMaterialListItem)
	{
		CompareMaterial->CompareDisplayOption = CurrentDisplayOption;
	}
}

ECheckBoxState SFbxCompareWindow::IsToggleMaterialDisplayChecked(FMaterialCompareData::EMaterialCompareDisplayOption InDisplayOption) const
{
	return CurrentDisplayOption == InDisplayOption ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

TSharedRef<ITableRow> SFbxCompareWindow::OnGenerateRowGeneralFbxInfo(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	int32 GeneralListIndex = GeneralListItem.Find(InItem);
	bool LightBackgroundColor = GeneralListIndex % 2 == 1;
	return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(*(InItem.Get())))
		];
}

TSharedRef<ITableRow> SFbxCompareWindow::OnGenerateRowForCompareMaterialList(TSharedPtr<FMaterialCompareData> RowData, const TSharedRef<STableViewBase>& Table)
{
	TSharedRef< SCompareRowDataTableListViewRow > ReturnRow = SNew(SCompareRowDataTableListViewRow, Table)
		.CompareRowData(RowData);
	return ReturnRow;
}

FSlateColor FMaterialCompareData::GetCellColor(FCompMesh *DataA, int32 MaterialIndexA, int32 MaterialMatchA, FCompMesh *DataB, int32 MaterialIndexB, bool bSkinxxError) const
{
	if (!DataA->CompMaterials.IsValidIndex(MaterialIndexA))
	{
		return FSlateColor::UseForeground();
	}

	bool bMatchIndexChanged = MaterialMatchA == INDEX_NONE || (DataA->CompMaterials.IsValidIndex(MaterialIndexA) && DataB->CompMaterials.IsValidIndex(MaterialIndexB) && MaterialMatchA == MaterialIndexB);

	if ((CompareDisplayOption == NoMatch || CompareDisplayOption == All) && MaterialMatchA == INDEX_NONE)
	{
		//There is no match for this material, so it will be add to the material array
		return FSlateColor(FLinearColor(0.7f, 0.3f, 0.0f));
	}
	if ((CompareDisplayOption == IndexChanged || CompareDisplayOption == All) && !bMatchIndexChanged)
	{
		//The match index has changed, so index base gameplay will be broken
		return FSlateColor(FLinearColor::Yellow);
	}
	if ((CompareDisplayOption == SkinxxError || CompareDisplayOption == All) && bSkinxxError)
	{
		//Skinxx error
		return FSlateColor(FLinearColor::Red);
	}
	return FSlateColor::UseForeground();
}

TSharedRef<SWidget> FMaterialCompareData::ConstructCell(FCompMesh *MeshData, int32 MeshMaterialIndex, bool bSkinxxDuplicate, bool bSkinxxMissing)
{
	if (!MeshData->CompMaterials.IsValidIndex(MeshMaterialIndex))
	{
		return SNew(SBox)
			.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FMaterialCompareData_EmptyCell", ""))
			];
	}
	FString CellContent = MeshData->CompMaterials[MeshMaterialIndex].ImportedMaterialSlotName.ToString();
	FString CellTooltip = TEXT("Material Slot Name: ") + MeshData->CompMaterials[MeshMaterialIndex].MaterialSlotName.ToString();
	if (bSkinxxDuplicate)
	{
		CellTooltip += TEXT(" (skinxx duplicate)");
	}
	if (bSkinxxMissing)
	{
		CellTooltip += TEXT(" (skinxx missing)");
	}

	return SNew(SBox)
		.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(CellContent))
		.ToolTipText(FText::FromString(CellTooltip))
		.ColorAndOpacity(this, MeshData == CurrentData ? &FMaterialCompareData::GetCurrentCellColor : &FMaterialCompareData::GetFbxCellColor)
		];
}

FSlateColor FMaterialCompareData::GetCurrentCellColor() const
{
	return GetCellColor(CurrentData, CurrentMaterialIndex, CurrentMaterialMatch, FbxData, FbxMaterialIndex, bCurrentSkinxxDuplicate || bCurrentSkinxxMissing);
}

TSharedRef<SWidget> FMaterialCompareData::ConstructCellCurrent()
{
	return ConstructCell(CurrentData, CurrentMaterialIndex, bCurrentSkinxxDuplicate, bCurrentSkinxxMissing);
}

FSlateColor FMaterialCompareData::GetFbxCellColor() const
{
	return GetCellColor(FbxData, FbxMaterialIndex, FbxMaterialMatch, CurrentData, CurrentMaterialIndex, bFbxSkinxxDuplicate || bFbxSkinxxMissing);
}

TSharedRef<SWidget> FMaterialCompareData::ConstructCellFbx()
{
	return ConstructCell(FbxData, FbxMaterialIndex, bFbxSkinxxDuplicate, bFbxSkinxxMissing);
}

void SFbxCompareWindow::FillGeneralListItem()
{
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.UE4SdkVersion)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.ApplicationCreator)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.CreationDate)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.FileVersion)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.AxisSystem)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.UnitSystem)));
	GeneralListItem.Add(MakeShareable(new FString(TEXT("Unskinned Mesh Count:    ") + FString::FromInt(FbxSceneInfo->NonSkinnedMeshNum))));
	GeneralListItem.Add(MakeShareable(new FString(TEXT("Skinned Count:    ") + FString::FromInt(FbxSceneInfo->SkinnedMeshNum))));
	GeneralListItem.Add(MakeShareable(new FString(TEXT("Material Count:    ") + FString::FromInt(FbxSceneInfo->TotalMaterialNum))));
	FString HasAnimationStr = TEXT("Has Animation:    ");
	HasAnimationStr += FbxSceneInfo->bHasAnimation ? TEXT("True") : TEXT("False");
	GeneralListItem.Add(MakeShareable(new FString(HasAnimationStr)));
	if (FbxSceneInfo->bHasAnimation)
	{
		TArray<FStringFormatArg> Args;
		Args.Add(FbxSceneInfo->TotalTime);
		FString AnimationTimeStr = FString::Format(TEXT("Animation Time:    {0}"), Args);
		GeneralListItem.Add(MakeShareable(new FString(AnimationTimeStr)));

		Args.Empty();
		Args.Add(FbxSceneInfo->FrameRate);
		FString AnimationRateStr = FString::Format(TEXT("Animation Rate:    {0}"), Args);
		GeneralListItem.Add(MakeShareable(new FString(AnimationRateStr)));
	}
}

/*
* Return true if there is some skinxx error. Both array will be allocate to the size of the materials array of MeshData
*/
bool SFbxCompareWindow::FindSkinxxErrors(FCompMesh *MeshData, TArray<bool> &DuplicateSkinxxMaterialNames, TArray<bool> &MissingSkinxxSuffixeMaterialNames)
{
	MissingSkinxxSuffixeMaterialNames.Empty();
	MissingSkinxxSuffixeMaterialNames.AddZeroed(MeshData->CompMaterials.Num());
	DuplicateSkinxxMaterialNames.Empty();
	DuplicateSkinxxMaterialNames.AddZeroed(MeshData->CompMaterials.Num());
	TArray<int32> SkinxxErrorIndexes;
	bool bContainSkinxxIndex = false;
	for (FCompMaterial CompMaterial : MeshData->CompMaterials)
	{
		if (CompMaterial.ImportedMaterialSlotName == NAME_None)
		{
			continue;
		}
		FString ImportedMaterialName = CompMaterial.ImportedMaterialSlotName.ToString();
		int32 Offset = ImportedMaterialName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Offset != INDEX_NONE)
		{
			FString SkinXXNumber = ImportedMaterialName.Right(ImportedMaterialName.Len() - (Offset + 1)).RightChop(4);
			if (SkinXXNumber.IsNumeric())
			{
				bContainSkinxxIndex = true;
				break;
			}
		}
	}

	//There is no skinxx suffixe, so no skinxx error
	if (!bContainSkinxxIndex)
	{
		return false;
	}

	bool bContainSkinxxError = false;
	for (int32 MaterialNamesIndex = 0; MaterialNamesIndex < MeshData->CompMaterials.Num(); ++MaterialNamesIndex)
	{
		FName MaterialName = MeshData->CompMaterials[MaterialNamesIndex].ImportedMaterialSlotName;
		if (MaterialName == NAME_None)
		{
			MissingSkinxxSuffixeMaterialNames[MaterialNamesIndex] = true;
			bContainSkinxxError = true;
			continue;
		}

		FString ImportedMaterialName = MaterialName.ToString();
		int32 Offset = ImportedMaterialName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Offset != INDEX_NONE)
		{
			FString SkinXXNumber = ImportedMaterialName.Right(ImportedMaterialName.Len() - (Offset + 1)).RightChop(4);

			if (SkinXXNumber.IsNumeric())
			{
				int32 TmpIndex = FPlatformString::Atoi(*SkinXXNumber);
				if (SkinxxErrorIndexes.Contains(TmpIndex))
				{
					DuplicateSkinxxMaterialNames[MaterialNamesIndex] = true;
					bContainSkinxxError = true;
				}
				SkinxxErrorIndexes.Add(TmpIndex);
			}
			else
			{
				MissingSkinxxSuffixeMaterialNames[MaterialNamesIndex] = true;
				bContainSkinxxError = true;
			}
		}
		else
		{
			MissingSkinxxSuffixeMaterialNames[MaterialNamesIndex] = true;
			bContainSkinxxError = true;
		}
	}
	return bContainSkinxxError;
}

void SFbxCompareWindow::FillMaterialListItem()
{
	TArray<bool> CurrentDuplicateSkinxx;
	TArray<bool> CurrentMissingSkinxxSuffixe;
	FindSkinxxErrors(CurrentMeshData, CurrentDuplicateSkinxx, CurrentMissingSkinxxSuffixe);
	
	TArray<bool> FbxDuplicateSkinxx;
	TArray<bool> FbxMissingSkinxxSuffixe;
	FindSkinxxErrors(FbxMeshData, FbxDuplicateSkinxx, FbxMissingSkinxxSuffixe);

	//Build the compare data to show in the UI
	int32 MaterialCompareRowNumber = FMath::Max(CurrentMeshData->CompMaterials.Num(), FbxMeshData->CompMaterials.Num());
	for (int32 RowIndex = 0; RowIndex < MaterialCompareRowNumber; ++RowIndex)
	{
		TSharedPtr<FMaterialCompareData> CompareRowData = MakeShareable(new FMaterialCompareData());
		CompareRowData->RowIndex = CompareMaterialListItem.Add(CompareRowData);
		CompareRowData->CurrentData = CurrentMeshData;
		CompareRowData->FbxData = FbxMeshData;
		
		CompareRowData->bCurrentSkinxxDuplicate = CurrentDuplicateSkinxx.IsValidIndex(RowIndex) && CurrentDuplicateSkinxx[RowIndex];
		CompareRowData->bCurrentSkinxxMissing = CurrentMissingSkinxxSuffixe.IsValidIndex(RowIndex) && CurrentMissingSkinxxSuffixe[RowIndex];
		CompareRowData->bFbxSkinxxDuplicate = FbxDuplicateSkinxx.IsValidIndex(RowIndex) && FbxDuplicateSkinxx[RowIndex];
		CompareRowData->bFbxSkinxxDuplicate = FbxMissingSkinxxSuffixe.IsValidIndex(RowIndex) && FbxMissingSkinxxSuffixe[RowIndex];

		CompareRowData->CompareDisplayOption = FMaterialCompareData::All;
		if (CurrentMeshData->CompMaterials.IsValidIndex(RowIndex))
		{
			CompareRowData->CurrentMaterialIndex = RowIndex;
			for (int32 FbxMaterialIndex = 0; FbxMaterialIndex < FbxMeshData->CompMaterials.Num(); ++FbxMaterialIndex)
			{
				if (FbxMeshData->CompMaterials[FbxMaterialIndex].ImportedMaterialSlotName == CurrentMeshData->CompMaterials[RowIndex].ImportedMaterialSlotName)
				{
					CompareRowData->CurrentMaterialMatch = FbxMaterialIndex;
					break;
				}
			}
		}
		if (FbxMeshData->CompMaterials.IsValidIndex(RowIndex))
		{
			CompareRowData->FbxMaterialIndex = RowIndex;
			for (int32 CurrentMaterialIndex = 0; CurrentMaterialIndex < CurrentMeshData->CompMaterials.Num(); ++CurrentMaterialIndex)
			{
				if (CurrentMeshData->CompMaterials[CurrentMaterialIndex].ImportedMaterialSlotName == FbxMeshData->CompMaterials[RowIndex].ImportedMaterialSlotName)
				{
					CompareRowData->FbxMaterialMatch = CurrentMaterialIndex;
					break;
				}
			}
		}
	}
}

TSharedPtr<SWidget> SFbxCompareWindow::ConstructSkeletonComparison()
{
	if (!PreviewObject->IsA(USkeletalMesh::StaticClass()))
	{
		//Return an empty widget, we do not show the skeleton when the mesh is not a skeletal mesh
		return SNew(SBox);
	}

	FString SkeletonStatusTooltip;
	if (AssetReferencingSkeleton.Num() > 0)
	{
		SkeletonStatusTooltip += TEXT("Skeleton is references by ") + FString::FromInt(AssetReferencingSkeleton.Num()) + TEXT(" assets.");
	}
	
	FText SkeletonStatus = FText(FbxMeshData->CompSkeleton.bSkeletonFitMesh ? LOCTEXT("SFbxCompareWindow_ConstructSkeletonComparison_MatchAndMerge", "The skeleton can be merged") : LOCTEXT("SFbxCompareWindow_ConstructSkeletonComparison_CannotMatchAndMerge", "The skeleton must be regenerated, it cannot be merged"));
	
	CompareTree = SNew(STreeView< TSharedPtr<FSkeletonCompareData> >)
		.ItemHeight(24)
		.SelectionMode(ESelectionMode::None)
		.TreeItemsSource(&DisplaySkeletonTreeItem)
		.OnGenerateRow(this, &SFbxCompareWindow::OnGenerateRowCompareTreeView)
		.OnGetChildren(this, &SFbxCompareWindow::OnGetChildrenRowCompareTreeView);


	return SNew(SBox)
	.MaxDesiredHeight(600)
	[
		SNew(SBorder)
		.Padding(FMargin(3))
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.IsFocusable(false)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.OnClicked(this, &SFbxCompareWindow::SetSectionVisible, EFBXCompareSection_Skeleton)
					[
						SNew(SImage).Image(this, &SFbxCompareWindow::GetCollapsableArrow, EFBXCompareSection_Skeleton)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.Text(LOCTEXT("SFbxCompareWindow_SkeletonCompareHeader", "Skeleton"))
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SFbxCompareWindow::IsSectionVisible, EFBXCompareSection_Skeleton)))
				[
					SNew(SBorder)
					.Padding(FMargin(3))
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							SNew(STextBlock)
							.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
							.Text(SkeletonStatus)
							.ToolTipText(FText::FromString(SkeletonStatusTooltip))
							.ColorAndOpacity(FbxMeshData->CompSkeleton.bSkeletonFitMesh ? FSlateColor::UseForeground() : FSlateColor(FLinearColor(0.7f, 0.3f, 0.0f)))
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							SNew(SSeparator)
							.Orientation(EOrientation::Orient_Horizontal)
						]
						+SVerticalBox::Slot()
						.FillHeight(1.0f)
						.Padding(2)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								CompareTree.ToSharedRef()
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(2)
							[
								SNew(SSeparator)
								.Orientation(EOrientation::Orient_Horizontal)
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							.MaxHeight(200.0f)
							[
								//Show the general fbx information
								SNew(SListView<TSharedPtr<FString>>)
								.ListItemsSource(&AssetReferencingSkeleton)
								.OnGenerateRow(this, &SFbxCompareWindow::OnGenerateRowAssetReferencingSkeleton)
							]
						]
					]
				]
			]
		]
	];
}

class SCompareSkeletonTreeViewItem : public STableRow< TSharedPtr<FSkeletonCompareData> >
{
public:

	SLATE_BEGIN_ARGS(SCompareSkeletonTreeViewItem)
		: _SkeletonCompareData(nullptr)
		, _CurrentMeshData(nullptr)
		, _FbxMeshData(nullptr)
	{}

	/** The item content. */
	SLATE_ARGUMENT(TSharedPtr<FSkeletonCompareData>, SkeletonCompareData)
	SLATE_ARGUMENT(FCompMesh*, CurrentMeshData)
	SLATE_ARGUMENT(FCompMesh*, FbxMeshData)
	SLATE_END_ARGS()

	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		SkeletonCompareData = InArgs._SkeletonCompareData;
		CurrentMeshData = InArgs._CurrentMeshData;
		FbxMeshData = InArgs._FbxMeshData;

		//This is suppose to always be valid
		check(SkeletonCompareData.IsValid());
		check(CurrentMeshData != nullptr);
		check(FbxMeshData != nullptr);

		const FSlateBrush* JointIcon = SkeletonCompareData->bMatchJoint ? FEditorStyle::GetDefaultBrush() : SkeletonCompareData->FbxJointIndex != INDEX_NONE ? FEditorStyle::GetBrush("FBXIcon.ReimportCompareAdd") : FEditorStyle::GetBrush("FBXIcon.ReimportCompareRemoved");

		//Prepare the tooltip
		FString Tooltip = SkeletonCompareData->bMatchJoint ? TEXT("") : FText(SkeletonCompareData->FbxJointIndex != INDEX_NONE ? LOCTEXT("SCompareSkeletonTreeViewItem_AddJoint_tooltip", "Fbx reimport will add this joint") : LOCTEXT("SCompareSkeletonTreeViewItem_RemoveJoint_tooltip", "Fbx reimport will remove this joint")).ToString();

		this->ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 2.0f, 6.0f, 2.0f)
				[
					SNew(SImage)
					.Image(JointIcon)
					.Visibility(JointIcon != FEditorStyle::GetDefaultBrush() ? EVisibility::Visible : EVisibility::Collapsed)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 3.0f, 6.0f, 3.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(SkeletonCompareData->JointName.ToString()))
					.ToolTipText(FText::FromString(Tooltip))
					.ColorAndOpacity(SkeletonCompareData->bMatchJoint && !SkeletonCompareData->bChildConflict ? FSlateColor::UseForeground() : FSlateColor(FLinearColor(0.7f, 0.3f, 0.0f)))
				]
			];

		STableRow< TSharedPtr<FSkeletonCompareData> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true),
			InOwnerTableView
			);
	}

private:
	/** The node info to build the tree view row from. */
	TSharedPtr<FSkeletonCompareData> SkeletonCompareData;
	FCompMesh *CurrentMeshData;
	FCompMesh *FbxMeshData;
};


TSharedRef<ITableRow> SFbxCompareWindow::OnGenerateRowCompareTreeView(TSharedPtr<FSkeletonCompareData> RowData, const TSharedRef<STableViewBase>& Table)
{
	TSharedRef< SCompareSkeletonTreeViewItem > ReturnRow = SNew(SCompareSkeletonTreeViewItem, Table)
		.SkeletonCompareData(RowData)
		.CurrentMeshData(CurrentMeshData)
		.FbxMeshData(FbxMeshData);
	return ReturnRow;
}

void SFbxCompareWindow::OnGetChildrenRowCompareTreeView(TSharedPtr<FSkeletonCompareData> InParent, TArray< TSharedPtr<FSkeletonCompareData> >& OutChildren)
{
	for (int32 ChildIndex = 0; ChildIndex < InParent->ChildJoints.Num(); ++ChildIndex)
	{
		TSharedPtr<FSkeletonCompareData> ChildJoint = InParent->ChildJoints[ChildIndex];
		if (ChildJoint.IsValid())
		{
			OutChildren.Add(ChildJoint);
		}
	}
}

TSharedRef<ITableRow> SFbxCompareWindow::OnGenerateRowAssetReferencingSkeleton(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	int32 AssetListIndex = AssetReferencingSkeleton.Find(InItem);
	bool LightBackgroundColor = AssetListIndex % 2 == 1;
	return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
		[
			SNew(SBorder)
			.BorderImage(LightBackgroundColor ? FEditorStyle::GetBrush("ToolPanel.GroupBorder") : FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			[
				SNew(STextBlock)
				.Text(FText::FromString(*(InItem.Get())))
				//.ColorAndOpacity(LightBackgroundColor ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
			]
		];
}

void SFbxCompareWindow::FilSkeletonTreeItem()
{
	//Create all the entries
	for (int32 RowIndex = 0; RowIndex < CurrentMeshData->CompSkeleton.Joints.Num(); ++RowIndex)
	{
		TSharedPtr<FSkeletonCompareData> CompareRowData = MakeShareable(new FSkeletonCompareData());
		int32 AddedIndex = CurrentSkeletonTreeItem.Add(CompareRowData);
		check(AddedIndex == RowIndex);
		CompareRowData->CurrentJointIndex = RowIndex;
		CompareRowData->JointName = CurrentMeshData->CompSkeleton.Joints[RowIndex].Name;
		CompareRowData->ChildJointIndexes = CurrentMeshData->CompSkeleton.Joints[RowIndex].ChildIndexes;
	}

	//Set the childrens and parent pointer
	for (int32 RowIndex = 0; RowIndex < CurrentMeshData->CompSkeleton.Joints.Num(); ++RowIndex)
	{
		check(CurrentSkeletonTreeItem.IsValidIndex(RowIndex));
		TSharedPtr<FSkeletonCompareData> CompareRowData = CurrentSkeletonTreeItem[RowIndex];
		if (CurrentSkeletonTreeItem.IsValidIndex(CurrentMeshData->CompSkeleton.Joints[RowIndex].ParentIndex))
		{
			CompareRowData->ParentJoint = CurrentSkeletonTreeItem[CurrentMeshData->CompSkeleton.Joints[RowIndex].ParentIndex];
		}
		
		for (int32 ChildJointIndex = 0; ChildJointIndex < CompareRowData->ChildJointIndexes.Num(); ++ChildJointIndex)
		{
			if (CurrentSkeletonTreeItem.IsValidIndex(CompareRowData->ChildJointIndexes[ChildJointIndex]))
			{
				CompareRowData->ChildJoints.Add(CurrentSkeletonTreeItem[CompareRowData->ChildJointIndexes[ChildJointIndex]]);
			}
		}
	}

	for (int32 RowIndex = 0; RowIndex < FbxMeshData->CompSkeleton.Joints.Num(); ++RowIndex)
	{
		TSharedPtr<FSkeletonCompareData> CompareRowData = MakeShareable(new FSkeletonCompareData());
		int32 AddedIndex = FbxSkeletonTreeItem.Add(CompareRowData);
		check(AddedIndex == RowIndex);
		CompareRowData->FbxJointIndex = RowIndex;
		CompareRowData->JointName = FbxMeshData->CompSkeleton.Joints[RowIndex].Name;
		CompareRowData->ChildJointIndexes = FbxMeshData->CompSkeleton.Joints[RowIndex].ChildIndexes;
	}

	//Set the childrens and parent pointer
	for (int32 RowIndex = 0; RowIndex < FbxMeshData->CompSkeleton.Joints.Num(); ++RowIndex)
	{
		check(FbxSkeletonTreeItem.IsValidIndex(RowIndex));
		TSharedPtr<FSkeletonCompareData> CompareRowData = FbxSkeletonTreeItem[RowIndex];
		if (FbxSkeletonTreeItem.IsValidIndex(FbxMeshData->CompSkeleton.Joints[RowIndex].ParentIndex))
		{
			CompareRowData->ParentJoint = FbxSkeletonTreeItem[FbxMeshData->CompSkeleton.Joints[RowIndex].ParentIndex];
		}

		for (int32 ChildJointIndex = 0; ChildJointIndex < CompareRowData->ChildJointIndexes.Num(); ++ChildJointIndex)
		{
			if (FbxSkeletonTreeItem.IsValidIndex(CompareRowData->ChildJointIndexes[ChildJointIndex]))
			{
				CompareRowData->ChildJoints.Add(FbxSkeletonTreeItem[CompareRowData->ChildJointIndexes[ChildJointIndex]]);
			}
		}
	}
}

void SFbxCompareWindow::RecursiveMatchJointInfo(TSharedPtr<FSkeletonCompareData> SkeletonItem)
{
	TArray<TSharedPtr<FSkeletonCompareData>> DisplayChilds;
	//Find the display child
	if (CurrentSkeletonTreeItem.IsValidIndex(SkeletonItem->CurrentJointIndex))
	{
		for (int32 ChildIndex = 0; ChildIndex < CurrentSkeletonTreeItem[SkeletonItem->CurrentJointIndex]->ChildJoints.Num(); ++ChildIndex)
		{
			DisplayChilds.Add(CurrentSkeletonTreeItem[SkeletonItem->CurrentJointIndex]->ChildJoints[ChildIndex]);
		}
	}
	if (FbxSkeletonTreeItem.IsValidIndex(SkeletonItem->FbxJointIndex))
	{
		for (int32 ChildIndex = 0; ChildIndex < FbxSkeletonTreeItem[SkeletonItem->FbxJointIndex]->ChildJoints.Num(); ++ChildIndex)
		{
			TSharedPtr<FSkeletonCompareData> FbxSkeletonItem = FbxSkeletonTreeItem[SkeletonItem->FbxJointIndex]->ChildJoints[ChildIndex];
			bool bFoundChildMatch = false;
			for (TSharedPtr<FSkeletonCompareData> DisplayChildJoint : DisplayChilds)
			{
				if (DisplayChildJoint->JointName == FbxSkeletonItem->JointName)
				{
					bFoundChildMatch = true;
					DisplayChildJoint->bMatchJoint = true;
					DisplayChildJoint->FbxJointIndex = FbxSkeletonItem->FbxJointIndex;
					break;
				}
			}
			if (!bFoundChildMatch)
			{
				DisplayChilds.Add(FbxSkeletonItem);
			}
		}
	}

	if (!SkeletonItem->bMatchJoint)
	{
		TSharedPtr<FSkeletonCompareData> ParentSkeletonItem = SkeletonItem->ParentJoint;
		while (ParentSkeletonItem.IsValid() && ParentSkeletonItem->bChildConflict == false)
		{
			ParentSkeletonItem->bChildConflict = true;
			ParentSkeletonItem = ParentSkeletonItem->ParentJoint;
		}
	}
	//Set the new child list to the display joint
	SkeletonItem->ChildJoints = DisplayChilds;
	SkeletonItem->ChildJointIndexes.Empty();
	for (TSharedPtr<FSkeletonCompareData> ChildJoint : SkeletonItem->ChildJoints)
	{
		ChildJoint->ParentJoint = SkeletonItem;
		RecursiveMatchJointInfo(ChildJoint);
	}
}

void SFbxCompareWindow::SetMatchJointInfo()
{
	TArray<TSharedPtr<FSkeletonCompareData>> RootJoint;
	for (TSharedPtr<FSkeletonCompareData> CurrentSkeletonItem : CurrentSkeletonTreeItem)
	{
		if (!CurrentSkeletonItem->ParentJoint.IsValid())
		{
			DisplaySkeletonTreeItem.Add(CurrentSkeletonItem);
		}
	}
	for (TSharedPtr<FSkeletonCompareData> CurrentSkeletonItem : FbxSkeletonTreeItem)
	{
		if (!CurrentSkeletonItem->ParentJoint.IsValid())
		{
			bool bInsertJoint = true;
			for (TSharedPtr<FSkeletonCompareData> DisplayTreeItem : DisplaySkeletonTreeItem)
			{
				if(DisplayTreeItem->JointName == CurrentSkeletonItem->JointName)
				{
					DisplayTreeItem->FbxJointIndex = CurrentSkeletonItem->FbxJointIndex;
					DisplayTreeItem->bMatchJoint = true;
					bInsertJoint = false;
				}
			}
			if (bInsertJoint)
			{
				DisplaySkeletonTreeItem.Add(CurrentSkeletonItem);
			}
		}
	}

	for (int32 SkeletonTreeIndex = 0; SkeletonTreeIndex < DisplaySkeletonTreeItem.Num(); ++SkeletonTreeIndex)
	{
		RecursiveMatchJointInfo(DisplaySkeletonTreeItem[SkeletonTreeIndex]);
	}
}
#undef LOCTEXT_NAMESPACE
