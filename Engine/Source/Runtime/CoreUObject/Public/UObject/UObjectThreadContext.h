// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnAsyncLoading.cpp: Unreal async loading code.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSingleton.h"

class FObjectInitializer;

DECLARE_LOG_CATEGORY_EXTERN(LogUObjectThreadContext, Log, All);

class COREUOBJECT_API FUObjectThreadContext : public TThreadSingleton<FUObjectThreadContext>
{
	friend TThreadSingleton<FUObjectThreadContext>;

	FUObjectThreadContext();

	/** Stack of currently used FObjectInitializers for this thread */
	TArray<FObjectInitializer*> InitializerStack;

public:
	
	/**
	* Remove top element from the stack.
	*/
	void PopInitializer()
	{
		InitializerStack.Pop(/*bAllowShrinking=*/ false);
	}

	/**
	* Push new FObjectInitializer on stack.
	* @param	Initializer			Object initializer to push.
	*/
	void PushInitializer(FObjectInitializer* Initializer)
	{
		InitializerStack.Push(Initializer);
	}

	/**
	* Retrieve current FObjectInitializer for current thread.
	* @return Current FObjectInitializer.
	*/
	FObjectInitializer* TopInitializer()
	{
		return InitializerStack.Num() ? InitializerStack.Last() : nullptr;
	}

	/**
	* Retrieves current FObjectInitializer for current thread. Will assert of no ObjectInitializer is currently set.
	* @return Current FObjectInitializer reference.
	*/
	FObjectInitializer& TopInitializerChecked()
	{
		FObjectInitializer* ObjectInitializerPtr = TopInitializer();
		UE_CLOG(!ObjectInitializerPtr, LogUObjectThreadContext, Fatal, TEXT("Tried to get the current ObjectInitializer, but none is set. Please use NewObject or NewNamedObject to construct new UObject-derived classes."));
		return *ObjectInitializerPtr;
	}


	/** Imports for EndLoad optimization.	*/
	int32 ImportCount;
	/** Forced exports for EndLoad optimization. */
	int32 ForcedExportCount;
	/** Count for BeginLoad multiple loads.	*/
	int32 ObjBeginLoadCount;
	/** Objects that might need preloading. */
	TArray<UObject*> ObjLoaded;
	/** List of linkers that we want to close the loaders for (to free file handles) - needs to be delayed until EndLoad is called with GObjBeginLoadCount of 0 */
	TArray<FLinkerLoad*> DelayedLinkerClosePackages;
	/** true when we are routing ConditionalPostLoad/PostLoad to objects										*/
	bool IsRoutingPostLoad;
	/** true when FLinkerManager deletes linkers */
	bool IsDeletingLinkers;
	/* Global flag so that FObjectFinders know if they are called from inside the UObject constructors or not. */
	int32 IsInConstructor;
	/* Object that is currently being constructed with ObjectInitializer */
	UObject* ConstructedObject;
	/** Points to the main UObject currently being serialized */
	UObject* SerializedObject;
	/** Points to the main PackageLinker currently being serialized (Defined in Linker.cpp) */
	FLinkerLoad* SerializedPackageLinker;
	/** The main Import Index currently being used for serialization by CreateImports() (Defined in Linker.cpp) */
	int32 SerializedImportIndex;
	/** Points to the main Linker currently being used for serialization by CreateImports() (Defined in Linker.cpp) */
	FLinkerLoad* SerializedImportLinker;
	/** The most recently used export Index for serialization by CreateExport() */
	int32 SerializedExportIndex;
	/** Points to the most recently used Linker for serialization by CreateExport() */
	FLinkerLoad* SerializedExportLinker;
	/** Async Package currently processing objects */
	struct FAsyncPackage* AsyncPackage;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Stack to ensure that PostInitProperties is routed through Super:: calls. **/
	TArray<UObject*> PostInitPropertiesCheck;
	/** Used to verify that the Super::PostLoad chain is intact.			*/
	TArray<UObject*, TInlineAllocator<16> > DebugPostLoad;
#endif
#if WITH_EDITORONLY_DATA
	/** Maps a package name to all packages marked as editor-only due to the fact it was marked as editor-only */
	TMap<FName, TSet<FName>> PackagesMarkedEditorOnlyByOtherPackage;
#endif
};
