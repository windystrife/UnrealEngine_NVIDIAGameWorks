// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

struct FSlateBrush;

// Event sent when a mode change is requested
DECLARE_DELEGATE_OneParam( FOnModeChangeRequested, FName );


// This is the base class for a persona mode widget
class KISMET_API SModeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SModeWidget)
		: _CanBeSelected(true)
	{}
		// The currently active mode, used to determine which mode is highlighted
		SLATE_ATTRIBUTE( FName, OnGetActiveMode )

		// The large image for the icon
		SLATE_ATTRIBUTE( const FSlateBrush*, IconImage )

		// The small image for the icon
		SLATE_ATTRIBUTE( const FSlateBrush*, SmallIconImage )

		// The delegate that will be called when this widget wants to change the active mode
		SLATE_EVENT( FOnModeChangeRequested, OnSetActiveMode )

		// Can this mode ever be selected?
		SLATE_ATTRIBUTE( bool, CanBeSelected )

		// Slot for the always displayed contents
		SLATE_NAMED_SLOT( FArguments, ShortContents )

		SLATE_ATTRIBUTE( const FSlateBrush*, DirtyMarkerBrush)
		
	SLATE_END_ARGS()

public:
	// Construct a SModeWidget
	void Construct(const FArguments& InArgs, const FText& InText, const FName InMode);

private:
	EVisibility GetLargeIconVisibility() const;
	EVisibility GetSmallIconVisibility() const;

	bool IsActiveMode() const;
	const FSlateBrush* GetModeNameBorderImage() const;
	FReply OnModeTabClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	FSlateFontInfo GetDesiredTitleFont() const;
private:
	// The active mode of this group
	TAttribute<FName> OnGetActiveMode;

	// The delegate to call when this mode is selected
	FOnModeChangeRequested OnSetActiveMode;

	// Can this mode be selected?
	TAttribute<bool> CanBeSelected;

	// The text representation of this mode
	FText ModeText;

	// The mode this widget is representing
	FName ThisMode;

	// Border images
	const FSlateBrush* ActiveModeBorderImage;
	const FSlateBrush* InactiveModeBorderImage;
	const FSlateBrush* HoverBorderImage;
};
