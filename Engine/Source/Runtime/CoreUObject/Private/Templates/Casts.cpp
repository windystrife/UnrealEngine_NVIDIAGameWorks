// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Templates/Casts.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCasts, Log, All);
DEFINE_LOG_CATEGORY(LogCasts);

void CastLogError(const TCHAR* FromType, const TCHAR* ToType)
{
	UE_LOG(LogCasts, Fatal, TEXT("Cast of %s to %s failed"), FromType, ToType);
    for(;;);
}


#if HACK_HEADER_GENERATOR

//////////////////////////////////////////////////////////////////////////
// ClassCastFlagMap

ClassCastFlagMap::ClassCastFlagMap()
{
	// Define macro to be applied to all cast class declarations.
	#define DECLARE_CAST_BY_FLAG(ClassName) CastFlagMap.Add(FString(#ClassName), CASTCLASS_##ClassName);

	// Now apply the macro to the whole list.
	DECLARE_ALL_CAST_FLAGS;

	#undef DECLARE_CAST_BY_FLAG
}

ClassCastFlagMap& ClassCastFlagMap::Get()
{
	static ClassCastFlagMap TheInstance;
	return TheInstance;
}

EClassCastFlags ClassCastFlagMap::GetCastFlag(const FString& ClassName) const
{
	const EClassCastFlags* ValuePtr = CastFlagMap.Find(ClassName);
	return ValuePtr ? *ValuePtr : CASTCLASS_None;
}

#endif //HACK_HEADER_GENERATOR
