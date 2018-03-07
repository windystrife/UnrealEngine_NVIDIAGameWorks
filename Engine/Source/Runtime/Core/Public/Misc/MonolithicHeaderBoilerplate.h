// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if UE_IS_ENGINE_MODULE && !defined(SUPPRESS_MONOLITHIC_HEADER_WARNINGS)
	#define MONOLITHIC_HEADER_BOILERPLATE() COMPILE_WARNING("Monolithic headers should not be used by this module. Please change it to explicitly include the headers it needs.")
	#define SUPPRESS_MONOLITHIC_HEADER_WARNINGS 1
#else
	#undef MONOLITHIC_HEADER_BOILERPLATE
	#define MONOLITHIC_HEADER_BOILERPLATE()
#endif