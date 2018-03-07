// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet2/ListenerManager.h"

struct FDataTableEditorColumnHeaderData
{
	/** Unique ID used to identify this column */
	FName ColumnId;

	/** Display name of this column */
	FText DisplayName;

	/** The calculated width of this column taking into account the cell data for each row */
	float DesiredColumnWidth;
};

struct FDataTableEditorRowListViewData
{
	/** Unique ID used to identify this row */
	FName RowId;

	/** Display name of this row */
	FText DisplayName;

	/** The calculated height of this row taking into account the cell data for each column */
	float DesiredRowHeight;

	/** Array corresponding to each cell in this row */
	TArray<FText> CellData;
};

typedef TSharedPtr<FDataTableEditorColumnHeaderData> FDataTableEditorColumnHeaderDataPtr;
typedef TSharedPtr<FDataTableEditorRowListViewData>  FDataTableEditorRowListViewDataPtr;

struct UNREALED_API FDataTableEditorUtils
{
	enum class EDataTableChangeInfo
	{
		/** The data corresponding to a single row has been changed */
		RowData,
		/** The data corresponding to the entire list of rows has been changed */
		RowList,
	};

	enum class ERowMoveDirection
	{
		Up,
		Down,
	};

	class FDataTableEditorManager : public FListenerManager < UDataTable, EDataTableChangeInfo >
	{
		FDataTableEditorManager() {}
	public:
		UNREALED_API static FDataTableEditorManager& Get();

		class UNREALED_API ListenerType : public InnerListenerType<FDataTableEditorManager>
		{
		public:
			virtual void SelectionChange(const UDataTable* DataTable, FName RowName) { }
		};
	};

	typedef FDataTableEditorManager::ListenerType INotifyOnDataTableChanged;

	static bool RemoveRow(UDataTable* DataTable, FName Name);
	static uint8* AddRow(UDataTable* DataTable, FName RowName);
	static bool RenameRow(UDataTable* DataTable, FName OldName, FName NewName);
	static bool MoveRow(UDataTable* DataTable, FName RowName, ERowMoveDirection Direction, int32 NumRowsToMoveBy = 1);
	static bool SelectRow(UDataTable* DataTable, FName RowName);
	static bool DiffersFromDefault(UDataTable* DataTable, FName RowName);
	static bool ResetToDefault(UDataTable* DataTable, FName RowName);

	static void BroadcastPreChange(UDataTable* DataTable, EDataTableChangeInfo Info);
	static void BroadcastPostChange(UDataTable* DataTable, EDataTableChangeInfo Info);

	static void CacheDataTableForEditing(const UDataTable* DataTable, TArray<FDataTableEditorColumnHeaderDataPtr>& OutAvailableColumns, TArray<FDataTableEditorRowListViewDataPtr>& OutAvailableRows);

	static TArray<UScriptStruct*> GetPossibleStructs();
	/** Utility function which verifies that the specified struct type is viable for data tables */
	static bool IsValidTableStruct(UScriptStruct* Struct);
};
