// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Notifications/SNotificationList.h"
#include "Animation/CurveHandle.h"
#include "Animation/CurveSequence.h"
#include "Application/ThrottleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Notifications/INotificationWidget.h"
#include "Framework/Notifications/NotificationManager.h"

/////////////////////////////////////////////////
// SNotificationExtendable

/** Contains the standard functionality for a notification to inherit from*/
class SNotificationExtendable : public SNotificationItem
{
public:
	virtual ~SNotificationExtendable()
	{
		// Just in case, make sure we have left responsive mode when getting cleaned up
		if ( ThrottleHandle.IsValid() )
		{
			FSlateThrottleManager::Get().LeaveResponsiveMode( ThrottleHandle );
		}
	}
	
	
	/** Sets the text for message element */
	virtual void SetText( const TAttribute< FText >& InText ) override
	{
		Text = InText;
		MyTextBlock->SetText( Text );
	}

	virtual ECompletionState GetCompletionState() const override
	{
		return CompletionState;
	}

	virtual void SetCompletionState(ECompletionState State) override
	{
		CompletionState = State;

		if (State == CS_Success || State == CS_Fail)
		{
			CompletionStateAnimation = FCurveSequence();
			GlowCurve = CompletionStateAnimation.AddCurve(0.f, 0.75f);
			CompletionStateAnimation.Play( this->AsShared() );
		}
	}

	virtual void ExpireAndFadeout() override
	{
		FadeAnimation = FCurveSequence();
		// Add some space for the expire time
		FadeAnimation.AddCurve(FadeOutDuration.Get(), ExpireDuration.Get());
		// Add the actual fade curve
		FadeCurve = FadeAnimation.AddCurve(0.f, FadeOutDuration.Get());
		FadeAnimation.PlayReverse(this->AsShared());
	}

	/** Begins the fadein of this message */
	void Fadein( const bool bAllowThrottleWhenFrameRateIsLow )
	{
		// Make visible
		SetVisibility(EVisibility::Visible);

		// Play Fadein animation
		FadeAnimation = FCurveSequence();
		FadeCurve = FadeAnimation.AddCurve(0.f, FadeInDuration.Get());
		FadeAnimation.Play( this->AsShared() );

		// Scale up/flash animation
		IntroAnimation = FCurveSequence();
		ScaleCurveX = IntroAnimation.AddCurve(0.2f, 0.3f, ECurveEaseFunction::QuadOut);
		ScaleCurveY = IntroAnimation.AddCurve(0.f, 0.2f);
		GlowCurve = IntroAnimation.AddCurve(0.5f, 0.55f, ECurveEaseFunction::QuadOut);
		IntroAnimation.Play( this->AsShared() );

		// When a fade in occurs, we need a high framerate for the animation to look good
		if( FadeInDuration.Get() > KINDA_SMALL_NUMBER && bAllowThrottleWhenFrameRateIsLow && !ThrottleHandle.IsValid() )
		{
			if( !FSlateApplication::Get().IsRunningAtTargetFrameRate() )
			{
				ThrottleHandle = FSlateThrottleManager::Get().EnterResponsiveMode();
			}
		}
	}

	/** Begins the fadeout of this message */
	virtual void Fadeout() override
	{
		// Start fade animation
		FadeAnimation = FCurveSequence();
		FadeCurve = FadeAnimation.AddCurve(0.f, FadeOutDuration.Get());
		FadeAnimation.PlayReverse(this->AsShared());
	}

	/** Sets the ExpireDuration */
	virtual void SetExpireDuration(float Duration) override
	{
		ExpireDuration = Duration;
	}

	/** Sets the FadeInDuration */
	virtual void SetFadeInDuration(float Duration) override
	{
		FadeInDuration = Duration;
	}

