// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshProxyTool/SMeshProxyDialog.h"
#include "Engine/MeshMerging.h"
#include "MeshProxyTool/MeshProxyTool.h"
#include "UObject/UnrealType.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/STextComboBox.h"

#define LOCTEXT_NAMESPACE "SMeshProxyDialog"


void SMeshProxyDialog::Construct(const FArguments& InArgs, FMeshProxyTool* InTool)
{
	Tool = InTool;
	check(Tool != nullptr);

	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+X"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+Y"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("+Z"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-X"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-Y"))));
	CuttingPlaneOptions.Add(MakeShareable(new FString(TEXT("-Z"))));

	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("64"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("128"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("256"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("512"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("1024"))));
	TextureResolutionOptions.Add(MakeShareable(new FString(TEXT("2048"))));

	CreateLayout();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMeshProxyDialog::CreateLayout()
{
	int32 TextureResEntryIndex = FindTextureResolutionEntryIndex(Tool->ProxySettings.MaterialSettings.TextureSize.X);
	int32 LightMapResEntryIndex = FindTextureResolutionEntryIndex(Tool->ProxySettings.LightMapResolution);
	TextureResEntryIndex = FMath::Max(TextureResEntryIndex, 0);
	LightMapResEntryIndex = FMath::Max(LightMapResEntryIndex, 0);
		
	this->ChildSlot
	[
		SNew(SVerticalBox)
			
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(10)
		[
			// Simplygon logo
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("MeshProxy.SimplygonLogo"))
		]
			
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				// Proxy options
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OnScreenSizeLabel", "On Screen Size (pixels)"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, ScreenSize)))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<int32>)
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
							.MinValue(40)
							.MaxValue(1200)
							.MinSliderValue(40)
							.MaxSliderValue(1200)
							.AllowSpin(true)
							.Value(this, &SMeshProxyDialog::GetScreenSize)
							.OnValueChanged(this, &SMeshProxyDialog::ScreenSizeChanged)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MergeDistanceLabel", "Merge Distance (pixels)"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, MergeDistance)))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<int32>)
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
							.MinValue(0)
							.MaxValue(300)
							.MinSliderValue(0)
							.MaxSliderValue(300)
							.AllowSpin(true)
							.Value(this, &SMeshProxyDialog::GetMergeDistance)
							.OnValueChanged(this, &SMeshProxyDialog::MergeDistanceChanged)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextureResolutionLabel", "Texture Resolution"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextComboBox)
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						.OptionsSource(&TextureResolutionOptions)
						.InitiallySelectedItem(TextureResolutionOptions[TextureResEntryIndex])
						.OnSelectionChanged(this, &SMeshProxyDialog::SetTextureResolution)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LightMapResolutionLabel", "LightMap Resolution"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, LightMapResolution)))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextComboBox)
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						.OptionsSource(&TextureResolutionOptions)
						.InitiallySelectedItem(TextureResolutionOptions[LightMapResEntryIndex])
						.OnSelectionChanged(this, &SMeshProxyDialog::SetLightMapResolution)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.VAlign(VAlign_Center)
					.Padding(0.0, 0.0, 3.0, 0.0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("HardAngleLabel", "Hard Edge Angle"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, HardAngleThreshold)))
					]
					+SHorizontalBox::Slot()
					.FillWidth(0.5f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Fill)
						.MinDesiredWidth(100.0f)
						.MaxDesiredWidth(100.0f)
						[
							SNew(SNumericEntryBox<float>)
							.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
							.MinValue(0.f)
							.MaxValue(180.f)
							.MinSliderValue(0.f)
							.MaxSliderValue(180.f)
							.AllowSpin(true)
							.Value(this, &SMeshProxyDialog::GetHardAngleThreshold)
							.OnValueChanged(this, &SMeshProxyDialog::HardAngleThresholdChanged)
							.IsEnabled(this, &SMeshProxyDialog::HardAngleThresholdEnabled)
						]
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetRecalculateNormals)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetRecalculateNormals)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RecalcNormalsLabel", "Recalculate Normals"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
						.ToolTipText(GetPropertyToolTipText(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, bRecalculateNormals)))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetExportNormalMap)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetExportNormalMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportNormalMapLabel", "Export Normal Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetExportMetallicMap)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetExportMetallicMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportMetallicMapLabel", "Export Metallic Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetExportRoughnessMap)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetExportRoughnessMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportRoughnessMapLabel", "Export Roughness Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SCheckBox)
					.Type(ESlateCheckBoxType::CheckBox)
					.IsChecked(this, &SMeshProxyDialog::GetExportSpecularMap)
					.OnCheckStateChanged(this, &SMeshProxyDialog::SetExportSpecularMap)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ExportSpecularMapLabel", "Export Specular Map"))
						.Font(FEditorStyle::GetFontStyle("StandardDialog.SmallFont"))
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

