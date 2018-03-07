// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Textures/SlateIcon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/MultiBox/MultiBox.h"

/**
 * Tool bar combo button MultiBlock
 */
class FToolBarComboButtonBlock
	: public FMultiBlock
{

public:

	/**
	 * Constructor
	 *
	 * @param	InAction					UI action that sets the enabled state for this combo button
	 * @param	InMenuContentGenerator		Delegate that generates a widget for this combo button's menu content.  Called when the menu is summoned.
	 * @param	InLabel						Optional label for this combo button.
	 * @param	InToolTip					Tool tip string (required!)
	 * @param	InIcon						Optional icon to use for the tool bar image
	 * @param	bInSimpleComboBox			If true, the icon and label won't be displayed
	 */
	FToolBarComboButtonBlock( const FUIAction& InAction, const FOnGetContent& InMenuContentGenerator, const TAttribute<FText>& InLabel = TAttribute<FText>(), const TAttribute<FText>& InToolTip = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIcon = TAttribute<FSlateIcon>(), bool bInSimpleComboBox = false );

	/** FMultiBlock interface */
	virtual void CreateMenuEntry(class FMenuBuilder& MenuBuilder) const override;
	virtual bool HasIcon() const override;

	/** 
	 * Sets the visibility of the blocks label
	 *
	 * @param InLabelVisibility		Visibility setting to use for the label
	 */
	void SetLabelVisibility( EVisibility InLabelVisibility ) { LabelVisibility = InLabelVisibility; }

	/** Set whether this toolbar should always use small icons, regardless of the current settings */
	void SetForceSmallIcons( const bool InForceSmallIcons ) { bForceSmallIcons = InForceSmallIcons; }


private:

	/**
	 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
	 *
	 * @return  MultiBlock widget object
	 */
	virtual TSharedRef< class IMultiBlockBaseWidget > ConstructWidget() const override;


private:

	// Friend our corresponding widget class
	friend class SToolBarComboButtonBlock;

	/** Delegate that generates a widget for this combo button's menu content.  Called when the menu is summoned. */
	FOnGetContent MenuContentGenerator;

	/** Optional overridden text label for this tool bar button.  If not set, then the action's label will be used instead. */
	TAttribute<FText> Label;

	/** Optional overridden tool tip for this tool bar button.  If not set, then the action's tool tip will be used instead. */
	TAttribute<FText> ToolTip;

	/** Optional overridden icon for this tool bar button.  IF not set, then the action's icon will be used instead. */
	TAttribute<FSlateIcon> Icon;

	/** Controls the Labels visibility, defaults to GetIconVisibility if no override is provided */
	TOptional< EVisibility > LabelVisibility;

	/** If true, the icon and label won't be displayed */
	bool bSimpleComboBox;

	/** Whether this toolbar should always use small icons, regardless of the current settings */
	bool bForceSmallIcons;
};



/**
 * Tool bar button MultiBlock widget
 */
class SLATE_API SToolBarComboButtonBlock
	: public SMultiBlockBaseWidget
{

public:

	SLATE_BEGIN_ARGS( SToolBarComboButtonBlock )
		: _ForceSmallIcons( false )
	{}

		/** Controls the visibility of the blocks label */
		SLATE_ARGUMENT( TOptional< EVisibility >, LabelVisibility )
		
		/** Optional overridden icon for this tool bar button.  IF not set, then the action's icon will be used instead. */
		SLATE_ATTRIBUTE( FSlateIcon, Icon );

		/** Whether this toolbar should always use small icons, regardless of the current settings */
		SLATE_ARGUMENT( bool, ForceSmallIcons )

	SLATE_END_ARGS()


	/**
	 * Builds this MultiBlock widget up from the MultiBlock associated with it
	 */
	virtual void BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName) override;


	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

protected:

	/**
	 * Called by Slate when content for this button's menu needs to be generated
	 *
	 * @return	The widget to use for the menu content
	 */
	TSharedRef<SWidget> OnGetMenuContent();


	/**
	 * Called by Slate to determine if this button is enabled
	 * 
	 * @return True if the menu entry is enabled, false otherwise
	 */
	bool IsEnabled() const;

	/**
	 * Called by Slate to determine if this button is visible
	 *
	 * @return EVisibility::Visible or EVisibility::Collapsed, depending on if the button should be displayed
	 */
	EVisibility GetVisibility() const;
private:
	/** @return True if this toolbar button is using a dynamically set icon */
	bool HasDynamicIcon() const;

	/** @return The icon for the toolbar button; may be dynamic, so check HasDynamicIcon */
	const FSlateBrush* GetIcon() const;

	/** @return The small icon for the toolbar button; may be dynamic, so check HasDynamicIcon */
	const FSlateBrush* GetSmallIcon() const;

	/** Called by Slate to determine whether icons/labels are visible */
	EVisibility GetIconVisibility(bool bIsASmallIcon) const;

	/** Controls the visibility of the of label, defaults to GetIconVisibility */
	TAttribute< EVisibility > LabelVisibility;

	/** Optional overridden icon for this tool bar button.  IF not set, then the action's icon will be used instead. */
	TAttribute<FSlateIcon> Icon;

	/** Whether this toolbar should always use small icons, regardless of the current settings */
	bool bForceSmallIcons;
};
