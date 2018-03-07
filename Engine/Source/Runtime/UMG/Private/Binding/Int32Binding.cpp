// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Binding/Int32Binding.h"

#define LOCTEXT_NAMESPACE "UMG"

UInt32Binding::UInt32Binding()
{
}

bool UInt32Binding::IsSupportedDestination(UProperty* Property) const
{
	return IsSupportedSource(Property);
}

bool UInt32Binding::IsSupportedSource(UProperty* Property) const
{
	return IsConcreteTypeCompatibleWithReflectedType<int32>(Property);
}

int32 UInt32Binding::GetValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		int32 Value = 0;
		if ( SourcePath.GetValue<int32>(Source, Value) )
		{
			return Value;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
