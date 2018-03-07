// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * UI to pick options when importing a data table
 */

#pragma once

#include "CoreMinimal.h"
#include "Factories/CSVImportFactory.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

class UNREALED_API SCSVImportOptions : public SCompoundWidget
{
private:
	/** Typedef for curve enum pointers */
	typedef TSharedPtr<ERichCurveInterpMode>		CurveInterpModePtr;

public:
	SLATE_BEGIN_ARGS(SCSVImportOptions)
		: _WidgetWindow()
		, _FullPath()
		{}

		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
		SLATE_ARGUMENT(FText, FullPath)
	SLATE_END_ARGS()

	SCSVImportOptions()
		: bImport(false)
		, SelectedImportType(ECSVImportType::ECSV_DataTable)
		, SelectedStruct(NULL)
		{}

	void Construct(const FArguments& InArgs);

	/** If we should import */
	bool ShouldImport();

	/** Get the row struct we selected */
	UScriptStruct* GetSelectedRowStruct();

	/** Get the import type we selected */
	ECSVImportType GetSelectedImportType();

	/** Get the interpolation mode we selected */
	ERichCurveInterpMode GetSelectedCurveIterpMode();

	/** Whether to show table row options */
	EVisibility GetTableRowOptionVis() const;

	/** Whether to show table row options */
	EVisibility GetCurveTypeVis() const;

	FString GetImportTypeText(TSharedPtr<ECSVImportType> Type) const;

	/** Called to create a widget for each struct */
	TSharedRef<SWidget> MakeImportTypeItemWidget(TSharedPtr<ECSVImportType> Type);

	/** Called to create a widget for each struct */
	TSharedRef<SWidget> MakeRowStructItemWidget(UScriptStruct* Struct);

	FString GetCurveTypeText(CurveInterpModePtr InterpMode) const;

	/** Called to create a widget for each curve interpolation enum */
	TSharedRef<SWidget> MakeCurveTypeWidget(CurveInterpModePtr InterpMode);

	/** Called when 'OK' button is pressed */
	FReply OnImport();

	/** Called when 'Cancel' button is pressed */
	FReply OnCancel();

	FText GetSelectedItemText() const;

	FText GetSelectedRowOptionText() const;

	FText GetSelectedCurveTypeText() const;

private:
	/** Whether we should go ahead with import */
	bool										bImport;

	/** Window that owns us */
	TWeakPtr< SWindow >							WidgetWindow;

	// Import type

	/** List of import types to pick from, drives combo box */
	TArray< TSharedPtr<ECSVImportType> >						ImportTypes;

	/** The combo box */
	TSharedPtr< SComboBox< TSharedPtr<ECSVImportType> > >		ImportTypeCombo;

	/** Indicates what kind of asset we want to make from the CSV file */
	ECSVImportType												SelectedImportType;


	// Row type

	/** Array of row struct options */
	TArray< UScriptStruct* >						RowStructs;

	/** The row struct combo box */
	TSharedPtr< SComboBox<UScriptStruct*> >			RowStructCombo;

	/** The selected row struct */
	UScriptStruct*									SelectedStruct;

	/** The curve interpolation combo box */
	TSharedPtr< SComboBox<CurveInterpModePtr> >		CurveInterpCombo;

	/** All available curve interpolation modes */
	TArray< CurveInterpModePtr >					CurveInterpModes;

	/** The selected curve interpolation type */
	ERichCurveInterpMode							SelectedCurveInterpMode;
};
