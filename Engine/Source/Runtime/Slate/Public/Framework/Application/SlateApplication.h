// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "InputCoreTypes.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/IMenu.h"
#include "Layout/Visibility.h"
#include "GenericPlatform/GenericWindow.h"
#include "Styling/SlateColor.h"
#include "Layout/SlateRect.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "GenericPlatform/GenericApplication.h"
#include "Input/Events.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "Application/SlateWindowHelper.h"
#include "Rendering/SlateRenderer.h"
#include "Application/SlateApplicationBase.h"
#include "Application/ThrottleManager.h"
#include "Widgets/IToolTip.h"
#include "Layout/WidgetPath.h"
#include "Logging/IEventLogger.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/SlateDelegates.h"

#include "GestureDetector.h"

class FNavigationConfig;
class IInputInterface;
class IInputProcessor;
class IPlatformTextField;
class ISlateSoundDevice;
class ITextInputMethodSystem;
class IVirtualKeyboardEntry;
class IWidgetReflector;
class SViewport;

/** A Delegate for querying whether source code access is possible */
DECLARE_DELEGATE_RetVal(bool, FQueryAccessSourceCode);

/** Delegates for when modal windows open or close */
DECLARE_DELEGATE(FModalWindowStackStarted)
DECLARE_DELEGATE(FModalWindowStackEnded)

/** Delegate for when window action occurs (ClickedNonClientArea, Maximize, Restore, WindowMenu). Return true if the OS layer should stop processing the action. */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnWindowAction, const TSharedRef<FGenericWindow>&, EWindowAction::Type);

DECLARE_DELEGATE_RetVal(bool, FDragDropCheckingOverride);


/** Allow widgets to find out when someone clicked outside them. Currently needed by MenuAnchros. */
class SLATE_API FPopupSupport
{
	public:

	/**
	 * Given a WidgetPath that was clicked, send notifications to any subscribers that were not under the mouse.
	 * i.e. Send the "Someone clicked outside me" notifications.
	 */
	void SendNotifications( const FWidgetPath& WidgetsUnderCursor );

	/**
	* Register for a notification when the user clicks outside a specific widget.
	*
	* @param NotifyWhenClickedOutsideMe    When the user clicks outside this widget, fire a notification.
	* @param InNotification                The notification to invoke.
	*/
	FDelegateHandle RegisterClickNotification(const TSharedRef<SWidget>& NotifyWhenClickedOutsideMe, const FOnClickedOutside& InNotification);

	/**
	* NOTE: Only necessary if notification no longer desired.
	*       Stale notifications are cleaned up automatically.
	*
	* Unregister the notification because it is no longer desired.
	*/
	void UnregisterClickNotification(FDelegateHandle InHandle);

	private:

	/** A single subscription about clicks happening outside the widget. */
	struct FClickSubscriber
	{
		FClickSubscriber( const TSharedRef<SWidget>& DetectClicksOutsideThisWidget, const FOnClickedOutside& InNotification )
		: DetectClicksOutsideMe( DetectClicksOutsideThisWidget )
		, Notification( InNotification )
		{}

		bool ShouldKeep() const
		{
			return DetectClicksOutsideMe.IsValid() && Notification.IsBound();
		}

		/** If a click occurs outside this widget, we'll send the notification */
		TWeakPtr<SWidget> DetectClicksOutsideMe;
		/** Notification to send */
		FOnClickedOutside Notification;
	};

	/** List of subscriptions that want to be notified when the user clicks outside a certain widget. */
	TArray<FClickSubscriber> ClickZoneNotifications;
};



/**
 * A representation of a slate input providing user.  We allocate a slate user as new input sources are
 * discovered.
 */
class SLATE_API FSlateUser
{
public:
	FSlateUser(int32 InUserIndex, bool InVirtualUser);
	virtual ~FSlateUser();

	FORCEINLINE int32 GetUserIndex() const { return UserIndex; }
	FORCEINLINE bool IsVirtualUser() const { return bVirtualUser; }

	TSharedPtr<SWidget> GetFocusedWidget() const;

	FORCEINLINE uint64 GetFocusVersion() const { return FocusVersion; }
	FORCEINLINE void UpdateFocusVersion() { FocusVersion++; }

	FGestureDetector GestureDetector;

	TSharedPtr<FNavigationConfig> NavigationConfig;

private:
	FORCEINLINE bool HasValidFocusPath() const
	{
		return FocusWidgetPathWeak.IsValid();
	}

	FORCEINLINE const FWeakWidgetPath& GetWeakFocusPath() const
	{
		return FocusWidgetPathWeak;
	}

	FORCEINLINE TSharedRef<FWidgetPath> GetFocusPath() const
	{
		if ( !FocusWidgetPathStrong.IsValid() )
		{
			FocusWidgetPathStrong = FocusWidgetPathWeak.ToWidgetPathRef();
		}

		return FocusWidgetPathStrong.ToSharedRef();
	}

	void SetFocusPath(const FWidgetPath& InWidgetPath, EFocusCause InCause, bool InShowFocus);

	void FinishFrame();

private:
	/** The index the user was assigned. */
	int32 UserIndex;

	/** Is this a virtual user?  Virtual users are generally ignored in most operations that affect all users. */
	bool bVirtualUser;

	/** A weak path to the widget currently focused by a user, if any. */
	FWeakWidgetPath FocusWidgetPathWeak;

	/** A strong widget path to a widget, this is cleared after the end of pumping messages. */
	mutable TSharedPtr<FWidgetPath> FocusWidgetPathStrong;

	/** Reason a widget was focused by a user, if any. */
	EFocusCause FocusCause;

	/** If we should show this focus */
	bool ShowFocus;

	/**
	 * The FocusVersion is used to know if the focus state is modified for a user while processing focus
	 * events, that way upon returning from focus calls, we know if we should abandon the remainder of the event.
	 */
	int32 FocusVersion;

	friend class FSlateApplication;
};

/**
 * Represents a virtual user of slate.
 */
class SLATE_API FSlateVirtualUser
{
public:
	FSlateVirtualUser(int32 InUserIndex, int32 InVirtualUserIndex);
	virtual ~FSlateVirtualUser();

	FORCEINLINE int32 GetUserIndex() const { return UserIndex; }
	FORCEINLINE int32 GetVirtualUserIndex() const { return VirtualUserIndex; }

private:

	/** The index the user was assigned. */
	int32 UserIndex;

	/** The virtual index the user was assigned. */
	int32 VirtualUserIndex;
};

enum class ESlateTickType : uint8
{
	/** Tick time only */
	TimeOnly,

	/** Update time, tick and paint widgets, and process input */
	All,
};

