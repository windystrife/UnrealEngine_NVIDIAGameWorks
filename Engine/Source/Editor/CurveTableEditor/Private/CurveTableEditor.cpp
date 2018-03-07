// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CurveTableEditor.h"
#include "Widgets/Text/STextBlock.h"
#include "Modules/ModuleManager.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Framework/Layout/Overscroll.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"
#include "CurveTableEditorModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "SCurveEditor.h"
#include "CurveTableEditorCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
 
#define LOCTEXT_NAMESPACE "CurveTableEditor"

const FName FCurveTableEditor::CurveTableTabId("CurveTableEditor_CurveTable");
const FName FCurveTableEditor::RowNameColumnId("RowName");

class SCurveTableEditor : public SCurveEditor
{
	SLATE_BEGIN_ARGS(SCurveTableEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SCurveEditor::Construct(SCurveEditor::FArguments()
			.DesiredSize(FVector2D(128.0f, 64.0f)));
	}

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.IsShiftDown())
		{
			SCurveEditor::OnMouseWheel(MyGeometry, MouseEvent);
			return FReply::Handled();
		}
		
		return FReply::Unhandled();
	}
};

class SCurveTableListViewRow : public SMultiColumnTableRow<FCurveTableEditorRowListViewDataPtr>
{
public:
	SLATE_BEGIN_ARGS(SCurveTableListViewRow) {}
		/** The widget that owns the tree.  We'll only keep a weak reference to it. */
		SLATE_ARGUMENT(TSharedPtr<FCurveTableEditor>, CurveTableEditor)
		/** The list item for this row */
		SLATE_ARGUMENT(FCurveTableEditorRowListViewDataPtr, Item)
	SLATE_END_ARGS()

	/** Construct function for this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		CurveTableEditor = InArgs._CurveTableEditor;
		Item = InArgs._Item;
		SMultiColumnTableRow<FCurveTableEditorRowListViewDataPtr>::Construct(
			FSuperRowType::FArguments()
				.Style(FEditorStyle::Get(), "DataTableEditor.CellListViewRow"), 
			InOwnerTableView
			);
	}

	virtual void ConstructChildren(ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent) override
	{
		TSharedPtr<FCurveTableEditor> CurveTableEditorPtr = CurveTableEditor.Pin();
		if (CurveTableEditorPtr.IsValid())
		{
			ChildSlot
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SBox)
					.Visibility(this, &SCurveTableListViewRow::GetTableViewVisibility)
					[
						InContent
					]
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SHorizontalBox)
					.Visibility(this, &SCurveTableListViewRow::GetCurveViewVisibility)
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						CurveTableEditorPtr->MakeCurveWidget(Item, IndexInList)
					]
				]
			];
		}
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		TSharedPtr<FCurveTableEditor> CurveTableEditorPtr = CurveTableEditor.Pin();
		return (CurveTableEditorPtr.IsValid())
			? CurveTableEditorPtr->MakeCellWidget(Item, IndexInList, ColumnName)
			: SNullWidget::NullWidget;
	}

	EVisibility GetTableViewVisibility() const
	{
		return CurveTableEditor.Pin()->GetViewMode() == ECurveTableViewMode::Grid ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetCurveViewVisibility() const
	{
		return CurveTableEditor.Pin()->GetViewMode() == ECurveTableViewMode::CurveTable ? EVisibility::Visible : EVisibility::Collapsed;
	}

private:
	/** Weak reference to the curve table editor that owns our list */
	TWeakPtr<FCurveTableEditor> CurveTableEditor;
	/** The item associated with this row of data */
	FCurveTableEditorRowListViewDataPtr Item;
};


void FCurveTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_CurveTableEditor", "Curve Table Editor"));

	InTabManager->RegisterTabSpawner( CurveTableTabId, FOnSpawnTab::CreateSP(this, &FCurveTableEditor::SpawnTab_CurveTable) )
		.SetDisplayName( LOCTEXT("CurveTableTab", "Curve Table") )
		.SetGroup( WorkspaceMenuCategory.ToSharedRef() );
}


void FCurveTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner( CurveTableTabId );
}


FCurveTableEditor::~FCurveTableEditor()
{
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
}


