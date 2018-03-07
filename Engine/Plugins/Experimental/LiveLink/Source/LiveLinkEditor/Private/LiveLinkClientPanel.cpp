// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkClientPanel.h"
#include "SBoxPanel.h"
#include "SSplitter.h"
#include "SOverlay.h"
#include "SharedPointer.h"
#include "SListView.h"
#include "SButton.h"
#include "SlateApplication.h"
#include "MultiBoxBuilder.h"
#include "SlateDelegates.h"
#include "LiveLinkClientCommands.h"
#include "ILiveLinkSource.h"
#include "LiveLinkClient.h"
#include "UObjectHash.h"
#include "EditorStyleSet.h"
#include "ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IStructureDetailsView.h"

#include "LiveLinkSourceFactory.h"

#define LOCTEXT_NAMESPACE "LiveLinkClientPanel"

static const FName TypeColumnName(TEXT("Type"));
static const FName MachineColumnName(TEXT("Machine"));
static const FName StatusColumnName(TEXT("Status"));

struct FLiveLinkSourceUIEntry
{
public:
	FLiveLinkSourceUIEntry(FGuid InEntryGuid, FLiveLinkClient* InClient)
		: EntryGuid(InEntryGuid)
		, Client(InClient)
	{}

	FText GetSourceType() { return Client->GetSourceTypeForEntry(EntryGuid); }
	FText GetMachineName() { return Client->GetMachineNameForEntry(EntryGuid); }
	FText GetEntryStatus() { return Client->GetEntryStatusForEntry(EntryGuid); }
	FLiveLinkConnectionSettings* GetConnectionSettings() { return Client->GetConnectionSettingsForEntry(EntryGuid); }
	void RemoveFromClient() { Client->RemoveSource(EntryGuid); }

private:
	FGuid EntryGuid;
	FLiveLinkClient* Client;
};

class SLiveLinkClientPanelSourcesRow : public SMultiColumnTableRow<FLiveLinkSourceUIEntryPtr>
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkClientPanelSourcesRow) {}

		/** The list item for this row */
		SLATE_ARGUMENT(FLiveLinkSourceUIEntryPtr, Entry)

	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		EntryPtr = Args._Entry;

		SourceType = EntryPtr->GetSourceType();
		MachineName = EntryPtr->GetMachineName();

		SMultiColumnTableRow<FLiveLinkSourceUIEntryPtr>::Construct(
			FSuperRowType::FArguments()
			.Padding(1.0f),
			OwnerTableView
		);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == TypeColumnName)
		{
			return	SNew(STextBlock)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SLiveLinkClientPanelSourcesRow::GetSourceTypeName)));
		}
		else if (ColumnName == MachineColumnName)
		{
			return	SNew(STextBlock)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SLiveLinkClientPanelSourcesRow::GetSourceMachineName)));
		}
		else if (ColumnName == StatusColumnName)
		{
			return	SNew(STextBlock)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SLiveLinkClientPanelSourcesRow::GetSourceStatus)));
		}

		return SNullWidget::NullWidget;
	}

private:

	FText GetSourceTypeName() const
	{
		return SourceType;
	}

	FText GetSourceMachineName() const
	{
		return MachineName;
	}

	FText GetSourceStatus() const
	{
		return EntryPtr->GetEntryStatus();
	}

	FLiveLinkSourceUIEntryPtr EntryPtr;

	FText SourceType;

	FText MachineName;
};

SLiveLinkClientPanel::~SLiveLinkClientPanel()
{
	if (Client)
	{
		Client->UnregisterSourcesChangedHandle(OnSourcesChangedHandle);
		OnSourcesChangedHandle.Reset();
	}
}