class SLATE_API FSlateApplication
	: public FSlateApplicationBase
	, public FGenericApplicationMessageHandler
{
public:

	/** Virtual destructor. */
	virtual ~FSlateApplication();

public:

	/**
	 * Returns the running average delta time (smoothed over several frames)
	 *
	 * @return  The average time delta
	 */
	const float GetAverageDeltaTime() const
	{
		return AverageDeltaTime;
	}

	/**
	 * Returns the real time delta since Slate last ticked widgets
	 *
	 * @return  The time delta since last tick
	 */
	const float GetDeltaTime() const
	{
		return (float)( CurrentTime - LastTickTime );
	}

	/**
	 * Returns the running average delta time (smoothed over several frames)
	 * Unlike GetAverageDeltaTime() it excludes exceptional
	 * situations, such as when throttling mode is active.
	 *
	 * @return  The average time delta
	 */
	float GetAverageDeltaTimeForResponsiveness() const
	{
		return AverageDeltaTimeForResponsiveness;
	}

public:

	static void Create();
	static TSharedRef<FSlateApplication> Create(const TSharedRef<class GenericApplication>& InPlatformApplication);

	static TSharedRef<FSlateApplication> InitializeAsStandaloneApplication(const TSharedRef< class FSlateRenderer >& PlatformRenderer);
	
	static TSharedRef<FSlateApplication> InitializeAsStandaloneApplication(const TSharedRef< class FSlateRenderer >& PlatformRenderer, const TSharedRef<class GenericApplication>& InPlatformApplication);

	/**
	 * Returns true if a Slate application instance is currently initialized and ready
	 *
	 * @return  True if Slate application is initialized
	 */
	static bool IsInitialized()
	{
		return CurrentApplication.IsValid();
	}

	/**
	 * Returns the current instance of the application. The application should have been initialized before
	 * this method is called
	 *
	 * @return  Reference to the application
	 */
	static FSlateApplication& Get()
	{
		check( IsInGameThread() || IsInSlateThread() );
		return *CurrentApplication;
	}

	static void Shutdown(bool bShutdownPlatform = true);

	/** @return the global tab manager */
	static TSharedRef<class FGlobalTabmanager> GetGlobalTabManager();

	/** @return the root style node, which is the entry point to the style graph representing all the current style rules. */
	const class FStyleNode* GetRootStyle() const;

	/**
	 * Initializes the renderer responsible for drawing all elements in this application
	 *
	 * @param InRenderer The renderer to use.
	 * @param bQuietMode Don't show any message boxes when initialization fails.
	 */
	virtual bool InitializeRenderer( TSharedRef<FSlateRenderer> InRenderer, bool bQuietMode = false );

	/** Set the slate sound provider that the slate app should use. */
	virtual void InitializeSound( const TSharedRef<ISlateSoundDevice>& InSlateSoundDevice );

	void DestroyRenderer();

	/** Play SoundToPlay. Interrupt previous sound if one is playing. */
	void PlaySound( const FSlateSound& SoundToPlay, int32 UserIndex = 0 ) const;

	/** @return The duration of the given sound resource */
	float GetSoundDuration(const FSlateSound& Sound) const;

	IInputInterface* GetInputInterface() const { return PlatformApplication->GetInputInterface(); }

	/** @return Whether or not the current platform supports system help */
	bool SupportsSystemHelp() const { return PlatformApplication->SupportsSystemHelp(); }

	void ShowSystemHelp() { PlatformApplication->ShowSystemHelp(); }

	/** @return The text input method interface for this application */
	ITextInputMethodSystem *GetTextInputMethodSystem() const { return PlatformApplication->GetTextInputMethodSystem(); }

	/** 
	 * Sets the position of the cursor.
	 *
	 * @param MouseCoordinate		The new position.
	 */
	void SetCursorPos( const FVector2D& MouseCoordinate );

	/** Polls game devices for input */
	void PollGameDeviceState();

	/** Occurs before Tick(), after all pointer and keyboard input has been processed. */
	void FinishedInputThisFrame();

	/** Ticks this application */
	void Tick(ESlateTickType TickType = ESlateTickType::All);

	/** Pumps OS messages when a modal window or intra-frame debugging session exists */
	void PumpMessages();

	/** Returns true if this slate application is ready to open modal windows */
	bool CanAddModalWindow() const;

	/** Returns true if this slate application is ready to display windows. */
	bool CanDisplayWindows() const;

	virtual EUINavigation GetNavigationDirectionFromKey(const FKeyEvent& InKeyEvent) const override;
	virtual EUINavigation GetNavigationDirectionFromAnalog(const FAnalogInputEvent& InAnalogEvent) override;

	/**
	 * Adds a modal window to the application.  
	 * In most cases, this function does not return until the modal window is closed (the only exception is a modal window for slow tasks)  
	 *
	 * @param InSlateWindow		A SlateWindow to which to add a native window.
	 * @param InParentWindow	The parent of the modal window.  All modal windows must have a parent.
	 * @param bSlowTaskWindow	true if the window is for a slow task and this function should return before the window is closed
	 */
	void AddModalWindow( TSharedRef<SWindow> InSlateWindow, const TSharedPtr<const SWidget> InParentWidget, bool bSlowTaskWindow = false );

	/** Sets the delegate for when a modal window stack begins */
	void SetModalWindowStackStartedDelegate(FModalWindowStackStarted StackStartedDelegate);

	/** Sets the delegate for when a modal window stack ends */
	void SetModalWindowStackEndedDelegate(FModalWindowStackEnded StackEndedDelegate);

	/**
	 * Associates a top level Slate Window with a native window, and "natively" parents that window to the specified Slate window.
	 * Although the window still a top level window in Slate, it will be considered a child window to the operating system.
	 *
	 * @param	InSlateWindow		A Slate window to which to add a native window.
	 * @param	InParentWindow		Slate window that the window being added should be a native child of
	 * @param	bShowImmediately	True to show the window.  Pass false if you're going to call ShowWindow() yourself later.
	 *
	 * @return a reference to the SWindow that was just added.
	 */
	TSharedRef<SWindow> AddWindowAsNativeChild( TSharedRef<SWindow> InSlateWindow, TSharedRef<SWindow> InParentWindow, const bool bShowImmediately = true );

	/**
	 * Creates a new Menu and adds it to the menu stack.
	 * Menus are always auto-sized. Use fixed-size content if a fixed size is required.
	 *
	 * @param InParentWidget		The parent of the menu. If the stack isn't empty, PushMenu will attempt to determine the stack level for the new menu by looking of an open menu in the parent's path.
	 * @param InOwnerPath			Optional full widget path of the parent if one is available. If an invalid path is given PushMenu will attempt to generate a path to the InParentWidget
	 * @param InContent				The content to be placed inside the new menu
	 * @param SummonLocation		The location where this menu should be summoned
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param bFocusImmediately		Should the popup steal focus when shown?
	 * @param SummonLocationSize	An optional rect which describes an area in which the menu may not appear
	 * @param Method				An optional popup method override. If not set, the widgets in the InOwnerPath will be queried for this.
	 * @param bIsCollapsedByParent	Is this menu collapsed when a parent menu receives focus/activation? If false, only focus/activation outside the entire stack will auto collapse it.
	 */
	TSharedPtr<IMenu> PushMenu(const TSharedRef<SWidget>& InParentWidget, const FWidgetPath& InOwnerPath, const TSharedRef<SWidget>& InContent, const FVector2D& SummonLocation, const FPopupTransitionEffect& TransitionEffect, const bool bFocusImmediately = true, const FVector2D& SummonLocationSize = FVector2D::ZeroVector, TOptional<EPopupMethod> Method = TOptional<EPopupMethod>(), const bool bIsCollapsedByParent = true);

	/**
	 * Creates a new Menu and adds it to the menu stack under the specified parent menu.
	 * Menus are always auto-sized. Use fixed-size content if a fixed size is required.
	 *
	 * @param InParentMenu			The parent of the menu. Must be a valid menu in the stack.
	 * @param InContent				The content to be placed inside the new menu
	 * @param SummonLocation		The location where this menu should be summoned
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param bFocusImmediately		Should the popup steal focus when shown?
	 * @param SummonLocationSize	An optional rect which describes an area in which the menu may not appear
	 * @param bIsCollapsedByParent	Is this menu collapsed when a parent menu receives focus/activation? If false, only focus/activation outside the entire stack will auto collapse it.
	 */
	TSharedPtr<IMenu> PushMenu(const TSharedPtr<IMenu>& InParentMenu, const TSharedRef<SWidget>& InContent, const FVector2D& SummonLocation, const FPopupTransitionEffect& TransitionEffect, const bool bFocusImmediately = true, const FVector2D& SummonLocationSize = FVector2D::ZeroVector, const bool bIsCollapsedByParent = true);

	/**
	 * Creates a new hosted Menu and adds it to the menu stack.
	 * Hosted menus are drawn by an external host widget.
	 * 
	 * @param InParentMenu			The parent of the menu. Must be a valid menu in the stack.
	 * @param InOwnerPath			Optional full widget path of the parent if one is available. If an invalid path is given PushMenu will attempt to generate a path to the InParentWidget
	 * @param InMenuHost			The host widget that draws the menu's content
	 * @param InContent				The content to be placed inside the new menu
	 * @param OutWrappedContent		Returns the InContent wrapped with widgets needed by the menu stack system. This is what should be drawn by the host after this call.
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param ShouldThrottle		Should we throttle engine ticking to maximize the menu responsiveness
	 * @param bIsCollapsedByParent	Is this menu collapsed when a parent menu receives focus/activation? If false, only focus/activation outside the entire stack will auto collapse it.
	 */
	TSharedPtr<IMenu> PushHostedMenu(const TSharedRef<SWidget>& InParentWidget, const FWidgetPath& InOwnerPath, const TSharedRef<IMenuHost>& InMenuHost, const TSharedRef<SWidget>& InContent, TSharedPtr<SWidget>& OutWrappedContent, const FPopupTransitionEffect& TransitionEffect, EShouldThrottle ShouldThrottle, const bool bIsCollapsedByParent = true);
	
	/**
	 * Creates a new hosted child Menu and adds it to the menu stack under the specified parent menu.
	 * Hosted menus are drawn by an external host widget.
	 * 
	 * @param InParentMenu			The parent menu for this menu
	 * @param InMenuHost			The host widget that draws the menu's content
	 * @param InContent				The menu's content
	 * @param OutWrappedContent		Returns the InContent wrapped with widgets needed by the menu stack system. This is what should be drawn by the host after this call.
	 * @param TransitionEffect		Animation to use when the popup appears
	 * @param ShouldThrottle		Should we throttle engine ticking to maximize the menu responsiveness
	 * @param bIsCollapsedByParent	Is this menu collapsed when a parent menu receives focus/activation? If false, only focus/activation outside the entire stack will auto collapse it.
	 */	
	TSharedPtr<IMenu> PushHostedMenu(const TSharedPtr<IMenu>& InParentMenu, const TSharedRef<IMenuHost>& InMenuHost, const TSharedRef<SWidget>& InContent, TSharedPtr<SWidget>& OutWrappedContent, const FPopupTransitionEffect& TransitionEffect, EShouldThrottle ShouldThrottle, const bool bIsCollapsedByParent = true);

	/** @return Returns whether the menu has child menus. */
	bool HasOpenSubMenus(TSharedPtr<IMenu> InMenu) const;

	/** @return	Returns true if there are any pop-up menus summoned */
	virtual bool AnyMenusVisible() const override;

	/**
	 * Attempt to locate a menu that contains the specified widget
	 *
	 * @param InWidgetPath Path to the widget to use for the search
	 * @return the menu in which the widget resides, or nullptr
	 */
	TSharedPtr<IMenu> FindMenuInWidgetPath(const FWidgetPath& InWidgetPath) const;

	/** @return	Returns a ptr to the window that is currently the host of the menu stack or null if no menus are visible */
	TSharedPtr<SWindow> GetVisibleMenuWindow() const;

	/** Dismisses all open menus */
	void DismissAllMenus();

	/**
	 * Dismisses a menu and all its children
	 *
	 * @param InFromMenu	The menu to dismiss, any children, grandchildren etc will also be dismissed
	 */
	void DismissMenu(const TSharedPtr<IMenu>& InFromMenu);

	/**
	 * Dismisses a menu and all its children. The menu is determined by looking for menus in the parent chain of the widget.
	 *
	 * @param InWidgetInMenu	The widget whose path is search upwards for a menu. That menu will then be dismissed.
	 */
	void DismissMenuByWidget(const TSharedRef<SWidget>& InWidgetInMenu);

	/**
	 * HACK: Don't use this unless shutting down a game viewport
	 * Game viewport windows need to be destroyed instantly or else the viewport could tick and access deleted data
	 *
	 * @param WindowToDestroy		Window for destruction
	 */
	void DestroyWindowImmediately( TSharedRef<SWindow> WindowToDestroy );

	/**
	 * Disable Slate components when an external, non-slate, modal window is brought up.  In the case of multiple
	 * external modal windows, we will only increment our tracking counter.
	 */
	void ExternalModalStart();

	/**
	 * Re-enable disabled Slate components when a non-slate modal window is dismissed.  Slate components
	 * will only be re-enabled when all tracked external modal windows have been dismissed.
	 */
	void ExternalModalStop();

	/** Event before slate application ticks. */
	DECLARE_EVENT_OneParam(FSlateApplication, FSlateTickEvent, float);
	FSlateTickEvent& OnPreTick()  { return PreTickEvent; }

	/** Event after slate application ticks. */
	FSlateTickEvent& OnPostTick()  { return PostTickEvent; }

	/** 
	 * Removes references to FViewportRHI's.  
	 * This has to be done explicitly instead of using the FRenderResource mechanism because FViewportRHI's are managed by the game thread.
	 * This is needed before destroying the RHI device. 
	 */
	void InvalidateAllViewports();

	/**
	 * Registers a game viewport with the Slate application so that specific messages can be routed directly to a viewport
	 * 
	 * @param InViewport	The viewport to register.  Note there is currently only one registered viewport
	 */
	void RegisterGameViewport( TSharedRef<SViewport> InViewport );

	/**
	 * Registers a viewport with the Slate application so that specific messages can be routed directly to a viewport
	 * This is for all viewports, there can be multiple of these as opposed to the singular "Game Viewport"
	 * 
	 * @param InViewport	The viewport to register.  Note there is currently only one registered viewport
	 */
	void RegisterViewport(TSharedRef<SViewport> InViewport);

	/**
	 * Returns the game viewport registered with the slate application
	 *
	 * @return registered game viewport
	 */
	TSharedPtr<SViewport> GetGameViewport() const;

	/** 
	 * Unregisters the current game viewport from Slate.  This method sends a final deactivation message to the viewport
	 * to allow it to do a final cleanup before being closed.
	 */
	void UnregisterGameViewport();

	/**
	 * Register another window that may be visible in a non-top level way that still needs to be able to maintain focus paths.
	 * Generally speaking - this is for Virtual Windows that are created to render in the 3D world slate content.
	 */
	void RegisterVirtualWindow(TSharedRef<SWindow> InWindow);

	/**
	 * Unregister a virtual window.
	 */
	void UnregisterVirtualWindow(TSharedRef<SWindow> InWindow);

	/**
	 * Flushes the render state of slate, releasing accesses and flushing all render commands.
	 */
	void FlushRenderState();

	/**
	 * Sets specified user focus to the SWidget representing the currently active game viewport
	 */
	void SetUserFocusToGameViewport(uint32 UserIndex, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/**
	 * Sets all users focus to the SWidget representing the currently active game viewport
	 */
	void SetAllUserFocusToGameViewport(EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/**
	 * Activates the Game Viewport if it is properly childed under a window
	 */
	void ActivateGameViewport();

	/**
	 * Sets specified user focus to the SWidget passed in.
	 *
	 * @param UserIndex Index of the user to change focus for
	 * @param WidgetToFocus the widget to set focus to
	 * @param ReasonFocusIsChanging the contextual reason for the focus change
	 */
	bool SetUserFocus(uint32 UserIndex, const TSharedPtr<SWidget>& WidgetToFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/**
	 * Sets focus for all users to the SWidget passed in.
	 *
	 * @param WidgetToFocus the widget to set focus to
	 * @param ReasonFocusIsChanging the contextual reason for the focus change
	 */
	void SetAllUserFocus(const TSharedPtr<SWidget>& WidgetToFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/** Releases the users focus from whatever it currently is on. */
	void ClearUserFocus(uint32 UserIndex, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/** Releases the focus for all users from whatever it currently is on. */
	void ClearAllUserFocus(EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/**
	 * Sets the Keyboard focus to the specified SWidget
	 */
	bool SetKeyboardFocus(const TSharedPtr<SWidget>& OptionalWidgetToFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

	/**
	 * Clears keyboard focus, if any widget is currently focused
	 *
	 * @param ReasonFocusIsChanging The reason that keyboard focus is changing
	 */
	void ClearKeyboardFocus(const EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly);

#if WITH_EDITOR
	/**
	* Gets a delegate that is invoked before the input key get process by slate widgets bubble system.
	* Its read only and you cannot mark the input as handled.
	*/
	DECLARE_EVENT_OneParam(FSlateApplication, FOnApplicationPreInputKeyDownListener, const FKeyEvent&);
	FOnApplicationPreInputKeyDownListener& OnApplicationPreInputKeyDownListener() { return OnApplicationPreInputKeyDownListenerEvent; }

	/**
	* Gets a delegate that is invoked before the mouse input button down get process by slate widgets bubble system.
	* Its read only and you cannot mark the input as handled.
	*/
	DECLARE_EVENT_OneParam(FSlateApplication, FOnApplicationMousePreInputButtonDownListener, const FPointerEvent&);
	FOnApplicationMousePreInputButtonDownListener& OnApplicationMousePreInputButtonDownListener() { return OnApplicationMousePreInputButtonDownListenerEvent; }

	/** Gets a delegate that is invoked in the editor when a windows dpi scale changes */
	DECLARE_EVENT_OneParam(FSlateApplication, FOnWindowDPIScaleChanged, TSharedRef<SWindow>);
	FOnWindowDPIScaleChanged& OnWindowDPIScaleChanged() { return OnWindowDPIScaleChangedEvent; }
#endif //WITH_EDITOR

	/**
	 * Returns the current modifier keys state
	 *
	 * @return  State of modifier keys
	 */
	FModifierKeysState GetModifierKeys() const;

	/**
	 *	Restores all input settings to their original values
	 * 
	 *  Clears all user focus
	 *  Shows the mouse, clears any locks and captures, turns off high precision input
	 */
	void ResetToDefaultInputSettings();
	
	/**
	 *	Restores all pointer input settings to their original values
	 * 
	 *  Shows the mouse, clears any locks and captures, turns off high precision input
	 */
	void ResetToDefaultPointerInputSettings();

	/**
	 * Mouse capture
	 */

	/** returning platform-specific value designating window that captures mouse, or nullptr if mouse isn't captured */
	virtual void* GetMouseCaptureWindow() const;

	/** Releases the mouse capture from whatever it currently is on - for all users for all pointers. */
	void ReleaseMouseCapture();

	/** Releases the mouse capture from whatever it currently is on for a particular user. */
	void ReleaseMouseCaptureForUser(int32 UserIndex);

	/** @return The active modal window or nullptr if there is no modal window. */
	TSharedPtr<SWindow> GetActiveModalWindow() const;

	/**
	 * Assign a delegate to be called when this application is requesting an exit (e.g. when the last window is closed).
	 *
	 * @param OnExitRequestHandler  Function to execute when the application wants to quit
	 */
	void SetExitRequestedHandler( const FSimpleDelegate& OnExitRequestedHandler );
		
	/**
	 * @todo slate: Remove this method or make it private.
	 * Searches for the specified widget and generates a full path to it.  Note that this is
	 * a relatively slow operation!  It can fail, in which case OutWidgetPath.IsValid() will be false.
	 * 
	 * @param  InWidget       Widget to generate a path to
	 * @param  OutWidgetPath  The generated widget path
	 * @param  VisibilityFilter	Widgets must have this type of visibility to be included the path
	 */
	bool GeneratePathToWidgetUnchecked( TSharedRef< const SWidget > InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter = EVisibility::Visible ) const;
	
	/**
	 * @todo slate: Remove this method or make it private.
	 * Searches for the specified widget and generates a full path to it.  Note that this is
	 * a relatively slow operation!  Asserts if the widget isn't found.
	 * 
	 * @param  InWidget       Widget to generate a path to
	 * @param  OutWidgetPath  The generated widget path
	 * @param  VisibilityFilter	Widgets must have this type of visibility to be included the path
	 */
	void GeneratePathToWidgetChecked( TSharedRef< const SWidget > InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter = EVisibility::Visible ) const;
	
	/**
	 * Finds the window that the provided widget resides in
	 * 
	 * @param InWidget		The widget to find the window for
	 * @return The window where the widget resides, or null if the widget wasn't found.  Remember, a widget might not be found simply because its parent decided not to report the widget in ArrangeChildren.
	 */
	TSharedPtr<SWindow> FindWidgetWindow( TSharedRef< const SWidget > InWidget ) const;

	/**
	 * Finds the window that the provided widget resides in
	 * 
	 * @param InWidget		The widget to find the window for
	 * @param OutWidgetPath Full widget path generated 
	 * @return The window where the widget resides, or null if the widget wasn't found.  Remember, a widget might not be found simply because its parent decided not to report the widget in ArrangeChildren.
	 */
	TSharedPtr<SWindow> FindWidgetWindow( TSharedRef< const SWidget > InWidget, FWidgetPath& OutWidgetPath) const;

	/**
	 * @return True if the application is currently routing high precision mouse movement events (OS specific)
	 * If this value is true, the mouse is captured and hidden by the widget that originally made the request.
	 */
	bool IsUsingHighPrecisionMouseMovment() const { return PlatformApplication.IsValid() ? PlatformApplication->IsUsingHighPrecisionMouseMode() : false; }
	
	/**
	 * @return True if the last mouse event was from a trackpad.
	 */
	bool IsUsingTrackpad() const { return PlatformApplication.IsValid() ? PlatformApplication->IsUsingTrackpad() : false; }

	/**
	 * @return True if there is a mouse device attached
	 */
	bool IsMouseAttached() const { return PlatformApplication.IsValid() ? PlatformApplication->IsMouseAttached() : false; }

	/**
	 * @return True if there is a gamepad attached
	 */
	bool IsGamepadAttached() const { return PlatformApplication.IsValid() ? PlatformApplication->IsGamepadAttached() : false; }

	/**
	 * Sets the widget reflector.
	 *
	 * @param WidgetReflector The widget reflector to set.
	 */
	void SetWidgetReflector( const TSharedRef<IWidgetReflector>& WidgetReflector );

	/** @param AccessDelegate The delegate to pass along to the widget reflector */
	void SetWidgetReflectorSourceAccessDelegate(FAccessSourceCode AccessDelegate)
	{
		SourceCodeAccessDelegate = AccessDelegate;
	}

	/** @param QueryAccessDelegate The delegate to pass along to the widget reflector */
	void SetWidgetReflectorQuerySourceAccessDelegate(FQueryAccessSourceCode QueryAccessDelegate)
	{
		QuerySourceCodeAccessDelegate = QueryAccessDelegate;
	}

	/** @param AccessDelegate The delegate to pass along to the widget reflector */
	void SetWidgetReflectorAssetAccessDelegate(FAccessAsset AccessDelegate)
	{
		AssetAccessDelegate = AccessDelegate;
	}

	/** @param Scale  Sets the ratio SlateUnit / ScreenPixel */
	void SetApplicationScale( float InScale ){ Scale = InScale; }

	virtual void GetInitialDisplayMetrics( FDisplayMetrics& OutDisplayMetrics ) const { PlatformApplication->GetInitialDisplayMetrics( OutDisplayMetrics ); }

	/** Are we drag-dropping right now? */
	bool IsDragDropping() const;

	/** Get the current drag-dropping content */
	TSharedPtr<class FDragDropOperation> GetDragDroppingContent() const;

	/** Cancels any in flight drag and drops */
	void CancelDragDrop();

	/**
	 * Returns the attribute that can be used by widgets to check if the application is in normal execution mode
	 * Don't hold a reference to this anywhere that can exist when this application closes
	 */
	const TAttribute<bool>& GetNormalExecutionAttribute() const { return NormalExecutionGetter; }

	/**
	 * @return true if not in debugging mode
	 */
	bool IsNormalExecution() const { return !GIntraFrameDebuggingGameThread; }

	/**
	 * @return true if in debugging mode
	 */
	bool InKismetDebuggingMode() const { return GIntraFrameDebuggingGameThread; }

	/**
	 * Enters debugging mode which is a special state that causes the
	 * Slate application to tick in place which in the middle of a stack frame
	 */
	void EnterDebuggingMode();

	/**
	 * Leaves debugging mode
	 *
	 * @param bLeavingDebugForSingleStep	Whether or not we are leaving debug mode due to single stepping
	 */
	void LeaveDebuggingMode( bool bLeavingDebugForSingleStep = false );
	
	/**
	 * Calculates the popup window position from the passed in window position and size. 
	 * Adjusts position for popup windows which are outside of the work area of the monitor where they reside
	 *
	 * @param InAnchor				The current(suggseted) window position and size of an area which may not be covered by the popup.
	 * @param InSize				The size of the window
	 * @param InProposedPlacement	The location on screen where the popup should go if allowed. If zero this will be determined from Orientation and Anchor
	 * @param Orientation			The direction of the popup.
	 *								If vertical it will attempt to open below the anchor but will open above if there is no room.
	 *								If horizontal it will attempt to open below the anchor but will open above if there is no room.
	 * @return The adjusted position
	 */
	virtual FVector2D CalculatePopupWindowPosition( const FSlateRect& InAnchor, const FVector2D& InSize, bool bAutoAdjustForDPIScale = true, const FVector2D& InProposedPlacement = FVector2D::ZeroVector, const EOrientation Orientation = Orient_Vertical) const;

	/**
	 * Is the window in the app's destroy queue? If so it will be destroyed next tick.
	 *
	 * @param Window		The window to find in the destroy list
	 * @return				true if Window is in the destroy queue
	 */
	bool IsWindowInDestroyQueue(TSharedRef<SWindow> Window) const;

	/** @return	Returns true if the application's average frame rate is at least as high as our target frame rate that satisfies our requirement for a smooth and responsive UI experience */
	bool IsRunningAtTargetFrameRate() const;

	/** @return Returns true if transition effects for new menus and windows should be played */
	bool AreMenuAnimationsEnabled() const;

	/** 
	 * Sets whether transition effects for new menus and windows should be played.  This can be called at any time.
	 *
	 * @param	bEnableAnimations	True if animations should be used, otherwise false.
	 */
	void EnableMenuAnimations( const bool bEnableAnimations );

	void SetPlatformApplication(const TSharedRef<class GenericApplication>& InPlatformApplication);

	/** Set the global application icon */
	void SetAppIcon(const FSlateBrush* const InAppIcon);

	/** Sets the display state of external UI such as Steam. */
	void ExternalUIChange(bool bIsOpening)
	{
		bIsExternalUIOpened = bIsOpening;
	}

	/**
	 * Shows or hides an onscreen keyboard
	 *
	 * @param bShow	true to show the keyboard, false to hide it
	 * @param TextEntryWidget The widget that will receive the input from the virtual keyboard
	 */
	void ShowVirtualKeyboard( bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget = nullptr );

	/** @return true if the current platform allows cursor positioning in editable text boxes */
	bool AllowMoveCursor();

	/** Get the work area that has the largest intersection with the specified rectangle */
	FSlateRect GetWorkArea( const FSlateRect& InRect ) const;

	/**
	 * Shows or hides an onscreen keyboard
	 *
	 * @param bShow	true to show the keyboard, false to hide it
	 */
	virtual void NativeApp_ShowKeyboard( bool bShow, FString InitialString = "", int32 SelectionStart = -1, int32 SelectionEnd = -1 )
	{
		// empty default functionality
	}

	/** @return true if the current platform allows source code access */
	bool SupportsSourceAccess() const;

	/** Opens the current platform's code editing IDE (if necessary) and focuses the specified line in the specified file. Will only work if SupportsSourceAccess() returns true */
	void GotoLineInSource(const FString& FileName, int32 LineNumber) const;

	/** @return Service that supports popups; helps with auto-hiding of popups */
	FPopupSupport& GetPopupSupport() { return PopupSupport; }

	/**
	 * Forces the window to redraw immediately.
	 */
	void ForceRedrawWindow( const TSharedRef<SWindow>& InWindowToDraw );

	/**
	 * Takes a screenshot of the widget writing the results into the color buffer provided.  Note that the format is BGRA.
	 * the size of the resulting image is also output.
	 * 
	 * @return true if taking the screenshot was successful.
	 */
	bool TakeScreenshot(const TSharedRef<SWidget>& Widget, TArray<FColor>&OutColorData, FIntVector& OutSize);

	/**
	 * Takes a screenshot of the widget writing the results into the color buffer provided, this version allows you to provide 
	 * an inner area to screenshot.  Note that the format is BGRA.  The size of the resulting image is also output.
	 *
	 * @return true if taking the screenshot was successful.
	 */
	bool TakeScreenshot(const TSharedRef<SWidget>& Widget, const FIntRect& InnerWidgetArea, TArray<FColor>& OutColorData, FIntVector& OutSize);

	/**
	 * 
	 */
	TSharedPtr< FSlateWindowElementList > GetCachableElementList(const TSharedPtr<SWindow>& CurrentWindow, const ILayoutCache* LayoutCache);

	/**
	 * Once a layout cache is destroyed it needs to free any resources it was using in a safe way to prevent
	 * any in-flight rendering from being interrupted by referencing resources that go away.  So when a layout
	 * cache is destroyed it should call this function so any associated resources can be collected when it's safe.
	 */
	void ReleaseResourcesForLayoutCache(const ILayoutCache* LayoutCache);

	/**
	 * @return a handle for the existing or newly created virtual slate user.  This is handy when you need to create
	 * virtual hardware users for slate components in the virtual world that may need to be interacted with with virtual hardware.
	 */
	TSharedRef<FSlateVirtualUser> FindOrCreateVirtualUser(int32 VirtualUserIndex);

	/**
	 * 
	 */
	void UnregisterUser(int32 UserIndex);

	/**
	 * Allows you do some operations for every registered user.
	 */
	void ForEachUser(TFunctionRef<void(FSlateUser*)> InPredicate, bool bIncludeVirtualUsers = false);

protected:
	/**
	 * Register a user with Slate.  Normally this is unnecessary as Slate automatically adds
	 * a user entry if it gets input from a controller for that index.  Might happen if the user
	 * allocates the virtual user.
	 */
	void RegisterUser(TSharedRef<FSlateUser> User);

	/**
	 * Gets the user at the given index, null if the user does not exist.
	 */
	FORCEINLINE const FSlateUser* GetUser(int32 UserIndex) const
	{
		return (UserIndex >= 0 && UserIndex < Users.Num()) ? Users[UserIndex].Get() : nullptr;
	}

	/**
	 * Gets the user at the given index, null if the user does not exist.
	 */
	FORCEINLINE FSlateUser* GetUser(int32 UserIndex)
	{
		return ( UserIndex >= 0 && UserIndex < Users.Num() ) ? Users[UserIndex].Get() : nullptr;
	}

	/**
	 * Locates the SlateUser object corresponding to the index, if one can't be found, it will create a slate user at
	 * the provided index.  If the index is less than 0, null is returned.
	 */
	FSlateUser* GetOrCreateUser(int32 UserIndex);

	friend class FAnalogCursor;
	friend class FEventRouter;

	virtual bool DoesWidgetHaveMouseCaptureByUser(const TSharedPtr<const SWidget> Widget, int32 UserIndex, TOptional<int32> PointerIndex) const override;
	virtual bool DoesWidgetHaveMouseCapture(const TSharedPtr<const SWidget> Widget) const override;

	virtual TOptional<EFocusCause> HasUserFocus(const TSharedPtr<const SWidget> Widget, int32 UserIndex) const override;
	virtual TOptional<EFocusCause> HasAnyUserFocus(const TSharedPtr<const SWidget> Widget) const override;
	virtual bool IsWidgetDirectlyHovered(const TSharedPtr<const SWidget> Widget) const override;
	virtual bool ShowUserFocus(const TSharedPtr<const SWidget> Widget) const override;

	/**
	 * Pumps and ticks the platform.
	 */
	void TickPlatform(float DeltaTime);

	/**
	 * Ticks and paints the actual Slate portion of the application.
	 */
	void TickApplication(ESlateTickType TickType, float DeltaTime);

	/** Draws Slate windows. Should only be called by the application's main loop or renderer. */
	void DrawWindows();

	/**
	 * Draws slate windows, optionally only drawing the passed in window
	 */
	void PrivateDrawWindows( TSharedPtr<SWindow> DrawOnlyThisWindow = nullptr );

	/**
	 * Pre-pass step before drawing windows to compute geometry size and reshape autosized windows
	 */
	void DrawPrepass( TSharedPtr<SWindow> DrawOnlyThisWindow );

	/**
	 * Draws a window and its children
	 */
	void DrawWindowAndChildren( const TSharedRef<SWindow>& WindowToDraw, struct FDrawWindowArgs& DrawWindowArgs );

	/**
	 * Gets all visible child windows of a window.
	 */
	void GetAllVisibleChildWindows(TArray< TSharedRef<SWindow> >& OutWindows, TSharedRef<SWindow> CurrentWindow);

	/** Engages or disengages application throttling based on user behavior */
	void ThrottleApplicationBasedOnMouseMovement();

	virtual FWidgetPath LocateWidgetInWindow(FVector2D ScreenspaceMouseCoordinate, const TSharedRef<SWindow>& Window, bool bIgnoreEnabledStatus) const override;

	/**
	 * Sets up any values that need to be based on the physical dimensions of the device.  
	 * Such as dead zones associated with precise tapping...etc
	 */
	void SetupPhysicalSensitivities();

public:

	/**
	 * Called by the native application in response to a mouse move. Routs the event to Slate Widgets.
	 *
	 * @param  InMouseEvent  Mouse event
	 * @param  bIsSynthetic  True when the even is synthesized by slate.
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseMoveEvent( FPointerEvent& MouseEvent, bool bIsSynthetic = false );

	/**
	 * Called by the native application in response to a mouse button press. Routs the event to Slate Widgets.
	 *
	 * @param  PlatformWindow  The platform window the event originated from, used to set focus at the platform level. 
	 *                         If Invalid the Mouse event will work but there will be no effect on the platform.
	 * @param  InMouseEvent    Mouse event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseButtonDownEvent(const TSharedPtr< FGenericWindow >& PlatformWindow, FPointerEvent& InMouseEvent);

	/**
	 * Called by the native application in response to a mouse button release. Routs the event to Slate Widgets.
	 *
	 * @param  InMouseEvent  Mouse event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseButtonUpEvent( FPointerEvent& MouseEvent );

	/**
	 * Called by the native application in response to a mouse release. Routs the event to Slate Widgets.
	 *
	 * @param  InMouseEvent  Mouse event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseButtonDoubleClickEvent( const TSharedPtr< FGenericWindow >& PlatformWindow, FPointerEvent& InMouseEvent );
	
	/**
	 * Called by the native application in response to a mouse wheel spin or a touch gesture. Routs the event to Slate Widgets.
	 *
	 * @param  InWheelEvent    Mouse wheel event details
	 * @param  InGestureEvent  Optional gesture event details
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessMouseWheelOrGestureEvent( FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent );

	/**
	 * Called when a character is entered
	 *
	 * @param  InCharacterEvent  Character event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessKeyCharEvent( FCharacterEvent& InCharacterEvent );

	/**
	 * Called when a key is pressed
	 *
	 * @param  InKeyEvent  Keyb event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessKeyDownEvent( FKeyEvent& InKeyEvent );

	/**
	 * Called when a key is released
	 *
	 * @param  InKeyEvent  Key event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessKeyUpEvent( FKeyEvent& InKeyEvent );
	
	/**
	 * Called when a analog input values change
	 *
	 * @param  InAnalogInputEvent Analog input event
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessAnalogInputEvent(FAnalogInputEvent& InAnalogInputEvent);

	/**
	 * Called when a drag from an external (non-slate) source enters a window
	 *
	 * @param WindowEntered  The window that was entered by the drag and drop
	 * @param DragDropEvent  Describes the mouse state (position, pressed buttons, etc) and associated payload
	 * @return true if the drag enter was handled and can be processed by some widget in this window; false otherwise
	 */
	bool ProcessDragEnterEvent( TSharedRef<SWindow> WindowEntered, FDragDropEvent& DragDropEvent );

	/**
	 * Called when a touchpad touch is started (finger down) when polling game device state
	 * 
	 * @param ControllerEvent	The touch event generated
	 */
	void ProcessTouchStartedEvent( const TSharedPtr< FGenericWindow >& PlatformWindow, FPointerEvent& InTouchEvent );

	/**
	 * Called when a touchpad touch is moved  (finger moved) when polling game device state
	 * 
	 * @param ControllerEvent	The touch event generated
	 */
	void ProcessTouchMovedEvent( FPointerEvent& InTouchEvent );

	/**
	 * Called when a touchpad touch is ended (finger lifted) when polling game device state
	 * 
	 * @param ControllerEvent	The touch event generated
	 */
	void ProcessTouchEndedEvent( FPointerEvent& InTouchEvent );

	/**
	 * Called when motion is detected (controller or device) when polling game device state
	 * 
	 * @param MotionEvent		The motion event generated
	 */
	void ProcessMotionDetectedEvent( FMotionEvent& InMotionEvent );

	/**
	 * Called by the native application in response to an activation or deactivation. 
	 *
	 * @param ActivateEvent Information about the window activation/deactivation
	 * @return  Was this event handled by the Slate application?
	 */
	bool ProcessWindowActivatedEvent( const FWindowActivateEvent& ActivateEvent );
	
	/**
	 * Called when the application is activated (i.e. one of its windows becomes active) or deactivated.
	 *
	 * @param InAppActivated Whether the application was activated.
	 */
	void ProcessApplicationActivationEvent( bool InAppActivated );

	/**
	 * Returns true if the we're currently processing mouse, keyboard, touch or gamepad input.
	 */
	bool IsProcessingInput() const { return ProcessingInput > 0; }

public:

	TSharedRef<FNavigationConfig> GetNavigationConfig() const { return NavigationConfigFactory(); }
	
	void SetNavigationConfigFactory( TFunction<TSharedRef<FNavigationConfig>()> InNavigationConfigFactory );

	/** Called when the slate application is being shut down. */
	void OnShutdown();

	/** Closes all active windows immediately */
	void CloseAllWindowsImmediately();

	/**
	 * Destroy the native and slate windows in the array provided.
	 *
	 * @param WindowsToDestroy   Destroy these windows
	 */
	void DestroyWindowsImmediately();

	/**
	 * Apply any requests from the Reply to the application. E.g. Capture mouse
	 *
	 * @param CurrentEventPath   The WidgetPath along which the reply-generated event was routed
	 * @param TheReply           The reply generated by an event that was being processed.
	 * @param WidgetsUnderMouse  Optional widgets currently under the mouse; initiating drag and drop needs access to widgets under the mouse.
	 * @param InMouseEvent       Optional mouse event that caused this action.
	 * @param UserIndex			 User index that generated the event we are replying to (defaults to 0, at least for now)
	 */
	void ProcessReply(const FWidgetPath& CurrentEventPath, const FReply TheReply, const FWidgetPath* WidgetsUnderMouse, const FPointerEvent* InMouseEvent, const uint32 UserIndex = 0);
	
	/** Bubble a request for which cursor to display for widgets under the mouse or the widget that captured the mouse. */
	void QueryCursor();

	/**
	 * Apply any requests from the CursorReply
	 *
	 * @param CursorReply        The reply generated by an event that was being processed.
	 */
	void ProcessCursorReply(const FCursorReply& CursorReply);
	
	/**
	 * Spawns a tool tip window.  If an existing tool tip window is open, it will be dismissed first.
	 *
	 * @param	InToolTip           Widget to display.
	 * @param	InSpawnLocation     Screen space location to show the tool tip (window top left)
	 */
	void SpawnToolTip( const TSharedRef<IToolTip>& InToolTip, const FVector2D& InSpawnLocation );

	/** Closes the open tool-tip, if a tool-tip is open */
	void CloseToolTip();

	/**
	 * Updates tool tip
	 */
	void UpdateToolTip( bool AllowSpawningOfNewToolTips );

	/** @return an array of top-level windows that can be interacted with. e.g. when a modal window is up, only return the modal window */
	TArray< TSharedRef<SWindow> > GetInteractiveTopLevelWindows();

	/** Gets all visible slate windows ordered from back to front based on child hierarchies */
	void GetAllVisibleWindowsOrdered(TArray< TSharedRef<SWindow> >& OutWindows);
	
	/** Tell the slate application to log a string to it's ui log */
	void OnLogSlateEvent(enum EEventLog::Type Event, const FString& AdditionalContent = FString());

	/** Tell the slate application to log a FText to it's ui log */
	void OnLogSlateEvent(enum EEventLog::Type Event, const FText& AdditionalContent );

	/** Sets the slate event logger */
	void SetSlateUILogger(TSharedPtr<class IEventLogger> InEventLogger = TSharedPtr<class IEventLogger>());

	/** @return true if mouse events are being turned into touch events, and touch UI should be forced on */
	bool IsFakingTouchEvents() const;

#if PLATFORM_DESKTOP || PLATFORM_HTML5
	
	/** Sets whether the application is treating mouse events as imitating touch events.  Optional CursorLocation can be supplied to override the platform's belief of where the cursor is */
	void SetGameIsFakingTouchEvents(const bool bIsFaking, FVector2D* CursorLocation = nullptr);

#endif

	/** Sets the handler for otherwise unhandled key down events. This is used by the editor to provide a global action list, if the key was not consumed by any widget. */
	void SetUnhandledKeyDownEventHandler( const FOnKeyEvent& NewHandler );

	/** @return the last time a user interacted with a keyboard, mouse, touch device, or controller */
	double GetLastUserInteractionTime() const { return LastUserInteractionTime; }

	DECLARE_EVENT_OneParam(FSlateApplication, FSlateLastUserInteractionTimeUpdateEvent, double);
	/** @return Gets the event for LasterUserInteractionTime update */
	FSlateLastUserInteractionTimeUpdateEvent& GetLastUserInteractionTimeUpdateEvent() { return LastUserInteractionTimeUpdateEvent; }

	/** @return the deadzone size for dragging in screen pixels (aka virtual desktop pixels) */
	float GetDragTriggerDistance() const;

	/** @return the deadzone size squared for dragging in screen pixels (aka virtual desktop pixels) */
	float GetDragTriggerDistanceSquared() const;

	/** @return true if the difference between the ScreenSpaceOrigin and the ScreenSpacePosition is larger than the trigger distance for dragging in Slate. */
	bool HasTraveledFarEnoughToTriggerDrag(const FPointerEvent& PointerEvent, const FVector2D ScreenSpaceOrigin) const;

	/** Set the size of the deadzone for dragging in screen pixels */
	void SetDragTriggerDistance( float ScreenPixels );
	
	/** [Deprecated] Adds or removes input pre-processor. */
	DEPRECATED(4.17, "SetInputPreProcessor(...) is deprecated. Use RegisterInputPreProcessor(...) and/or UnregisterInputPreProcessor(...) / UnregisterAllInputPreProcessors(...) instead.")
	void SetInputPreProcessor(bool bEnable, TSharedPtr<class IInputProcessor> InputProcessor = nullptr);

	/** 
	 * Adds input pre-processor if unique. 
	 * @param InputProcessor	The input pre-processor to add.
	 * @param Index				Where to insert the InputProcessor, when sorting is needed. Default index will add at the end.
	 * @return True if added to list of input pre-processors, false if not
	 */
	bool RegisterInputPreProcessor(TSharedPtr<class IInputProcessor> InputProcessor, const int32 Index = INDEX_NONE);

	/**
	 * Removes an input pre-processor.
	 * @param InputProcessor	The input pre-processor to Remove.
	 */
	void UnregisterInputPreProcessor(TSharedPtr<class IInputProcessor> InputProcessor);

	/** 
	 * Removes all input pre-processor from list of input pre-processors.
	 */
	void UnregisterAllInputPreProcessors();

	/** Sets the hit detection radius of the cursor */
	void SetCursorRadius(float NewRadius);

	/** Getter for the cursor radius */
	float GetCursorRadius() const;

	void SetAllowTooltips(bool bCanShow);
	bool GetAllowTooltips() const;
	
public:

	//~ Begin FSlateApplicationBase Interface

	virtual bool IsActive() const override
	{
		return bAppIsActive;
	}

	virtual TSharedRef<SWindow> AddWindow( TSharedRef<SWindow> InSlateWindow, const bool bShowImmediately = true ) override;

	virtual void ArrangeWindowToFrontVirtual( TArray<TSharedRef<SWindow>>& Windows, const TSharedRef<SWindow>& WindowToBringToFront ) override
	{
		FSlateWindowHelper::ArrangeWindowToFront(Windows, WindowToBringToFront);
	}

	virtual bool FindPathToWidget( TSharedRef<const SWidget> InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter = EVisibility::Visible ) override
	{
		if ( !FSlateWindowHelper::FindPathToWidget(GetInteractiveTopLevelWindows(), InWidget, OutWidgetPath, VisibilityFilter) )
		{
			return FSlateWindowHelper::FindPathToWidget(SlateVirtualWindows, InWidget, OutWidgetPath, VisibilityFilter);
		}

		return true;
	}

	virtual const double GetCurrentTime() const override
	{
		return CurrentTime;
	}

	virtual TSharedPtr<SWindow> GetActiveTopLevelWindow() const override;
	virtual const FSlateBrush* GetAppIcon() const override;

	virtual float GetApplicationScale() const override
	{
		return Scale;
	}

	virtual FVector2D GetCursorPos() const override;
	virtual FVector2D GetLastCursorPos() const override;
	virtual FVector2D GetCursorSize() const override;

	virtual bool GetSoftwareCursorAvailable() const override
	{
		return bSoftwareCursorAvailable;
	}

	virtual EVisibility GetSoftwareCursorVis() const override;	
	virtual TSharedPtr<SWidget> GetKeyboardFocusedWidget() const override;

	virtual EWindowTransparency GetWindowTransparencySupport() const override
	{
		return PlatformApplication->GetWindowTransparencySupport();
	}

protected:

	virtual TSharedPtr< SWidget > GetMouseCaptorImpl() const override;

public:

	//~ Begin FSlateApplicationBase Interface

	virtual bool HasAnyMouseCaptor() const override;
	virtual bool HasUserMouseCapture(int32 UserIndex) const override;
	virtual FSlateRect GetPreferredWorkArea() const override;
	virtual bool HasFocusedDescendants( const TSharedRef<const SWidget>& Widget ) const override;
	virtual bool HasUserFocusedDescendants(const TSharedRef< const SWidget >& Widget, int32 UserIndex) const override;
	virtual bool IsExternalUIOpened() override;
	virtual FWidgetPath LocateWindowUnderMouse( FVector2D ScreenspaceMouseCoordinate, const TArray<TSharedRef<SWindow>>& Windows, bool bIgnoreEnabledStatus = false ) override;
	virtual bool IsWindowHousingInteractiveTooltip(const TSharedRef<const SWindow>& WindowToTest) const override;
	virtual TSharedRef<SWidget> MakeImage( const TAttribute<const FSlateBrush*>& Image, const TAttribute<FSlateColor>& Color, const TAttribute<EVisibility>& Visibility ) const override;
	virtual TSharedRef<SWidget> MakeWindowTitleBar( const TSharedRef<SWindow>& Window, const TSharedPtr<SWidget>& CenterContent, EHorizontalAlignment CenterContentAlignment, TSharedPtr<IWindowTitleBar>& OutTitleBar ) const override;
	virtual TSharedRef<IToolTip> MakeToolTip( const TAttribute<FText>& ToolTipText ) override;
	virtual TSharedRef<IToolTip> MakeToolTip( const FText& ToolTipText ) override;
	virtual void RequestDestroyWindow( TSharedRef<SWindow> WindowToDestroy ) override;
	virtual bool SetKeyboardFocus( const FWidgetPath& InFocusPath, const EFocusCause InCause ) override;
	virtual bool SetUserFocus(const uint32 InUserIndex, const FWidgetPath& InFocusPath, const EFocusCause InCause) override;
	virtual void SetAllUserFocus(const FWidgetPath& InFocusPath, const EFocusCause InCause) override;
	virtual void SetAllUserFocusAllowingDescendantFocus(const FWidgetPath& InFocusPath, const EFocusCause InCause) override;
	virtual TSharedPtr<SWidget> GetUserFocusedWidget(uint32 UserIndex) const override;

	DECLARE_EVENT_OneParam(FSlateApplication, FApplicationActivationStateChangedEvent, const bool /*IsActive*/)
	virtual FApplicationActivationStateChangedEvent& OnApplicationActivationStateChanged() { return ApplicationActivationStateChangedEvent; }

public:

	//~ Begin FGenericApplicationMessageHandler Interface

	virtual bool ShouldProcessUserInputMessages( const TSharedPtr< FGenericWindow >& PlatformWindow ) const override;
	virtual bool OnKeyChar( const TCHAR Character, const bool IsRepeat ) override;
	virtual bool OnKeyDown( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat ) override;
	virtual bool OnKeyUp( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat ) override;
	virtual bool OnMouseDown( const TSharedPtr< FGenericWindow >& PlatformWindow, const EMouseButtons::Type Button ) override;
	virtual bool OnMouseDown( const TSharedPtr< FGenericWindow >& PlatformWindow, const EMouseButtons::Type Button, const FVector2D CursorPos ) override;
	virtual bool OnMouseUp( const EMouseButtons::Type Button ) override;
	virtual bool OnMouseUp( const EMouseButtons::Type Button, const FVector2D CursorPos ) override;
	virtual bool OnMouseDoubleClick( const TSharedPtr< FGenericWindow >& PlatformWindow, const EMouseButtons::Type Button ) override;
	virtual bool OnMouseDoubleClick( const TSharedPtr< FGenericWindow >& PlatformWindow, const EMouseButtons::Type Button, const FVector2D CursorPos ) override;
	virtual bool OnMouseWheel( const float Delta ) override;
	virtual bool OnMouseWheel( const float Delta, const FVector2D CursorPos ) override;
	virtual bool OnMouseMove() override;
	virtual bool OnRawMouseMove( const int32 X, const int32 Y ) override;
	virtual bool OnCursorSet() override;
	virtual bool OnControllerAnalog( FGamepadKeyNames::Type KeyName, int32 ControllerId, float AnalogValue ) override;
	virtual bool OnControllerButtonPressed( FGamepadKeyNames::Type KeyName, int32 ControllerId, bool IsRepeat ) override;
	virtual bool OnControllerButtonReleased( FGamepadKeyNames::Type KeyName, int32 ControllerId, bool IsRepeat ) override;
	virtual bool OnTouchGesture( EGestureEvent GestureType, const FVector2D& Delta, float WheelDelta, bool bIsDirectionInvertedFromDevice ) override;
	virtual bool OnTouchStarted( const TSharedPtr< FGenericWindow >& PlatformWindow, const FVector2D& Location, int32 TouchIndex, int32 ControllerId ) override;
	virtual bool OnTouchMoved( const FVector2D& Location, int32 TouchIndex, int32 ControllerId ) override;
	virtual bool OnTouchEnded( const FVector2D& Location, int32 TouchIndex, int32 ControllerId ) override;
	virtual void ShouldSimulateGesture(EGestureEvent Gesture, bool bEnable) override;
	virtual bool OnMotionDetected(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration, int32 ControllerId) override;
	virtual bool OnSizeChanged( const TSharedRef< FGenericWindow >& PlatformWindow, const int32 Width, const int32 Height, bool bWasMinimized = false ) override;
	virtual void OnOSPaint( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual FWindowSizeLimits GetSizeLimitsForWindow(const TSharedRef<FGenericWindow>& Window) const override;
	virtual void OnResizingWindow( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual bool BeginReshapingWindow( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual void FinishedReshapingWindow( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual void HandleDPIScaleChanged(const TSharedRef<FGenericWindow>& Window) override;
	virtual void OnMovedWindow( const TSharedRef< FGenericWindow >& PlatformWindow, const int32 X, const int32 Y ) override;
	virtual bool OnWindowActivationChanged( const TSharedRef< FGenericWindow >& PlatformWindow, const EWindowActivation ActivationType ) override;
	virtual bool OnApplicationActivationChanged( const bool IsActive ) override;
	virtual bool OnConvertibleLaptopModeChanged() override;
	virtual EWindowZone::Type GetWindowZoneForPoint( const TSharedRef< FGenericWindow >& PlatformWindow, const int32 X, const int32 Y ) override;
	virtual void OnWindowClose( const TSharedRef< FGenericWindow >& PlatformWindow ) override;
	virtual EDropEffect::Type OnDragEnterText( const TSharedRef< FGenericWindow >& Window, const FString& Text ) override;
	virtual EDropEffect::Type OnDragEnterFiles( const TSharedRef< FGenericWindow >& Window, const TArray< FString >& Files ) override;
	virtual EDropEffect::Type OnDragEnterExternal( const TSharedRef< FGenericWindow >& Window, const FString& Text, const TArray< FString >& Files ) override;

	EDropEffect::Type OnDragEnter( const TSharedRef< SWindow >& Window, const TSharedRef<FExternalDragOperation>& DragDropOperation );

	virtual EDropEffect::Type OnDragOver( const TSharedPtr< FGenericWindow >& Window ) override;
	virtual void OnDragLeave( const TSharedPtr< FGenericWindow >& Window ) override;
	virtual EDropEffect::Type OnDragDrop( const TSharedPtr< FGenericWindow >& Window ) override;
	virtual bool OnWindowAction( const TSharedRef< FGenericWindow >& PlatformWindow, const EWindowAction::Type InActionType ) override;

public:

	/**
	 * Directly routes a pointer down event to the widgets in the specified widget path
	 *
	 * @param WidgetsUnderPointer	The path of widgets the event is routed to.
	 * @param PointerEvent		The event data that is is routed to the widget path
	 * 
	 * @return The reply returned by the widget that handled the event
	 */
	FReply RoutePointerDownEvent(FWidgetPath& WidgetsUnderPointer, FPointerEvent& PointerEvent);

	/**
	 * Directly routes a pointer up event to the widgets in the specified widget path
	 *
	 * @param WidgetsUnderPointer	The path of widgets the event is routed to.
	 * @param PointerEvent		The event data that is is routed to the widget path
	 *
	 * @return The reply from the event
	 */
	FReply RoutePointerUpEvent(FWidgetPath& WidgetsUnderPointer, FPointerEvent& PointerEvent);

	/**
	 * Directly routes a pointer move event to the widgets in the specified widget path
	 *
	 * @param WidgetsUnderPointer	The path of widgets the event is routed to.
	 * @param PointerEvent		The event data that is is routed to the widget path
	 * @param bIsSynthetic		Whether or not the move event is synthetic.  Synthetic pointer moves used simulate an event without the pointer actually moving 
	 */
	bool RoutePointerMoveEvent( const FWidgetPath& WidgetsUnderPointer, FPointerEvent& PointerEvent, bool bIsSynthetic );

	/**
	 * Directly routes a pointer double click event to the widgets in the specified widget path
	 *
	 * @param WidgetsUnderPointer	The path of widgets the event is routed to.
	 * @param PointerEvent		The event data that is is routed to the widget path
	 */
	FReply RoutePointerDoubleClickEvent( FWidgetPath& WidgetsUnderPointer, FPointerEvent& PointerEvent );

	/**
	 * Directly routes a pointer mouse wheel or gesture event to the widgets in the specified widget path.
	 * 
	 * @param WidgetsUnderPointer	The path of widgets the event is routed to.
	 * @param InWheelEvent			The event data that is is routed to the widget path
	 * @param InGestureEvent		The event data that is is routed to the widget path
	 */
	FReply RouteMouseWheelOrGestureEvent(const FWidgetPath& WidgetsUnderPointer, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent = nullptr);

	/**
	 * @return int user index that the keyboard is mapped to. -1 if the keyboard isn't mapped
	 */
	int32 GetUserIndexForKeyboard() const;

	/** 
	 * @return int user index that this controller is mapped to. -1 if the controller isn't mapped
	 */
	int32 GetUserIndexForController(int32 ControllerId) const;

	/**
	 * Register for a notification when the window action occurs.
	 *
	 * @param Notification          The notification to invoke.
	 *
	 * @return Handle to the registered delegate.
	 */
	FDelegateHandle RegisterOnWindowActionNotification(const FOnWindowAction& Notification);


	/** Event type for when Slate is ticking during a modal dialog loop */
	DECLARE_EVENT_OneParam(FSlateApplication, FOnModalLoopTickEvent, float);

	/**
	 * Get the FOnModalLoopTickEvent for the Slate Application. Allows clients to register for callbacks during modal dialog loops.
	 *
	 * @return The application's FOnModalLoopTickEvent.
	 */
	FOnModalLoopTickEvent& GetOnModalLoopTickEvent() { return ModalLoopTickEvent; }

	/**
	* Unregister the notification because it is no longer desired.
	*
	* @param Handle                Hanlde to the delegate to unregister.
	*/
	void UnregisterOnWindowActionNotification(FDelegateHandle Handle);

	/**
	 * Destroys an SWindow, removing it and all its children from the Slate window list.  Notifies the native window to destroy itself and releases rendering resources
	 *
	 * @param InUserIndex The user that is doing the navigation
	 * @param NavigationDestination The navigation destination widget
	 * @param NavigationSource The source type of the navigation
	 */
	void NavigateToWidget(const uint32 UserIndex, const TSharedPtr<SWidget>& NavigationDestination, ENavigationSource NavigationSource = ENavigationSource::FocusedWidget);

	/**
	 * Destroys an SWindow, removing it and all its children from the Slate window list.  Notifies the native window to destroy itself and releases rendering resources
	 *
	 * @param InUserIndex The user that is doing the navigation
	 * @param InNavigationType The navigation type / direction
	 * @param InWindow The window to do the navigation within
	 */
	void NavigateFromWidgetUnderCursor(const uint32 InUserIndex, EUINavigation InNavigationType, TSharedRef<SWindow> InWindow);

	/**
	* Given an optional widget, try and get the most suitable parent window to use with dialogs (such as file and directory pickers).
	* This will first try and get the window that owns the widget (if provided), before falling back to using the MainFrame window.
	*/
	TSharedPtr<SWindow> FindBestParentWindowForDialogs(const TSharedPtr<SWidget>& InWidget);

	/**
	* Given an optional widget, try and get the most suitable parent window handle to use with dialogs (such as file and directory pickers).
	* This will first try and get the window that owns the widget (if provided), before falling back to using the MainFrame window.
	*/
	const void* FindBestParentWindowHandleForDialogs(const TSharedPtr<SWidget>& InWidget);

public:
#if WITH_EDITORONLY_DATA
	FDragDropCheckingOverride OnDragDropCheckOverride;
#endif

private:

	TSharedRef< FGenericWindow > MakeWindow( TSharedRef<SWindow> InSlateWindow, const bool bShowImmediately );

	/**
	 * Destroys an SWindow, removing it and all its children from the Slate window list.  Notifies the native window to destroy itself and releases rendering resources
	 *
	 * @param DestroyedWindow The window to destroy
	 */
	void PrivateDestroyWindow( const TSharedRef<SWindow>& DestroyedWindow );

	/**
	 * Attempts to navigate to the next widget in the direction specified
	 *
	 * @return if a new widget was navigated too
	 */
	bool AttemptNavigation(const FWidgetPath& NavigationSource, const FNavigationEvent& NavigationEvent, const FNavigationReply& NavigationReply, const FArrangedWidget& BoundaryWidget);

	/**
	 * Executes a navigate to the specified widget if possible
	 *
	 * @return if the widget was navigated too
	 */
	bool ExecuteNavigation(const FWidgetPath& NavigationSource, TSharedPtr<SWidget> DestinationWidget, const uint32 UserIndex);

private:

	bool SetUserFocus(FSlateUser* User, const FWidgetPath& InFocusPath, const EFocusCause InCause);

	/** Lock the cursor such that it cannot leave the bounds of the specified widget. Null widget implies no cursor lock. */
	void LockCursor(const TSharedPtr<SWidget>& Widget);

	/**
	 * Lock the cursor such that it cannot leave the bounds of the leaf-most widget of the specified path.
	 * Assumes a valid path.
	 */
	void LockCursorToPath(const FWidgetPath& WidgetPath);

	/** Clear any cursor locks */
	void UnlockCursor();

	/** Make sure that the cursor lock region matcher the locking widget's geometry */
	void UpdateCursorLockRegion();

	/** State related to cursor locking. */
	struct
	{
		/** Path to widget that currently holds the cursor lock; invalid path if no cursor lock. */
		FWeakWidgetPath PathToLockingWidget;

		/** Desktop Space Rect that bounds the cursor. */
		FSlateRect LastComputedBounds;
	} CursorLock;

private:

	/** Sets the LastUserInteractionTime and fires off the LastUserInteractionTimeUpdateEvent */
	void SetLastUserInteractionTime(const double InCurrentTime);

private:

	// Hidden default constructor.
	FSlateApplication();

private:
	/** Represents a single user and pointer index for a device 
	 *  Used to uniquely track widget state per user and per device
	 */
	struct FUserAndPointer
	{
		uint32 UserIndex;
		uint32 PointerIndex;
		
		FUserAndPointer( uint32 InUserIndex, uint32 InPointerIndex )
			: UserIndex( InUserIndex )
			, PointerIndex( InPointerIndex )
		{}

		bool operator==( const FUserAndPointer& Other ) const
		{
			return UserIndex == Other.UserIndex && PointerIndex == Other.PointerIndex;
		}

		friend uint32 GetTypeHash( const FUserAndPointer& UserAndPointer )
		{
			return UserAndPointer.UserIndex << 16 | UserAndPointer.PointerIndex;
		}
	};

	/**
	 * Creates a mouse move event for the last known cursor position.  This should be called every tick to make
	 * sure that widgets that appear (or vanish from) underneath the cursor have hover state set appropriately.
	 */
	void SynthesizeMouseMove();

	/** Signal that a synthesized mouse move will be required after this operation. */
	void QueueSynthesizedMouseMove();

	/**
	 * Will be invoked when the size of the geometry of the virtual
	 * desktop changes (e.g. resolution change or monitors re-arranged)
	 */
	void OnVirtualDesktopSizeChanged(const FDisplayMetrics& NewDisplayMetric);

	/** Application singleton */
	static TSharedPtr< FSlateApplication > CurrentApplication;

	TSet<FKey> PressedMouseButtons;

	/** After processing an event or performing an active timer, we need to synthesize a mouse move. @see SynthesizeMouseMove */
	int32 SynthesizeMouseMovePending;

	/** true when the slate app is active; i.e. the current foreground window is from our Slate app*/
	bool bAppIsActive;

	/** true if any slate window is currently active (not just top level windows) */
	bool bSlateWindowActive;

	/** Application-wide scale for supporting monitors of varying pixel density */
	float Scale;

	/** The dead zone distance in virtual desktop pixels (a.k.a screen pixels) that the user has to move their finder before it is considered a drag.*/
	float DragTriggerDistance;

	/** All the top-level windows owned by this application; they are tracked here in a platform-agnostic way. */
	TArray< TSharedRef<SWindow> > SlateWindows;

	/** All the virtual windows, which can be anywhere - likely inside the virtual world. */
	TArray< TSharedRef<SWindow> > SlateVirtualWindows;

	/** The currently active slate window that is a top-level window (full fledged window; not a menu or tooltip)*/
	TWeakPtr<SWindow> ActiveTopLevelWindow;
	
	/** List of active modal windows.  The last item in the list is the top-most modal window */
	TArray< TSharedPtr<SWindow> > ActiveModalWindows;

	/** These windows will be destroyed next tick. */
	TArray< TSharedRef<SWindow> > WindowDestroyQueue;
	
	/** The stack of menus that are open */
	FMenuStack MenuStack;

	/** A vertical slice through the tree of widgets on screen; it represents widgets that were under the cursor last time an event was processed */
	TMap<FUserAndPointer, FWeakWidgetPath> WidgetsUnderCursorLastEvent;

	/** Stores the position for the pointer position. */
	TMap<FUserAndPointer, FVector2D> PointerIndexPositionMap;

	/** Stores the position for the last pointer position. */
	TMap<FUserAndPointer, FVector2D> PointerIndexLastPositionMap;

	/**
	 * A helper class to wrap the weak path functionality. The advantage of using this
	 * class is that the path can be validated and the current mouse captor (if any) can
	 * be informed they have lost mouse capture in the case of a number of events, such as
	 * application losing focus, or the widget being hidden.
	 */
	class MouseCaptorHelper
	{
	public:
		/**
		 * Returns whether or not there are any active pointer captures.
		 */
		bool HasCapture() const;

		/**
		* Returns whether or not the particular UserIndex has capture.
		*/
		bool HasCaptureForUser(uint32 UserIndex) const;

		/**
		 * Returns whether or not the particular PointerIndex has capture.
		 */
		bool HasCaptureForPointerIndex(uint32 UserIndex, uint32 PointerIndex) const;

		bool DoesWidgetHaveMouseCapture(const TSharedPtr<const SWidget> Widget) const;
		bool DoesWidgetHaveMouseCaptureByUser(const TSharedPtr<const SWidget> Widget, int32 UserIndex, TOptional<int32> PointerIndex) const;

		/**
		 * Sets a new mouse captor widget for a specific pointer index, invalidating the previous one if any and calling
		 * its OnMouseCaptureLost() handler.
		 *
		 * @param PointerIndex	The index of the pointer which initiated the capture.
		 * @param EventPath		The path to the event.
		 * @param Widget		The widget that wants to capture the mouse.
		 */
		bool SetMouseCaptor(uint32 UserIndex, uint32 PointerIndex, const FWidgetPath& EventPath, TSharedPtr< SWidget > Widget );

		/** Invalidates all current mouse captors. Calls OnMouseCaptureLost() on the current mouse captor if one exists */
		void InvalidateCaptureForAllPointers();

		/** Invalidates a specific mouse captor. Calls OnMouseCaptureLost() on the specific mouse captor if one exists */
		void InvalidateCaptureForPointer(uint32 UserIndex, uint32 PointIndex);

		/** Invalidates a specific mouse captor. Calls OnMouseCaptureLost() on the specific mouse captor if one exists */
		void InvalidateCaptureForUser(uint32 UserIndex);

		/**
		 * Retrieves a resolved FWidgetPath for a specific pointer index, if possible.
		 * If the path is invalid or truncated (i.e. the widget is no longer relevant) then the current mouse captor's
		 * OnMouseCaptureLost() handler is called and the path is invalidated.
		 *
		 * @param PointerIndex				The index of the pointer which has capture.
		 * @param InterruptedPathHandling	How to handled incomplete paths. "Truncate" will return a partial path, "ReturnInvalid" will return an empty path.
		 */
		FWidgetPath ToWidgetPath(uint32 UserIndex, uint32 PointerIndex, FWeakWidgetPath::EInterruptedPathHandling::Type InterruptedPathHandling = FWeakWidgetPath::EInterruptedPathHandling::Truncate );

		FWidgetPath ToWidgetPath( FWeakWidgetPath::EInterruptedPathHandling::Type InterruptedPathHandling = FWeakWidgetPath::EInterruptedPathHandling::Truncate, const FPointerEvent* PointerEvent = nullptr );

		/**
		 * Retrieves an array of the resolved widget paths for any active captures.
		 */
		TArray<FWidgetPath> ToWidgetPaths();

		/** Retrieves the weak path for a current mouse captor with a specific pointer index */
		FWeakWidgetPath ToWeakPath(uint32 UserIndex, uint32 PointerIndex) const;
		

		/* Walks the weak path and retrieves the widget that is set as the current mouse captor with a specific pointer index */
		TSharedPtr< SWidget > ToSharedWidget(uint32 UserIndex, uint32 PointerIndex) const;

		/*
		 * Retrieves an array of shared widget pointers for the active mouse captures.
		 */
		TArray<TSharedRef<SWidget>> ToSharedWidgets() const;

		/* Walks the weak path and retrieves the window for the widget belonging to the mouse captor with the specified pointer index */
		TSharedPtr< SWidget > ToSharedWindow(uint32 UserIndex, uint32 PointerIndex);

	protected:
		/** Call the OnMouseCaptureLost() handler for the widget captured by the specific pointer index */
		void InformCurrentCaptorOfCaptureLoss(uint32 UserIndex, uint32 PointerIndex) const;

		/** A map of pointer indices to weak widget paths for the active mouse captures */
		TMap<FUserAndPointer, FWeakWidgetPath> PointerIndexToMouseCaptorWeakPathMap;
	};
	/** The current mouse captor for the application, if any. */
	MouseCaptorHelper MouseCaptor;

	/** The cursor widget and window to render that cursor for the current software cursor.*/
	TWeakPtr<SWindow> CursorWindowPtr;
	TWeakPtr<SWidget> CursorWidgetPtr;

	/** The hit-test radius of the cursor. Default value is 0. */
	float CursorRadius;

	/**
	 * All users currently registered with Slate.  Normally this is 1, but in a 
	 * situation where multiple users are providing input you need to track ui state
	 * of each user separately.
	 */
	TArray<TSharedPtr<FSlateUser>> Users;

	/**
	 * Weak pointers to the allocated virtual users.
	 */
	TArray<TWeakPtr<FSlateVirtualUser>> VirtualUsers;

	/**
	 * Application throttling
	 */

	/** Holds a current request to ensure Slate is responsive in low FPS situations, based in mouse button pressed state */
	FThrottleRequest MouseButtonDownResponsivnessThrottle;

	/** Separate throttle handle that engages automatically based on mouse movement and other user behavior */
	FThrottleRequest UserInteractionResponsivnessThrottle;

	/** The last real time that the user pressed a key or mouse button */
	double LastUserInteractionTime;

	/** Subset of LastUserInteractionTime that is used only when considering when to throttle */
	double LastUserInteractionTimeForThrottling;

	/** Delegate that gets called for LastUserInteractionTime Update */
	FSlateLastUserInteractionTimeUpdateEvent LastUserInteractionTimeUpdateEvent;

	/** Used when considering whether to put Slate to sleep */
	double LastMouseMoveTime;

	/** Helper for detecting when a drag should begin */
	class FDragDetector
	{
	public:
		FDragDetector()
		{
		}

		void StartDragDetection(const FWidgetPath& PathToWidget, int32 UserIndex, int32 PointerIndex, FKey DragButton, FVector2D StartLocation);
		bool IsDetectingDrag(const FPointerEvent& PointerEvent);
		bool DetectDrag(const FPointerEvent& PointerEvent, float DragTriggerDistance, FWeakWidgetPath*& OutWeakWidgetPath);
		void OnPointerRelease(const FPointerEvent& PointerEvent);
		void ResetDetection();

	private:

		struct FDragDetectionState
		{
			FDragDetectionState()
				: DetectDragStartLocation(FVector2D::ZeroVector)
				, DetectDragButton(EKeys::Invalid)
				, DetectDragUserIndex(INDEX_NONE)
				, DetectDragPointerIndex(INDEX_NONE)
			{
			}

			FDragDetectionState(const FWidgetPath& PathToWidget, int32 UserIndex, int32 PointerIndex, FKey DragButton, FVector2D StartLocation)
				: DetectDragForWidget(PathToWidget)
				, DetectDragStartLocation(StartLocation)
				, DetectDragButton(DragButton)
				, DetectDragUserIndex(UserIndex)
				, DetectDragPointerIndex(PointerIndex)
			{
			}

			/** If not null, a widget has request that we detect a drag being triggered in this widget and send an OnDragDetected() event*/
			FWeakWidgetPath DetectDragForWidget;
			/** Location from which be begin detecting the drag */
			FVector2D DetectDragStartLocation;
			/** Button that must be pressed to trigger the drag */
			FKey DetectDragButton;
			/** User index of the drag operation */
			int32 DetectDragUserIndex;
			/** Pointer index of the drag operation */
			int32 DetectDragPointerIndex;
		};

		/** A map of pointer indices drag states currently being tracked. */
		TMap<FUserAndPointer, FDragDetectionState> PointerIndexToDragState;
	};

	FDragDetector DragDetector;

	/** Support for auto-dismissing pop-ups */
	FPopupSupport PopupSupport;
	
	/** Pointer to the currently registered game viewport widget if any */
	TWeakPtr<SViewport> GameViewportWidget;

	TSharedPtr<ISlateSoundDevice> SlateSoundDevice;

	/** The current cached absolute real time, right before we tick widgets */
	double CurrentTime;

	/** Last absolute real time that we ticked */
	double LastTickTime;

	/** Running average time in seconds between calls to Tick (used for monitoring responsiveness) */
	float AverageDeltaTime;

	/** Average delta time for application responsiveness tracking.  This is like AverageDeltaTime, but it excludes frame
	    deltas spent while the the application is in a throttled state */
	float AverageDeltaTimeForResponsiveness;

	
	/**
	 * Provides a platform-agnostic method for requesting that the application exit.
	 * Implementations should assign a handler that terminates the process when this delegate is invoked.
	 */
	FSimpleDelegate OnExitRequested;
	
	/** A Widget that introspects the current UI hierarchy */
	TWeakPtr<IWidgetReflector> WidgetReflectorPtr;

	/** Delegate for accessing source code, to pass to any widget inspectors. */
	FAccessSourceCode SourceCodeAccessDelegate;

	/** Delegate for querying if source code access is available */
	FQueryAccessSourceCode QuerySourceCodeAccessDelegate;

	/** Delegate for accessing assets, to pass to any widget inspectors. */
	FAccessAsset AssetAccessDelegate;

	/** System for logging all relevant slate events to a log file */
	TSharedPtr<class IEventLogger> EventLogger;

	/** Allows us to track the number of non-slate modal windows active. */
	int32 NumExternalModalWindowsActive;

	/** List of delegates that need to be called when the window action occurs. */
	TArray<FOnWindowAction> OnWindowActionNotifications;

	/**
	 * Tool-tips
	 */

	/** Window that we'll re-use for spawned tool tips */
	TWeakPtr< SWindow > ToolTipWindow;

	/** Widget that last visualized the tooltip;  */
	TWeakPtr< SWidget > TooltipVisualizerPtr;

	/** The tool tip that is currently being displayed; can be invalid (i.e. not showing tooltip) */
	TWeakPtr<IToolTip> ActiveToolTip;

	/** The widget that sourced the currently active tooltip widget */
	TWeakPtr<SWidget> ActiveToolTipWidgetSource;

	/** Whether tool-tips are allowed to spawn at all.  Used only for debugging purposes. */
	int32 bAllowToolTips;

	/** How long before the tool tip should start fading in? */
	float ToolTipDelay;

	/** Tool-tip fade-in duration */
	float ToolTipFadeInDuration;

	/** Absolute real time that the current tool-tip window was summoned */
	double ToolTipSummonTime;

	/** Desired tool tip position in screen space, updated whenever the mouse moves */
	FVector2D DesiredToolTipLocation;

	/** Direction that tool-tip is being repelled from a force field in.  We cache this to avoid tool-tips
	    teleporting between different offset directions as the user moves the mouse cursor around. */
	enum class EToolTipOffsetDirection : uint8
	{
		Undetermined,
		Down,
		Right
	};
	EToolTipOffsetDirection ToolTipOffsetDirection;

	/** The top of the Style tree. */
	const class FStyleNode* RootStyleNode;

	/**
	 * Drag and Drop (DragDrop)
	 */
	
	/** When not null, the content of the current drag drop operation. */
	TSharedPtr< FDragDropOperation > DragDropContent;

	/** The window the drag drop content is over. */
	TWeakPtr< SWindow > DragDropWindowPtr;

	/** Whether or not we are requesting that we leave debugging mode after the tick is complete */
	bool bRequestLeaveDebugMode;
	/** Whether or not we need to leave debug mode for single stepping */
	bool bLeaveDebugForSingleStep;
	TAttribute<bool> NormalExecutionGetter;

	/**
	 * Console objects
	 */

	/** AllowToolTips console variable **/
	FAutoConsoleVariableRef CVarAllowToolTips;
	/** ToolTipDelay console variable **/
	FAutoConsoleVariableRef CVarToolTipDelay;
	/** ToolTipFadeInDuration console variable **/
	FAutoConsoleVariableRef CVarToolTipFadeInDuration;

	/**
	 * Modal Windows
	 */

	/** Delegates for when modal windows open or close */
	FModalWindowStackStarted ModalWindowStackStartedDelegate;
	FModalWindowStackEnded ModalWindowStackEndedDelegate;

	/** Keeps track of whether or not the UI for services such as Steam is open. */
	bool bIsExternalUIOpened;

	/** Handle to a throttle request made to ensure the window is responsive in low FPS situations */
	FThrottleRequest ThrottleHandle;

	/** When an drag and drop is happening, we keep track of whether slate knew what to do with the payload on last mouse move */
	bool DragIsHandled;

	/**
	 * Virtual keyboard text field
	 */
	IPlatformTextField* SlateTextField;

	/** For desktop platforms that want to test touch style input, pass -faketouches or -simmobile on the commandline to set this */
	bool bIsFakingTouch;

	/** For games that want to allow mouse to imitate touch */
	bool bIsGameFakingTouch;

	/**For desktop platforms that the touch move event be called when this variable is true */
	bool bIsFakingTouched;

	/** Delegate for when a key down event occurred but was not handled in any other way by ProcessKeyDownMessage */
	FOnKeyEvent UnhandledKeyDownEventHandler;

	/** controls whether unhandled touch events fall back to sending mouse events */
	bool bTouchFallbackToMouse;

	/** .ini controlled option to allow or disallow software cursor rendering */
	bool bSoftwareCursorAvailable;

	/** The OS or actions taken by the user may require we refresh the current state of the cursor. */
	bool bQueryCursorRequested;

	/**
	 * Slate look and feel
	 */

	/** Globally enables or disables transition effects for pop-up menus (menu stacks) */
	bool bMenuAnimationsEnabled;

	/** The icon to use on application windows */
	const FSlateBrush *AppIcon;

	FApplicationActivationStateChangedEvent ApplicationActivationStateChangedEvent;
	//
	// Hittest 2.0
	//

	// The rectangle that bounds all the physical monitors given their arrangement.
	// Info comes from the native platform.
	// e.g. On windows the origin (coordinates X=0, Y=0) is the upper left of the primary monitor,
	// but there could be another monitor on any of the sides.
	FSlateRect VirtualDesktopRect;

	//
	// Invalidation Support
	//

	class FCacheElementPools
	{
	public:
		TSharedPtr< FSlateWindowElementList > GetNextCachableElementList(const TSharedPtr<SWindow>& CurrentWindow );
		bool IsInUse() const;

	private:
		TArray< TSharedPtr< FSlateWindowElementList > > ActiveCachedElementListPool;
		TArray< TSharedPtr< FSlateWindowElementList > > InactiveCachedElementListPool;
	};

	TMap< const ILayoutCache*, TSharedPtr<FCacheElementPools> > CachedElementLists;
	TArray< TSharedPtr<FCacheElementPools> > ReleasedCachedElementLists;

	/** This factory function creates a navigation config for each slate user. */
	TFunction<TSharedRef<FNavigationConfig>()> NavigationConfigFactory;

	/** The simulated gestures Slate Application will be in charge of. */
	TBitArray<FDefaultBitArrayAllocator> SimulateGestures;

	/** Delegate for pre slate tick */
	FSlateTickEvent PreTickEvent;

	/** Delegate for post slate Tick */
	FSlateTickEvent PostTickEvent;

	/** Delegate for slate Tick during modal dialogs */
	FOnModalLoopTickEvent ModalLoopTickEvent;

	/** Critical section to avoid multiple threads calling Slate Tick when we're synchronizing between the Slate Loading Thread and the Game Thread. */
	FCriticalSection SlateTickCriticalSection;

	/** Are we currently processing input in slate?  If so this value will be greater than 0. */
	int32 ProcessingInput;

		
	/**
	 * A helper class to wrap the list of input pre-processors. 
	 */
	class InputPreProcessorsHelper
	{
	public:

		// Wrapper functions that call the corresponding function of IInputProcessor for each InputProcessor in the list.
		void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor);
		bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
		bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
		bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent);
		bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent);
		bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent);
		bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent);
		bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& MotionEvent);

		/**
		 * Adds or inserts an unique input pre-processor. 
		 * @param InputProcessor	The InputProcessor to add.
		 * @param Index				When this is set the index will be used to insert the InputProcessor. Defaults to INDEX_NONE, resulting in AddUnique.
		 */
		bool Add(TSharedPtr<IInputProcessor> InputProcessor, const int32 Index = INDEX_NONE);

		/**
		 * Remove an input pre-processor. 
		 * @param InputProcessor	The InputProcessor to remove.
		 */
		void Remove(TSharedPtr<IInputProcessor> InputProcessor);

		/**
		 * Remove all registered input pre-processors.
		 */
		void RemoveAll();

	private:

		/** The list of input pre-processors. */
		TArray<TSharedPtr<IInputProcessor>> InputPreProcessorList;
	};

	/** A list of input pre-processors, gets an opportunity to parse input before anything else. */
	InputPreProcessorsHelper InputPreProcessors;

#if WITH_EDITOR
	/**
	 * Delegate that is invoked before the input key get process by slate widgets bubble system.
	 * User Function cannot mark the input as handled.
	 */
	FOnApplicationPreInputKeyDownListener OnApplicationPreInputKeyDownListenerEvent;

	/**
	 * Delegate that is invoked before the mouse input button get process by slate widgets bubble system.
	 * User Function cannot mark the input as handled.
	 */
	FOnApplicationMousePreInputButtonDownListener OnApplicationMousePreInputButtonDownListenerEvent;

	/**
	 * Called when an editor window dpi scale is changed
	 */
	FOnWindowDPIScaleChanged OnWindowDPIScaleChangedEvent;
#endif // WITH_EDITOR
};
