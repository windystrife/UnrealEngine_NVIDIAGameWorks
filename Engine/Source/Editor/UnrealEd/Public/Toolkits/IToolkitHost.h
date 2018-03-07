// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkit.h"

/**
 * Base interface class for toolkit hosts
 */
class IToolkitHost
{

public:

	/** Gets a widget that can be used to parent a modal window or pop-up to.  You shouldn't be using this widget for
	    anything other than parenting, as the type of widget and behavior/lifespan is completely up to the host. */
	virtual TSharedRef< class SWidget > GetParentWidget() = 0;

	/** Brings this toolkit host's window (and tab, if it has one), to the front */
	virtual void BringToFront() = 0;

	/** Gets a tab stack to place a new tab for the specified toolkit area */
	virtual TSharedRef< class SDockTabStack > GetTabSpot( const EToolkitTabSpot::Type TabSpot ) = 0;

	/** Access the toolkit host's tab manager */
	virtual TSharedPtr< class FTabManager > GetTabManager() const = 0;

	/** Called when a toolkit is opened within this host */
	virtual void OnToolkitHostingStarted( const TSharedRef< class IToolkit >& Toolkit ) = 0;

	/** Called when a toolkit is no longer being hosted within this host */
	virtual void OnToolkitHostingFinished( const TSharedRef< class IToolkit >& Toolkit ) = 0;

	/** @return For world-centric toolkit hosts, gets the UWorld associated with this host */
	virtual class UWorld* GetWorld() const = 0;
};

