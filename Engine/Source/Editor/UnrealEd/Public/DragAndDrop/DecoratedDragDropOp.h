// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "DecoratedDragDrop"

class FDecoratedDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FDecoratedDragDropOp, FDragDropOperation)

	/** String to show as hover text */
	FText								CurrentHoverText;

	/** Icon to be displayed */
	const FSlateBrush*					CurrentIconBrush;

	FDecoratedDragDropOp()
		: CurrentIconBrush(nullptr)
		, DefaultHoverIcon(nullptr)
	{ }

	/** Overridden to provide public access */
	virtual void Construct() override
	{
		FDragDropOperation::Construct();
	}

	/** Set the decorator back to the icon and text defined by the default */
	virtual void ResetToDefaultToolTip()
	{
		CurrentHoverText = DefaultHoverText;
		CurrentIconBrush = DefaultHoverIcon;
	}

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		// Create hover widget
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			.Content()
			[			
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 0.0f, 0.0f, 3.0f, 0.0f )
				.VAlign( VAlign_Center )
				[
					SNew( SImage )
					.Image( this, &FDecoratedDragDropOp::GetIcon )
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign( VAlign_Center )
				[
					SNew(STextBlock) 
					.Text( this, &FDecoratedDragDropOp::GetHoverText )
				]
			];
	}

	FText GetHoverText() const
	{
		return CurrentHoverText;
	}

	const FSlateBrush* GetIcon( ) const
	{
		return CurrentIconBrush;
	}

	/** Set the text and icon for this tooltip */
	void SetToolTip(const FText& Text, const FSlateBrush* Icon)
	{
		CurrentHoverText = Text;
		CurrentIconBrush = Icon;
	}

	/** Setup some default values for the decorator */
	void SetupDefaults()
	{
		DefaultHoverText = CurrentHoverText;
		DefaultHoverIcon = CurrentIconBrush;
	}

protected:

	/** Default string to show as hover text */
	FText								DefaultHoverText;

	/** Default icon to be displayed */
	const FSlateBrush*					DefaultHoverIcon;
};


#undef LOCTEXT_NAMESPACE
