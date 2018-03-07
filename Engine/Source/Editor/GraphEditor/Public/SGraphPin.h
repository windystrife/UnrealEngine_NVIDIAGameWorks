// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "EdGraph/EdGraphPin.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "SGraphNode.h"

class SGraphPanel;
class SGraphPin;
class SHorizontalBox;
class SImage;

#define NAME_DefaultPinLabelStyle TEXT("Graph.Node.PinName")

/////////////////////////////////////////////////////
// FGraphPinHandle

/** A handle to a pin, defined by its owning node's GUID, and the pin's name. Used to reference a pin without referring to its widget */
struct GRAPHEDITOR_API FGraphPinHandle
{
	/** The GUID of the node to which this pin belongs */
	FGuid NodeGuid;

	/** The GUID of the pin we are referencing */
	FGuid PinId;

	/** Constructor */
	FGraphPinHandle(UEdGraphPin* InPin);

	/** */
	UEdGraphPin* GetPinObj(const SGraphPanel& InPanel) const;

	/** Find a pin widget in the specified panel from this handle */
	TSharedPtr<class SGraphPin> FindInGraphPanel(const class SGraphPanel& InPanel) const;

	/** */
	bool IsValid() const
	{
		return PinId.IsValid() && NodeGuid.IsValid();
	}

	/** */
	bool operator==(const FGraphPinHandle& Other) const
	{
		return PinId == Other.PinId && NodeGuid == Other.NodeGuid;
	}

	/** */
	friend inline uint32 GetTypeHash(const FGraphPinHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.PinId), GetTypeHash(Handle.NodeGuid));
	}
};

/////////////////////////////////////////////////////
// SGraphPin

/**
 * Represents a pin on a node in the GraphEditor
 */
class GRAPHEDITOR_API SGraphPin : public SBorder
{
public:

	SLATE_BEGIN_ARGS(SGraphPin)
		: _PinLabelStyle(NAME_DefaultPinLabelStyle)
		, _UsePinColorForText(false)
		, _SideToSideMargin(5.0f)
		{}
		SLATE_ARGUMENT(FName, PinLabelStyle)
		SLATE_ARGUMENT(bool, UsePinColorForText)
		SLATE_ARGUMENT(float, SideToSideMargin)
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InPin	The pin to create a widget for
	 */
	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

public:
	SGraphPin();

	/** Set attribute for determining if pin is editable */
	void SetIsEditable(TAttribute<bool> InIsEditable);

	/** Retrieves the full horizontal box that contains the pin's row content */
	TWeakPtr<SHorizontalBox> GetFullPinHorizontalRowWidget()
	{
		return FullPinHorizontalRowWidget;
	}

	/** Handle clicking on the pin */
	virtual FReply OnPinMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent);

	/** Handle clicking on the pin name */
	FReply OnPinNameMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent);

	// SWidget interface
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

public:
	UEdGraphPin* GetPinObj() const;

	/** @param OwnerNode  The SGraphNode that this pin belongs to */
	void SetOwner( const TSharedRef<SGraphNode> OwnerNode );

	/** @return whether this pin is incoming or outgoing */
	EEdGraphPinDirection GetDirection() const;

	/** @return whether this pin is an array value */
	bool IsArray() const;

	/** @return whether this pin is a set value */
	bool IsSet() const;

	/** @return whether this pin is a map value */
	bool IsMap() const;

	/** @return whether this pin is passed by mutable ref */
	bool IsByMutableRef() const;

	/** @return whether this pin is passed by mutable ref */
	bool IsDelegate() const;

	/** @return whether this pin is connected to another pin */
	bool IsConnected() const;

	/** Tries to handle making a connection to another pin, depending on the schema and the pins it may do:  
		 - Nothing
		 - Break existing links on either side while making the new one
		 - Just make the new one
		 @return Whether a connection was actually made or not
     */
	virtual bool TryHandlePinConnection(SGraphPin& OtherSPin);

	/** @return Visibility based on whether or not we should show the inline editable text field for this pin */
	virtual EVisibility GetDefaultValueVisibility() const;

	/** Control whether we draw the label on this pin */
	void SetShowLabel(bool bNewDrawLabel);

	/** Allows the connection drawing policy to control the pin color */
	void SetPinColorModifier(FLinearColor InColor)
	{
		PinColorModifier = InColor;
	}

	/** Set this pin to only be used to display default value */
	void SetOnlyShowDefaultValue(bool bNewOnlyShowDefaultValue);

	/** If pin in node is visible at all */
	EVisibility IsPinVisibleAsAdvanced() const;

	/** Gets Node Offset */
	FVector2D GetNodeOffset() const;

	/** Returns whether or not this pin is currently connectable */
	bool GetIsConnectable() const;

	/** Get the widget we should put into the 'default value' space, shown when nothing connected */
	virtual TSharedRef<SWidget>	GetDefaultValueWidget();

