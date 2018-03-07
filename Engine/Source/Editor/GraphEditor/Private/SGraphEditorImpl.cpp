// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGraphEditorImpl.h"
#include "GraphEditAction.h"
#include "EdGraph/EdGraph.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "GraphEditorModule.h"
#include "SGraphPanel.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "SGraphEditorActionMenu.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "GraphEditorModule"

/////////////////////////////////////////////////////
// SGraphEditorImpl

FVector2D SGraphEditorImpl::GetPasteLocation() const
{
	return GraphPanel->GetPastePosition();
}

bool SGraphEditorImpl::IsNodeTitleVisible( const UEdGraphNode* Node, bool bEnsureVisible )
{
	return GraphPanel->IsNodeTitleVisible(Node, bEnsureVisible);
}

void SGraphEditorImpl::JumpToNode( const UEdGraphNode* JumpToMe, bool bRequestRename, bool bSelectNode )
{
	GraphPanel->JumpToNode(JumpToMe, bRequestRename, bSelectNode);
	FocusLockedEditorHere();
}


void SGraphEditorImpl::JumpToPin( const UEdGraphPin* JumpToMe )
{
	GraphPanel->JumpToPin(JumpToMe);
	FocusLockedEditorHere();
}


bool SGraphEditorImpl::SupportsKeyboardFocus() const
{
	return true;
}

FReply SGraphEditorImpl::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	OnFocused.ExecuteIfBound(SharedThis(this));
	return FReply::Handled();
}

FReply SGraphEditorImpl::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if(MouseEvent.IsMouseButtonDown(EKeys::ThumbMouseButton))
	{
		OnNavigateHistoryBack.ExecuteIfBound();
	}
	else if(MouseEvent.IsMouseButtonDown(EKeys::ThumbMouseButton2))
	{
		OnNavigateHistoryForward.ExecuteIfBound();
	}
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SGraphEditorImpl::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	int32 NumNodes = GetCurrentGraph()->Nodes.Num();
	if (Commands->ProcessCommandBindings( InKeyEvent ) )
	{
		bool bPasteOperation = InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::V;

		if(	!bPasteOperation && GetCurrentGraph()->Nodes.Num() > NumNodes )
		{
			OnNodeSpawnedByKeymap.ExecuteIfBound();
		}
		return FReply::Handled();
	}
	else
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
}

void SGraphEditorImpl::NotifyGraphChanged()
{
	FEdGraphEditAction DefaultAction;
	OnGraphChanged(DefaultAction);
}

void SGraphEditorImpl::OnGraphChanged(const FEdGraphEditAction& InAction)
{
	if ( !bIsActiveTimerRegistered )
	{
		const UEdGraphSchema* Schema = EdGraphObj->GetSchema();
		const bool bSchemaRequiresFullRefresh = Schema->ShouldAlwaysPurgeOnModification();

		const bool bWasAddAction = (InAction.Action & GRAPHACTION_AddNode) != 0;
		const bool bWasSelectAction = (InAction.Action & GRAPHACTION_SelectNode) != 0;
		const bool bWasRemoveAction = (InAction.Action & GRAPHACTION_RemoveNode) != 0;

		// If we did a 'default action' (or some other action not handled by SGraphPanel::OnGraphChanged
		// or if we're using a schema that always needs a full refresh, then purge the current nodes
		// and queue an update:
		if (bSchemaRequiresFullRefresh || 
			(!bWasAddAction && !bWasSelectAction && !bWasRemoveAction) )
		{
			GraphPanel->PurgeVisualRepresentation();
			// Trigger the refresh
			bIsActiveTimerRegistered = true;
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SGraphEditorImpl::TriggerRefresh));
		}
	}
}

//void SGraphEditorImpl::GraphEd_OnPanelUpdated()
//{
//	
//}

const TSet<class UObject*>& SGraphEditorImpl::GetSelectedNodes() const
{
	return GraphPanel->SelectionManager.GetSelectedNodes();
}

void SGraphEditorImpl::ClearSelectionSet()
{
	GraphPanel->SelectionManager.ClearSelectionSet();
}

void SGraphEditorImpl::SetNodeSelection(UEdGraphNode* Node, bool bSelect)
{
	GraphPanel->SelectionManager.SetNodeSelection(Node, bSelect);
}

void SGraphEditorImpl::SelectAllNodes()
{
	FGraphPanelSelectionSet NewSet;
	for (int32 NodeIndex = 0; NodeIndex < EdGraphObj->Nodes.Num(); ++NodeIndex)
	{
		UEdGraphNode* Node = EdGraphObj->Nodes[NodeIndex];
		if (Node)
		{
			ensureMsgf(Node->IsValidLowLevel(), TEXT("Node is invalid"));
			NewSet.Add(Node);
		}
	}
	GraphPanel->SelectionManager.SetSelectionSet(NewSet);
}

UEdGraphPin* SGraphEditorImpl::GetGraphPinForMenu()
{
	return GraphPinForMenu.Get();
}

UEdGraphNode* SGraphEditorImpl::GetGraphNodeForMenu()
{
	return GraphNodeForMenu.IsValid() ? GraphNodeForMenu.Get() : nullptr;
}

void SGraphEditorImpl::ZoomToFit(bool bOnlySelection)
{
	GraphPanel->ZoomToFit(bOnlySelection);
}
bool SGraphEditorImpl::GetBoundsForSelectedNodes( class FSlateRect& Rect, float Padding )
{
	return GraphPanel->GetBoundsForSelectedNodes(Rect, Padding);
}

bool SGraphEditorImpl::GetBoundsForNode( const UEdGraphNode* InNode, class FSlateRect& Rect, float Padding) const
{
	FVector2D TopLeft, BottomRight;
	if (GraphPanel->GetBoundsForNode(InNode, TopLeft, BottomRight, Padding))
	{
		Rect.Left = TopLeft.X;
		Rect.Top = TopLeft.Y;
		Rect.Bottom = BottomRight.Y;
		Rect.Right = BottomRight.X;
		return true;
	}
	return false;
}

void SGraphEditorImpl::StraightenConnections()
{
	GraphPanel->StraightenConnections();
}

void SGraphEditorImpl::StraightenConnections(UEdGraphPin* SourcePin, UEdGraphPin* PinToAlign)
{
	GraphPanel->StraightenConnections(SourcePin, PinToAlign);
}

void SGraphEditorImpl::Construct( const FArguments& InArgs )
{
	Commands = MakeShareable( new FUICommandList() );
	IsEditable = InArgs._IsEditable;
	DisplayAsReadOnly = InArgs._DisplayAsReadOnly;
	Appearance = InArgs._Appearance;
	TitleBar	= InArgs._TitleBar;
	bAutoExpandActionMenu = InArgs._AutoExpandActionMenu;
	ShowGraphStateOverlay = InArgs._ShowGraphStateOverlay;

	OnNavigateHistoryBack = InArgs._OnNavigateHistoryBack;
	OnNavigateHistoryForward = InArgs._OnNavigateHistoryForward;
	OnNodeSpawnedByKeymap = InArgs._GraphEvents.OnNodeSpawnedByKeymap;

	bIsActiveTimerRegistered = false;

	// Make sure that the editor knows about what kinds
	// of commands GraphEditor can do.
	FGraphEditorCommands::Register();

	// Tell GraphEditor how to handle all the known commands
	{
		Commands->MapAction( FGraphEditorCommands::Get().ReconstructNodes,
			FExecuteAction::CreateSP( this, &SGraphEditorImpl::ReconstructNodes ),
			FCanExecuteAction::CreateSP( this, &SGraphEditorImpl::CanReconstructNodes )
		);

		Commands->MapAction( FGraphEditorCommands::Get().BreakNodeLinks,
			FExecuteAction::CreateSP( this, &SGraphEditorImpl::BreakNodeLinks ),
			FCanExecuteAction::CreateSP( this, &SGraphEditorImpl::CanBreakNodeLinks )
		);

		Commands->MapAction( FGraphEditorCommands::Get().BreakPinLinks,
			FExecuteAction::CreateSP( this, &SGraphEditorImpl::BreakPinLinks, true),
			FCanExecuteAction::CreateSP( this, &SGraphEditorImpl::CanBreakPinLinks )
		);

		// Append any additional commands that a consumer of GraphEditor wants us to be aware of.
		const TSharedPtr<FUICommandList>& AdditionalCommands = InArgs._AdditionalCommands;
		if ( AdditionalCommands.IsValid() )
		{
			Commands->Append( AdditionalCommands.ToSharedRef() );
		}

	}

	bResetMenuContext = false;
	GraphPinForMenu.SetPin(nullptr);
	EdGraphObj = InArgs._GraphToEdit;

	OnFocused = InArgs._GraphEvents.OnFocused;
	OnCreateActionMenu = InArgs._GraphEvents.OnCreateActionMenu;
	
	struct Local
	{
		static FText GetPIENotifyText(TAttribute<FGraphAppearanceInfo> Appearance, FText DefaultText)
		{
			FText OverrideText = Appearance.Get().PIENotifyText;
			return !OverrideText.IsEmpty() ? OverrideText : DefaultText;
		}

		static FText GetReadOnlyText(TAttribute<FGraphAppearanceInfo>Appearance, FText DefaultText)
		{
			FText OverrideText = Appearance.Get().ReadOnlyText;
			return !OverrideText.IsEmpty() ? OverrideText : DefaultText;
		}
	};
	
	FText DefaultPIENotify(LOCTEXT("GraphSimulatingText", "SIMULATING"));
	TAttribute<FText> PIENotifyText = Appearance.IsBound() ?
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&Local::GetPIENotifyText, Appearance, DefaultPIENotify)) :
		TAttribute<FText>(DefaultPIENotify);

	FText DefaultReadOnlyText(LOCTEXT("GraphReadOnlyText", "READ-ONLY"));
	TAttribute<FText> ReadOnlyText = Appearance.IsBound() ?
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&Local::GetReadOnlyText, Appearance, DefaultReadOnlyText)) :
		TAttribute<FText>(DefaultReadOnlyText);

	TSharedPtr<SOverlay> OverlayWidget;

	this->ChildSlot
	[
		SAssignNew(OverlayWidget, SOverlay)

		// The graph panel
		+SOverlay::Slot()
		.Expose(GraphPanelSlot)
		[
			SAssignNew(GraphPanel, SGraphPanel)
			.GraphObj( EdGraphObj )
			.GraphObjToDiff( InArgs._GraphToDiff)
			.OnGetContextMenuFor( this, &SGraphEditorImpl::GraphEd_OnGetContextMenuFor )
			.OnSelectionChanged( InArgs._GraphEvents.OnSelectionChanged )
			.OnNodeDoubleClicked( InArgs._GraphEvents.OnNodeDoubleClicked )
			.IsEditable( this, &SGraphEditorImpl::IsGraphEditable )
			.DisplayAsReadOnly( this, &SGraphEditorImpl::DisplayGraphAsReadOnly )
			.OnDropActor( InArgs._GraphEvents.OnDropActor )
			.OnDropStreamingLevel( InArgs._GraphEvents.OnDropStreamingLevel )
			.OnVerifyTextCommit( InArgs._GraphEvents.OnVerifyTextCommit )
			.OnTextCommitted( InArgs._GraphEvents.OnTextCommitted )
			.OnSpawnNodeByShortcut( InArgs._GraphEvents.OnSpawnNodeByShortcut )
			//.OnUpdateGraphPanel( this, &SGraphEditorImpl::GraphEd_OnPanelUpdated )
			.OnDisallowedPinConnection( InArgs._GraphEvents.OnDisallowedPinConnection )
			.ShowGraphStateOverlay(InArgs._ShowGraphStateOverlay)
		]

		// Indicator of current zoom level
		+SOverlay::Slot()
		.Padding(5)
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), "Graph.ZoomText" )
			.Text( this, &SGraphEditorImpl::GetZoomText )
			.ColorAndOpacity( this, &SGraphEditorImpl::GetZoomTextColorAndOpacity )
		]

		// Title bar - optional
		+SOverlay::Slot()
		.VAlign(VAlign_Top)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				InArgs._TitleBar.IsValid() ? InArgs._TitleBar.ToSharedRef() : (TSharedRef<SWidget>)SNullWidget::NullWidget
			]
			+ SVerticalBox::Slot()
			.Padding(20.f, 20.f, 20.f, 0.f)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(FMargin(10.f, 4.f))
				.BorderImage(FEditorStyle::GetBrush(TEXT("Graph.InstructionBackground")))
				.BorderBackgroundColor(this, &SGraphEditorImpl::InstructionBorderColor)
				.HAlign(HAlign_Center)
				.ColorAndOpacity(this, &SGraphEditorImpl::InstructionTextTint)
				.Visibility(this, &SGraphEditorImpl::InstructionTextVisibility)
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "Graph.InstructionText")
					.Text(this, &SGraphEditorImpl::GetInstructionText)
				]
			]			
		]

		// Bottom-right corner text indicating the type of tool
		+SOverlay::Slot()
		.Padding(10)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Visibility( EVisibility::HitTestInvisible )
			.TextStyle( FEditorStyle::Get(), "Graph.CornerText" )
			.Text(Appearance.Get().CornerText)
		]

		// Top-right corner text indicating PIE is active
		+SOverlay::Slot()
		.Padding(20)
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Visibility(this, &SGraphEditorImpl::PIENotification)
			.TextStyle( FEditorStyle::Get(), "Graph.SimulatingText" )
			.Text( PIENotifyText )
		]

		// Top-right corner text indicating read only when not simulating
		+ SOverlay::Slot()
		.Padding(20)
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Visibility(this, &SGraphEditorImpl::ReadOnlyVisibility)
			.TextStyle(FEditorStyle::Get(), "Graph.CornerText")
			.Text(ReadOnlyText)
		]

// 		+ SOverlay::Slot()
// 		.Padding(20)
// 		.VAlign(VAlign_Fill)
// 		.HAlign(HAlign_Fill)
// 		[
// 			SNew(SVerticalBox)
// 			+ SVerticalBox::Slot()
// 			.FillHeight(0.5)
// 			.VAlign(VAlign_Bottom)
// 			.HAlign(HAlign_Center)
// 			[
// 				
// 			]
// 			+ SVerticalBox::Slot()
// 			.FillHeight(0.5)
// 		]

		// Bottom-right corner text for notification list position
		+SOverlay::Slot()
		.Padding(15.f)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		[
			SAssignNew(NotificationListPtr, SNotificationList)
			.Visibility(EVisibility::HitTestInvisible)
		]
	];

	GraphPanel->RestoreViewSettings(FVector2D::ZeroVector, -1);

	NotifyGraphChanged();
}

EVisibility SGraphEditorImpl::PIENotification( ) const
{
	if(ShowGraphStateOverlay.Get() && (GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != NULL))
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}
	
SGraphEditorImpl::~SGraphEditorImpl()
{
}

void SGraphEditorImpl::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bResetMenuContext)
	{
		GraphPinForMenu.SetPin(nullptr);
		GraphNodeForMenu.Reset();
		bResetMenuContext = false;
	}

	// If locked to another graph editor, and our panel has moved, synchronise the locked graph editor accordingly
	if ((EdGraphObj != NULL) && GraphPanel.IsValid())
	{
		if(GraphPanel->HasMoved() && IsLocked())
		{
			FocusLockedEditorHere();
		}
	}
}

EActiveTimerReturnType SGraphEditorImpl::TriggerRefresh( double InCurrentTime, float InDeltaTime )
{
	GraphPanel->Update();

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SGraphEditorImpl::OnClosedActionMenu()
{
	GraphPanel->OnStopMakingConnection(/*bForceStop=*/ true);
}

FActionMenuContent SGraphEditorImpl::GraphEd_OnGetContextMenuFor(const FGraphContextMenuArguments& SpawnInfo)
{
	FActionMenuContent Result;

	if (EdGraphObj != NULL)
	{
		Result = FActionMenuContent( SNew(STextBlock) .Text( NSLOCTEXT("GraphEditor", "NoNodes", "No Nodes") ) );

		const UEdGraphSchema* Schema = EdGraphObj->GetSchema();
		check(Schema);
			
		// Cache the pin this menu is being brought up for
		GraphPinForMenu.SetPin(SpawnInfo.GraphPin);
		GraphNodeForMenu = SpawnInfo.GraphNode;
		
		if ((SpawnInfo.GraphPin != NULL) || (SpawnInfo.GraphNode != NULL))
		{
			// Get all menu extenders for this context menu from the graph editor module
			FGraphEditorModule& GraphEditorModule = FModuleManager::GetModuleChecked<FGraphEditorModule>( TEXT("GraphEditor") );
			TArray<FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode> MenuExtenderDelegates = GraphEditorModule.GetAllGraphEditorContextMenuExtender();

			TArray<TSharedPtr<FExtender>> Extenders;
			for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
			{
				if (MenuExtenderDelegates[i].IsBound())
				{
					Extenders.Add(MenuExtenderDelegates[i].Execute(this->Commands.ToSharedRef(), EdGraphObj, SpawnInfo.GraphNode, SpawnInfo.GraphPin, !IsEditable.Get()));
				}
			}
			TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

			// Show the menu for the pin or node under the cursor
			const bool bShouldCloseAfterAction = true;
			FMenuBuilder MenuBuilder( bShouldCloseAfterAction, this->Commands, MenuExtender );
			Schema->GetContextMenuActions(EdGraphObj, SpawnInfo.GraphNode, SpawnInfo.GraphPin, &MenuBuilder, !IsEditable.Get());

			Result = FActionMenuContent(MenuBuilder.MakeWidget());
		}
		else if (IsEditable.Get())
		{
			if (EdGraphObj->GetSchema() != NULL )
			{
				if(OnCreateActionMenu.IsBound())
				{
					Result = OnCreateActionMenu.Execute(
						EdGraphObj, 
						SpawnInfo.NodeAddPosition,
						SpawnInfo.DragFromPins,
						bAutoExpandActionMenu, 
						SGraphEditor::FActionMenuClosed::CreateSP(this, &SGraphEditorImpl::OnClosedActionMenu)
						);
				}
				else
				{
					TSharedRef<SGraphEditorActionMenu> Menu =	
						SNew(SGraphEditorActionMenu)
						.GraphObj( EdGraphObj )
						.NewNodePosition(SpawnInfo.NodeAddPosition)
						.DraggedFromPins(SpawnInfo.DragFromPins)
						.AutoExpandActionMenu(bAutoExpandActionMenu)
						.OnClosedCallback( SGraphEditor::FActionMenuClosed::CreateSP(this, &SGraphEditorImpl::OnClosedActionMenu)
						);

					Result = FActionMenuContent( Menu, Menu->GetFilterTextBox() );
				}

				if (SpawnInfo.DragFromPins.Num() > 0)
				{
					GraphPanel->PreservePinPreviewUntilForced();
				}
			}
		}
		else
		{
			Result = FActionMenuContent( SNew(STextBlock)  .Text( NSLOCTEXT("GraphEditor", "CannotCreateWhileDebugging", "Cannot create new nodes in a read only graph") ) );
		}
	}
	else
	{
		Result = FActionMenuContent( SNew(STextBlock) .Text( NSLOCTEXT("GraphEditor", "GraphObjectIsNull", "Graph Object is Null") ) );
	}

	Result.OnMenuDismissed.AddLambda([this]()
	{
		bResetMenuContext = true;
	});

	return Result;
}

bool SGraphEditorImpl::CanReconstructNodes() const
{
	return IsGraphEditable() && (GraphPanel->SelectionManager.AreAnyNodesSelected());
}

bool SGraphEditorImpl::CanBreakNodeLinks() const
{
	return IsGraphEditable() && (GraphPanel->SelectionManager.AreAnyNodesSelected());
}

bool SGraphEditorImpl::CanBreakPinLinks() const
{
	return IsGraphEditable() && (GraphPinForMenu.Get() != nullptr);
}

void SGraphEditorImpl::ReconstructNodes()
{
	const UEdGraphSchema* Schema = this->EdGraphObj->GetSchema();
	{
		FScopedTransaction const Transaction(LOCTEXT("ReconstructNodeTransaction", "Refresh Node(s)"));

		for (FGraphPanelSelectionSet::TConstIterator NodeIt( GraphPanel->SelectionManager.GetSelectedNodes() ); NodeIt; ++NodeIt)
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
			{
				TGuardValue<ESaveOrphanPinMode> GuardSaveMode(Node->OrphanedPinSaveMode, ESaveOrphanPinMode::SaveNone);
				Schema->ReconstructNode(*Node);
				Node->ClearCompilerMessage();
			}
		}
	}
	NotifyGraphChanged();
}
	
void SGraphEditorImpl::BreakNodeLinks()
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakNodeLinks", "Break Node Links") );

	for (FGraphPanelSelectionSet::TConstIterator NodeIt( GraphPanel->SelectionManager.GetSelectedNodes() ); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			const UEdGraphSchema* Schema = Node->GetSchema();
			Schema->BreakNodeLinks(*Node);
		}
	}
}
	
void SGraphEditorImpl::BreakPinLinks(bool bSendNodeNotification)
{
	if (UEdGraphPin* PinContext = GraphPinForMenu.Get())
	{
		const UEdGraphSchema* Schema = PinContext->GetSchema();
		Schema->BreakPinLinks(*PinContext, bSendNodeNotification);
	}
}

FText SGraphEditorImpl::GetZoomText() const
{
	return GraphPanel->GetZoomText();
}

FSlateColor SGraphEditorImpl::GetZoomTextColorAndOpacity() const
{
	return GraphPanel->GetZoomTextColorAndOpacity();
}

bool SGraphEditorImpl::IsGraphEditable() const
{
	return (EdGraphObj != NULL) && IsEditable.Get();
}

bool SGraphEditorImpl::DisplayGraphAsReadOnly() const
{
	return (EdGraphObj != NULL) && DisplayAsReadOnly.Get();
}

bool SGraphEditorImpl::IsLocked() const
{
	for( auto LockedGraph : LockedGraphs )
	{
		if( LockedGraph.IsValid() )
		{
			return true;
		}
	}
	return false;
}

TSharedPtr<SWidget> SGraphEditorImpl::GetTitleBar() const
{
	return TitleBar;
}

void SGraphEditorImpl::SetViewLocation( const FVector2D& Location, float ZoomAmount ) 
{
	if( GraphPanel.IsValid() &&  EdGraphObj && (!IsLocked() || !GraphPanel->HasDeferredObjectFocus()))
	{
		GraphPanel->RestoreViewSettings(Location, ZoomAmount);
	}
}

void SGraphEditorImpl::GetViewLocation( FVector2D& Location, float& ZoomAmount ) 
{
	if( GraphPanel.IsValid() &&  EdGraphObj && (!IsLocked() || !GraphPanel->HasDeferredObjectFocus()))
	{
		Location = GraphPanel->GetViewOffset();
		ZoomAmount = GraphPanel->GetZoomAmount();
	}
}

