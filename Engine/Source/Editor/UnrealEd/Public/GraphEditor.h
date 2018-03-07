// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "GameFramework/Actor.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "BlueprintUtilities.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandList.h"
#include "Engine/LevelStreaming.h"

class UEdGraph;
struct FNotificationInfo;
struct Rect;

DECLARE_DELEGATE_ThreeParams( FOnNodeTextCommitted, const FText&, ETextCommit::Type, UEdGraphNode* );
DECLARE_DELEGATE_RetVal_ThreeParams( bool, FOnNodeVerifyTextCommit, const FText&, UEdGraphNode*, FText& );
DECLARE_MULTICAST_DELEGATE(FOnGraphContentMenuDismissed);

typedef TSet<class UObject*> FGraphPanelSelectionSet;

/** Info about how to draw the graph */
struct FGraphAppearanceInfo
{
	FGraphAppearanceInfo()
		: CornerImage(NULL)
		, InstructionFade(1.f)
	{
	}

	/** Image to draw in corner of graph */
	const FSlateBrush* CornerImage;
	/** Text to write in corner of graph */
	FText CornerText;
	/** If set, will be used as override for PIE notify text */
	FText PIENotifyText;
	/** If set, will be used as override for read only text */
	FText ReadOnlyText;
	/** Text to display if the graph is empty (to guide the user on what to do) */
	FText InstructionText;
	/** Allows graphs to nicely fade instruction text (or completely hide it). */
	TAttribute<float> InstructionFade;
};

/** Struct used to return info about action menu */
struct FActionMenuContent
{
	explicit FActionMenuContent( TSharedRef<SWidget> InContent, TSharedPtr<SWidget> InWidgetToFocus = TSharedPtr<SWidget>() )
		: Content( InContent )
		, WidgetToFocus( InWidgetToFocus )
	{
	}

	FActionMenuContent()
	: Content(SNullWidget::NullWidget)
	{	
	}

	TSharedRef<SWidget> Content;
	TSharedPtr<SWidget> WidgetToFocus;
	FOnGraphContentMenuDismissed OnMenuDismissed;
};

/**
 * Interface and wrapper for GraphEditor widgets.
 * Gracefully handles the GraphEditorModule being unloaded.
 */
