// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Stats/StatsMisc.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"

FScopeLogTime::FScopeLogTime( const WIDECHAR* InName, FTotalTimeAndCount* InCumulative /*= nullptr */, EScopeLogTimeUnits InUnits /*= ScopeLog_Milliseconds */ )
: StartTime( FPlatformTime::Seconds() )
, Name( InName )
, Cumulative( InCumulative )
, Units( InUnits )
{}

FScopeLogTime::FScopeLogTime( const ANSICHAR* InName, FTotalTimeAndCount* InCumulative /*= nullptr*/, EScopeLogTimeUnits InUnits /*= ScopeLog_Milliseconds */ )
: StartTime( FPlatformTime::Seconds() )
, Name( InName )
, Cumulative( InCumulative )
, Units( InUnits )
{}

FScopeLogTime::~FScopeLogTime()
{
	const double ScopedTime = FPlatformTime::Seconds() - StartTime;
	const FString DisplayUnitsString = GetDisplayUnitsString();
	if( Cumulative )
	{
		Cumulative->Key += ScopedTime;
		Cumulative->Value++;

		const double Average = Cumulative->Key / (double)Cumulative->Value;
		UE_LOG( LogStats, Log, TEXT( "%32s - %6.3f %s - Total %6.2f s / %5u / %6.3f %s" ), *Name, GetDisplayScopedTime(ScopedTime), *DisplayUnitsString, Cumulative->Key, Cumulative->Value, GetDisplayScopedTime(Average), *DisplayUnitsString );
	}
	else
	{
		UE_LOG( LogStats, Log, TEXT( "%32s - %6.3f %s" ), *Name, GetDisplayScopedTime(ScopedTime), *DisplayUnitsString );
	}
}

double FScopeLogTime::GetDisplayScopedTime(double InScopedTime) const
{
	switch(Units)
	{
		case ScopeLog_Seconds: return InScopedTime;
		case ScopeLog_Milliseconds:
		default:
			return InScopedTime * 1000.0f;
	}

	return InScopedTime * 1000.0f;
}

FString FScopeLogTime::GetDisplayUnitsString() const
{
	switch (Units)
	{
		case ScopeLog_Seconds: return TEXT("s");
		case ScopeLog_Milliseconds:
		default:
			return TEXT("ms");
	}

	return TEXT("ms");
}
