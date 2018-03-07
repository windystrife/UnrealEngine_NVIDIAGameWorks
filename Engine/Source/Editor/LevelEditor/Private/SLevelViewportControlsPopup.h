// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
class SImage;
class SMenuAnchor;
struct FSlateBrush;

/**
* Widget that opens a controls help popup when clicked
*/
class SLevelViewportControlsPopup : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLevelViewportControlsPopup)
	{}
	SLATE_END_ARGS()

	/**
	* Constructs the menu
	*/
	void Construct(const FArguments& Declaration);
private:
	/**
	 * Gets the current image for the button depending on state
	 */
	const FSlateBrush* GetButtonImage() const;

	/**
	 * Responds to mouse clicks
	 */
	FReply OnClicked() const;

private:
	/** The Button to open the menu*/
	TSharedPtr< SButton > Button;
	/** The Image to display on the button */
	TSharedPtr< SImage > ButtonImage;
	/** Default Button Image */
	const FSlateBrush* Default;
	/** Button Image while hovered */
	const FSlateBrush* Hovered;
	/** Button Image when pressed */
	const FSlateBrush* Pressed;
	/** Path to html file for viewport controls */
	FString PopupPath;
};