class SGraphEditor : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam( FOnSelectionChanged, const FGraphPanelSelectionSet& )

	DECLARE_DELEGATE_OneParam( FOnFocused, const TSharedRef<SGraphEditor>& );

	DECLARE_DELEGATE_ThreeParams( FOnDropActor, const TArray< TWeakObjectPtr<class AActor> >&, class UEdGraph*, const FVector2D& );
	
	DECLARE_DELEGATE_ThreeParams( FOnDropStreamingLevel, const TArray< TWeakObjectPtr<class ULevelStreaming> >&, class UEdGraph*, const FVector2D& );

	DECLARE_DELEGATE( FActionMenuClosed );

	DECLARE_DELEGATE_RetVal_FiveParams( FActionMenuContent, FOnCreateActionMenu, UEdGraph*, const FVector2D&, const TArray<UEdGraphPin*>&, bool, FActionMenuClosed );

	DECLARE_DELEGATE_RetVal_TwoParams( FReply, FOnSpawnNodeByShortcut, FInputChord, const FVector2D& );

	DECLARE_DELEGATE( FOnNodeSpawnedByKeymap );

	DECLARE_DELEGATE_TwoParams( FOnDisallowedPinConnection, const UEdGraphPin*, const UEdGraphPin* );

	/** Info about events occurring in/on the graph */
	struct FGraphEditorEvents
	{
		/** Called when selection changes */
		FOnSelectionChanged OnSelectionChanged;
		/** Called when a node is double clicked */
		FSingleNodeEvent OnNodeDoubleClicked;
		/* Called when focus moves to graph */
		FOnFocused OnFocused;
		/* Called when an actor is dropped on graph */
		FOnDropActor OnDropActor;
		/* Called when a streaming level is dropped on graph */
		FOnDropStreamingLevel OnDropStreamingLevel;
		/** Called when text is being committed on the graph to verify */
		FOnNodeVerifyTextCommit OnVerifyTextCommit;
		/** Called when text is committed on the graph */
		FOnNodeTextCommitted OnTextCommitted;
		/** Called to create context menu */
		FOnCreateActionMenu OnCreateActionMenu;
		/** Called to spawn a node in the graph using a shortcut */
		FOnSpawnNodeByShortcut OnSpawnNodeByShortcut;
		/** Called when a keymap spawns a node */
		FOnNodeSpawnedByKeymap OnNodeSpawnedByKeymap;
		/** Called when the user generates a warning tooltip because a connection was invalid */
		FOnDisallowedPinConnection OnDisallowedPinConnection;
	};


	SLATE_BEGIN_ARGS(SGraphEditor)
		: _AdditionalCommands( static_cast<FUICommandList*>(NULL) )
		, _IsEditable(true)
		, _DisplayAsReadOnly(false)
		, _IsEmpty(false)
		, _GraphToEdit(NULL)
		, _GraphToDiff(NULL)
		, _AutoExpandActionMenu(false)
		, _ShowGraphStateOverlay(true)
		{}

		SLATE_ARGUMENT( TSharedPtr<FUICommandList>, AdditionalCommands )
		SLATE_ATTRIBUTE( bool, IsEditable )		
		SLATE_ATTRIBUTE( bool, DisplayAsReadOnly )		
		SLATE_ATTRIBUTE( bool, IsEmpty )	
		SLATE_ARGUMENT( TSharedPtr<SWidget>, TitleBar )
		SLATE_ATTRIBUTE( FGraphAppearanceInfo, Appearance )
		SLATE_EVENT( FEdGraphEvent, OnGraphModuleReloaded )
		SLATE_ARGUMENT( UEdGraph*, GraphToEdit )
		SLATE_ARGUMENT( UEdGraph*, GraphToDiff )
		SLATE_ARGUMENT( FGraphEditorEvents, GraphEvents)
		SLATE_ARGUMENT( bool, AutoExpandActionMenu )
		SLATE_EVENT(FSimpleDelegate, OnNavigateHistoryBack)
		SLATE_EVENT(FSimpleDelegate, OnNavigateHistoryForward)

		/** Show overlay elements for the graph state such as the PIE and read-only borders and text */
		SLATE_ATTRIBUTE(bool, ShowGraphStateOverlay)				
	SLATE_END_ARGS()

	/**
	 * Loads the GraphEditorModule and constructs a GraphEditor as a child of this widget.
	 *
	 * @param InArgs   Declaration params from which to construct the widget.
	 */
	UNREALED_API void Construct( const FArguments& InArgs );

	/** @return The current graph being edited */
	UEdGraph* GetCurrentGraph() const
	{
		return EdGraphObj;
	}

	virtual FVector2D GetPasteLocation() const
	{
		if (Implementation.IsValid())
		{
			return Implementation->GetPasteLocation();
		}
		else
		{
			return FVector2D::ZeroVector;
		}
	}

	/* Set new viewer location */
	virtual void SetViewLocation(const FVector2D& Location, float ZoomAmount)
	{
		if (Implementation.IsValid())
		{
			Implementation->SetViewLocation(Location, ZoomAmount);
		}
	}

	/**
	 * Gets the view location of the graph
	 *
	 * @param OutLocation		Will have the current view location
	 * @param OutZoomAmount		Will have the current zoom amount
	 */
	virtual void GetViewLocation(FVector2D& OutLocation, float& OutZoomAmount)
	{
		if (Implementation.IsValid())
		{
			Implementation->GetViewLocation(OutLocation, OutZoomAmount);
		}
	}

	/** Check if node title is visible with optional flag to ensure it is */
	virtual bool IsNodeTitleVisible(const class UEdGraphNode* Node, bool bRequestRename)
	{
		bool bResult = false;
		if (Implementation.IsValid())
		{
			bResult = Implementation->IsNodeTitleVisible(Node, bRequestRename);
		}
		return bResult;
	}

	/* Lock two graph editors together */
	virtual void LockToGraphEditor(TWeakPtr<SGraphEditor> Other)
	{
		if (Implementation.IsValid())
		{
			Implementation->LockToGraphEditor(Other);
		}
	}

	/* Unlock two graph editors from each other */
	virtual void UnlockFromGraphEditor(TWeakPtr<SGraphEditor> Other)
	{
		if (Implementation.IsValid())
		{
			Implementation->UnlockFromGraphEditor(Other);
		}
	}

	/** Bring the specified node into view */
	virtual void JumpToNode( const class UEdGraphNode* JumpToMe, bool bRequestRename = false, bool bSelectNode = true )
	{
		if (Implementation.IsValid())
		{
			Implementation->JumpToNode(JumpToMe, bRequestRename, bSelectNode);
		}
	}

	/** Bring the specified pin into view */
	virtual void JumpToPin( const class UEdGraphPin* JumpToMe )
	{
		if (Implementation.IsValid())
		{
			Implementation->JumpToPin(JumpToMe);
		}
	}

	/** Pin visibility modes */
	enum EPinVisibility
	{
		Pin_Show,
		Pin_HideNoConnection,
		Pin_HideNoConnectionNoDefault
	};

	/*Set the pin visibility mode*/
	virtual void SetPinVisibility(EPinVisibility InVisibility)
	{
		if (Implementation.IsValid())
		{
			Implementation->SetPinVisibility(InVisibility);
		}
	}

	/** Register an active timer on the graph editor. */
	virtual TSharedRef<FActiveTimerHandle> RegisterActiveTimer(float TickPeriod, FWidgetActiveTimerDelegate TickFunction)
	{
		if (Implementation.IsValid())
		{
			return Implementation->RegisterActiveTimer(TickPeriod, TickFunction);
		}
		return TSharedPtr<FActiveTimerHandle>().ToSharedRef();
	}

	/** @return a reference to the list of selected graph nodes */
	virtual const TSet<class UObject*>& GetSelectedNodes() const
	{
		static TSet<class UObject*> NoSelection;

		if (Implementation.IsValid())
		{
			return Implementation->GetSelectedNodes();
		}
		else
		{
			return NoSelection;
		}
	}

	/** Clear the selection */
	virtual void ClearSelectionSet()
	{
		if (Implementation.IsValid())
		{
			Implementation->ClearSelectionSet();
		}
	}

	/** Set the selection status of a node */
	virtual void SetNodeSelection(UEdGraphNode* Node, bool bSelect)
	{
		if (Implementation.IsValid())
		{
			Implementation->SetNodeSelection(Node, bSelect);
		}
	}
	
	/** Select all nodes */
	virtual void SelectAllNodes()
	{
		if (Implementation.IsValid())
		{
			Implementation->SelectAllNodes();
		}		
	}

	virtual class UEdGraphPin* GetGraphPinForMenu()
	{
		if ( Implementation.IsValid() )
		{
			return Implementation->GetGraphPinForMenu();
		}
		else
		{
			return NULL;
		}
	}

	virtual class UEdGraphNode* GetGraphNodeForMenu()
	{
		if ( Implementation.IsValid() )
		{
			return Implementation->GetGraphNodeForMenu();
		}
		else
		{
			return NULL;
		}
	}

	// Zooms out to fit either all nodes or only the selected ones
	virtual void ZoomToFit(bool bOnlySelection)
	{
		if (Implementation.IsValid())
		{
			return Implementation->ZoomToFit(bOnlySelection);
		}
	}

	/** Get Bounds for selected nodes, false if nothing selected*/
	virtual bool GetBoundsForSelectedNodes( class FSlateRect& Rect, float Padding  )
	{
		if (Implementation.IsValid())
		{
			return Implementation->GetBoundsForSelectedNodes(Rect, Padding);
		}
		return false;
	}

	/** Get Bounds for the specified node, returns false on failure */
	virtual bool GetBoundsForNode( const UEdGraphNode* InNode, class FSlateRect& Rect, float Padding ) const
	{
		if (Implementation.IsValid())
		{
			return Implementation->GetBoundsForNode(InNode, Rect, Padding);
		}
		return false;
	}

	virtual void StraightenConnections()
	{
		if (Implementation.IsValid())
		{
			return Implementation->StraightenConnections();
		}
	}

	virtual void StraightenConnections(UEdGraphPin* SourcePin, UEdGraphPin* PinToAlign = nullptr)
	{
		if (Implementation.IsValid())
		{
			return Implementation->StraightenConnections(SourcePin, PinToAlign);
		}
	}

	// Invoked to let this widget know that the GraphEditor module has been reloaded
	UNREALED_API void OnModuleReloaded();

	// Invoked to let this widget know that the GraphEditor module is being unloaded.
	UNREALED_API void OnModuleUnloading();

	UNREALED_API void NotifyPrePropertyChange(const FString& PropertyName);
	UNREALED_API void NotifyPostPropertyChange(const FPropertyChangedEvent& PropertyChangeEvent, const FString& PropertyName);

	/** Invoked when the Graph being edited changes in some way. */
	virtual void NotifyGraphChanged()
	{
		if (Implementation.IsValid())
		{
			Implementation->NotifyGraphChanged();
		}
	}

	/* Get the title bar if there is one */
	virtual TSharedPtr<SWidget> GetTitleBar() const
	{
		if (Implementation.IsValid())
		{
			return Implementation->GetTitleBar();
		}
		return TSharedPtr<SWidget>();
	}

	/** Show notification on graph */
	virtual void AddNotification(FNotificationInfo& Info, bool bSuccess)
	{
		if (Implementation.IsValid())
		{
			Implementation->AddNotification(Info, bSuccess);
		}
	}

	/** Capture keyboard */
	virtual void CaptureKeyboard()
	{
		if (Implementation.IsValid())
		{
			Implementation->CaptureKeyboard();
		}
	}

protected:
	/** Invoked when the underlying Graph is being changed. */
	virtual void OnGraphChanged(const struct FEdGraphEditAction& InAction)
	{
		if (Implementation.IsValid())
		{
			Implementation->OnGraphChanged(InAction);
		}
	}

private:
	static void RegisterGraphEditor(const TSharedRef<SGraphEditor>& InGraphEditor);

	void ConstructImplementation( const FArguments& InArgs );
protected:
	/** The Graph we are currently editing */
	UEdGraph* EdGraphObj;
private:
	/** The actual implementation of the GraphEditor */
	TSharedPtr<SGraphEditor> Implementation;

	/** Active GraphEditor wrappers; we will notify these about the module being unloaded so they can handle it gracefully. */
	UNREALED_API static TArray< TWeakPtr<SGraphEditor> > AllInstances;

	// This callback is triggered whenever the graph module is reloaded
	FEdGraphEvent OnGraphModuleReloadedCallback;

	// The graph editor module needs to access AllInstances, but no-one else should be able to
	friend class FGraphEditorModule;
};