	/** Sets the FadeOutDuration */
	virtual void SetFadeOutDuration(float Duration) override
	{
		FadeOutDuration = Duration;
	}

	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override
	{
		const bool bIsFadingOut = FadeAnimation.IsInReverse();
		const bool bIsCurrentlyPlaying = FadeAnimation.IsPlaying();
		const bool bIsIntroPlaying = IntroAnimation.IsPlaying();

		if ( !bIsCurrentlyPlaying && bIsFadingOut )
		{
			// Reset the Animation
			FadeoutComplete();
		}

		if ( !bIsIntroPlaying && ThrottleHandle.IsValid() )
		{
			// Leave responsive mode once the intro finishes playing
			FSlateThrottleManager::Get().LeaveResponsiveMode( ThrottleHandle );
		}
	}

protected:
	/** A fadeout has completed */
	void FadeoutComplete()
	{
		// Make sure we are no longer fading
		FadeAnimation = FCurveSequence();
		FadeCurve = FCurveHandle();

		// Clear the complete state to hide all the images/throbber
		SetCompletionState(CS_None);

		// Make sure we have left responsive mode
		if ( ThrottleHandle.IsValid() )
		{
			FSlateThrottleManager::Get().LeaveResponsiveMode( ThrottleHandle );
		}

		// Clear reference
		if( MyList.IsValid() )
		{
			MyList.Pin()->NotificationItemFadedOut(SharedThis(this));
		}
	}

	/** Gets the current color along the fadeout curve */
	FSlateColor GetContentColor() const
	{
		return GetContentColorRaw();
	}

	/** Gets the current color along the fadeout curve */
	FLinearColor GetContentColorRaw() const
	{
		// if we have a parent window, we need to make that transparent, rather than
		// this widget
		if(MyList.IsValid() && MyList.Pin()->ParentWindowPtr.IsValid())
		{
			MyList.Pin()->ParentWindowPtr.Pin()->SetOpacity( FadeCurve.GetLerp() );
			return FLinearColor(1,1,1,1);
		}
		else
		{
			return FMath::Lerp( FLinearColor(1,1,1,0), FLinearColor(1,1,1,1), FadeCurve.GetLerp() );
		}
	}

	/** Gets the color of the glow effect */
	FSlateColor GetGlowColor() const
	{
		float GlowAlpha = 1.0f - GlowCurve.GetLerp();
		
		if (GlowAlpha == 1.0f)
		{
			GlowAlpha = 0.0f;
		}

		switch (CompletionState)
		{
		case CS_Success: return FLinearColor(0,1,0,GlowAlpha);
		case CS_Fail: return FLinearColor(1,0,0,GlowAlpha);
		default: return FLinearColor(1,1,1,GlowAlpha);
		}
	}

	/** Gets the scale for the entire item */
	FVector2D GetItemScale() const
	{
		return FVector2D(  ScaleCurveX.GetLerp(), ScaleCurveY.GetLerp() );
	}

