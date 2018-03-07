// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SSceneOutliner.h"
#include "Widgets/SOverlay.h"
#include "Engine/GameViewportClient.h"
#include "Modules/ModuleManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor/GroupActor.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "EdMode.h"
#include "EditorModeManager.h"
#include "UnrealEdGlobals.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerSettings.h"
#include "ISceneOutlinerColumn.h"
#include "SceneOutlinerModule.h"



#include "Widgets/Input/SSearchBox.h"

#include "SceneOutlinerDragDrop.h"


#include "ActorEditorUtils.h"
#include "LevelUtils.h"

#include "EditorActorFolders.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogSceneOutliner, Log, All);

#define LOCTEXT_NAMESPACE "SSceneOutliner"

// The amount of time that must pass before the Scene Outliner will attempt a sort when in PIE/SIE.
#define SCENE_OUTLINER_RESORT_TIMER 1.0f

namespace SceneOutliner
{
	FText GetWorldDescription(UWorld* World)
	{
		FText Description;
		if(World)
		{
			FText PostFix;
			const FWorldContext* WorldContext = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if(Context.World() == World)
				{
					WorldContext = &Context;
					break;
				}
			}

			if (World->WorldType == EWorldType::PIE)
			{
				switch(World->GetNetMode())
				{
					case NM_Client:
						if (WorldContext)
						{
							PostFix = FText::Format(LOCTEXT("ClientPostfixFormat", "(Client {0})"), FText::AsNumber(WorldContext->PIEInstance - 1));
						}
						else
						{
							PostFix = LOCTEXT("ClientPostfix", "(Client)");
						}
						break;
					case NM_DedicatedServer:
					case NM_ListenServer:
						PostFix = LOCTEXT("ServerPostfix", "(Server)");
						break;
					case NM_Standalone:
						PostFix = LOCTEXT("PlayInEditorPostfix", "(Play In Editor)");
						break;
				}
			}
			else if(World->WorldType == EWorldType::Editor)
			{
				PostFix = LOCTEXT("EditorPostfix", "(Editor)");
			}

			Description = FText::Format(LOCTEXT("WorldFormat", "{0} {1}"), FText::FromString(World->GetFName().GetPlainNameString()), PostFix);	
		}

