// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Types/SlateStructs.h"
#include "Widgets/SWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Toolkits/IToolkitHost.h"
#include "IDataTableEditor.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "EditorUndoClient.h"
#include "Kismet2/StructureEditorUtils.h"
#include "DataTableEditorUtils.h"

class FJsonObject;

DECLARE_DELEGATE_OneParam(FOnRowHighlighted, FName /*Row name*/);

/** Viewer/editor for a DataTable */
class FDataTableEditor : public IDataTableEditor
	, public FEditorUndoClient
	, public FStructureEditorUtils::INotifyOnStructChanged
	, public FDataTableEditorUtils::INotifyOnDataTableChanged
{
	friend class SDataTableListViewRow;

public:

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/**
	 * Edits the specified table
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	Table					The table to edit
	 */
	void InitDataTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDataTable* Table );

	/** Constructor */
	FDataTableEditor();

	/** Destructor */
	virtual ~FDataTableEditor();

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	void HandleUndoRedo();

	// INotifyOnStructChanged
	virtual void PreChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override;
	virtual void PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info) override;

	// INotifyOnDataTableChanged
	virtual void PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) override;
	virtual void PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) override;
	virtual void SelectionChange(const UDataTable* Changed, FName RowName) override;

	/** Get the data table being edited */
	const UDataTable* GetDataTable() const;

	void HandlePostChange();

	void SetHighlightedRow(FName Name);

private:

	void RefreshCachedDataTable(const FName InCachedSelection = NAME_None, const bool bUpdateEvenIfValid = false);

	void UpdateVisibleRows(const FName InCachedSelection = NAME_None, const bool bUpdateEvenIfValid = false);

	void RestoreCachedSelection(const FName InCachedSelection, const bool bUpdateEvenIfValid = false);

	FText GetFilterText() const;

	void OnFilterTextChanged(const FText& InFilterText);

	FSlateColor GetRowTextColor(FName RowName) const;
	FText GetCellText(FDataTableEditorRowListViewDataPtr InRowDataPointer, int32 ColumnIndex) const;
	FText GetCellToolTipText(FDataTableEditorRowListViewDataPtr InRowDataPointer, int32 ColumnIndex) const;

	TSharedRef<SVerticalBox> CreateContentBox();

	TSharedRef<SWidget> CreateRowEditorBox();

	/**	Spawns the tab with the data table inside */
	TSharedRef<SDockTab> SpawnTab_DataTable( const FSpawnTabArgs& Args );

	/**	Spawns the tab with the Row Editor inside */
	TSharedRef<SDockTab> SpawnTab_RowEditor(const FSpawnTabArgs& Args);

	FOptionalSize GetRowNameColumnWidth() const;

	float GetColumnWidth(const int32 ColumnIndex) const;

	void OnColumnResized(const float NewWidth, const int32 ColumnIndex);

	void LoadLayoutData();

	void SaveLayoutData();

	/** Make the widget for a row name entry in the data table row list view */
	TSharedRef<ITableRow> MakeRowNameWidget(FDataTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable);

	/** Make the widget for a row entry in the data table row list view */
	TSharedRef<ITableRow> MakeRowWidget(FDataTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable);

	/** Make the widget for a cell entry in the data table row list view */
	TSharedRef<SWidget> MakeCellWidget(FDataTableEditorRowListViewDataPtr InRowDataPtr, const int32 InRowIndex, const FName& InColumnId);

	void OnRowNamesListViewScrolled(double InScrollOffset);

	void OnCellsListViewScrolled(double InScrollOffset);

	void OnRowSelectionChanged(FDataTableEditorRowListViewDataPtr InNewSelection, ESelectInfo::Type InSelectInfo);

private:

	/** Struct holding information about the current column widths */
	struct FColumnWidth
	{
		FColumnWidth()
			: bIsAutoSized(true)
			, CurrentWidth(0.0f)
		{
		}

		/** True if this column is being auto-sized rather than sized by the user */
		bool bIsAutoSized;
		/** The width of the column, either sized by the user, or auto-sized */
		float CurrentWidth;
	};

	/** Array of the columns that are available for editing */
	TArray<FDataTableEditorColumnHeaderDataPtr> AvailableColumns;

	/** Array of the rows that are available for editing */
	TArray<FDataTableEditorRowListViewDataPtr> AvailableRows;
	
	/** Array of the rows that match the active filter(s) */
	TArray<FDataTableEditorRowListViewDataPtr> VisibleRows;

	/** Header row containing entries for each column in AvailableColumns */
	TSharedPtr<SHeaderRow> ColumnNamesHeaderRow;

	/** List view responsible for showing the row names column */
	TSharedPtr<SListView<FDataTableEditorRowListViewDataPtr>> RowNamesListView;

	/** List view responsible for showing the rows in VisibleRows for each entry in AvailableColumns */
	TSharedPtr<SListView<FDataTableEditorRowListViewDataPtr>> CellsListView;

	/** Width of the row name column */
	float RowNameColumnWidth;

	/** Widths of data table cell columns */
	TArray<FColumnWidth> ColumnWidths;

	/** The layout data for the currently loaded data table */
	TSharedPtr<FJsonObject> LayoutData;

	/** The name of the currently selected row */
	FName HighlightedRowName;

	/** The current filter text applied to the data table */
	FText ActiveFilterText;

	FOnRowHighlighted CallbackOnRowHighlighted;

	FSimpleDelegate CallbackOnDataTableUndoRedo;

	/**	The tab id for the data table tab */
	static const FName DataTableTabId;

	/**	The tab id for the row editor tab */
	static const FName RowEditorTabId;

	/** The column id for the row name list view column */
	static const FName RowNameColumnId;
};
