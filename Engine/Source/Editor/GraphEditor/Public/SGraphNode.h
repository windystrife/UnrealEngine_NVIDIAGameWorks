// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Layout/SlateRect.h"
#include "Input/Reply.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Animation/CurveHandle.h"
#include "Animation/CurveSequence.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "GraphEditor.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "SNodePanel.h"
#include "Widgets/Notifications/SErrorText.h"

class FActorDragDropOp;
class IToolTip;
class SGraphPanel;
class SGraphPin;
class SToolTip;
class SVerticalBox;

/////////////////////////////////////////////////////
// SNodeTitle

class GRAPHEDITOR_API SNodeTitle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNodeTitle)
		: _Style(TEXT("Graph.Node.NodeTitle"))
		, _ExtraLineStyle(TEXT("Graph.Node.NodeTitleExtraLines"))
		{}

		// The style of the text block, which dictates the font, color, and shadow options. Style overrides all other properties!
		SLATE_ARGUMENT(FName, Style)

		// The style of any additional lines in the the text block
		SLATE_ARGUMENT(FName, ExtraLineStyle)

		// Title text to display, auto-binds to get the title if not set externally
		SLATE_ATTRIBUTE(FText, Text)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode);

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

	/** Returns the main title for the node */
	FText GetHeadTitle() const;

	/** Get the size of this title the last time it was drawn */
	FVector2D GetTitleSize() const;

protected:
	UEdGraphNode* GraphNode;
	FNodeTextCache NodeTitleCache;
	FName ExtraLineStyle;

	/** The cached head title to return */
	FText CachedHeadTitle;

	/** The title text to use, auto-binds to get the title if not set externally */
	TAttribute< FText > TitleText;

	/** The cached size of the title */
	FVector2D CachedSize;

protected:
	// Gets the expected node title
	FText GetNodeTitle() const;

	// Rebuilds the widget if needed
	void RebuildWidget();
};

/////////////////////////////////////////////////////
// SGraphNode

class GRAPHEDITOR_API SGraphNode : public SNodePanel::SNode
{
public:
	// SWidget interface
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;

	virtual TSharedPtr<IToolTip> GetToolTip() override;
	virtual void OnToolTipClosing() override;

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

	// SNodePanel::SNode interface
	virtual void MoveTo( const FVector2D& NewPosition, FNodeSet& NodeFilter ) override;
	virtual FVector2D GetPosition() const override;
	virtual FString GetNodeComment() const override;
	virtual UObject* GetObjectBeingDisplayed() const override;
	// End of SNodePanel::SNode interface

	/** Set attribute for determining if widget is editable */
	void SetIsEditable(TAttribute<bool> InIsEditable);

	/** Returns if widget is editable, additionally considers if the owning graph is read only */
	virtual bool IsNodeEditable() const;

	/** Set event when node is double clicked */
	void SetDoubleClickEvent(FSingleNodeEvent InDoubleClickEvent);

	/** @param OwnerPanel  The GraphPanel that this node belongs to */
	virtual void SetOwner( const TSharedRef<SGraphPanel>& OwnerPanel );

	/** @return the editable title for a node */
	FString GetEditableNodeTitle() const;

	/** @return the editable title for a node */
	FText GetEditableNodeTitleAsText() const;

	/** @return the tint for the node's title image */
	FSlateColor GetNodeTitleColor() const;

	/** @return the tint for the node's comment */
	FSlateColor GetNodeCommentColor() const;

	/** @return the tint for the node's main body */
	FSlateColor GetNodeBodyColor() const;

	/** @return the tint for the node's title icon */
	FSlateColor GetNodeTitleIconColor() const;

	/** @return the tint for the node's title text */
	FLinearColor GetNodeTitleTextColor() const;

	/** @return the tooltip to display when over the node */
	FText GetNodeTooltip() const;
	
	/** @return the node being observed by this widget*/
	UEdGraphNode* GetNodeObj() const;

