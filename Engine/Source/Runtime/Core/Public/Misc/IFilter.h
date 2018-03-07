// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Delegates/Delegate.h"

/**
 * A generic interface that represents a Filter of ItemType.
 */
template< typename TItemType >
class IFilter
{
public:
	typedef TItemType ItemType;

	virtual ~IFilter(){ }

	/** Returns whether the specified Item passes the Filter's restrictions */
	virtual bool PassesFilter( TItemType InItem ) const = 0;

	/** Broadcasts anytime the restrictions of the Filter changes */
	DECLARE_EVENT( IFilter<TItemType>, FChangedEvent );
	virtual FChangedEvent& OnChanged() = 0;
};
