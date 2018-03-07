// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Application/SlateApplicationBase.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "Application/ActiveTimerHandle.h"
#include "ScopeLock.h"


/* Static initialization
 *****************************************************************************/

TSharedPtr<FSlateApplicationBase> FSlateApplicationBase::CurrentBaseApplication = nullptr;
TSharedPtr<GenericApplication> FSlateApplicationBase::PlatformApplication = nullptr;
// TODO: Identifier the cursor index in a smarter way.
const uint32 FSlateApplicationBase::CursorPointerIndex = ETouchIndex::CursorPointerIndex;
const uint32 FSlateApplicationBase::CursorUserIndex = 0;

FWidgetPath FHitTesting::LocateWidgetInWindow(FVector2D ScreenspaceMouseCoordinate, const TSharedRef<SWindow>& Window, bool bIgnoreEnabledStatus) const
{
	return SlateApp->LocateWidgetInWindow(ScreenspaceMouseCoordinate, Window, bIgnoreEnabledStatus);
}


FSlateApplicationBase::FSlateApplicationBase()
: Renderer()
, HitTesting(this)
, bIsSlateAsleep(false)
{

}

void FSlateApplicationBase::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics) const 
{ 
	FDisplayMetrics::GetDisplayMetrics(OutDisplayMetrics); 
}

const FHitTesting& FSlateApplicationBase::GetHitTesting() const
{
	return HitTesting;
}

void FSlateApplicationBase::RegisterActiveTimer( const TSharedRef<FActiveTimerHandle>& ActiveTimerHandle )
{
	FScopeLock ActiveTimerLock(&ActiveTimerCS);
	ActiveTimerHandles.Add(ActiveTimerHandle);
}

void FSlateApplicationBase::UnRegisterActiveTimer( const TSharedRef<FActiveTimerHandle>& ActiveTimerHandle )
{
	FScopeLock ActiveTimerLock(&ActiveTimerCS);
	ActiveTimerHandles.Remove(ActiveTimerHandle);
}

bool FSlateApplicationBase::AnyActiveTimersArePending()
{
	FScopeLock ActiveTimerLock(&ActiveTimerCS);

	// first remove any tick handles that may have become invalid.
	// If we didn't remove invalid handles here, they would never get removed because
	// we don't force widgets to UnRegister before they are destroyed.
	ActiveTimerHandles.RemoveAll([](const TWeakPtr<FActiveTimerHandle>& ActiveTimerHandle)
	{
		// only check the weak pointer to the handle. Just want to make sure to clear out any widgets that have since been deleted.
		return !ActiveTimerHandle.IsValid();
	});

	// The rest are valid. Update their pending status and see if any are ready.
	const double CurrentTime = GetCurrentTime();
	bool bAnyTickReady = false;
	for ( auto& ActiveTimerInfo : ActiveTimerHandles )
	{
		auto ActiveTimerInfoPinned = ActiveTimerInfo.Pin();
		check( ActiveTimerInfoPinned.IsValid() );

		// If an active timer is still pending execution from last frame, it is collapsed 
		// or otherwise blocked from ticking. Disregard until it executes.
		if ( ActiveTimerInfoPinned->IsPendingExecution() )
		{
			continue;
		}

		if ( ActiveTimerInfoPinned->UpdateExecutionPendingState( CurrentTime ) )
		{
			bAnyTickReady = true;
		}
	}

	return bAnyTickReady;
}

bool FSlateApplicationBase::IsSlateAsleep()
{
	return bIsSlateAsleep;
}

void FSlateApplicationBase::InvalidateAllWidgets() const
{
	OnGlobalInvalidateEvent.Broadcast();
}
