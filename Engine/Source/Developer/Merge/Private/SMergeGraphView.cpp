// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SMergeGraphView.h"
#include "Engine/Blueprint.h"
#include "Widgets/Layout/SSplitter.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "GraphDiffControl.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SMergeGraphView"

const FName MergeMyBluerpintTabId = FName(TEXT("MergeMyBluerpintTab"));
const FName MergeGraphTabId = FName(TEXT("MergeGraphTab"));

struct FBlueprintRevPair
{
	FBlueprintRevPair(const UBlueprint* InBlueprint, const FRevisionInfo& InRevData)
	: Blueprint(InBlueprint)
	, RevData(InRevData)
	{
	}

	const UBlueprint* Blueprint;
	const FRevisionInfo& RevData;
};;

static UEdGraph* FindGraphByName(UBlueprint const& FromBlueprint, const FName& GraphName)
{
	TArray<UEdGraph*> Graphs;
	FromBlueprint.GetAllGraphs(Graphs);

	UEdGraph* Ret = nullptr;
	if (UEdGraph** Result = Graphs.FindByPredicate(FMatchFName(GraphName)))
	{
		Ret = *Result;
	}
	return Ret;
}

struct FMergeGraphRowEntry
{
	FText Label;

	FName GraphName;

	UEdGraphNode* LocalNode;
	UEdGraphNode* BaseNode;
	UEdGraphNode* RemoteNode;

	UEdGraphPin* LocalPin;
	UEdGraphPin* BasePin;
	UEdGraphPin* RemotePin;

	FLinearColor DisplayColor;

	bool bHasConflicts;
};

struct FMergeGraphEntry
{
	FName GraphName;

	TArray<FMergeGraphRowEntry> Changes;
	bool bAnyConflics;
	bool bRemoteDifferences;
	bool bLocalDifferences;

	bool bExistsInRemote;
	bool bExistsInBase;
	bool bExistsInLocal;
};