	/** Gets the visibility for the throbber */
	EVisibility GetThrobberVisibility() const
	{
		return CompletionState == CS_Pending ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetSuccessFailImageVisibility() const
	{
		return (CompletionState == CS_Success || CompletionState == CS_Fail) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* GetSuccessFailImage() const
	{
		return CompletionState == CS_Success ? FCoreStyle::Get().GetBrush("NotificationList.SuccessImage") : FCoreStyle::Get().GetBrush("NotificationList.FailImage");
	}

public:
	/** The SNotificationList that is displaying this Item */
	TWeakPtr<SNotificationList> MyList;

protected:

	/** The text displayed in this text block */
	TAttribute< FText > Text;

	/** The fade in duration for this element */
	TAttribute< float > FadeInDuration;

	/** The fade out duration for this element */
	TAttribute< float > FadeOutDuration;

	/** The duration before a fadeout for this element */
	TAttribute< float > ExpireDuration;

	/** The text displayed in this element */
	TSharedPtr<STextBlock> MyTextBlock;

	/** The completion state of this message */	
	ECompletionState CompletionState;

	/** The fading animation */
	FCurveSequence FadeAnimation;
	FCurveHandle FadeCurve;

	/** The intro animation */
	FCurveSequence IntroAnimation;
	FCurveHandle ScaleCurveX;
	FCurveHandle ScaleCurveY;
	FCurveHandle GlowCurve;

	/** The completion state change animation */
	FCurveSequence CompletionStateAnimation;

	/** Handle to a throttle request made to ensure the intro animation is smooth in low FPS situations */
	FThrottleRequest ThrottleHandle;
};


/////////////////////////////////////////////////
// SNotificationItemImpl

/** A single line in the event message list with additional buttons*/
class SNotificationItemImpl : public SNotificationExtendable
{
public:
	SLATE_BEGIN_ARGS( SNotificationItemImpl )
		: _Text()
		, _Font()
		, _Image()	
		, _FadeInDuration(0.5f)
		, _FadeOutDuration(2.f)
		, _ExpireDuration(1.f)
		, _WidthOverride(FOptionalSize())
	{}

		/** The text displayed in this text block */
		SLATE_ATTRIBUTE( FText, Text )
		/** Sets the font used to draw the text */
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
		/** Setup information for the buttons on the notification */ 
		SLATE_ATTRIBUTE(TArray<FNotificationButtonInfo>, ButtonDetails)
		/** The icon image to display next to the text */
		SLATE_ATTRIBUTE( const FSlateBrush*, Image )
		/** The fade in duration for this element */
		SLATE_ATTRIBUTE( float, FadeInDuration )
		/** The fade out duration for this element */
		SLATE_ATTRIBUTE( float, FadeOutDuration )
		/** The duration before a fadeout for this element */
		SLATE_ATTRIBUTE( float, ExpireDuration )
		/** Controls whether or not to add the animated throbber */
		SLATE_ATTRIBUTE( bool, bUseThrobber)
		/** Controls whether or not to display the success and fail icons */
		SLATE_ATTRIBUTE( bool, bUseSuccessFailIcons)
		/** When true the larger bolder font will be used to display the message */
		SLATE_ATTRIBUTE( bool, bUseLargeFont)
		/** When set this forces the width of the box, used to stop resizeing on text change */
		SLATE_ARGUMENT( FOptionalSize, WidthOverride )
		/** When set this will display a check box on the notification; handles getting the current check box state */
		SLATE_ATTRIBUTE( ECheckBoxState, CheckBoxState )
		/** When set this will display a check box on the notification; handles setting the new check box state */
		SLATE_EVENT( FOnCheckStateChanged, CheckBoxStateChanged );
		/** Text to display for the check box message */
		SLATE_ATTRIBUTE( FText, CheckBoxText );
		/** When set this will display as a hyperlink on the right side of the notification. */
		SLATE_EVENT( FSimpleDelegate, Hyperlink)
		/** Text to display for the hyperlink (if Hyperlink is valid) */
		SLATE_ATTRIBUTE( FText, HyperlinkText );

	SLATE_END_ARGS()
	/**
	 * Constructs this widget
	 *
	 * @param InArgs    Declaration from which to construct the widget
	*/
	void Construct( const FArguments& InArgs )
	{
		CompletionState = CS_None;

		Text = InArgs._Text;
		FadeInDuration = InArgs._FadeInDuration;
		FadeOutDuration = InArgs._FadeOutDuration;
		ExpireDuration = InArgs._ExpireDuration;

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("NotificationList.ItemBackground"))
			.BorderBackgroundColor(this, &SNotificationItemImpl::GetContentColor)
			.ColorAndOpacity(this, &SNotificationItemImpl::GetContentColorRaw)
			.DesiredSizeScale(this, &SNotificationItemImpl::GetItemScale)
			[
				SNew(SBorder)
				.Padding( FMargin(5) )
				.BorderImage(FCoreStyle::Get().GetBrush("NotificationList.ItemBackground_Border"))
				.BorderBackgroundColor(this, &SNotificationItemImpl::GetGlowColor)
				[
					ConstructInternals(InArgs)
				]
			]
		];
	}
	
