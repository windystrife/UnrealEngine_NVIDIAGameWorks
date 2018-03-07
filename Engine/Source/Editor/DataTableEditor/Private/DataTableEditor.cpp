// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DataTableEditor.h"
#include "Dom/JsonObject.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Framework/Layout/Overscroll.h"
#include "Widgets/Layout/SScrollBox.h"
#include "EditorStyleSet.h"
#include "DataTableEditorModule.h"
#include "Editor.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Views/SListView.h"
#include "SRowEditor.h"
 
#define LOCTEXT_NAMESPACE "DataTableEditor"

const FName FDataTableEditor::DataTableTabId("DataTableEditor_DataTable");
const FName FDataTableEditor::RowEditorTabId("DataTableEditor_RowEditor");
const FName FDataTableEditor::RowNameColumnId("RowName");

class SDataTableListViewRow : public SMultiColumnTableRow<FDataTableEditorRowListViewDataPtr>
{
public:
	SLATE_BEGIN_ARGS(SDataTableListViewRow) {}
		/** The widget that owns the tree.  We'll only keep a weak reference to it. */
		SLATE_ARGUMENT(TSharedPtr<FDataTableEditor>, DataTableEditor)
		/** The list item for this row */
		SLATE_ARGUMENT(FDataTableEditorRowListViewDataPtr, Item)
	SLATE_END_ARGS()

	/** Construct function for this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		DataTableEditor = InArgs._DataTableEditor;
		Item = InArgs._Item;
		SMultiColumnTableRow<FDataTableEditorRowListViewDataPtr>::Construct(
			FSuperRowType::FArguments()
				.Style(FEditorStyle::Get(), "DataTableEditor.CellListViewRow"), 
			InOwnerTableView
			);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		TSharedPtr<FDataTableEditor> DataTableEditorPtr = DataTableEditor.Pin();
		return (DataTableEditorPtr.IsValid())
			? DataTableEditorPtr->MakeCellWidget(Item, IndexInList, ColumnName)
			: SNullWidget::NullWidget;
	}

private:
	/** Weak reference to the data table editor that owns our list */
	TWeakPtr<FDataTableEditor> DataTableEditor;
	/** The item associated with this row of data */
	FDataTableEditorRowListViewDataPtr Item;
};

void FDataTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_Data Table Editor", "Data Table Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(DataTableTabId, FOnSpawnTab::CreateSP(this, &FDataTableEditor::SpawnTab_DataTable))
		.SetDisplayName(LOCTEXT("DataTableTab", "Data Table"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(RowEditorTabId, FOnSpawnTab::CreateSP(this, &FDataTableEditor::SpawnTab_RowEditor))
		.SetDisplayName(LOCTEXT("RowEditorTab", "Row Editor"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FDataTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(DataTableTabId);
	InTabManager->UnregisterTabSpawner(RowEditorTabId);
}

FDataTableEditor::FDataTableEditor()
{
}

FDataTableEditor::~FDataTableEditor()
{
	GEditor->UnregisterForUndo(this);

	const UDataTable* Table = GetDataTable();
	if (Table)
	{
		SaveLayoutData();
	}
}

void FDataTableEditor::PostUndo(bool bSuccess)
{
	HandleUndoRedo();
}

void FDataTableEditor::PostRedo(bool bSuccess)
{
	HandleUndoRedo();
}

void FDataTableEditor::HandleUndoRedo()
{
	const UDataTable* Table = GetDataTable();
	if (Table)
	{
		HandlePostChange();
		CallbackOnDataTableUndoRedo.ExecuteIfBound();
	}
}

void FDataTableEditor::PreChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
}

void FDataTableEditor::PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	const UDataTable* Table = GetDataTable();
	if (Struct && Table && (Table->RowStruct == Struct))
	{
		HandlePostChange();
	}
}

void FDataTableEditor::SelectionChange(const UDataTable* Changed, FName RowName)
{
	const UDataTable* Table = GetDataTable();
	if (Changed == Table)
	{
		const bool bSelectionChanged = HighlightedRowName != RowName;
		SetHighlightedRow(RowName);

		if (bSelectionChanged)
		{
			CallbackOnRowHighlighted.ExecuteIfBound(HighlightedRowName);
		}
	}
}

void FDataTableEditor::PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
}