int32 SMeshProxyDialog::FindTextureResolutionEntryIndex(int32 InResolution) const
{
	FString ResolutionStr = TTypeToString<int32>::ToString(InResolution);
	
	int32 Result = TextureResolutionOptions.IndexOfByPredicate([&](const TSharedPtr<FString>& Entry)
	{
		return (ResolutionStr == *Entry);
	});

	return Result;
}

FText SMeshProxyDialog::GetPropertyToolTipText(const FName& PropertyName) const
{
	UProperty* Property = FMeshProxySettings::StaticStruct()->FindPropertyByName(PropertyName);
	if (Property)
	{
		return Property->GetToolTipText();
	}
	
	return FText::GetEmpty();
}

//Screen size
TOptional<int32> SMeshProxyDialog::GetScreenSize() const
{
	return Tool->ProxySettings.ScreenSize;
}

void SMeshProxyDialog::ScreenSizeChanged(int32 NewValue)
{
	Tool->ProxySettings.ScreenSize = NewValue;
}

//Recalculate normals
ECheckBoxState SMeshProxyDialog::GetRecalculateNormals() const
{
	return Tool->ProxySettings.bRecalculateNormals ? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetRecalculateNormals(ECheckBoxState NewValue)
{
	Tool->ProxySettings.bRecalculateNormals = (NewValue == ECheckBoxState::Checked);
}

//Hard Angle Threshold
bool SMeshProxyDialog::HardAngleThresholdEnabled() const
{
	if(Tool->ProxySettings.bRecalculateNormals)
	{
		return true;
	}

	return false;
}

TOptional<float> SMeshProxyDialog::GetHardAngleThreshold() const
{
	return Tool->ProxySettings.HardAngleThreshold;
}

void SMeshProxyDialog::HardAngleThresholdChanged(float NewValue)
{
	Tool->ProxySettings.HardAngleThreshold = NewValue;
}

//Merge Distance
TOptional<int32> SMeshProxyDialog::GetMergeDistance() const
{
	return Tool->ProxySettings.MergeDistance;
}

void SMeshProxyDialog::MergeDistanceChanged(int32 NewValue)
{
	Tool->ProxySettings.MergeDistance = NewValue;
}

//Texture Resolution
void SMeshProxyDialog::SetTextureResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Resolution = 512;
	TTypeFromString<int32>::FromString(Resolution, **NewSelection);
	FIntPoint TextureSize(Resolution, Resolution);
	
	Tool->ProxySettings.MaterialSettings.TextureSize = TextureSize;
}

void SMeshProxyDialog::SetLightMapResolution(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Resolution = 256;
	TTypeFromString<int32>::FromString(Resolution, **NewSelection);
		
	Tool->ProxySettings.LightMapResolution = Resolution;
}

ECheckBoxState SMeshProxyDialog::GetExportNormalMap() const
{
	return Tool->ProxySettings.MaterialSettings.bNormalMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetExportNormalMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.MaterialSettings.bNormalMap = (NewValue == ECheckBoxState::Checked);
}

ECheckBoxState SMeshProxyDialog::GetExportMetallicMap() const
{
	return Tool->ProxySettings.MaterialSettings.bMetallicMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetExportMetallicMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.MaterialSettings.bMetallicMap = (NewValue == ECheckBoxState::Checked);
}

ECheckBoxState SMeshProxyDialog::GetExportRoughnessMap() const
{
	return Tool->ProxySettings.MaterialSettings.bRoughnessMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetExportRoughnessMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.MaterialSettings.bRoughnessMap = (NewValue == ECheckBoxState::Checked);
}

ECheckBoxState SMeshProxyDialog::GetExportSpecularMap() const
{
	return Tool->ProxySettings.MaterialSettings.bSpecularMap ? ECheckBoxState::Checked :  ECheckBoxState::Unchecked;
}

void SMeshProxyDialog::SetExportSpecularMap(ECheckBoxState NewValue)
{
	Tool->ProxySettings.MaterialSettings.bSpecularMap = (NewValue == ECheckBoxState::Checked);
}


#undef LOCTEXT_NAMESPACE