		return Description;
	}

	TSharedPtr< FOutlinerFilter > CreateSelectedActorFilter()
	{
		auto* Filter = new FOutlinerPredicateFilter(FActorFilterPredicate::CreateStatic([](const AActor* InActor){	return InActor->IsSelected(); }), EDefaultFilterBehaviour::Fail);

		// If anything fails this filter, make it non interactive. We don't want to allow selection of implicitly included parents which might nuke the actor selection.
		Filter->FailedItemState = EFailedFilterState::NonInteractive;
		return MakeShareable( Filter );
	}

	TSharedPtr< FOutlinerFilter > CreateHideTemporaryActorsFilter()
	{
		return MakeShareable( new FOutlinerPredicateFilter( FActorFilterPredicate::CreateStatic( []( const AActor* InActor ){			
			return InActor->GetWorld()->WorldType != EWorldType::PIE || GEditor->ObjectsThatExistInEditorWorld.Get(InActor);
		} ), EDefaultFilterBehaviour::Pass ) );
	}

	TSharedPtr< FOutlinerFilter > CreateIsInCurrentLevelFilter()
	{
		struct FOnlyCurrentLevelFilter : FOutlinerFilter
		{
			FOnlyCurrentLevelFilter() : FOutlinerFilter(EDefaultFilterBehaviour::Fail, EFailedFilterState::Interactive) {}

			virtual bool PassesFilter(const AActor* InActor) const override
			{
				return InActor->GetLevel() == InActor->GetWorld()->GetCurrentLevel();
			}
		};

		return MakeShareable( new FOnlyCurrentLevelFilter() );
	}

	struct FItemSelection : IMutableTreeItemVisitor
	{
		mutable TArray<FActorTreeItem*> Actors;
		mutable TArray<FWorldTreeItem*> Worlds;
		mutable TArray<FFolderTreeItem*> Folders;

		FItemSelection()
		{}

		FItemSelection(SOutlinerTreeView& Tree)
		{
			for (auto& Item : Tree.GetSelectedItems())
			{
				Item->Visit(*this);
			}
		}

		TArray<TWeakObjectPtr<AActor>> GetWeakActors() const
		{
			TArray<TWeakObjectPtr<AActor>> ActorArray;
			for (const auto* ActorItem : Actors)
			{
				if (ActorItem->Actor.IsValid())
				{
					ActorArray.Add(ActorItem->Actor);
				}
			}
			return ActorArray;
		}

		TArray<AActor*> GetActorPtrs() const
		{
			TArray<AActor*> ActorArray;
			for (const auto* ActorItem : Actors)
			{
				if (AActor* Actor = ActorItem->Actor.Get())
				{
					ActorArray.Add(Actor);
				}
			}
			return ActorArray;
		}

	private:
		virtual void Visit(FActorTreeItem& ActorItem) const override
		{
			Actors.Add(&ActorItem);
		}
		virtual void Visit(FWorldTreeItem& WorldItem) const override
		{
			Worlds.Add(&WorldItem);
		}
		virtual void Visit(FFolderTreeItem& FolderItem) const override
		{
			Folders.Add(&FolderItem);
		}
	};

	void SSceneOutliner::Construct( const FArguments& InArgs, const FInitializationOptions& InInitOptions )
	{
		// Copy over the shared data from the initialization options
		static_cast<FSharedDataBase&>(*SharedData) = static_cast<const FSharedDataBase&>(InInitOptions);

		OnItemPicked = InArgs._OnItemPickedDelegate;

		if (InInitOptions.OnSelectionChanged.IsBound())
		{
			SelectionChanged.Add(InInitOptions.OnSelectionChanged);
		}

		bFullRefresh = true;
		bNeedsRefresh = true;
		bIsReentrant = false;
		bSortDirty = true;
		bActorSelectionDirty = SharedData->Mode == ESceneOutlinerMode::ActorBrowsing;
		FilteredActorCount = 0;
		SortOutlinerTimer = 0.0f;
		bPendingFocusNextFrame = InInitOptions.bFocusSearchBoxWhenOpened;

		SortByColumn = FBuiltInColumnTypes::Label();
		SortMode = EColumnSortMode::Ascending;

		// @todo outliner: Should probably save this in layout!
		// @todo outliner: Should save spacing for list view in layout

		NoBorder = FEditorStyle::GetBrush( "LevelViewport.NoViewportBorder" );
		PlayInEditorBorder = FEditorStyle::GetBrush( "LevelViewport.StartingPlayInEditorBorder" );
		SimulateBorder = FEditorStyle::GetBrush( "LevelViewport.StartingSimulateBorder" );

		//Setup the SearchBox filter
		{
			auto Delegate = TreeItemTextFilter::FItemToStringArray::CreateSP( this, &SSceneOutliner::PopulateSearchStrings );
			SearchBoxFilter = MakeShareable( new TreeItemTextFilter( Delegate ) );
		}

		TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

		// We use the filter collection provided, otherwise we create our own
		Filters = InInitOptions.Filters.IsValid() ? InInitOptions.Filters : MakeShareable(new FOutlinerFilters);
	
		// Add additional filters
		if (SharedData->Mode == ESceneOutlinerMode::ActorBrowsing)
		{
			FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked< FSceneOutlinerModule >("SceneOutliner");

			for (auto& OutlinerFilterInfo : SceneOutlinerModule.OutlinerFilterInfoMap)
			{
				OutlinerFilterInfo.Value.InitFilter(Filters);
			}
		}

		SearchBoxFilter->OnChanged().AddSP( this, &SSceneOutliner::FullRefresh );
		Filters->OnChanged().AddSP( this, &SSceneOutliner::FullRefresh );

		//Apply custom filters based on global preferences
		if (SharedData->Mode == ESceneOutlinerMode::ActorBrowsing)
		{
			ApplyShowOnlySelectedFilter( IsShowingOnlySelected() );
			ApplyHideTemporaryActorsFilter( IsHidingTemporaryActors() );
			ApplyShowOnlyCurrentLevelFilter( IsShowingOnlyCurrentLevel() );
		}

		TSharedRef< SHeaderRow > HeaderRowWidget =
			SNew( SHeaderRow )
				// Only show the list header if the user configured the outliner for that
				.Visibility(InInitOptions.bShowHeaderRow ? EVisibility::Visible : EVisibility::Collapsed);

		SetupColumns(*HeaderRowWidget);

		ChildSlot
		[
			SNew( SBorder )
			.BorderImage( this, &SSceneOutliner::OnGetBorderBrush )
			.BorderBackgroundColor( this, &SSceneOutliner::OnGetBorderColorAndOpacity )
			.ShowEffectWhenDisabled( false )
			[
				VerticalBox
			]
		];

		auto Toolbar = SNew(SHorizontalBox);

		Toolbar->AddSlot()
		.VAlign(VAlign_Center)
		[
			SAssignNew( FilterTextBoxWidget, SSearchBox )
			.Visibility( InInitOptions.bShowSearchBox ? EVisibility::Visible : EVisibility::Collapsed )
			.HintText( LOCTEXT( "FilterSearch", "Search..." ) )
			.ToolTipText( LOCTEXT("FilterSearchHint", "Type here to search (pressing enter selects the results)") )
			.OnTextChanged( this, &SSceneOutliner::OnFilterTextChanged )
			.OnTextCommitted( this, &SSceneOutliner::OnFilterTextCommitted )
		];

		if (SharedData->Mode == ESceneOutlinerMode::ActorBrowsing && InInitOptions.bShowCreateNewFolder)
		{
			Toolbar->AddSlot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("CreateFolderToolTip", "Create a new folder containing the current actor selection"))
					.OnClicked(this, &SSceneOutliner::OnCreateFolderClicked)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("SceneOutliner.NewFolderIcon"))
					]
				];
		}

		VerticalBox->AddSlot()
		.AutoHeight()
		.Padding( 0.0f, 0.0f, 0.0f, 4.0f )
		[
			Toolbar
		];

		VerticalBox->AddSlot()
		.FillHeight( 1.0f )
		[
			SNew( SOverlay )
			+SOverlay::Slot()
			.HAlign( HAlign_Center )
			[
				SNew( STextBlock )
				.Visibility( this, &SSceneOutliner::GetEmptyLabelVisibility )
				.Text( LOCTEXT( "EmptyLabel", "Empty" ) )
				.ColorAndOpacity( FLinearColor( 0.4f, 1.0f, 0.4f ) )
			]

			+SOverlay::Slot()
			[
				SAssignNew( OutlinerTreeView, SOutlinerTreeView, StaticCastSharedRef<SSceneOutliner>(AsShared()) )

				// multi-select if we're in browsing mode, 
				// single-select if we're in picking mode,
				.SelectionMode( this, &SSceneOutliner::GetSelectionMode )

				// Point the tree to our array of root-level items.  Whenever this changes, we'll call RequestTreeRefresh()
				.TreeItemsSource( &RootTreeItems )

				// Find out when the user selects something in the tree
				.OnSelectionChanged( this, &SSceneOutliner::OnOutlinerTreeSelectionChanged )

				// Called when the user double-clicks with LMB on an item in the list
				.OnMouseButtonDoubleClick( this, &SSceneOutliner::OnOutlinerTreeDoubleClick )

				// Called when an item is scrolled into view
				.OnItemScrolledIntoView( this, &SSceneOutliner::OnOutlinerTreeItemScrolledIntoView )

				// Called when an item is expanded or collapsed
				.OnExpansionChanged(this, &SSceneOutliner::OnItemExpansionChanged)

				// Called to child items for any given parent item
				.OnGetChildren( this, &SSceneOutliner::OnGetChildrenForOutlinerTree )

				// Generates the actual widget for a tree item
				.OnGenerateRow( this, &SSceneOutliner::OnGenerateRowForOutlinerTree ) 

				// Use the level viewport context menu as the right click menu for tree items
				.OnContextMenuOpening(this, &SSceneOutliner::OnOpenContextMenu)

				// Header for the tree
				.HeaderRow( HeaderRowWidget )

				// Called when an item is expanded or collapsed with the shift-key pressed down
				.OnSetExpansionRecursive(this, &SSceneOutliner::SetItemExpansionRecursive)
			]
		];

		// Separator
		VerticalBox->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 1)
		[
			SNew(SSeparator)
		];

		if (SharedData->Mode == ESceneOutlinerMode::ActorBrowsing)
		{
			// Bottom panel
			VerticalBox->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Asset count
				+SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				.Padding(8, 0)
				[
					SNew( STextBlock )
					.Text( this, &SSceneOutliner::GetFilterStatusText )
					.ColorAndOpacity( this, &SSceneOutliner::GetFilterStatusTextColor )
				]

				// View mode combo button
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew( ViewOptionsComboButton, SComboButton )
					.ContentPadding(0)
					.ForegroundColor( this, &SSceneOutliner::GetViewButtonForegroundColor )
					.ButtonStyle( FEditorStyle::Get(), "ToggleButton" ) // Use the tool bar item style for this button
					.OnGetMenuContent( this, &SSceneOutliner::GetViewButtonContent, false )
					.ButtonContent()
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage).Image( FEditorStyle::GetBrush("GenericViewButton") )
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2, 0, 0, 0)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text( LOCTEXT("ViewButton", "View Options") )
						]
					]
				]
			];
		} //end if (SharedData->Mode == ESceneOutlinerMode::ActorBrowsing)
		else
		{
			// Bottom panel
			VerticalBox->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// World picker combo button
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				[
					SAssignNew( ViewOptionsComboButton, SComboButton )
					.ContentPadding(0)
					.ForegroundColor( this, &SSceneOutliner::GetViewButtonForegroundColor )
					.ButtonStyle( FEditorStyle::Get(), "ToggleButton" ) // Use the tool bar item style for this button
					.OnGetMenuContent( this, &SSceneOutliner::GetViewButtonContent, true )
					.ButtonContent()
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage).Image( FEditorStyle::GetBrush("SceneOutliner.World") )
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2, 0, 0, 0)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text( LOCTEXT("ChooseWorldMenu", "Choose World") )
						]
					]
				]
			];
		}


		// Don't allow tool-tips over the header
		HeaderRowWidget->EnableToolTipForceField( true );


		// Populate our data set
		Populate();

		// We only synchronize selection when in actor browsing mode
		if (SharedData->Mode == ESceneOutlinerMode::ActorBrowsing)
		{
			// Populate and register to find out when the level's selection changes
			OnLevelSelectionChanged( NULL );
			USelection::SelectionChangedEvent.AddRaw(this, &SSceneOutliner::OnLevelSelectionChanged);
			USelection::SelectObjectEvent.AddRaw(this, &SSceneOutliner::OnLevelSelectionChanged);
		}

		// Register to find out when actors are added or removed
		// @todo outliner: Might not catch some cases (see: CALLBACK_ActorPropertiesChange, CALLBACK_LayerChange, CALLBACK_LevelDirtied, CALLBACK_OnActorMoved, CALLBACK_UpdateLevelsForAllActors)
		FEditorDelegates::MapChange.AddSP( this, &SSceneOutliner::OnMapChange );
		FEditorDelegates::NewCurrentLevel.AddSP( this, &SSceneOutliner::OnNewCurrentLevel );
		GEngine->OnLevelActorListChanged().AddSP( this, &SSceneOutliner::FullRefresh );
		FWorldDelegates::LevelAddedToWorld.AddSP( this, &SSceneOutliner::OnLevelAdded );
		FWorldDelegates::LevelRemovedFromWorld.AddSP( this, &SSceneOutliner::OnLevelRemoved );

		GEngine->OnLevelActorAdded().AddSP( this, &SSceneOutliner::OnLevelActorsAdded );
		GEngine->OnLevelActorDetached().AddSP( this, &SSceneOutliner::OnLevelActorsDetached );
		GEngine->OnLevelActorFolderChanged().AddSP( this, &SSceneOutliner::OnLevelActorFolderChanged );

		GEngine->OnLevelActorDeleted().AddSP( this, &SSceneOutliner::OnLevelActorsRemoved );
		GEngine->OnLevelActorAttached().AddSP( this, &SSceneOutliner::OnLevelActorsAttached );

		GEngine->OnLevelActorRequestRename().AddSP( this, &SSceneOutliner::OnLevelActorsRequestRename );

		// Register to update when an undo/redo operation has been called to update our list of actors
		GEditor->RegisterForUndo( this );

		// Register to be notified when properties are edited
		FCoreDelegates::OnActorLabelChanged.AddRaw(this, &SSceneOutliner::OnActorLabelChanged);

		auto& Folders = FActorFolders::Get();
		Folders.OnFolderCreate.AddSP(this, &SSceneOutliner::OnBroadcastFolderCreate);
		Folders.OnFolderMove.AddSP(this, &SSceneOutliner::OnBroadcastFolderMove);
		Folders.OnFolderDelete.AddSP(this, &SSceneOutliner::OnBroadcastFolderDelete);
	}

	void SSceneOutliner::SetupColumns(SHeaderRow& HeaderRow)
	{
		FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");

		if (SharedData->ColumnMap.Num() == 0)
		{
			SharedData->UseDefaultColumns();
		}

		Columns.Empty(SharedData->ColumnMap.Num());
		HeaderRow.ClearColumns();

		// Get a list of sorted columns IDs to create
		TArray<FName> SortedIDs;
		SortedIDs.Reserve(SharedData->ColumnMap.Num());
		SharedData->ColumnMap.GenerateKeyArray(SortedIDs);

		SortedIDs.Sort([&](const FName& A, const FName& B){
			return SharedData->ColumnMap[A].PriorityIndex < SharedData->ColumnMap[B].PriorityIndex;
		});

		for (const FName& ID : SortedIDs)
		{
			if (SharedData->ColumnMap[ID].Visibility == EColumnVisibility::Invisible)
			{
				continue;
			}

			TSharedPtr<ISceneOutlinerColumn> Column;

			if (SharedData->ColumnMap[ID].Factory.IsBound())
			{
				Column = SharedData->ColumnMap[ID].Factory.Execute(*this);
			}
			else
			{
				Column = SceneOutlinerModule.FactoryColumn(ID, *this);
			}

			if (ensure(Column.IsValid()))
			{
				check(Column->GetColumnID() == ID);
				Columns.Add(Column->GetColumnID(), Column);

				auto ColumnArgs = Column->ConstructHeaderRowColumn();

				if (Column->SupportsSorting())
				{
					ColumnArgs
						.SortMode(this, &SSceneOutliner::GetColumnSortMode, Column->GetColumnID())
						.OnSort(this, &SSceneOutliner::OnColumnSortModeChanged);
				}

				HeaderRow.AddColumn(ColumnArgs);
			}
		}

		Columns.Shrink();
	}

	SSceneOutliner::~SSceneOutliner()
	{
		// We only synchronize selection when in actor browsing mode
		if( SharedData->Mode == ESceneOutlinerMode::ActorBrowsing )
		{
			USelection::SelectionChangedEvent.RemoveAll(this);
			USelection::SelectObjectEvent.RemoveAll(this);
		}
		FEditorDelegates::MapChange.RemoveAll( this );
		FEditorDelegates::NewCurrentLevel.RemoveAll( this );
		GEngine->OnLevelActorListChanged().RemoveAll( this );
		GEditor->UnregisterForUndo( this );

		SearchBoxFilter->OnChanged().RemoveAll( this );
		Filters->OnChanged().RemoveAll( this );
		
		FWorldDelegates::LevelAddedToWorld.RemoveAll( this );
		FWorldDelegates::LevelRemovedFromWorld.RemoveAll( this );

		FCoreDelegates::OnActorLabelChanged.RemoveAll(this);

		if (FActorFolders::IsAvailable())
		{
			auto& Folders = FActorFolders::Get();
			Folders.OnFolderCreate.RemoveAll(this);
			Folders.OnFolderMove.RemoveAll(this);
			Folders.OnFolderDelete.RemoveAll(this);
		}
	}

	void SSceneOutliner::OnItemAdded(const FTreeItemID& ItemID, uint8 ActionMask)
	{
		NewItemActions.Add(ItemID, ActionMask);
	}

	FSlateColor SSceneOutliner::GetViewButtonForegroundColor() const
	{
		static const FName InvertedForegroundName("InvertedForeground");
		static const FName DefaultForegroundName("DefaultForeground");

		return ViewOptionsComboButton->IsHovered() ? FEditorStyle::GetSlateColor(InvertedForegroundName) : FEditorStyle::GetSlateColor(DefaultForegroundName);
	}

	TSharedRef<SWidget> SSceneOutliner::GetViewButtonContent(bool bWorldPickerOnly)
	{
		FMenuBuilder MenuBuilder(!bWorldPickerOnly, NULL);

		if(bWorldPickerOnly)
		{
			BuildWorldPickerContent(MenuBuilder);
		}
		else
		{
			MenuBuilder.BeginSection("AssetThumbnails", LOCTEXT("ShowHeading", "Show"));
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("ToggleShowOnlySelected", "Only Selected"),
					LOCTEXT("ToggleShowOnlySelectedToolTip", "When enabled, only displays actors that are currently selected."),
					FSlateIcon(),
					FUIAction(
					FExecuteAction::CreateSP( this, &SSceneOutliner::ToggleShowOnlySelected ),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP( this, &SSceneOutliner::IsShowingOnlySelected )
					),
					NAME_None,
					EUserInterfaceActionType::ToggleButton
				);

				MenuBuilder.AddMenuEntry(
					LOCTEXT("ToggleHideTemporaryActors", "Hide Temporary Actors"),
					LOCTEXT("ToggleHideTemporaryActorsToolTip", "When enabled, hides temporary/run-time Actors."),
					FSlateIcon(),
					FUIAction(
					FExecuteAction::CreateSP( this, &SSceneOutliner::ToggleHideTemporaryActors ),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP( this, &SSceneOutliner::IsHidingTemporaryActors )
					),
					NAME_None,
					EUserInterfaceActionType::ToggleButton
				);

				MenuBuilder.AddMenuEntry(
					LOCTEXT("ToggleShowOnlyCurrentLevel", "Only in Current Level"),
					LOCTEXT("ToggleShowOnlyCurrentLevelToolTip", "When enabled, only shows Actors that are in the Current Level."),
					FSlateIcon(),
					FUIAction(
					FExecuteAction::CreateSP( this, &SSceneOutliner::ToggleShowOnlyCurrentLevel ),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP( this, &SSceneOutliner::IsShowingOnlyCurrentLevel )
					),
					NAME_None,
					EUserInterfaceActionType::ToggleButton
				);

				// Add additional filters
				FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked< FSceneOutlinerModule >("SceneOutliner");
			
				for (auto& OutlinerFilterInfo : SceneOutlinerModule.OutlinerFilterInfoMap)
				{
					OutlinerFilterInfo.Value.AddMenu(MenuBuilder);
				}
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("AssetThumbnails", LOCTEXT("ShowWorldHeading", "World"));
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("ChooseWorldSubMenu", "Choose World"),
					LOCTEXT("ChooseWorldSubMenuToolTip", "Choose the world to display in the outliner."),
					FNewMenuDelegate::CreateSP(this, &SSceneOutliner::BuildWorldPickerContent)
				);
			}
			MenuBuilder.EndSection();
		}

		return MenuBuilder.MakeWidget();
	}

	void SSceneOutliner::BuildWorldPickerContent(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("Worlds", LOCTEXT("WorldsHeading", "Worlds"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AutoWorld", "Auto"),
				LOCTEXT("AutoWorldToolTip", "Automatically pick the world to display based on context."),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateSP( this, &SSceneOutliner::OnSelectWorld, TWeakObjectPtr<UWorld>() ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SSceneOutliner::IsWorldChecked, TWeakObjectPtr<UWorld>() )
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);

			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (World && (World->WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Editor))
				{
					MenuBuilder.AddMenuEntry(
						GetWorldDescription(World),
						LOCTEXT("ChooseWorldToolTip", "Display actors for this world."),
						FSlateIcon(),
						FUIAction(
						FExecuteAction::CreateSP( this, &SSceneOutliner::OnSelectWorld, TWeakObjectPtr<UWorld>(World) ),
						FCanExecuteAction(),
						FIsActionChecked::CreateSP( this, &SSceneOutliner::IsWorldChecked, TWeakObjectPtr<UWorld>(World) )
						),
						NAME_None,
						EUserInterfaceActionType::RadioButton
					);
				}
			}
		}
		MenuBuilder.EndSection();
	}

	/** FILTERS */

	// Show Only Selected
	void SSceneOutliner::ToggleShowOnlySelected()
	{
		const bool bEnableFlag = !IsShowingOnlySelected();

		USceneOutlinerSettings* Settings = GetMutableDefault<USceneOutlinerSettings>();
		Settings->bShowOnlySelectedActors = bEnableFlag;
		Settings->PostEditChange();

		ApplyShowOnlySelectedFilter(bEnableFlag);
	}
	void SSceneOutliner::ApplyShowOnlySelectedFilter(bool bShowOnlySelected)
	{
		if ( !SelectedActorFilter.IsValid() )
		{
			SelectedActorFilter = CreateSelectedActorFilter();
		}

		if ( bShowOnlySelected )
		{			
			Filters->Add( SelectedActorFilter );
		}
		else
		{
			Filters->Remove( SelectedActorFilter );
		}
	}
	bool SSceneOutliner::IsShowingOnlySelected() const
	{
		return GetDefault<USceneOutlinerSettings>()->bShowOnlySelectedActors;
	}

	// Hide Temporary Actors
	void SSceneOutliner::ToggleHideTemporaryActors()
	{
		const bool bEnableFlag = !IsHidingTemporaryActors();

		USceneOutlinerSettings* Settings = GetMutableDefault<USceneOutlinerSettings>();
		Settings->bHideTemporaryActors = bEnableFlag;
		Settings->PostEditChange();

		ApplyHideTemporaryActorsFilter(bEnableFlag);
	}

	void SSceneOutliner::ApplyHideTemporaryActorsFilter(bool bHideTemporaryActors)
	{
		if ( !HideTemporaryActorsFilter.IsValid() )
		{
			HideTemporaryActorsFilter = CreateHideTemporaryActorsFilter();
		}

		if ( bHideTemporaryActors )
		{			
			Filters->Add( HideTemporaryActorsFilter );
		}
		else
		{
			Filters->Remove( HideTemporaryActorsFilter );
		}
	}
	bool SSceneOutliner::IsHidingTemporaryActors() const
	{
		return GetDefault<USceneOutlinerSettings>()->bHideTemporaryActors;
	}

	// Show Only Actors In Current Level
	void SSceneOutliner::ToggleShowOnlyCurrentLevel()
	{
		const bool bEnableFlag = !IsShowingOnlyCurrentLevel();

		USceneOutlinerSettings* Settings = GetMutableDefault<USceneOutlinerSettings>();
		Settings->bShowOnlyActorsInCurrentLevel = bEnableFlag;
		Settings->PostEditChange();

		ApplyShowOnlyCurrentLevelFilter(bEnableFlag);
	}
	void SSceneOutliner::ApplyShowOnlyCurrentLevelFilter(bool bShowOnlyActorsInCurrentLevel)
	{
		if ( !ShowOnlyActorsInCurrentLevelFilter.IsValid() )
		{
			ShowOnlyActorsInCurrentLevelFilter = CreateIsInCurrentLevelFilter();
		}

		if ( bShowOnlyActorsInCurrentLevel )
		{			
			Filters->Add( ShowOnlyActorsInCurrentLevelFilter );
		}
		else
		{
			Filters->Remove( ShowOnlyActorsInCurrentLevelFilter );
		}
	}
	bool SSceneOutliner::IsShowingOnlyCurrentLevel() const
	{
		return GetDefault<USceneOutlinerSettings>()->bShowOnlyActorsInCurrentLevel;
	}

	/** END FILTERS */


	const FSlateBrush* SSceneOutliner::OnGetBorderBrush() const
	{
		if (SharedData->bRepresentingPlayWorld)
		{
			return GEditor->bIsSimulatingInEditor ? SimulateBorder : PlayInEditorBorder;
		}
		else
		{
			return NoBorder;
		}
	}

	FSlateColor SSceneOutliner::OnGetBorderColorAndOpacity() const
	{
		return SharedData->bRepresentingPlayWorld	? FLinearColor(1.0f, 1.0f, 1.0f, 0.6f)
									   				: FLinearColor::Transparent;
	}

	ESelectionMode::Type SSceneOutliner::GetSelectionMode() const
	{
		return (SharedData->Mode == ESceneOutlinerMode::ActorBrowsing)? 
			ESelectionMode::Multi : ESelectionMode::Single;
	}


	void SSceneOutliner::Refresh()
	{
		bNeedsRefresh = true;
	}

	void SSceneOutliner::FullRefresh()
	{
		bFullRefresh = true;
		Refresh();
	}


	void SSceneOutliner::Populate()
	{
		// Block events while we clear out the list.  We don't want actors in the level to become deselected
		// while we doing this
		TGuardValue<bool> ReentrantGuard(bIsReentrant, true);

		SharedData->RepresentingWorld = nullptr;

		// check if the user-chosen world is valid and in the editor contexts
		if(SharedData->UserChosenWorld.IsValid())
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if(SharedData->UserChosenWorld.Get() == Context.World())
				{
					SharedData->RepresentingWorld = Context.World();
					break;
				}
			}
		}
		
		if(SharedData->RepresentingWorld == nullptr)
		{
			// try to pick the most suitable world context

			// ideally we want a PIE world that is standalone or the first client
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (World && Context.WorldType == EWorldType::PIE)
				{
					if(World->GetNetMode() == NM_Standalone)
					{
						SharedData->RepresentingWorld = World;
						break;
					}
					else if(World->GetNetMode() == NM_Client && Context.PIEInstance == 2)	// Slightly dangerous: assumes server is always PIEInstance = 1;
					{
						SharedData->RepresentingWorld = World;
						break;						
					}
				}
			}
		}

		if(SharedData->RepresentingWorld == nullptr)
		{
			// still not world so fallback to old logic where we just prefer PIE over Editor

			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE)
				{
					SharedData->RepresentingWorld = Context.World();
					break;
				}
				else if (Context.WorldType == EWorldType::Editor)
				{
					SharedData->RepresentingWorld = Context.World();
				}
			}
		}
		
		if (!CheckWorld())
		{
			return;
		}

		SharedData->bRepresentingPlayWorld = SharedData->RepresentingWorld->WorldType == EWorldType::PIE;

		// Get a collection of items and folders which were formerly collapsed
		const FParentsExpansionState ExpansionStateInfo = GetParentsExpansionState();

		bool bMadeAnySignificantChanges = false;
		if(bFullRefresh)
		{
			// Clear the selection here - RepopulateEntireTree will reconstruct it.
			OutlinerTreeView->ClearSelection();

			RepopulateEntireTree();

			bMadeAnySignificantChanges = true;
			bFullRefresh = false;
		}

		// Only deal with 500 at a time
		const int32 End = FMath::Min(PendingOperations.Num(), 500);
		for (int32 Index = 0; Index < End; ++Index)
		{
			auto& PendingOp = PendingOperations[Index];
			switch (PendingOp.Type)
			{
				case FPendingTreeOperation::Added:
					bMadeAnySignificantChanges = AddItemToTree(PendingOp.Item) || bMadeAnySignificantChanges;
					break;

				case FPendingTreeOperation::Moved:
					bMadeAnySignificantChanges = true;
					OnItemMoved(PendingOp.Item);
					break;

				case FPendingTreeOperation::Removed:
					bMadeAnySignificantChanges = true;
					RemoveItemFromTree(PendingOp.Item);
					break;

				default:
					check(false);
					break;
			}
		}

		PendingOperations.RemoveAt(0, End);
		SetParentsExpansionState(ExpansionStateInfo);

		if (bMadeAnySignificantChanges)
		{
			RequestSort();
		}

		if (PendingOperations.Num() == 0)
		{
			// We're fully refreshed now.
			NewItemActions.Empty();
			bNeedsRefresh = false;
		}
	}

	bool SSceneOutliner::ShouldShowFolders() const
	{
		return SharedData->Mode == ESceneOutlinerMode::ActorBrowsing || SharedData->bOnlyShowFolders;
	}

	void SSceneOutliner::EmptyTreeItems()
	{
		FilteredActorCount = 0;
		ApplicableActors.Empty();

		PendingOperations.Empty();
		TreeItemMap.Reset();
		PendingTreeItemMap.Empty();

		RootTreeItems.Empty();
	}

	void SSceneOutliner::RepopulateEntireTree()
	{
		EmptyTreeItems();

		ConstructItemFor<FWorldTreeItem>(SharedData->RepresentingWorld);

		if (!SharedData->bOnlyShowFolders)
		{
			// Iterate over every actor in memory. WARNING: This is potentially very expensive!
			for( FActorIterator ActorIt(SharedData->RepresentingWorld); ActorIt; ++ActorIt )
			{
				AActor* Actor = *ActorIt;
				if (Actor && IsActorDisplayable(Actor))
				{
					if (Filters->PassesAllFilters(FActorTreeItem(Actor)))
					{
						ApplicableActors.Emplace(Actor);
					}
					ConstructItemFor<FActorTreeItem>(Actor);
				}
			}
		}

		if (!IsShowingOnlySelected() && ShouldShowFolders())
		{
			// Add any folders which might match the current search terms
			for (const auto& Pair : FActorFolders::Get().GetFolderPropertiesForWorld(*SharedData->RepresentingWorld))
			{
				if (!TreeItemMap.Contains(Pair.Key))
				{
					ConstructItemFor<FFolderTreeItem>(Pair.Key);
				}
			}
		}
	}

	void SSceneOutliner::OnChildRemovedFromParent(ITreeItem& Parent)
	{
		if (Parent.Flags.bIsFilteredOut && !Parent.GetChildren().Num())
		{
			// The parent no longer has any children that match the current search terms. Remove it.
			RemoveItemFromTree(Parent.AsShared());
		}
	}

	void SSceneOutliner::OnItemMoved(const FTreeItemRef& Item)
	{
		// Just remove the item if it no longer matches the filters
		if (!Item->Flags.bIsFilteredOut && !SearchBoxFilter->PassesFilter(*Item))
		{
			// This will potentially remove any non-matching, empty parents as well
			RemoveItemFromTree(Item);
		}
		else
		{
			// The item still matches the filters (or has children that do)
			// When an item has been asked to move, it will still reside under its old parent
			FTreeItemPtr Parent = Item->GetParent();
			if (Parent.IsValid())
			{
				Parent->RemoveChild(Item);
				OnChildRemovedFromParent(*Parent);
			}
			else
			{
				RootTreeItems.Remove(Item);
			}

			Parent = EnsureParentForItem(Item);
			if (Parent.IsValid())
			{
				Parent->AddChild(Item);
				OutlinerTreeView->SetItemExpansion(Parent, true);
			}
			else
			{
				RootTreeItems.Add(Item);
			}
		}
	}

	void SSceneOutliner::RemoveItemFromTree(FTreeItemRef InItem)
	{
		if (TreeItemMap.Contains(InItem->GetID()))
		{
			auto Parent = InItem->GetParent();

			if (Parent.IsValid())
			{
				Parent->RemoveChild(InItem);
				OnChildRemovedFromParent(*Parent);
			}
			else
			{
				RootTreeItems.Remove(InItem);
			}

			InItem->Visit(FFunctionalVisitor().Actor([&](const FActorTreeItem& ActorItem){
				if (!ActorItem.Flags.bIsFilteredOut)
				{
					--FilteredActorCount;
				}
			}));

			TreeItemMap.Remove(InItem->GetID());
		}
	}

	FTreeItemPtr SSceneOutliner::EnsureParentForItem(FTreeItemRef Item)
	{
		if (SharedData->bShowParentTree)
		{
			FTreeItemPtr Parent = Item->FindParent(TreeItemMap);
			if (Parent.IsValid())
			{
				return Parent;
			}
			else
			{
				auto NewParent = Item->CreateParent();
				if (NewParent.IsValid())
				{
					NewParent->Flags.bIsFilteredOut = !Filters->TestAndSetInteractiveState(*NewParent) || !SearchBoxFilter->PassesFilter(*NewParent);

					AddUnfilteredItemToTree(NewParent.ToSharedRef());
					return NewParent;
				}
			}
		}

		return nullptr;
	}

	bool SSceneOutliner::AddItemToTree(FTreeItemRef Item)
	{
		const auto ItemID = Item->GetID();

		PendingTreeItemMap.Remove(ItemID);

		// If a tree item already exists that represents the same data, bail
		if (TreeItemMap.Find(ItemID))
		{
			return false;
		}

		// Set the filtered out flag
		Item->Flags.bIsFilteredOut = !SearchBoxFilter->PassesFilter(*Item);

		if (!Item->Flags.bIsFilteredOut)
		{
			AddUnfilteredItemToTree(Item);

			// Check if we need to do anything with this new item
			if (uint8* ActionMask = NewItemActions.Find(ItemID))
			{
				if (*ActionMask & ENewItemAction::Select)
				{
					OutlinerTreeView->ClearSelection();
					OutlinerTreeView->SetItemSelection(Item, true);
				}

				if (*ActionMask & ENewItemAction::Rename)
				{
					PendingRenameItem = Item;
				}

				if (*ActionMask & (ENewItemAction::ScrollIntoView | ENewItemAction::Rename))
				{
					ScrollItemIntoView(Item);
				}
			}
		}

		return true;
	}

	void SSceneOutliner::AddUnfilteredItemToTree(FTreeItemRef Item)
	{
		Item->SharedData = SharedData;

		FTreeItemPtr Parent = EnsureParentForItem(Item);

		const FTreeItemID ItemID = Item->GetID();
		if(TreeItemMap.Contains(ItemID))
		{
			UE_LOG(LogSceneOutliner, Error, TEXT("(%d | %s) already exists in tree.  Dumping map..."), GetTypeHash(ItemID), *Item->GetDisplayString() );
			for(TPair<FTreeItemID, FTreeItemPtr>& Entry : TreeItemMap)
			{
				UE_LOG(LogSceneOutliner, Log, TEXT("(%d | %s)"), GetTypeHash(Entry.Key), *Entry.Value->GetDisplayString());
			}

			// this is a fatal error
			check(false);
		}

		TreeItemMap.Add(ItemID, Item);

		if (Parent.IsValid())
		{
			Parent->AddChild(Item);
		}
		else
		{
			RootTreeItems.Add(Item);
		}

		Item->Visit(FOnItemAddedToTree(*this));
	}

	SSceneOutliner::FParentsExpansionState SSceneOutliner::GetParentsExpansionState() const
	{
		FParentsExpansionState States;
		for (const auto& Pair : TreeItemMap)
		{
			if (Pair.Value->GetChildren().Num())
			{
				States.Add(Pair.Key, Pair.Value->Flags.bIsExpanded);
			}
		}
		return States;
	}

	void SSceneOutliner::SetParentsExpansionState(const FParentsExpansionState& ExpansionStateInfo) const
	{
		for (const auto& Pair : TreeItemMap)
		{
			auto& Item = Pair.Value;
			if (Item->GetChildren().Num())
			{
				const bool* bIsExpanded = ExpansionStateInfo.Find(Pair.Key);
				if (bIsExpanded)
				{
					OutlinerTreeView->SetItemExpansion(Item, *bIsExpanded);
				}
				else
				{
					OutlinerTreeView->SetItemExpansion(Item, Item->Flags.bIsExpanded);
				}
			}
		}
	}

	void SSceneOutliner::PopulateSearchStrings(const ITreeItem& Item, TArray< FString >& OutSearchStrings) const
	{
		for (const auto& Pair : Columns)
		{
			Pair.Value->PopulateSearchStrings(Item, OutSearchStrings);
		}
	}

	TArray<FFolderTreeItem*> SSceneOutliner::GetSelectedFolders() const
	{
		return FItemSelection(*OutlinerTreeView).Folders;
	}

	TSharedPtr<SWidget> SSceneOutliner::OnOpenContextMenu()
	{
		TArray<AActor*> SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>( SelectedActors );

		/** Legacy context menu override only supports actors */
		if (SelectedActors.Num() && SharedData->ContextMenuOverride.IsBound())
		{
			return SharedData->ContextMenuOverride.Execute();
		}

		if (SharedData->Mode == ESceneOutlinerMode::ActorBrowsing)
		{
			// Make sure that no components are selected
			if (GEditor->GetSelectedComponentCount() > 0)
			{
				// We want to be able to undo to regain the previous component selection
				const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "ClickingOnActorsContextMenu", "Clicking on Actors (context menu)"));
				USelection* ComponentSelection = GEditor->GetSelectedComponents();
				ComponentSelection->Modify(false);
				ComponentSelection->DeselectAll();

				GUnrealEd->UpdatePivotLocationForSelection();
				GEditor->RedrawLevelEditingViewports(false);
			}

			return BuildDefaultContextMenu();
		}

		return TSharedPtr<SWidget>();
	}

	TSharedPtr<SWidget> SSceneOutliner::BuildDefaultContextMenu()
	{
		if (!CheckWorld())
		{
			return nullptr;
		}

		// Build up the menu for a selection
		const bool bCloseAfterSelection = true;
		FMenuBuilder MenuBuilder(bCloseAfterSelection, TSharedPtr<FUICommandList>(), SharedData->DefaultMenuExtender);

		const auto NumSelectedItems = OutlinerTreeView->GetNumItemsSelected();
		if (NumSelectedItems == 1)
		{
			OutlinerTreeView->GetSelectedItems()[0]->GenerateContextMenu(MenuBuilder, *this);
		}

		bool MenuBuilderHasContent = false;

		// We always create a section here, even if there is no parent so that clients can still extend the menu
		MenuBuilder.BeginSection("MainSection");
		{
			FItemSelection ItemSelection(*OutlinerTreeView);

			// Don't add any of these menu items if we're not showing the parent tree
			if (SharedData->bShowParentTree)
			{
				MenuBuilderHasContent = true;

				if (NumSelectedItems == 0)
				{
					const FSlateIcon NewFolderIcon(FEditorStyle::GetStyleSetName(), "SceneOutliner.NewFolderIcon");
					MenuBuilder.AddMenuEntry(LOCTEXT("CreateFolder", "Create Folder"), FText(), NewFolderIcon, FUIAction(FExecuteAction::CreateSP(this, &SSceneOutliner::CreateFolder)));
				}
				else
				{
					// Can't move worlds or level blueprints
					const bool bCanMoveSelection = ItemSelection.Worlds.Num() == 0;
					if (bCanMoveSelection)
					{
						MenuBuilder.AddSubMenu(
							LOCTEXT("MoveActorsTo", "Move To"),
							LOCTEXT("MoveActorsTo_Tooltip", "Move selection to another folder"),
							FNewMenuDelegate::CreateSP(this,  &SSceneOutliner::FillFoldersSubMenu));
					}

					// If we've only got folders selected, show the selection sub menu
					if (ItemSelection.Folders.Num() == NumSelectedItems)
					{
						MenuBuilder.AddSubMenu(
							LOCTEXT("SelectSubmenu", "Select"),
							LOCTEXT("SelectSubmenu_Tooltip", "Select the contents of the current selection"),
							FNewMenuDelegate::CreateSP(this, &SSceneOutliner::FillSelectionSubMenu));
					}
				}
			}
		}
		MenuBuilder.EndSection();

		if (MenuBuilderHasContent)
		{
			return MenuBuilder.MakeWidget();
		}

		return nullptr;
	}

	void SSceneOutliner::FillFoldersSubMenu(FMenuBuilder& MenuBuilder) const
	{
		MenuBuilder.AddMenuEntry(LOCTEXT( "CreateNew", "Create New Folder" ), LOCTEXT( "CreateNew_ToolTip", "Move to a new folder" ),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SceneOutliner.NewFolderIcon"), FExecuteAction::CreateSP(this, &SSceneOutliner::CreateFolder));

		AddMoveToFolderOutliner(MenuBuilder);
	}

	TSharedRef<TSet<FName>> SSceneOutliner::GatherInvalidMoveToDestinations() const
	{
		// We use a pointer here to save copying the whole array for every invocation of the filter delegate
		TSharedRef<TSet<FName>> ExcludedParents(new TSet<FName>());

		struct FFindInvalidFolders : ITreeItemVisitor
		{
			TSet<FName>& ExcludedParents;
			const TMap<FTreeItemID, FTreeItemPtr>& TreeItemMap;
			FFindInvalidFolders(TSet<FName>& InExcludedParents, const TMap<FTreeItemID, FTreeItemPtr>& InTreeItemMap)
				: ExcludedParents(InExcludedParents), TreeItemMap(InTreeItemMap)
			{}

			static bool ItemHasSubFolders(const TWeakPtr<ITreeItem>& WeakItem)
			{
				bool bHasSubFolder = false;
				WeakItem.Pin()->Visit(FFunctionalVisitor().Folder([&](const FFolderTreeItem&){
					bHasSubFolder = true;
				}));
				return bHasSubFolder;
			}

			virtual void Visit(const FActorTreeItem& ActorItem) const override
			{
				if (AActor* Actor = ActorItem.Actor.Get())
				{
					// We exclude actor parent folders if they don't have any sub folders
					const FName& Folder = Actor->GetFolderPath();
					if (!Folder.IsNone() && !ExcludedParents.Contains(Folder))
					{
						auto FolderItem = TreeItemMap.FindRef(Folder);
						if (FolderItem.IsValid() && !FolderItem->GetChildren().ContainsByPredicate(&ItemHasSubFolders))
						{
							ExcludedParents.Add(Folder);
						}
					}
				}
			}

			virtual void Visit(const FFolderTreeItem& Folder) const override
			{
				// Cannot move into its parent
				const FName ParentPath = GetParentPath(Folder.Path);
				if (!ParentPath.IsNone())
				{
					ExcludedParents.Add(ParentPath);
				}
				else
				{
					// Failing that, cannot move into itself, or any child
					ExcludedParents.Add(Folder.Path);
				}
			}
		};

		auto Visitor = FFindInvalidFolders(*ExcludedParents, TreeItemMap);
		for (const auto& Item : OutlinerTreeView->GetSelectedItems())
		{
			Item->Visit(Visitor);
		}

		return ExcludedParents;
	}

	void SSceneOutliner::AddMoveToFolderOutliner(FMenuBuilder& MenuBuilder) const
	{
		// We don't show this if there aren't any folders in the world
		if (!FActorFolders::Get().GetFolderPropertiesForWorld(*SharedData->RepresentingWorld).Num())
		{
			return;
		}

		// Add a mini scene outliner for choosing an existing folder
		FInitializationOptions MiniSceneOutlinerInitOptions;
		MiniSceneOutlinerInitOptions.bShowHeaderRow = false;
		MiniSceneOutlinerInitOptions.bFocusSearchBoxWhenOpened = true;
		MiniSceneOutlinerInitOptions.bOnlyShowFolders = true;
		
		// Don't show any folders that are a child of any of the selected folders
		auto ExcludedParents = GatherInvalidMoveToDestinations();
		if (ExcludedParents->Num())
		{
			// Add a filter if necessary
			auto FilterOutChildFolders = [](FName Path, TSharedRef<TSet<FName>> InExcludedParents){
				for (const auto& Parent : *InExcludedParents)
				{
					if (Path == Parent || FActorFolders::PathIsChildOf(Path.ToString(), Parent.ToString()))
					{
						return false;
					}
				}
				return true;
			};

			MiniSceneOutlinerInitOptions.Filters->AddFilterPredicate(FFolderFilterPredicate::CreateStatic(FilterOutChildFolders, ExcludedParents), EDefaultFilterBehaviour::Pass);
		}

		{
			// Filter in/out the world according to whether it is valid to move to/from the root
			FDragDropPayload DraggedObjects(OutlinerTreeView->GetSelectedItems());

			const bool bMoveToRootValid = FFolderDropTarget(FName()).ValidateDrop(DraggedObjects, *SharedData->RepresentingWorld).IsValid();

			MiniSceneOutlinerInitOptions.Filters->AddFilterPredicate(FWorldFilterPredicate::CreateStatic([](const UWorld*, bool bInMoveToRootValid){
				return bInMoveToRootValid;
			}, bMoveToRootValid), EDefaultFilterBehaviour::Pass);
		}

		// Don't show the actor info column
		MiniSceneOutlinerInitOptions.UseDefaultColumns();
		MiniSceneOutlinerInitOptions.ColumnMap.Remove(FBuiltInColumnTypes::ActorInfo());

		// Actor selector to allow the user to choose a folder
		FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
		TSharedRef< SWidget > MiniSceneOutliner =
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.MaxHeight(400.0f)
			[
				SNew(SSceneOutliner, MiniSceneOutlinerInitOptions)
				.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
				.OnItemPickedDelegate(FOnSceneOutlinerItemPicked::CreateSP(this, &SSceneOutliner::MoveSelectionTo))
			];

		MenuBuilder.BeginSection(FName(), LOCTEXT("ExistingFolders", "Existing:"));
		MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), false);
		MenuBuilder.EndSection();
	}

	void SSceneOutliner::FillSelectionSubMenu(FMenuBuilder& MenuBuilder) const
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "AddChildrenToSelection", "Immediate Children" ),
			LOCTEXT( "AddChildrenToSelection_ToolTip", "Select all immediate children of the selected folders" ),
			FSlateIcon(),
			FExecuteAction::CreateSP(this, &SSceneOutliner::SelectFoldersImmediateChildren));
		MenuBuilder.AddMenuEntry(
			LOCTEXT( "AddDescendantsToSelection", "All Descendants" ),
			LOCTEXT( "AddDescendantsToSelection_ToolTip", "Select all descendants of the selected folders" ),
			FSlateIcon(),
			FExecuteAction::CreateSP(this, &SSceneOutliner::SelectFoldersDescendants));
	}

	struct FSelectActors : ITreeItemVisitor
	{
		virtual void Visit(const FActorTreeItem& ActorItem) const override
		{
			if (AActor* Actor = ActorItem.Actor.Get())
			{
				GEditor->SelectActor(Actor, true, /*bNotify=*/false);
			}
		}
	};

	struct FSelectActorsRecursive : FSelectActors
	{
		using FSelectActors::Visit;

		virtual void Visit(const FFolderTreeItem& FolderItem) const override
		{
			for (auto& Child : FolderItem.GetChildren())
			{
				Child.Pin()->Visit(FSelectActorsRecursive());
			}
		}
	};

	void SSceneOutliner::SelectFoldersImmediateChildren() const
	{
		auto SelectedFolders = GetSelectedFolders();
		if (SelectedFolders.Num())
		{
			// We'll batch selection changes instead by using BeginBatchSelectOperation()
			GEditor->GetSelectedActors()->BeginBatchSelectOperation();

			OutlinerTreeView->ClearSelection();

			for (const auto& Folder : SelectedFolders)
			{
				for (auto& Child : Folder->GetChildren())
				{
					Child.Pin()->Visit(FSelectActors());
				}
			}

			GEditor->GetSelectedActors()->EndBatchSelectOperation();
			GEditor->NoteSelectionChange();
		}
	}

	void SSceneOutliner::SelectFoldersDescendants() const
	{
		auto SelectedFolders = GetSelectedFolders();
		if (SelectedFolders.Num())
		{
			// We'll batch selection changes instead by using BeginBatchSelectOperation()
			GEditor->GetSelectedActors()->BeginBatchSelectOperation();

			OutlinerTreeView->ClearSelection();

			for (const auto& Folder : SelectedFolders)
			{
				Folder->Visit(FSelectActorsRecursive());
			}

			GEditor->GetSelectedActors()->EndBatchSelectOperation();
			GEditor->NoteSelectionChange();
		}
	}

	void SSceneOutliner::MoveSelectionTo(FTreeItemRef NewParent)
	{
		struct FMoveToFolder : ITreeItemVisitor
		{
			SSceneOutliner& Outliner;
			FMoveToFolder(SSceneOutliner& InOutliner) : Outliner(InOutliner) {}

			virtual void Visit(const FFolderTreeItem& Folder) const override
			{
				Outliner.MoveSelectionTo(Folder.Path);
			}
			virtual void Visit(const FWorldTreeItem&) const override
			{
				Outliner.MoveSelectionTo(FName());
			}
		};

		NewParent->Visit(FMoveToFolder(*this));
	}

	void SSceneOutliner::MoveSelectionTo(FName NewParent)
	{
		if (!CheckWorld())
		{
			return;
		}

		FSlateApplication::Get().DismissAllMenus();
		
		FFolderDropTarget 	DropTarget(NewParent);
		FDragDropPayload 	DraggedObjects(OutlinerTreeView->GetSelectedItems());

		FDragValidationInfo Validation = DropTarget.ValidateDrop(DraggedObjects, *SharedData->RepresentingWorld);
		if (!Validation.IsValid())
		{
			FNotificationInfo Info(Validation.ValidationText);
			Info.ExpireDuration = 3.0f;
			Info.bUseLargeFont = false;
			Info.bFireAndForget = true;
			Info.bUseSuccessFailIcons = true;
			FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
			return;
		}

		const FScopedTransaction Transaction( LOCTEXT("MoveOutlinerItems", "Move World Outliner Items") );
		DropTarget.OnDrop(DraggedObjects, *SharedData->RepresentingWorld, Validation, SNullWidget::NullWidget);
	}

	FReply SSceneOutliner::OnCreateFolderClicked()
	{
		CreateFolder();
		return FReply::Handled();
	}

	void SSceneOutliner::CreateFolder()
	{
		if (!CheckWorld())
		{
			return;
		}

		UWorld& World = *SharedData->RepresentingWorld;
		const FScopedTransaction Transaction(LOCTEXT("UndoAction_CreateFolder", "Create Folder"));

		const FName NewFolderName = FActorFolders::Get().GetDefaultFolderNameForSelection(World);
		FActorFolders::Get().CreateFolderContainingSelection(World, NewFolderName);

		auto PreviouslySelectedItems = OutlinerTreeView->GetSelectedItems();

		auto Visit = [&](const FFolderTreeItem& Folder)
		{
			MoveFolderTo(Folder.Path, NewFolderName, World);
		};
		auto Visitor = FFunctionalVisitor().Folder(Visit);

		// Move any selected folders into the new folder name
		for (const auto& Item : PreviouslySelectedItems)
		{
			Item->Visit(Visitor);
		}

		// At this point the new folder will be in our newly added list, so select it and open a rename when it gets refreshed
		NewItemActions.Add(NewFolderName, ENewItemAction::Select | ENewItemAction::Rename);
	}

	void SSceneOutliner::OnBroadcastFolderCreate(UWorld& InWorld, FName NewPath)
	{
		if (!ShouldShowFolders() || &InWorld != SharedData->RepresentingWorld)
		{
			return;
		}

		if (!TreeItemMap.Contains(NewPath))
		{
			ConstructItemFor<FFolderTreeItem>(NewPath);
		}
	}

	void SSceneOutliner::OnBroadcastFolderMove(UWorld& InWorld, FName OldPath, FName NewPath)
	{
		if (!ShouldShowFolders() || &InWorld != SharedData->RepresentingWorld)
		{
			return;
		}

		auto Item = TreeItemMap.FindRef(OldPath);
		if (Item.IsValid())
		{
			// Remove it from the map under the old ID (which is derived from the folder path)
			TreeItemMap.Remove(Item->GetID());

			// Now change the path and put it back in the map with its new ID
			auto Folder = StaticCastSharedPtr<FFolderTreeItem>(Item);
			Folder->Path = NewPath;
			Folder->LeafName = GetFolderLeafName(NewPath);

			TreeItemMap.Add(Item->GetID(), Item);

			// Add an operation to move the item in the hierarchy
			PendingOperations.Emplace(FPendingTreeOperation::Moved, Item.ToSharedRef());
			Refresh();
		}
	}

	void SSceneOutliner::OnBroadcastFolderDelete(UWorld& InWorld, FName Path)
	{
		if (&InWorld != SharedData->RepresentingWorld)
		{
			return;
		}

		auto* Folder = TreeItemMap.Find(Path);
		if (Folder)
		{
			PendingOperations.Emplace(FPendingTreeOperation::Removed, Folder->ToSharedRef());
			Refresh();
		}
	}

	void SSceneOutliner::ScrollItemIntoView(FTreeItemPtr Item)
	{
		auto Parent = Item->GetParent();
		while(Parent.IsValid())
		{
			OutlinerTreeView->SetItemExpansion(Parent->AsShared(), true);
			Parent = Parent->GetParent();
		}

		OutlinerTreeView->RequestScrollIntoView(Item);
	}

	void SSceneOutliner::InitiateRename(TSharedRef<ITreeItem> Item)
	{
		if (Item->CanInteract())
		{
			PendingRenameItem = Item;
			ScrollItemIntoView(Item);
		}
	}

	TSharedRef< ITableRow > SSceneOutliner::OnGenerateRowForOutlinerTree( FTreeItemPtr Item, const TSharedRef< STableViewBase >& OwnerTable )
	{
		return SNew( SSceneOutlinerTreeRow, OutlinerTreeView.ToSharedRef(), SharedThis(this) ).Item( Item );
	}

	void SSceneOutliner::OnGetChildrenForOutlinerTree( FTreeItemPtr InParent, TArray< FTreeItemPtr >& OutChildren )
	{
		if( SharedData->bShowParentTree )
		{
			for (auto& WeakChild : InParent->GetChildren())
			{
				auto Child = WeakChild.Pin();
				// Should never have bogus entries in this list
				check(Child.IsValid());
				OutChildren.Add(Child);
			}

			// If the item needs it's children sorting, do that now
			if (OutChildren.Num() && InParent->Flags.bChildrenRequireSort)
			{
				// Sort the children we returned
				SortItems(OutChildren);

				// Empty out the children and repopulate them in the correct order
				InParent->Children.Empty();
				for (auto& Child : OutChildren)
				{
					InParent->Children.Emplace(Child);
				}
				
				// They no longer need sorting
				InParent->Flags.bChildrenRequireSort = false;
			}
		}
	}

	static const FName SequencerActorTag(TEXT("SequencerActor"));

	bool SSceneOutliner::IsActorDisplayable( const AActor* Actor ) const
	{
		return	!SharedData->bOnlyShowFolders && 												// Don't show actors if we're only showing folders
				Actor->IsEditable() &&															// Only show actors that are allowed to be selected and drawn in editor
				Actor->IsListedInSceneOutliner() &&
				( (SharedData->bRepresentingPlayWorld || !Actor->HasAnyFlags(RF_Transient)) ||
				  (SharedData->bShowTransient && Actor->HasAnyFlags(RF_Transient)) ||			// Don't show transient actors in non-play worlds
				  (Actor->ActorHasTag(SequencerActorTag)) ) &&		
				!Actor->IsTemplate() &&															// Should never happen, but we never want CDOs displayed
				!FActorEditorUtils::IsABuilderBrush(Actor) &&									// Don't show the builder brush
				!Actor->IsA( AWorldSettings::StaticClass() ) &&									// Don't show the WorldSettings actor, even though it is technically editable
				!Actor->IsPendingKill() &&														// We don't want to show actors that are about to go away
				FLevelUtils::IsLevelVisible( Actor->GetLevel() );								// Only show Actors whose level is visible
	}

	void SSceneOutliner::OnOutlinerTreeSelectionChanged( FTreeItemPtr TreeItem, ESelectInfo::Type SelectInfo )
	{
		if (SelectInfo == ESelectInfo::Direct)
		{
			return;
		}

		if( SharedData->Mode == ESceneOutlinerMode::ActorPicker )
		{
			// In actor picking mode, we fire off the notification to whoever is listening.
			// This may often cause the widget itself to be enqueued for destruction
			if( OutlinerTreeView->GetNumItemsSelected() > 0 )
			{
				auto FirstItem = OutlinerTreeView->GetSelectedItems()[0];
				if (FirstItem->CanInteract())
				{
					OnItemPicked.ExecuteIfBound( FirstItem->AsShared() );
				}
			}
		}

		// We only synchronize selection when in actor browsing mode
		else if( SharedData->Mode == ESceneOutlinerMode::ActorBrowsing )
		{
			if( !bIsReentrant )
			{
				TGuardValue<bool> ReentrantGuard(bIsReentrant,true);

				// @todo outliner: Can be called from non-interactive selection

				// The tree let us know that selection has changed, but wasn't able to tell us
				// what changed.  So we'll perform a full difference check and update the editor's
				// selected actors to match the control's selection set.

				// Make a list of all the actors that should now be selected in the world.
				FItemSelection Selection(*OutlinerTreeView);
				auto SelectedActors = TSet<AActor*>(Selection.GetActorPtrs());

				bool bChanged = false;
				bool bAnyInPIE = false;
				for (auto* Actor : SelectedActors)
				{
					if (!bAnyInPIE && Actor && Actor->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
					{
						bAnyInPIE = true;
					}
					if (!GEditor->GetSelectedActors()->IsSelected(Actor))
					{
						bChanged = true;
						break;
					}
				}

				for (FSelectionIterator SelectionIt( *GEditor->GetSelectedActors() ); SelectionIt && !bChanged; ++SelectionIt)
				{
					AActor* Actor = CastChecked< AActor >( *SelectionIt );
					if (!bAnyInPIE && Actor->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
					{
						bAnyInPIE = true;
					}
					if (!SelectedActors.Contains(Actor))
					{
						// Actor has been deselected
						bChanged = true;

						// If actor was a group actor, remove its members from the ActorsToSelect list
						AGroupActor* DeselectedGroupActor = Cast<AGroupActor>(Actor);
						if ( DeselectedGroupActor )
						{
							TArray<AActor*> GroupActors;
							DeselectedGroupActor->GetGroupActors( GroupActors );

							for (auto* GroupActor : GroupActors)
							{
								SelectedActors.Remove(GroupActor);
							}

						}
					}
				}

				// If there's a discrepancy, update the selected actors to reflect this list.
				if ( bChanged )
				{
					const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnActors", "Clicking on Actors"), !bAnyInPIE );
					GEditor->GetSelectedActors()->Modify();

					// Clear the selection.
					GEditor->SelectNone(false, true, true);

					// We'll batch selection changes instead by using BeginBatchSelectOperation()
					GEditor->GetSelectedActors()->BeginBatchSelectOperation();

					const bool bShouldSelect = true;
					const bool bNotifyAfterSelect = false;
					const bool bSelectEvenIfHidden = true;	// @todo outliner: Is this actually OK?
					for (auto* Actor : SelectedActors)
					{
						UE_LOG(LogSceneOutliner, Verbose,  TEXT("Clicking on Actor (world outliner): %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel());
						GEditor->SelectActor( Actor, bShouldSelect, bNotifyAfterSelect, bSelectEvenIfHidden );
					}

					// Commit selection changes
					GEditor->GetSelectedActors()->EndBatchSelectOperation();

					// Fire selection changed event
					GEditor->NoteSelectionChange();
				}

				bActorSelectionDirty = true;
			}
		}
	}

	
	void SSceneOutliner::OnLevelSelectionChanged(UObject* Obj)
	{
		// We only synchronize selection when in actor browsing mode
		if( SharedData->Mode == ESceneOutlinerMode::ActorBrowsing )
		{
			// @todo outliner: Because we are not notified of which items are being added/removed from selection, we have
			// no immediate means to incrementally update the tree when selection changes.

			// Ideally, we can improve the filtering paradigm to better support incremental updates in cases such as these
			if ( IsShowingOnlySelected() )
			{
				FullRefresh();
			}
			else if (!bIsReentrant)
			{
				OutlinerTreeView->ClearSelection();
				bActorSelectionDirty = true;

				// Scroll last item into view - this means if we are multi-selecting, we show newest selection. @TODO Not perfect though
				if (AActor* LastSelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>())
				{
					auto TreeItem = TreeItemMap.FindRef(LastSelectedActor);
					if (TreeItem.IsValid())
					{
						if (!OutlinerTreeView->IsItemVisible(TreeItem))
						{
							ScrollItemIntoView(TreeItem);
						}
					}
					else
					{
						OnItemAdded(LastSelectedActor, ENewItemAction::ScrollIntoView);
					}
				}
			}
		}
	}

	void SSceneOutliner::OnOutlinerTreeDoubleClick( FTreeItemPtr TreeItem )
	{
		// We only deal with double clicks when in actor browsing mode
		if( SharedData->Mode == ESceneOutlinerMode::ActorBrowsing )
		{
			auto ExpandCollapseFolder = [&](const FFolderTreeItem& Folder){
				auto Shared = const_cast<FFolderTreeItem&>(Folder).AsShared();
				OutlinerTreeView->SetItemExpansion(Shared, !OutlinerTreeView->IsItemExpanded(Shared));
			};

			if (TreeItem->CanInteract())
			{
				TreeItem->Visit(FFunctionalVisitor()

					.Actor([&](const FActorTreeItem&){
						// Move all actors into view
						FItemSelection Selection(*OutlinerTreeView);
						if( Selection.Actors.Num() > 0 )
						{
							const bool bActiveViewportOnly = false;
							GEditor->MoveViewportCamerasToActor( Selection.GetActorPtrs(), bActiveViewportOnly );
						}
					})

					.Folder(ExpandCollapseFolder)

					.World([](const FWorldTreeItem& WorldItem){
						WorldItem.OpenWorldSettings();
					})

				);

			}
			else
			{
				TreeItem->Visit(FFunctionalVisitor()

					.Folder(ExpandCollapseFolder)

					.Actor([&](const FActorTreeItem& Item){
						// Move just this actor into view
						if (AActor* Actor = Item.Actor.Get())
						{
							const bool bActiveViewportOnly = false;
							GEditor->MoveViewportCamerasToActor( *Actor, bActiveViewportOnly );
						}
					})
				);
			}
		}
	}

	void SSceneOutliner::OnOutlinerTreeItemScrolledIntoView( FTreeItemPtr TreeItem, const TSharedPtr<ITableRow>& Widget )
	{
		if (TreeItem == PendingRenameItem.Pin())
		{
			PendingRenameItem = nullptr;
			TreeItem->RenameRequestEvent.ExecuteIfBound();
		}
	}
	
	void SSceneOutliner::OnItemExpansionChanged(FTreeItemPtr TreeItem, bool bIsExpanded) const
	{
		TreeItem->Flags.bIsExpanded = bIsExpanded;
		TreeItem->OnExpansionChanged();

		// Expand any children that are also expanded
		for (auto WeakChild : TreeItem->GetChildren())
		{
			auto Child = WeakChild.Pin();
			if (Child->Flags.bIsExpanded)
			{
				OutlinerTreeView->SetItemExpansion(Child, true);
			}
		}
	}

	void SSceneOutliner::OnLevelAdded(ULevel* InLevel, UWorld* InWorld)
	{
		FullRefresh();
	}

	void SSceneOutliner::OnLevelRemoved(ULevel* InLevel, UWorld* InWorld)
	{
		FullRefresh();
	}

	void SSceneOutliner::OnLevelActorsAdded(AActor* InActor)
	{
		if( !bIsReentrant )
		{
			if( InActor && SharedData->RepresentingWorld == InActor->GetWorld() && IsActorDisplayable(InActor) )
			{
				if (!TreeItemMap.Find(InActor) && !PendingTreeItemMap.Find(InActor))
				{
					// Update the total actor count that match the filters
					if (Filters->PassesAllFilters(FActorTreeItem(InActor)))
					{
						ApplicableActors.Emplace(InActor);
					}

					ConstructItemFor<FActorTreeItem>(InActor);
				}
			}
		}
	}

	void SSceneOutliner::OnLevelActorsRemoved(AActor* InActor)
	{
		if( !bIsReentrant )
		{
			if( InActor && SharedData->RepresentingWorld == InActor->GetWorld() )
			{
				ApplicableActors.Remove(InActor);
				auto* ItemPtr = TreeItemMap.Find(InActor);
				if (!ItemPtr)
				{
					ItemPtr = PendingTreeItemMap.Find(InActor);
				}

				if (ItemPtr)
				{
					PendingOperations.Emplace(FPendingTreeOperation::Removed, ItemPtr->ToSharedRef());
					Refresh();
				}
			}
		}
	}

	void SSceneOutliner::OnLevelActorsAttached(AActor* InActor, const AActor* InParent)
	{
		// InActor can be equal to InParent in cases of components being attached internally. The Scene Outliner does not need to do anything in this case.
		if( !bIsReentrant && InActor != InParent )
		{
			if( InActor && SharedData->RepresentingWorld == InActor->GetWorld() )
			{
				if (auto* ItemPtr = TreeItemMap.Find(InActor))
				{
					PendingOperations.Emplace(FPendingTreeOperation::Moved, ItemPtr->ToSharedRef());
					Refresh();
				}
			}
		}
	}

	void SSceneOutliner::OnLevelActorsDetached(AActor* InActor, const AActor* InParent)
	{
		// InActor can be equal to InParent in cases of components being attached internally. The Scene Outliner does not need to do anything in this case.
		if( !bIsReentrant && InActor != InParent)
		{
			if( InActor && SharedData->RepresentingWorld == InActor->GetWorld() )
			{
				if (auto* ItemPtr = TreeItemMap.Find(InActor))
				{
					PendingOperations.Emplace(FPendingTreeOperation::Moved, ItemPtr->ToSharedRef());
					Refresh();
				}
				else
				{
					// We should find the item, but if we don't, do an add.
					OnLevelActorsAdded(InActor);
				}
			}
		}
	}

	/** Called by the engine when an actor's folder is changed */
	void SSceneOutliner::OnLevelActorFolderChanged(const AActor* InActor, FName OldPath)
	{
		auto* ActorTreeItem = TreeItemMap.Find(InActor);
		if (!ShouldShowFolders() || !InActor || !ActorTreeItem)
		{
			return;
		}
		
		PendingOperations.Emplace(FPendingTreeOperation::Moved, ActorTreeItem->ToSharedRef());
		Refresh();
	}

	void SSceneOutliner::OnLevelActorsRequestRename(const AActor* InActor)
	{
		auto SelectedItems = OutlinerTreeView->GetSelectedItems();
		if( SelectedItems.Num() > 0)
		{
			// Ensure that the item we want to rename is visible in the tree
			FTreeItemPtr ItemToRename = SelectedItems[SelectedItems.Num() - 1];
			if (ItemToRename->CanInteract())
			{
				PendingRenameItem = ItemToRename->AsShared();
				ScrollItemIntoView(ItemToRename);
			}
		}
	}

	void SSceneOutliner::OnMapChange(uint32 MapFlags)
	{
		FullRefresh();
	}

	void SSceneOutliner::OnNewCurrentLevel()
	{
		if (IsShowingOnlyCurrentLevel())
		{
			FullRefresh();
		}
	}

	void SSceneOutliner::PostUndo(bool bSuccess)
	{
		// Refresh our tree in case any changes have been made to the scene that might effect our actor list
		if( !bIsReentrant )
		{
			FullRefresh();
		}
	}

	void SSceneOutliner::OnActorLabelChanged(AActor* ChangedActor)
	{
		if ( !ensure(ChangedActor) )
		{
			return;
		}
		
		auto TreeItem = TreeItemMap.FindRef(ChangedActor);
		if (TreeItem.IsValid())
		{
			if (SearchBoxFilter->PassesFilter(*TreeItem))
			{
				OutlinerTreeView->FlashHighlightOnItem(TreeItem);
				RequestSort();
			}
			else
			{
				// Do longer matches the filters, remove it
				PendingOperations.Emplace(FPendingTreeOperation::Removed, TreeItem.ToSharedRef());
				Refresh();
			}
		}
		else if (IsActorDisplayable(ChangedActor))
		{
			// Attempt to add the item if we didn't find it - perhaps it now matches the filter?
			ConstructItemFor<FActorTreeItem>(ChangedActor);
		}
	}

	void SSceneOutliner::OnFilterTextChanged( const FText& InFilterText )
	{
		SearchBoxFilter->SetRawFilterText( InFilterText );
		FilterTextBoxWidget->SetError( SearchBoxFilter->GetFilterErrorText() );
	}

	void SSceneOutliner::OnFilterTextCommitted( const FText& InFilterText, ETextCommit::Type CommitInfo )
	{
		const FString CurrentFilterText = InFilterText.ToString();
		// We'll only select actors if the user actually pressed the enter key.  We don't want to change
		// selection just because focus was lost from the search text field.
		if( CommitInfo == ETextCommit::OnEnter )
		{
			// Any text in the filter?  If not, we won't bother doing anything
			if( !CurrentFilterText.IsEmpty() )
			{
				FItemSelection Selection;

				// Gather all of the actors that match the filter text
				for (auto& Pair : TreeItemMap)
				{
					if (!Pair.Value->Flags.bIsFilteredOut)
					{
						Pair.Value->Visit(Selection);
					}
				}

				// We only select level actors when in actor browsing mode
				if( SharedData->Mode == ESceneOutlinerMode::ActorBrowsing )
				{
					// Start batching selection changes
					GEditor->GetSelectedActors()->BeginBatchSelectOperation();

					// Select actors (and only the actors) that match the filter text
					const bool bNoteSelectionChange = false;
					const bool bDeselectBSPSurfs = false;
					const bool WarnAboutManyActors = true;
					GEditor->SelectNone( bNoteSelectionChange, bDeselectBSPSurfs, WarnAboutManyActors );
					for (auto* Actor : Selection.GetActorPtrs())
					{
						const bool bShouldSelect = true;
						const bool bSelectEvenIfHidden = false;
						GEditor->SelectActor( Actor, bShouldSelect, bNoteSelectionChange, bSelectEvenIfHidden );
					}

					// Commit selection changes
					GEditor->GetSelectedActors()->EndBatchSelectOperation();

					// Fire selection changed event
					GEditor->NoteSelectionChange();

					// Set keyboard focus to the SceneOutliner, so the user can perform keyboard commands that interact
					// with selected actors (such as Delete, to delete selected actors.)
					SetKeyboardFocus();
				}

				// In 'actor picking' mode, we allow the user to commit their selection by pressing enter
				// in the search window when a single actor is available
				else if( SharedData->Mode == ESceneOutlinerMode::ActorPicker )
				{
					// In actor picking mode, we check to see if we have a selected actor, and if so, fire
					// off the notification to whoever is listening.  This may often cause the widget itself
					// to be enqueued for destruction
					if( Selection.Actors.Num() == 1 )
					{
						// Signal that an actor was selected. We assume it is valid as it won't have been added to ActorsToSelect if not.
						OnItemPicked.ExecuteIfBound( Selection.Actors[0]->AsShared() );
					}
				}
			}
		}
	}

	EVisibility SSceneOutliner::GetFilterStatusVisibility() const
	{
		return IsFilterActive() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SSceneOutliner::GetEmptyLabelVisibility() const
	{
		return ( IsFilterActive() || RootTreeItems.Num() > 0 ) ? EVisibility::Collapsed : EVisibility::Visible;
	}

	FText SSceneOutliner::GetFilterStatusText() const
	{
		const int32 TotalActorCount = ApplicableActors.Num();

		int32 SelectedActorCount = 0;
		auto Count = [&](const FActorTreeItem&) { ++SelectedActorCount; };
		for (const auto& Item : OutlinerTreeView->GetSelectedItems())
		{
			Item->Visit(FFunctionalVisitor().Actor(Count));
		}

		if ( !IsFilterActive() )
		{
			if (SelectedActorCount == 0)
			{
				return FText::Format( LOCTEXT("ShowingAllActorsFmt", "{0} actors"), FText::AsNumber( TotalActorCount ) );
			}
			else
			{
				return FText::Format( LOCTEXT("ShowingAllActorsSelectedFmt", "{0} actors ({1} selected)"), FText::AsNumber( TotalActorCount ), FText::AsNumber( SelectedActorCount ) );
			}
		}
		else if( IsFilterActive() && FilteredActorCount == 0 )
		{
			return FText::Format( LOCTEXT("ShowingNoActorsFmt", "No matching actors ({0} total)"), FText::AsNumber( TotalActorCount ) );
		}
		else if (SelectedActorCount != 0)
		{
			return FText::Format( LOCTEXT("ShowingOnlySomeActorsSelectedFmt", "Showing {0} of {1} actors ({2} selected)"), FText::AsNumber( FilteredActorCount ), FText::AsNumber( TotalActorCount ), FText::AsNumber( SelectedActorCount ) );
		}
		else
		{
			return FText::Format( LOCTEXT("ShowingOnlySomeActorsFmt", "Showing {0} of {1} actors"), FText::AsNumber( FilteredActorCount ), FText::AsNumber( TotalActorCount ) );
		}
	}

	FSlateColor SSceneOutliner::GetFilterStatusTextColor() const
	{
		if ( !IsFilterActive() )
		{
			// White = no text filter
			return FLinearColor( 1.0f, 1.0f, 1.0f );
		}
		else if( FilteredActorCount == 0 )
		{
			// Red = no matching actors
			return FLinearColor( 1.0f, 0.4f, 0.4f );
		}
		else
		{
			// Green = found at least one match!
			return FLinearColor( 0.4f, 1.0f, 0.4f );
		}
	}

	bool SSceneOutliner::IsFilterActive() const
	{
		return FilterTextBoxWidget->GetText().ToString().Len() > 0 && ApplicableActors.Num() != FilteredActorCount;
	}

	const FSlateBrush* SSceneOutliner::GetFilterButtonGlyph() const
	{
		if( IsFilterActive() )
		{
			return FEditorStyle::GetBrush(TEXT("SceneOutliner.FilterCancel"));
		}
		else
		{
			return FEditorStyle::GetBrush(TEXT("SceneOutliner.FilterSearch"));
		}
	}

	FString SSceneOutliner::GetFilterButtonToolTip() const
	{
		return IsFilterActive() ? LOCTEXT("ClearSearchFilter", "Clear search filter").ToString() : LOCTEXT("StartSearching", "Search").ToString();

	}

	TAttribute<FText> SSceneOutliner::GetFilterHighlightText() const
	{
		return TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic([](TWeakPtr<TreeItemTextFilter> Filter){
			auto FilterPtr = Filter.Pin();
			return FilterPtr.IsValid() ? FilterPtr->GetRawFilterText() : FText();
		}, TWeakPtr<TreeItemTextFilter>(SearchBoxFilter)));
	}

	void SSceneOutliner::SetKeyboardFocus()
	{
		if (SupportsKeyboardFocus())
		{
			FWidgetPath OutlinerTreeViewWidgetPath;
			// NOTE: Careful, GeneratePathToWidget can be reentrant in that it can call visibility delegates and such
			FSlateApplication::Get().GeneratePathToWidgetUnchecked( OutlinerTreeView.ToSharedRef(), OutlinerTreeViewWidgetPath );
			FSlateApplication::Get().SetKeyboardFocus( OutlinerTreeViewWidgetPath, EFocusCause::SetDirectly );
		}
	}

	bool SSceneOutliner::SupportsKeyboardFocus() const
	{
		// We only need to support keyboard focus if we're in actor browsing mode
		if( SharedData->Mode == ESceneOutlinerMode::ActorBrowsing )
		{
			// Scene outliner needs keyboard focus so the user can press keys to activate commands, such as the Delete
			// key to delete selected actors
			return true;
		}

		return false;
	}

	FReply SSceneOutliner::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		// @todo outliner: Use command system for these for discoverability? (allow bindings?)

		// We only allow these operations in actor browsing mode
		if( SharedData->Mode == ESceneOutlinerMode::ActorBrowsing )
		{
			// Rename key: Rename selected actors (not rebindable, because it doesn't make much sense to bind.)
			if( InKeyEvent.GetKey() == EKeys::F2 )
			{
				if (OutlinerTreeView->GetNumItemsSelected() == 1)
				{
					FTreeItemPtr ItemToRename = OutlinerTreeView->GetSelectedItems()[0];
					
					if (ItemToRename->CanInteract())
					{
						PendingRenameItem = ItemToRename->AsShared();
						ScrollItemIntoView(ItemToRename);
					}

					return FReply::Handled();
				}
			}

			// F5 forces a full refresh
			else if ( InKeyEvent.GetKey() == EKeys::F5 )
			{
				FullRefresh();
				return FReply::Handled();
			}

			// Delete key: Delete selected actors (not rebindable, because it doesn't make much sense to bind.)
			else if ( InKeyEvent.GetKey() == EKeys::Platform_Delete )
			{
				const FItemSelection Selection(*OutlinerTreeView);

				if( SharedData->CustomDelete.IsBound() )
				{
					SharedData->CustomDelete.Execute( Selection.GetWeakActors() );
				}
				else if (CheckWorld())
				{
					const FScopedTransaction Transaction( LOCTEXT("UndoAction_DeleteSelection", "Delete selection") );

					// Delete selected folders too
					auto SelectedItems = OutlinerTreeView->GetSelectedItems();
					
					GEditor->SelectNone(true, true);

					for (auto* Folder : Selection.Folders)
					{
						Folder->Delete();
					}

					for (auto* Actor : Selection.GetActorPtrs())
					{
						GEditor->SelectActor(Actor, true, false);
					}

					// Code from FLevelEditorActionCallbacks::Delete_CanExecute()
					// Should this be just return FReply::Unhandled()?
					TArray<FEdMode*> ActiveModes; 
					GLevelEditorModeTools().GetActiveModes( ActiveModes );
					for( int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex )
					{
						const EEditAction::Type CanProcess = ActiveModes[ModeIndex]->GetActionEditDelete();
						if (CanProcess == EEditAction::Process)
						{
							// We don't consider the return value here, as false is assumed to mean there was an internal error processing delete, not that it should defer to other modes/default behaviour
							ActiveModes[ModeIndex]->ProcessEditDelete();
							return FReply::Handled();
						}
						else if (CanProcess == EEditAction::Halt)
						{
							return FReply::Unhandled();
						}
					}

					if (GUnrealEd->CanDeleteSelectedActors( SharedData->RepresentingWorld, true, false ))
					{
						GEditor->edactDeleteSelected( SharedData->RepresentingWorld );
					}
				}

				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}

	void SSceneOutliner::SynchronizeActorSelection()
	{
		TGuardValue<bool> ReentrantGuard(bIsReentrant, true);

		USelection* SelectedActors = GEditor->GetSelectedActors();

		// Deselect actors in the tree that are no longer selected in the world
		FItemSelection Selection(*OutlinerTreeView);
		for (FActorTreeItem* ActorItem : Selection.Actors)
		{
			if(!ActorItem->Actor.IsValid() || !ActorItem->Actor.Get()->IsSelected())
			{
				OutlinerTreeView->SetItemSelection(ActorItem->AsShared(), false);
			}
		}

		// Ensure that all selected actors in the world are selected in the tree
		for (FSelectionIterator SelectionIt( *SelectedActors ); SelectionIt; ++SelectionIt)
		{
			AActor* Actor = CastChecked< AActor >(*SelectionIt);
			if (FTreeItemPtr* ActorItem = TreeItemMap.Find(Actor))
			{
				OutlinerTreeView->SetItemSelection(*ActorItem, true);
			}
		}

		// Broadcast selection changed delegate
		SelectionChanged.Broadcast();
	}

	void SSceneOutliner::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
	{	
		for (auto& Pair : Columns)
		{
			Pair.Value->Tick(InCurrentTime, InDeltaTime);
		}

		if ( bPendingFocusNextFrame && FilterTextBoxWidget->GetVisibility() == EVisibility::Visible )
		{
			FWidgetPath WidgetToFocusPath;
			FSlateApplication::Get().GeneratePathToWidgetUnchecked( FilterTextBoxWidget.ToSharedRef(), WidgetToFocusPath );
			FSlateApplication::Get().SetKeyboardFocus( WidgetToFocusPath, EFocusCause::SetDirectly );
			bPendingFocusNextFrame = false;
		}

		if( bNeedsRefresh )
		{
			if( !bIsReentrant )
			{
				Populate();
			}
		}
		SortOutlinerTimer -= InDeltaTime;

		if (bSortDirty && (!SharedData->bRepresentingPlayWorld || SortOutlinerTimer <= 0))
		{
			SortItems(RootTreeItems);
			for (const auto& Pair : TreeItemMap)
			{
				Pair.Value->Flags.bChildrenRequireSort = true;
			}

			OutlinerTreeView->RequestTreeRefresh();
			bSortDirty = false;
		}

		if (SortOutlinerTimer <= 0)
		{
			SortOutlinerTimer = SCENE_OUTLINER_RESORT_TIMER;
		}


		if (bActorSelectionDirty)
		{
			SynchronizeActorSelection();
			bActorSelectionDirty = false;
		}
	}

	EColumnSortMode::Type SSceneOutliner::GetColumnSortMode( const FName ColumnId ) const
	{
		if (SortByColumn == ColumnId)
		{
			auto Column = Columns.FindRef(ColumnId);
			if (Column.IsValid() && Column->SupportsSorting())
			{
				return SortMode;
			}
		}

		return EColumnSortMode::None;
	}

	void SSceneOutliner::OnColumnSortModeChanged( const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode )
	{
		auto Column = Columns.FindRef(ColumnId);
		if (!Column.IsValid() || !Column->SupportsSorting())
		{
			return;
		}

		SortByColumn = ColumnId;
		SortMode = InSortMode;

		RequestSort();
	}

	void SSceneOutliner::RequestSort()
	{
		bSortDirty = true;
	}

	void SSceneOutliner::SortItems(TArray<FTreeItemPtr>& Items) const
	{
		auto Column = Columns.FindRef(SortByColumn);
		if (Column.IsValid())
		{
			Column->SortItems(Items, SortMode);
		}
	}

	void SSceneOutliner::FOnItemAddedToTree::Visit(FActorTreeItem& ActorItem) const
	{
		Outliner.FilteredActorCount += ActorItem.Flags.bIsFilteredOut ? 0 : 1;

		// Synchronize selection
		if (Outliner.SharedData->Mode == ESceneOutlinerMode::ActorBrowsing && GEditor->GetSelectedActors()->IsSelected(ActorItem.Actor.Get()))
		{
			// Have to const cast here as the tree view is templated on non-const ptrs
			Outliner.OutlinerTreeView->SetItemSelection(ActorItem.AsShared(), true);
		}
	}

	void SSceneOutliner::FOnItemAddedToTree::Visit(FFolderTreeItem& Folder) const
	{
		if (!Outliner.SharedData->RepresentingWorld)
		{
			return;
		}

		if (FActorFolderProps* Props = FActorFolders::Get().GetFolderProperties(*Outliner.SharedData->RepresentingWorld, Folder.Path))
		{
			Folder.Flags.bIsExpanded = Props->bIsExpanded;
		}
	}

	void SSceneOutliner::OnSelectWorld(TWeakObjectPtr<UWorld> InWorld)
	{
		SharedData->UserChosenWorld = InWorld;
		FullRefresh();
	}

	bool SSceneOutliner::IsWorldChecked(TWeakObjectPtr<UWorld> InWorld)
	{
		return (InWorld == SharedData->UserChosenWorld);
	}

	void SSceneOutliner::SetItemExpansionRecursive(FTreeItemPtr Model, bool bInExpansionState)
	{
		if (Model.IsValid())
		{
			OutlinerTreeView->SetItemExpansion(Model, bInExpansionState);
			for (auto& Child : Model->Children)
			{
				if (Child.IsValid())
				{
					SetItemExpansionRecursive(Child.Pin(), bInExpansionState);
				}
			}
		}
	}
}	// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE
