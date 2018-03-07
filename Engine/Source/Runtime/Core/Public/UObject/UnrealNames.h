// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

//
// Macros.
//

/** Index of highest hardcoded name to be replicated by index by the networking code
 * @warning: changing this number or making any change to the list of hardcoded names with index
 * less than this value breaks network compatibility, which by default checks for the same changelist
 * @note: names with a greater value than this can still be replicated, but they are sent as
 * strings instead of an efficient index
 */
#define MAX_NETWORKED_HARDCODED_NAME 410

//
// Hardcoded names.
//

// Define a message as an enumeration.
#define REGISTER_NAME(num,name) NAME_##name = num,
enum EName 
{
	// Include all the hard-coded names
	#include "UnrealNames.inl"

	// Special constant for the last hard-coded name index
	NAME_MaxHardcodedNameIndex,
};
#undef REGISTER_NAME
