// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Containers/LockFreeList.h"
#include "Layout/SlateRect.h"
#include "Widgets/SWindow.h"

struct FNotificationInfo;

template <class T> class TLockFreePointerListLIFO;

/**
 * A class which manages a group of notification windows                 
 */
class SLATE_API FSlateNotificationManager
{
	friend class SNotificationList;

public:
	/**
	 * Gets the instance of this manager                   
	 */
	static FSlateNotificationManager& Get();

	/** Update the manager */
	void Tick();

	/** Provide a window under which all notifications should nest. */
	void SetRootWindow( const TSharedRef<SWindow> InRootWindow );

	/**
	 * Adds a floating notification
	 * @param Info 		Contains various settings used to initialize the notification
	 */
	TSharedPtr<SNotificationItem> AddNotification(const FNotificationInfo& Info);

	/**
	 * Thread safe method of queuing a notification for presentation on the next tick
	 * @param Info 		Pointer to heap allocated notification info. Released by FSlateNotificationManager once the notification is displayed;
	 */
	void QueueNotification(FNotificationInfo* Info);

	/**
	 * Called back from the SlateApplication when a window is activated/resized
	 * We need to keep notifications topmost in the z-order so we manage it here directly
	 * as there isn't a cross-platform OS-level way of making a 'topmost child'.
	 * @param ActivateEvent 	Information about the activation event
	 */
	void ForceNotificationsInFront( const TSharedRef<SWindow> InWindow );

	/**
	 * Gets all the windows that represent notifications
	 * @returns an array of windows that contain notifications.
	 */
	void GetWindows(TArray< TSharedRef<SWindow> >& OutWindows) const;

	/**
	 * Sets whether notifications should be displayed at all 
	 * @param	bShouldAllow	Whether notifications should be enabled.  It defaults to on.
	 */
	void SetAllowNotifications( const bool bShouldAllow )
	{
		this->bAllowNotifications = bShouldAllow;
	}

	/** @return	Checks whether notifications are currently enabled */
	bool AreNotificationsAllowed() const
	{
		return this->bAllowNotifications;
	}

protected:
	/** Protect constructor as this is a singleton */
	FSlateNotificationManager();

	/** Arranges the active notifications in a stack */
	void ArrangeNotifications();

	/** Create a notification list for the specified screen rectangle */
	TSharedRef<SNotificationList> CreateStackForArea(const FSlateRect& InRectangle);

private:

	/** A list of notifications, bound to a particular region */
	struct FRegionalNotificationList
	{
		/** Constructor */
		FRegionalNotificationList(const FSlateRect& InRectangle);

		/** Arranges the notifications in a stack */
		void Arrange();

		/** Remove any dead notifications */
		void RemoveDeadNotifications();

		/** The notification list itself (one per notification) */
		TArray<TSharedRef<SNotificationList>> Notifications;

		/** The rectangle we use to determine the anchor point for this list */
		FSlateRect Region;
	};

	/** A window under which all of the notification windows will nest. */
	TWeakPtr<SWindow> RootWindowPtr;

	/** An array of notification lists grouped by work area regions */
	TArray< FRegionalNotificationList > RegionalLists;

	/** Thread safe queue of notifications to display */
	TLockFreePointerListLIFO<FNotificationInfo> PendingNotifications;

	/** Whether notifications should be displayed or not.  This can be used to globally suppress notification pop-ups */
	bool bAllowNotifications;
};
