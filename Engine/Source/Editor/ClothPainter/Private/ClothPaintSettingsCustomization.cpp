// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ClothPaintSettingsCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "STextBlock.h"
#include "DetailLayoutBuilder.h"
#include "ClothPaintSettings.h"
#include "ClothingAsset.h"
#include "SComboBox.h"
#include "ClothPainter.h"
#include "ClothPaintToolBase.h"

#define LOCTEXT_NAMESPACE "ClothPaintSettingsCustomization"

FClothPaintSettingsCustomization::FClothPaintSettingsCustomization(FClothPainter* InPainter) 
	: Painter(InPainter)
{

}

FClothPaintSettingsCustomization::~FClothPaintSettingsCustomization()
{
	
}

TSharedRef<IDetailCustomization> FClothPaintSettingsCustomization::MakeInstance(FClothPainter* InPainter)
{
	return MakeShareable(new FClothPaintSettingsCustomization(InPainter));
}

void FClothPaintSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);
	
	UClothPainterSettings* PainterSettings = nullptr;
	for(TWeakObjectPtr<UObject>& WeakObj : CustomizedObjects)
	{
		if(UObject* CustomizedObject = WeakObj.Get())
		{
			if(UClothPainterSettings* Settings = Cast<UClothPainterSettings>(CustomizedObject))
			{
				PainterSettings = Settings;
			}
		}
	}

	const FName ClothCategory = "ClothPainting";
	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(ClothCategory);

	// Add tool selection from the tools array on the painter
	FText ToolText = LOCTEXT("ToolSelectionRow", "Tool");
	FDetailWidgetRow& ToolSelectionRow = CategoryBuilder.AddCustomRow(ToolText);

	ToolSelectionRow.NameContent()
		[
			SNew(STextBlock)
				.Text(ToolText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	ToolSelectionRow.ValueContent()
		[
			SNew(SComboBox<TSharedPtr<FClothPaintToolBase>>)
				.OptionsSource(&Painter->Tools)
				.OnGenerateWidget(this, &FClothPaintSettingsCustomization::OnGenerateToolComboRow)
				.OnSelectionChanged(this, &FClothPaintSettingsCustomization::OnHandleToolSelection, &DetailBuilder)
				[
					SNew(STextBlock)
						.Text(this, &FClothPaintSettingsCustomization::GetToolComboText)
						.Font(IDetailLayoutBuilder::GetDetailFont())
				]
		];
}

TSharedRef<SWidget> FClothPaintSettingsCustomization::OnGenerateToolComboRow(TSharedPtr<FClothPaintToolBase> InItem)
{
	FText RowText = LOCTEXT("ToolComboRow_Error", "Invalid");

	if(InItem.IsValid())
	{
		RowText = InItem->GetDisplayName();
	}

	return SNew(STextBlock).Text(RowText);
}

void FClothPaintSettingsCustomization::OnHandleToolSelection(TSharedPtr<FClothPaintToolBase> InItem, ESelectInfo::Type InSelectInfo, IDetailLayoutBuilder* DetailBuider)
{
	if(InItem.IsValid() && Painter && Painter->SelectedTool != InItem)
	{
		// Update selection
		Painter->SetTool(InItem);
		Painter->Refresh();
	}
}

FText FClothPaintSettingsCustomization::GetToolComboText() const
{
	if(Painter && Painter->SelectedTool.IsValid())
	{
		return Painter->SelectedTool->GetDisplayName();
	}

	return LOCTEXT("ToolComboRow_Error", "Invalid");
}

#undef LOCTEXT_NAMESPACE

TSharedRef<IDetailCustomization> FClothPaintBrushSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FClothPaintBrushSettingsCustomization());
}

void FClothPaintBrushSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> ColorViewProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPaintBrushSettings, ColorViewMode));

	ColorViewProp->MarkHiddenByCustomization();
}