	/**
	 * Returns the internals of the notification
	 */
	TSharedRef<SHorizontalBox> ConstructInternals( const FArguments& InArgs ) 
	{
		CheckBoxStateChanged = InArgs._CheckBoxStateChanged;
		Hyperlink = InArgs._Hyperlink;
		HyperlinkText = InArgs._HyperlinkText;

		TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

		// Notification image
		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(10.f, 0.f, 0.f, 0.f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(SImage)
			.Image( InArgs._Image )
		];

		{
			FSlateFontInfo Font = InArgs._Font.Get();

			if (!Font.HasValidFont())
			{
				Font = InArgs._bUseLargeFont.Get()
					? FCoreStyle::Get().GetFontStyle(TEXT("NotificationList.FontBold"))
					: FCoreStyle::Get().GetFontStyle(TEXT("NotificationList.FontLight"));
			}

			// Container for the text and optional interactive widgets (buttons, check box, and hyperlink)
			TSharedRef<SVerticalBox> TextAndInteractiveWidgetsBox = SNew(SVerticalBox);

			HorizontalBox->AddSlot()
			.AutoWidth()
			.Padding(10.f, 0.f, 15.f, 0.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				TextAndInteractiveWidgetsBox
			];

			// Build Text box
			TextAndInteractiveWidgetsBox->AddSlot()
			.AutoHeight()
			[
				SNew(SBox)
				.WidthOverride(InArgs._WidthOverride)
				[
					SAssignNew(MyTextBlock, STextBlock)
					.Text(Text)
					.Font(Font)
					.AutoWrapText(InArgs._WidthOverride.IsSet()) // only auto-wrap the text if we've been given a size constraint; otherwise, fill the notification area
				]
			];

			TSharedRef<SHorizontalBox> InteractiveWidgetsBox = SNew(SHorizontalBox);
			TextAndInteractiveWidgetsBox->AddSlot()
			.AutoHeight()
			[
				InteractiveWidgetsBox
			];

			// Adds any buttons that were passed in.
			{
				TSharedRef<SHorizontalBox> ButtonsBox = SNew(SHorizontalBox);
				for (int32 idx = 0; idx < InArgs._ButtonDetails.Get().Num(); idx++)
				{
					FNotificationButtonInfo Button = InArgs._ButtonDetails.Get()[idx];

					ButtonsBox->AddSlot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.Text(Button.Text)
						.ToolTipText(Button.ToolTip)
						.OnClicked(this, &SNotificationItemImpl::OnButtonClicked, Button.Callback) 
						.Visibility( this, &SNotificationItemImpl::GetButtonVisibility, Button.VisibilityOnNone, Button.VisibilityOnPending, Button.VisibilityOnSuccess, Button.VisibilityOnFail )
					];
				}
				InteractiveWidgetsBox->AddSlot()
				.AutoWidth()
				.Padding(0.0f, 2.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					ButtonsBox
				];
			}

			// Adds a check box, but only visible when bound
			InteractiveWidgetsBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 2.0f, 4.0f, 0.0f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Visibility(this, &SNotificationItemImpl::GetCheckBoxVisibility)
				[
					SNew(SCheckBox)
					.IsChecked(InArgs._CheckBoxState)
					.OnCheckStateChanged(CheckBoxStateChanged)
					[
						SNew(STextBlock)
						.Text(InArgs._CheckBoxText)
					]
				]
			];

			// Adds a hyperlink, but only visible when bound
			InteractiveWidgetsBox->AddSlot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Visibility(this, &SNotificationItemImpl::GetHyperlinkVisibility)
				[
					SNew(SHyperlink)
					.Text(this, &SNotificationItemImpl::GetHyperlinkText)
					.OnNavigate(this, &SNotificationItemImpl::OnHyperlinkClicked)
				]
			];
		}

		if (InArgs._bUseThrobber.Get())
		{
			// Build pending throbber
			HorizontalBox->AddSlot()
			.AutoWidth()
			[
				SNew(SBox)
				.Padding(FMargin(5.f, 0.f, 10.f, 0.f))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Visibility( this, &SNotificationItemImpl::GetThrobberVisibility )
				[
					SNew(SThrobber)
				]
			];
		}

		if (InArgs._bUseSuccessFailIcons.Get())
		{
			// Build success/fail image
			HorizontalBox->AddSlot()
			.AutoWidth()
			[
				SNew(SBox)
				.Padding(FMargin(8.f, 0.f, 10.f, 0.f))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Visibility( this, &SNotificationItemImpl::GetSuccessFailImageVisibility )
				[
					SNew(SImage)
					.Image( this, &SNotificationItemImpl::GetSuccessFailImage )
				]
			];
		}

		return HorizontalBox;
	}

	/** Sets the text and delegate for the hyperlink */
	virtual void SetHyperlink( const FSimpleDelegate& InHyperlink, const TAttribute< FText >& InHyperlinkText = TAttribute< FText >() ) override
	{
		Hyperlink = InHyperlink;

		// Only replace the text if specified
		if ( InHyperlinkText.IsBound() )
		{
			HyperlinkText = InHyperlinkText;
		}
	}

