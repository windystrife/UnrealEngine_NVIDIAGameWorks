// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_NewLandscape.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorObject.h"
#include "Landscape.h"
#include "LandscapeEditorUtils.h"

#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "SLandscapeEditor.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"
#include "AssetRegistryModule.h"

#include "TutorialMetaData.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "LandscapeDataAccess.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.NewLandscape"

const int32 FLandscapeEditorDetailCustomization_NewLandscape::SectionSizes[] = {7, 15, 31, 63, 127, 255};
const int32 FLandscapeEditorDetailCustomization_NewLandscape::NumSections[] = {1, 2};

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_NewLandscape::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_NewLandscape);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_NewLandscape::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (!IsToolActive("NewLandscape"))
	{
		return;
	}

	IDetailCategoryBuilder& NewLandscapeCategory = DetailBuilder.EditCategory("New Landscape");

	NewLandscapeCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SUniformGridPanel)
		.SlotPadding(FMargin(10, 2))
		+ SUniformGridPanel::Slot(0, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked(this, &FLandscapeEditorDetailCustomization_NewLandscape::NewLandscapeModeIsChecked, ENewLandscapePreviewMode::NewLandscape)
			.OnCheckStateChanged(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnNewLandscapeModeChanged, ENewLandscapePreviewMode::NewLandscape)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NewLandscape", "Create New"))
			]
		]
		+ SUniformGridPanel::Slot(1, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "RadioButton")
			.IsChecked(this, &FLandscapeEditorDetailCustomization_NewLandscape::NewLandscapeModeIsChecked, ENewLandscapePreviewMode::ImportLandscape)
			.OnCheckStateChanged(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnNewLandscapeModeChanged, ENewLandscapePreviewMode::ImportLandscape)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ImportLandscape", "Import from File"))
			]
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapFilename));
	TSharedRef<IPropertyHandle> PropertyHandle_HeightmapImportResult = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapImportResult));
	TSharedRef<IPropertyHandle> PropertyHandle_HeightmapErrorMessage = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_HeightmapErrorMessage));
	DetailBuilder.HideProperty(PropertyHandle_HeightmapImportResult);
	DetailBuilder.HideProperty(PropertyHandle_HeightmapErrorMessage);
	PropertyHandle_HeightmapFilename->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnImportHeightmapFilenameChanged));
	NewLandscapeCategory.AddProperty(PropertyHandle_HeightmapFilename)
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)))
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_HeightmapFilename->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0,0,2,0)
		[
			SNew(SErrorText)
			.Visibility_Static(&GetHeightmapErrorVisibility, PropertyHandle_HeightmapImportResult)
			.BackgroundColor_Static(&GetHeightmapErrorColor, PropertyHandle_HeightmapImportResult)
			.ErrorText(NSLOCTEXT("UnrealEd", "Error", "!"))
			.ToolTip(
				SNew(SToolTip)
				.Text_Static(&GetPropertyValue<FText>, PropertyHandle_HeightmapErrorMessage)
			)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Font(DetailBuilder.GetDetailFont())
			.Text_Static(&GetPropertyValueText, PropertyHandle_HeightmapFilename)
			.OnTextCommitted_Static(&SetImportHeightmapFilenameString, PropertyHandle_HeightmapFilename)
			.HintText(LOCTEXT("Import_HeightmapNotSet", "(Please specify a heightmap)"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(1,0,0,0)
		[
			SNew(SButton)
			//.Font(DetailBuilder.GetDetailFont())
			.ContentPadding(FMargin(4, 0))
			.Text(NSLOCTEXT("UnrealEd", "GenericOpenDialog", "..."))
			.OnClicked_Static(&OnImportHeightmapFilenameButtonClicked, PropertyHandle_HeightmapFilename)
		]
	];

	NewLandscapeCategory.AddCustomRow(LOCTEXT("HeightmapResolution", "Heightmap Resolution"))
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)))
	.NameContent()
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("HeightmapResolution", "Heightmap Resolution"))
		]
	]
	.ValueContent()
	[
		SNew(SBox)
		.Padding(FMargin(0,0,12,0)) // Line up with the other properties due to having no reset to default button
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolutionMenu)
			.ContentPadding(2)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.Text(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolution)
			]
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Material = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Material));
	NewLandscapeCategory.AddProperty(PropertyHandle_Material);

	NewLandscapeCategory.AddCustomRow(LOCTEXT("LayersLabel", "Layers"))
	.Visibility(TAttribute<EVisibility>(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMaterialTipVisibility))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(15, 12, 0, 12)
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("Material_Tip","Hint: Assign a material to see landscape layers"))
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_AlphamapType = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_AlphamapType));
	NewLandscapeCategory.AddProperty(PropertyHandle_AlphamapType)
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)));

	TSharedRef<IPropertyHandle> PropertyHandle_Layers = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ImportLandscape_Layers));
	NewLandscapeCategory.AddProperty(PropertyHandle_Layers);

	TSharedRef<IPropertyHandle> PropertyHandle_Location = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Location));
	TSharedRef<IPropertyHandle> PropertyHandle_Location_X = PropertyHandle_Location->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Location_Y = PropertyHandle_Location->GetChildHandle("Y").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Location_Z = PropertyHandle_Location->GetChildHandle("Z").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_Location)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Location->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f) // copied from FComponentTransformDetails
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SVectorInputBox)
		.bColorAxisLabels(true)
		.Font(DetailBuilder.GetDetailFont())
		.X_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Location_X)
		.Y_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Location_Y)
		.Z_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Location_Z)
		.OnXCommitted_Static(&SetPropertyValue<float>, PropertyHandle_Location_X)
		.OnYCommitted_Static(&SetPropertyValue<float>, PropertyHandle_Location_Y)
		.OnZCommitted_Static(&SetPropertyValue<float>, PropertyHandle_Location_Z)
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Rotation = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Rotation));
	TSharedRef<IPropertyHandle> PropertyHandle_Rotation_Roll  = PropertyHandle_Rotation->GetChildHandle("Roll").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Rotation_Pitch = PropertyHandle_Rotation->GetChildHandle("Pitch").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Rotation_Yaw   = PropertyHandle_Rotation->GetChildHandle("Yaw").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_Rotation)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Rotation->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f) // copied from FComponentTransformDetails
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SRotatorInputBox)
		.bColorAxisLabels(true)
		.AllowResponsiveLayout(true)
		.Font(DetailBuilder.GetDetailFont())
		.Roll_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Rotation_Roll)
		.Pitch_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Rotation_Pitch)
		.Yaw_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Rotation_Yaw)
		.OnYawCommitted_Static(&SetPropertyValue<float>, PropertyHandle_Rotation_Yaw) // not allowed to roll or pitch landscape
		.OnYawChanged_Lambda([=](float NewValue){ ensure(PropertyHandle_Rotation_Yaw->SetValue(NewValue, EPropertyValueSetFlags::InteractiveChange) == FPropertyAccess::Success); })
	];

	TSharedRef<IPropertyHandle> PropertyHandle_Scale = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_Scale));
	TSharedRef<IPropertyHandle> PropertyHandle_Scale_X = PropertyHandle_Scale->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Scale_Y = PropertyHandle_Scale->GetChildHandle("Y").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_Scale_Z = PropertyHandle_Scale->GetChildHandle("Z").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_Scale)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_Scale->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f * 3.0f) // copied from FComponentTransformDetails
	.MaxDesiredWidth(125.0f * 3.0f)
	[
		SNew(SVectorInputBox)
		.bColorAxisLabels(true)
		.Font(DetailBuilder.GetDetailFont())
		.X_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Scale_X)
		.Y_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Scale_Y)
		.Z_Static(&GetOptionalPropertyValue<float>, PropertyHandle_Scale_Z)
		.OnXCommitted_Static(&SetScale, PropertyHandle_Scale_X)
		.OnYCommitted_Static(&SetScale, PropertyHandle_Scale_Y)
		.OnZCommitted_Static(&SetScale, PropertyHandle_Scale_Z)
	];

	TSharedRef<IPropertyHandle> PropertyHandle_QuadsPerSection = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_QuadsPerSection));
	NewLandscapeCategory.AddProperty(PropertyHandle_QuadsPerSection)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_QuadsPerSection->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SComboButton)
		.OnGetMenuContent_Static(&GetSectionSizeMenu, PropertyHandle_QuadsPerSection)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text_Static(&GetSectionSize, PropertyHandle_QuadsPerSection)
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_SectionsPerComponent = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_SectionsPerComponent));
	NewLandscapeCategory.AddProperty(PropertyHandle_SectionsPerComponent)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_SectionsPerComponent->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SComboButton)
		.OnGetMenuContent_Static(&GetSectionsPerComponentMenu, PropertyHandle_SectionsPerComponent)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text_Static(&GetSectionsPerComponent, PropertyHandle_SectionsPerComponent)
		]
	];

	TSharedRef<IPropertyHandle> PropertyHandle_ComponentCount = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, NewLandscape_ComponentCount));
	TSharedRef<IPropertyHandle> PropertyHandle_ComponentCount_X = PropertyHandle_ComponentCount->GetChildHandle("X").ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ComponentCount_Y = PropertyHandle_ComponentCount->GetChildHandle("Y").ToSharedRef();
	NewLandscapeCategory.AddProperty(PropertyHandle_ComponentCount)
	.CustomWidget()
	.NameContent()
	[
		PropertyHandle_ComponentCount->CreatePropertyNameWidget()
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
			.MaxValue(32)
			.MinSliderValue(1)
			.MaxSliderValue(32)
			.AllowSpin(true)
			.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
			.Value_Static(&FLandscapeEditorDetailCustomization_Base::OnGetValue<int32>, PropertyHandle_ComponentCount_X)
			.OnValueChanged_Static(&FLandscapeEditorDetailCustomization_Base::OnValueChanged<int32>, PropertyHandle_ComponentCount_X)
			.OnValueCommitted_Static(&FLandscapeEditorDetailCustomization_Base::OnValueCommitted<int32>, PropertyHandle_ComponentCount_X)
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
			.MaxValue(32)
			.MinSliderValue(1)
			.MaxSliderValue(32)
			.AllowSpin(true)
			.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
			.Value_Static(&FLandscapeEditorDetailCustomization_Base::OnGetValue<int32>, PropertyHandle_ComponentCount_Y)
			.OnValueChanged_Static(&FLandscapeEditorDetailCustomization_Base::OnValueChanged<int32>, PropertyHandle_ComponentCount_Y)
			.OnValueCommitted_Static(&FLandscapeEditorDetailCustomization_Base::OnValueCommitted<int32>, PropertyHandle_ComponentCount_Y)
		]
	];

	NewLandscapeCategory.AddCustomRow(LOCTEXT("Resolution", "Overall Resolution"))
	.RowTag("LandscapeEditor.OverallResolution")
	.NameContent()
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("Resolution", "Overall Resolution"))
			.ToolTipText(TAttribute<FText>(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetOverallResolutionTooltip))
		]
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SNumericEntryBox<int32>)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(8192)
			.MinSliderValue(1)
			.MaxSliderValue(8192)
			.AllowSpin(true)
			//.MinSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMinLandscapeResolution))
			//.MaxSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMaxLandscapeResolution))
			.Value(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionX)
			.OnValueChanged(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionX)
			.OnValueCommitted(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCommitLandscapeResolutionX)
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
		.Padding(0,0,12,0) // Line up with the other properties due to having no reset to default button
		[
			SNew(SNumericEntryBox<int32>)
			.Font(DetailBuilder.GetDetailFont())
			.MinValue(1)
			.MaxValue(8192)
			.MinSliderValue(1)
			.MaxSliderValue(8192)
			.AllowSpin(true)
			//.MinSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMinLandscapeResolution))
			//.MaxSliderValue(TAttribute<TOptional<int32> >(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetMaxLandscapeResolution))
			.Value(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionY)
			.OnValueChanged(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionY)
			.OnValueCommitted(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCommitLandscapeResolutionY)
		]
	];

	NewLandscapeCategory.AddCustomRow(LOCTEXT("TotalComponents", "Total Components"))
	.RowTag("LandscapeEditor.TotalComponents")
	.NameContent()
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(DetailBuilder.GetDetailFont())
			.Text(LOCTEXT("TotalComponents", "Total Components"))
			.ToolTipText(LOCTEXT("NewLandscape_TotalComponents", "The total number of components that will be created for this landscape."))
		]
	]
	.ValueContent()
	[
		SNew(SBox)
		.Padding(FMargin(0,0,12,0)) // Line up with the other properties due to having no reset to default button
		[
			SNew(SEditableTextBox)
			.IsReadOnly(true)
			.Font(DetailBuilder.GetDetailFont())
			.Text(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetTotalComponentCount)
		]
	];

	NewLandscapeCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::NewLandscape)
			.Text(LOCTEXT("FillWorld", "Fill World"))
			.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("FillWorldButton"), TEXT("LevelEditorToolBox")))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnFillWorldButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)
			.Text(LOCTEXT("FitToData", "Fit To Data"))
			.AddMetaData<FTagMetaData>(TEXT("ImportButton"))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnFitImportDataButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		//[
		//]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::NewLandscape)
			.Text(LOCTEXT("Create", "Create"))
			.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("CreateButton"), TEXT("LevelEditorToolBox")))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Visibility_Static(&GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)
			.Text(LOCTEXT("Import", "Import"))
			.OnClicked(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked)
			.IsEnabled(this, &FLandscapeEditorDetailCustomization_NewLandscape::GetImportButtonIsEnabled)
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText FLandscapeEditorDetailCustomization_NewLandscape::GetOverallResolutionTooltip() const
{
	return (GetEditorMode() && GetEditorMode()->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
	? LOCTEXT("ImportLandscape_OverallResolution", "Overall final resolution of the imported landscape in vertices")
	: LOCTEXT("NewLandscape_OverallResolution", "Overall final resolution of the new landscape in vertices");
}

void FLandscapeEditorDetailCustomization_NewLandscape::SetScale(float NewValue, ETextCommit::Type, TSharedRef<IPropertyHandle> PropertyHandle)
{
	float OldValue = 0;
	PropertyHandle->GetValue(OldValue);

	if (NewValue == 0)
	{
		if (OldValue < 0)
		{
			NewValue = -1;
		}
		else
		{
			NewValue = 1;
		}
	}

	ensure(PropertyHandle->SetValue(NewValue) == FPropertyAccess::Success);

	// Make X and Y scale match
	FName PropertyName = PropertyHandle->GetProperty()->GetFName();
	if (PropertyName == "X")
	{
		TSharedRef<IPropertyHandle> PropertyHandle_Y = PropertyHandle->GetParentHandle()->GetChildHandle("Y").ToSharedRef();
		ensure(PropertyHandle_Y->SetValue(NewValue) == FPropertyAccess::Success);
	}
	else if (PropertyName == "Y")
	{
		TSharedRef<IPropertyHandle> PropertyHandle_X = PropertyHandle->GetParentHandle()->GetChildHandle("X").ToSharedRef();
		ensure(PropertyHandle_X->SetValue(NewValue) == FPropertyAccess::Success);
	}
}

TSharedRef<SWidget> FLandscapeEditorDetailCustomization_NewLandscape::GetSectionSizeMenu(TSharedRef<IPropertyHandle> PropertyHandle)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	for (int32 i = 0; i < ARRAY_COUNT(SectionSizes); i++)
	{
		MenuBuilder.AddMenuEntry(FText::Format(LOCTEXT("NxNQuads", "{0}\u00D7{0} Quads"), FText::AsNumber(SectionSizes[i])), FText::GetEmpty(), FSlateIcon(), FExecuteAction::CreateStatic(&OnChangeSectionSize, PropertyHandle, SectionSizes[i]));
	}

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeSectionSize(TSharedRef<IPropertyHandle> PropertyHandle, int32 NewSize)
{
	ensure(PropertyHandle->SetValue(NewSize) == FPropertyAccess::Success);
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetSectionSize(TSharedRef<IPropertyHandle> PropertyHandle)
{
	int32 QuadsPerSection = 0;
	FPropertyAccess::Result Result = PropertyHandle->GetValue(QuadsPerSection);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	return FText::Format(LOCTEXT("NxNQuads", "{0}\u00D7{0} Quads"), FText::AsNumber(QuadsPerSection));
}

TSharedRef<SWidget> FLandscapeEditorDetailCustomization_NewLandscape::GetSectionsPerComponentMenu(TSharedRef<IPropertyHandle> PropertyHandle)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	for (int32 i = 0; i < ARRAY_COUNT(NumSections); i++)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Width"), NumSections[i]);
		Args.Add(TEXT("Height"), NumSections[i]);
		MenuBuilder.AddMenuEntry(FText::Format(NumSections[i] == 1 ? LOCTEXT("1x1Section", "{Width}\u00D7{Height} Section") : LOCTEXT("NxNSections", "{Width}\u00D7{Height} Sections"), Args), FText::GetEmpty(), FSlateIcon(), FExecuteAction::CreateStatic(&OnChangeSectionsPerComponent, PropertyHandle, NumSections[i]));
	}

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeSectionsPerComponent(TSharedRef<IPropertyHandle> PropertyHandle, int32 NewSize)
{
	ensure(PropertyHandle->SetValue(NewSize) == FPropertyAccess::Success);
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetSectionsPerComponent(TSharedRef<IPropertyHandle> PropertyHandle)
{
	int32 SectionsPerComponent = 0;
	FPropertyAccess::Result Result = PropertyHandle->GetValue(SectionsPerComponent);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Width"), SectionsPerComponent);
	Args.Add(TEXT("Height"), SectionsPerComponent);
	return FText::Format(SectionsPerComponent == 1 ? LOCTEXT("1x1Section", "{Width}\u00D7{Height} Section") : LOCTEXT("NxNSections", "{Width}\u00D7{Height} Sections"), Args);
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionX() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		return (LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X * LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection + 1);
	}

	return 0;
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionX(int32 NewValue)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		int32 NewComponentCountX = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		if (NewComponentCountX != LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X)
		{
			if (!GEditor->IsTransactionActive())
			{
				GEditor->BeginTransaction(LOCTEXT("ChangeResolutionX_Transaction", "Change Landscape Resolution X"));
			}

			LandscapeEdMode->UISettings->Modify();
			LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = NewComponentCountX;
		}
	}
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnCommitLandscapeResolutionX(int32 NewValue, ETextCommit::Type CommitInfo)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (!GEditor->IsTransactionActive())
		{
			GEditor->BeginTransaction(LOCTEXT("ChangeResolutionX_Transaction", "Change Landscape Resolution X"));
		}
		LandscapeEdMode->UISettings->Modify();
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		GEditor->EndTransaction();
	}
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetLandscapeResolutionY() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		return (LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y * LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection + 1);
	}

	return 0;
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeLandscapeResolutionY(int32 NewValue)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		int32 NewComponentCountY = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		if (NewComponentCountY != LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y)
		{
			if (!GEditor->IsTransactionActive())
			{
				GEditor->BeginTransaction(LOCTEXT("ChangeResolutionY_Transaction", "Change Landscape Resolution Y"));
			}
			
			LandscapeEdMode->UISettings->Modify();
			LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = NewComponentCountY;
		}
	}
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnCommitLandscapeResolutionY(int32 NewValue, ETextCommit::Type CommitInfo)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (!GEditor->IsTransactionActive())
		{
			GEditor->BeginTransaction(LOCTEXT("ChangeResolutionY_Transaction", "Change Landscape Resolution Y"));
		}
		LandscapeEdMode->UISettings->Modify();
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = LandscapeEdMode->UISettings->CalcComponentsCount(NewValue);
		GEditor->EndTransaction();
	}
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetMinLandscapeResolution() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		// Min size is one component
		return (LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection + 1);
	}

	return 0;
}