void SLiveLinkClientPanel::Construct(const FArguments& Args, FLiveLinkClient* InClient)
{
	check(InClient);
	Client = InClient;

	OnSourcesChangedHandle = Client->RegisterSourcesChangedHandle(FLiveLinkSourcesChanged::FDelegate::CreateSP(this, &SLiveLinkClientPanel::OnSourcesChangedHandler));

	RefreshSourceData(false);

	CommandList = MakeShareable(new FUICommandList);

	BindCommands();

	FToolBarBuilder ToolBarBuilder(CommandList, FMultiBoxCustomization::None);

	ToolBarBuilder.BeginSection(TEXT("Add"));
	{
		ToolBarBuilder.AddComboButton(	FUIAction(),
										FOnGetContent::CreateSP(this, &SLiveLinkClientPanel::GenerateSourceMenu),
										LOCTEXT("AddSource", "Add"),
										LOCTEXT("AddSource_ToolTip", "Add a new live link source"),
										FSlateIcon(FName("LiveLinkStyle"), "LiveLinkClient.Common.AddSource")
										);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection(TEXT("Remove"));
	{
		ToolBarBuilder.AddToolBarButton(FLiveLinkClientCommands::Get().RemoveSource);
		ToolBarBuilder.AddToolBarButton(FLiveLinkClientCommands::Get().RemoveAllSources);
	}
	ToolBarBuilder.EndSection();

	// Connection Settings
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	//DetailsViewArgs.
	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;
	StructureViewArgs.bShowObjects = true;

	StructureDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr);

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+ SSplitter::Slot()
		.Value(0.33f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				ToolBarBuilder.MakeWidget()
			]
			+ SVerticalBox::Slot()
			.FillHeight(0.5f)
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(4.0f, 4.0f))
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SAssignNew(ListView, SListView<FLiveLinkSourceUIEntryPtr>)
							.ListItemsSource(&SourceData)
							.SelectionMode(ESelectionMode::SingleToggle)
							.OnGenerateRow(this, &SLiveLinkClientPanel::MakeSourceListViewWidget)
							.OnSelectionChanged(this, &SLiveLinkClientPanel::OnSourceListSelectionChanged)
							.HeaderRow
							(
								SNew(SHeaderRow)
								+ SHeaderRow::Column(TypeColumnName)
								.FillWidth(43.0f)
								.DefaultLabel(LOCTEXT("TypeColumnHeaderName", "Source Type"))
								+ SHeaderRow::Column(MachineColumnName)
								.FillWidth(43.0f)
								.DefaultLabel(LOCTEXT("MachineColumnHeaderName", "Source Machine"))
								+ SHeaderRow::Column(StatusColumnName)
								.FillWidth(14.0f)
								.DefaultLabel(LOCTEXT("StatusColumnHeaderName", "Status"))
							)
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(0.5f)
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				StructureDetailsView->GetWidget().ToSharedRef()
			]
		]
	];

	// Register refresh timer
	/*if (!ActiveTimerHandle.IsValid())
	{
		ActiveTimerHandle = RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SSequenceRecorder::HandleRefreshItems));
	}*/
}

void SLiveLinkClientPanel::BindCommands()
{
	CommandList->MapAction(FLiveLinkClientCommands::Get().RemoveSource,
		FExecuteAction::CreateSP(this, &SLiveLinkClientPanel::HandleRemoveSource),
		FCanExecuteAction::CreateSP(this, &SLiveLinkClientPanel::CanRemoveSource)
	);

	CommandList->MapAction(FLiveLinkClientCommands::Get().RemoveAllSources,
		FExecuteAction::CreateSP(this, &SLiveLinkClientPanel::HandleRemoveAllSources),
		FCanExecuteAction::CreateSP(this, &SLiveLinkClientPanel::CanRemoveSource)
	);
}

void SLiveLinkClientPanel::RefreshSourceData(bool bRefreshUI)
{
	SourceData.Reset();

	for (FGuid Source : Client->GetSourceEntries())
	{
		SourceData.Add(MakeShareable(new FLiveLinkSourceUIEntry(Source, Client)));
	}

	if (bRefreshUI)
	{
		ListView->RequestListRefresh();
	}
}

const TArray<FLiveLinkSourceUIEntryPtr>& SLiveLinkClientPanel::GetCurrentSources() const
{
	return SourceData;
}

TSharedRef<ITableRow> SLiveLinkClientPanel::MakeSourceListViewWidget(FLiveLinkSourceUIEntryPtr Entry, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(SLiveLinkClientPanelSourcesRow, OwnerTable)
		   .Entry(Entry);
}