static TArray< FMergeGraphEntry > GenerateDiffListItems(const FBlueprintRevPair& RemoteBlueprint, const FBlueprintRevPair& BaseBlueprint, const FBlueprintRevPair& LocalBlueprint )
{
	// Index all the graphs by name, we use the name of the graph as the 
	// basis of comparison between the various versions of the blueprint.
	TMap< FName, UEdGraph* > RemoteGraphMap, BaseGraphMap, LocalGraphMap;
	// We also want the set of all graph names in these blueprints, so that we 
	// can iterate over every graph.
	TSet< FName > AllGraphNames;
	{
		TArray<UEdGraph*> GraphsRemote, GraphsBase, GraphsLocal;
		RemoteBlueprint.Blueprint->GetAllGraphs(GraphsRemote);
		BaseBlueprint.Blueprint->GetAllGraphs(GraphsBase);
		LocalBlueprint.Blueprint->GetAllGraphs(GraphsLocal);

		const auto ToMap = [&AllGraphNames](const TArray<UEdGraph*>& InList, TMap<FName, UEdGraph*>& OutMap)
		{
			for (auto Graph : InList)
			{
				OutMap.Add(Graph->GetFName(), Graph);
				AllGraphNames.Add(Graph->GetFName());
			}
		};
		ToMap(GraphsRemote, RemoteGraphMap);
		ToMap(GraphsBase, BaseGraphMap);
		ToMap(GraphsLocal, LocalGraphMap);
	}

	TArray< FMergeGraphEntry > Ret;
	{
		const auto GenerateDifferences = [](UEdGraph* GraphNew, UEdGraph** GraphOld)
		{
			TArray<FDiffSingleResult> Results;
			FGraphDiffControl::DiffGraphs(GraphOld ? *GraphOld : nullptr, GraphNew, Results);
			struct SortDiff
			{
				bool operator () (const FDiffResultItem& A, const FDiffResultItem& B) const
				{
					return A.Result.Diff < B.Result.Diff;
				}
			};

			Sort(Results.GetData(), Results.Num(), SortDiff());
			return Results;
		};

		for (const auto& GraphName : AllGraphNames)
		{
			TArray< FDiffSingleResult > RemoteDifferences;
			TArray< FDiffSingleResult > LocalDifferences;
			bool bExistsInRemote, bExistsInBase, bExistsInLocal;

			FMergeGraphEntry GraphEntry;
			GraphEntry.GraphName = GraphName;
			{
				UEdGraph** RemoteGraph = RemoteGraphMap.Find(GraphName);
				UEdGraph** BaseGraph = BaseGraphMap.Find(GraphName);
				UEdGraph** LocalGraph = LocalGraphMap.Find(GraphName);

				GraphEntry.bAnyConflics = false;
				GraphEntry.bExistsInRemote = RemoteGraph != nullptr;
				GraphEntry.bExistsInBase = BaseGraph != nullptr;
				GraphEntry.bExistsInLocal = LocalGraph != nullptr;

				if (RemoteGraph)
				{
					RemoteDifferences = GenerateDifferences(*RemoteGraph, BaseGraph);
				}

				if (LocalGraph)
				{
					LocalDifferences = GenerateDifferences(*LocalGraph, BaseGraph);
				}

				// 'join' the local differences and remote differences by noting changes
				// that affected the same common base:
				{
					TMap< const FDiffSingleResult*, const FDiffSingleResult*> ConflictMap;

					for (const auto& RemoteDifference : RemoteDifferences)
					{
						const FDiffSingleResult* ConflictingDifference = nullptr;

						for (const auto& LocalDifference : LocalDifferences)
						{
							if (RemoteDifference.Node1 == LocalDifference.Node1)
							{
								if (RemoteDifference.Diff == EDiffType::NODE_REMOVED ||
									LocalDifference.Diff == EDiffType::NODE_REMOVED ||
									RemoteDifference.Pin1 == LocalDifference.Pin1)
								{
									ConflictingDifference = &LocalDifference;
									break;
								}
							}
							else if (RemoteDifference.Pin1 != nullptr && (RemoteDifference.Pin1 == LocalDifference.Pin1))
							{
								// it's possible the users made the same change to the same pin, but given the wide
								// variety of changes that can be made to a pin it is difficult to identify the change 
								// as identical, for now I'm just flagging all changes to the same pin as a conflict:
								ConflictingDifference = &LocalDifference;
								break;
							}
						}

						if (ConflictingDifference != nullptr)
						{
							// For now, we don't want to create a hard conflict for changes that don't effect runtime behavior:
							if (RemoteDifference.Diff == EDiffType::NODE_MOVED ||
								RemoteDifference.Diff == EDiffType::NODE_COMMENT)
							{
								continue;
							}

							ConflictMap.Add(&RemoteDifference, ConflictingDifference);
							ConflictMap.Add(ConflictingDifference, &RemoteDifference);
						}
					}


					for( const auto& Difference : RemoteDifferences )
					{
						FText Label;

						const FDiffSingleResult** ConflictingDifference = ConflictMap.Find(&Difference);

						if( ConflictingDifference )
						{
							Label = FText::Format( NSLOCTEXT("SMergeGraphView", "ConflictIdentifier", "CONFLICT: {0} conflicts with {1}" ), (*ConflictingDifference)->DisplayString, Difference.DisplayString );
						}
						else
						{
							Label = Difference.DisplayString;
						}

						FMergeGraphRowEntry NewEntry = {
							Label
							, Difference.OwningGraph
							, ConflictingDifference ? (*ConflictingDifference)->Node2 : nullptr /*UEdGraphNode* LocalNode*/
							, Difference.Node1 /*UEdGraphNode* BaseNode*/
							, Difference.Node2 /*UEdGraphNode* RemoteNode*/
							, ConflictingDifference ? (*ConflictingDifference)->Pin2 : nullptr /*UEdGraphPin* LocalPin*/
							, Difference.Pin1 /*UEdGraphPin* BasePin*/
							, Difference.Pin2 /*UEdGraphPin* RemotePin*/
							, Difference.DisplayColor
							, ConflictingDifference ? true : false
						};

						GraphEntry.bAnyConflics |= NewEntry.bHasConflicts;
						GraphEntry.Changes.Push( NewEntry );
					}

					for (const auto& Difference : LocalDifferences)
					{
						FText Label;

						const FDiffSingleResult** ConflictingDifference = ConflictMap.Find( &Difference );

						if (!ConflictingDifference)
						{
							FMergeGraphRowEntry NewEntry = {
								Difference.DisplayString
								, Difference.OwningGraph
								, Difference.Node2 /*UEdGraphNode* LocalNode*/
								, Difference.Node1 /*UEdGraphNode* BaseNode*/
								, nullptr
								, Difference.Pin2 /*UEdGraphPin* LocalPin*/
								, Difference.Pin1 /*UEdGraphPin* BasePin*/
								, nullptr
								, Difference.DisplayColor
								, false
							};

							GraphEntry.Changes.Push(NewEntry);
						}
					}

					GraphEntry.bLocalDifferences = LocalDifferences.Num() != 0;
					GraphEntry.bRemoteDifferences = RemoteDifferences.Num() != 0;
				}

				Ret.Add(GraphEntry);

				bExistsInRemote = RemoteGraph != nullptr;
				bExistsInBase = BaseGraph != nullptr;
				bExistsInLocal = LocalGraph != nullptr;
			}
		}
	}

	return Ret;
}

