// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class FPaintArgs;
class FSlateWindowElementList;

/**
 * A virtual joystsick
 */
class SLATE_API SVirtualJoystick : public SLeafWidget
{

public:
	/** The settings and current state of each zone we render */ 
	struct FControlInfo
	{
		FControlInfo()
		{
			// default to all 0
			FMemory::Memzero(this, sizeof(*this));
			CapturedPointerIndex = -1;
			InputScale = FVector2D(1.f, 1.f);
		}

		// Set by the game

		/** The brush to use to draw the background for joysticks, or unclicked for buttons */
		TSharedPtr< FSlateDynamicImageBrush > Image1;

		/** The brush to use to draw the thumb for joysticks, or clicked for buttons */
		TSharedPtr< FSlateDynamicImageBrush > Image2;

		/** The actual center of the control */
		FVector2D Center;

		/** The size of a joystick that can be re-centered within InteractionSize area */
		FVector2D VisualSize;

		/** The size of the thumb that can be re-centered within InteractionSize area */
		FVector2D ThumbSize;

		/** The size of a the interactable area around Center */
		FVector2D InteractionSize;

		/** The scale for control input */
		FVector2D InputScale;

		/** The input to send from this control (for sticks, this is the horizontal/X input) */ 
		FKey MainInputKey;

		/** The secondary input (for sticks, this is the vertical/Y input, unused for buttons) */ 
		FKey AltInputKey;

		/** Positioned center in viewport */
		FVector2D PositionedCenter;

	private:
		friend SVirtualJoystick;

		/**
		 * Reset the control to a centered/inactive state
		 */
		void Reset();

		// Current state

		/** The position of the thumb, in relation to the VisualCenter */
		FVector2D ThumbPosition;

		/** For recentered joysticks, this is the re-center location */
		FVector2D VisualCenter;

		/** The corrected actual center of the control */
		FVector2D CorrectedCenter;

		/** The corrected size of a joystick that can be re-centered within InteractionSize area */
		FVector2D CorrectedVisualSize;

		/** The corrected size of the thumb that can be re-centered within InteractionSize area */
		FVector2D CorrectedThumbSize;

		/** The corrected size of a the interactable area around Center */
		FVector2D CorrectedInteractionSize;

		/** The corrected scale for control input */
		FVector2D CorrectedInputScale;

		/** Which pointer index is interacting with this control right now, or -1 if not interacting */
		int32 CapturedPointerIndex;

		/** Time to activate joystick **/
		float ElapsedTime;

		/** Visual center to be updated */
		FVector2D NextCenter;

		/** Whether or not to send one last "release" event next tick */
		bool bSendOneMoreEvent;

		/** Whether or not we need position the control against the geometry */
		bool bHasBeenPositioned;

		/** Whether or not to update center position */
		bool bNeedUpdatedCenter;
	};

	SLATE_BEGIN_ARGS( SVirtualJoystick )
		{}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/**
	 * Static function to return if external users should create/activate/etc a touch interface
	 * Note that this function is also used internally, so even if this returns false but an SVirtualJoystick
	 * is created, it won't actually show any controls
	 */
	static bool ShouldDisplayTouchInterface();

	/**
	 * Shows or hides the controls (for instance during cinematics
	 */
	void SetJoystickVisibility(const bool bVisible, const bool bFade);

	void AddControl(const FControlInfo& Control)
	{
		Controls.Add(Control);
	}

	void ClearControls()
	{
		Controls.Empty();
	}

	void SetControls(const TArray<FControlInfo>& InControls)
	{
		Controls = InControls;
	}

	/**
	 * Sets parameters that control all controls
	 */
	void SetGlobalParameters(float InActiveOpacity, float InInactiveOpacity, float InTimeUntilDeactive, float InTimeUntilReset, float InActivationDelay, bool InbPreventReCenter, float InStartupDelay);

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	
	virtual FVector2D ComputeDesiredSize(float) const override;

	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& Event) override;
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& Event) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& Event) override;

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	virtual bool SupportsKeyboardFocus() const override;

protected:
	/** Callback for handling display metrics changes. */
	virtual void HandleDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetric);

	void AlignBoxIntoScreen(FVector2D& Position, const FVector2D& Size, const FVector2D& ScreenSize);

	/**
	 * Process a touch event (on movement and possibly on initial touch)
	 *
	 * @return true if the touch was successful
	 */
	virtual bool HandleTouch(int32 ControlIndex, const FVector2D& LocalCoord, const FVector2D& ScreenSize);

	/** 
	 * Return the target opacity to lerp to given the current state
	 */
	FORCEINLINE float GetBaseOpacity();

	/** List of controls set by the UTouchInterface */
	TArray<FControlInfo> Controls;

	/** Global settings from the UTouchInterface */
	float ActiveOpacity;
	float InactiveOpacity;
	float TimeUntilDeactive;
	float TimeUntilReset;
	float ActivationDelay;
	float StartupDelay;

	enum EVirtualJoystickState
	{
		State_Active,
		State_CountingDownToInactive,
		State_CountingDownToReset,
		State_Inactive,
		State_WaitForStart,
		State_CountingDownToStart,
	};

	/** The current state of all controls */
	EVirtualJoystickState State;

	/** True if the joystick should be visible */
	uint32 bVisible:1;

	/** If true, this zone will have it's "center" set when you touch it, otherwise the center will be set to the center of the zone */
	uint32 bCenterOnEvent:1;

	/** If true, ignore re-centering */
	uint32 bPreventReCenter:1;

	/** Target opacity */
	float CurrentOpacity;

	/* Countdown until next state change */
	float Countdown;
};
