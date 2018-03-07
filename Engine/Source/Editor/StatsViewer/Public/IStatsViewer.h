// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "IPropertyTable.h"

/**
 * The public interface for the Stats Viewer widget
 */
class IStatsViewer : public SCompoundWidget
{

public:

	/** Sends a requests to the Stats Viewer to refresh itself the next chance it gets */
	virtual void Refresh() = 0;

	/** Get the property table we are displaying */
	virtual TSharedPtr< class IPropertyTable > GetPropertyTable() = 0;

	/** Get the currently selected object set */
	virtual int32 GetObjectSetIndex() const = 0;
};