TOptional<int32> FLandscapeEditorDetailCustomization_NewLandscape::GetMaxLandscapeResolution() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		// Max size is either whole components below 8192 verts, or 32 components
		const int32 QuadsPerComponent = (LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection);
		//return (float)(FMath::Min(32, FMath::FloorToInt(8191 / QuadsPerComponent)) * QuadsPerComponent);
		return (8191 / QuadsPerComponent) * QuadsPerComponent + 1;
	}

	return 0;
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetTotalComponentCount() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		return FText::AsNumber(LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X * LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y);
	}

	return FText::FromString(TEXT("---"));
}


EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetVisibilityOnlyInNewLandscapeMode(ENewLandscapePreviewMode::Type value)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (LandscapeEdMode->NewLandscapePreviewMode == value)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

ECheckBoxState FLandscapeEditorDetailCustomization_NewLandscape::NewLandscapeModeIsChecked(ENewLandscapePreviewMode::Type value) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (LandscapeEdMode->NewLandscapePreviewMode == value)
		{
			return ECheckBoxState::Checked;
		}
	}
	return ECheckBoxState::Unchecked;
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnNewLandscapeModeChanged(ECheckBoxState NewCheckedState, ENewLandscapePreviewMode::Type value)
{
	if (NewCheckedState == ECheckBoxState::Checked)
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		if (LandscapeEdMode != nullptr)
		{
			LandscapeEdMode->NewLandscapePreviewMode = value;

			if (value == ENewLandscapePreviewMode::ImportLandscape)
			{
				LandscapeEdMode->NewLandscapePreviewMode = ENewLandscapePreviewMode::ImportLandscape;
			}
		}
	}
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnCreateButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr && 
		LandscapeEdMode->GetWorld() != nullptr && 
		LandscapeEdMode->GetWorld()->GetCurrentLevel()->bIsVisible)
	{
		// Initialize heightmap data
		TArray<uint16> Data;
		const int32 ComponentCountX = LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X;
		const int32 ComponentCountY = LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y;
		const int32 QuadsPerComponent = LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection;
		const int32 SizeX = ComponentCountX * QuadsPerComponent + 1;
		const int32 SizeY = ComponentCountY * QuadsPerComponent + 1;
		Data.AddUninitialized(SizeX * SizeY);
		uint16* WordData = Data.GetData();

		// Initialize blank heightmap data
		for (int32 i = 0; i < SizeX * SizeY; i++)
		{
			WordData[i] = 32768;
		}

		TArray<FLandscapeImportLayerInfo> ImportLayers;

		if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::NewLandscape)
		{
			const auto& ImportLandscapeLayersList = LandscapeEdMode->UISettings->ImportLandscape_Layers;
			ImportLayers.Reserve(ImportLandscapeLayersList.Num());

			// Fill in LayerInfos array and allocate data
			for (const FLandscapeImportLayer& UIImportLayer : ImportLandscapeLayersList)
			{
				FLandscapeImportLayerInfo ImportLayer = FLandscapeImportLayerInfo(UIImportLayer.LayerName);
				ImportLayer.LayerInfo = UIImportLayer.LayerInfo;
				ImportLayer.SourceFilePath = "";
				ImportLayer.LayerData = TArray<uint8>();
				ImportLayers.Add(MoveTemp(ImportLayer));
			}

			// Fill the first weight-blended layer to 100%
			if (FLandscapeImportLayerInfo* FirstBlendedLayer = ImportLayers.FindByPredicate([](const FLandscapeImportLayerInfo& ImportLayer) { return ImportLayer.LayerInfo && !ImportLayer.LayerInfo->bNoWeightBlend; }))
			{
				FirstBlendedLayer->LayerData.AddUninitialized(SizeX * SizeY);

				uint8* ByteData = FirstBlendedLayer->LayerData.GetData();
				for (int32 i = 0; i < SizeX * SizeY; i++)
				{
					ByteData[i] = 255;
				}
			}
		}
		else if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
		{
			const uint32 ImportSizeX = LandscapeEdMode->UISettings->ImportLandscape_Width;
			const uint32 ImportSizeY = LandscapeEdMode->UISettings->ImportLandscape_Height;

			if (LandscapeEdMode->UISettings->ImportLandscape_HeightmapImportResult == ELandscapeImportResult::Error)
			{
				// Cancel import
				return FReply::Handled();
			}

			TArray<FLandscapeImportLayer>& ImportLandscapeLayersList = LandscapeEdMode->UISettings->ImportLandscape_Layers;
			ImportLayers.Reserve(ImportLandscapeLayersList.Num());

			// Fill in LayerInfos array and allocate data
			for (FLandscapeImportLayer& UIImportLayer : ImportLandscapeLayersList)
			{
				ImportLayers.Add((const FLandscapeImportLayer&)UIImportLayer); //slicing is fine here
				FLandscapeImportLayerInfo& ImportLayer = ImportLayers.Last();

				if (ImportLayer.LayerInfo != nullptr && ImportLayer.SourceFilePath != "")
				{
					ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
					const ILandscapeWeightmapFileFormat* WeightmapFormat = LandscapeEditorModule.GetWeightmapFormatByExtension(*FPaths::GetExtension(ImportLayer.SourceFilePath, true));

					if (WeightmapFormat)
					{
						FLandscapeWeightmapImportData WeightmapImportData = WeightmapFormat->Import(*ImportLayer.SourceFilePath, ImportLayer.LayerName, {ImportSizeX, ImportSizeY});
						UIImportLayer.ImportResult = WeightmapImportData.ResultCode;
						UIImportLayer.ErrorMessage = WeightmapImportData.ErrorMessage;
						ImportLayer.LayerData = MoveTemp(WeightmapImportData.Data);
					}
					else
					{
						UIImportLayer.ImportResult = ELandscapeImportResult::Error;
						UIImportLayer.ErrorMessage = LOCTEXT("Import_UnknownFileType", "File type not recognised");
					}

					if (UIImportLayer.ImportResult == ELandscapeImportResult::Error)
					{
						ImportLayer.LayerData.Empty();
						FMessageDialog::Open(EAppMsgType::Ok, UIImportLayer.ErrorMessage);

						// Cancel import
						return FReply::Handled();
					}
				}
			}

			const TArray<uint16>& ImportData = LandscapeEdMode->UISettings->GetImportLandscapeData();
			if (ImportData.Num() != 0)
			{
				const int32 OffsetX = (int32)(SizeX - ImportSizeX) / 2;
				const int32 OffsetY = (int32)(SizeY - ImportSizeY) / 2;

				// Heightmap
				Data = LandscapeEditorUtils::ExpandData(ImportData,
					0, 0, ImportSizeX - 1, ImportSizeY - 1,
					-OffsetX, -OffsetY, SizeX - OffsetX - 1, SizeY - OffsetY - 1);

				// Layers
				for (int32 LayerIdx = 0; LayerIdx < ImportLayers.Num(); LayerIdx++)
				{
					TArray<uint8>& ImportLayerData = ImportLayers[LayerIdx].LayerData;
					if (ImportLayerData.Num())
					{
						ImportLayerData = LandscapeEditorUtils::ExpandData(ImportLayerData,
							0, 0, ImportSizeX - 1, ImportSizeY - 1,
							-OffsetX, -OffsetY, SizeX - OffsetX - 1, SizeY - OffsetY - 1);
					}
				}
			}
		}

		FScopedTransaction Transaction(LOCTEXT("Undo", "Creating New Landscape"));

		const FVector Offset = FTransform(LandscapeEdMode->UISettings->NewLandscape_Rotation, FVector::ZeroVector, LandscapeEdMode->UISettings->NewLandscape_Scale).TransformVector(FVector(-ComponentCountX * QuadsPerComponent / 2, -ComponentCountY * QuadsPerComponent / 2, 0));
		ALandscape* Landscape = LandscapeEdMode->GetWorld()->SpawnActor<ALandscape>(LandscapeEdMode->UISettings->NewLandscape_Location + Offset, LandscapeEdMode->UISettings->NewLandscape_Rotation);
		Landscape->LandscapeMaterial = LandscapeEdMode->UISettings->NewLandscape_Material.Get();
		Landscape->SetActorRelativeScale3D(LandscapeEdMode->UISettings->NewLandscape_Scale);
		Landscape->Import(FGuid::NewGuid(), 0, 0, SizeX-1, SizeY-1, LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent, LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection, Data.GetData(), nullptr, ImportLayers, LandscapeEdMode->UISettings->ImportLandscape_AlphamapType);

		// automatically calculate a lighting LOD that won't crash lightmass (hopefully)
		// < 2048x2048 -> LOD0
		// >=2048x2048 -> LOD1
		// >= 4096x4096 -> LOD2
		// >= 8192x8192 -> LOD3
		Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), (uint32)2);

		if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
		{
			Landscape->ReimportHeightmapFilePath = LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename;
		}

		ULandscapeInfo* LandscapeInfo = Landscape->CreateLandscapeInfo();
		LandscapeInfo->UpdateLayerInfoMap(Landscape);

		// Import doesn't fill in the LayerInfo for layers with no data, do that now
		const TArray<FLandscapeImportLayer>& ImportLandscapeLayersList = LandscapeEdMode->UISettings->ImportLandscape_Layers;
		for (int32 i = 0; i < ImportLandscapeLayersList.Num(); i++)
		{
			if (ImportLandscapeLayersList[i].LayerInfo != nullptr)
			{
				if (LandscapeEdMode->NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
				{
					Landscape->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(ImportLandscapeLayersList[i].LayerInfo, ImportLandscapeLayersList[i].SourceFilePath));
				}
				else
				{
					Landscape->EditorLayerSettings.Add(FLandscapeEditorLayerSettings(ImportLandscapeLayersList[i].LayerInfo));
				}

				int32 LayerInfoIndex = LandscapeInfo->GetLayerInfoIndex(ImportLandscapeLayersList[i].LayerName);
				if (ensure(LayerInfoIndex != INDEX_NONE))
				{
					FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[LayerInfoIndex];
					LayerSettings.LayerInfoObj = ImportLandscapeLayersList[i].LayerInfo;
				}
			}
		}

		LandscapeEdMode->UpdateLandscapeList();
		LandscapeEdMode->CurrentToolTarget.LandscapeInfo = LandscapeInfo;
		LandscapeEdMode->CurrentToolTarget.TargetType = ELandscapeToolTargetType::Heightmap;
		LandscapeEdMode->CurrentToolTarget.LayerInfo = nullptr;
		LandscapeEdMode->CurrentToolTarget.LayerName = NAME_None;
		LandscapeEdMode->UpdateTargetList();

		LandscapeEdMode->SetCurrentTool("Select"); // change tool so switching back to the manage mode doesn't give "New Landscape" again
		LandscapeEdMode->SetCurrentTool("Sculpt"); // change to sculpting mode and tool

		if (LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
		{
			ALandscapeProxy* LandscapeProxy = LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			LandscapeProxy->OnMaterialChangedDelegate().AddRaw(LandscapeEdMode, &FEdModeLandscape::OnLandscapeMaterialChangedDelegate);
		}
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnFillWorldButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		FVector& NewLandscapeLocation = LandscapeEdMode->UISettings->NewLandscape_Location;
		NewLandscapeLocation.X = 0;
		NewLandscapeLocation.Y = 0;

		const int32 QuadsPerComponent = LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent * LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection;
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = FMath::CeilToInt(WORLD_MAX / QuadsPerComponent / LandscapeEdMode->UISettings->NewLandscape_Scale.X);
		LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = FMath::CeilToInt(WORLD_MAX / QuadsPerComponent / LandscapeEdMode->UISettings->NewLandscape_Scale.Y);
		LandscapeEdMode->UISettings->NewLandscape_ClampSize();
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnFitImportDataButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		ChooseBestComponentSizeForImport(LandscapeEdMode);
	}

	return FReply::Handled();
}

bool FLandscapeEditorDetailCustomization_NewLandscape::GetImportButtonIsEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (LandscapeEdMode->UISettings->ImportLandscape_HeightmapImportResult == ELandscapeImportResult::Error)
		{
			return false;
		}

		for (int32 i = 0; i < LandscapeEdMode->UISettings->ImportLandscape_Layers.Num(); ++i)
		{
			if (LandscapeEdMode->UISettings->ImportLandscape_Layers[i].ImportResult == ELandscapeImportResult::Error)
			{
				return false;
			}
		}

		return true;
	}
	return false;
}

EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetHeightmapErrorVisibility(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapImportResult)
{
	ELandscapeImportResult HeightmapImportResult;
	FPropertyAccess::Result Result = PropertyHandle_HeightmapImportResult->GetValue((uint8&)HeightmapImportResult);

	if (Result == FPropertyAccess::Fail)
	{
		return EVisibility::Collapsed;
	}

	if (Result == FPropertyAccess::MultipleValues)
	{
		return EVisibility::Visible;
	}

	if (HeightmapImportResult != ELandscapeImportResult::Success)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FSlateColor FLandscapeEditorDetailCustomization_NewLandscape::GetHeightmapErrorColor(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapImportResult)
{
	ELandscapeImportResult HeightmapImportResult;
	FPropertyAccess::Result Result = PropertyHandle_HeightmapImportResult->GetValue((uint8&)HeightmapImportResult);

	if (Result == FPropertyAccess::Fail ||
		Result == FPropertyAccess::MultipleValues)
	{
		return FCoreStyle::Get().GetColor("ErrorReporting.BackgroundColor");
	}

	switch (HeightmapImportResult)
	{
	case ELandscapeImportResult::Success:
		return FCoreStyle::Get().GetColor("InfoReporting.BackgroundColor");
	case ELandscapeImportResult::Warning:
		return FCoreStyle::Get().GetColor("ErrorReporting.WarningBackgroundColor");
	case ELandscapeImportResult::Error:
		return FCoreStyle::Get().GetColor("ErrorReporting.BackgroundColor");
	default:
		check(0);
		return FSlateColor();
	}
}

void FLandscapeEditorDetailCustomization_NewLandscape::SetImportHeightmapFilenameString(const FText& NewValue, ETextCommit::Type CommitInfo, TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename)
{
	FString HeightmapFilename = NewValue.ToString();
	ensure(PropertyHandle_HeightmapFilename->SetValue(HeightmapFilename) == FPropertyAccess::Success);
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnImportHeightmapFilenameChanged()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		ImportResolutions.Reset(1);
		LandscapeEdMode->UISettings->ImportLandscape_Width = 0;
		LandscapeEdMode->UISettings->ImportLandscape_Height = 0;
		LandscapeEdMode->UISettings->ClearImportLandscapeData();
		LandscapeEdMode->UISettings->ImportLandscape_HeightmapImportResult = ELandscapeImportResult::Success;
		LandscapeEdMode->UISettings->ImportLandscape_HeightmapErrorMessage = FText();

		if (!LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename.IsEmpty())
		{
			ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
			const ILandscapeHeightmapFileFormat* HeightmapFormat = LandscapeEditorModule.GetHeightmapFormatByExtension(*FPaths::GetExtension(LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename, true));

			if (HeightmapFormat)
			{
				FLandscapeHeightmapInfo HeightmapImportInfo = HeightmapFormat->Validate(*LandscapeEdMode->UISettings->ImportLandscape_HeightmapFilename);
				LandscapeEdMode->UISettings->ImportLandscape_HeightmapImportResult = HeightmapImportInfo.ResultCode;
				LandscapeEdMode->UISettings->ImportLandscape_HeightmapErrorMessage = HeightmapImportInfo.ErrorMessage;
				ImportResolutions = MoveTemp(HeightmapImportInfo.PossibleResolutions);
				if (HeightmapImportInfo.DataScale.IsSet())
				{
					LandscapeEdMode->UISettings->NewLandscape_Scale = HeightmapImportInfo.DataScale.GetValue();
					LandscapeEdMode->UISettings->NewLandscape_Scale.Z *= LANDSCAPE_INV_ZSCALE;
				}
			}
			else
			{
				LandscapeEdMode->UISettings->ImportLandscape_HeightmapImportResult = ELandscapeImportResult::Error;
				LandscapeEdMode->UISettings->ImportLandscape_HeightmapErrorMessage = LOCTEXT("Import_UnknownFileType", "File type not recognised");
			}
		}

		if (ImportResolutions.Num() > 0)
		{
			int32 i = ImportResolutions.Num() / 2;
			LandscapeEdMode->UISettings->ImportLandscape_Width = ImportResolutions[i].Width;
			LandscapeEdMode->UISettings->ImportLandscape_Height = ImportResolutions[i].Height;
			LandscapeEdMode->UISettings->ImportLandscapeData();
			ChooseBestComponentSizeForImport(LandscapeEdMode);
		}
	}
}

FReply FLandscapeEditorDetailCustomization_NewLandscape::OnImportHeightmapFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	check(LandscapeEdMode != nullptr);

	// Prompt the user for the Filenames
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform != nullptr)
	{
		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
		const TCHAR* FileTypes = LandscapeEditorModule.GetHeightmapImportDialogTypeString();

		TArray<FString> OpenFilenames;
		bool bOpened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("UnrealEd", "Import", "Import").ToString(),
			LandscapeEdMode->UISettings->LastImportPath,
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OpenFilenames);

		if (bOpened)
		{
			ensure(PropertyHandle_HeightmapFilename->SetValue(OpenFilenames[0]) == FPropertyAccess::Success);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(OpenFilenames[0]);
		}
	}

	return FReply::Handled();

}

TSharedRef<SWidget> FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolutionMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	for (int32 i = 0; i < ImportResolutions.Num(); i++)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Width"), ImportResolutions[i].Width);
		Args.Add(TEXT("Height"), ImportResolutions[i].Height);
		MenuBuilder.AddMenuEntry(FText::Format(LOCTEXT("ImportResolution_Format", "{Width}\u00D7{Height}"), Args), FText(), FSlateIcon(), FExecuteAction::CreateSP(this, &FLandscapeEditorDetailCustomization_NewLandscape::OnChangeImportLandscapeResolution, i));
	}

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorDetailCustomization_NewLandscape::OnChangeImportLandscapeResolution(int32 Index)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		LandscapeEdMode->UISettings->ImportLandscape_Width = ImportResolutions[Index].Width;
		LandscapeEdMode->UISettings->ImportLandscape_Height = ImportResolutions[Index].Height;
		LandscapeEdMode->UISettings->ClearImportLandscapeData();
		ChooseBestComponentSizeForImport(LandscapeEdMode);
	}
}

