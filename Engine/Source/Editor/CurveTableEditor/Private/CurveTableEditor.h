// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateStructs.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Toolkits/IToolkitHost.h"
#include "ICurveTableEditor.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "CurveTableEditorHandle.h"

struct FCurveTableEditorColumnHeaderData
{
	/** Unique ID used to identify this column */
	FName ColumnId;

	/** Display name of this column */
	FText DisplayName;

	/** The calculated width of this column taking into account the cell data for each row */
	float DesiredColumnWidth;
};

struct FCurveTableEditorRowListViewData
{
	/** Unique ID used to identify this row */
	FName RowId;

	/** Display name of this row */
	FText DisplayName;

	/** Array corresponding to each cell in this row */
	TArray<FText> CellData;

	/** Handle to the row */
	FCurveTableEditorHandle RowHandle;
};

/** The manner in which curve tables are displayed */
enum class ECurveTableViewMode : int32
{
	/** Displays values in a spreadsheet-like table */
	Grid,

	/** Displays values as curves */
	CurveTable,
};

typedef TSharedPtr<FCurveTableEditorColumnHeaderData> FCurveTableEditorColumnHeaderDataPtr;
typedef TSharedPtr<FCurveTableEditorRowListViewData>  FCurveTableEditorRowListViewDataPtr;

/** Viewer/editor for a CurveTable */
class FCurveTableEditor :
	public ICurveTableEditor
{
	friend class SCurveTableListViewRow;
	friend class SCurveTableCurveViewRow;

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
	void InitCurveTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UCurveTable* Table );

	/** Destructor */
	virtual ~FCurveTableEditor();

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** Get the curve table being edited */
	const UCurveTable* GetCurveTable() const;

	/**	Spawns the tab with the curve table inside */
	TSharedRef<SDockTab> SpawnTab_CurveTable( const FSpawnTabArgs& Args );

	/** Get the mode that we are displaying data in */
	ECurveTableViewMode GetViewMode() const { return ViewMode; }

private:

	/** Add extra menu items */
	void ExtendMenu();

	/** Bind commands to delegates */
	void BindCommands();

	/** Update the cached state of this curve table, and then reflect that new state in the UI */
	void RefreshCachedCurveTable();

	/** Cache the data from the current curve table so that it can be shown in the editor */
	void CacheDataTableForEditing();

	/** Make the widget for a row name entry in the data table row list view */
	TSharedRef<ITableRow> MakeRowNameWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable);

	/** Make the widget for a row entry in the data table row list view */
	TSharedRef<ITableRow> MakeRowWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable);

	/** Make the widget for a cell entry in the data table row list view */
	TSharedRef<SWidget> MakeCellWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const int32 InRowIndex, const FName& InColumnId);

	/** Make the curve widget for a row entry in the data table row list view */
	TSharedRef<SWidget> MakeCurveWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const int32 InRowIndex);

	/** Called when the row names list is scrolled - used to keep the two list views in sync */
	void OnRowNamesListViewScrolled(double InScrollOffset);

	/** Called when the cell names list view is scrolled - used to keep the two list views in sync */
	void OnCellsListViewScrolled(double InScrollOffset);

	/** Get the width to use for the row names column */
	FOptionalSize GetRowNameColumnWidth() const;

	/** Called when an asset has finished being imported */
	void OnPostReimport(UObject* InObject, bool);

	/** Control control visibility based on view mode */
	EVisibility GetGridViewControlsVisibility() const;

	/** Control control visibility based on view mode */
	EVisibility GetCurveViewControlsVisibility() const;

	/** Toggle between curve & grid view */
	void ToggleViewMode();

	/** Get whether the curve view checkbox should be toggled on */
	bool IsCurveViewChecked() const;

	/** Array of the columns that are available for editing */
	TArray<FCurveTableEditorColumnHeaderDataPtr> AvailableColumns;

	/** Array of the rows that are available for editing */
	TArray<FCurveTableEditorRowListViewDataPtr> AvailableRows;

	/** Header row containing entries for each column in AvailableColumns */
	TSharedPtr<SHeaderRow> ColumnNamesHeaderRow;

	/** List view responsible for showing the row names column */
	TSharedPtr<SListView<FCurveTableEditorRowListViewDataPtr>> RowNamesListView;

	/** List view responsible for showing the rows from AvailableColumns */
	TSharedPtr<SListView<FCurveTableEditorRowListViewDataPtr>> CellsListView;

	/** Menu extender */
	TSharedPtr<FExtender> MenuExtender;

	/** Width of the row name column */
	float RowNameColumnWidth;

	/**	The tab id for the curve table tab */
	static const FName CurveTableTabId;

	/** The column id for the row name list view column */
	static const FName RowNameColumnId;

	/** The manner in which curve tables are displayed */
	ECurveTableViewMode ViewMode;
};