static void LockViews(TArray<FDiffPanel>& Views, bool bAreLocked)
{
	for (auto& Panel : Views)
	{
		auto GraphEditor = Panel.GraphEditor.Pin();
		if (GraphEditor.IsValid())
		{
			// lock this panel to ever other panel:
			for (auto& OtherPanel : Views)
			{
				auto OtherGraphEditor = OtherPanel.GraphEditor.Pin();
				if (OtherGraphEditor.IsValid() &&
					OtherGraphEditor != GraphEditor)
				{
					if (bAreLocked)
					{
						GraphEditor->LockToGraphEditor(OtherGraphEditor);
					}
					else
					{
						GraphEditor->UnlockFromGraphEditor(OtherGraphEditor);
					}
				}
			}
		}
	}
}

FDiffPanel& GetDiffPanelForNode(const UEdGraphNode& Node, TArray< FDiffPanel >& Panels)
{
	for (auto& Panel : Panels)
	{
		auto GraphEditor = Panel.GraphEditor.Pin();
		if (GraphEditor.IsValid())
		{
			if (Node.GetGraph() == GraphEditor->GetCurrentGraph())
			{
				return Panel;
			}
		}
	}
	checkf(false, TEXT("Looking for node %s but it cannot be found in provided panels"), *Node.GetName());
	static FDiffPanel Default;
	return Default;
}