void SLiveLinkClientPanel::OnSourceListSelectionChanged(FLiveLinkSourceUIEntryPtr Entry, ESelectInfo::Type SelectionType) const
{
	if(Entry.IsValid())
	{
		FStructOnScope* Struct = new FStructOnScope(FLiveLinkConnectionSettings::StaticStruct(), (uint8*)Entry->GetConnectionSettings());
		StructureDetailsView->SetStructureData(MakeShareable(Struct));
	}
	else
	{
		StructureDetailsView->SetStructureData(nullptr);
	}
}

TSharedRef< SWidget > SLiveLinkClientPanel::GenerateSourceMenu()
{
	TArray<UClass*> Results;
	GetDerivedClasses(ULiveLinkSourceFactory::StaticClass(), Results, true);

	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder(CloseAfterSelection, NULL);

	MenuBuilder.BeginSection("SourceSection", LOCTEXT("Sources", "Live Link Sources"));

	for (UClass* SourceFactory : Results)
	{
		ULiveLinkSourceFactory* FactoryCDO = SourceFactory->GetDefaultObject<ULiveLinkSourceFactory>();

		//Prep UI
		TSharedPtr<SWidget> SourcePanel = FactoryCDO->CreateSourceCreationPanel();
		SourcePanels.Add(FactoryCDO) = SourcePanel;

		MenuBuilder.AddSubMenu(
			FactoryCDO->GetSourceDisplayName(),
			FactoryCDO->GetSourceTooltip(),
			FNewMenuDelegate::CreateRaw(this, &SLiveLinkClientPanel::RetrieveFactorySourcePanel, FactoryCDO),
			false
		);
	}

	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

void SLiveLinkClientPanel::RetrieveFactorySourcePanel(FMenuBuilder& MenuBuilder, ULiveLinkSourceFactory* FactoryCDO)
{
	TSharedPtr<SWidget> SourcePanel = SourcePanels[FactoryCDO];
	MenuBuilder.AddWidget(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SourcePanel.ToSharedRef()
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(EHorizontalAlignment::HAlign_Right)
			[
				SNew(SButton)
				.Text(LOCTEXT("OkButton", "Ok"))
				.OnClicked(this, &SLiveLinkClientPanel::OnCloseSourceSelectionPanel, FactoryCDO, true)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(EHorizontalAlignment::HAlign_Right)
			[
				SNew(SButton)
				.Text(LOCTEXT("CancelButton", "Cancel"))
				.OnClicked(this, &SLiveLinkClientPanel::OnCloseSourceSelectionPanel, FactoryCDO, false)
			]
		]
		, FText(), true);
}

FReply SLiveLinkClientPanel::OnCloseSourceSelectionPanel(ULiveLinkSourceFactory* FactoryCDO, bool bMakeSource)
{
	TSharedPtr<ILiveLinkSource> Source = FactoryCDO->OnSourceCreationPanelClosed(bMakeSource);
	// If we want a source we should get one ... if we dont we shouldn't. Make sure source factory does the right thing in both cases
	if (bMakeSource)
	{
		check(Source.IsValid());
		Client->AddSource(Source);

		RefreshSourceData(true);
	}
	else
	{
		check(!Source.IsValid());
	}
	FSlateApplication::Get().DismissAllMenus();
	return FReply::Handled();
}

void SLiveLinkClientPanel::HandleRemoveSource()
{
	TArray<FLiveLinkSourceUIEntryPtr> Selected;
	ListView->GetSelectedItems(Selected);
	if (Selected.Num() > 0)
	{
		Selected[0]->RemoveFromClient();
	}
}

bool SLiveLinkClientPanel::CanRemoveSource()
{
	return ListView->GetNumItemsSelected() > 0;
}

void SLiveLinkClientPanel::HandleRemoveAllSources()
{
	Client->RemoveAllSources();
}

void SLiveLinkClientPanel::OnSourcesChangedHandler()
{
	RefreshSourceData(true);
}

#undef LOCTEXT_NAMESPACE