protected:

	/* Used to determine whether the button is visible */
	EVisibility GetButtonVisibility( const EVisibility VisibilityOnNone, const EVisibility VisibilityOnPending, const EVisibility VisibilityOnSuccess, const EVisibility VisibilityOnFail ) const
	{
		switch ( CompletionState )
		{
		case SNotificationItem::CS_None: return VisibilityOnNone;
		case SNotificationItem::CS_Pending: return VisibilityOnPending;
		case SNotificationItem::CS_Success: return VisibilityOnSuccess;
		case SNotificationItem::CS_Fail: return VisibilityOnFail;
		default:
			check( false );
			return EVisibility::Visible;
		}
	}

	/* Used to determine whether the check box is visible */
	EVisibility GetCheckBoxVisibility() const
	{
		return CheckBoxStateChanged.IsBound() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	/* Used to determine whether the hyperlink is visible */
	EVisibility GetHyperlinkVisibility() const
	{
		return Hyperlink.IsBound() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	/* Used to fetch the text to display in the hyperlink */
	FText GetHyperlinkText() const
	{
		return HyperlinkText.Get();
	}

private:

	/* Used as a wrapper for the callback, this means any code calling it does not require access to FReply type */
	FReply OnButtonClicked(FSimpleDelegate InCallback)
	{
		InCallback.ExecuteIfBound();
		return FReply::Handled();
	}

	/* Execute the delegate for the hyperlink, if bound */
	void OnHyperlinkClicked() const
	{
		Hyperlink.ExecuteIfBound();
	}

protected:

	/** When set this will display a check box on the notification; handles setting the new check box state */
	FOnCheckStateChanged CheckBoxStateChanged;

	/** When set this will display as a hyperlink on the right side of the notification. */
	FSimpleDelegate Hyperlink;

	/** Text to display for the hyperlink message */
	TAttribute< FText > HyperlinkText;
};

/////////////////////////////////////////////////
// SNotificationItemExternalImpl

/** A single line in the event message list with the actual content provided by the client */
class SNotificationItemExternalImpl : public SNotificationItemImpl
{
public:
	SLATE_BEGIN_ARGS(SNotificationItemExternalImpl)
		: _FadeInDuration(0.5f)
		, _FadeOutDuration(2.f)
		, _ExpireDuration(1.f)
	{}

		/** The fade in duration for this element */
		SLATE_ATTRIBUTE(float, FadeInDuration)
		/** The fade out duration for this element */
		SLATE_ATTRIBUTE(float, FadeOutDuration)
		/** The duration before a fadeout for this element */
		SLATE_ATTRIBUTE(float, ExpireDuration)
		/** The widget that provides the notification to display */
		SLATE_ARGUMENT(TSharedPtr<INotificationWidget>, ContentWidget)

	SLATE_END_ARGS()

	/**
	* Constructs this widget
	*
	* @param InArgs    Declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs)
	{
		check(InArgs._ContentWidget.IsValid());

		FadeInDuration = InArgs._FadeInDuration;
		FadeOutDuration = InArgs._FadeOutDuration;
		ExpireDuration = InArgs._ExpireDuration;
		NotificationWidget = InArgs._ContentWidget;

		ChildSlot
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
				.BorderBackgroundColor(this, &SNotificationItemExternalImpl::GetContentColor)
				.ColorAndOpacity(this, &SNotificationItemExternalImpl::GetContentColorRaw)
				.DesiredSizeScale(this, &SNotificationItemExternalImpl::GetItemScale)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						NotificationWidget->AsWidget()
					]
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.Padding(0)
						.BorderImage(FCoreStyle::Get().GetBrush("NotificationList.ItemBackground_Border_Transparent"))
						.BorderBackgroundColor(this, &SNotificationItemExternalImpl::GetGlowColor)
						.Visibility(EVisibility::SelfHitTestInvisible)
					]
				]
			];
	}

	/** SNotificationItem interface */
	virtual void SetCompletionState(ECompletionState State) override
	{
		SNotificationItemImpl::SetCompletionState(State);

		NotificationWidget->OnSetCompletionState(State);
	}

private:
	TSharedPtr<INotificationWidget> NotificationWidget;
};

///////////////////////////////////////////////////
//// SNotificationList
TSharedRef<SNotificationItem> SNotificationList::AddNotification(const FNotificationInfo& Info)
{
	TSharedPtr<SNotificationExtendable> NewItem;

	if (FSlateNotificationManager::Get().AreNotificationsAllowed())
	{
		if (Info.ContentWidget.IsValid())
		{
			NewItem = SNew(SNotificationItemExternalImpl)
				.ContentWidget(Info.ContentWidget)
				.FadeInDuration(Info.FadeInDuration)
				.ExpireDuration(Info.ExpireDuration)
				.FadeOutDuration(Info.FadeOutDuration);
		}
		else
		{
			static const FSlateBrush* CachedImage = FCoreStyle::Get().GetBrush("NotificationList.DefaultMessage");

			// Create notification.
			NewItem = SNew(SNotificationItemImpl)
				.Text(Info.Text)
				.Font(Font)
				.ButtonDetails(Info.ButtonDetails)
				.Image((Info.Image != nullptr) ? Info.Image : CachedImage)
				.FadeInDuration(Info.FadeInDuration)
				.ExpireDuration(Info.ExpireDuration)
				.FadeOutDuration(Info.FadeOutDuration)
				.bUseThrobber(Info.bUseThrobber)
				.bUseSuccessFailIcons(Info.bUseSuccessFailIcons)
				.bUseLargeFont(Info.bUseLargeFont)
				.WidthOverride(Info.WidthOverride)
				.CheckBoxState(Info.CheckBoxState)
				.CheckBoxStateChanged(Info.CheckBoxStateChanged)
				.CheckBoxText(Info.CheckBoxText)
				.Hyperlink(Info.Hyperlink)
				.HyperlinkText(Info.HyperlinkText);
		}
		
		NewItem->MyList = SharedThis(this);

		MessageItemBoxPtr->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				NewItem.ToSharedRef()
			];

		NewItem->Fadein( Info.bAllowThrottleWhenFrameRateIsLow );

		if (Info.bFireAndForget)
		{
			NewItem->ExpireAndFadeout();
		}
	}
	else
	{
		// When notifications are not allowed we want to return an empty notification.
		NewItem = SNew(SNotificationItemImpl);
	}

	LastNotification = NewItem;

	return NewItem.ToSharedRef();
}

void SNotificationList::NotificationItemFadedOut (const TSharedRef<SNotificationItem>& NotificationItem)
{
	if( ParentWindowPtr.IsValid() )
	{
		// If we are in a single-window per notification situation, we dont want to 
		// remove the NotificationItem straight away, rather we will flag us as done
		// and wait for the FSlateNotificationManager to release the parent window.
		bDone = true;
	}
	else
	{
		// This should remove the last non-local reference to this SNotificationItem.
		// Since there may be many local references on the call stack we are not checking
		// if the reference is unique.
		MessageItemBoxPtr->RemoveSlot(NotificationItem);
	}
}

void SNotificationList::Construct(const FArguments& InArgs)
{
	bDone = false;
	Font = InArgs._Font;

	ChildSlot
	[
		SAssignNew(MessageItemBoxPtr, SVerticalBox)
	];
}