FText FLandscapeEditorDetailCustomization_NewLandscape::GetImportLandscapeResolution() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		const int32 Width = LandscapeEdMode->UISettings->ImportLandscape_Width;
		const int32 Height = LandscapeEdMode->UISettings->ImportLandscape_Height;
		if (Width != 0 && Height != 0)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Width"), Width);
			Args.Add(TEXT("Height"), Height);
			return FText::Format(LOCTEXT("ImportResolution_Format", "{Width}\u00D7{Height}"), Args);
		}
		else
		{
			return LOCTEXT("ImportResolution_Invalid", "(invalid)");
		}
	}

	return FText::GetEmpty();
}

void FLandscapeEditorDetailCustomization_NewLandscape::ChooseBestComponentSizeForImport(FEdModeLandscape* LandscapeEdMode)
{
	int32 Width = LandscapeEdMode->UISettings->ImportLandscape_Width;
	int32 Height = LandscapeEdMode->UISettings->ImportLandscape_Height;

	bool bFoundMatch = false;
	if (Width > 0 && Height > 0)
	{
		// Try to find a section size and number of sections that exactly matches the dimensions of the heightfield
		for (int32 SectionSizesIdx = ARRAY_COUNT(SectionSizes) - 1; SectionSizesIdx >= 0; SectionSizesIdx--)
		{
			for (int32 NumSectionsIdx = ARRAY_COUNT(NumSections) - 1; NumSectionsIdx >= 0; NumSectionsIdx--)
			{
				int32 ss = SectionSizes[SectionSizesIdx];
				int32 ns = NumSections[NumSectionsIdx];

				if (((Width - 1) % (ss * ns)) == 0 && ((Width - 1) / (ss * ns)) <= 32 &&
					((Height - 1) % (ss * ns)) == 0 && ((Height - 1) / (ss * ns)) <= 32)
				{
					bFoundMatch = true;
					LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection = ss;
					LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent = ns;
					LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = (Width - 1) / (ss * ns);
					LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = (Height - 1) / (ss * ns);
					LandscapeEdMode->UISettings->NewLandscape_ClampSize();
					break;
				}
			}
			if (bFoundMatch)
			{
				break;
			}
		}

		if (!bFoundMatch)
		{
			// if there was no exact match, try increasing the section size until we encompass the whole heightmap
			const int32 CurrentSectionSize = LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection;
			const int32 CurrentNumSections = LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent;
			for (int32 SectionSizesIdx = 0; SectionSizesIdx < ARRAY_COUNT(SectionSizes); SectionSizesIdx++)
			{
				if (SectionSizes[SectionSizesIdx] < CurrentSectionSize)
				{
					continue;
				}

				const int32 ComponentsX = FMath::DivideAndRoundUp((Width - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
				const int32 ComponentsY = FMath::DivideAndRoundUp((Height - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
				if (ComponentsX <= 32 && ComponentsY <= 32)
				{
					bFoundMatch = true;
					LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection = SectionSizes[SectionSizesIdx];
					//LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent = ;
					LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = ComponentsX;
					LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = ComponentsY;
					LandscapeEdMode->UISettings->NewLandscape_ClampSize();
					break;
				}
			}
		}

		if (!bFoundMatch)
		{
			// if the heightmap is very large, fall back to using the largest values we support
			const int32 MaxSectionSize = SectionSizes[ARRAY_COUNT(SectionSizes) - 1];
			const int32 MaxNumSubSections = NumSections[ARRAY_COUNT(NumSections) - 1];
			const int32 ComponentsX = FMath::DivideAndRoundUp((Width - 1), MaxSectionSize * MaxNumSubSections);
			const int32 ComponentsY = FMath::DivideAndRoundUp((Height - 1), MaxSectionSize * MaxNumSubSections);

			bFoundMatch = true;
			LandscapeEdMode->UISettings->NewLandscape_QuadsPerSection = MaxSectionSize;
			LandscapeEdMode->UISettings->NewLandscape_SectionsPerComponent = MaxNumSubSections;
			LandscapeEdMode->UISettings->NewLandscape_ComponentCount.X = ComponentsX;
			LandscapeEdMode->UISettings->NewLandscape_ComponentCount.Y = ComponentsY;
			LandscapeEdMode->UISettings->NewLandscape_ClampSize();
		}

		check(bFoundMatch);
	}
}

EVisibility FLandscapeEditorDetailCustomization_NewLandscape::GetMaterialTipVisibility() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (LandscapeEdMode->UISettings->ImportLandscape_Layers.Num() == 0)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

//////////////////////////////////////////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FLandscapeEditorStructCustomization_FLandscapeImportLayer::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorStructCustomization_FLandscapeImportLayer);
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<IPropertyHandle> PropertyHandle_LayerName = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, LayerName)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, LayerInfo)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_SourceFilePath = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, SourceFilePath)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ThumbnailMIC = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, ThumbnailMIC)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ImportResult = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, ImportResult)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertyHandle_ErrorMessage = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLandscapeImportLayer, ErrorMessage)).ToSharedRef();

	FName LayerName;
	FText LayerNameText;
	FPropertyAccess::Result Result = PropertyHandle_LayerName->GetValue(LayerName);
	checkSlow(Result == FPropertyAccess::Success);
	LayerNameText = FText::FromName(LayerName);
	if (Result == FPropertyAccess::MultipleValues)
	{
		LayerName = NAME_None;
		LayerNameText = NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	UObject* ThumbnailMIC = nullptr;
	Result = PropertyHandle_ThumbnailMIC->GetValue(ThumbnailMIC);
	checkSlow(Result == FPropertyAccess::Success);

	ChildBuilder.AddCustomRow(LayerNameText)
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(STextBlock)
			.Font(StructCustomizationUtils.GetRegularFont())
			.Text(LayerNameText)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(2))
		[
			SNew(SLandscapeAssetThumbnail, ThumbnailMIC, StructCustomizationUtils.GetThumbnailPool().ToSharedRef())
			.ThumbnailSize(FIntPoint(48, 48))
		]
	]
	.ValueContent()
	.MinDesiredWidth(250.0f) // copied from SPropertyEditorAsset::GetDesiredWidth
	.MaxDesiredWidth(0)
	[
		SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0,0,12,0)) // Line up with the other properties due to having no reset to default button
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(ULandscapeLayerInfoObject::StaticClass())
					.PropertyHandle(PropertyHandle_LayerInfo)
					.OnShouldFilterAsset_Static(&ShouldFilterLayerInfo, LayerName)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					//.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
					.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
					.HasDownArrow(false)
					//.ContentPadding(0)
					.ContentPadding(4.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.IsFocusable(false)
					.ToolTipText(LOCTEXT("Target_Create", "Create Layer Info"))
					.Visibility_Static(&GetImportLayerCreateVisibility, PropertyHandle_LayerInfo)
					.OnGetMenuContent_Static(&OnGetImportLayerCreateMenu, PropertyHandle_LayerInfo, LayerName)
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("LandscapeEditor.Target_Create"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.Visibility_Static(&FLandscapeEditorDetailCustomization_NewLandscape::GetVisibilityOnlyInNewLandscapeMode, ENewLandscapePreviewMode::ImportLandscape)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 2, 0)
				[
					SNew(SErrorText)
					.Visibility_Static(&GetErrorVisibility, PropertyHandle_ImportResult)
					.BackgroundColor_Static(&GetErrorColor, PropertyHandle_ImportResult)
					.ErrorText(NSLOCTEXT("UnrealEd", "Error", "!"))
					.ToolTip(
						SNew(SToolTip)
						.Text_Static(&GetErrorText, PropertyHandle_ErrorMessage)
					)
				]
				+ SHorizontalBox::Slot()
				[
					PropertyHandle_SourceFilePath->CreatePropertyValueWidget()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1, 0, 0, 0)
				[
					SNew(SButton)
					.ContentPadding(FMargin(4, 0))
					.Text(NSLOCTEXT("UnrealEd", "GenericOpenDialog", "..."))
					.OnClicked_Static(&FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnLayerFilenameButtonClicked, PropertyHandle_SourceFilePath)
				]
			]
		]
	];
}

