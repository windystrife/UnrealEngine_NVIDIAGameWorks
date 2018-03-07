// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Types/SlateStructs.h"
#include "Layout/Visibility.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SCheckBox.h"

class INotificationWidget;
class SVerticalBox;
class SWindow;

/**
 * Interface class for an item in the event message list.
 * Real implementation is found in SEventMessageItemImpl
 */
class SLATE_API SNotificationItem
	: public SCompoundWidget
{
public:
	enum ECompletionState
	{
		CS_None,
		CS_Pending,
		CS_Success,
		CS_Fail
	};

	/** Sets the text for message element */
	virtual void SetText( const TAttribute< FText >& InText ) = 0;

	/** Sets the text and delegate for the hyperlink */
	virtual void SetHyperlink( const FSimpleDelegate& InHyperlink, const TAttribute< FText >& InHyperlinkText = TAttribute< FText >() ) = 0;

	/** Sets the ExpireDuration */
	virtual void SetExpireDuration(float ExpireDuration) = 0;

	/** Sets the FadeInDuration */
	virtual void SetFadeInDuration(float FadeInDuration) = 0;

	/** Sets the FadeOutDuration */
	virtual void SetFadeOutDuration(float FadeOutDuration) = 0;

	/** Gets the visibility state of the throbber, success, and fail images */
	virtual ECompletionState GetCompletionState() const = 0;

	/** Sets the visibility state of the throbber, success, and fail images */
	virtual void SetCompletionState(ECompletionState State) = 0;

	/** Waits for the ExpireDuration then begins to fade out */
	virtual void ExpireAndFadeout() = 0;

	/** Begin the fade out */
	virtual void Fadeout() = 0;
};


/**
 * Setup class to initialize buttons on a notification
 */
struct FNotificationButtonInfo
{
	FNotificationButtonInfo( const FText& InText, const FText& InToolTip, FSimpleDelegate InCallback, SNotificationItem::ECompletionState VisibleInState = SNotificationItem::CS_Pending )
		: Text( InText )
		, ToolTip( InToolTip )
		, Callback( InCallback )
		, VisibilityOnNone( VisibleInState == SNotificationItem::CS_None ? EVisibility::Visible : EVisibility::Collapsed )
		, VisibilityOnPending( VisibleInState == SNotificationItem::CS_Pending ? EVisibility::Visible : EVisibility::Collapsed )
		, VisibilityOnSuccess( VisibleInState == SNotificationItem::CS_Success ? EVisibility::Visible : EVisibility::Collapsed )
		, VisibilityOnFail( VisibleInState == SNotificationItem::CS_Fail ? EVisibility::Visible : EVisibility::Collapsed )
	{ }

	/** Message on the button */
	FText Text;
	/** Tip displayed when moused over */
	FText ToolTip;
	/** Method called when button clicked */
	FSimpleDelegate Callback;

	/** Visibility of the button when the completion state of the button is SNotificationItem::ECompletionState::None */
	EVisibility VisibilityOnNone;
	/** Visibility of the button when the completion state of the button is SNotificationItem::ECompletionState::Pending */
	EVisibility VisibilityOnPending;
	/** Visibility of the button when the completion state of the button is SNotificationItem::ECompletionState::Success */
	EVisibility VisibilityOnSuccess;
	/** Visibility of the button when the completion state of the button is SNotificationItem::ECompletionState::Fail */
	EVisibility VisibilityOnFail;
};


/**
 * Setup class to initialize a notification.
 */
struct FNotificationInfo
{
	/**
	 * FNotifcationInfo initialization constructor
	 *
	 * @param	InText	Text string to display for this notification
	 */
	FNotificationInfo( const FText& InText )
		: ContentWidget(),
		Text(InText),
		ButtonDetails(),
		Image(nullptr),
		FadeInDuration(0.5f),
		FadeOutDuration(2.0f),
		ExpireDuration(1.0f),
		bUseThrobber(true),
		bUseSuccessFailIcons(true),
		bUseLargeFont(true),
		WidthOverride(),
		bFireAndForget(true),
		CheckBoxState(ECheckBoxState::Unchecked),
		CheckBoxStateChanged(),
		CheckBoxText(),
		Hyperlink(),
		HyperlinkText( NSLOCTEXT("EditorNotification", "DefaultHyperlinkText", "Show Log") ),
		bAllowThrottleWhenFrameRateIsLow(true)
	{ };

