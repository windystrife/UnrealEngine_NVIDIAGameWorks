// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SCSVImportOptions.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Engine/UserDefinedStruct.h"



#define LOCTEXT_NAMESPACE "CSVImportFactory"

void SCSVImportOptions::Construct(const FArguments& InArgs)
{
	WidgetWindow = InArgs._WidgetWindow;

	// Make array of enum pointers
	TSharedPtr<ECSVImportType> DataTableTypePtr = MakeShareable(new ECSVImportType(ECSVImportType::ECSV_DataTable));
	ImportTypes.Add( DataTableTypePtr );
	ImportTypes.Add(MakeShareable(new ECSVImportType(ECSVImportType::ECSV_CurveTable)));
	ImportTypes.Add(MakeShareable(new ECSVImportType(ECSVImportType::ECSV_CurveFloat)));
	ImportTypes.Add(MakeShareable(new ECSVImportType(ECSVImportType::ECSV_CurveVector)));

	// Find table row struct info
	UScriptStruct* TableRowStruct = FindObjectChecked<UScriptStruct>(ANY_PACKAGE, TEXT("TableRowBase"));
	if (TableRowStruct != NULL)
	{
		// Make combo of table rowstruct options
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Struct = *It;
			// If a child of the table row struct base, but not itself
			const bool bBasedOnTableRowBase = Struct->IsChildOf(TableRowStruct) && (Struct != TableRowStruct);
			const bool bUDStruct = Struct->IsA<UUserDefinedStruct>();
			const bool bValidStruct = (Struct->GetOutermost() != GetTransientPackage());
			if ((bBasedOnTableRowBase || bUDStruct) && bValidStruct)
			{
				RowStructs.Add(Struct);
			}
		}

		// Alphabetically sort the row structs by name
		RowStructs.Sort([](const UScriptStruct& ElementA, const UScriptStruct& ElementB) { return (ElementA.GetName() < ElementB.GetName()); } );
	}

	// Create widget
	this->ChildSlot
	[
		SNew(SBorder)
		. BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Background")))
		. Padding(10)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Visibility( InArgs._FullPath.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible )
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
						.Text(InArgs._FullPath)
					]
				]
			]

			// Import type
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(STextBlock)
				.Text( LOCTEXT("ChooseAssetType", "Import As:") )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ImportTypeCombo, SComboBox< TSharedPtr<ECSVImportType> >)
				.OptionsSource( &ImportTypes )
				.OnGenerateWidget( this, &SCSVImportOptions::MakeImportTypeItemWidget )
				[
					SNew(STextBlock)
					.Text(this, &SCSVImportOptions::GetSelectedItemText)
				]
			]
			// Data row struct
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text( LOCTEXT("ChooseRowType", "Choose DataTable Row Type:") )
				.Visibility( this, &SCSVImportOptions::GetTableRowOptionVis )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(RowStructCombo, SComboBox<UScriptStruct*>)
				.OptionsSource( &RowStructs )
				.OnGenerateWidget( this, &SCSVImportOptions::MakeRowStructItemWidget )
				.Visibility( this, &SCSVImportOptions::GetTableRowOptionVis )
				[
					SNew(STextBlock)
					.Text(this, &SCSVImportOptions::GetSelectedRowOptionText)
				]
			]
			// Curve interpolation
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text( LOCTEXT("ChooseCurveType", "Choose Curve Interpolation Type:") )
				.Visibility( this, &SCSVImportOptions::GetCurveTypeVis )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(CurveInterpCombo, SComboBox<CurveInterpModePtr>)
				.OptionsSource( &CurveInterpModes )
				.OnGenerateWidget( this, &SCSVImportOptions::MakeCurveTypeWidget )
				.Visibility( this, &SCSVImportOptions::GetCurveTypeVis )
				[
					SNew(STextBlock)
					.Text(this, &SCSVImportOptions::GetSelectedCurveTypeText)
				]
			]
			// Ok/Cancel
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("OK", "OK"))
					.OnClicked( this, &SCSVImportOptions::OnImport )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked( this, &SCSVImportOptions::OnCancel )
				]
			]
		]
	];

	// set-up selection
	ImportTypeCombo->SetSelectedItem(DataTableTypePtr);

	// Populate the valid interploation modes
	{
		CurveInterpModes.Add( MakeShareable( new ERichCurveInterpMode(ERichCurveInterpMode::RCIM_Constant) ) );
		CurveInterpModes.Add( MakeShareable( new ERichCurveInterpMode(ERichCurveInterpMode::RCIM_Linear) ) );
		CurveInterpModes.Add( MakeShareable( new ERichCurveInterpMode(ERichCurveInterpMode::RCIM_Cubic) ) );
	}

	// NB: Both combo boxes default to first item in their options lists as initially selected item
}

	/** If we should import */
