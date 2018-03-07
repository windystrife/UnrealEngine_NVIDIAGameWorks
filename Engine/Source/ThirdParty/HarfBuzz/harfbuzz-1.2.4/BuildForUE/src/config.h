// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#ifdef HAVE_STUB_GETENV
	// Stub out getenv as not all platforms support it
	#define getenv(name) 0
#endif // HAVE_STUB_GETENV

#ifdef __EMSCRIPTEN__
	// WASM does not support pthreads yet...
	#define HB_NO_MT
#endif
