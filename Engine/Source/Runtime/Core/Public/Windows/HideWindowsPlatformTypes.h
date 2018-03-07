// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#ifdef WINDOWS_PLATFORM_TYPES_GUARD
	#undef WINDOWS_PLATFORM_TYPES_GUARD
#else
	#error Mismatched HideWindowsPLatformTypes.h detected.
#endif

#undef INT
#undef UINT
#undef DWORD
#undef FLOAT

#ifdef TRUE
	#undef TRUE
#endif

#ifdef FALSE
	#undef FALSE
#endif

#pragma warning( pop )