FReply FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnLayerFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerFilename)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	check(LandscapeEdMode != nullptr);

	// Prompt the user for the Filenames
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform != nullptr)
	{
		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
		const TCHAR* FileTypes = LandscapeEditorModule.GetWeightmapImportDialogTypeString();

		TArray<FString> OpenFilenames;
		bool bOpened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("UnrealEd", "Import", "Import").ToString(),
			LandscapeEdMode->UISettings->LastImportPath,
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OpenFilenames);

		if (bOpened)
		{
			ensure(PropertyHandle_LayerFilename->SetValue(OpenFilenames[0]) == FPropertyAccess::Success);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(OpenFilenames[0]);
		}
	}

	return FReply::Handled();
}

bool FLandscapeEditorStructCustomization_FLandscapeImportLayer::ShouldFilterLayerInfo(const FAssetData& AssetData, FName LayerName)
{
	const FName LayerNameMetaData = AssetData.GetTagValueRef<FName>("LayerName");
	if (!LayerNameMetaData.IsNone())
	{
		return LayerNameMetaData != LayerName;
	}

	ULandscapeLayerInfoObject* LayerInfo = CastChecked<ULandscapeLayerInfoObject>(AssetData.GetAsset());
	return LayerInfo->LayerName != LayerName;
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetImportLayerCreateVisibility(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo)
{
	UObject* LayerInfoAsUObject = nullptr;
	if (PropertyHandle_LayerInfo->GetValue(LayerInfoAsUObject) != FPropertyAccess::Fail &&
		LayerInfoAsUObject == nullptr)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

TSharedRef<SWidget> FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnGetImportLayerCreateMenu(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo, FName LayerName)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(LOCTEXT("Target_Create_Blended", "Weight-Blended Layer (normal)"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&OnImportLayerCreateClicked, PropertyHandle_LayerInfo, LayerName, false)));

	MenuBuilder.AddMenuEntry(LOCTEXT("Target_Create_NoWeightBlend", "Non Weight-Blended Layer"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&OnImportLayerCreateClicked, PropertyHandle_LayerInfo, LayerName, true)));

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorStructCustomization_FLandscapeImportLayer::OnImportLayerCreateClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo, FName LayerName, bool bNoWeightBlend)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		// Hack as we don't have a direct world pointer in the EdMode...
		ULevel* Level = LandscapeEdMode->CurrentGizmoActor->GetWorld()->GetCurrentLevel();

		// Build default layer object name and package name
		FName LayerObjectName = FName(*FString::Printf(TEXT("%s_LayerInfo"), *LayerName.ToString()));
		FString Path = Level->GetOutermost()->GetName() + TEXT("_sharedassets/");
		if (Path.StartsWith("/Temp/"))
		{
			Path = FString("/Game/") + Path.RightChop(FString("/Temp/").Len());
		}
		FString PackageName = Path + LayerObjectName.ToString();

		TSharedRef<SDlgPickAssetPath> NewLayerDlg =
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("CreateNewLayerInfo", "Create New Landscape Layer Info Object"))
			.DefaultAssetPath(FText::FromString(PackageName));

		if (NewLayerDlg->ShowModal() != EAppReturnType::Cancel)
		{
			PackageName = NewLayerDlg->GetFullAssetPath().ToString();
			LayerObjectName = FName(*NewLayerDlg->GetAssetName().ToString());

			UPackage* Package = CreatePackage(nullptr, *PackageName);
			ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, LayerObjectName, RF_Public | RF_Standalone | RF_Transactional);
			LayerInfo->LayerName = LayerName;
			LayerInfo->bNoWeightBlend = bNoWeightBlend;

			const UObject* LayerInfoAsUObject = LayerInfo; // HACK: If SetValue took a reference to a const ptr (T* const &) or a non-reference (T*) then this cast wouldn't be necessary
			ensure(PropertyHandle_LayerInfo->SetValue(LayerInfoAsUObject) == FPropertyAccess::Success);

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(LayerInfo);

			// Mark the package dirty...
			Package->MarkPackageDirty();

			// Show in the content browser
			TArray<UObject*> Objects;
			Objects.Add(LayerInfo);
			GEditor->SyncBrowserToObjects(Objects);
		}
	}
}

