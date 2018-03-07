// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_CopyPaste.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SButton.h"
#include "LandscapeEditorObject.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"

#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.CopyPaste"

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_CopyPaste::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_CopyPaste);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_CopyPaste::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (!IsToolActive("CopyPaste"))
	{
		return;
	}

	IDetailCategoryBuilder& ToolsCategory = DetailBuilder.EditCategory("Tool Settings");

	ToolsCategory.AddCustomRow(LOCTEXT("CopyToGizmo", "Copy Data to Gizmo"))
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("CopyToGizmo.Tooltip", "Copies the data within the gizmo bounds to the gizmo taking into account any masking from selected regions."))
		.Text(LOCTEXT("CopyToGizmo", "Copy Data to Gizmo"))
		.HAlign(HAlign_Center)
		.OnClicked_Static(&FLandscapeEditorDetailCustomization_CopyPaste::OnCopyToGizmoButtonClicked)
	];

	ToolsCategory.AddCustomRow(LOCTEXT("FitGizmoToSelection", "Fit Gizmo to Selected Regions"))
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("FitGizmoToSelection.Tooltip", "Positions and resizes the gizmo so that it completely encompasses all region selections."))
		.Text(LOCTEXT("FitGizmoToSelection", "Fit Gizmo to Selected Regions"))
		.HAlign(HAlign_Center)
		.OnClicked_Static(&FLandscapeEditorDetailCustomization_CopyPaste::OnFitGizmoToSelectionButtonClicked)
	];

	ToolsCategory.AddCustomRow(LOCTEXT("FitHeightsToGizmo", "Fit Height Values to Gizmo Size"))
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("FitHeightsToGizmo.Tooltip", "Scales the data in the gizmo to fit the gizmo's Z size"))
		.Text(LOCTEXT("FitHeightsToGizmo", "Fit Height Values to Gizmo Size"))
		.HAlign(HAlign_Center)
		.OnClicked_Static(&FLandscapeEditorDetailCustomization_CopyPaste::OnFitHeightsToGizmoButtonClicked)
	];

	ToolsCategory.AddCustomRow(LOCTEXT("ClearGizmoData", "Clear Gizmo Data"))
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("ClearGizmoData.Tooltip", "Clears the gizmo of any copied data."))
		.Text(LOCTEXT("ClearGizmoData", "Clear Gizmo Data"))
		.HAlign(HAlign_Center)
		.OnClicked_Static(&FLandscapeEditorDetailCustomization_CopyPaste::OnClearGizmoDataButtonClicked)
	];


	IDetailGroup& GizmoImportExportGroup = ToolsCategory.AddGroup("Gizmo Import / Export", LOCTEXT("ImportExportTitle", "Gizmo Import / Export"), true);

	TSharedRef<IPropertyHandle> PropertyHandle_Heightmap = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, GizmoHeightmapFilenameString));
	DetailBuilder.HideProperty(PropertyHandle_Heightmap);
	GizmoImportExportGroup.AddPropertyRow(PropertyHandle_Heightmap)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Heightmap->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			PropertyHandle_Heightmap->CreatePropertyValueWidget()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		//.Padding(0,0,12,0) // Line up with the other properties due to having no reset to default button
		[
			SNew(SButton)
			.ContentPadding(FMargin(4, 0))
			.Text(NSLOCTEXT("UnrealEd", "GenericOpenDialog", "..."))
			.OnClicked_Static(&FLandscapeEditorDetailCustomization_CopyPaste::OnGizmoHeightmapFilenameButtonClicked, PropertyHandle_Heightmap)
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_ImportSize = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, GizmoImportSize));
	TSharedRef<IPropertyHandle> PropertyHandle_ImportSize_X = PropertyHandle_ImportSize->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ImportSize_Y = PropertyHandle_ImportSize->GetChildHandle("Y").ToSharedRef();
	DetailBuilder.HideProperty(PropertyHandle_ImportSize);
	GizmoImportExportGroup.AddPropertyRow(PropertyHandle_ImportSize)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_ImportSize->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SNumericEntryBox<int32>)
			.LabelVAlign(VAlign_Center)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(8192)
			.MinSliderValue(1)
			.MaxSliderValue(8192)
			.AllowSpin(true)
			.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
			.Value_Static(&FLandscapeEditorDetailCustomization_Base::OnGetValue<int32>, PropertyHandle_ImportSize_X)
			.OnValueChanged_Static(&FLandscapeEditorDetailCustomization_Base::OnValueChanged<int32>, PropertyHandle_ImportSize_X)
			.OnValueCommitted_Static(&FLandscapeEditorDetailCustomization_Base::OnValueCommitted<int32>, PropertyHandle_ImportSize_X)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(FText::FromString(FString().AppendChar(0xD7))) // Multiply sign
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SNumericEntryBox<int32>)
			.LabelVAlign(VAlign_Center)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(8192)
			.MinSliderValue(1)
			.MaxSliderValue(8192)
			.AllowSpin(true)
			.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
			.Value_Static(&FLandscapeEditorDetailCustomization_Base::OnGetValue<int32>, PropertyHandle_ImportSize_Y)
			.OnValueChanged_Static(&FLandscapeEditorDetailCustomization_Base::OnValueChanged<int32>, PropertyHandle_ImportSize_Y)
			.OnValueCommitted_Static(&FLandscapeEditorDetailCustomization_Base::OnValueCommitted<int32>, PropertyHandle_ImportSize_Y)
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_ImportLayers = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, GizmoImportLayers));
	DetailBuilder.HideProperty(PropertyHandle_ImportLayers);
	GizmoImportExportGroup.AddPropertyRow(PropertyHandle_ImportLayers);

	GizmoImportExportGroup.AddWidgetRow()
	.FilterString(LOCTEXT("GizmoImportExport", "ImportExport"))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("GizmoImport", "Import"))
			.IsEnabled(this, &FLandscapeEditorDetailCustomization_CopyPaste::GetGizmoImportButtonIsEnabled)
			.OnClicked(this, &FLandscapeEditorDetailCustomization_CopyPaste::OnGizmoImportButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("GizmoExport", "Export"))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_CopyPaste::OnGizmoExportButtonClicked)
		]
	];

	//GuessGizmoImportSize();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply FLandscapeEditorDetailCustomization_CopyPaste::OnCopyToGizmoButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		LandscapeEdMode->CopyDataToGizmo();
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_CopyPaste::OnFitGizmoToSelectionButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		ALandscapeGizmoActiveActor* Gizmo = LandscapeEdMode->CurrentGizmoActor.Get();
		if (Gizmo && Gizmo->TargetLandscapeInfo)
		{
			FScopedTransaction Transaction(LOCTEXT("LandscapeGizmo_FitToSelection", "Fit gizmo size to selection"));
			Gizmo->Modify();
			Gizmo->FitToSelection();
		}
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_CopyPaste::OnFitHeightsToGizmoButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		ALandscapeGizmoActiveActor* Gizmo = LandscapeEdMode->CurrentGizmoActor.Get();
		if (Gizmo && Gizmo->TargetLandscapeInfo)
		{
			FScopedTransaction Transaction(LOCTEXT("LandscapeGizmo_FitMinMaxHeight", "Set gizmo height to fix contained data"));
			Gizmo->Modify();
			Gizmo->FitMinMaxHeight();
		}
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_CopyPaste::OnClearGizmoDataButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		ALandscapeGizmoActiveActor* Gizmo = LandscapeEdMode->CurrentGizmoActor.Get();
		if (Gizmo && Gizmo->TargetLandscapeInfo)
		{
			Gizmo->ClearGizmoData();
		}
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_CopyPaste::OnGizmoHeightmapFilenameButtonClicked(TSharedRef<IPropertyHandle> HeightmapPropertyHandle)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		// Prompt the user for the Filenames
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (DesktopPlatform != NULL)
		{
			TArray<FString> OpenFilenames;
			bool bOpened = DesktopPlatform->OpenFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				NSLOCTEXT("UnrealEd", "Import", "Import").ToString(),
				LandscapeEdMode->UISettings->LastImportPath,
				TEXT(""),
				TEXT("Raw Heightmap files (*.raw,*.r16)|*.raw;*.r16|All files (*.*)|*.*"),
				EFileDialogFlags::None,
				OpenFilenames);

			if (bOpened)
			{
				//SetGizmoHeightmapFilenameString(FText::FromString(OpenFilenames[0]), ETextCommit::OnEnter);
				HeightmapPropertyHandle->SetValue(OpenFilenames[0]);
				LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(OpenFilenames[0]);
			}
		}
	}

	return FReply::Handled();
}