void FCurveTableEditor::InitCurveTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UCurveTable* Table )
{
	const TSharedRef< FTabManager::FLayout > StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_CurveTableEditor_Layout_v1.1")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->Split
		(
			FTabManager::NewStack()
			->AddTab( CurveTableTabId, ETabState::OpenedTab )
			->SetHideTabWell(true)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FCurveTableEditorModule::CurveTableEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Table );
	
	BindCommands();
	ExtendMenu();
	RegenerateMenusAndToolbars();

	FReimportManager::Instance()->OnPostReimport().AddSP(this, &FCurveTableEditor::OnPostReimport);

	// @todo toolkit world centric editing
	/*// Setup our tool's layout
	if( IsWorldCentricAssetEditor() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for table properties
		SpawnToolkitTab( CurveTableTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/

	// NOTE: Could fill in asset editor commands here!
}

void FCurveTableEditor::BindCommands()
{
	FCurveTableEditorCommands::Register();

	ToolkitCommands->MapAction(FCurveTableEditorCommands::Get().CurveViewToggle,
		FExecuteAction::CreateSP(this, &FCurveTableEditor::ToggleViewMode),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FCurveTableEditor::IsCurveViewChecked));
}

void FCurveTableEditor::ExtendMenu()
{
	MenuExtender = MakeShareable(new FExtender);

	struct Local
	{
		static void ExtendMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection("CurveTableEditor", LOCTEXT("CurveTableEditor", "Curve Table"));
			{
				MenuBuilder.AddMenuEntry(FCurveTableEditorCommands::Get().CurveViewToggle);
			}
			MenuBuilder.EndSection();
		}
	};

	MenuExtender->AddMenuExtension(
		"WindowLayout",
		EExtensionHook::After,
		GetToolkitCommands(),
		FMenuExtensionDelegate::CreateStatic(&Local::ExtendMenu)
	);

	AddMenuExtender(MenuExtender);

	FCurveTableEditorModule& CurveTableEditorModule = FModuleManager::LoadModuleChecked<FCurveTableEditorModule>("CurveTableEditor");
	AddMenuExtender(CurveTableEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

FName FCurveTableEditor::GetToolkitFName() const
{
	return FName("CurveTableEditor");
}

FText FCurveTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "CurveTable Editor" );
}


FString FCurveTableEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "CurveTable ").ToString();
}


FLinearColor FCurveTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}


const UCurveTable* FCurveTableEditor::GetCurveTable() const
{
	return Cast<const UCurveTable>(GetEditingObject());
}

TSharedRef<SDockTab> FCurveTableEditor::SpawnTab_CurveTable( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == CurveTableTabId );

	TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(8.0f, 8.0f));

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(8.0f, 8.0f));

	TSharedRef<SHeaderRow> RowNamesHeaderRow = SNew(SHeaderRow)
		.Visibility(this, &FCurveTableEditor::GetGridViewControlsVisibility);

	RowNamesHeaderRow->AddColumn(
		SHeaderRow::Column(RowNameColumnId)
		.DefaultLabel(FText::GetEmpty())
		);

	ColumnNamesHeaderRow = SNew(SHeaderRow)
		.Visibility(this, &FCurveTableEditor::GetGridViewControlsVisibility);

	RowNamesListView = SNew(SListView<FCurveTableEditorRowListViewDataPtr>)
		.ListItemsSource(&AvailableRows)
		.HeaderRow(RowNamesHeaderRow)
		.OnGenerateRow(this, &FCurveTableEditor::MakeRowNameWidget)
		.OnListViewScrolled(this, &FCurveTableEditor::OnRowNamesListViewScrolled)
		.ScrollbarVisibility(EVisibility::Collapsed)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		.SelectionMode(ESelectionMode::None)
		.AllowOverscroll(EAllowOverscroll::No);

	CellsListView = SNew(SListView<FCurveTableEditorRowListViewDataPtr>)
		.ListItemsSource(&AvailableRows)
		.HeaderRow(ColumnNamesHeaderRow)
		.OnGenerateRow(this, &FCurveTableEditor::MakeRowWidget)
		.OnListViewScrolled(this, &FCurveTableEditor::OnCellsListViewScrolled)
		.ExternalScrollbar(VerticalScrollBar)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		.SelectionMode(ESelectionMode::None)
		.AllowOverscroll(EAllowOverscroll::No);

	RefreshCachedCurveTable();

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("CurveTableEditor.Tabs.Properties") )
		.Label( LOCTEXT("CurveTableTitle", "Curve Table") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(this, &FCurveTableEditor::GetRowNameColumnWidth)
						[
							RowNamesListView.ToSharedRef()
						]
					]
					+SHorizontalBox::Slot()
					[
						SNew(SScrollBox)
						.Orientation(Orient_Horizontal)
						.ExternalScrollbar(HorizontalScrollBar)
						.Visibility(this, &FCurveTableEditor::GetGridViewControlsVisibility)
						+SScrollBox::Slot()
						[
							CellsListView.ToSharedRef()
						]
					]
					+SHorizontalBox::Slot()
					[
						SNew(SBox)
						.Visibility(this, &FCurveTableEditor::GetCurveViewControlsVisibility)
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
						.WidthOverride(this, &FCurveTableEditor::GetRowNameColumnWidth)
						.Visibility(this, &FCurveTableEditor::GetGridViewControlsVisibility)
						[
							SNullWidget::NullWidget
						]
					]
					+SHorizontalBox::Slot()
					[
						HorizontalScrollBar
					]
				]
			]
		];
}


void FCurveTableEditor::RefreshCachedCurveTable()
{
	CacheDataTableForEditing();

	ColumnNamesHeaderRow->ClearColumns();
	for (int32 ColumnIndex = 0; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
	{
		const FCurveTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];

		ColumnNamesHeaderRow->AddColumn(
			SHeaderRow::Column(ColumnData->ColumnId)
			.DefaultLabel(ColumnData->DisplayName)
			.FixedWidth(ColumnData->DesiredColumnWidth)
			);
	}

	RowNamesListView->RequestListRefresh();
	CellsListView->RequestListRefresh();
}


void FCurveTableEditor::CacheDataTableForEditing()
{
	RowNameColumnWidth = 10.0f;

	const UCurveTable* Table = GetCurveTable();
	if (!Table || Table->RowMap.Num() == 0)
	{
		AvailableColumns.Empty();
		AvailableRows.Empty();
		return;
	}

	TArray<FName> Names;
	TArray<FRichCurve*> Curves;
		
	// get the row names and curves they represent
	Table->RowMap.GenerateKeyArray(Names);
	Table->RowMap.GenerateValueArray(Curves);

	TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FTextBlockStyle& CellTextStyle = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("DataTableEditor.CellText");
	static const float CellPadding = 10.0f;

	// Find unique column titles
	TArray<float> UniqueColumns;
	for (int32 CurvesIdx = 0; CurvesIdx < Curves.Num(); ++CurvesIdx)
	{
		for (auto CurveIt(Curves[CurvesIdx]->GetKeyIterator()); CurveIt; ++CurveIt)
		{
			UniqueColumns.AddUnique(CurveIt->Time);
		}
	}

	UniqueColumns.Sort();

	AvailableColumns.Empty();
	for (const float& ColumnTime : UniqueColumns)
	{
		const FText ColumnText = FText::AsNumber(ColumnTime);

		FCurveTableEditorColumnHeaderDataPtr CachedColumnData = MakeShareable(new FCurveTableEditorColumnHeaderData());
		CachedColumnData->ColumnId = *ColumnText.ToString();
		CachedColumnData->DisplayName = ColumnText;
		CachedColumnData->DesiredColumnWidth = FontMeasure->Measure(CachedColumnData->DisplayName, CellTextStyle.Font).X + CellPadding;

		AvailableColumns.Add(CachedColumnData);
	}

	// Each curve is a row entry
	AvailableRows.Reset(Curves.Num());
	for (int32 CurvesIdx = 0; CurvesIdx < Curves.Num(); ++CurvesIdx)
	{
		const FName& CurveName = Names[CurvesIdx];

		FCurveTableEditorRowListViewDataPtr CachedRowData = MakeShareable(new FCurveTableEditorRowListViewData());
		CachedRowData->RowId = CurveName;
		CachedRowData->DisplayName = FText::FromName(CurveName);
		CachedRowData->RowHandle = FCurveTableEditorHandle(Table, CurveName);

		check(CachedRowData->RowHandle.IsValid());

		const float RowNameWidth = FontMeasure->Measure(CachedRowData->DisplayName, CellTextStyle.Font).X + CellPadding;
		RowNameColumnWidth = FMath::Max(RowNameColumnWidth, RowNameWidth);

		CachedRowData->CellData.AddDefaulted(AvailableColumns.Num());
		{
			for (auto It(Curves[CurvesIdx]->GetKeyIterator()); It; ++It)
		{
			int32 ColumnIndex = 0;
				if (UniqueColumns.Find(It->Time, ColumnIndex))
			{
					FCurveTableEditorColumnHeaderDataPtr CachedColumnData = AvailableColumns[ColumnIndex];

				const FText CellText = FText::AsNumber(It->Value);
					CachedRowData->CellData[ColumnIndex] = CellText;

				const float CellWidth = FontMeasure->Measure(CellText, CellTextStyle.Font).X + CellPadding;
				CachedColumnData->DesiredColumnWidth = FMath::Max(CachedColumnData->DesiredColumnWidth, CellWidth);
			}
		}
		}

		AvailableRows.Add(CachedRowData);
	}
}


TSharedRef<ITableRow> FCurveTableEditor::MakeRowNameWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	TWeakPtr<FCurveTableEditor> WeakEditorPtr = SharedThis(this);

	return
		SNew(STableRow<FCurveTableEditorRowListViewDataPtr>, OwnerTable)
		.Style(FEditorStyle::Get(), "DataTableEditor.NameListViewRow")
		[
			SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			.HeightOverride_Lambda([WeakEditorPtr]() { TSharedPtr<FCurveTableEditor> This = WeakEditorPtr.Pin(); return (This.IsValid() && This->GetViewMode() == ECurveTableViewMode::CurveTable) ? FOptionalSize(68.0f) : FOptionalSize(); })
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InRowDataPtr->DisplayName)
			]
		];
}


TSharedRef<ITableRow> FCurveTableEditor::MakeRowWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(SCurveTableListViewRow, OwnerTable)
		.CurveTableEditor(SharedThis(this))
		.Item(InRowDataPtr);
}


TSharedRef<SWidget> FCurveTableEditor::MakeCellWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const int32 InRowIndex, const FName& InColumnId)
{
	int32 ColumnIndex = 0;
	for (; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
	{
		const FCurveTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];
		if (ColumnData->ColumnId == InColumnId)
		{
			break;
		}
	}

	// Valid column ID?
	if (AvailableColumns.IsValidIndex(ColumnIndex))
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "DataTableEditor.CellText")
				.Text(InRowDataPtr->CellData[ColumnIndex])
			];
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> FCurveTableEditor::MakeCurveWidget(FCurveTableEditorRowListViewDataPtr InRowDataPtr, const int32 InRowIndex)
{
	TSharedRef<SCurveTableEditor> CurveEditor = SNew(SCurveTableEditor);
	CurveEditor->SetCurveOwner(&InRowDataPtr->RowHandle, false);

	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding(FMargin(4, 2, 4, 2))
		.FillWidth(1.0f)
		[
			CurveEditor
		];
}


void FCurveTableEditor::OnRowNamesListViewScrolled(double InScrollOffset)
{
	// Synchronize the list views
	CellsListView->SetScrollOffset(InScrollOffset);
}


void FCurveTableEditor::OnCellsListViewScrolled(double InScrollOffset)
{
	// Synchronize the list views
	RowNamesListView->SetScrollOffset(InScrollOffset);
}


FOptionalSize FCurveTableEditor::GetRowNameColumnWidth() const
{
	return FOptionalSize(RowNameColumnWidth);
}

void FCurveTableEditor::OnPostReimport(UObject* InObject, bool)
{
	const UCurveTable* Table = GetCurveTable();
	if (Table && Table == InObject)
	{
		RefreshCachedCurveTable();
	}
}

EVisibility FCurveTableEditor::GetGridViewControlsVisibility() const
{
	return ViewMode == ECurveTableViewMode::CurveTable ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FCurveTableEditor::GetCurveViewControlsVisibility() const
{
	return ViewMode == ECurveTableViewMode::Grid ? EVisibility::Collapsed : EVisibility::Visible;
}

void FCurveTableEditor::ToggleViewMode()
{
	ViewMode = (ViewMode == ECurveTableViewMode::CurveTable) ? ECurveTableViewMode::Grid : ECurveTableViewMode::CurveTable;
}

bool FCurveTableEditor::IsCurveViewChecked() const
{
	return (ViewMode == ECurveTableViewMode::CurveTable);
}

#undef LOCTEXT_NAMESPACE
