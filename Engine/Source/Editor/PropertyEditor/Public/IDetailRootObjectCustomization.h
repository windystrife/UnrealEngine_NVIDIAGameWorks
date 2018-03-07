// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class SWidget;

/** 
 * Interface for any class that lays out details for a specific class
 */
class IDetailRootObjectCustomization : public TSharedFromThis<IDetailRootObjectCustomization>
{
public:
	virtual ~IDetailRootObjectCustomization() {}

	/** 
	 * Called when the details panel wants to display an object header widget for a given object
	 *
	 * @param	InRootObject	The object whose header is being customized
 	 */
	virtual TSharedPtr<SWidget> CustomizeObjectHeader(const UObject* InRootObject) = 0;

	/**
	 * Whether or not the object and all of its children should be visible in the details panel
	 */
	virtual bool IsObjectVisible(const UObject* InRootObject) const = 0;

	/**
	 * Whether or object should have a header displayed or just show the children directly
	 *
	 * @return true if the this customization should be displayed or false to just show the children directly
	 */
	virtual bool ShouldDisplayHeader(const UObject* InRootObject) const = 0;
};
