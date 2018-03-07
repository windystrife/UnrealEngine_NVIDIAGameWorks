// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "UObject/NameTypes.h"

class FFixedUObjectArray;

// Boilerplate that is included once for each module, even in monolithic builds
#if !defined(PER_MODULE_BOILERPLATE_ANYLINK)
#define PER_MODULE_BOILERPLATE_ANYLINK(ModuleImplClass, ModuleName)
#endif

/**
 * Override new + delete operators (and array versions) in every module.
 * This prevents the possibility of mismatched new/delete calls such as a new[] that
 * uses Unreal's allocator and a delete[] that uses the system allocator.
 */
#if USING_CODE_ANALYSIS
	#define OPERATOR_NEW_MSVC_PRAGMA MSVC_PRAGMA( warning( suppress : 28251 ) )	//	warning C28251: Inconsistent annotation for 'new': this instance has no annotations
#else
	#define OPERATOR_NEW_MSVC_PRAGMA
#endif

#define REPLACEMENT_OPERATOR_NEW_AND_DELETE \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new  ( size_t Size                        ) OPERATOR_NEW_THROW_SPEC      { return FMemory::Malloc( Size ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new[]( size_t Size                        ) OPERATOR_NEW_THROW_SPEC      { return FMemory::Malloc( Size ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new  ( size_t Size, const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return FMemory::Malloc( Size ); } \
	OPERATOR_NEW_MSVC_PRAGMA void* operator new[]( size_t Size, const std::nothrow_t& ) OPERATOR_NEW_NOTHROW_SPEC    { return FMemory::Malloc( Size ); } \
	void operator delete  ( void* Ptr )                                                 OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr )                                                 OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr, const std::nothrow_t& )                          OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr, const std::nothrow_t& )                          OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr, size_t Size )                                    OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr, size_t Size )                                    OPERATOR_DELETE_THROW_SPEC   { FMemory::Free( Ptr ); } \
	void operator delete  ( void* Ptr, size_t Size, const std::nothrow_t& )             OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); } \
	void operator delete[]( void* Ptr, size_t Size, const std::nothrow_t& )             OPERATOR_DELETE_NOTHROW_SPEC { FMemory::Free( Ptr ); }

class FFixedUObjectArray;

// GDB/LLDB pretty printers don't use these - no need to export additional symbols. This also solves ODR violation reported by ASan on Linux
#if PLATFORM_LINUX
#define UE4_VISUALIZERS_HELPERS
#else
#define UE4_VISUALIZERS_HELPERS \
	FNameEntry*** GFNameTableForDebuggerVisualizers_MT = FName::GetNameTableForDebuggerVisualizers_MT(); \
	FFixedUObjectArray*& GObjectArrayForDebugVisualizers = GCoreObjectArrayForDebugVisualizers;
#endif

// in DLL builds, these are done per-module, otherwise we just need one in the application
// visual studio cannot find cross dll data for visualizers, so these provide access
#define PER_MODULE_BOILERPLATE \
	UE4_VISUALIZERS_HELPERS \
	REPLACEMENT_OPERATOR_NEW_AND_DELETE