void SMergeGraphView::Construct(const FArguments InArgs
	, const FBlueprintMergeData& InData
	, FOnMergeNodeSelected SelectionCallback
	, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries
	, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences
	, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutConflicts
	)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab)
		.TabRole(ETabRole::MajorTab);

	TabManager = FGlobalTabmanager::Get()->NewTabManager(MajorTab);

	TabManager->RegisterTabSpawner(MergeGraphTabId,
		FOnSpawnTab::CreateRaw(this, &SMergeGraphView::CreateGraphDiffViews))
		.SetDisplayName(LOCTEXT("MergeGraphsTabTitle", "Graphs"))
		.SetTooltipText(LOCTEXT("MergeGraphsTooltipText", "Differences in the various graphs present in the blueprint"));

	TabManager->RegisterTabSpawner(MergeMyBluerpintTabId,
		FOnSpawnTab::CreateRaw(this, &SMergeGraphView::CreateMyBlueprintsViews))
		.SetDisplayName(LOCTEXT("MergeMyBlueprintTabTitle", "My Blueprint"))
		.SetTooltipText(LOCTEXT("MergeMyBlueprintTooltipText", "Differences in the 'My Blueprints' attributes of the blueprint"));

	Data = InData;
	bViewsAreLocked = true;
	
	TArray<FBlueprintRevPair> BlueprintsForDisplay;
	// EMergeParticipant::Remote
	BlueprintsForDisplay.Add(FBlueprintRevPair(InData.BlueprintRemote, InData.RevisionRemote));
	// EMergeParticipant::Base
	BlueprintsForDisplay.Add(FBlueprintRevPair(InData.BlueprintBase, InData.RevisionBase));
	// EMergeParticipant::Local
	BlueprintsForDisplay.Add(FBlueprintRevPair(InData.BlueprintLocal, FRevisionInfo()));
	
	const TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout("BlueprintMerge_Layout_v1")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->Split
		(
			FTabManager::NewStack()
			->AddTab(MergeMyBluerpintTabId, ETabState::OpenedTab)
			->AddTab(MergeGraphTabId, ETabState::OpenedTab)
		)
	);

	for (int32 i = 0; i < EMergeParticipant::Max_None; ++i)
	{
		DiffPanels.Add(FDiffPanel());
		FDiffPanel& NewPanel = DiffPanels[i];
		NewPanel.Blueprint = BlueprintsForDisplay[i].Blueprint;
		NewPanel.RevisionInfo = BlueprintsForDisplay[i].RevData;
		NewPanel.bShowAssetName = false;
	}

	auto GraphPanelContainer = TabManager->RestoreFrom(DefaultLayout, TSharedPtr<SWindow>()).ToSharedRef();

	for (auto& Panel : DiffPanels )
	{
		Panel.InitializeDiffPanel();
	}

	auto DetailsPanelContainer = SNew(SSplitter);
	for( auto& Panel : DiffPanels )
	{
		DetailsPanelContainer->AddSlot()
		[
			Panel.DetailsView.ToSharedRef()
		];
	}

	Differences = TSharedPtr< TArray< FMergeGraphEntry > >( new TArray< FMergeGraphEntry >( 
																GenerateDiffListItems(BlueprintsForDisplay[EMergeParticipant::Remote]
																					, BlueprintsForDisplay[EMergeParticipant::Base]
																					, BlueprintsForDisplay[EMergeParticipant::Local]) ) );

	for( const auto& Difference : *Differences)
	{
		TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > Children;
		for( const auto& Change : Difference.Changes )
		{
			const auto ChangeWidget = [](FText Label, FLinearColor Color) -> TSharedRef<SWidget>
			{
				return SNew(STextBlock)
						.Text(Label)
						.ColorAndOpacity(Color);
			};

			const auto SelectGraphNode = [](FOnMergeNodeSelected InSelectionCallback, FMergeGraphRowEntry DiffEntry, SMergeGraphView* Parent)
			{
				InSelectionCallback.ExecuteIfBound();
				Parent->HighlightEntry( DiffEntry );
			};

			TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
				FOnDiffEntryFocused::CreateStatic(SelectGraphNode, SelectionCallback, Change, this)
				, FGenerateDiffEntryWidget::CreateStatic(ChangeWidget, Change.Label, Change.DisplayColor )
				, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >()
			));
			Children.Push(Entry);
			OutRealDifferences.Push(Entry);
			if( Change.LocalNode && Change.RemoteNode )
			{
				OutConflicts.Push(Entry);
			}
		}

		const auto Widget = []( const FMergeGraphEntry* InDifference ) -> TSharedRef<SWidget>
		{
			// blue indicates added, red indicates changed, yellow indicates removed, white indicates no change:
			const auto ComputeColor = [](const bool bAnyConflicts, const bool bAnyDifferences) -> FLinearColor
			{
				if( bAnyConflicts )
				{
					return DiffViewUtils::Conflicting();
				}
				else if( bAnyDifferences )
				{
					return DiffViewUtils::Differs();
				}
				return DiffViewUtils::Identical();
			};

			const auto Box = [](bool bIsPresent, FLinearColor Color) -> SHorizontalBox::FSlot&
			{
				return SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.MaxWidth(8.0f)
					[
						SNew(SImage)
						.ColorAndOpacity(Color)
						.Image(bIsPresent ? FEditorStyle::GetBrush("BlueprintDif.HasGraph") : FEditorStyle::GetBrush("BlueprintDif.MissingGraph"))
					];
			};

			FLinearColor RemoteColor = ComputeColor(InDifference->bAnyConflics, InDifference->bRemoteDifferences);
			FLinearColor BaseColor = ComputeColor(InDifference->bAnyConflics, false);
			FLinearColor LocalColor = ComputeColor(InDifference->bAnyConflics, InDifference->bLocalDifferences);
			FLinearColor TextColor = ComputeColor(InDifference->bAnyConflics, InDifference->bLocalDifferences || InDifference->bRemoteDifferences);

			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.ColorAndOpacity( TextColor )
					.Text(FText::FromString(InDifference->GraphName.GetPlainNameString()))
				]
				+ DiffViewUtils::Box(InDifference->bExistsInRemote, RemoteColor)
				+ DiffViewUtils::Box(InDifference->bExistsInBase, BaseColor)
				+ DiffViewUtils::Box(InDifference->bExistsInLocal, LocalColor);
		};

		const auto FocusGraph = [](FOnMergeNodeSelected InSelectionCallback, SMergeGraphView* Parent, FName GraphName)
		{
			InSelectionCallback.ExecuteIfBound();
			Parent->FocusGraph( GraphName );
		};

		if( Children.Num() == 0 )
		{
			Children.Push(FBlueprintDifferenceTreeEntry::NoDifferencesEntry());
		}

		OutTreeEntries.Push(
			TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
				FOnDiffEntryFocused::CreateStatic(FocusGraph, SelectionCallback, this, Difference.GraphName)
				, FGenerateDiffEntryWidget::CreateStatic(Widget, &Difference)
				, Children
			))
		);
	}

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		+ SSplitter::Slot()
		.Value(0.9f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(.8f)
			[
				GraphPanelContainer
			]
			+ SSplitter::Slot()
			.Value(.2f)
			[
				DetailsPanelContainer
			]
		]
	];
}