bool SCSVImportOptions::ShouldImport()
{
	return ((SelectedStruct != NULL) || GetSelectedImportType() != ECSVImportType::ECSV_DataTable) && bImport;
}

/** Get the row struct we selected */
UScriptStruct* SCSVImportOptions::GetSelectedRowStruct()
{
	return SelectedStruct;
}

/** Get the import type we selected */
ECSVImportType SCSVImportOptions::GetSelectedImportType()
{
	return SelectedImportType;
}

/** Get the interpolation mode we selected */
ERichCurveInterpMode SCSVImportOptions::GetSelectedCurveIterpMode()
{
	return SelectedCurveInterpMode;
}
	
/** Whether to show table row options */
EVisibility SCSVImportOptions::GetTableRowOptionVis() const
{
	return (ImportTypeCombo.IsValid() && *ImportTypeCombo->GetSelectedItem() == ECSVImportType::ECSV_DataTable) ? EVisibility::Visible : EVisibility::Collapsed;
}

/** Whether to show table row options */
EVisibility SCSVImportOptions::GetCurveTypeVis() const
{
	return (ImportTypeCombo.IsValid() && *ImportTypeCombo->GetSelectedItem() == ECSVImportType::ECSV_CurveTable) ? EVisibility::Visible : EVisibility::Collapsed;
}

FString SCSVImportOptions::GetImportTypeText(TSharedPtr<ECSVImportType> Type) const
{
	FString EnumString;
	if (*Type == ECSVImportType::ECSV_DataTable)
	{
		EnumString = TEXT("DataTable");
	}
	else if (*Type == ECSVImportType::ECSV_CurveTable)
	{
		EnumString = TEXT("CurveTable");
	}
	else if (*Type == ECSVImportType::ECSV_CurveFloat)
	{
		EnumString = TEXT("Float Curve");
	}
	else if (*Type == ECSVImportType::ECSV_CurveVector)
	{
		EnumString = TEXT("Vector Curve");
	}
	return EnumString;
}

/** Called to create a widget for each struct */
TSharedRef<SWidget> SCSVImportOptions::MakeImportTypeItemWidget(TSharedPtr<ECSVImportType> Type)
{
	return	SNew(STextBlock)
			.Text(FText::FromString(GetImportTypeText(Type)));
}

/** Called to create a widget for each struct */
TSharedRef<SWidget> SCSVImportOptions::MakeRowStructItemWidget(UScriptStruct* Struct)
{
	check( Struct != NULL );
	return	SNew(STextBlock)
			.Text(FText::FromString(Struct->GetName()));
}

FString SCSVImportOptions::GetCurveTypeText(CurveInterpModePtr InterpMode) const
{
	FString EnumString;

	switch(*InterpMode)
	{
		case ERichCurveInterpMode::RCIM_Constant : 
			EnumString = TEXT("Constant");
			break;

		case ERichCurveInterpMode::RCIM_Linear : 
			EnumString = TEXT("Linear");
			break;

		case ERichCurveInterpMode::RCIM_Cubic : 
			EnumString = TEXT("Cubic");
			break;
	}
	return EnumString;
}

/** Called to create a widget for each curve interpolation enum */
TSharedRef<SWidget> SCSVImportOptions::MakeCurveTypeWidget(CurveInterpModePtr InterpMode)
{
	FString Label = GetCurveTypeText(InterpMode);
	return SNew(STextBlock) .Text( FText::FromString(Label) );
}

/** Called when 'OK' button is pressed */
FReply SCSVImportOptions::OnImport()
{
	SelectedStruct = RowStructCombo->GetSelectedItem();
	SelectedImportType = *ImportTypeCombo->GetSelectedItem();
	if(CurveInterpCombo->GetSelectedItem().IsValid())
	{
		SelectedCurveInterpMode = *CurveInterpCombo->GetSelectedItem();
	}
	bImport = true;
	if ( WidgetWindow.IsValid() )
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

/** Called when 'Cancel' button is pressed */
FReply SCSVImportOptions::OnCancel()
{
	bImport = false;
	if ( WidgetWindow.IsValid() )
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FText SCSVImportOptions::GetSelectedItemText() const
{
	TSharedPtr<ECSVImportType> SelectedType = ImportTypeCombo->GetSelectedItem();

	return (SelectedType.IsValid())
		? FText::FromString(GetImportTypeText(SelectedType))
		: FText::GetEmpty();
}

FText SCSVImportOptions::GetSelectedRowOptionText() const
{
	UScriptStruct* SelectedScript = RowStructCombo->GetSelectedItem();
	return (SelectedScript)
		? FText::FromString(SelectedScript->GetName())
		: FText::GetEmpty();
}

FText SCSVImportOptions::GetSelectedCurveTypeText() const
{
	CurveInterpModePtr CurveModePtr = CurveInterpCombo->GetSelectedItem();
	return (CurveModePtr.IsValid())
		? FText::FromString(GetCurveTypeText(CurveModePtr))
		: FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