bool FLandscapeEditorDetailCustomization_CopyPaste::GetGizmoImportButtonIsEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		if (!LandscapeEdMode->UISettings->GizmoHeightmapFilenameString.IsEmpty() && LandscapeEdMode->UISettings->GizmoImportSize != FIntPoint(0, 0))
		{
			return true;
		}
	}
	return false;
}

FReply FLandscapeEditorDetailCustomization_CopyPaste::OnGizmoImportButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		ALandscapeGizmoActiveActor* Gizmo = LandscapeEdMode->CurrentGizmoActor.Get();
		if (Gizmo)
		{
			TArray<uint8> Data;

			FFileHelper::LoadFileToArray(Data, *LandscapeEdMode->UISettings->GizmoHeightmapFilenameString);

			if (Data.Num() <= 0
				|| Data.Num() != (LandscapeEdMode->UISettings->GizmoImportSize.X * LandscapeEdMode->UISettings->GizmoImportSize.Y * 2))
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "LandscapeImport_BadHeightmapSize", "File size does not match"));
				return FReply::Handled();
			}

			TArray<ULandscapeLayerInfoObject*> LayerInfos;
			TArray<TArray<uint8> > LayerDataArrays;
			TArray<uint8*> LayerDataPtrs;

			for (int32 LayerIndex = 0; LayerIndex < LandscapeEdMode->UISettings->GizmoImportLayers.Num(); LayerIndex++)
			{
				const FGizmoImportLayer& Layer = LandscapeEdMode->UISettings->GizmoImportLayers[LayerIndex];
				FString LayerName = Layer.LayerName.Replace(TEXT(" "), TEXT(""));
				if (LayerName == TEXT(""))
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(NSLOCTEXT("UnrealEd", "LandscapeImport_BadLayerName", "You must enter a name for the layer being imported from {0}."), FText::FromString(Layer.LayerFilename)));
					return FReply::Handled();
				}

				if (Layer.LayerFilename != TEXT("") && !Layer.bNoImport)
				{
					TArray<uint8>* LayerData = new(LayerDataArrays)(TArray<uint8>);
					FFileHelper::LoadFileToArray(*LayerData, *Layer.LayerFilename);

					if (LayerData->Num() != (LandscapeEdMode->UISettings->GizmoImportSize.X * LandscapeEdMode->UISettings->GizmoImportSize.Y))
					{
						FMessageDialog::Open(EAppMsgType::Ok,
							FText::Format(NSLOCTEXT("UnrealEd", "LandscapeImport_BadLayerSize", "Layer {0} file size does not match the heightmap resolution."), FText::FromString(Layer.LayerFilename)));
						return FReply::Handled();
					}

					LayerInfos.Add(LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetLayerInfoByName(FName(*LayerName)));
					LayerDataPtrs.Add(&(*LayerData)[0]);
				}
			}

			Gizmo->Import(LandscapeEdMode->UISettings->GizmoImportSize.X, LandscapeEdMode->UISettings->GizmoImportSize.Y, (uint16*)Data.GetData(), LayerInfos, LayerDataPtrs.Num() ? LayerDataPtrs.GetData() : NULL);

			// Make sure gizmo actor is selected
			GEditor->SelectNone(false, true);
			GEditor->SelectActor(Gizmo, true, false, true);
		}
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_CopyPaste::OnGizmoExportButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		ALandscapeGizmoActiveActor* Gizmo = LandscapeEdMode->CurrentGizmoActor.Get();
		if (Gizmo && Gizmo->TargetLandscapeInfo && Gizmo->SelectedData.Num())
		{
			int32 TargetIndex = -1;
			ULandscapeInfo* LandscapeInfo = Gizmo->TargetLandscapeInfo;
			TArray<FString> Filenames;

			// Local set for export
			TSet<ULandscapeLayerInfoObject*> LayerInfoSet;
			for (int32 i = 0; i < Gizmo->LayerInfos.Num(); i++)
			{
				if (LandscapeEdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Weightmap && LandscapeEdMode->CurrentToolTarget.LayerInfo == Gizmo->LayerInfos[i])
				{
					TargetIndex = i;
				}
				LayerInfoSet.Add(Gizmo->LayerInfos[i]);
			}

			for (int32 i = -1; i < Gizmo->LayerInfos.Num(); i++)
			{
				if (!LandscapeEdMode->UISettings->bApplyToAllTargets && i != TargetIndex)
				{
					continue;
				}
				FString SaveDialogTitle;
				FString DefaultFilename;
				FString FileTypes;

				if (i < 0)
				{
					if (!(Gizmo->DataType & LGT_Height))
					{
						continue;
					}
					SaveDialogTitle = NSLOCTEXT("UnrealEd", "LandscapeExport_HeightmapFilename", "Choose filename for Heightmap Export").ToString();
					DefaultFilename = TEXT("Heightmap.raw");
					FileTypes = TEXT("Heightmap .raw files|*.raw|Heightmap .r16 files|*.r16|All files|*.*");
				}
				else
				{
					if (!(Gizmo->DataType & LGT_Weight))
					{
						continue;
					}

					FName LayerName = Gizmo->LayerInfos[i]->LayerName;
					SaveDialogTitle = FText::Format(NSLOCTEXT("UnrealEd", "LandscapeExport_LayerFilename", "Choose filename for Layer {0} Export"), FText::FromString(LayerName.ToString())).ToString();
					DefaultFilename = FString::Printf(TEXT("%s.raw"), *LayerName.ToString());
					FileTypes = TEXT("Layer .raw files|*.raw|Layer .r8 files|*.r8|All files|*.*");
				}

				TArray<FString> SaveFilenames;
				IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
				bool bSave = false;
				if (DesktopPlatform)
				{
					bSave = DesktopPlatform->SaveFileDialog(
						FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
						SaveDialogTitle,
						LandscapeEdMode->UISettings->LastImportPath,
						DefaultFilename,
						FileTypes,
						EFileDialogFlags::None,
						SaveFilenames
						);
				}

				if (!bSave)
				{
					return FReply::Handled();
				}

				Filenames.Add(SaveFilenames[0]);
				LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(SaveFilenames[0]);
			}

			Gizmo->Export(TargetIndex, Filenames);
		}
	}

	return FReply::Handled();
}