	/** @return the node under the mouse (either this node or one of its children) */
	virtual TSharedRef<SGraphNode> GetNodeUnderMouse(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	TSharedPtr<SGraphPanel> GetOwnerPanel() const;

	/**
	 * Update this GraphNode to match the data that it is observing
	 */
	virtual void UpdateGraphNode();
	
	/** Create the widgets for pins on the node */
	virtual void CreatePinWidgets();

	/** Create a single pin widget */
	virtual void CreateStandardPinWidget(UEdGraphPin* Pin);

	/**
	 * Get all the pins found on this node.
	 *
	 * @param AllPins  The set of pins found on this node.
	 */
	void GetPins( TSet< TSharedRef<SWidget> >& AllPins ) const;
	void GetPins( TArray< TSharedRef<SWidget> >& AllPins ) const;

	/** 
	 * Find the pin that is hovered.
	 *
	 * @param MyGeometry  The geometry of the node
	 * @param MouseEvent  Information about the mouse
	 *
	 * @return A pointer to the pin widget hovered in this node, or invalid pointer if none.
	 */
	TSharedPtr<SGraphPin> GetHoveredPin( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) const;

	TSharedPtr<SGraphPin> FindWidgetForPin( UEdGraphPin* ThePin ) const;

	void PlaySpawnEffect();

	/** Given a coordinate in SGraphNode space, return the same coordinate in graph space while taking zoom and panning of the parent graph into account */
	FVector2D NodeCoordToGraphCoord( const FVector2D& PanelSpaceCoordinate ) const;

	FVector2D GetContentScale() const;
	FLinearColor GetColorAndOpacity() const;
	FLinearColor GetPinLabelColorAndOpacity() const;	

	/** Set event when text is committed on the node */
	void SetVerifyTextCommitEvent(FOnNodeVerifyTextCommit InOnVerifyTextCommit);
	/** Set event when text is committed on the node */
	void SetTextCommittedEvent(FOnNodeTextCommitted InDelegate);
	/** Set event when the user generates a warning tooltip because a connection was invalid */
	void SetDisallowedPinConnectionEvent(SGraphEditor::FOnDisallowedPinConnection InOnDisallowedPinConnection);
	/** called to replace this nodes comment text */
	virtual void OnCommentTextCommitted(const FText& NewComment, ETextCommit::Type CommitInfo);
	/** called when the node's comment bubble is toggled */
	virtual void OnCommentBubbleToggled(bool bInCommentBubbleVisible);
	/** returns true if a rename is pending on this node */
	bool IsRenamePending() const { return bRenameIsPending; }

	/** Requests a rename when the node was initially spawned */
	virtual void RequestRenameOnSpawn()
	{
		RequestRename();
	}

	/** flags node as rename pending if supported */
	void RequestRename();

	/** Sets node into rename state if supported */
	void ApplyRename();

	/** return rect of the title area */
	virtual FSlateRect GetTitleRect() const;

	/** Called from drag drop code when a disallowed connection is hovered */
	void NotifyDisallowedPinConnection(const class UEdGraphPin* PinA, const class UEdGraphPin* PinB) const;

	/** Gets the unscaled position of the node from the last tick */
	FVector2D GetUnscaledPosition() const {return CachedUnscaledPosition;}

	/** Returns the current Node LOD or Highest LOD if unable to query */
	EGraphRenderingLOD::Type GetCurrentLOD() const;

	/** Called when GraphNode changes its error information, may be called when no change has actually occurred: */
	void RefreshErrorInfo();

protected:
	SGraphNode();

	void PositionThisNodeBetweenOtherNodes(const TMap< UObject*, TSharedRef<SNode> >& NodeToWidgetLookup, UEdGraphNode* PreviousNode, UEdGraphNode* NextNode, float HeightAboveWire) const;
	void PositionThisNodeBetweenOtherNodes(const TMap< UObject*, TSharedRef<SNode> >& NodeToWidgetLookup, TSet<UEdGraphNode*>& PreviousNodes, TSet<UEdGraphNode*>& NextNodes, float HeightAboveWire) const;
	void PositionThisNodeBetweenOtherNodes(const FVector2D& PrevPos, const FVector2D& NextPos, float HeightAboveWire) const;

	/**
	 * Check whether drag and drop functionality is permitted on the given node 
	 */
	static bool CanAllowInteractionUsingDragDropOp( const UEdGraphNode* GraphNodePtr, const TSharedPtr<FActorDragDropOp>& DragDropOp );

	/**
	 *	Function to get error description string
	 *
	 *	@return string to be displayed as tooltip.
	 */
	FText GetErrorMsgToolTip() const;

	/**
	 * Add a new pin to this graph node. The pin must be newly created.
	 *
	 * @param PinToAdd   A new pin to add to this GraphNode.
	 */
	virtual void AddPin( const TSharedRef<SGraphPin>& PinToAdd );

	/** Hook that allows derived classes to supply their own SGraphPin derivatives for any pin. */
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const;

	/**
	 * Override this to provide support for an 'expensive' tooltip widget that is only built on demand
	 */
	virtual TSharedPtr<SToolTip> GetComplexTooltip() { return NULL; }

	// Override this to add widgets below the node and pins
	virtual void CreateBelowWidgetControls(TSharedPtr<SVerticalBox> MainBox) {}

	// Override this to add widgets below the pins but above advanced view arrow
	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) {}

	/* Helper function to check if node can be renamed */
	virtual bool IsNameReadOnly () const;

	/** Called when text is being committed to check for validity */
	bool OnVerifyNameTextChanged ( const FText& InText, FText& OutErrorMessage );

	/* Called when text is committed on the node */
	void OnNameTextCommited ( const FText& InText, ETextCommit::Type CommitInfo ) ;