void SGraphEditorImpl::LockToGraphEditor( TWeakPtr<SGraphEditor> Other ) 
{
	if( !LockedGraphs.Contains(Other) )
	{
		LockedGraphs.Push(Other);
	}

	if (GraphPanel.IsValid())
	{
		FocusLockedEditorHere();
	}
}

void SGraphEditorImpl::UnlockFromGraphEditor( TWeakPtr<SGraphEditor> Other )
{
	check(Other.IsValid());
	int idx = LockedGraphs.Find(Other);
	if( ensureMsgf(idx != INDEX_NONE, TEXT("Attempted to unlock graphs that were not locked together: %s %s"), *GetReadableLocation(), *(Other.Pin()->GetReadableLocation()) ) )
	{
		LockedGraphs.RemoveAtSwap(idx);
	}
}

void SGraphEditorImpl::AddNotification( FNotificationInfo& Info, bool bSuccess )
{
	// set up common notification properties
	Info.bUseLargeFont = true;

	TSharedPtr<SNotificationItem> Notification = NotificationListPtr->AddNotification(Info);
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail );
	}
}

void SGraphEditorImpl::FocusLockedEditorHere()
{
	for( int i = 0; i < LockedGraphs.Num(); ++i )
	{
		TSharedPtr<SGraphEditor> LockedGraph = LockedGraphs[i].Pin();
		if (LockedGraph != TSharedPtr<SGraphEditor>())
		{
			LockedGraph->SetViewLocation(GraphPanel->GetViewOffset(), GraphPanel->GetZoomAmount());
		}
		else
		{
			LockedGraphs.RemoveAtSwap(i--);
		}
	}
}

void SGraphEditorImpl::SetPinVisibility( SGraphEditor::EPinVisibility InVisibility ) 
{
	if( GraphPanel.IsValid())
	{
		SGraphEditor::EPinVisibility CachedVisibility = GraphPanel->GetPinVisibility();
		GraphPanel->SetPinVisibility(InVisibility);
		if(CachedVisibility != InVisibility)
		{
			NotifyGraphChanged();
		}
	}
}

TSharedRef<FActiveTimerHandle> SGraphEditorImpl::RegisterActiveTimer(float TickPeriod, FWidgetActiveTimerDelegate TickFunction)
{
	return SWidget::RegisterActiveTimer(TickPeriod, TickFunction);
}

EVisibility SGraphEditorImpl::ReadOnlyVisibility() const
{
	if(ShowGraphStateOverlay.Get() && PIENotification() == EVisibility::Hidden && !IsEditable.Get())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

FText SGraphEditorImpl::GetInstructionText() const
{
	if (Appearance.IsBound())
	{
		return Appearance.Get().InstructionText;
	}
	return FText::GetEmpty();
}

EVisibility SGraphEditorImpl::InstructionTextVisibility() const
{
	if (!GetInstructionText().IsEmpty() && (GetInstructionTextFade() > 0.0f))
	{
		return EVisibility::HitTestInvisible;
	}
	return EVisibility::Hidden;
}

float SGraphEditorImpl::GetInstructionTextFade() const
{
	float InstructionOpacity = 1.0f;
	if (Appearance.IsBound())
	{
		InstructionOpacity = Appearance.Get().InstructionFade.Get();
	}
	return InstructionOpacity;
}

FLinearColor SGraphEditorImpl::InstructionTextTint() const
{
	return FLinearColor(1.f, 1.f, 1.f, GetInstructionTextFade());
}

FSlateColor SGraphEditorImpl::InstructionBorderColor() const
{
	FLinearColor BorderColor(0.1f, 0.1f, 0.1f, 0.7f);
	BorderColor.A *= GetInstructionTextFade();
	return BorderColor;
}

void SGraphEditorImpl::CaptureKeyboard()
{
	FSlateApplication::Get().SetKeyboardFocus(GraphPanel);
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE 