EVisibility FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetErrorVisibility(TSharedRef<IPropertyHandle> PropertyHandle_ImportResult)
{
	ELandscapeImportResult WeightmapImportResult;
	FPropertyAccess::Result Result = PropertyHandle_ImportResult->GetValue((uint8&)WeightmapImportResult);

	if (Result == FPropertyAccess::Fail ||
		Result == FPropertyAccess::MultipleValues)
	{
		return EVisibility::Visible;
	}

	if (WeightmapImportResult != ELandscapeImportResult::Success)
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FSlateColor FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetErrorColor(TSharedRef<IPropertyHandle> PropertyHandle_ImportResult)
{
	ELandscapeImportResult WeightmapImportResult;
	FPropertyAccess::Result Result = PropertyHandle_ImportResult->GetValue((uint8&)WeightmapImportResult);
	check(Result == FPropertyAccess::Success);

	if (Result == FPropertyAccess::MultipleValues)
	{
		return FCoreStyle::Get().GetColor("ErrorReporting.BackgroundColor");
	}

	switch (WeightmapImportResult)
	{
	case ELandscapeImportResult::Success:
		return FCoreStyle::Get().GetColor("InfoReporting.BackgroundColor");
	case ELandscapeImportResult::Warning:
		return FCoreStyle::Get().GetColor("ErrorReporting.WarningBackgroundColor");
	case ELandscapeImportResult::Error:
		return FCoreStyle::Get().GetColor("ErrorReporting.BackgroundColor");
	default:
		check(0);
		return FSlateColor();
	}
}

FText FLandscapeEditorStructCustomization_FLandscapeImportLayer::GetErrorText(TSharedRef<IPropertyHandle> PropertyHandle_ErrorMessage)
{
	FText ErrorMessage;
	FPropertyAccess::Result Result = PropertyHandle_ErrorMessage->GetValue(ErrorMessage);
	if (Result == FPropertyAccess::Fail)
	{
		return LOCTEXT("Import_LayerUnknownError", "Unknown Error");
	}
	else if (Result == FPropertyAccess::MultipleValues)
	{
		return NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values");
	}

	return ErrorMessage;
}

#undef LOCTEXT_NAMESPACE