	/* Helper function to set the error color for the node */
	FSlateColor GetErrorColor() const	{return ErrorColor;}

	/** Helper function to get any error text for the node */
	FString GetErrorMessage() const {return ErrorMsg;}

	/** Called to set error text on the node */
	void UpdateErrorInfo();

	/** Set-up the error reporting widget for the node */
	void SetupErrorReporting();

	// Should we use low-detail node titles?
	virtual bool UseLowDetailNodeTitles() const;

	/** Return the desired comment bubble color */
	virtual FSlateColor GetCommentColor() const { return FLinearColor::White; }

	///// ADVANCED VIEW FUNCTIONS /////

	/** Create button to show/hide advanced pins */
	void CreateAdvancedViewArrow(TSharedPtr<SVerticalBox> MainBox);

	/** Returns visibility of AdvancedViewButton */
	EVisibility AdvancedViewArrowVisibility() const;

	/** Show/hide advanced view */
	void OnAdvancedViewChanged( const ECheckBoxState NewCheckedState );

	/** hidden == unchecked, shown == checked */
	ECheckBoxState IsAdvancedViewChecked() const;

	/** Up when shown, down when hidden */
	const FSlateBrush* GetAdvancedViewArrow() const;

	/** Checks if the node is the only node selected */
	bool IsSelectedExclusively() const;

	virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) {}
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle);
	
	/** Create the inner node content area, including the left/right pin boxes */
	virtual TSharedRef<SWidget> CreateNodeContentArea();

	///// ADD PIN BUTTON FUNCTIONS /////

	/** Override this to create a button to add pins on the input side of the node */
	virtual void CreateInputSideAddButton(TSharedPtr<SVerticalBox> InputBox) {};

	/** Override this to create a button to add pins on the output side of the node */
	virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) {};

	/** Creates widget for an Add pin button, which can then be added to the node */
	TSharedRef<SWidget> AddPinButtonContent(FText PinText, FText PinTooltipText, bool bRightSide = true, FString DocumentationExcerpt = FString(), TSharedPtr<SToolTip> CustomTooltip = NULL);

	/** Checks whether Add pin button should currently be visible */
	virtual EVisibility IsAddPinButtonVisible() const;

	/** Callback function executed when Add pin button is clicked */
	virtual FReply OnAddPin() {return FReply::Handled();}

	/* Populate a meta data tag with information about this graph node */
	virtual void PopulateMetaTag(class FGraphNodeMetaData* TagMeta) const;

	/** Returns TRUE if the input pin should be hidden from view */
	bool ShouldPinBeHidden(const UEdGraphPin* InPin) const;

protected:
	/** Input pin widgets on this node */
	TArray< TSharedRef<SGraphPin> > InputPins;
	/** Output pin widgets on this node */
	TArray< TSharedRef<SGraphPin> > OutputPins;
	/** The GraphPanel within in which this node resides.*/
	TWeakPtr<SGraphPanel> OwnerGraphPanelPtr;
	/** The GraphNode being observed by this widget */
	UEdGraphNode* GraphNode;
	/** The area where input pins reside */
	TSharedPtr<SVerticalBox> LeftNodeBox;
	/** The area where output pins reside */
	TSharedPtr<SVerticalBox> RightNodeBox;
	/** Used to display the name of the node and allow renaming of the node */
	TSharedPtr<SInlineEditableTextBlock> InlineEditableText;
	/** Error handling widget */
	TSharedPtr<class IErrorReportingWidget> ErrorReporting;

	FCurveSequence SpawnAnim;
	FCurveHandle ZoomCurve;
	FCurveHandle FadeCurve;

	/** Is this node editable */
	TAttribute<bool> IsEditable;

	FSingleNodeEvent OnDoubleClick;

	// Is the current tooltip a complex one that should be dropped when the tooltip is no longer displayed?
	bool bProvidedComplexTooltip;

	// Is a rename operation pending
	bool bRenameIsPending;
	/** Called whenever the text on the node is being committed interactively by the user, validates the string for commit */
	FOnNodeVerifyTextCommit OnVerifyTextCommit;
	/** Called whenever the text on the node is committed interactively by the user */
	FOnNodeTextCommitted OnTextCommitted;
	/** Called when the user generates a warning tooltip because a connection was invalid */
	SGraphEditor::FOnDisallowedPinConnection OnDisallowedPinConnection;
	/** Used to report errors on the node */
	FString ErrorMsg;
	/** Used to set the error color */
	FSlateColor ErrorColor;

	/** Caches true position of node */
	FVector2D CachedUnscaledPosition;

	/** Cached icon color for the node */
	FLinearColor IconColor;

	/** Cached pointer to graph editor settings */
	const class UGraphEditorSettings* Settings;
};