void SMergeGraphView::FocusGraph(FName GraphName)
{
	UEdGraph* GraphRemote = FindGraphByName(*GetRemotePanel().Blueprint, GraphName);
	UEdGraph* GraphBase = FindGraphByName(*GetBasePanel().Blueprint, GraphName);
	UEdGraph* GraphLocal = FindGraphByName(*GetLocalPanel().Blueprint, GraphName);

	GetBasePanel().GeneratePanel(GraphBase, nullptr);
	GetRemotePanel().GeneratePanel(GraphRemote, GraphBase);
	GetLocalPanel().GeneratePanel(GraphLocal, GraphBase);

	LockViews(DiffPanels, bViewsAreLocked);
}

void SMergeGraphView::HighlightEntry(const struct FMergeGraphRowEntry& Conflict)
{
	FocusGraph(Conflict.GraphName);

	const auto FocusPinOrNode = [this]( UEdGraphPin* Pin, UEdGraphNode* Node )
	{
		if (Pin)
		{
			// then look for the diff panel and focus on the change:
			GetDiffPanelForNode(*Pin->GetOwningNode(), DiffPanels).FocusDiff(*Pin);
		}
		else if (Node)
		{
			GetDiffPanelForNode(*Node, DiffPanels).FocusDiff(*Node);
		}
	};

	// highlight the change made to the remote graph:
	FocusPinOrNode(Conflict.RemotePin, Conflict.RemoteNode);
	FocusPinOrNode(Conflict.LocalPin, Conflict.LocalNode);
	FocusPinOrNode(Conflict.BasePin, Conflict.BaseNode);
}

TSharedRef<SDockTab> SMergeGraphView::CreateGraphDiffViews(const FSpawnTabArgs& Args)
{
	auto PanelContainer = SNew(SSplitter);
	for (auto& Panel : DiffPanels)
	{
		PanelContainer->AddSlot()
		[
			SAssignNew(Panel.GraphEditorBorder, SBox)
			.VAlign(VAlign_Fill)
			[
				SBlueprintDiff::DefaultEmptyPanel()
			]
		];
	}

	return SNew(SDockTab)
	[
		PanelContainer
	];
}

TSharedRef<SDockTab> SMergeGraphView::CreateMyBlueprintsViews(const FSpawnTabArgs& Args)
{
	auto PanelContainer = SNew(SSplitter);
	for (auto& Panel : DiffPanels)
	{
		PanelContainer->AddSlot()
		[
			Panel.GenerateMyBlueprintPanel()
		];
	}

	return SNew(SDockTab)
	[
		PanelContainer
	];
}

FReply SMergeGraphView::OnToggleLockView()
{
	bViewsAreLocked = !bViewsAreLocked;

	LockViews(DiffPanels, bViewsAreLocked);
	return FReply::Handled();
}

const FSlateBrush* SMergeGraphView::GetLockViewImage() const
{
	return bViewsAreLocked ? FEditorStyle::GetBrush("GenericLock") : FEditorStyle::GetBrush("GenericUnlock");
}

#undef LOCTEXT_NAMESPACE