protected:
	FText GetPinLabel() const;

	/** If we should show the label on this pin */
	EVisibility GetPinLabelVisibility() const
	{
		return bShowLabel ? EVisibility::Visible : EVisibility::Collapsed;
	}

	/** Get the widget we should put in the label space, which displays the name of the pin.*/
	virtual TSharedRef<SWidget> GetLabelWidget(const FName& InPinLabelStyle);

	/** @return The brush with which to paint this graph pin's incoming/outgoing bullet point */
	virtual const FSlateBrush* GetPinIcon() const;

	/** @return the brush which is to paint the 'secondary icon' for this pin, (e.g. value type for Map pins */
	const FSlateBrush* GetSecondaryPinIcon() const;

	const FSlateBrush* GetPinBorder() const;
	
	/** @return The status brush to show after/before the name (on an input/output) */
	virtual const FSlateBrush* GetPinStatusIcon() const;

	/** @return Should we show a status brush after/before the name (on an input/output) */
	virtual EVisibility GetPinStatusIconVisibility() const;

	/** Toggle the watch pinning */
	virtual FReply ClickedOnPinStatusIcon();

	/** @return The color that we should use to draw this pin */
	virtual FSlateColor GetPinColor() const;

	/** @return The secondary color that we should use to draw this pin (e.g. value color for Map pins) */
	FSlateColor GetSecondaryPinColor() const;

	/** @return The color that we should use to draw this pin's text */
	virtual FSlateColor GetPinTextColor() const;

	/** @return The tooltip to display for this pin */
	FText GetTooltipText() const;

	TOptional<EMouseCursor::Type> GetPinCursor() const;

	/** Spawns a FDragConnection or similar class for the pin drag event */
	virtual TSharedRef<FDragDropOperation> SpawnPinDragEvent(const TSharedRef<class SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins);
	
	/** True if pin can be edited */
	bool IsEditingEnabled() const;

	// Should we use low-detail pin names?
	virtual bool UseLowDetailPinNames() const;

	/** Determines the pin's visibility based on the LOD factor, when it is low LOD, no hit test will occur */
	EVisibility GetPinVisiblity() const;

protected:
	/** The GraphNode that owns this pin */
	TWeakPtr<SGraphNode> OwnerNodePtr;

	/** Image of pin */
	TSharedPtr<SWidget> PinImage;

	/** Horizontal box that holds the full detail pin widget, useful for outsiders to inject widgets into the pin */
	TWeakPtr<SHorizontalBox> FullPinHorizontalRowWidget;

	/** The GraphPin that this widget represents. */
	class UEdGraphPin* GraphPinObj;

	/** Is this pin editable */
	TAttribute<bool> IsEditable;

	/** If we should draw the label on this pin */
	bool bShowLabel;

	/** If we should draw the label on this pin */
	bool bOnlyShowDefaultValue;

	/** true when we're moving links between pins. */
	bool bIsMovingLinks;

	/** Color modifier for use by the connection drawing policy */
	FLinearColor PinColorModifier;

	/** Cached offset from owning node to approximate position of culled pins */
	FVector2D CachedNodeOffset;

	TSet< FEdGraphPinReference > HoverPinSet;

	/** TRUE if the pin should use the Pin's color for the text */
	bool bUsePinColorForText;

	//@TODO: Want to cache these once for all SGraphPins, but still handle slate style updates
	const FSlateBrush* CachedImg_ArrayPin_Connected;
	const FSlateBrush* CachedImg_ArrayPin_Disconnected;
	const FSlateBrush* CachedImg_RefPin_Connected;
	const FSlateBrush* CachedImg_RefPin_Disconnected;
	const FSlateBrush* CachedImg_Pin_Connected;
	const FSlateBrush* CachedImg_Pin_Disconnected;
	const FSlateBrush* CachedImg_DelegatePin_Connected;
	const FSlateBrush* CachedImg_DelegatePin_Disconnected;
	const FSlateBrush* CachedImg_SetPin;
	const FSlateBrush* CachedImg_MapPinKey;
	const FSlateBrush* CachedImg_MapPinValue;

	const FSlateBrush* CachedImg_Pin_Background;
	const FSlateBrush* CachedImg_Pin_BackgroundHovered;
};
