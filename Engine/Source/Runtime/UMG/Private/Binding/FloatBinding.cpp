// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Binding/FloatBinding.h"

#define LOCTEXT_NAMESPACE "UMG"

UFloatBinding::UFloatBinding()
{
}

bool UFloatBinding::IsSupportedDestination(UProperty* Property) const
{
	return IsSupportedSource(Property);
}

bool UFloatBinding::IsSupportedSource(UProperty* Property) const
{
	return IsConcreteTypeCompatibleWithReflectedType<float>(Property);
}

float UFloatBinding::GetValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		float Value = 0;
		if ( SourcePath.GetValue<float>(Source, Value) )
		{
			return Value;
		}
	}

	return 0;
}

#undef LOCTEXT_NAMESPACE