void FDataTableEditor::PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
	const UDataTable* Table = GetDataTable();
	if (Changed == Table)
	{
		HandlePostChange();
	}
}

const UDataTable* FDataTableEditor::GetDataTable() const
{
	return Cast<const UDataTable>(GetEditingObject());
}

void FDataTableEditor::HandlePostChange()
{
	// We need to cache and restore the selection here as RefreshCachedDataTable will re-create the list view items
	const FName CachedSelection = HighlightedRowName;
	HighlightedRowName = NAME_None;
	RefreshCachedDataTable(CachedSelection, true/*bUpdateEvenIfValid*/);
}

void FDataTableEditor::InitDataTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDataTable* Table )
{
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_DataTableEditor_Layout_v2" )
	->AddArea
	(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell(true)
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewStack()
			->AddTab(DataTableTabId, ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewStack()
			->AddTab(RowEditorTabId, ETabState::OpenedTab)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FDataTableEditorModule::DataTableEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Table );
	
	FDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FDataTableEditorModule>( "DataTableEditor" );
	AddMenuExtender(DataTableEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	// Support undo/redo
	GEditor->RegisterForUndo(this);

	// @todo toolkit world centric editing
	/*// Setup our tool's layout
	if( IsWorldCentricAssetEditor() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for table properties
		SpawnToolkitTab( DataTableTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/

	// NOTE: Could fill in asset editor commands here!
}

FName FDataTableEditor::GetToolkitFName() const
{
	return FName("DataTableEditor");
}

FText FDataTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "DataTable Editor" );
}

FString FDataTableEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DataTable ").ToString();
}

FLinearColor FDataTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

FSlateColor FDataTableEditor::GetRowTextColor(FName RowName) const
{
	if (RowName == HighlightedRowName)
	{
		return FSlateColor(FColorList::Orange);
	}
	return FSlateColor::UseForeground();
}

FText FDataTableEditor::GetCellText(FDataTableEditorRowListViewDataPtr InRowDataPointer, int32 ColumnIndex) const
{
	if (InRowDataPointer.IsValid() && ColumnIndex < InRowDataPointer->CellData.Num())
	{
		return InRowDataPointer->CellData[ColumnIndex];
	}

	return FText();
}

FText FDataTableEditor::GetCellToolTipText(FDataTableEditorRowListViewDataPtr InRowDataPointer, int32 ColumnIndex) const
{
	FText TooltipText;

	if (ColumnIndex < AvailableColumns.Num())
	{
		TooltipText = AvailableColumns[ColumnIndex]->DisplayName;
	}

	if (InRowDataPointer.IsValid() && ColumnIndex < InRowDataPointer->CellData.Num())
	{
		TooltipText = FText::Format(LOCTEXT("ColumnRowNameFmt", "{0}: {1}"), TooltipText, InRowDataPointer->CellData[ColumnIndex]);
	}

	return TooltipText;
}

FOptionalSize FDataTableEditor::GetRowNameColumnWidth() const
{
	return FOptionalSize(RowNameColumnWidth);
}

float FDataTableEditor::GetColumnWidth(const int32 ColumnIndex) const
{
	if (ColumnWidths.IsValidIndex(ColumnIndex))
	{
		return ColumnWidths[ColumnIndex].CurrentWidth;
	}
	return 0.0f;
}

void FDataTableEditor::OnColumnResized(const float NewWidth, const int32 ColumnIndex)
{
	if (ColumnWidths.IsValidIndex(ColumnIndex))
	{
		FColumnWidth& ColumnWidth = ColumnWidths[ColumnIndex];
		ColumnWidth.bIsAutoSized = false;
		ColumnWidth.CurrentWidth = NewWidth;

		// Update the persistent column widths in the layout data
		{
			if (!LayoutData.IsValid())
			{
				LayoutData = MakeShareable(new FJsonObject());
			}

			TSharedPtr<FJsonObject> LayoutColumnWidths;
			if (!LayoutData->HasField(TEXT("ColumnWidths")))
			{
				LayoutColumnWidths = MakeShareable(new FJsonObject());
				LayoutData->SetObjectField(TEXT("ColumnWidths"), LayoutColumnWidths);
			}
			else
			{
				LayoutColumnWidths = LayoutData->GetObjectField(TEXT("ColumnWidths"));
			}

			const FString& ColumnName = AvailableColumns[ColumnIndex]->ColumnId.ToString();
			LayoutColumnWidths->SetNumberField(ColumnName, NewWidth);
		}
	}
}

void FDataTableEditor::LoadLayoutData()
{
	LayoutData.Reset();

	const UDataTable* Table = GetDataTable();
	if (!Table)
	{
		return;
	}

	const FString LayoutDataFilename = FPaths::ProjectSavedDir() / TEXT("AssetData") / TEXT("DataTableEditorLayout") / Table->GetName() + TEXT(".json");

	FString JsonText;
	if (FFileHelper::LoadFileToString(JsonText, *LayoutDataFilename))
	{
		TSharedRef< TJsonReader<TCHAR> > JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonText);
		FJsonSerializer::Deserialize(JsonReader, LayoutData);
	}
}

void FDataTableEditor::SaveLayoutData()
{
	const UDataTable* Table = GetDataTable();
	if (!Table || !LayoutData.IsValid())
	{
		return;
	}

	const FString LayoutDataFilename = FPaths::ProjectSavedDir() / TEXT("AssetData") / TEXT("DataTableEditorLayout") / Table->GetName() + TEXT(".json");

	FString JsonText;
	TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&JsonText);
	if (FJsonSerializer::Serialize(LayoutData.ToSharedRef(), JsonWriter))
	{
		FFileHelper::SaveStringToFile(JsonText, *LayoutDataFilename);
	}
}

TSharedRef<ITableRow> FDataTableEditor::MakeRowNameWidget(FDataTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(STableRow<FDataTableEditorRowListViewDataPtr>, OwnerTable)
		.Style(FEditorStyle::Get(), "DataTableEditor.NameListViewRow")
		[
			SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			[
				SNew(SBox)
				.HeightOverride(InRowDataPtr->DesiredRowHeight)
				[
					SNew(STextBlock)
					.ColorAndOpacity(this, &FDataTableEditor::GetRowTextColor, InRowDataPtr->RowId)
					.Text(InRowDataPtr->DisplayName)
					.HighlightText(this, &FDataTableEditor::GetFilterText)
				]
			]
		];
}

TSharedRef<ITableRow> FDataTableEditor::MakeRowWidget(FDataTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(SDataTableListViewRow, OwnerTable)
		.DataTableEditor(SharedThis(this))
		.Item(InRowDataPtr);
}

TSharedRef<SWidget> FDataTableEditor::MakeCellWidget(FDataTableEditorRowListViewDataPtr InRowDataPtr, const int32 InRowIndex, const FName& InColumnId)
{
	int32 ColumnIndex = 0;
	for (; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
	{
		const FDataTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];
		if (ColumnData->ColumnId == InColumnId)
		{
			break;
		}
	}

	// Valid column ID?
	if (AvailableColumns.IsValidIndex(ColumnIndex) && InRowDataPtr->CellData.IsValidIndex(ColumnIndex))
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "DataTableEditor.CellText")
				.ColorAndOpacity(this, &FDataTableEditor::GetRowTextColor, InRowDataPtr->RowId)
				.Text(this, &FDataTableEditor::GetCellText, InRowDataPtr, ColumnIndex)
				.HighlightText(this, &FDataTableEditor::GetFilterText)
				.ToolTipText(this, &FDataTableEditor::GetCellToolTipText, InRowDataPtr, ColumnIndex)
			];
	}

	return SNullWidget::NullWidget;
}

void FDataTableEditor::OnRowNamesListViewScrolled(double InScrollOffset)
{
	// Synchronize the list views
	CellsListView->SetScrollOffset(InScrollOffset);
}

void FDataTableEditor::OnCellsListViewScrolled(double InScrollOffset)
{
	// Synchronize the list views
	RowNamesListView->SetScrollOffset(InScrollOffset);
}

void FDataTableEditor::OnRowSelectionChanged(FDataTableEditorRowListViewDataPtr InNewSelection, ESelectInfo::Type InSelectInfo)
{
	const bool bSelectionChanged = !InNewSelection.IsValid() || InNewSelection->RowId != HighlightedRowName;
	const FName NewRowName = (InNewSelection.IsValid()) ? InNewSelection->RowId : NAME_None;

	SetHighlightedRow(NewRowName);

	if (bSelectionChanged)
	{
		CallbackOnRowHighlighted.ExecuteIfBound(HighlightedRowName);
	}
}

FText FDataTableEditor::GetFilterText() const
{
	return ActiveFilterText;
}

void FDataTableEditor::OnFilterTextChanged(const FText& InFilterText)
{
	ActiveFilterText = InFilterText;
	UpdateVisibleRows();
}

void FDataTableEditor::RefreshCachedDataTable(const FName InCachedSelection, const bool bUpdateEvenIfValid)
{
	const UDataTable* Table = GetDataTable();
	TArray<FDataTableEditorColumnHeaderDataPtr> PreviousColumns = AvailableColumns;

	FDataTableEditorUtils::CacheDataTableForEditing(Table, AvailableColumns, AvailableRows);

	// Update the desired width of the row names column
	// This prevents it growing or shrinking as you scroll the list view
	{
		TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FTextBlockStyle& CellTextStyle = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("DataTableEditor.CellText");
		static const float CellPadding = 10.0f;
		
		RowNameColumnWidth = 10.0f;
		for (const FDataTableEditorRowListViewDataPtr& RowData : AvailableRows)
		{
			const float RowNameWidth = FontMeasure->Measure(RowData->DisplayName, CellTextStyle.Font).X + CellPadding;
			RowNameColumnWidth = FMath::Max(RowNameColumnWidth, RowNameWidth);
		}
	}

	// Setup the default auto-sized columns
	ColumnWidths.SetNum(AvailableColumns.Num());
	for (int32 ColumnIndex = 0; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
	{
		const FDataTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];
		FColumnWidth& ColumnWidth = ColumnWidths[ColumnIndex];
		ColumnWidth.CurrentWidth = FMath::Clamp(ColumnData->DesiredColumnWidth, 10.0f, 400.0f); // Clamp auto-sized columns to a reasonable limit
	}

	// Load the persistent column widths from the layout data
	{
		const TSharedPtr<FJsonObject>* LayoutColumnWidths = nullptr;
		if (LayoutData.IsValid() && LayoutData->TryGetObjectField(TEXT("ColumnWidths"), LayoutColumnWidths))
		{
			for(int32 ColumnIndex = 0; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
			{
				const FDataTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];

				double LayoutColumnWidth = 0.0f;
				if ((*LayoutColumnWidths)->TryGetNumberField(ColumnData->ColumnId.ToString(), LayoutColumnWidth))
				{
					FColumnWidth& ColumnWidth = ColumnWidths[ColumnIndex];
					ColumnWidth.bIsAutoSized = false;
					ColumnWidth.CurrentWidth = static_cast<float>(LayoutColumnWidth);
				}
			}
		}
	}

	if (PreviousColumns != AvailableColumns)
	{
		ColumnNamesHeaderRow->ClearColumns();
		for (int32 ColumnIndex = 0; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
		{
			const FDataTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];

			ColumnNamesHeaderRow->AddColumn(
				SHeaderRow::Column(ColumnData->ColumnId)
				.DefaultLabel(ColumnData->DisplayName)
				.ManualWidth(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(this, &FDataTableEditor::GetColumnWidth, ColumnIndex)))
				.OnWidthChanged(this, &FDataTableEditor::OnColumnResized, ColumnIndex)
			);
		}
	}

	UpdateVisibleRows(InCachedSelection, bUpdateEvenIfValid);
}

void FDataTableEditor::UpdateVisibleRows(const FName InCachedSelection, const bool bUpdateEvenIfValid)
{
	if (ActiveFilterText.IsEmptyOrWhitespace())
	{
		VisibleRows = AvailableRows;
	}
	else
	{
		VisibleRows.Empty(AvailableRows.Num());

		const FString& ActiveFilterString = ActiveFilterText.ToString();
		for (const FDataTableEditorRowListViewDataPtr& RowData : AvailableRows)
		{
			bool bPassesFilter = false;

			if (RowData->DisplayName.ToString().Contains(ActiveFilterString))
			{
				bPassesFilter = true;
			}
			else
			{
				for (const FText& CellText : RowData->CellData)
				{
					if (CellText.ToString().Contains(ActiveFilterString))
					{
						bPassesFilter = true;
						break;
					}
				}
			}

			if (bPassesFilter)
			{
				VisibleRows.Add(RowData);
			}
		}
	}
	
	// Abort restoring the cached selection if data was changed while the user is selecting a different row
	if (RowNamesListView->GetSelectedItems() == CellsListView->GetSelectedItems())
	{
		RowNamesListView->RequestListRefresh();
		CellsListView->RequestListRefresh();

		RestoreCachedSelection(InCachedSelection, bUpdateEvenIfValid);
	}
}

void FDataTableEditor::RestoreCachedSelection(const FName InCachedSelection, const bool bUpdateEvenIfValid)
{
	// Validate the requested selection to see if it matches a known row
	bool bSelectedRowIsValid = false;
	if (!InCachedSelection.IsNone())
	{
		bSelectedRowIsValid = VisibleRows.ContainsByPredicate([&InCachedSelection](const FDataTableEditorRowListViewDataPtr& RowData) -> bool
		{
			return RowData->RowId == InCachedSelection;
		});
	}

	// Apply the new selection (if required)
	if (!bSelectedRowIsValid)
	{
		SetHighlightedRow((VisibleRows.Num() > 0) ? VisibleRows[0]->RowId : NAME_None);
		CallbackOnRowHighlighted.ExecuteIfBound(HighlightedRowName);
	}
	else if (bUpdateEvenIfValid)
	{
		SetHighlightedRow(InCachedSelection);
		CallbackOnRowHighlighted.ExecuteIfBound(HighlightedRowName);
	}
}

TSharedRef<SVerticalBox> FDataTableEditor::CreateContentBox()
{
	TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(8.0f, 8.0f));

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(8.0f, 8.0f));

	TSharedRef<SHeaderRow> RowNamesHeaderRow = SNew(SHeaderRow);
	RowNamesHeaderRow->AddColumn(
		SHeaderRow::Column(RowNameColumnId)
		.DefaultLabel(FText::GetEmpty())
		);

	ColumnNamesHeaderRow = SNew(SHeaderRow);

	RowNamesListView = SNew(SListView<FDataTableEditorRowListViewDataPtr>)
		.ListItemsSource(&VisibleRows)
		.HeaderRow(RowNamesHeaderRow)
		.OnGenerateRow(this, &FDataTableEditor::MakeRowNameWidget)
		.OnListViewScrolled(this, &FDataTableEditor::OnRowNamesListViewScrolled)
		.OnSelectionChanged(this, &FDataTableEditor::OnRowSelectionChanged)
		.ScrollbarVisibility(EVisibility::Collapsed)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		.SelectionMode(ESelectionMode::Single)
		.AllowOverscroll(EAllowOverscroll::No);

	CellsListView = SNew(SListView<FDataTableEditorRowListViewDataPtr>)
		.ListItemsSource(&VisibleRows)
		.HeaderRow(ColumnNamesHeaderRow)
		.OnGenerateRow(this, &FDataTableEditor::MakeRowWidget)
		.OnListViewScrolled(this, &FDataTableEditor::OnCellsListViewScrolled)
		.OnSelectionChanged(this, &FDataTableEditor::OnRowSelectionChanged)
		.ExternalScrollbar(VerticalScrollBar)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		.SelectionMode(ESelectionMode::Single)
		.AllowOverscroll(EAllowOverscroll::No);

	RefreshCachedDataTable();

	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSearchBox)
			.InitialText(this, &FDataTableEditor::GetFilterText)
			.OnTextChanged(this, &FDataTableEditor::OnFilterTextChanged)
		]
		+SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(this, &FDataTableEditor::GetRowNameColumnWidth)
				[
					RowNamesListView.ToSharedRef()
				]
			]
			+SHorizontalBox::Slot()
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				.ExternalScrollbar(HorizontalScrollBar)
				+SScrollBox::Slot()
				[
					CellsListView.ToSharedRef()
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				VerticalScrollBar
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(this, &FDataTableEditor::GetRowNameColumnWidth)
				[
					SNullWidget::NullWidget
				]
			]
			+SHorizontalBox::Slot()
			[
				HorizontalScrollBar
			]
		];
}

TSharedRef<SWidget> FDataTableEditor::CreateRowEditorBox()
{
	UDataTable* Table = Cast<UDataTable>(GetEditingObject());

	// Support undo/redo
	if (Table)
	{
		Table->SetFlags(RF_Transactional);
	}

	auto RowEditor = SNew(SRowEditor, Table);
	RowEditor->RowSelectedCallback.BindSP(this, &FDataTableEditor::SetHighlightedRow);
	CallbackOnRowHighlighted.BindSP(RowEditor, &SRowEditor::SelectRow);
	CallbackOnDataTableUndoRedo.BindSP(RowEditor, &SRowEditor::HandleUndoRedo);
	return RowEditor;
}

TSharedRef<SDockTab> FDataTableEditor::SpawnTab_RowEditor(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == RowEditorTabId);

	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("DataTableEditor.Tabs.Properties"))
		.Label(LOCTEXT("RowEditorTitle", "Row Editor"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SBorder)
			.Padding(2)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				CreateRowEditorBox()
			]
		];
}


TSharedRef<SDockTab> FDataTableEditor::SpawnTab_DataTable( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == DataTableTabId );

	UDataTable* Table = Cast<UDataTable>(GetEditingObject());

	// Support undo/redo
	if (Table)
	{
		Table->SetFlags(RF_Transactional);
	}

	LoadLayoutData();

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("DataTableEditor.Tabs.Properties") )
		.Label( LOCTEXT("DataTableTitle", "Data Table") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
			[
				CreateContentBox()
			]
		];
}

void FDataTableEditor::SetHighlightedRow(FName Name)
{
	if (Name == HighlightedRowName)
	{
		return;
	}

	if (Name.IsNone())
	{
		HighlightedRowName = NAME_None;

		// Synchronize the list views
		RowNamesListView->ClearSelection();
		CellsListView->ClearSelection();
	}
	else
	{
		HighlightedRowName = Name;

		FDataTableEditorRowListViewDataPtr* NewSelectionPtr = VisibleRows.FindByPredicate([&Name](const FDataTableEditorRowListViewDataPtr& RowData) -> bool
		{
			return RowData->RowId == Name;
		});

		// Synchronize the list views
		if (NewSelectionPtr)
		{
			RowNamesListView->SetSelection(*NewSelectionPtr);
			CellsListView->SetSelection(*NewSelectionPtr);

			CellsListView->RequestScrollIntoView(*NewSelectionPtr);
		}
		else
		{
			RowNamesListView->ClearSelection();
			CellsListView->ClearSelection();
		}
	}
}

#undef LOCTEXT_NAMESPACE
