// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Input/CursorReply.h"
#include "Widgets/SCompoundWidget.h"
#include "SNodePanel.h"
#include "Framework/SlateDelegates.h"

class SMultiLineEditableTextBox;

DECLARE_DELEGATE_RetVal( bool, FIsGraphNodeHovered );
DECLARE_DELEGATE_OneParam( FOnCommentBubbleToggled, bool );

class GRAPHEDITOR_API SCommentBubble : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SCommentBubble )
		: _GraphNode( NULL )
		, _ColorAndOpacity( FLinearColor::White )
		, _AllowPinning( false )
		, _EnableTitleBarBubble( false )
		, _EnableBubbleCtrls( false )
		, _InvertLODCulling( false )
		, _GraphLOD( EGraphRenderingLOD::DefaultDetail )
	{}

		/** the GraphNode this bubble should interact with */
		SLATE_ARGUMENT( UEdGraphNode*, GraphNode )

		/** Attribute to determine the visibility buttons check state */
		SLATE_ATTRIBUTE( ECheckBoxState, ToggleButtonCheck )

		/** The comment text for the bubble */
		SLATE_ATTRIBUTE( FString, Text )

		/** Called when the comment text is committed */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Called when the comment bubble is toggled */
		SLATE_EVENT( FOnCommentBubbleToggled, OnToggled )

		/** The comment hint text for the bubble */
		SLATE_ATTRIBUTE( FText, HintText )

		/** Color and opacity for the comment bubble */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Allow bubble to be pinned */
		SLATE_ARGUMENT( bool, AllowPinning )

		/** Enable the title bar bubble to toggle */
		SLATE_ARGUMENT( bool, EnableTitleBarBubble )

		/** Enable the controls within the bubble */
		SLATE_ARGUMENT( bool, EnableBubbleCtrls )

		/** Invert LOD culling */
		SLATE_ARGUMENT( bool, InvertLODCulling )

		/** The current level of detail */
		SLATE_ATTRIBUTE( EGraphRenderingLOD::Type, GraphLOD )

		/** Delegate to determine if the parent node is in the hovered state. */
		SLATE_EVENT( FIsGraphNodeHovered, IsGraphNodeHovered )

	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 */
	void Construct( const FArguments& InArgs );


	//~ Begin SWidget Interface
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	//~ End SWidget Interface

	/** Updates the comment Visibility */
	void TickVisibility(const double InCurrentTime, const float InDeltaTime);

	/** Returns the offset from the SNode center slot */
	FVector2D GetOffset() const;

	/** Returns the offset to the arrow center accounting for zoom on either the comment bubble or the title bar button based on current state */
	float GetArrowCenterOffset() const;

	/** Returns the bubble size */
	FVector2D GetSize() const;

	/** Returns if comment bubble is visible */
	bool IsBubbleVisible() const;

	/** Returns if graph scaling can be applied to this bubble */
	bool IsScalingAllowed() const;

	/** Called to display/hide the comment bubble */
	void OnCommentBubbleToggle( ECheckBoxState State );

	/** Directly sets the bubble's visibility, without assuming it was from a user action (no undo transaction, or OnToggled callback)*/
	void SetCommentBubbleVisibility(bool bVisible);

	/** Called when a node's comment bubble pinned state is changed */
	void OnPinStateToggle( ECheckBoxState State ) const;

	/** Called to update the bubble widget layout */
	void UpdateBubble();

	/** Returns if the text block currently has keyboard focus*/
	bool TextBlockHasKeyboardFocus() const;

protected:

	/** Returns the current scale button tooltip */
	FText GetScaleButtonTooltip() const;

	/** Called to determine if the comment bubble's current visibility */
	EVisibility GetBubbleVisibility() const;

	/** Called to determine if the toggle button's current visibility */
	EVisibility GetToggleButtonVisibility() const;

	/** Returns the color for the toggle bubble including the opacity value */
	FSlateColor GetToggleButtonColor() const;

	/** Returns the color of the main bubble */
	FSlateColor GetBubbleColor() const;

	/** Returns the current background color for the textbox */
	FSlateColor GetTextBackgroundColor() const;
	
	/** Returns the current foreground color for the textbox */
	FSlateColor GetTextForegroundColor() const;

	/** Returns the foreground color for the text and buttons, taking into account the bubble color */
	FSlateColor GetForegroundColor() const override { return ForegroundColor; }

	/** Called when the comment text is committed */
	void OnCommentTextCommitted( const FText& NewText, ETextCommit::Type CommitInfo );

	/** Returns bubble toggle check state */
	ECheckBoxState GetToggleButtonCheck() const;

	/** Returns pinned check state */
	ECheckBoxState GetPinnedButtonCheck() const;

	/** Called to determine if the comment bubble is readonly */
	bool IsReadOnly() const;

protected:

	/** The GraphNode this widget interacts with */
	UEdGraphNode* GraphNode;
	/** Cached inline editable text box */
	TSharedPtr<SMultiLineEditableTextBox> TextBlock;

	/** The Comment Bubble color and opacity value */
	TAttribute<FSlateColor> ColorAndOpacity;
	/** Attribute to query node comment */
	TAttribute<FString> CommentAttribute;
	/** Attribute to query current LOD */
	TAttribute<EGraphRenderingLOD::Type> GraphLOD;
	/** Hint Text */
	TAttribute<FText> HintText;
	/** Toggle button checked state  */
	TAttribute<ECheckBoxState> ToggleButtonCheck;

	/** Optional delegate to call when the comment text is committed */
	FOnTextCommitted OnTextCommittedDelegate;
	/** Optional delegate to call when the comment bubble is toggled */
	FOnCommentBubbleToggled OnToggledDelegate;
	/** Delegate to determine if the graph node is currently hovered */
	FIsGraphNodeHovered IsGraphNodeHovered;

	/** Current Foreground Color */
	FLinearColor ForegroundColor;
	/** Allow pin behaviour */
	bool bAllowPinning;
	/** Allow in bubble controls */
	bool bEnableBubbleCtrls;
	/** Invert the LOD culling behavior, used by comment nodes */
	bool bInvertLODCulling;
	/** Enable the title bar bubble toggle */
	bool bEnableTitleBarBubble;
	/** Used to Control hover fade up/down for widgets */
	float OpacityValue;
	/** Cached comment */
	FString CachedComment;
	/** Cached FText comment */
	FText CachedCommentText;

};
