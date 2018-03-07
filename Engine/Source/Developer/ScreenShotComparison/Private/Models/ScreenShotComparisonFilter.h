// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IScreenShotData.h"
#include "Misc/IFilter.h"

/*=============================================================================
	ScreenShotComparisonFilter.h: Declares the FScreenShotComparisonFilter class.
=============================================================================*/

class FScreenShotComparisonFilter : public IFilter< const TSharedPtr< class IScreenShotData >&  >
{
public:

	// Begin IFilter functions

	DECLARE_DERIVED_EVENT(FScreenShotComparisonFilter, IFilter< const TSharedPtr< class IScreenShotData >& >::FChangedEvent, FChangedEvent);
	virtual FChangedEvent& OnChanged() override { return ChangedEvent; }


	/**
	 * Checks if the report passes the filter
	 *
	 * @param InReport - The automation report
	 * @return true if it passes the test
	 */
	virtual bool PassesFilter( const TSharedPtr< IScreenShotData >& InReport ) const override
	{
		bool FilterPassed = true;

		EScreenShotDataType::Type FilterType = InReport->GetScreenNodeType();
		FString TestName = InReport->GetName();

		// Screen Node
		if ( FilterType == EScreenShotDataType::SSDT_ScreenView && !ScreenShotFiltertext.IsEmpty() )
		{
			if ( !TestName.Contains( ScreenShotFiltertext, ESearchCase::CaseSensitive, ESearchDir::FromEnd) )
			{
				FilterPassed = false;
			}
		}
		else if ( FilterType == EScreenShotDataType::SSDT_Platform && !PlatformFilterText.IsEmpty() )
		{
			// Platform Node
			if ( TestName != PlatformFilterText )
			{
				FilterPassed = false;
			}
		}

		return FilterPassed;
	}

	// End IFilter functions


	/**
	 * Set the platform filter
	 * @param InPlatformFilter The platform to filter on
	 */
	void SetPlatformFilter( const FString InPlatformFilter )
	{
		PlatformFilterText = InPlatformFilter;
	}


	/**
	 * Set the text for the screen view filter
	 * @param InScreenFilter - The Screen Filter
	 */
	void SetScreenFilter( const FString InScreenFilter )
	{
		ScreenShotFiltertext = InScreenFilter;
	}


private:

	//	The event that broadcasts whenever a change occurs to the filter
	FChangedEvent ChangedEvent;
	
	// Platform Filter Text
	FString PlatformFilterText;

	// Screen Filter Text
	FString ScreenShotFiltertext;
};
