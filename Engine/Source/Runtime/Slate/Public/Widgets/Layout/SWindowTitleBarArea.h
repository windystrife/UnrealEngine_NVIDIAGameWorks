// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Input/SButton.h"


class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;
class SWindow;

template< typename ObjectType > class TAttribute;

class SLATE_API SWindowTitleBarArea
	: public SPanel
{
public:
	
	class SLATE_API FSlot : public TSupportsOneChildMixin<FSlot>, public TSupportsContentAlignmentMixin<FSlot>, public TSupportsContentPaddingMixin<FSlot>
	{
		public:
			FSlot()
			: TSupportsOneChildMixin<FSlot>()
			, TSupportsContentAlignmentMixin<FSlot>(HAlign_Fill, VAlign_Fill)
			{ }
	};

	SLATE_BEGIN_ARGS(SWindowTitleBarArea)
		: _HAlign( HAlign_Fill )
		, _VAlign( VAlign_Fill )
		, _Padding( 0.0f )
		, _Content()
		{ }

		/** Horizontal alignment of content in the area allotted to the SWindowTitleBarArea by its parent */
		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )

		/** Vertical alignment of content in the area allotted to the SWindowTitleBarArea by its parent */
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )

		/** Padding between the SWindowTitleBarArea and the content that it presents. Padding affects desired size. */
		SLATE_ATTRIBUTE( FMargin, Padding )

		/** The widget content presented by the SWindowTitleBarArea */
		SLATE_DEFAULT_SLOT( FArguments, Content )

		/** Called when the the title bar area is double clicked */
		SLATE_EVENT( FSimpleDelegate, OnDoubleClick )

	SLATE_END_ARGS()

	SWindowTitleBarArea();

	void Construct( const FArguments& InArgs );

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FChildren* GetChildren() override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual EWindowZone::Type GetWindowZoneOverride() const override;

	/** See the Content slot. */
	void SetContent(const TSharedRef< SWidget >& InContent);

	/** See HAlign argument */
	void SetHAlign(EHorizontalAlignment HAlign);

	/** See VAlign argument */
	void SetVAlign(EVerticalAlignment VAlign);

	/** See Padding attribute */
	void SetPadding(const TAttribute<FMargin>& InPadding);

	void SetGameWindow(TSharedPtr<SWindow> Window)
	{
		GameWindow = Window;
	}

	void SetOnDoubleClickCallback(FSimpleDelegate InOnDoubleClick)
	{
		OnDoubleClick = InOnDoubleClick;
	}

	void SetCustomStyleForWindowButtons(const FButtonStyle& InMinimizeButtonStyle, const FButtonStyle& InMaximizeButtonStyle, const FButtonStyle& InRestoreButtonStyle, const FButtonStyle& InCloseButtonStyle)
	{
		MinimizeButtonStyle = InMinimizeButtonStyle;
		MaximizeButtonStyle = InMaximizeButtonStyle;
		RestoreButtonStyle = InRestoreButtonStyle;
		CloseButtonStyle = InCloseButtonStyle;
	}

protected:

	FSlot ChildSlot;

private:

	TSharedPtr<SWindow> GameWindow;
	FSimpleDelegate OnDoubleClick;

	FButtonStyle MinimizeButtonStyle;
	FButtonStyle MaximizeButtonStyle;
	FButtonStyle RestoreButtonStyle;
	FButtonStyle CloseButtonStyle;

	bool bIsMinimizeButtonEnabled;
	bool bIsMaximizeRestoreButtonEnabled;
	bool bIsCloseButtonEnabled;

	FReply MinimizeButton_OnClicked();
	FReply MaximizeRestoreButton_OnClicked();
	FReply CloseButton_OnClicked();

	const FSlateBrush* GetMinimizeImage() const;
	const FSlateBrush* GetMaximizeRestoreImage() const;
	const FSlateBrush* GetCloseImage() const;

	TSharedPtr<SButton> MinimizeButton;
	TSharedPtr<SButton> MaximizeRestoreButton;
	TSharedPtr<SButton> CloseButton;

	TSharedPtr<SVerticalBox> WindowButtonsBox;
};