//////////////////////////////////////////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FLandscapeEditorStructCustomization_FGizmoImportLayer::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorStructCustomization_FGizmoImportLayer);
}

void FLandscapeEditorStructCustomization_FGizmoImportLayer::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FLandscapeEditorStructCustomization_FGizmoImportLayer::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<IPropertyHandle> PropertyHandle_LayerFilename = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGizmoImportLayer, LayerFilename)).ToSharedRef();
	ChildBuilder.AddProperty(PropertyHandle_LayerFilename)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_LayerFilename->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			PropertyHandle_LayerFilename->CreatePropertyValueWidget()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		//.Padding(0,0,12,0) // Line up with the other properties due to having no reset to default button
		[
			SNew(SButton)
			.ContentPadding(FMargin(4, 0))
			.Text(NSLOCTEXT("UnrealEd", "GenericOpenDialog", "..."))
			.OnClicked_Static(&FLandscapeEditorStructCustomization_FGizmoImportLayer::OnGizmoImportLayerFilenameButtonClicked, PropertyHandle_LayerFilename)
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_LayerName = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGizmoImportLayer, LayerName)).ToSharedRef();
	ChildBuilder.AddProperty(PropertyHandle_LayerName);
}

FReply FLandscapeEditorStructCustomization_FGizmoImportLayer::OnGizmoImportLayerFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerFilename)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	check(LandscapeEdMode != NULL);

	// Prompt the user for the Filenames
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform != NULL)
	{
		TArray<FString> OpenFilenames;
		bool bOpened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("UnrealEd", "Import", "Import").ToString(),
			LandscapeEdMode->UISettings->LastImportPath,
			TEXT(""),
			TEXT("Raw Layer files (*.raw,*.r16)|*.raw;*.r16|All files (*.*)|*.*"),
			EFileDialogFlags::None,
			OpenFilenames);

		if (bOpened)
		{
			PropertyHandle_LayerFilename->SetValue(OpenFilenames[0]);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(OpenFilenames[0]);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