	/**
	* FNotifcationInfo initialization constructor
	*
	* @param	InContentWidget	The content of the notification
	*/
	FNotificationInfo(TSharedPtr<INotificationWidget> InContentWidget)
		: ContentWidget(InContentWidget),
		Text(),
		ButtonDetails(),
		Image(nullptr),
		FadeInDuration(0.5f),
		FadeOutDuration(2.0f),
		ExpireDuration(1.0f),
		bUseThrobber(false),
		bUseSuccessFailIcons(false),
		bUseLargeFont(false),
		WidthOverride(),
		bFireAndForget(true),
		CheckBoxState(ECheckBoxState::Unchecked),
		CheckBoxStateChanged(),
		CheckBoxText(),
		Hyperlink(),
		HyperlinkText(),
		bAllowThrottleWhenFrameRateIsLow(true)
	{ };

	/** If set, overrides the entire content of the notification with this widget */
	TSharedPtr<INotificationWidget> ContentWidget;

	/** The text displayed in this text block */
	FText Text;

	/** Setup information for the buttons on the notification */ 
	TArray<FNotificationButtonInfo> ButtonDetails;

	/** The icon image to display next to the text */
	const FSlateBrush* Image;

	/** The fade in duration for this element */
	float FadeInDuration;

	/** The fade out duration for this element */
	float FadeOutDuration;

	/** The duration before a fadeout for this element */
	float ExpireDuration;

	/** Controls whether or not to add the animated throbber */
	bool bUseThrobber;

	/** Controls whether or not to display the success and fail icons */
	bool bUseSuccessFailIcons;

	/** When true the larger bolder font will be used to display the message */
	bool bUseLargeFont;

	/** When set this forces the width of the box, used to stop resizing on text change */
	FOptionalSize WidthOverride;

	/** When true the notification will automatically time out after the expire duration. */ 
	bool bFireAndForget;

	/** When set this will display a check box on the notification; handles getting the current check box state */
	TAttribute< ECheckBoxState > CheckBoxState;

	/** When set this will display a check box on the notification; handles setting the new check box state */
	FOnCheckStateChanged CheckBoxStateChanged;

	/** Text to display for the check box message */
	TAttribute< FText > CheckBoxText;

	/** When set this will display as a hyperlink on the right side of the notification. */
	FSimpleDelegate Hyperlink;

	/** Text to display for the hyperlink message */
	TAttribute< FText > HyperlinkText;

	/** True if we should throttle the editor while the notification is transitioning and performance is poor, to make sure the user can see the animation */
	bool bAllowThrottleWhenFrameRateIsLow;
};


/**
 * A list of non-intrusive messages about the status of currently active work.
 */
class SLATE_API SNotificationList
	: public SCompoundWidget
{
	friend class SNotificationExtendable;
	friend class FSlateNotificationManager;

public:

	SLATE_BEGIN_ARGS( SNotificationList ){}
		/** Sets the font used to draw the text */
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
	SLATE_END_ARGS()
	
	/**
	 * Constructs this widget.
	 *
	 * @param InArgs    Declaration from which to construct the widget.
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Adds a floating notification.
	 *
	 * @param Info 		Contains various settings used to initialize the notification.
	 */
	virtual TSharedRef<SNotificationItem> AddNotification(const FNotificationInfo& Info);

protected:

	/**
	 * Invoked when a notification item has finished fading out. Called by the faded item.
	 *
	 * @param NotificationItem The item which finished fading out.
	 */
	virtual void NotificationItemFadedOut (const TSharedRef<SNotificationItem>& NotificationItem);

protected:

	/** The vertical box containing the list of message items. */
	TSharedPtr<SVerticalBox> MessageItemBoxPtr;

	/** The parent window of this list. */
	TWeakPtr<SWindow> ParentWindowPtr;

	/** Holds passed in font */
	TAttribute<FSlateFontInfo> Font;

	/** Flag to auto-destroy this list. */
	bool bDone;

private:

	/** The last notification added to the list. To avoid weak pointer pointing to a destroyed notification when notification are not allowed. */
	TSharedPtr<SNotificationItem> LastNotification;

};
