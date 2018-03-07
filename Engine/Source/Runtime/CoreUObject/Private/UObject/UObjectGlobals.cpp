// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObj.cpp: Unreal object global data and functions
=============================================================================*/

#include "UObject/UObjectGlobals.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ITransaction.h"
#include "Serialization/ArchiveProxy.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Misc/SlowTask.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "UObject/ScriptInterface.h"
#include "UObject/UObjectAllocator.h"
#include "UObject/UObjectBase.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectHash.h"
#include "UObject/Object.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "UObject/PropertyPortFlags.h"
#include "Serialization/SerializedPropertyScope.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UObjectAnnotation.h"
#include "Serialization/DuplicatedObject.h"
#include "Serialization/DuplicatedDataReader.h"
#include "Serialization/DuplicatedDataWriter.h"
#include "Misc/PackageName.h"
#include "UObject/LinkerLoad.h"
#include "Blueprint/BlueprintSupport.h"
#include "Misc/SecureHash.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/LinkerManager.h"
#include "Misc/ExclusiveLoadPackageTimeTracker.h"
#include "ProfilingDebugging/CookStats.h"
#include "Modules/ModuleManager.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/MetaData.h"
#include "HAL/LowLevelMemTracker.h"

DEFINE_LOG_CATEGORY(LogUObjectGlobals);

#if USE_MALLOC_PROFILER
#include "MallocProfiler.h"
#endif

bool						GIsSavingPackage = false;

/** Object annotation used by the engine to keep track of which objects are selected */
FUObjectAnnotationSparseBool GSelectedObjectAnnotation;

DEFINE_STAT(STAT_InitProperties);
DEFINE_STAT(STAT_ConstructObject);
DEFINE_STAT(STAT_AllocateObject);
DEFINE_STAT(STAT_PostConstructInitializeProperties);
DEFINE_STAT(STAT_LoadConfig);
DEFINE_STAT(STAT_LoadObject);
DEFINE_STAT(STAT_FindObject);
DEFINE_STAT(STAT_FindObjectFast);
DEFINE_STAT(STAT_NameTableEntries);
DEFINE_STAT(STAT_NameTableAnsiEntries);
DEFINE_STAT(STAT_NameTableWideEntries);
DEFINE_STAT(STAT_NameTableMemorySize);
DEFINE_STAT(STAT_DestroyObject);

DECLARE_CYCLE_STAT(TEXT("InstanceSubobjects"), STAT_InstanceSubobjects, STATGROUP_Object);
DECLARE_CYCLE_STAT(TEXT("PostInitProperties"), STAT_PostInitProperties, STATGROUP_Object);

#if ENABLE_COOK_STATS
#include "ProfilingDebugging/ScopedTimers.h"

namespace LoadPackageStats
{
	static double LoadPackageTimeSec = 0.0;
	static int NumPackagesLoaded = 0;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		AddStat(TEXT("Package.Load"), FCookStatsManager::CreateKeyValueArray(
			TEXT("NumPackagesLoaded"), NumPackagesLoaded,
			TEXT("LoadPackageTimeSec"), LoadPackageTimeSec));
	});
}
#endif

/** CoreUObject delegates */
FCoreUObjectDelegates::FRegisterHotReloadAddedClassesDelegate FCoreUObjectDelegates::RegisterHotReloadAddedClassesDelegate;
FCoreUObjectDelegates::FRegisterClassForHotReloadReinstancingDelegate FCoreUObjectDelegates::RegisterClassForHotReloadReinstancingDelegate;
FCoreUObjectDelegates::FReinstanceHotReloadedClassesDelegate FCoreUObjectDelegates::ReinstanceHotReloadedClassesDelegate;
// Delegates used by SavePackage()
FCoreUObjectDelegates::FIsPackageOKToSaveDelegate FCoreUObjectDelegates::IsPackageOKToSaveDelegate;
FCoreUObjectDelegates::FAutoPackageBackupDelegate FCoreUObjectDelegates::AutoPackageBackupDelegate;

FCoreUObjectDelegates::FOnPackageReloaded FCoreUObjectDelegates::OnPackageReloaded;
FCoreUObjectDelegates::FNetworkFileRequestPackageReload FCoreUObjectDelegates::NetworkFileRequestPackageReload;

FCoreUObjectDelegates::FOnPreObjectPropertyChanged FCoreUObjectDelegates::OnPreObjectPropertyChanged;
FCoreUObjectDelegates::FOnObjectPropertyChanged FCoreUObjectDelegates::OnObjectPropertyChanged;

#if WITH_EDITOR
// Set of objects modified this frame
TSet<UObject*> FCoreUObjectDelegates::ObjectsModifiedThisFrame;
FCoreUObjectDelegates::FOnObjectModified FCoreUObjectDelegates::OnObjectModified;
FCoreUObjectDelegates::FOnAssetLoaded FCoreUObjectDelegates::OnAssetLoaded;
FCoreUObjectDelegates::FOnObjectSaved FCoreUObjectDelegates::OnObjectSaved;
#endif // WITH_EDITOR


FSimpleMulticastDelegate& FCoreUObjectDelegates::GetPreGarbageCollectDelegate()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

FSimpleMulticastDelegate& FCoreUObjectDelegates::GetPostGarbageCollect()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

FCoreUObjectDelegates::FTraceExternalRootsForReachabilityAnalysisDelegate FCoreUObjectDelegates::TraceExternalRootsForReachabilityAnalysis;
FSimpleMulticastDelegate FCoreUObjectDelegates::PostReachabilityAnalysis;

FSimpleMulticastDelegate FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy;
FSimpleMulticastDelegate FCoreUObjectDelegates::PostGarbageCollectConditionalBeginDestroy;

FCoreUObjectDelegates::FPreLoadMapDelegate FCoreUObjectDelegates::PreLoadMap;
FCoreUObjectDelegates::FPostLoadMapDelegate FCoreUObjectDelegates::PostLoadMapWithWorld;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
FSimpleMulticastDelegate FCoreUObjectDelegates::PostLoadMap;
FCoreUObjectDelegates::FSoftObjectPathLoaded FCoreUObjectDelegates::StringAssetReferenceLoaded;
FCoreUObjectDelegates::FSoftObjectPathSaving FCoreUObjectDelegates::StringAssetReferenceSaving;
FCoreUObjectDelegates::FOnRedirectorFollowed FCoreUObjectDelegates::RedirectorFollowed;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
FSimpleMulticastDelegate FCoreUObjectDelegates::PostDemoPlay;
FCoreUObjectDelegates::FOnLoadObjectsOnTop FCoreUObjectDelegates::ShouldLoadOnTop;

FCoreUObjectDelegates::FPackageCreatedForLoad FCoreUObjectDelegates::PackageCreatedForLoad;
FCoreUObjectDelegates::FGetPrimaryAssetIdForObject FCoreUObjectDelegates::GetPrimaryAssetIdForObject;

/** Check whether we should report progress or not */
bool ShouldReportProgress()
{
	return GIsEditor && IsInGameThread() && !IsRunningCommandlet() && !IsAsyncLoading();
}

/**
 * Returns true if code is called form the game thread while collecting garbage.
 * We only have to guard against StaticFindObject on the game thread as other threads will be blocked anyway
 */
static FORCEINLINE bool IsGarbageCollectingOnGameThread()
{
	return IsInGameThread() && IsGarbageCollecting();
}

// Anonymous namespace to not pollute global.
namespace
{
	/**
	 * Legacy static find object helper, that helps to find reflected types, that
	 * are no longer a subobjects of UCLASS defined in the same header.
	 *
	 * If the class looked for is of one of the relocated types (or theirs subclass)
	 * then it performs another search in containing package.
	 *
	 * If the class match wasn't exact (i.e. either nullptr or subclass of allowed
	 * ones) and we've found an object we're revalidating it to make sure the
	 * legacy search was valid.
	 *
	 * @param ObjectClass Class of the object to find.
	 * @param ObjectPackage Package of the object to find.
	 * @param ObjectName Name of the object to find.
	 * @param ExactClass If the class match has to be exact. I.e. ObjectClass == FoundObjects.GetClass()
	 *
	 * @returns Found object.
	 */
	UObject* StaticFindObjectWithChangedLegacyPath(UClass* ObjectClass, UObject* ObjectPackage, FName ObjectName, bool ExactClass)
	{
		UObject* MatchingObject = nullptr;

		// This is another look-up for native enums, structs or delegate signatures, cause they're path changed
		// and old packages can have invalid ones. The path now does not have a UCLASS as an outer. All mentioned
		// types are just children of package of the file there were defined in.
		if (!FPlatformProperties::RequiresCookedData() && // Cooked platforms will have all paths resolved.
			ObjectPackage != nullptr &&
			ObjectPackage->IsA<UClass>()) // Only if outer is a class.
		{
			bool bHasDelegateSignaturePostfix = ObjectName.ToString().EndsWith(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX);

			bool bExactPathChangedClass = ObjectClass == UEnum::StaticClass() // Enums
				|| ObjectClass == UScriptStruct::StaticClass() || ObjectClass == UStruct::StaticClass() // Structs
				|| (ObjectClass == UFunction::StaticClass() && bHasDelegateSignaturePostfix); // Delegates

			bool bSubclassOfPathChangedClass = !bExactPathChangedClass && !ExactClass
				&& (ObjectClass == nullptr // Any class
				|| UEnum::StaticClass()->IsChildOf(ObjectClass) // Enums
				|| UScriptStruct::StaticClass()->IsChildOf(ObjectClass) || UStruct::StaticClass()->IsChildOf(ObjectClass) // Structs
				|| (UFunction::StaticClass()->IsChildOf(ObjectClass) && bHasDelegateSignaturePostfix)); // Delegates

			if (!bExactPathChangedClass && !bSubclassOfPathChangedClass)
			{
				return nullptr;
			}

			MatchingObject = StaticFindObject(ObjectClass, ObjectPackage->GetOutermost(), *ObjectName.ToString(), ExactClass);

			if (MatchingObject && bSubclassOfPathChangedClass)
			{
				// If the class wasn't given exactly, check if found object is of class that outers were changed.
				UClass* MatchingObjectClass = MatchingObject->GetClass();
				if (!(MatchingObjectClass == UEnum::StaticClass()	// Enums
					|| MatchingObjectClass == UScriptStruct::StaticClass() || MatchingObjectClass == UStruct::StaticClass() // Structs
					|| (MatchingObjectClass == UFunction::StaticClass() && bHasDelegateSignaturePostfix)) // Delegates
					)
				{
					return nullptr;
				}
			}
		}

		return MatchingObject;
	}
}

/**
 * Fast version of StaticFindObject that relies on the passed in FName being the object name
 * without any group/ package qualifiers.
 *
 * @param	ObjectClass		The to be found object's class
 * @param	ObjectPackage	The to be found object's outer
 * @param	ObjectName		The to be found object's class
 * @param	ExactClass		Whether to require an exact match with the passed in class
 * @param	AnyPackage		Whether to look in any package
 * @param	ExclusiveFlags	Ignores objects that contain any of the specified exclusive flags
 * @return	Returns a pointer to the found object or NULL if none could be found
 */
UObject* StaticFindObjectFast(UClass* ObjectClass, UObject* ObjectPackage, FName ObjectName, bool ExactClass, bool AnyPackage, EObjectFlags ExclusiveFlags, EInternalObjectFlags ExclusiveInternalFlags)
{
	if (GIsSavingPackage || IsGarbageCollectingOnGameThread())
	{
		UE_LOG(LogUObjectGlobals, Fatal,TEXT("Illegal call to StaticFindObjectFast() while serializing object data or garbage collecting!"));
	}

	// We don't want to return any objects that are currently being background loaded unless we're using FindObject during async loading.
	ExclusiveInternalFlags |= IsInAsyncLoadingThread() ? EInternalObjectFlags::None : EInternalObjectFlags::AsyncLoading;	
	UObject* FoundObject = StaticFindObjectFastInternal(ObjectClass, ObjectPackage, ObjectName, ExactClass, AnyPackage, ExclusiveFlags, ExclusiveInternalFlags);

	if (!FoundObject)
	{
		FoundObject = StaticFindObjectWithChangedLegacyPath(ObjectClass, ObjectPackage, ObjectName, ExactClass);
	}

	return FoundObject;
}

//
// Find an optional object.
//
UObject* StaticFindObject( UClass* ObjectClass, UObject* InObjectPackage, const TCHAR* OrigInName, bool ExactClass )
{
	INC_DWORD_STAT(STAT_FindObject);

	if (GIsSavingPackage)
	{
		UE_LOG(LogUObjectGlobals, Fatal,TEXT("Illegal call to StaticFindObject() while serializing object data!"));
	}

	if (IsGarbageCollectingOnGameThread())
	{
		UE_LOG(LogUObjectGlobals, Fatal,TEXT("Illegal call to StaticFindObject() while collecting garbage!"));
	}

	// Resolve the object and package name.
	const bool bAnyPackage = InObjectPackage==ANY_PACKAGE;
	UObject* ObjectPackage = bAnyPackage ? nullptr : InObjectPackage;

	UObject* MatchingObject = nullptr;

#if WITH_EDITOR
	// If the editor is running, and T3D is being imported, ensure any packages referenced are fully loaded.
	if ((GIsEditor == true) && (GIsImportingT3D == true))// && (ObjectPackage != ANY_PACKAGE) && (ObjectPackage != NULL))
	{
		static bool s_bCurrentlyLoading = false;

		if (s_bCurrentlyLoading == false)
		{
			FString NameCheck = OrigInName;
			if (NameCheck.Contains(TEXT("."), ESearchCase::CaseSensitive) && 
				!NameCheck.Contains(TEXT("'"), ESearchCase::CaseSensitive) && 
				!NameCheck.Contains(TEXT(":"), ESearchCase::CaseSensitive) )
			{
				s_bCurrentlyLoading = true;
				MatchingObject = StaticLoadObject(ObjectClass, nullptr, OrigInName, nullptr,  LOAD_NoWarn, nullptr);
				s_bCurrentlyLoading = false;
				if (MatchingObject != nullptr)
				{
					return MatchingObject;
				}
			}
		}
	}
#endif	//#if !WITH_EDITOR

	FName ObjectName;

	// Don't resolve the name if we're searching in any package
	if (!bAnyPackage)
	{
		FString InName = OrigInName;
		if (!ResolveName(ObjectPackage, InName, false, false))
		{
			return nullptr;
		}
		ObjectName = FName(*InName, FNAME_Add);
	}
	else
	{
		ObjectName = FName(OrigInName, FNAME_Add);
	}

	return StaticFindObjectFast(ObjectClass, ObjectPackage, ObjectName, ExactClass, bAnyPackage);
}

//
// Find an object; can't fail.
//
UObject* StaticFindObjectChecked( UClass* ObjectClass, UObject* ObjectParent, const TCHAR* InName, bool ExactClass )
{
	UObject* Result = StaticFindObject( ObjectClass, ObjectParent, InName, ExactClass );
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if( !Result )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), *FString::Printf( TEXT("Failed to find object '%s %s.%s'"), *ObjectClass->GetName(), ObjectParent==ANY_PACKAGE ? TEXT("Any") : ObjectParent ? *ObjectParent->GetName() : TEXT("None"), InName));
	}
#endif
	return Result;
}

//
// Find an object; won't assert on GIsSavingPackage or IsGarbageCollecting()
//
UObject* StaticFindObjectSafe( UClass* ObjectClass, UObject* ObjectParent, const TCHAR* InName, bool ExactClass )
{
	if (!GIsSavingPackage && !IsGarbageCollectingOnGameThread())
	{
		FGCScopeGuard GCAndSavepackageGuard;
		return StaticFindObject( ObjectClass, ObjectParent, InName, ExactClass );
	}
	else
	{
		return NULL;
	}
}


//
// Global property setting.
//
void GlobalSetProperty( const TCHAR* Value, UClass* Class, UProperty* Property, bool bNotifyObjectOfChange )
{
	if ( Property != NULL && Class != NULL )
	{
		// Apply to existing objects of the class.
		for( FObjectIterator It; It; ++It )
		{	
			UObject* Object = *It;
			if( Object->IsA(Class) && !Object->IsPendingKill() )
			{
				// If we're in a PIE session then only allow set commands to affect PlayInEditor objects.
				if( !GIsPlayInEditorWorld || Object->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor)  )
				{
#if WITH_EDITOR
					if( !Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange )
					{
						Object->PreEditChange(Property);
					}
#endif // WITH_EDITOR
					Property->ImportText( Value, Property->ContainerPtrToValuePtr<uint8>(Object), 0, Object );
#if WITH_EDITOR
					if( !Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange )
					{
						FPropertyChangedEvent PropertyEvent(Property);
						Object->PostEditChangeProperty(PropertyEvent);
					}
#endif // WITH_EDITOR
				}
			}
		}

		if (FPlatformProperties::HasEditorOnlyData())
		{
			// Apply to defaults.
			UObject* DefaultObject = Class->GetDefaultObject();
			check(DefaultObject != NULL);
			DefaultObject->SaveConfig();
		}
	}
}


/**
 * Executes a delegate by calling the named function on the object bound to the delegate.  You should
 * always first verify that the delegate is safe to execute by calling IsBound() before calling this function.
 *
 * @param	Params		Parameter structure
 */
//void FScriptDelegate::ProcessDelegate( void* Parameters ) const
//{
//	checkf( Object.IsValid() != false, TEXT( "ProcessDelegate() called with no object bound to delegate!" ) );
//	checkf( FunctionName != NAME_None, TEXT( "ProcessDelegate() called with no function name set!" ) );
//
//	// Object was pending kill, so we cannot execute the delegate.  Note that it's important to assert
//	// here and not simply continue execution, as memory may be left uninitialized if the delegate is
//	// not able to execute, resulting in much harder-to-detect code errors.  Users should always make
//	// sure IsBound() returns true before calling ProcessDelegate()!
//	UObject* ObjectPtr = static_cast< UObject* >( Object.Get() );	// Down-cast
//	checkSlow( !ObjectPtr->IsPendingKill() );
//
//	// Object *must* implement the specified function
//	UFunction* Function = ObjectPtr->FindFunctionChecked( FunctionName );
//
//	// Execute the delegate!
//	ObjectPtr->ProcessEvent(Function, Parameters);
//}


/**
 * Executes a multi-cast delegate by calling all functions on objects bound to the delegate.  Always
 * safe to call, even if when no objects are bound, or if objects have expired.
 *
 * @param	Params				Parameter structure
 */
//void FMulticastScriptDelegate::ProcessMulticastDelegate(void* Parameters) const
//{
//	if( InvocationList.Num() > 0 )
//	{
//		// Create a copy of the invocation list, just in case the list is modified by one of the callbacks during the broadcast
//		typedef TArray< FScriptDelegate, TInlineAllocator< 4 > > FInlineInvocationList;
//		FInlineInvocationList InvocationListCopy = FInlineInvocationList(InvocationList);
//	
//		// Invoke each bound function
//		for( FInlineInvocationList::TConstIterator FunctionIt( InvocationListCopy ); FunctionIt; ++FunctionIt )
//		{
//			if( FunctionIt->IsBound() )
//			{
//				// Invoke this delegate!
//				FunctionIt->ProcessDelegate(Parameters);
//			}
//			else if ( FunctionIt->IsCompactable() )
//			{
//				// Function couldn't be executed, so remove it.  Note that because the original list could have been modified by one of the callbacks, we have to search for the function to remove here.
//				RemoveInternal( *FunctionIt );
//			}
//		}
//	}
//}


/*-----------------------------------------------------------------------------
	UObject Tick.
-----------------------------------------------------------------------------*/

/**
 * Static UObject tick function, used to verify certain key assumptions and to tick the async loading code.
 *
 * @warning: The streaming stats rely on this function not doing any work besides calling ProcessAsyncLoading.
 * @todo: Move stats code into core?
 *
 * @param DeltaTime	Time in seconds since last call
 * @param bUseFullTimeLimit	If true, use the entire time limit even if blocked on I/O
 * @param AsyncLoadingTime Time in seconds to use for async loading limit
 */
void StaticTick( float DeltaTime, bool bUseFullTimeLimit, float AsyncLoadingTime )
{
	check(!IsLoading());

	// Spend a bit of time (pre)loading packages - currently 5 ms.
	ProcessAsyncLoading(true, bUseFullTimeLimit, AsyncLoadingTime);

	// Check natives.
	extern int32 GNativeDuplicate;
	if( GNativeDuplicate )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Duplicate native registered: %i"), GNativeDuplicate );
	}
	// Check for duplicates.
	extern int32 GCastDuplicate;
	if( GCastDuplicate )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Duplicate cast registered: %i"), GCastDuplicate );
	}

#if STATS
	// Set name table stats.
	int32 NameTableEntries = FName::GetMaxNames();
	int32 NameTableAnsiEntries = FName::GetNumAnsiNames();
	int32 NameTableWideEntries = FName::GetNumWideNames();
	int32 NameTableMemorySize = FName::GetNameTableMemorySize();
	SET_DWORD_STAT( STAT_NameTableEntries, NameTableEntries );
	SET_DWORD_STAT( STAT_NameTableAnsiEntries, NameTableAnsiEntries );
	SET_DWORD_STAT( STAT_NameTableWideEntries, NameTableWideEntries);
	SET_DWORD_STAT( STAT_NameTableMemorySize, NameTableMemorySize );

#if 0 // can't read stats with the new stats system
	// Set async I/O bandwidth stats.
	static uint32 PreviousReadSize	= 0;
	static uint32 PrevioudReadCount	= 0;
	static float PreviousReadTime	= 0;
	float ReadTime	= GStatManager.GetStatValueFLOAT( STAT_AsyncIO_PlatformReadTime );
	uint32 ReadSize	= GStatManager.GetStatValueDWORD( STAT_AsyncIO_FulfilledReadSize );
	uint32 ReadCount	= GStatManager.GetStatValueDWORD( STAT_AsyncIO_FulfilledReadCount );

	// It is possible that the stats are update in between us reading the values so we simply defer till
	// next frame if that is the case. This also handles partial updates. An individual value might be 
	// slightly wrong but we have enough small requests to smooth it out over a few frames.
	if( (ReadTime  - PreviousReadTime ) > 0.f 
	&&	(ReadSize  - PreviousReadSize ) > 0 
	&&	(ReadCount - PrevioudReadCount) > 0 )
	{
		float Bandwidth = (ReadSize - PreviousReadSize) / (ReadTime - PreviousReadTime) / 1048576.f;
		SET_FLOAT_STAT( STAT_AsyncIO_Bandwidth, Bandwidth );
		PreviousReadTime	= ReadTime;
		PreviousReadSize	= ReadSize;
		PrevioudReadCount	= ReadCount;
	}
	else
	{
		SET_FLOAT_STAT( STAT_AsyncIO_Bandwidth, 0.f );
	}
#endif
#endif
}



/*-----------------------------------------------------------------------------
   File loading.
-----------------------------------------------------------------------------*/

//
// Safe load error-handling.
//
void SafeLoadError( UObject* Outer, uint32 LoadFlags, const TCHAR* ErrorMessage)
{
	if( FParse::Param( FCommandLine::Get(), TEXT("TREATLOADWARNINGSASERRORS") ) == true )
	{
		UE_LOG(LogUObjectGlobals, Error, TEXT("%s"), ErrorMessage);
	}
	else
	{
		// Don't warn here if either quiet or no warn are set
		if( (LoadFlags & LOAD_Quiet) == 0 && (LoadFlags & LOAD_NoWarn) == 0)
		{ 
			UE_LOG(LogUObjectGlobals, Warning, TEXT("%s"), ErrorMessage);
		}
	}
}

/**
 * Find an existing package by name
 * @param InOuter		The Outer object to search inside
 * @param PackageName	The name of the package to find
 *
 * @return The package if it exists
 */
UPackage* FindPackage( UObject* InOuter, const TCHAR* PackageName )
{
	FString InName;
	if( PackageName )
	{
		InName = PackageName;
	}
	else
	{
		InName = MakeUniqueObjectName( InOuter, UPackage::StaticClass() ).ToString();
	}
	ResolveName( InOuter, InName, true, false );

	UPackage* Result = NULL;
	if ( InName != TEXT("None") )
	{
		Result = FindObject<UPackage>( InOuter, *InName );
	}
	else
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), TEXT("Attempted to create a package named 'None'") );
	}
	return Result;
}

UPackage* CreatePackage( UObject* InOuter, const TCHAR* PackageName )
{
	FString InName;

	if( PackageName )
	{
		InName = PackageName;
	}

	if (InName.Contains(TEXT("//"), ESearchCase::CaseSensitive))
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Attempted to create a package with name containing double slashes. PackageName: %s"), PackageName);
	}

	if( InName.EndsWith( TEXT( "." ), ESearchCase::CaseSensitive ) )
	{
		FString InName2 = InName.Left( InName.Len() - 1 );
		UE_LOG(LogUObjectGlobals, Log,  TEXT( "Invalid Package Name entered - '%s' renamed to '%s'" ), *InName, *InName2 );
		InName = InName2;
	}

	if(InName.Len() == 0)
	{
		InName = MakeUniqueObjectName( InOuter, UPackage::StaticClass() ).ToString();
	}

	ResolveName( InOuter, InName, true, false );


	UPackage* Result = NULL;
	if ( InName.Len() == 0 )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), TEXT("Attempted to create a package with an empty package name.") );
	}

	if ( InName != TEXT("None") )
	{
		Result = FindObject<UPackage>( InOuter, *InName );
		if( Result == NULL )
		{
			FName NewPackageName(*InName, FNAME_Add);
			if (FPackageName::IsShortPackageName(NewPackageName))
			{
				UE_LOG(LogUObjectGlobals, Warning, TEXT("Attempted to create a package with a short package name: %s Outer: %s"), PackageName, InOuter ? *InOuter->GetFullName() : TEXT("NullOuter"));
			}
			else
			{
				Result = NewObject<UPackage>(InOuter, NewPackageName, RF_Public);
			}
		}
	}
	else
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), TEXT("Attempted to create a package named 'None'") );
	}

	return Result;
}

FString ResolveIniObjectsReference(const FString& ObjectReference, const FString* IniFilename, bool bThrow)
{
	if (!IniFilename)
	{
		IniFilename = GetIniFilenameFromObjectsReference(ObjectReference);
	}

	if (!IniFilename)
	{
		return ObjectReference;
	}

	// Get .ini key and section.
	FString Section = ObjectReference.Mid(1 + ObjectReference.Find(TEXT(":"), ESearchCase::CaseSensitive));
	int32 i = Section.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	FString Key;
	if (i != -1)
	{
		Key = Section.Mid(i + 1);
		Section = Section.Left(i);
	}

	FString Output;

	// Look up name.
	if (!GConfig->GetString(*Section, *Key, Output, *IniFilename))
	{
		if (bThrow == true)
		{
			UE_LOG(LogUObjectGlobals, Error, TEXT(" %s %s "), *FString::Printf(TEXT("Can't find '%s' in configuration file section=%s key=%s"), *ObjectReference, *Section, *Key), **IniFilename);
		}
	}

	return Output;
}

const FString* GetIniFilenameFromObjectsReference(const FString& Name)
{
	// See if the name is specified in the .ini file.
	if (FCString::Strnicmp(*Name, TEXT("engine-ini:"), FCString::Strlen(TEXT("engine-ini:"))) == 0)
	{
		return &GEngineIni;
	}
	else if (FCString::Strnicmp(*Name, TEXT("game-ini:"), FCString::Strlen(TEXT("game-ini:"))) == 0)
	{
		return &GGameIni;
	}
	else if (FCString::Strnicmp(*Name, TEXT("input-ini:"), FCString::Strlen(TEXT("input-ini:"))) == 0)
	{
		return &GInputIni;
	}
	else if (FCString::Strnicmp(*Name, TEXT("editor-ini:"), FCString::Strlen(TEXT("editor-ini:"))) == 0)
	{
		return &GEditorIni;
	}

	return nullptr;
}

//
// Resolve a package and name.
//
bool ResolveName(UObject*& InPackage, FString& InOutName, bool Create, bool Throw, uint32 LoadFlags /*= LOAD_None*/)
{
	// Strip off the object class.
	ConstructorHelpers::StripObjectClass( InOutName );

	InOutName = FPackageName::GetDelegateResolvedPackagePath(InOutName);

	// if you're attempting to find an object in any package using a dotted name that isn't fully
	// qualified (such as ObjectName.SubobjectName - notice no package name there), you normally call
	// StaticFindObject and pass in ANY_PACKAGE as the value for InPackage.  When StaticFindObject calls ResolveName,
	// it passes NULL as the value for InPackage, rather than ANY_PACKAGE.  As a result, unless the first chunk of the
	// dotted name (i.e. ObjectName from the above example) is a UPackage, the object will not be found.  So here we attempt
	// to detect when this has happened - if we aren't attempting to create a package, and a UPackage with the specified
	// name couldn't be found, pass in ANY_PACKAGE as the value for InPackage to the call to FindObject<UObject>().
	bool bSubobjectPath = false;

	// Handle specified packages.
	int32 DotIndex = INDEX_NONE;// InOutName.Find(TEXT("."), ESearchCase::CaseSensitive);

	// to make parsing the name easier, replace the subobject delimiter with an extra dot
	InOutName.ReplaceInline(SUBOBJECT_DELIMITER, TEXT(".."), ESearchCase::CaseSensitive);
	while ((DotIndex = InOutName.Find(TEXT("."), ESearchCase::CaseSensitive)) != INDEX_NONE)
	{
		FString PartialName = InOutName.Left(DotIndex);

		// if the next part of InOutName ends in two dots, it indicates that the next object in the path name
		// is not a top-level object (i.e. it's a subobject).  e.g. SomePackage.SomeGroup.SomeObject..Subobject
		if (InOutName.IsValidIndex(DotIndex+1) && InOutName[DotIndex+1] == TEXT('.'))
		{
			InOutName.RemoveAt(DotIndex, 1, false);
			bSubobjectPath = true;
			Create         = false;
		}

		FName* ScriptPackageName = nullptr;
		if (!bSubobjectPath)
		{
			// In case this is a short script package name, convert to long name before passing to CreatePackage/FindObject.
			ScriptPackageName = FPackageName::FindScriptPackageName(*PartialName);
			if (ScriptPackageName)
			{
				PartialName = ScriptPackageName->ToString();
			}
		}

		// Process any package redirects before calling CreatePackage/FindObject
		{
			const FCoreRedirectObjectName NewPackageName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, *PartialName));
			PartialName = NewPackageName.PackageName.ToString();
		}

		// Only long package names are allowed so don't even attempt to create one because whatever the name represents
		// it's not a valid package name anyway.
		
		if (!Create)
		{
			UObject* NewPackage = FindObject<UPackage>( InPackage, *PartialName );
			if( !NewPackage )
			{
				NewPackage = FindObject<UObject>( InPackage == NULL ? ANY_PACKAGE : InPackage, *PartialName );
				if( !NewPackage )
				{
					return bSubobjectPath;
				}
			}
			InPackage = NewPackage;
		}
		else if (!FPackageName::IsShortPackageName(PartialName))
		{
			// Try to find the package in memory first, should be faster than attempting to load or create
			InPackage = StaticFindObjectFast(UPackage::StaticClass(), InPackage, *PartialName);
			if (!ScriptPackageName && !InPackage)
			{
				InPackage = LoadPackage(dynamic_cast<UPackage*>(InPackage), *PartialName, LoadFlags);
			}
			if (!InPackage)
			{
				InPackage = CreatePackage(InPackage, *PartialName);
			}

			check(InPackage);
		}
		InOutName.RemoveAt(0, DotIndex + 1, false);
	}

	return true;
}

/**
 * Parse an object from a text representation
 *
 * @param Stream			String containing text to parse
 * @param Match				Tag to search for object representation within string
 * @param Class				The class of the object to be loaded.
 * @param DestRes			returned uobject pointer
 * @param InParent			Outer to search
 * @param bInvalidObject	[opt] Optional output.  If true, Tag was matched but the specified object wasn't found.
 *
 * @return true if the object parsed successfully
 */
bool ParseObject( const TCHAR* Stream, const TCHAR* Match, UClass* Class, UObject*& DestRes, UObject* InParent, bool* bInvalidObject )
{
	TCHAR TempStr[1024];
	if( !FParse::Value( Stream, Match, TempStr, ARRAY_COUNT(TempStr) ) )
	{
		// Match not found
		return 0;
	}
	else if( FCString::Stricmp(TempStr,TEXT("NONE"))==0 )
	{
		// Match found, object explicit set to be None
		DestRes = NULL;
		return 1;
	}
	else
	{
		// Look this object up.
		UObject* Res = StaticFindObject( Class, InParent, TempStr );
		if( !Res )
		{
			// Match found, object not found
			if (bInvalidObject)
			{
				*bInvalidObject = true;
			}
			return 0;
		}

		// Match found, object found
		DestRes = Res;
		return 1;
	}
}

/**
 * Find or load an object by string name with optional outer and filename specifications.
 * These are optional because the InName can contain all of the necessary information.
 *
 * @param ObjectClass	The class (or a superclass) of the object to be loaded.
 * @param InOuter		An optional object to narrow where to find/load the object from
 * @param InName		String name of the object. If it's not fully qualified, InOuter and/or Filename will be needed
 * @param Filename		An optional file to load from (or find in the file's package object)
 * @param LoadFlags		Flags controlling how to handle loading from disk
 * @param Sandbox		A list of packages to restrict the search for the object
 * @param bAllowObjectReconciliation	Whether to allow the object to be found via FindObject in the case of seek free loading
 *
 * @return The object that was loaded or found. NULL for a failure.
 */
UObject* StaticLoadObjectInternal(UClass* ObjectClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, uint32 LoadFlags, UPackageMap* Sandbox, bool bAllowObjectReconciliation)
{
	SCOPE_CYCLE_COUNTER(STAT_LoadObject);
	check(ObjectClass);
	check(InName);

	FScopedLoadingState ScopedLoadingState(InName);
	FString StrName = InName;
	UObject* Result = nullptr;
	const bool bContainsObjectName = !!FCString::Strstr(InName, TEXT("."));

	// break up the name into packages, returning the innermost name and its outer
	ResolveName(InOuter, StrName, true, true, LoadFlags & (LOAD_EditorOnly | LOAD_Quiet | LOAD_NoWarn));
	if (InOuter)
	{
		// If we have a full UObject name then attempt to find the object in memory first,
		if (bAllowObjectReconciliation && (bContainsObjectName
#if WITH_EDITOR
			|| GIsImportingT3D
#endif
			))
		{
			Result = StaticFindObjectFast(ObjectClass, InOuter, *StrName);
			if (Result && Result->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded))
			{
				// Object needs loading so load it before returning
				Result = nullptr;
			}
		}

		if (!Result)
		{
			if (!InOuter->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn))
			{
				// now that we have one asset per package, we load the entire package whenever a single object is requested
				LoadPackage(NULL, *InOuter->GetOutermost()->GetName(), LoadFlags & ~LOAD_Verify);
			}

			// now, find the object in the package
			Result = StaticFindObjectFast(ObjectClass, InOuter, *StrName);
			if (GEventDrivenLoaderEnabled && Result && Result->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WillBeLoaded))
			{
				UE_LOG(LogUObjectGlobals, Fatal, TEXT("Return an object still needing load from StaticLoadObjectInternal %s"), *GetFullNameSafe(Result));
			}

			// If the object was not found, check for a redirector and follow it if the class matches
			if (!Result && !(LoadFlags & LOAD_NoRedirects))
			{
				UObjectRedirector* Redirector = FindObjectFast<UObjectRedirector>(InOuter, *StrName);
				if (Redirector && Redirector->DestinationObject && Redirector->DestinationObject->IsA(ObjectClass))
				{
					return Redirector->DestinationObject;
				}
			}
		}
	}

	if (!Result && !bContainsObjectName)
	{
		// Assume that the object we're trying to load is the main asset inside of the package 
		// which usually has the same name as the short package name.
		StrName = InName;
		StrName += TEXT(".");
		StrName += FPackageName::GetShortName(InName);
		Result = StaticLoadObjectInternal(ObjectClass, InOuter, *StrName, Filename, LoadFlags, Sandbox, bAllowObjectReconciliation);
	}
#if WITH_EDITORONLY_DATA
	else if (Result && !(LoadFlags & LOAD_EditorOnly))
	{
		Result->GetOutermost()->SetLoadedByEditorPropertiesOnly(false);
	}
#endif

	return Result;
}

UObject* StaticLoadObject(UClass* ObjectClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, uint32 LoadFlags, UPackageMap* Sandbox, bool bAllowObjectReconciliation )
{
	UE_CLOG(FUObjectThreadContext::Get().IsRoutingPostLoad && IsInAsyncLoadingThread(), LogUObjectGlobals, Warning, TEXT("Calling StaticLoadObject during PostLoad may result in hitches during streaming."));

	UObject* Result = StaticLoadObjectInternal(ObjectClass, InOuter, InName, Filename, LoadFlags, Sandbox, bAllowObjectReconciliation);
	if (!Result)
	{
		FString ObjectName = InName;
		ResolveName(InOuter, ObjectName, true, true, LoadFlags & LOAD_EditorOnly);

		if (InOuter == nullptr || FLinkerLoad::IsKnownMissingPackage(FName(*InOuter->GetPathName())) == false)
		{
			// we haven't created or found the object, error
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ClassName"), FText::FromString(ObjectClass->GetName()));
			Arguments.Add(TEXT("OuterName"), InOuter ? FText::FromString(InOuter->GetPathName()) : NSLOCTEXT("Core", "None", "None"));
			Arguments.Add(TEXT("ObjectName"), FText::FromString(ObjectName));
			const FString Error = FText::Format(NSLOCTEXT("Core", "ObjectNotFound", "Failed to find object '{ClassName} {OuterName}.{ObjectName}'"), Arguments).ToString();
			SafeLoadError(InOuter, LoadFlags, *Error);

			if (InOuter)
			{
				// Stop future repeated warnings
				FLinkerLoad::AddKnownMissingPackage(FName(*InOuter->GetPathName()));
			}
		}
	}
	return Result;
}

//
// Load a class.
//
UClass* StaticLoadClass( UClass* BaseClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, uint32 LoadFlags, UPackageMap* Sandbox )
{
	check(BaseClass);

	UClass* Class = LoadObject<UClass>( InOuter, InName, Filename, LoadFlags, Sandbox );
	if( Class && !Class->IsChildOf(BaseClass) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ClassName"), FText::FromString( Class->GetFullName() ));
		Arguments.Add(TEXT("BaseClassName"), FText::FromString( BaseClass->GetFullName() ));
		const FString Error = FText::Format( NSLOCTEXT( "Core", "LoadClassMismatch", "{ClassName} is not a child class of {BaseClassName}" ), Arguments ).ToString();
		SafeLoadError(InOuter, LoadFlags, *Error);

		// return NULL class due to error
		Class = NULL;
	}
	return Class;
}

#if WITH_EDITOR
#include "StackTracker.h"
class FDiffFileArchive : public FArchiveProxy
{
private:
	FArchive* DiffArchive;
	FArchive* InnerArchivePtr;
	bool bDisable;
	TArray<FName> DebugDataStack;
public:
	FDiffFileArchive(FArchive* InDiffArchive, FArchive* InInnerArchive) : FArchiveProxy(*InInnerArchive), DiffArchive(InDiffArchive), InnerArchivePtr(InInnerArchive), bDisable(false)
	{
	}

	~FDiffFileArchive()
	{
		if (InnerArchivePtr)
			delete InnerArchivePtr;

		if (DiffArchive)
			delete DiffArchive;
	}

	virtual void PushDebugDataString(const FName& DebugData) override
	{
		FArchiveProxy::PushDebugDataString(DebugData);
		DebugDataStack.Add(DebugData);
	}

	virtual void PopDebugDataString() override
	{
		FArchiveProxy::PopDebugDataString();
		DebugDataStack.Pop();
	}

	virtual void Serialize(void* V, int64 Length) override
	{
		int64 Pos = InnerArchive.Tell();
		InnerArchive.Serialize(V, Length);

		if (DiffArchive && !bDisable)
		{
			TArray<uint8> Data;
			Data.AddUninitialized(Length);
			DiffArchive->Seek(Pos);
			DiffArchive->Serialize(Data.GetData(), Length);

			if (FMemory::Memcmp((const void*)Data.GetData(), V, Length) != 0)
			{
				// get the calls debug callstack and 
				FString DebugStackString;
				for (const auto& DebugData : DebugDataStack)
				{
					DebugStackString += DebugData.ToString();
					DebugStackString += TEXT("->");
				}

				UE_LOG(LogUObjectGlobals, Warning, TEXT("Diff cooked package archive recognized a difference %lld Filename %s"), Pos, *InnerArchive.GetArchiveName());

				UE_LOG(LogUObjectGlobals, Warning, TEXT("debug stack %s"), *DebugStackString);


				FStackTracker TempTracker(NULL, NULL, true);
				TempTracker.CaptureStackTrace(1);
				TempTracker.DumpStackTraces(0, *GLog);
				TempTracker.ResetTracking();

				// only log one message per archive, from this point the entire package is probably messed up
				bDisable = true;

				static int i = 0;
				i++;
			}
		}
	}
};

// this class is a hack to work around calling private functions int he linker 
// I just want to replace the Linkers loader with a custom one
class FUnsafeLinkerLoad : public FLinkerLoad
{
public:
	FUnsafeLinkerLoad(UPackage *Package, const TCHAR* FileName, const TCHAR* DiffFilename, uint32 LoadFlags) : FLinkerLoad(Package, FileName, LoadFlags)
	{
		Package->LinkerLoad = this;

		/*while (CreateLoader(TFunction<void()>([]() {})) == FLinkerLoad::LINKER_TimedOut)
		{
		}*/


		

		while ( Tick(0.0, false, false) == FLinkerLoad::LINKER_TimedOut ) 
		{ 
		}

		FArchive* OtherFile = IFileManager::Get().CreateFileReader(DiffFilename);
		FDiffFileArchive* DiffArchive = new FDiffFileArchive(Loader, OtherFile);
		Loader = DiffArchive;

	}
};

#endif

/**
* Loads a package and all contained objects that match context flags.
*
* @param	InOuter		Package to load new package into (usually NULL or ULevel->GetOuter())
* @param	Filename	Long package name to load.
* @param	LoadFlags	Flags controlling loading behavior
* @param	ImportLinker	Linker that requests this package through one of its imports
* @return	Loaded package if successful, NULL otherwise
*/
UPackage* LoadPackageInternal(UPackage* InOuter, const TCHAR* InLongPackageNameOrFilename, uint32 LoadFlags, FLinkerLoad* ImportLinker)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("LoadPackageInternal"), STAT_LoadPackageInternal, STATGROUP_ObjectVerbose);

	UPackage* Result = nullptr;

	if (FPlatformProperties::RequiresCookedData() && GEventDrivenLoaderEnabled
		&& EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME
		)
	{
		FString InName;
		FString InPackageName;

		if (FPackageName::IsPackageFilename(InLongPackageNameOrFilename))
		{
			FPackageName::TryConvertFilenameToLongPackageName(InLongPackageNameOrFilename, InPackageName);
		}
		else
		{
			InPackageName = InLongPackageNameOrFilename;
		}

		if (InOuter)
		{
			InName = InOuter->GetPathName();
		}
		else
		{
			InName = InPackageName;
		}

		FName PackageFName(*InPackageName);

		{
			int32 RequestID = LoadPackageAsync(InName, nullptr, *InPackageName);
			FlushAsyncLoading(RequestID);
		}

		Result = FindObjectFast<UPackage>(nullptr, PackageFName);
		return Result;
	}

	FString FileToLoad;
#if WITH_EDITOR
	FString DiffFileToLoad;
#endif

#if WITH_EDITOR
	if (LoadFlags & LOAD_ForFileDiff)
	{
		FString TempFilenames = InLongPackageNameOrFilename;
		ensure(TempFilenames.Split(TEXT(";"), &FileToLoad, &DiffFileToLoad, ESearchCase::CaseSensitive));
	}
	else
#endif
	if (InLongPackageNameOrFilename && FCString::Strlen(InLongPackageNameOrFilename) > 0)
	{
		FileToLoad = InLongPackageNameOrFilename;
	}
	else if (InOuter)
	{
		FileToLoad = InOuter->GetName();
	}

	// Make sure we're trying to load long package names only.
	if (FPackageName::IsShortPackageName(FileToLoad))
	{
		FString LongPackageName;
		FName* ScriptPackageName = FPackageName::FindScriptPackageName(*FileToLoad);
		if (ScriptPackageName)
		{
			UE_LOG(LogUObjectGlobals, Warning, TEXT("LoadPackage: %s is a short script package name."), InLongPackageNameOrFilename);
			FileToLoad = ScriptPackageName->ToString();
		}
		else if (!FPackageName::SearchForPackageOnDisk(FileToLoad, &FileToLoad))
		{
			UE_LOG(LogUObjectGlobals, Warning, TEXT("LoadPackage can't find package %s."), *FileToLoad);
			return NULL;
		}
	}
#if WITH_EDITOR
	// In the editor loading cannot be part of a transaction as it cannot be undone, and may result in recording half-loaded objects. So we suppress any active transaction while in this stack, and set the editor loading flag
	TGuardValue<ITransaction*> SuppressTransaction(GUndo, nullptr);
	TGuardValue<bool> IsEditorLoadingPackage(GIsEditorLoadingPackage, GIsEditor || GIsEditorLoadingPackage);
#endif

	FScopedSlowTask SlowTask(100, FText::Format(NSLOCTEXT("Core", "LoadingPackage_Scope", "Loading Package '{0}'"), FText::FromString(FileToLoad)), ShouldReportProgress());
	SlowTask.Visibility = ESlowTaskVisibility::Invisible;
	
	SlowTask.EnterProgressFrame(10);

	// Try to load.
	BeginLoad(InLongPackageNameOrFilename);

	bool bFullyLoadSkipped = false;

	SlowTask.EnterProgressFrame(30);

	// Declare here so that the linker does not get destroyed before ResetLoaders is called
	FLinkerLoad* Linker = nullptr;
	{
		// Keep track of start time.
		const double StartTime = FPlatformTime::Seconds();

		// Create a new linker object which goes off and tries load the file.
#if WITH_EDITOR
		if (LoadFlags & LOAD_ForFileDiff)
		{
			// Create the package with the provided long package name.
			if (!InOuter)
			{
				InOuter = CreatePackage(nullptr, *FileToLoad);
			}
			
			new FUnsafeLinkerLoad(InOuter, *FileToLoad, *DiffFileToLoad, LOAD_ForDiff);
		}
#endif

		Linker = GetPackageLinker(InOuter, *FileToLoad, LoadFlags, nullptr, nullptr);
		
		if (!Linker)
		{
			EndLoad();
			return nullptr;
		}

		Result = Linker->LinkerRoot;
		checkf(Result, TEXT("LinkerRoot is null"));

		auto EndLoadAndCopyLocalizationGatherFlag = [&]
		{
			EndLoad();
			// Set package-requires-localization flags from archive after loading. This reinforces flagging of packages that haven't yet been resaved.
			Result->ThisRequiresLocalizationGather(Linker->RequiresLocalizationGather());
		};

#if WITH_EDITORONLY_DATA
		if (!(LoadFlags & (LOAD_IsVerifying|LOAD_EditorOnly)))
		{
			bool bIsEditorOnly = false;
			UProperty* SerializingProperty = ImportLinker ? ImportLinker->GetSerializedProperty() : nullptr;
			
			// Check property parent chain
			while (SerializingProperty)
			{
				if (SerializingProperty->IsEditorOnlyProperty())
				{
					bIsEditorOnly = true;
					break;
				}
				SerializingProperty = Cast<UProperty>(SerializingProperty->GetOuter());
			}

			if (!bIsEditorOnly)
			{
				// If this package hasn't been loaded as part of import verification and there's no import linker or the
				// currently serialized property is not editor-only mark this package as runtime.
				Result->SetLoadedByEditorPropertiesOnly(false);
			}
		}
#endif

		if (Result->HasAnyFlags(RF_WasLoaded))
		{
			// The linker is associated with a package that has already been loaded.
			// Loading packages that have already been loaded is unsupported.
			EndLoadAndCopyLocalizationGatherFlag();			
			return Result;
		}

		// The time tracker keeps track of time spent in LoadPackage.
		FExclusiveLoadPackageTimeTracker::FScopedPackageTracker Tracker(Result);

		// If we are loading a package for diffing, set the package flag
		if(LoadFlags & LOAD_ForDiff)
		{
			Result->SetPackageFlags(PKG_ForDiffing);
		}

		// Save the filename we load from
		Result->FileName = FName(*FileToLoad);

		// is there a script SHA hash for this package?
		uint8 SavedScriptSHA[20];
		bool bHasScriptSHAHash = FSHA1::GetFileSHAHash(*Linker->LinkerRoot->GetName(), SavedScriptSHA, false);
		if (bHasScriptSHAHash)
		{
			// if there is, start generating the SHA for any script code in this package
			Linker->StartScriptSHAGeneration();
		}

		SlowTask.EnterProgressFrame(30);

		uint32 DoNotLoadExportsFlags = LOAD_Verify;
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
		// if this linker already has the DeferDependencyLoads flag, then we're
		// already loading it earlier up the load chain (don't let it invoke any
		// deeper loads that may introduce a circular dependency)
		DoNotLoadExportsFlags |= LOAD_DeferDependencyLoads;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

		if ((LoadFlags & DoNotLoadExportsFlags) == 0)
		{
			// Make sure we pass the property that's currently being serialized by the linker that owns the import 
			// that triggered this LoadPackage call
			FSerializedPropertyScope SerializedProperty(*Linker, ImportLinker ? ImportLinker->GetSerializedProperty() : Linker->GetSerializedProperty());
			Linker->LoadAllObjects(GEventDrivenLoaderEnabled);
		}
		else
		{
			bFullyLoadSkipped = true;
		}

		SlowTask.EnterProgressFrame(30);

		Linker->FinishExternalReadDependencies(0.0);

		EndLoadAndCopyLocalizationGatherFlag();

#if WITH_EDITOR
		GIsEditorLoadingPackage = *IsEditorLoadingPackage;
#endif

		// if we are calculating the script SHA for a package, do the comparison now
		if (bHasScriptSHAHash)
		{
			// now get the actual hash data
			uint8 LoadedScriptSHA[20];
			Linker->GetScriptSHAKey(LoadedScriptSHA);

			// compare SHA hash keys
			if (FMemory::Memcmp(SavedScriptSHA, LoadedScriptSHA, 20) != 0)
			{
				appOnFailSHAVerification(*Linker->Filename, false);
			}
		}

		// Only set time it took to load package if the above EndLoad is the "outermost" EndLoad.
		if( Result && !IsLoading() && !(LoadFlags & LOAD_Verify) )
		{
			Result->SetLoadTime( FPlatformTime::Seconds() - StartTime );
		}

		Linker->Flush();

		if (!FPlatformProperties::RequiresCookedData())
		{
			// Flush cache on uncooked platforms to free precache memory
			Linker->FlushCache();
		}

		// With UE4 and single asset per package, we load so many packages that some platforms will run out
		// of file handles. So, this will close the package, but just things like bulk data loading will
		// fail, so we only currently do this when loading on consoles.
		// The only exception here is when we're in the middle of async loading where we can't reset loaders yet. This should only happen when
		// doing synchronous load in the middle of streaming.
		if (FPlatformProperties::RequiresCookedData())
		{
			if (!IsInAsyncLoadingThread())
			{
				if (FUObjectThreadContext::Get().ObjBeginLoadCount == 0)
				{
					// Sanity check to make sure that Linker is the linker that loaded our Result package or the linker has already been detached
					check(!Result || Result->LinkerLoad == Linker || Result->LinkerLoad == nullptr);
					if (Result && Linker->Loader)
					{
						ResetLoaders(Result);
					}
					// Reset loaders could have already deleted Linker so guard against deleting stale pointers
					if (Result && Result->LinkerLoad)
					{
						delete Linker->Loader;
						Linker->Loader = nullptr;
					}
					// And make sure no one can use it after it's been deleted
					Linker = nullptr;
				}
				// Async loading removes delayed linkers on the game thread after streaming has finished
				else
				{
					FUObjectThreadContext::Get().DelayedLinkerClosePackages.AddUnique(Linker);
				}
			}
			else
			{
				FUObjectThreadContext::Get().DelayedLinkerClosePackages.AddUnique(Linker);
			}
		}
	}

	if (!bFullyLoadSkipped)
	{
		// Mark package as loaded.
		Result->SetFlags(RF_WasLoaded);
	}

	return Result;
}

UPackage* LoadPackage(UPackage* InOuter, const TCHAR* InLongPackageName, uint32 LoadFlags)
{
	COOK_STAT(LoadPackageStats::NumPackagesLoaded++);
	COOK_STAT(FScopedDurationTimer LoadTimer(LoadPackageStats::LoadPackageTimeSec));
	// Change to 1 if you want more detailed stats for loading packages, but at the cost of adding dynamic stats.
#if	STATS && 0
	static FString Package = TEXT( "Package" );
	const FString LongName = Package / InLongPackageName;
	const TStatId StatId = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_UObjects>( LongName );
	FScopeCycleCounter CycleCounter( StatId );
#endif // STATS

	// since we are faking the object name, this is basically a duplicate of LLM_SCOPED_TAG_WITH_OBJECT_IN_SET
	FString FakePackageName = FString(TEXT("Package ")) + InLongPackageName;
	LLM_SCOPED_TAG_WITH_STAT_NAME_IN_SET(FLowLevelMemTracker::Get().IsTagSetActive(ELLMTagSet::Assets) ? FDynamicStats::CreateMemoryStatId<FStatGroup_STATGROUP_LLMAssets>(FName(*FakePackageName)).GetName() : NAME_None, ELLMTagSet::Assets, ELLMTracker::Default);
	return LoadPackageInternal(InOuter, InLongPackageName, LoadFlags, /*ImportLinker =*/ nullptr);
}

/**
 * Returns whether we are currently loading a package (sync or async)
 *
 * @return true if we are loading a package, false otherwise
 */
bool IsLoading()
{
	check(FUObjectThreadContext::Get().ObjBeginLoadCount >= 0);
	return FUObjectThreadContext::Get().ObjBeginLoadCount > 0;
}

//
// Begin loading packages.
//warning: Objects may not be destroyed between BeginLoad/EndLoad calls.
//
void BeginLoad(const TCHAR* DebugContext)
{
	auto& ThreadContext = FUObjectThreadContext::Get();
	if (ThreadContext.ObjBeginLoadCount == 0 && !IsInAsyncLoadingThread())
	{
		if (IsAsyncLoading() && (DebugContext != nullptr))
		{
			UE_LOG(LogUObjectGlobals, Log, TEXT("BeginLoad(%s) is flushing async loading"), DebugContext);
		}

		// Make sure we're finishing up all pending async loads, and trigger texture streaming next tick if necessary.
		FlushAsyncLoading();

		// Validate clean load state.
		check(ThreadContext.ObjLoaded.Num() == 0);
	}

	++ThreadContext.ObjBeginLoadCount;
}

// Sort objects by linker name and file offset
struct FCompareUObjectByLinkerAndOffset
{
	FORCEINLINE bool operator()( const UObject& A, const UObject &B ) const
	{
		FLinker* LinkerA = A.GetLinker();
		FLinker* LinkerB = B.GetLinker();

		// Both objects have linkers.
		if( LinkerA && LinkerB )
		{
			// Identical linkers, sort by offset in file.
			if( LinkerA == LinkerB )
			{
				FObjectExport& ExportA = LinkerA->ExportMap[ A.GetLinkerIndex() ];
				FObjectExport& ExportB = LinkerB->ExportMap[ B.GetLinkerIndex() ];
				return ExportA.SerialOffset < ExportB.SerialOffset;
			}
			// Sort by pointer address.
			else
			{
				return LinkerA < LinkerB;
			}
		}
		// Neither objects have a linker, don't do anything.
		else if( LinkerA == LinkerB )
		{
			return false;
		}
		// Sort objects with linkers vs. objects without
		else
		{
			return (LinkerA != NULL);
		}
	}
};

//
// End loading packages.
//
void EndLoad()
{
	auto& ThreadContext = FUObjectThreadContext::Get();

	check(ThreadContext.ObjBeginLoadCount > 0);
	if (IsInAsyncLoadingThread())
	{
		ThreadContext.ObjBeginLoadCount--;
		return;
	}

#if WITH_EDITOR
	FScopedSlowTask SlowTask(0, NSLOCTEXT("Core", "PerformingPostLoad", "Performing post-load..."), ShouldReportProgress());

	int32 NumObjectsLoaded = 0, NumObjectsFound = 0;
#endif

	while (--ThreadContext.ObjBeginLoadCount == 0 && (ThreadContext.ObjLoaded.Num() || ThreadContext.ImportCount || ThreadContext.ForcedExportCount))
	{
		// The time tracker keeps track of time spent in EndLoad.
		FExclusiveLoadPackageTimeTracker::FScopedEndLoadTracker Tracker;

		// Make sure we're not recursively calling EndLoad as e.g. loading a config file could cause
		// BeginLoad/EndLoad to be called.
		ThreadContext.ObjBeginLoadCount++;

		// Temporary list of loaded objects as GObjLoaded might expand during iteration.
		TArray<UObject*> ObjLoaded;
		TSet<FLinkerLoad*> LoadedLinkers;
		while (ThreadContext.ObjLoaded.Num())
		{
			// Accumulate till GObjLoaded no longer increases.
			ObjLoaded += ThreadContext.ObjLoaded;
			ThreadContext.ObjLoaded.Empty();

			// Sort by Filename and Offset.
			ObjLoaded.Sort(FCompareUObjectByLinkerAndOffset());

			// Finish loading everything.
			for (int32 i = 0; i < ObjLoaded.Num(); i++)
			{
				// Preload.
				UObject* Obj = ObjLoaded[i];
				if (Obj->HasAnyFlags(RF_NeedLoad))
				{
					check(Obj->GetLinker());
					Obj->GetLinker()->Preload(Obj);
				}
			}

			// Start over again as new objects have been loaded that need to have "Preload" called on them before
			// we can safely PostLoad them.
			if (ThreadContext.ObjLoaded.Num())
			{
				continue;
			}

#if WITH_EDITOR
			SlowTask.CompletedWork = SlowTask.TotalAmountOfWork;
			SlowTask.TotalAmountOfWork += ObjLoaded.Num();
			SlowTask.CurrentFrameScope = 0;
#endif

			if (GIsEditor)
			{
				for (int32 i = 0; i < ObjLoaded.Num(); i++)
				{
					UObject* Obj = ObjLoaded[i];
					if (Obj->GetLinker())
					{
						LoadedLinkers.Add(Obj->GetLinker());
					}
				}
			}

			{
				// set this so that we can perform certain operations in which are only safe once all objects have been de-serialized.
				TGuardValue<bool> GuardIsRoutingPostLoad(FUObjectThreadContext::Get().IsRoutingPostLoad, true);

				// Postload objects.
				for (int32 i = 0; i < ObjLoaded.Num(); i++)
				{

					UObject* Obj = ObjLoaded[i];
					check(Obj);
#if WITH_EDITOR
					SlowTask.EnterProgressFrame(1, FText::Format(NSLOCTEXT("Core", "FinalizingUObject", "Finalizing load of {0}"), FText::FromString(Obj->GetName())));
#endif
					Obj->ConditionalPostLoad();
				}
			}

			if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
			{
#if DO_CHECK
				for (UObject* Obj : ObjLoaded)
				{
					if (UDynamicClass* DynamicClass = Cast<UDynamicClass>(Obj))
					{
						check((DynamicClass->ClassFlags & CLASS_Constructed) != 0);
						check(DynamicClass->GetDefaultObject(false)); // this should have already been done
					}
				}
#endif
			}
			else
			{
				// Dynamic Class doesn't require/use pre-loading (or post-loading). 
				// The CDO is created at this point, because now it's safe to solve cyclic dependencies.
				for (UObject* Obj : ObjLoaded)
				{
					if (UDynamicClass* DynamicClass = Cast<UDynamicClass>(Obj))
					{
						check((DynamicClass->ClassFlags & CLASS_Constructed) != 0);
						DynamicClass->GetDefaultObject(true);
					}
				}
			}
			// Create clusters after all objects have been loaded
			extern int32 GCreateGCClusters;
			if (FPlatformProperties::RequiresCookedData() && !GIsInitialLoad && GCreateGCClusters && !GUObjectArray.IsOpenForDisregardForGC())
			{
				for (UObject* Obj : ObjLoaded)
				{
					check(Obj);
					if (Obj->CanBeClusterRoot())
					{
						Obj->CreateCluster();
					}
				}
			}

#if WITH_EDITOR
			// Send global notification for each object that was loaded.
			// Useful for updating UI such as ContentBrowser's loaded status.
			{
				for( int32 CurObjIndex=0; CurObjIndex<ObjLoaded.Num(); CurObjIndex++ )
				{
					UObject* Obj = ObjLoaded[CurObjIndex];
					check(Obj);
					if ( Obj->IsAsset() )
					{
						FCoreUObjectDelegates::OnAssetLoaded.Broadcast(Obj);
					}
				}
			}
#endif	// WITH_EDITOR

			// Empty array before next iteration as we finished postloading all objects.
			ObjLoaded.Empty(ThreadContext.ObjLoaded.Num());
		}

		if ( GIsEditor && LoadedLinkers.Num() > 0 )
		{
			for (FLinkerLoad* LoadedLinker : LoadedLinkers)
			{
				check(LoadedLinker != nullptr);

				LoadedLinker->FlushCache();

				if (LoadedLinker->LinkerRoot != nullptr && !LoadedLinker->LinkerRoot->IsFullyLoaded())
				{
					bool bAllExportsCreated = true;
					for ( int32 ExportIndex = 0; ExportIndex < LoadedLinker->ExportMap.Num(); ExportIndex++ )
					{
						FObjectExport& Export = LoadedLinker->ExportMap[ExportIndex];
						if ( !Export.bForcedExport && Export.Object == nullptr )
						{
							bAllExportsCreated = false;
							break;
						}
					}

					if ( bAllExportsCreated )
					{
						LoadedLinker->LinkerRoot->MarkAsFullyLoaded();
					}
				}
			}
		}

		// Dissociate all linker import and forced export object references, since they
		// may be destroyed, causing their pointers to become invalid.
		FLinkerManager::Get().DissociateImportsAndForcedExports();

		// close any linkers' loaders that were requested to be closed once GObjBeginLoadCount goes to 0
		TArray<FLinkerLoad*> PackagesToClose = MoveTemp(ThreadContext.DelayedLinkerClosePackages);
		for (FLinkerLoad* Linker : PackagesToClose)
		{
			if (Linker)
			{				
				if (Linker->Loader && Linker->LinkerRoot)
				{
					ResetLoaders(Linker->LinkerRoot);
				}
				check(Linker->Loader == nullptr);				
			}
		}

		FBlueprintSupport::FlushReinstancingQueue();
	}

	// Loaded new objects, so allow reaccessing asset ptrs
	FSoftObjectPath::InvalidateTag();
}

/*-----------------------------------------------------------------------------
	Object name functions.
-----------------------------------------------------------------------------*/

/**
 * Create a unique name by combining a base name and an arbitrary number string.
 * The object name returned is guaranteed not to exist.
 *
 * @param	Parent		the outer for the object that needs to be named
 * @param	Class		the class for the object
 * @param	BaseName	optional base name to use when generating the unique object name; if not specified, the class's name is used
 *
 * @return	name is the form BaseName_##, where ## is the number of objects of this
 *			type that have been created since the last time the class was garbage collected.
 */
FName MakeUniqueObjectName( UObject* Parent, UClass* Class, FName InBaseName/*=NAME_None*/ )
{
	check(Class);
	const FName BaseName = (InBaseName == NAME_None) ? Class->GetFName() : InBaseName;

	FName TestName;
	do
	{
		// cache the class's name's index for faster name creation later
		if (!FPlatformProperties::HasEditorOnlyData() && GFastPathUniqueNameGeneration)
		{
			/*   Fast Path Name Generation
			* A significant fraction of object creation time goes into verifying that the a chosen unique name is really unique.
			* The idea here is to generate unique names using very high numbers and only in situations where collisions are
			* impossible for other reasons.
			*
			* Rationale for uniqueness as used here.
			* - Consoles do not save objects in general, and certainly not animation trees. So we could never load an object that would later clash.
			* - We assume that we never load or create any object with a "name number" as large as, say, MAX_int32 / 2, other than via
			*   HACK_FastPathUniqueNameGeneration.
			* - After using one of these large "name numbers", we decrement the static UniqueIndex, this no two names generated this way, during the
			*   same run, could ever clash.
			* - We assume that we could never create anywhere near MAX_int32/2 total objects at runtime, within a single run.
			* - We require an outer for these items, thus outers must themselves be unique. Therefore items with unique names created on the fast path
			*   could never clash with anything with a different outer. For animation trees, these outers are never saved or loaded, thus clashes are
			*   impossible.
			*/
			static int32 UniqueIndex = MAX_int32 - 1000;
			TestName = FName(BaseName, --UniqueIndex);
			checkSlow(Parent);
			checkSlow(Parent != ANY_PACKAGE);
			checkSlow(!StaticFindObjectFastInternal(NULL, Parent, TestName));
		}
		else
		{
			UObject* ExistingObject;

			do
			{
				// create the next name in the sequence for this class
				if (BaseName.GetComparisonIndex() == NAME_Package)
				{
					if (Parent == NULL)
					{
						//package names should default to "/Temp/Untitled" when their parent is NULL. Otherwise they are a group.
						TestName = FName(*FString::Printf(TEXT("/Temp/%s"), *FName(NAME_Untitled).ToString()), ++Class->ClassUnique);
					}
					else
					{
						//package names should default to "Untitled"
						TestName = FName(NAME_Untitled, ++Class->ClassUnique);
					}
				}
				else
				{
					int32 NameNumber = 0;
					if (Parent && (Parent != ANY_PACKAGE) )
					{
						UPackage* ParentPackage = Parent->GetOutermost();
						int32& ClassUnique = ParentPackage->ClassUniqueNameIndexMap.FindOrAdd(Class->GetFName());
						NameNumber = ++ClassUnique;
					}
					else
					{
						NameNumber = ++Class->ClassUnique;
					}
					TestName = FName(BaseName, NameNumber);
				}

				if (Parent == ANY_PACKAGE)
				{
					ExistingObject = StaticFindObject(NULL, ANY_PACKAGE, *TestName.ToString());
				}
				else
				{
					ExistingObject = StaticFindObjectFastInternal(NULL, Parent, TestName);
				}
			} while (ExistingObject);
		}
	// InBaseName can be a name of an object from a different hierarchy (so it's still unique within given parents scope), we don't want to return the same name.
	} while (TestName == BaseName);
	return TestName;
}

/**
 * Given a display label string, generates an FName slug that is a valid FName for that label.
 * If the object's current name is already satisfactory, then that name will be returned.
 * For example, "[MyObject]: Object Label" becomes "MyObjectObjectLabel" FName slug.
 * 
 * Note: The generated name isn't guaranteed to be unique.
 *
 * @param DisplayLabel The label string to convert to an FName
 * @param CurrentObjectName The object's current name, or NAME_None if it has no name yet
 *
 * @return	The generated object name
 */
FName MakeObjectNameFromDisplayLabel(const FString& DisplayLabel, const FName CurrentObjectName)
{
	FString GeneratedName = SlugStringForValidName(DisplayLabel);

	// If the current object name (without a number) already matches our object's name, then use the existing name
	if( CurrentObjectName.GetPlainNameString() == GeneratedName )
	{
		// The object's current name is good enough!  This avoids renaming objects that don't really need to be renamed.
		return CurrentObjectName;
	}

	// If the new name is empty (for example, because it was composed entirely of invalid characters).
	// then we'll use the current name
	if( GeneratedName.IsEmpty() )
	{
		return CurrentObjectName;
	}

	const FName GeneratedFName( *GeneratedName );
	check( GeneratedFName.IsValidXName( INVALID_OBJECTNAME_CHARACTERS ) );

	return GeneratedFName;
}

/*-----------------------------------------------------------------------------
   Duplicating Objects.
-----------------------------------------------------------------------------*/

struct FObjectDuplicationHelperMethods
{
	// Helper method intended to gather up all default subobjects that have already been created and prepare them for duplication.
	static void GatherDefaultSubobjectsForDuplication(UObject* SrcObject, UObject* DstObject, FUObjectAnnotationSparse<FDuplicatedObject, false>& DuplicatedObjectAnnotation, FDuplicateDataWriter& Writer)
	{
		TArray<UObject*> SrcDefaultSubobjects;
		SrcObject->GetDefaultSubobjects(SrcDefaultSubobjects);
		
		// Iterate over all default subobjects within the source object.
		for (UObject* SrcDefaultSubobject : SrcDefaultSubobjects)
		{
			if (SrcDefaultSubobject)
			{
				// Attempt to find a default subobject with the same name within the destination object.
				UObject* DupDefaultSubobject = DstObject->GetDefaultSubobjectByName(SrcDefaultSubobject->GetFName());
				if (DupDefaultSubobject)
				{
					// Map the duplicated default subobject to the source and register it for serialization.
					DuplicatedObjectAnnotation.AddAnnotation(SrcDefaultSubobject, FDuplicatedObject(DupDefaultSubobject));
					Writer.UnserializedObjects.Add(SrcDefaultSubobject);

					// Recursively gather any nested default subobjects that have already been constructed through CreateDefaultSubobject().
					GatherDefaultSubobjectsForDuplication(SrcDefaultSubobject, DupDefaultSubobject, DuplicatedObjectAnnotation, Writer);
				}
			}
		}
	}
};

/**
 * Constructor - zero initializes all members
 */
FObjectDuplicationParameters::FObjectDuplicationParameters( UObject* InSourceObject, UObject* InDestOuter )
: SourceObject(InSourceObject)
, DestOuter(InDestOuter)
, DestName(NAME_None)
, FlagMask(RF_AllFlags & ~(RF_MarkAsRootSet|RF_MarkAsNative))
, InternalFlagMask(EInternalObjectFlags::AllFlags)
, ApplyFlags(RF_NoFlags)
, ApplyInternalFlags(EInternalObjectFlags::None)
, PortFlags(PPF_None)
, DuplicateMode(EDuplicateMode::Normal)
, DestClass(NULL)
, CreatedObjects(NULL)
{
	checkSlow(SourceObject);
	checkSlow(DestOuter);
	checkSlow(SourceObject->IsValidLowLevel());
	checkSlow(DestOuter->IsValidLowLevel());
	DestClass = SourceObject->GetClass();
}


UObject* StaticDuplicateObject(UObject const* SourceObject, UObject* DestOuter, const FName DestName, EObjectFlags FlagMask, UClass* DestClass, EDuplicateMode::Type DuplicateMode, EInternalObjectFlags InternalFlagsMask)
{
	if (!IsAsyncLoading() && !IsLoading() && SourceObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		// Detach linker for the outer if it already exists, to avoid problems with PostLoad checking the Linker version
		ResetLoaders(DestOuter);
	}

	// @todo: handle const down the callstack.  for now, let higher level code use it and just cast it off
	FObjectDuplicationParameters Parameters(const_cast<UObject*>(SourceObject), DestOuter);
	if ( !DestName.IsNone() )
	{
		Parameters.DestName = DestName;
	}
	else if (SourceObject->GetOuter() != DestOuter)
	{
		// try to keep the object name consistent if possible
		if (FindObjectFast<UObject>(DestOuter, SourceObject->GetFName()) == nullptr)
		{
			Parameters.DestName = SourceObject->GetFName();
		}
	}

	if ( DestClass == NULL )
	{
		Parameters.DestClass = SourceObject->GetClass();
	}
	else
	{
		Parameters.DestClass = DestClass;
	}
	Parameters.FlagMask = FlagMask;
	Parameters.InternalFlagMask = InternalFlagsMask;
	Parameters.DuplicateMode = DuplicateMode;

	if( DuplicateMode == EDuplicateMode::PIE)
	{
		Parameters.PortFlags = PPF_DuplicateForPIE;
	}

	return StaticDuplicateObjectEx(Parameters);
}

UObject* StaticDuplicateObjectEx( FObjectDuplicationParameters& Parameters )
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_StaticDuplicateObject);
	// make sure the two classes are the same size, as this hopefully will mean they are serialization
	// compatible. It's not a guarantee, but will help find errors
	checkf( (Parameters.DestClass->GetPropertiesSize() >= Parameters.SourceObject->GetClass()->GetPropertiesSize()),
		TEXT("Source and destination class sizes differ.  Source: %s (%i)   Destination: %s (%i)"),
		*Parameters.SourceObject->GetClass()->GetName(), Parameters.SourceObject->GetClass()->GetPropertiesSize(),
		*Parameters.DestClass->GetName(), Parameters.DestClass->GetPropertiesSize());
	FObjectInstancingGraph InstanceGraph;

	if( !GIsDuplicatingClassForReinstancing )
	{
	// make sure we are not duplicating RF_RootSet as this flag is special
	// also make sure we are not duplicating the RF_ClassDefaultObject flag as this can only be set on the real CDO
		Parameters.FlagMask &= ~RF_ClassDefaultObject;
		Parameters.InternalFlagMask &= ~EInternalObjectFlags::RootSet;
	}

	// disable object and component instancing while we're duplicating objects, as we're going to instance components manually a little further below
	InstanceGraph.EnableSubobjectInstancing(false);

	// we set this flag so that the component instancing code doesn't think we're creating a new archetype, because when creating a new archetype,
	// the ObjectArchetype for instanced components is set to the ObjectArchetype of the source component, which in the case of duplication (or loading)
	// will be changing the archetype's ObjectArchetype to the wrong object (typically the CDO or something)
	InstanceGraph.SetLoadingObject(true);

	UObject* DupRootObject = Parameters.DuplicationSeed.FindRef(Parameters.SourceObject);
	if ( DupRootObject == NULL )
	{
		DupRootObject = StaticConstructObject_Internal(	Parameters.DestClass,
														Parameters.DestOuter,
														Parameters.DestName,
														Parameters.ApplyFlags | Parameters.SourceObject->GetMaskedFlags(Parameters.FlagMask),
														Parameters.ApplyInternalFlags | (Parameters.SourceObject->GetInternalFlags() & Parameters.InternalFlagMask),
														Parameters.SourceObject->GetArchetype()->GetClass() == Parameters.DestClass
																? Parameters.SourceObject->GetArchetype()
																: NULL,
														true,
														&InstanceGraph
														);
	}

	TArray<uint8> ObjectData;

	FUObjectAnnotationSparse<FDuplicatedObject,false>  DuplicatedObjectAnnotation;

	// if seed objects were specified, add those to the DuplicatedObjects map now
	if ( Parameters.DuplicationSeed.Num() > 0 )
	{
		for ( TMap<UObject*,UObject*>::TIterator It(Parameters.DuplicationSeed); It; ++It )
		{
			UObject* Src = It.Key();
			UObject* Dup = It.Value();
			checkSlow(Src);
			checkSlow(Dup);

			// create the DuplicateObjectInfo for this object
			DuplicatedObjectAnnotation.AddAnnotation( Src, FDuplicatedObject( Dup ) );
		}
	}

	// Read from the source object(s)
	FDuplicateDataWriter Writer(
		DuplicatedObjectAnnotation,	// Ref: Object annotation which stores the duplicated object for each source object
		ObjectData,					// Out: Serialized object data
		Parameters.SourceObject,	// Source object to copy
		DupRootObject,				// Destination object to copy into
		Parameters.FlagMask,		// Flags to be copied for duplicated objects
		Parameters.ApplyFlags,		// Flags to always set on duplicated objects
		Parameters.InternalFlagMask,		// Internal Flags to be copied for duplicated objects
		Parameters.ApplyInternalFlags,		// Internal Flags to always set on duplicated objects
		&InstanceGraph,				// Instancing graph
		Parameters.PortFlags );		// PortFlags

	TArray<UObject*> SerializedObjects;

	
	if (GIsDuplicatingClassForReinstancing)
	{
		FBlueprintSupport::DuplicateAllFields(dynamic_cast<UStruct*>(Parameters.SourceObject), Writer);
	}

	// Add default subobjects to the DuplicatedObjects map so they don't get recreated during serialization.
	FObjectDuplicationHelperMethods::GatherDefaultSubobjectsForDuplication(Parameters.SourceObject, DupRootObject, DuplicatedObjectAnnotation, Writer);

	InstanceGraph.SetDestinationRoot( DupRootObject );
	while(Writer.UnserializedObjects.Num())
	{
		UObject*	Object = Writer.UnserializedObjects.Pop();
		Object->Serialize(Writer);
		SerializedObjects.Add(Object);
	};


	FDuplicateDataReader	Reader(DuplicatedObjectAnnotation, ObjectData, Parameters.PortFlags, Parameters.DestOuter);
	for(int32 ObjectIndex = 0;ObjectIndex < SerializedObjects.Num();ObjectIndex++)
	{
		UObject* SerializedObject = SerializedObjects[ObjectIndex];

		FDuplicatedObject ObjectInfo = DuplicatedObjectAnnotation.GetAnnotation( SerializedObject );
		checkSlow( !ObjectInfo.IsDefault() );

		if ( !SerializedObject->HasAnyFlags(RF_ClassDefaultObject) )
		{
			ObjectInfo.DuplicatedObject->Serialize(Reader);
		}
		else
		{
			// if the source object was a CDO, then transient property values were serialized by the FDuplicateDataWriter
			// and in order to read those properties out correctly, we'll need to enable defaults serialization on the
			// reader as well.
			Reader.StartSerializingDefaults();
			ObjectInfo.DuplicatedObject->Serialize(Reader);
			Reader.StopSerializingDefaults();
		}
	}

	InstanceGraph.EnableSubobjectInstancing(true);

	for( int32 ObjectIndex = 0;ObjectIndex < SerializedObjects.Num(); ObjectIndex++)
	{
		UObject* OrigObject = SerializedObjects[ObjectIndex];

		// don't include any objects which were included in the duplication seed map in the instance graph, as the "duplicate" of these objects
		// may not necessarily be the object that is supposed to be its archetype (the caller can populate the duplication seed map with any objects they wish)
		// and the DuplicationSeed is only used for preserving inter-object references, not for object graphs in SCO and we don't want to call PostDuplicate/PostLoad
		// on them as they weren't actually duplicated
		if ( Parameters.DuplicationSeed.Find(OrigObject) == NULL )
		{
			FDuplicatedObject DupObjectInfo = DuplicatedObjectAnnotation.GetAnnotation( OrigObject );

			UObject* DupObjectArchetype = DupObjectInfo.DuplicatedObject->GetArchetype();

			bool bDuplicateForPIE = (Parameters.PortFlags & PPF_DuplicateForPIE) != 0;

			// Any PIE duplicated object that has the standalone flag is a potential garbage collection issue
			ensure(!(bDuplicateForPIE && DupObjectInfo.DuplicatedObject->HasAnyFlags(RF_Standalone)));

			DupObjectInfo.DuplicatedObject->PostDuplicate(Parameters.DuplicateMode);
			if ( !DupObjectInfo.DuplicatedObject->IsTemplate() )
			{
				// Don't want to call PostLoad on class duplicated CDOs
				TGuardValue<bool> GuardIsRoutingPostLoad(FUObjectThreadContext::Get().IsRoutingPostLoad, true);
				DupObjectInfo.DuplicatedObject->ConditionalPostLoad();
			}
			DupObjectInfo.DuplicatedObject->CheckDefaultSubobjects();
		}
	}

	// if the caller wanted to know which objects were created, do that now
	if ( Parameters.CreatedObjects != NULL )
	{
		// note that we do not clear the map first - this is to allow callers to incrementally build a collection
		// of duplicated objects through multiple calls to StaticDuplicateObject

		// now add each pair of duplicated objects;
		// NOTE: we don't check whether the entry was added from the DuplicationSeed map, so this map
		// will contain those objects as well.
		for(int32 ObjectIndex = 0;ObjectIndex < SerializedObjects.Num();ObjectIndex++)
		{
			UObject* OrigObject = SerializedObjects[ObjectIndex];

			// don't include any objects which were in the DuplicationSeed map, as CreatedObjects should only contain the list
			// of objects actually created during this call to SDO
			if ( Parameters.DuplicationSeed.Find(OrigObject) == NULL )
			{
				FDuplicatedObject DupObjectInfo = DuplicatedObjectAnnotation.GetAnnotation( OrigObject );
				Parameters.CreatedObjects->Add(OrigObject, DupObjectInfo.DuplicatedObject);
			}
		}
	}

	//return DupRoot;
	return DupRootObject;
}


/**
 * Save a copy of this object into the transaction buffer if we are currently recording into
 * one (undo/redo). If bMarkDirty is true, will also mark the package as needing to be saved.
 *
 * @param	bMarkDirty	If true, marks the package dirty if we are currently recording into a
 *						transaction buffer
 * @param	Object		object to save.
*
 * @return	true if a copy of the object was saved and the package potentially marked dirty; false
 *			if we are not recording into a transaction buffer, the package is a PIE/script package,
 *			or the object is not transactional (implies the package was not marked dirty)
 */
bool SaveToTransactionBuffer(UObject* Object, bool bMarkDirty)
{
	bool bSavedToTransactionBuffer = false;

	// Neither PIE world objects nor script packages should end up in the transaction buffer. Additionally, in order
	// to save a copy of the object, we must have a transactor and the object must be transactional.
	const bool IsTransactional = Object->HasAnyFlags(RF_Transactional);
	const bool IsNotPIEOrContainsScriptObject = (Object->GetOutermost()->HasAnyPackageFlags( PKG_PlayInEditor | PKG_ContainsScript) == false);

	if ( GUndo && IsTransactional && IsNotPIEOrContainsScriptObject )
	{
		// Mark the package dirty, if requested
		if ( bMarkDirty )
		{
			Object->MarkPackageDirty();
		}

		// Save a copy of the object to the transactor
		GUndo->SaveObject( Object );
		bSavedToTransactionBuffer = true;
	}

	return bSavedToTransactionBuffer;
}

/**
 * Check for StaticAllocateObject error; only for use with the editor, make or other commandlets.
 * 
 * @param	Class		the class of the object to create
 * @param	InOuter		the object to create this object within (the Outer property for the new object will be set to the value specified here).
 * @param	Name		the name to give the new object. If no value (NAME_None) is specified, the object will be given a unique name in the form of ClassName_#.
 * @param	SetFlags	the ObjectFlags to assign to the new object. some flags can affect the behavior of constructing the object.
 * @return	true if NULL should be returned; there was a problem reported 
 */
bool StaticAllocateObjectErrorTests( UClass* InClass, UObject* InOuter, FName InName, EObjectFlags InFlags)
{
	// Validation checks.
	if( !InClass )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Empty class for object %s"), *InName.ToString() );
		return true;
	}

	// for abstract classes that are being loaded NOT in the editor we want to error.  If they are in the editor we do not want to have an error
	if ( InClass->HasAnyClassFlags(CLASS_Abstract) && (InFlags&RF_ClassDefaultObject) == 0 )
	{
		if ( GIsEditor )
		{
			const FString ErrorMsg = FString::Printf(TEXT("Class which was marked abstract was trying to be loaded.  It will be nulled out on save. %s %s"), *InName.ToString(), *InClass->GetName());
			// if we are trying instantiate an abstract class in the editor we'll warn the user that it will be nulled out on save
			UE_LOG(LogUObjectGlobals, Warning, TEXT("%s"), *ErrorMsg);
			ensureMsgf(false, *ErrorMsg);
		}
		else
		{
			UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), *FString::Printf( TEXT("Can't create object %s: class %s is abstract"), *InName.ToString(), *InClass->GetName()));
			return true;
		}
	}

	if( InOuter == NULL )
	{
		if ( InClass != UPackage::StaticClass() )
		{
			UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), *FString::Printf( TEXT("Object is not packaged: %s %s"), *InClass->GetName(), *InName.ToString()) );
			return true;
		}
		else if ( InName == NAME_None )
		{
			UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), TEXT("Attempted to create a package named 'None'") );
			return true;
		}
	}

	if ( (InFlags & RF_ClassDefaultObject) == 0 )
	{
		if ( InOuter != NULL && !InOuter->IsA(InClass->ClassWithin) )
		{
			UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s"), *FString::Printf( TEXT("Object %s %s created in %s instead of %s"), *InClass->GetName(), *InName.ToString(), *InOuter->GetClass()->GetName(), *InClass->ClassWithin->GetName()) );
			return true;
		}
	}
	return false;
}

/**
* Call back into the async loading code to inform of the creation of a new object
*/
void NotifyConstructedDuringAsyncLoading(UObject* Object, bool bSubObject);

/**
* For object overwrites, the class may want to persist some info over the re-initialize
* this is only used for classes in the script compiler
**/
//@todo UE4 this is clunky
static FRestoreForUObjectOverwrite* ObjectRestoreAfterInitProps = NULL;  

COREUOBJECT_API bool GOutputCookingWarnings = false;


UObject* StaticAllocateObject
(
	UClass*			InClass,
	UObject*		InOuter,
	FName			InName,
	EObjectFlags	InFlags,
	EInternalObjectFlags InternalSetFlags,
	bool bCanRecycleSubobjects,
	bool* bOutRecycledSubobject
)
{
	LLM_SCOPE(ELLMTag::UObject);

	SCOPE_CYCLE_COUNTER(STAT_AllocateObject);
	checkSlow(InOuter != INVALID_OBJECT); // not legal
	check(!InClass || (InClass->ClassWithin && InClass->ClassConstructor));
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (StaticAllocateObjectErrorTests(InClass,InOuter,InName,InFlags))
		{
			return NULL;
		}
	}
#endif // WITH_EDITOR
	bool bCreatingCDO = (InFlags & RF_ClassDefaultObject) != 0;

	check(InClass);
	check(GIsEditor || bCreatingCDO || !InClass->HasAnyClassFlags(CLASS_Abstract)); // this is a warning in the editor, otherwise it is illegal to create an abstract class, except the CDO
	check(InOuter || (InClass == UPackage::StaticClass() && InName != NAME_None)); // only packages can not have an outer, and they must be named explicitly
	check(bCreatingCDO || !InOuter || InOuter->IsA(InClass->ClassWithin));


	if (bCreatingCDO)
	{
		check(InClass->GetClass());
		ensure(!GIsDuplicatingClassForReinstancing || InClass->HasAnyClassFlags(CLASS_Native));
		InName = InClass->GetDefaultObjectName();
		// never call PostLoad on class default objects
		InFlags &= ~(RF_NeedPostLoad|RF_NeedPostLoadSubobjects);
	}

	UObject* Obj = NULL;
	if(InName == NAME_None)
	{
#if WITH_EDITOR
		if ( GOutputCookingWarnings && GetTransientPackage() != InOuter->GetOutermost() )
		{
			static const FName NAME_UniqueObjectNameForCooking(TEXT("UniqueObjectNameForCooking"));
			InName = MakeUniqueObjectName(InOuter, InClass, NAME_UniqueObjectNameForCooking);
		}
		else
#endif
		{
			InName = MakeUniqueObjectName(InOuter, InClass);
		}
	}
	else
	{
		// See if object already exists.
		Obj = StaticFindObjectFastInternal( /*Class=*/ NULL, InOuter, InName, true );

		// Temporary: If the object we found is of a different class, allow the object to be allocated.
		// This breaks new UObject assumptions and these need to be fixed.
		if (Obj && !Obj->GetClass()->IsChildOf(InClass))
		{
			UE_LOG(LogUObjectGlobals, Fatal,
				TEXT("Objects have the same fully qualified name but different paths.\n")
				TEXT("\tNew Object: %s %s.%s\n")
				TEXT("\tExisting Object: %s"),
				*InClass->GetName(), InOuter ? *InOuter->GetPathName() : TEXT(""), *InName.ToString(),
				*Obj->GetFullName());
		}
	}

	FLinkerLoad*	Linker						= NULL;
	int32				LinkerIndex					= INDEX_NONE;
	bool			bWasConstructedOnOldObject	= false;
	// True when the object to be allocated already exists and is a subobject.
	bool bSubObject = false;
	int32 TotalSize = InClass->GetPropertiesSize();
	checkSlow(TotalSize);

	if( Obj == NULL )
	{	
		int32 Alignment	= FMath::Max( 4, InClass->GetMinAlignment() );
		Obj = (UObject *)GUObjectAllocator.AllocateUObject(TotalSize,Alignment,GIsInitialLoad);
	}
	else
	{
		// Replace an existing object without affecting the original's address or index.
		check(!Obj->IsUnreachable());

		check(!ObjectRestoreAfterInitProps); // otherwise recursive construction
		ObjectRestoreAfterInitProps = Obj->GetRestoreForUObjectOverwrite();

		// Remember linker, flags, index, and native class info.
		Linker		= Obj->GetLinker();
		LinkerIndex = Obj->GetLinkerIndex();
		InternalSetFlags |= (Obj->GetInternalFlags() & (EInternalObjectFlags::Native | EInternalObjectFlags::RootSet));

		if ( bCreatingCDO )
		{
			check(Obj->HasAllFlags(RF_ClassDefaultObject));
			Obj->SetFlags(InFlags);
			Obj->SetInternalFlags(InternalSetFlags);
			// never call PostLoad on class default objects
			Obj->ClearFlags(RF_NeedPostLoad|RF_NeedPostLoadSubobjects);
		}
		else if(!InOuter || !InOuter->HasAnyFlags(RF_ClassDefaultObject))
		{
#if !UE_BUILD_SHIPPING
			// Handle nested DSOs
			bool bIsOwnedByCDO = false;
			UObject* Iter = InOuter;
			while (Iter)
			{
				if (Iter->HasAnyFlags(RF_ClassDefaultObject))
				{
					bIsOwnedByCDO = true;
					break;
				}
				Iter = Iter->GetOuter();
			}
			// Should only get in here if we're NOT creating a subobject of a CDO.  CDO subobjects may still need to be serialized off of disk after being created by the constructor
			// if really necessary there was code to allow replacement of object just needing postload, but lets not go there unless we have to
			checkf(!Obj->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_ClassDefaultObject) || bIsOwnedByCDO,
				*FText::Format(NSLOCTEXT("Core", "ReplaceNotFullyLoaded_f", "Attempting to replace an object that hasn't been fully loaded: {0} (Outer={1}, Flags={2})"),
					FText::FromString(Obj->GetFullName()),
					InOuter ? FText::FromString(InOuter->GetFullName()) : FText::FromString(TEXT("NULL")),
					FText::FromString(FString::Printf(TEXT("0x%08x"), (int32)Obj->GetFlags()))).ToString());
#endif//UE_BUILD_SHIPPING
		}
		// Subobjects are always created in the constructor, no need to re-create them here unless their archetype != CDO or they're blueprint generated.	
		if (!bCreatingCDO && (!bCanRecycleSubobjects || !Obj->IsDefaultSubobject()))
		{
			// Destroy the object.
			SCOPE_CYCLE_COUNTER(STAT_DestroyObject);
			// Check that the object hasn't been destroyed yet.
			if(!Obj->HasAnyFlags(RF_FinishDestroyed))
			{
				// Get the name before we start the destroy, as destroy renames it
				FString OldName = Obj->GetFullName();

				// Begin the asynchronous object cleanup.
				Obj->ConditionalBeginDestroy();

				// Wait for the object's asynchronous cleanup to finish.
				while (!Obj->IsReadyForFinishDestroy()) 
				{
					// If we're not in the editor, and aren't doing something specifically destructive like reconstructing blueprints, this is fatal
					if (!GIsEditor && FApp::IsGame() && !GIsReconstructingBlueprintInstances)
					{
						// Switching to warning, investigate why level duplication triggers this
						UE_LOG(LogUObjectGlobals, Warning, TEXT("Gamethread hitch waiting for resource cleanup on a UObject (%s) overwrite. Fix the higher level code so that this does not happen."), *OldName );
					}
					FPlatformProcess::Sleep(0);
				}
				// Finish destroying the object.
				Obj->ConditionalFinishDestroy();
			}
			Obj->~UObject();
			bWasConstructedOnOldObject	= true;
		}
		else
		{
			bSubObject = true;
		}
	}

	// If class is transient, non-archetype objects must be transient.
	bool const bCreatingArchetype = (InFlags & RF_ArchetypeObject) != 0;
	if ( !bCreatingCDO && InClass->HasAnyClassFlags(CLASS_Transient) && !bCreatingArchetype )
	{
		InFlags |= RF_Transient;
	}

	if (!bSubObject)
	{
		FMemory::Memzero((void *)Obj, TotalSize);
		new ((void *)Obj) UObjectBase(InClass, InFlags|RF_NeedInitialization, InternalSetFlags, InOuter, InName);
	}
	else
	{
		// Propagate flags to subobjects created in the native constructor.
		Obj->SetFlags(InFlags);
		Obj->SetInternalFlags(InternalSetFlags);
	}

	if (bWasConstructedOnOldObject)
	{
		// Reassociate the object with it's linker.
		Obj->SetLinker(Linker,LinkerIndex,false);
		if(Linker)
		{
			check(Linker->ExportMap[LinkerIndex].Object == NULL);
			Linker->ExportMap[LinkerIndex].Object = Obj;
		}
	}

	if (IsInAsyncLoadingThread())
	{
		NotifyConstructedDuringAsyncLoading(Obj, bSubObject);
	}
	else
	{
		// Sanity checks for async flags.
		// It's possible to duplicate an object on the game thread that is still being referenced 
		// by async loading code or has been created on a different thread than the main thread.
		Obj->ClearInternalFlags(EInternalObjectFlags::AsyncLoading);
		if (Obj->HasAnyInternalFlags(EInternalObjectFlags::Async) && IsInGameThread())
		{
			Obj->ClearInternalFlags(EInternalObjectFlags::Async);
		}
	}


	// Let the caller know if a subobject has just been recycled.
	if (bOutRecycledSubobject)
	{
		*bOutRecycledSubobject = bSubObject;
	}
	
	return Obj;
}

//@todo UE4 - move this stuff to UnObj.cpp or something



void UObject::PostInitProperties()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FUObjectThreadContext::Get().PostInitPropertiesCheck.Push(this);
#endif
#if USE_UBER_GRAPH_PERSISTENT_FRAME
	GetClass()->CreatePersistentUberGraphFrame(this, true);
#endif
}

UObject::UObject()
{
	EnsureNotRetrievingVTablePtr();

	FObjectInitializer* ObjectInitializerPtr = FUObjectThreadContext::Get().TopInitializer();
	UE_CLOG(!ObjectInitializerPtr, LogUObjectGlobals, Fatal, TEXT("%s is not being constructed with either NewObject, NewNamedObject or ConstructObject."), *GetName());
	FObjectInitializer& ObjectInitializer = *ObjectInitializerPtr;
	UE_CLOG(ObjectInitializer.Obj != nullptr && ObjectInitializer.Obj != this, LogUObjectGlobals, Fatal, TEXT("UObject() constructor called but it's not the object that's currently being constructed with NewObject. Maybe you are trying to construct it on the stack, which is not supported."));
	const_cast<FObjectInitializer&>(ObjectInitializer).Obj = this;
	const_cast<FObjectInitializer&>(ObjectInitializer).FinalizeSubobjectClassInitialization();
}

UObject::UObject(const FObjectInitializer& ObjectInitializer)
{
	EnsureNotRetrievingVTablePtr();

	UE_CLOG(ObjectInitializer.Obj != nullptr && ObjectInitializer.Obj != this, LogUObjectGlobals, Fatal, TEXT("UObject(const FObjectInitializer&) constructor called but it's not the object that's currently being constructed with NewObject. Maybe you are trying to construct it on the stack, which is not supported."));
	const_cast<FObjectInitializer&>(ObjectInitializer).Obj = this;
	const_cast<FObjectInitializer&>(ObjectInitializer).FinalizeSubobjectClassInitialization();
}



FObjectInitializer::FObjectInitializer()
	: Obj(nullptr)
	, ObjectArchetype(nullptr)
	, bCopyTransientsFromClassDefaults(false)
	, bShouldInitializePropsFromArchetype(false)
	, bSubobjectClassInitializationAllowed(true)
	, InstanceGraph(nullptr)
	, LastConstructedObject(nullptr)
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	, bIsDeferredInitializer(false)
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	// Mark we're in the constructor now.	
	ThreadContext.IsInConstructor++;
	LastConstructedObject = ThreadContext.ConstructedObject;
	ThreadContext.ConstructedObject = Obj;
	ThreadContext.PushInitializer(this);
}	

FObjectInitializer::FObjectInitializer(UObject* InObj, UObject* InObjectArchetype, bool bInCopyTransientsFromClassDefaults, bool bInShouldInitializeProps, struct FObjectInstancingGraph* InInstanceGraph)
	: Obj(InObj)
	, ObjectArchetype(InObjectArchetype)
	  // if the SubobjectRoot NULL, then we want to copy the transients from the template, otherwise we are doing a duplicate and we want to copy the transients from the class defaults
	, bCopyTransientsFromClassDefaults(bInCopyTransientsFromClassDefaults)
	, bShouldInitializePropsFromArchetype(bInShouldInitializeProps)
	, bSubobjectClassInitializationAllowed(true)
	, InstanceGraph(InInstanceGraph)
	, LastConstructedObject(nullptr)
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	, bIsDeferredInitializer(false)
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	// Mark we're in the constructor now.
	ThreadContext.IsInConstructor++;
	LastConstructedObject = ThreadContext.ConstructedObject;
	ThreadContext.ConstructedObject = Obj;
	ThreadContext.PushInitializer(this);
}

/**
 * Destructor for internal class to finalize UObject creation (initialize properties) after the real C++ constructor is called.
 **/
FObjectInitializer::~FObjectInitializer()
{

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	// if we're not at the top of ObjectInitializers, then this is most 
	// likely a deferred FObjectInitializer that's a copy of one that was used 
	// in a constructor (that has already been popped)
	if (!bIsDeferredInitializer)
	{
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		check(ThreadContext.TopInitializer() == this);
		ThreadContext.PopInitializer();
		
		// Let the FObjectFinders know we left the constructor.
		ThreadContext.IsInConstructor--;
		check(ThreadContext.IsInConstructor >= 0);
		ThreadContext.ConstructedObject = LastConstructedObject;

		check(Obj != nullptr);

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	}
	else if (Obj == nullptr)
	{
		// the deferred initialization has already been ran, we clear Obj once 
		// PostConstructInit() has been executed
		return;
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	const bool bIsCDO = Obj->HasAnyFlags(RF_ClassDefaultObject);
	UClass* Class = Obj->GetClass();

	if ( Class != UObject::StaticClass() )
	{
		// InClass->GetClass() == NULL when InClass hasn't been fully initialized yet (during static registration)
		if ( !ObjectArchetype  && Class->GetClass() )
		{
			ObjectArchetype = Class->GetDefaultObject();
		}
	}
	else if (bIsCDO)
	{
		// for the Object CDO, make sure that we do not use an archetype
		check(ObjectArchetype == nullptr);
	}

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	bool bIsPostConstructInitDeferred = false;
	if (!FBlueprintSupport::IsDeferredCDOInitializationDisabled())
	{		
		UClass* BlueprintClass = nullptr;
		// since "InheritableComponentTemplate"s are not default sub-objects, 
		// they won't be fixed up by the owner's FObjectInitializer (CDO 
		// FObjectInitializers will init default sub-object properties, copying  
		// from the super's DSOs) - this means that we need to separately defer 
		// init'ing these sub-objects when their archetype hasn't been loaded 
		// yet (it is possible that the archetype isn't even correct, as the 
		// super's sub-object hasn't even been created yet; in this case the 
		// component's CDO is used, which is probably wrong)
		if (Obj->HasAnyFlags(RF_InheritableComponentTemplate))
		{
			BlueprintClass = Cast<UClass>(Obj->GetOuter());
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			check(BlueprintClass != nullptr);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		}
		else if (bIsCDO && !Class->IsNative())
		{
			BlueprintClass = Class;
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			check(Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		}

		if (BlueprintClass != nullptr)
		{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			check(!BlueprintClass->IsNative());
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

			UClass* SuperClass = BlueprintClass->GetSuperClass();
			if (SuperClass && !SuperClass->IsNative())
			{
				UObject* SuperBpCDO = nullptr;
				// if this is a CDO (then we know/assume the archetype is the 
				// CDO from the super class), use the ObjectArchetype for the 
				// SuperBpCDO (because the SuperClass may have a REINST CDO 
				// cached currently)
				if (bIsCDO)
				{
					SuperBpCDO = ObjectArchetype;
					SuperClass = ObjectArchetype->GetClass();

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
					check(ObjectArchetype->HasAnyFlags(RF_ClassDefaultObject));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
				}
				else
				{
					SuperBpCDO = SuperClass->GetDefaultObject(/*bCreateIfNeeded =*/false);
				}

				FLinkerLoad* SuperClassLinker = SuperClass->GetLinker();
				const bool bSuperLoadPending = FDeferredObjInitializerTracker::IsCdoDeferred(SuperClass) ||
					(SuperBpCDO && SuperBpCDO->HasAnyFlags(RF_NeedLoad)) ||
					(SuperClassLinker && SuperClassLinker->IsBlueprintFinalizationPending());

				FLinkerLoad* ObjLinker = BlueprintClass->GetLinker();
				const bool bIsBpClassSerializing    = ObjLinker && (ObjLinker->LoadFlags & LOAD_DeferDependencyLoads);
				const bool bIsResolvingDeferredObjs = BlueprintClass->HasAnyFlags(RF_LoadCompleted) &&
					ObjLinker && ObjLinker->IsBlueprintFinalizationPending();

				if (bSuperLoadPending && (bIsBpClassSerializing || bIsResolvingDeferredObjs))
				{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
					// make sure we haven't already deferred this once, if we have
					// then something is destroying this one prematurely 
					check(bIsDeferredInitializer == false);

					for (const FSubobjectsToInit::FSubobjectInit& SubObjInfo : ComponentInits.SubobjectInits)
					{
						check(!SubObjInfo.Subobject->HasAnyFlags(RF_NeedLoad));
					}
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
					
					// makes a copy of this and saves it off, to be ran later
					if (FObjectInitializer* DeferredCopy = FDeferredObjInitializerTracker::Add(*this))
					{
						bIsPostConstructInitDeferred = true;
						DeferredCopy->bIsDeferredInitializer = true;

						// make sure this wasn't mistakenly pushed into ObjectInitializers
						// (the copy constructor should have been what was invoked, 
						// which doesn't push to ObjectInitializers)
						check(FUObjectThreadContext::Get().TopInitializer() != DeferredCopy);
					}
				}
			}
		}
	}

	if (!bIsPostConstructInitDeferred)
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	{
		PostConstructInit();
	}
}

void FObjectInitializer::PostConstructInit()
{
	// we clear the Obj pointer at the end of this function, so if it is null 
	// then it most likely means that this is being ran for a second time
	if (Obj == nullptr)
	{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		checkf(Obj != nullptr, TEXT("Looks like you're attempting to run FObjectInitializer::PostConstructInit() twice, and that should never happen."));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_PostConstructInitializeProperties);
	const bool bIsCDO = Obj->HasAnyFlags(RF_ClassDefaultObject);
	UClass* Class = Obj->GetClass();
	UClass* SuperClass = Class->GetSuperClass();

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (bIsDeferredInitializer)
	{
		const bool bIsDeferredSubObject = Obj->HasAnyFlags(RF_InheritableComponentTemplate);
		if (bIsDeferredSubObject)
		{
			// when this sub-object was created it's archetype object (the 
			// super's sub-obj) may not have been created yet (thanks cyclic 
			// dependencies). in that scenario, the component class's CDO would  
			// have been used in its place; now that we're resolving the defered 
			// sub-obj initialization we should try to update the archetype
			if (ObjectArchetype->HasAnyFlags(RF_ClassDefaultObject))
			{
				ObjectArchetype = UObject::GetArchetypeFromRequiredInfo(Class, Obj->GetOuter(), Obj->GetFName(), Obj->GetFlags());
				// NOTE: this may still be the component class's CDO (like when 
				// a component was removed from the super, without resaving the child)
			}			
		}

		UClass* ArchetypeClass = ObjectArchetype->GetClass();
		const bool bSuperHasBeenRegenerated = ArchetypeClass->HasAnyClassFlags(CLASS_NewerVersionExists);
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		check(bIsCDO || bIsDeferredSubObject);
		check(ObjectArchetype->GetOutermost() != GetTransientPackage());
		check(!bIsCDO || (ArchetypeClass == SuperClass && !bSuperHasBeenRegenerated));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

		if ( !ensureMsgf(!bSuperHasBeenRegenerated, TEXT("The archetype for %s has been regenerated, we cannot properly initialize inherited properties, as the class layout may have changed."), *Obj->GetName()) )
		{
			// attempt to complete initialization/instancing as best we can, but
			// it would not be surprising if our CDO was improperly initialized 
			// as a result...

			// iterate backwards, so we can remove elements as we go
			for (int32 SubObjIndex = ComponentInits.SubobjectInits.Num() - 1; SubObjIndex >= 0; --SubObjIndex)
			{
				FSubobjectsToInit::FSubobjectInit& SubObjInitInfo = ComponentInits.SubobjectInits[SubObjIndex];
				const FName SubObjName = SubObjInitInfo.Subobject->GetFName();

				UObject* OuterArchetype = SubObjInitInfo.Subobject->GetOuter()->GetArchetype();
				UObject* NewTemplate = OuterArchetype->GetClass()->GetDefaultSubobjectByName(SubObjName);

				if (ensure(NewTemplate != nullptr))
				{
					SubObjInitInfo.Template = NewTemplate;
				}
				else
				{
					ComponentInits.SubobjectInits.RemoveAtSwap(SubObjIndex);
				}
			}
		}
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	if (bShouldInitializePropsFromArchetype)
	{
		UClass* BaseClass = (bIsCDO && !GIsDuplicatingClassForReinstancing) ? SuperClass : Class;
		if (BaseClass == NULL)
		{
			check(Class==UObject::StaticClass());
			BaseClass = Class;
		}
	
		UObject* Defaults = ObjectArchetype ? ObjectArchetype : BaseClass->GetDefaultObject(false); // we don't create the CDO here if it doesn't already exist
		InitProperties(Obj, BaseClass, Defaults, bCopyTransientsFromClassDefaults);
	}

	bool bAllowInstancing = IsInstancingAllowed();
	bool bNeedSubobjectInstancing = InitSubobjectProperties(bAllowInstancing);

	// Restore class information if replacing native class.
	if (ObjectRestoreAfterInitProps != NULL)
	{
		ObjectRestoreAfterInitProps->Restore();
		delete ObjectRestoreAfterInitProps;
		ObjectRestoreAfterInitProps = NULL;
	}

	bool bNeedInstancing = false;
	// if HasAnyFlags(RF_NeedLoad), we do these steps later
#if !USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (!Obj->HasAnyFlags(RF_NeedLoad))
#else 
	// we defer this initialization in special set of cases (when Obj is a CDO 
	// and its parent hasn't been serialized yet)... in those cases, Obj (the 
	// CDO) wouldn't have had RF_NeedLoad set (not yet, because it is created 
	// from Class->GetDefualtObject() without that flag); since we've deferred
	// all this, it is likely that this flag is now present... these steps 
	// (specifically sub-object instancing) is important for us to run on the
	// CDO, so we allow all this when the bIsDeferredInitializer is true as well
	if (!Obj->HasAnyFlags(RF_NeedLoad) || bIsDeferredInitializer)
#endif // !USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	{
		if ((bIsCDO && !Class->HasAnyFlags(RF_Dynamic)) || Class->HasAnyClassFlags(CLASS_PerObjectConfig))
		{
			Obj->LoadConfig(NULL, NULL, bIsCDO ? UE4::LCPF_ReadParentSections : UE4::LCPF_None);
		}
		else if (bIsCDO && Class->HasAnyFlags(RF_Dynamic) && Class->HasAnyClassFlags(CLASS_Config))
		{
			Obj->LoadConfig(Class);
		}
		if (bAllowInstancing)
		{
			// Instance subobject templates for non-cdo blueprint classes or when using non-CDO template.
			const bool bInitPropsWithArchetype = Class->GetDefaultObject(false) == NULL || Class->GetDefaultObject(false) != ObjectArchetype || Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
			if ((!bIsCDO || bShouldInitializePropsFromArchetype) && Class->HasAnyClassFlags(CLASS_HasInstancedReference) && bInitPropsWithArchetype)
			{
				// Only blueprint generated CDOs can have their subobjects instanced.
				check(!bIsCDO || !Class->HasAnyClassFlags(CLASS_Intrinsic|CLASS_Native));

				bNeedInstancing = true;
			}
		}
	}
	if (bNeedInstancing || bNeedSubobjectInstancing)
	{
		InstanceSubobjects(Class, bNeedInstancing, bNeedSubobjectInstancing);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_PostInitProperties);
		Obj->PostInitProperties();
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!FUObjectThreadContext::Get().PostInitPropertiesCheck.Num() || (FUObjectThreadContext::Get().PostInitPropertiesCheck.Pop() != Obj))
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s failed to route PostInitProperties. Call Super::PostInitProperties() in %s::PostInitProperties()."), *Obj->GetClass()->GetName(), *Obj->GetClass()->GetName());
	}
	// Check if all TSubobjectPtr properties have been initialized.
#if !USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (!Obj->HasAnyFlags(RF_NeedLoad))
#else 
	// we defer this initialization in special set of cases (when Obj is a CDO 
	// and its parent hasn't been serialized yet)... in those cases, Obj (the 
	// CDO) wouldn't have had RF_NeedLoad set (not yet, because it is created 
	// from Class->GetDefualtObject() without that flag); since we've deferred
	// all this, it is likely that this flag is now present... we want to run 
	// all this as if the object was just created, so we check 
	// bIsDeferredInitializer as well
	if (!Obj->HasAnyFlags(RF_NeedLoad) || bIsDeferredInitializer)
#endif // !USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	{
		for (UProperty* P = Class->RefLink; P; P = P->NextRef)
		{
			if (P->HasAnyPropertyFlags(CPF_SubobjectReference))
			{
				UObjectProperty* ObjProp = CastChecked<UObjectProperty>(P);
				UObject* PropertyValue = ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(Obj));
				if (!FSubobjectPtr::IsInitialized(PropertyValue))
				{
					UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s must be initialized in the constructor (at least to NULL) by calling ObjectInitializer.CreateDefaultSubobject"), *ObjProp->GetFullName() );
				}
				else if (PropertyValue && P->HasAnyPropertyFlags(CPF_Transient))
				{
					// Transient subobjects can't be in the list of ComponentInits
					for (int32 Index = 0; Index < ComponentInits.SubobjectInits.Num(); Index++)
					{
						UObject* Subobject = ComponentInits.SubobjectInits[Index].Subobject;
						UE_CLOG(Subobject->GetFName() == PropertyValue->GetFName(), 
							LogUObjectGlobals, Fatal, TEXT("Transient property %s contains a reference to non-transient subobject %s."), *ObjProp->GetFullName(), *PropertyValue->GetName() );
					}
				}
			}
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if !USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (!Obj->HasAnyFlags(RF_NeedLoad) 
#else 
	// we defer this initialization in special set of cases (when Obj is a CDO 
	// and its parent hasn't been serialized yet)... in those cases, Obj (the 
	// CDO) wouldn't have had RF_NeedLoad set (not yet, because it is created 
	// from Class->GetDefualtObject() without that flag); since we've deferred
	// all this, it is likely that this flag is now present... we want to run 
	// all this as if the object was just created, so we check 
	// bIsDeferredInitializer as well
	if ( (!Obj->HasAnyFlags(RF_NeedLoad) || bIsDeferredInitializer)
#endif // !USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
		// if component instancing is not enabled, then we leave the components in an invalid state, which will presumably be fixed by the caller
		&& ((InstanceGraph == NULL) || InstanceGraph->IsSubobjectInstancingEnabled())) 
	{
		Obj->CheckDefaultSubobjects();
	}

	Obj->ClearFlags(RF_NeedInitialization);

	// clear the object pointer so we can guard against running this function again
	Obj = nullptr;
}

bool FObjectInitializer::IsInstancingAllowed() const
{
	return (InstanceGraph == NULL) || InstanceGraph->IsSubobjectInstancingEnabled();
}

bool FObjectInitializer::InitSubobjectProperties(bool bAllowInstancing) const
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	bool bNeedSubobjectInstancing = bAllowInstancing && bIsDeferredInitializer;
#else 
	bool bNeedSubobjectInstancing = false;
#endif
	// initialize any subobjects, now that the constructors have run
	for (int32 Index = 0; Index < ComponentInits.SubobjectInits.Num(); Index++)
	{
		UObject* Subobject = ComponentInits.SubobjectInits[Index].Subobject;
		UObject* Template = ComponentInits.SubobjectInits[Index].Template;
		InitProperties(Subobject, Template->GetClass(), Template, false);
		if (bAllowInstancing && !Subobject->HasAnyFlags(RF_NeedLoad))
		{
			bNeedSubobjectInstancing = true;
		}
	}

	return bNeedSubobjectInstancing;
}

void FObjectInitializer::InstanceSubobjects(UClass* Class, bool bNeedInstancing, bool bNeedSubobjectInstancing) const
{
	SCOPE_CYCLE_COUNTER(STAT_InstanceSubobjects);

	FObjectInstancingGraph TempInstancingGraph;
	FObjectInstancingGraph* UseInstancingGraph = InstanceGraph ? InstanceGraph : &TempInstancingGraph;
	{
		UseInstancingGraph->AddNewObject(Obj, ObjectArchetype);
	}
	// Add any default subobjects
	for (auto& SubobjectInit : ComponentInits.SubobjectInits)
	{
		UseInstancingGraph->AddNewObject(SubobjectInit.Subobject, SubobjectInit.Template);
	}
	if (bNeedInstancing)
	{
		UObject* Archetype = ObjectArchetype ? ObjectArchetype : Obj->GetArchetype();
		Class->InstanceSubobjectTemplates(Obj, Archetype, Archetype ? Archetype->GetClass() : NULL, Obj, UseInstancingGraph);
	}
	if (bNeedSubobjectInstancing)
	{
		// initialize any subobjects, now that the constructors have run
		for (int32 Index = 0; Index < ComponentInits.SubobjectInits.Num(); Index++)
		{
			UObject* Subobject = ComponentInits.SubobjectInits[Index].Subobject;
			UObject* Template = ComponentInits	.SubobjectInits[Index].Template;

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			if ( !Subobject->HasAnyFlags(RF_NeedLoad) || bIsDeferredInitializer )
#else 
			if ( !Subobject->HasAnyFlags(RF_NeedLoad) )
#endif
			{
				Subobject->GetClass()->InstanceSubobjectTemplates(Subobject, Template, Template->GetClass(), Subobject, UseInstancingGraph);
			}
		}
	}
}

UClass* FObjectInitializer::GetClass() const
{
	return Obj->GetClass();
}

void FSubobjectPtr::Set(UObject* InObject)
{
	if (Object != InObject && IsInitialized(Object) && !Object->IsPendingKill())
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Unable to overwrite default subobject %s"), *Object->GetPathName());
	}
	else
	{
		Object = InObject;
	}
}

// Binary initialize object properties to zero or defaults.
void FObjectInitializer::InitProperties(UObject* Obj, UClass* DefaultsClass, UObject* DefaultData, bool bCopyTransientsFromClassDefaults)
{
	check(!GEventDrivenLoaderEnabled || !DefaultsClass || !DefaultsClass->HasAnyFlags(RF_NeedLoad));
	check(!GEventDrivenLoaderEnabled || !DefaultData || !DefaultData->HasAnyFlags(RF_NeedLoad));

	SCOPE_CYCLE_COUNTER(STAT_InitProperties);

	check(DefaultsClass && Obj);

	UClass* Class = Obj->GetClass();

	// bool to indicate that we need to initialize any non-native properties (native ones were done when the native constructor was called by the code that created and passed in a FObjectInitializer object)
	bool bNeedInitialize = !Class->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic);

	// bool to indicate that we can use the faster PostConstructLink chain for initialization.
	bool bCanUsePostConstructLink = !bCopyTransientsFromClassDefaults && DefaultsClass == Class;

	if (Obj->HasAnyFlags(RF_NeedLoad))
	{
		bCopyTransientsFromClassDefaults = false;
	}

	if (!bNeedInitialize && bCanUsePostConstructLink)
	{
		// This is just a fast path for the below in the common case that we are not doing a duplicate or initializing a CDO and this is all native.
		// We only do it if the DefaultData object is NOT a CDO of the object that's being initialized. CDO data is already initialized in the
		// object's constructor.
		if (DefaultData)
		{
			if (Class->GetDefaultObject(false) != DefaultData)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_InitProperties_FromTemplate);
				for (UProperty* P = Class->PropertyLink; P; P = P->PropertyLinkNext)
				{
					P->CopyCompleteValue_InContainer(Obj, DefaultData);
				}
			}
			else
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_InitProperties_ConfigEtcOnly);
				// Copy all properties that require additional initialization (e.g. CPF_Config).
				for (UProperty* P = Class->PostConstructLink; P; P = P->PostConstructLinkNext)
				{
					P->CopyCompleteValue_InContainer(Obj, DefaultData);
				}
			}
		}
	}
	else
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitProperties_Blueprint);

		// As with native classes, we must iterate through all properties (slow path) if default data is pointing at something other than the CDO.
		bCanUsePostConstructLink &= (DefaultData == Class->GetDefaultObject(false));

		UObject* ClassDefaults = bCopyTransientsFromClassDefaults ? DefaultsClass->GetDefaultObject() : NULL;	
		check(!GEventDrivenLoaderEnabled || !bCopyTransientsFromClassDefaults || !DefaultsClass->GetDefaultObject()->HasAnyFlags(RF_NeedLoad));

		for (UProperty* P = bCanUsePostConstructLink ? Class->PostConstructLink : Class->PropertyLink; P; P = bCanUsePostConstructLink ? P->PostConstructLinkNext : P->PropertyLinkNext)
		{
			if (bNeedInitialize)
			{		
				bNeedInitialize = InitNonNativeProperty(P, Obj);
			}

			bool IsTransient = P->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient);
			if (!IsTransient || !P->ContainsInstancedObjectProperty())
			{
				if (bCopyTransientsFromClassDefaults && IsTransient)
				{
					// This is a duplicate. The value for all transient or non-duplicatable properties should be copied
					// from the source class's defaults.
					P->CopyCompleteValue_InContainer(Obj, ClassDefaults);
				}
				else if (P->IsInContainer(DefaultsClass))
				{
					P->CopyCompleteValue_InContainer(Obj, DefaultData);
				}
			}
		}

		// This step is only necessary if we're not iterating the full property chain.
		if (bCanUsePostConstructLink)
		{
			// Initialize remaining property values from defaults using an explicit custom post-construction property list returned by the class object.
			Class->InitPropertiesFromCustomList((uint8*)Obj, (uint8*)DefaultData);
		}
	}
}

bool FObjectInitializer::IslegalOverride(FName InComponentName, class UClass *DerivedComponentClass, class UClass *BaseComponentClass) const
{
	if (DerivedComponentClass && BaseComponentClass && !DerivedComponentClass->IsChildOf(BaseComponentClass) )
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("%s is not a legal override for component %s because it does not derive from %s."), 
			*DerivedComponentClass->GetFullName(), *InComponentName.ToString(), *BaseComponentClass->GetFullName());
		return false;
	}
	return true;
}

void FObjectInitializer::AssertIfSubobjectSetupIsNotAllowed(const TCHAR* SubobjectName) const
{
	UE_CLOG(!bSubobjectClassInitializationAllowed, LogUObjectGlobals, Fatal,
		TEXT("%s.%s: Subobject class setup is only allowed in base class constructor call (in the initialization list)"), Obj ? *Obj->GetFullName() : TEXT("NULL"), SubobjectName);
}

#if DO_CHECK
void CheckIsClassChildOf_Internal(UClass* Parent, UClass* Child)
{
	// This is a function to avoid platform compilation issues
	checkf(Child, TEXT("NewObject called with a nullptr class object"));
	checkf(Child->IsChildOf(Parent), TEXT("NewObject called with invalid class, %s must be a child of %s"), *Child->GetName(), *Parent->GetName());
}
#endif

UObject* StaticConstructObject_Internal
(
	UClass*			InClass,
	UObject*		InOuter								/*=GetTransientPackage()*/,
	FName			InName								/*=NAME_None*/,
	EObjectFlags	InFlags								/*=0*/,
	EInternalObjectFlags InternalSetFlags /*=0*/,
	UObject*		InTemplate							/*=NULL*/,
	bool bCopyTransientsFromClassDefaults	/*=false*/,
	FObjectInstancingGraph* InInstanceGraph				/*=NULL*/,
	bool bAssumeTemplateIsArchetype	/*=false*/
)
{
	LLM_SCOPE(ELLMTag::UObject);

	SCOPE_CYCLE_COUNTER(STAT_ConstructObject);
	UObject* Result = NULL;

#if WITH_EDITORONLY_DATA
	UE_CLOG(GIsSavingPackage && InOuter != GetTransientPackage(), LogUObjectGlobals, Fatal, TEXT("Illegal call to StaticConstructObject() while serializing object data! (Object will not be saved!)"));
#endif

	checkf(!InTemplate || InTemplate->IsA(InClass) || (InFlags & RF_ClassDefaultObject), TEXT("StaticConstructObject %s is not an instance of class %s and it is not a CDO."), *GetFullNameSafe(InTemplate), *GetFullNameSafe(InClass)); // template must be an instance of the class we are creating, except CDOs

	// Subobjects are always created in the constructor, no need to re-create them unless their archetype != CDO or they're blueprint generated.
	// If the existing subobject is to be re-used it can't have BeginDestroy called on it so we need to pass this information to StaticAllocateObject.	
	const bool bIsNativeClass = InClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic);
	const bool bIsNativeFromCDO = bIsNativeClass &&
		(
			!InTemplate || 
			(InName != NAME_None && (bAssumeTemplateIsArchetype || InTemplate == UObject::GetArchetypeFromRequiredInfo(InClass, InOuter, InName, InFlags)))
		);
#if WITH_HOT_RELOAD
	// Do not recycle subobjects when performing hot-reload as they may contain old property values.
	const bool bCanRecycleSubobjects = bIsNativeFromCDO && !GIsHotReload;
#else
	const bool bCanRecycleSubobjects = bIsNativeFromCDO;
#endif
	bool bRecycledSubobject = false;	
	Result = StaticAllocateObject(InClass, InOuter, InName, InFlags, InternalSetFlags, bCanRecycleSubobjects, &bRecycledSubobject);
	check(Result != NULL);
	// Don't call the constructor on recycled subobjects, they haven't been destroyed.
	if (!bRecycledSubobject)
	{		
		FScopeCycleCounterUObject ConstructorScope(InClass, GET_STATID(STAT_ConstructObject));
		(*InClass->ClassConstructor)( FObjectInitializer(Result, InTemplate, bCopyTransientsFromClassDefaults, true, InInstanceGraph) );
	}
	
	if( GIsEditor && GUndo && (InFlags & RF_Transactional) && !(InFlags & RF_NeedLoad) && !InClass->IsChildOf(UField::StaticClass()) )
	{
		// Set RF_PendingKill and update the undo buffer so an undo operation will set RF_PendingKill on the newly constructed object.
		Result->MarkPendingKill();
		SaveToTransactionBuffer(Result, false);
		Result->ClearPendingKill();
	}
	return Result;
}

void FObjectInitializer::AssertIfInConstructor(UObject* Outer, const TCHAR* ErrorMessage)
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	UE_CLOG(ThreadContext.IsInConstructor && Outer == ThreadContext.ConstructedObject, LogUObjectGlobals, Fatal, TEXT("%s"), ErrorMessage);
}

FObjectInitializer& FObjectInitializer::Get()
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	UE_CLOG(!ThreadContext.IsInConstructor, LogUObjectGlobals, Fatal, TEXT("FObjectInitializer::Get() can only be used inside of UObject-derived class constructor."));
	return ThreadContext.TopInitializerChecked();
}

/**
 * Stores the object flags for all objects in the tracking array.
 */
void FScopedObjectFlagMarker::SaveObjectFlags()
{
	StoredObjectFlags.Empty();

	for (FObjectIterator It; It; ++It)
	{
		UObject* Obj = *It;
		StoredObjectFlags.Add(*It, FStoredObjectFlags(Obj->GetFlags(), Obj->GetInternalFlags()));
	}
}

/**
 * Restores the object flags for all objects from the tracking array.
 */
void FScopedObjectFlagMarker::RestoreObjectFlags()
{
	for (TMap<UObject*, FStoredObjectFlags>::TIterator It(StoredObjectFlags); It; ++It)
	{
		UObject* Object = It.Key();
		FStoredObjectFlags& PreviousObjectFlags = It.Value();

		// clear all flags
		Object->ClearFlags(RF_AllFlags);
		Object->ClearInternalFlags(EInternalObjectFlags::AllFlags);

		// then reset the ones that were originally set
		Object->SetFlags(PreviousObjectFlags.Flags);
		Object->SetInternalFlags(PreviousObjectFlags.InternalFlags);
	}
}

void ConstructorHelpers::FailedToFind(const TCHAR* ObjectToFind)
{
	FObjectInitializer* CurrentInitializer = FUObjectThreadContext::Get().TopInitializer();
	const FString Message = FString::Printf(TEXT("CDO Constructor (%s): Failed to find %s\n"),
		(CurrentInitializer && CurrentInitializer->GetClass()) ? *CurrentInitializer->GetClass()->GetName() : TEXT("Unknown"),
		ObjectToFind);
	FPlatformMisc::LowLevelOutputDebugString(*Message);
	UClass::GetDefaultPropertiesFeedbackContext().Log(ELogVerbosity::Error, *Message);
}

void ConstructorHelpers::CheckFoundViaRedirect(UObject *Object, const FString& PathName, const TCHAR* ObjectToFind)
{
	UObjectRedirector* Redir = FindObject<UObjectRedirector>(ANY_PACKAGE, *PathName);
	if (Redir && Redir->DestinationObject == Object)
	{
		FString NewString = Object->GetFullName();
		NewString.ReplaceInline(TEXT(" "), TEXT("'"), ESearchCase::CaseSensitive);
		NewString += TEXT("'");

		FObjectInitializer* CurrentInitializer = FUObjectThreadContext::Get().TopInitializer();
		const FString Message = FString::Printf(TEXT("CDO Constructor (%s): Followed redirector (%s), change code to new path (%s)\n"),
			(CurrentInitializer && CurrentInitializer->GetClass()) ? *CurrentInitializer->GetClass()->GetName() : TEXT("Unknown"),
			ObjectToFind, *NewString);

		FPlatformMisc::LowLevelOutputDebugString(*Message);
		UClass::GetDefaultPropertiesFeedbackContext().Log(ELogVerbosity::Warning, *Message);
	}
}

void ConstructorHelpers::CheckIfIsInConstructor(const TCHAR* ObjectToFind)
{
	auto& ThreadContext = FUObjectThreadContext::Get();
	UE_CLOG(!ThreadContext.IsInConstructor, LogUObjectGlobals, Fatal, TEXT("FObjectFinders can't be used outside of constructors to find %s"), ObjectToFind);
}

void ConstructorHelpers::StripObjectClass( FString& PathName, bool bAssertOnBadPath /*= false */ )
{	
	int32 NameStartIndex = INDEX_NONE;
	PathName.FindChar( TCHAR('\''), NameStartIndex );
	if( NameStartIndex != INDEX_NONE )
	{
		int32 NameEndIndex = INDEX_NONE;
		PathName.FindLastChar( TCHAR('\''), NameEndIndex );
		if(NameEndIndex > NameStartIndex)
		{
			PathName = PathName.Mid( NameStartIndex+1, NameEndIndex-NameStartIndex-1 );
		}
		else
		{
			UE_CLOG( bAssertOnBadPath, LogUObjectGlobals, Fatal, TEXT("Bad path name: %s, missing \' or an incorrect format"), *PathName );
		}
	}
}

/*----------------------------------------------------------------------------
FSimpleObjectReferenceCollectorArchive.
----------------------------------------------------------------------------*/
class FSimpleObjectReferenceCollectorArchive : public FArchiveUObject
{
public:

	/**
	* Constructor
	*
	* @param	InObjectArray			Array to add object references to
	*/
	FSimpleObjectReferenceCollectorArchive(const UObject* InSerializingObject, FReferenceCollector& InCollector)
		: Collector(InCollector)
		, SerializingObject(InSerializingObject)
	{
		ArIsObjectReferenceCollector = true;
		ArIsPersistent = Collector.IsIgnoringTransient();
		ArIgnoreArchetypeRef = Collector.IsIgnoringArchetypeRef();
	}

protected:

	/**
	* UObject serialize operator implementation
	*
	* @param Object	reference to Object reference
	* @return reference to instance of this class
	*/
	FArchive& operator<<(UObject*& Object)
	{
		if (Object)
		{
			UProperty* OldCollectorSerializedProperty = Collector.GetSerializedProperty();
			Collector.SetSerializedProperty(GetSerializedProperty());
			Collector.AddReferencedObject(Object, SerializingObject, GetSerializedProperty());
			Collector.SetSerializedProperty(OldCollectorSerializedProperty);
		}
		return *this;
	}

	/** Stored pointer to reference collector. */
	FReferenceCollector& Collector;
	/** Object which is performing the serialization. */
	const UObject* SerializingObject;
};

class FPersistentFrameCollectorArchive : public FSimpleObjectReferenceCollectorArchive
{
public:
	FPersistentFrameCollectorArchive(const UObject* InSerializingObject, FReferenceCollector& InCollector)
		: FSimpleObjectReferenceCollectorArchive(InSerializingObject, InCollector)
	{}

protected:
	virtual FArchive& operator<<(UObject*& Object) override
	{
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		if (!ensureMsgf((Object == nullptr) || Object->IsValidLowLevelFast()
			, TEXT("Invalid object referenced by the PersistentFrame: 0x%016llx (Blueprint object: %s, ReferencingProperty: %s) - If you have a reliable repro for this, please contact the development team with it.")
			, (int64)(PTRINT)Object
			, SerializingObject ? *SerializingObject->GetFullName() : TEXT("NULL")
			, GetSerializedProperty() ? *GetSerializedProperty()->GetFullName() : TEXT("NULL")))
		{
			// clear the property value (it's garbage)... the ubergraph-frame
			// has just lost a reference to whatever it was attempting to hold onto
			Object = nullptr;
		}
#endif
		if (Object)
		{
			bool bWeakRef = false;

			// If the property that serialized us is not an object property we are in some native serializer, we have to treat these as strong
			if (!Object->HasAnyFlags(RF_StrongRefOnFrame))
			{
				UObjectProperty* ObjectProperty = Cast<UObjectProperty>(GetSerializedProperty());

				if (ObjectProperty)
				{
					// This was a raw UObject* serialized by UObjectProperty, so just save the address
					bWeakRef = true;
				}
			}

			// Try to handle it as a weak ref, if it returns false treat it as a strong ref instead
			bWeakRef = bWeakRef && Collector.MarkWeakObjectReferenceForClearing(&Object);

			if (!bWeakRef)
			{
				// This is a hard reference or we don't know what's serializing it, so serialize it normally
				return FSimpleObjectReferenceCollectorArchive::operator<<(Object);
			}
		}

		return *this;
	}
};


FReferenceCollector::FReferenceCollector()
	: DefaultReferenceCollectorArchive(nullptr)
	, PersistentFrameReferenceCollectorArchive(nullptr)
{
}

FReferenceCollector::~FReferenceCollector()
{
	delete DefaultReferenceCollectorArchive;
	delete PersistentFrameReferenceCollectorArchive;
}

void FReferenceCollector::CreateVerySlowReferenceCollectorArchive()
{
	check(DefaultReferenceCollectorArchive == nullptr);
	DefaultReferenceCollectorArchive = new FSimpleObjectReferenceCollectorArchive(nullptr, *this);
}

void FReferenceCollector::CreatePersistentFrameReferenceCollectorArchive()
{
	check(PersistentFrameReferenceCollectorArchive == nullptr);
	PersistentFrameReferenceCollectorArchive = new FPersistentFrameCollectorArchive(nullptr, *this);
}

/**
 * Archive for tagging unreachable objects in a non recursive manner.
 */
class FCollectorTagUsedNonRecursive : public FReferenceCollector
{
public:

	/**
	 * Default constructor.
	 */
	FCollectorTagUsedNonRecursive()
		:	CurrentObject(NULL)
	{
	}

	// FReferenceCollector interface
	virtual bool IsIgnoringArchetypeRef() const override
	{
		return false;
	}
	virtual bool IsIgnoringTransient() const override
	{
		return false;
	}

	/**
	 * Performs reachability analysis. This information is later used by e.g. IncrementalPurgeGarbage or IsReferenced. The 
	 * algorithm is a simple mark and sweep where all objects are marked as unreachable. The root set passed in is 
	 * considered referenced and also objects that have any of the KeepFlags but none of the IgnoreFlags. RF_PendingKill is 
	 * implicitly part of IgnoreFlags and no object in the root set can have this flag set.
	 *
	 * @param KeepFlags		Objects with these flags will be kept regardless of being referenced or not (see line below)
	 * @param SearchFlags	If set, ignore objects with these flags for initial set, and stop recursion when found
	 * @param FoundReferences	If non-NULL, fill in with all objects that point to an object with SearchFlags set
	 * @param 
	 */
	void PerformReachabilityAnalysis( EObjectFlags KeepFlags, EInternalObjectFlags InternalKeepFlags, EObjectFlags SearchFlags = RF_NoFlags, FReferencerInformationList* FoundReferences = NULL)
	{
		// Reset object count.
		extern int32 GObjectCountDuringLastMarkPhase;
		GObjectCountDuringLastMarkPhase = 0;
		ReferenceSearchFlags = SearchFlags;
		FoundReferencesList = FoundReferences;

		// Iterate over all objects.
		for( FObjectIterator It; It; ++It )
		{
			UObject* Object	= *It;
			checkSlow(Object->IsValidLowLevel());
			GObjectCountDuringLastMarkPhase++;

			// Special case handling for objects that are part of the root set.
			if( Object->IsRooted() )
			{
				checkSlow( Object->IsValidLowLevel() );
				// We cannot use RF_PendingKill on objects that are part of the root set.
				checkCode( if( Object->IsPendingKill() ) { UE_LOG(LogUObjectGlobals, Fatal, TEXT("Object %s is part of root set though has been marked RF_PendingKill!"), *Object->GetFullName() ); } );
				// Add to list of objects to serialize.
				ObjectsToSerialize.Add( Object );
			}
			// Regular objects.
			else
			{
				// Mark objects as unreachable unless they have any of the passed in KeepFlags set and none of the passed in Search.
				if (!Object->HasAnyFlags(SearchFlags) &&
					((KeepFlags == RF_NoFlags && InternalKeepFlags == EInternalObjectFlags::None) || Object->HasAnyFlags(KeepFlags) || Object->HasAnyInternalFlags(InternalKeepFlags))
					)
				{
					ObjectsToSerialize.Add(Object);
				}
				else
				{
					Object->SetInternalFlags(EInternalObjectFlags::Unreachable);
				}
			}
		}

		// Keep serializing objects till we reach the end of the growing array at which point
		// we are done.
		int32 CurrentIndex = 0;
		while( CurrentIndex < ObjectsToSerialize.Num() )
		{
			CurrentObject = ObjectsToSerialize[CurrentIndex++];
			CurrentReferenceInfo = NULL;

			// Serialize object.
			FindReferences( CurrentObject );
		}
	}

private:

	void FindReferences( UObject* Object )
	{
		check(Object != NULL);

		if( !Object->GetClass()->IsChildOf(UClass::StaticClass()) )
		{
			FSimpleObjectReferenceCollectorArchive CollectorArchive( Object, *this );
			Object->SerializeScriptProperties( CollectorArchive );
		}
		Object->CallAddReferencedObjects(*this);
	}

	/**
	 * Adds passed in object to ObjectsToSerialize list and also removed RF_Unreachable
	 * which is used to signify whether an object already is in the list or not.
	 *
	 * @param	Object	object to add
	 */
	void AddToObjectList( const UObject* ReferencingObject, const UObject* ReferencingProperty, UObject* Object )
	{
#if ENABLE_GC_DEBUG_OUTPUT
		// this message is to help track down culprits behind "Object in PIE world still referenced" errors
		if ( GIsEditor && !GIsPlayInEditorWorld && !CurrentObject->RootPackageHasAnyFlags(PKG_PlayInEditor) && Object->RootPackageHasAnyFlags(PKG_PlayInEditor) )
		{
			UE_LOG(LogGarbage, Warning, TEXT("GC detected illegal reference to PIE object from content [possibly via %s]:"), *ReferencingProperty->GetFullName());
			UE_LOG(LogGarbage, Warning, TEXT("      PIE object: %s"), *Object->GetFullName());
			UE_LOG(LogGarbage, Warning, TEXT("  NON-PIE object: %s"), *CurrentObject->GetFullName());
		}
#endif

		// Mark it as reachable.
		Object->ThisThreadAtomicallyClearedRFUnreachable();

		// Add it to the list of objects to serialize.
		ObjectsToSerialize.Add( Object );
	}

	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override
	{
		checkSlow(!InObject || InObject->IsValidLowLevel());
		if (InObject)
		{
			if (InObject->HasAnyFlags(ReferenceSearchFlags))
			{
				// Stop recursing, and add to the list of references
				if (FoundReferencesList)
				{
					if (!CurrentReferenceInfo)
					{
						CurrentReferenceInfo = new(FoundReferencesList->ExternalReferences) FReferencerInformation(CurrentObject);
					}
					if (InReferencingProperty)
					{
						CurrentReferenceInfo->ReferencingProperties.AddUnique(InReferencingProperty);
					}
					CurrentReferenceInfo->TotalReferences++;
				}
				// Mark it as reachable.
				InObject->ThisThreadAtomicallyClearedRFUnreachable();
			}
			else if (InObject->IsUnreachable())
			{
				// Add encountered object reference to list of to be serialized objects if it hasn't already been added.
				AddToObjectList(InReferencingObject, InReferencingProperty, InObject);
			}
		}
	}

	/** Object we're currently serializing */
	UObject*			CurrentObject;
	/** Growing array of objects that require serialization */
	TArray<UObject*>	ObjectsToSerialize;
	/** Ignore any references from objects that match these flags */
	EObjectFlags		ReferenceSearchFlags;
	/** List of found references to fill in, if valid */
	FReferencerInformationList*	FoundReferencesList;
	/** Current reference info being filled out */
	FReferencerInformation *CurrentReferenceInfo;
};

/**
 * Returns whether an object is referenced, not counting the one
 * reference at Obj.
 *
 * @param	Obj			Object to check
 * @param	KeepFlags	Objects with these flags will be considered as being referenced. If RF_NoFlags, check all objects
 * @param	bCheckSubObjects	Treat subobjects as if they are the same as passed in object
 * @param	FoundReferences		If non-NULL fill in with list of objects that hold references
 * @return true if object is referenced, false otherwise
 */
bool IsReferenced(UObject*& Obj, EObjectFlags KeepFlags, EInternalObjectFlags InternalKeepFlags, bool bCheckSubObjects, FReferencerInformationList* FoundReferences)
{
	check(!Obj->IsUnreachable());

	FScopedObjectFlagMarker ObjectFlagMarker;
	bool bTempReferenceList = false;

	// Tag objects.
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;
		Object->ClearFlags( RF_TagGarbageTemp );
	}
	// Ignore this object and possibly subobjects
	Obj->SetFlags( RF_TagGarbageTemp );

	if (FoundReferences)
	{
		// Clear old references
		FoundReferences->ExternalReferences.Empty();
		FoundReferences->InternalReferences.Empty();
	}

	if (bCheckSubObjects)
	{
		if (!FoundReferences)
		{
			// Allocate a temporary reference list
			FoundReferences = new FReferencerInformationList;
			bTempReferenceList = true;
		}
		Obj->TagSubobjects( RF_TagGarbageTemp );
	}

	FCollectorTagUsedNonRecursive ObjectReferenceTagger;
	// Exclude passed in object when peforming reachability analysis.
	ObjectReferenceTagger.PerformReachabilityAnalysis(KeepFlags, InternalKeepFlags, RF_TagGarbageTemp, FoundReferences);

	bool bIsReferenced = false;
	if (FoundReferences)
	{
		bool bReferencedByOuters = false;		
		// Move some from external to internal before returning
		for (int32 i = 0; i < FoundReferences->ExternalReferences.Num(); i++)
		{
			FReferencerInformation *OldRef = &FoundReferences->ExternalReferences[i];
			if (OldRef->Referencer == Obj)
			{
				FoundReferences->ExternalReferences.RemoveAt(i);
				i--;
			}
			else if (OldRef->Referencer->IsIn(Obj))
			{
				bReferencedByOuters = true;
				FReferencerInformation *NewRef = new(FoundReferences->InternalReferences) FReferencerInformation(OldRef->Referencer, OldRef->TotalReferences, OldRef->ReferencingProperties);
				FoundReferences->ExternalReferences.RemoveAt(i);
				i--;
			}
		}
		bIsReferenced = FoundReferences->ExternalReferences.Num() > 0 || bReferencedByOuters || !Obj->IsUnreachable();
	}
	else
	{
		// Return whether the object was referenced and restore original state.
		bIsReferenced = !Obj->IsUnreachable();
	}
	
	if (bTempReferenceList)
	{
		// We allocated a temp list
		delete FoundReferences;
	}

	return bIsReferenced;
}


FArchive& FScriptInterface::Serialize(FArchive& Ar, UClass* InterfaceType)
{
	UObject* ObjectValue = GetObject();
	Ar << ObjectValue;
	SetObject(ObjectValue);
	if (Ar.IsLoading())
	{
		SetInterface(ObjectValue != NULL ? ObjectValue->GetInterfaceAddress(InterfaceType) : NULL);
	}
	return Ar;
}

/** A struct used as stub for deleted ones. */
UScriptStruct* GetFallbackStruct()
{
	return TBaseStructure<FFallbackStruct>::Get();
}

UObject* FObjectInitializer::CreateDefaultSubobject(UObject* Outer, FName SubobjectFName, UClass* ReturnType, UClass* ClassToCreateByDefault, bool bIsRequired, bool bAbstract, bool bIsTransient) const
{
	UE_CLOG(!FUObjectThreadContext::Get().IsInConstructor, LogClass, Fatal, TEXT("Subobjects cannot be created outside of UObject constructors. UObject constructing subobjects cannot be created using new or placement new operator."));
	if (SubobjectFName == NAME_None)
	{
		UE_LOG(LogClass, Fatal, TEXT("Illegal default subobject name: %s"), *SubobjectFName.ToString());
	}

	UObject* Result = NULL;
	UClass* OverrideClass = ComponentOverrides.Get(SubobjectFName, ReturnType, ClassToCreateByDefault, *this);
	if (!OverrideClass && bIsRequired)
	{
		OverrideClass = ClassToCreateByDefault;
		UE_LOG(LogClass, Warning, TEXT("Ignored DoNotCreateDefaultSubobject for %s as it's marked as required. Creating %s."), *SubobjectFName.ToString(), *OverrideClass->GetName());
	}
	if (OverrideClass)
	{
		check(OverrideClass->IsChildOf(ReturnType));

		// Abstract sub-objects are only allowed when explicitly created with CreateAbstractDefaultSubobject.
		if (!OverrideClass->HasAnyClassFlags(CLASS_Abstract) || !bAbstract)
		{
			UObject* Template = OverrideClass->GetDefaultObject(); // force the CDO to be created if it hasn't already
			EObjectFlags SubobjectFlags = Outer->GetMaskedFlags(RF_PropagateToSubObjects);
			bool bOwnerArchetypeIsNotNative;
			UClass* OuterArchetypeClass;

			// It is not safe to mark this component as properly transient, that results in it being nulled incorrectly

			OuterArchetypeClass = Outer->GetArchetype()->GetClass();
			bOwnerArchetypeIsNotNative = !OuterArchetypeClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic);

			const bool bOwnerTemplateIsNotCDO = ObjectArchetype != nullptr && ObjectArchetype != Outer->GetClass()->GetDefaultObject(false) && !Outer->HasAnyFlags(RF_ClassDefaultObject);
#if !UE_BUILD_SHIPPING
			// Guard against constructing the same subobject multiple times.
			// We only need to check the name as ConstructObject would fail anyway if an object of the same name but different class already existed.
			if (ConstructedSubobjects.Find(SubobjectFName) != INDEX_NONE)
			{
				UE_LOG(LogClass, Fatal, TEXT("Default subobject %s %s already exists for %s."), *OverrideClass->GetName(), *SubobjectFName.ToString(), *Outer->GetFullName());
			}
			else
			{
				ConstructedSubobjects.Add(SubobjectFName);
			}
#endif
			Result = StaticConstructObject_Internal(OverrideClass, Outer, SubobjectFName, SubobjectFlags);
			if (!bIsTransient && (bOwnerArchetypeIsNotNative || bOwnerTemplateIsNotCDO))
			{
				UObject* MaybeTemplate = nullptr;
				if (bOwnerTemplateIsNotCDO)
				{
					// Try to get the subobject template from the specified object template
					MaybeTemplate = ObjectArchetype->GetDefaultSubobjectByName(SubobjectFName);
				}
				if (!MaybeTemplate)
				{
					// The archetype of the outer is not native, so we need to copy properties to the subobjects after the C++ constructor chain for the outer has run (because those sets properties on the subobjects)
					MaybeTemplate = OuterArchetypeClass->GetDefaultSubobjectByName(SubobjectFName);
				}
				if (MaybeTemplate && MaybeTemplate->IsA(ReturnType) && Template != MaybeTemplate)
				{
					ComponentInits.Add(Result, MaybeTemplate);
				}
			}
			if (Outer->HasAnyFlags(RF_ClassDefaultObject) && Outer->GetClass()->GetSuperClass())
			{
#if WITH_EDITOR
				// Default subobjects on the CDO should be transactional, so that we can undo/redo changes made to those objects.
				// One current example of this is editing natively defined components in the Blueprint Editor.
				Result->SetFlags(RF_Transactional);
#endif
				Outer->GetClass()->AddDefaultSubobject(Result, ReturnType);
			}
			Result->SetFlags(RF_DefaultSubObject);
			// Clear PendingKill flag in case we recycled a subobject of a dead object.
			// @todo: we should not be recycling subobjects unless we're currently loading from a package
			Result->ClearInternalFlags(EInternalObjectFlags::PendingKill);
		}
	}
	return Result;
}
UObject* FObjectInitializer::CreateEditorOnlyDefaultSubobject(UObject* Outer, FName SubobjectName, UClass* ReturnType, bool bTransient /*= false*/) const
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		UObject* EditorSubobject = CreateDefaultSubobject(Outer, SubobjectName, ReturnType, ReturnType, /*bIsRequired =*/ false, /*bIsAbstract =*/ false, bTransient);
		if (EditorSubobject)
		{
			EditorSubobject->MarkAsEditorOnlySubobject();
		}
		return EditorSubobject;
	}
#endif
	return nullptr;
}

COREUOBJECT_API UFunction* FindDelegateSignature(FName DelegateSignatureName)
{
	FString StringName = DelegateSignatureName.ToString();

	if (StringName.EndsWith(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX))
	{
		return FindObject<UFunction>(ANY_PACKAGE, *StringName);
	}

	return nullptr;
}

/**
 * Takes a path of some sort and attempts to turn it into the asset log's canonical path.
 */
FString FAssetMsg::FormatPathForAssetLog(const TCHAR* InPath)
{
	static bool Once = false;
	static bool ShowDiskPath = true;

	if (!Once)
	{
		GConfig->GetBool(TEXT("Core.System"), TEXT("AssetLogShowsDiskPath"), ShowDiskPath, GEngineIni);
		Once = true;
	}

	if (FPlatformProperties::RequiresCookedData() || !ShowDiskPath)
	{
		return FString(InPath);
	}
	
	FString AssetPath = InPath;
	FString FilePath;

	// check for /Game/Path/Package.obj and turn it into a package reference
	if (FPackageName::IsValidObjectPath(AssetPath))
	{
		AssetPath = FPackageName::ObjectPathToPackageName(AssetPath);
	}

	// Try to convert this to a file path
	if (FPackageName::DoesPackageExist(AssetPath, 0, &FilePath) == false)
	{
		// if failed, assume we were given something that's a file path (e.g. ../../../Game/Whatever)
		FilePath = AssetPath;
	}

	// if that succeeded FilePath will be a relative path to a  file, if not just assume that's what we were given and proceed...
	if (IFileManager::Get().FileExists(*FilePath) == false)
	{
		return FString::Printf(TEXT("%s (no disk path found)"), InPath);
	}

	// turn this into an absolute path for error logging
	FilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FilePath);
	
	// turn into a native platform file
	FPaths::MakePlatformFilename(FilePath);
	return FilePath;
}

/**
 * Format the path of the passed in object
 */
FString FAssetMsg::FormatPathForAssetLog(const UObject* Object)
{
	FString ResolvedPath;
	ResolvedPath = FormatPathForAssetLog(*Object->GetPathName());
	return ResolvedPath;
}


namespace UE4CodeGen_Private
{
	void ConstructUProperty(UObject* Outer, const FPropertyParamsBase* const*& PropertyArray, int32& NumProperties)
	{
		const FPropertyParamsBase* PropBase = *PropertyArray++;

		uint32 ReadMore = 0;

#if WITH_METADATA
		const FMetaDataPairParam* MetaDataArray = nullptr;
		int32                     NumMetaData   = 0;
#endif

		UProperty* NewProp = nullptr;
		switch (PropBase->Type)
		{
			default:
			{
				// Unsupported property type
				check(false);
			}

			case EPropertyClass::Byte:
			{
				const FBytePropertyParams* Prop = (const FBytePropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UByteProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->EnumFunc ? Prop->EnumFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Int8:
			{
				const FInt8PropertyParams* Prop = (const FInt8PropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UInt8Property(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Int16:
			{
				const FInt16PropertyParams* Prop = (const FInt16PropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UInt16Property(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Int:
			{
				const FIntPropertyParams* Prop = (const FIntPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UIntProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Int64:
			{
				const FInt64PropertyParams* Prop = (const FInt64PropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UInt64Property(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::UInt16:
			{
				const FUInt16PropertyParams* Prop = (const FUInt16PropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UUInt16Property(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::UInt32:
			{
				const FUInt32PropertyParams* Prop = (const FUInt32PropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UUInt32Property(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::UInt64:
			{
				const FUInt64PropertyParams* Prop = (const FUInt64PropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UUInt64Property(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::UnsizedInt:
			{
				const FUnsizedIntPropertyParams* Prop = (const FUnsizedIntPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UUInt64Property(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::UnsizedUInt:
			{
				const FUnsizedUIntPropertyParams* Prop = (const FUnsizedUIntPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UUInt64Property(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Float:
			{
				const FFloatPropertyParams* Prop = (const FFloatPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UFloatProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Double:
			{
				const FDoublePropertyParams* Prop = (const FDoublePropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UDoubleProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Bool:
			{
				auto DoDetermineBitfieldOffsetAndMask = [](uint32& Offset, uint32& BitMask, void (*SetBit)(void* Obj), const SIZE_T SizeOf)
				{
					TUniquePtr<uint8[]> Buffer = MakeUnique<uint8[]>(SizeOf);

					SetBit(Buffer.Get());

					// Here we are making the assumption that bitfields are aligned in the struct. Probably true.
					// If not, it may be ok unless we are on a page boundary or something, but the check will fire in that case.
					// Have faith.
					for (uint32 TestOffset = 0; TestOffset < SizeOf; TestOffset++)
					{
						if (uint8 Mask = Buffer[TestOffset])
						{
							Offset = TestOffset;
							BitMask = (uint32)Mask;
							check(FMath::RoundUpToPowerOfTwo(BitMask) == BitMask); // better be only one bit on
							break;
						}
					}
				};

				const FBoolPropertyParams* Prop = (const FBoolPropertyParams*)PropBase;
				uint32 Offset = 0;
				uint32 BitMask = 0;
				if (Prop->SetBitFunc)
				{
					DoDetermineBitfieldOffsetAndMask(Offset, BitMask, Prop->SetBitFunc, Prop->SizeOfOuter);
					check(BitMask);
				}

				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UBoolProperty(FObjectInitializer(), EC_CppProperty, Offset, Prop->PropertyFlags, BitMask, Prop->ElementSize, Prop->NativeBool == ENativeBool::Native);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Object:
			{
				const FObjectPropertyParams* Prop = (const FObjectPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UObjectProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->ClassFunc ? Prop->ClassFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::WeakObject:
			{
				const FWeakObjectPropertyParams* Prop = (const FWeakObjectPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UWeakObjectProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->ClassFunc ? Prop->ClassFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::LazyObject:
			{
				const FLazyObjectPropertyParams* Prop = (const FLazyObjectPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) ULazyObjectProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->ClassFunc ? Prop->ClassFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::SoftObject:
			{
				const FSoftObjectPropertyParams* Prop = (const FSoftObjectPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) USoftObjectProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->ClassFunc ? Prop->ClassFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Class:
			{
				const FClassPropertyParams* Prop = (const FClassPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UClassProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->MetaClassFunc ? Prop->MetaClassFunc() : nullptr, Prop->ClassFunc ? Prop->ClassFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::SoftClass:
			{
				const FSoftClassPropertyParams* Prop = (const FSoftClassPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) USoftClassProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->MetaClassFunc ? Prop->MetaClassFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Interface:
			{
				const FInterfacePropertyParams* Prop = (const FInterfacePropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UInterfaceProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->InterfaceClassFunc ? Prop->InterfaceClassFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Name:
			{
				const FNamePropertyParams* Prop = (const FNamePropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UNameProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Str:
			{
				const FStrPropertyParams* Prop = (const FStrPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UStrProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Array:
			{
				const FArrayPropertyParams* Prop = (const FArrayPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UArrayProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

				// Next property is the array inner
				ReadMore = 1;

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Map:
			{
				const FMapPropertyParams* Prop = (const FMapPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UMapProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

				// Next two properties are the map key and value inners
				ReadMore = 2;

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Set:
			{
				const FSetPropertyParams* Prop = (const FSetPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) USetProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

				// Next property is the set inner
				ReadMore = 1;

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Struct:
			{
				const FStructPropertyParams* Prop = (const FStructPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UStructProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->ScriptStructFunc ? Prop->ScriptStructFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Delegate:
			{
				const FDelegatePropertyParams* Prop = (const FDelegatePropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UDelegateProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->SignatureFunctionFunc ? Prop->SignatureFunctionFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::MulticastDelegate:
			{
				const FMulticastDelegatePropertyParams* Prop = (const FMulticastDelegatePropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UMulticastDelegateProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->SignatureFunctionFunc ? Prop->SignatureFunctionFunc() : nullptr);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Text:
			{
				const FTextPropertyParams* Prop = (const FTextPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UTextProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags);

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;

			case EPropertyClass::Enum:
			{
				const FEnumPropertyParams* Prop = (const FEnumPropertyParams*)PropBase;
				NewProp = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Prop->NameUTF8), Prop->ObjectFlags) UEnumProperty(FObjectInitializer(), EC_CppProperty, Prop->Offset, Prop->PropertyFlags, Prop->EnumFunc ? Prop->EnumFunc() : nullptr);

				// Next property is the underlying integer property
				ReadMore = 1;

#if WITH_METADATA
				MetaDataArray = Prop->MetaDataArray;
				NumMetaData   = Prop->NumMetaData;
#endif
			}
			break;
		}

#if WITH_METADATA
		if (NumMetaData)
		{
			UMetaData* MetaData = NewProp->GetOutermost()->GetMetaData();
			for (const FMetaDataPairParam* MetaDataData = MetaDataArray, *MetaDataEnd = MetaDataData + NumMetaData; MetaDataData != MetaDataEnd; ++MetaDataData)
			{
				MetaData->SetValue(NewProp, UTF8_TO_TCHAR(MetaDataData->NameUTF8), UTF8_TO_TCHAR(MetaDataData->ValueUTF8));
			}
		}
#endif

		NewProp->ArrayDim = PropBase->ArrayDim;
		if (PropBase->RepNotifyFuncUTF8)
		{
			NewProp->RepNotifyFunc = FName(UTF8_TO_TCHAR(PropBase->RepNotifyFuncUTF8));
		}

		--NumProperties;

		for (; ReadMore; --ReadMore)
		{
			ConstructUProperty(NewProp, PropertyArray, NumProperties);
		}
	}

	void ConstructUProperties(UObject* Outer, const FPropertyParamsBase* const* PropertyArray, int32 NumProperties)
	{
		while (NumProperties)
		{
			ConstructUProperty(Outer, PropertyArray, NumProperties);
		}
	}

#if WITH_METADATA
	void AddMetaData(UObject* Object, const FMetaDataPairParam* MetaDataArray, int32 NumMetaData)
	{
		if (NumMetaData)
		{
			UMetaData* MetaData = Object->GetOutermost()->GetMetaData();
			for (const FMetaDataPairParam* MetaDataParam = MetaDataArray, *MetaDataParamEnd = MetaDataParam + NumMetaData; MetaDataParam != MetaDataParamEnd; ++MetaDataParam)
			{
				MetaData->SetValue(Object, UTF8_TO_TCHAR(MetaDataParam->NameUTF8), UTF8_TO_TCHAR(MetaDataParam->ValueUTF8));
			}
		}
	}
#endif

	void ConstructUFunction(UFunction*& OutFunction, const FFunctionParams& Params)
	{
		UObject*   (*OuterFunc)() = Params.OuterFunc;
		UFunction* (*SuperFunc)() = Params.SuperFunc;

		UObject*   Outer = OuterFunc ? OuterFunc() : nullptr;
		UFunction* Super = SuperFunc ? SuperFunc() : nullptr;

		if (OutFunction)
		{
			return;
		}

		UFunction* NewFunction;
		if (Params.FunctionFlags & FUNC_Delegate)
		{
			NewFunction = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Params.NameUTF8), Params.ObjectFlags) UDelegateFunction(
				FObjectInitializer(),
				Super,
				Params.FunctionFlags,
				Params.StructureSize
			);
		}
		else
		{
			NewFunction = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Params.NameUTF8), Params.ObjectFlags) UFunction(
				FObjectInitializer(),
				Super,
				Params.FunctionFlags,
				Params.StructureSize
			);
		}
		OutFunction = NewFunction;

#if WITH_METADATA
		AddMetaData(NewFunction, Params.MetaDataArray, Params.NumMetaData);
#endif

		ConstructUProperties(NewFunction, Params.PropertyArray, Params.NumProperties);

		NewFunction->Bind();
		NewFunction->StaticLink();
	}

	void ConstructUEnum(UEnum*& OutEnum, const FEnumParams& Params)
	{
		UObject* (*OuterFunc)() = Params.OuterFunc;

		UObject* Outer = OuterFunc ? OuterFunc() : nullptr;

		if (OutEnum)
		{
			return;
		}

		UEnum* NewEnum = new (EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Params.NameUTF8), Params.ObjectFlags) UEnum(FObjectInitializer());
		OutEnum = NewEnum;

		TArray<TPair<FName, int64>> EnumNames;
		EnumNames.Reserve(Params.NumEnumerators);
		for (const FEnumeratorParam* Enumerator = Params.EnumeratorParams, *EnumeratorEnd = Enumerator + Params.NumEnumerators; Enumerator != EnumeratorEnd; ++Enumerator)
		{
			EnumNames.Emplace(UTF8_TO_TCHAR(Enumerator->NameUTF8), Enumerator->Value);
		}

		NewEnum->SetEnums(EnumNames, (UEnum::ECppForm)Params.CppForm, Params.DynamicType == EDynamicType::NotDynamic);
		NewEnum->CppType = UTF8_TO_TCHAR(Params.CppTypeUTF8);

		if (Params.DisplayNameFunc)
		{
			NewEnum->SetEnumDisplayNameFn(Params.DisplayNameFunc);
		}

#if WITH_METADATA
		AddMetaData(NewEnum, Params.MetaDataArray, Params.NumMetaData);
#endif
	}

	void ConstructUScriptStruct(UScriptStruct*& OutStruct, const FStructParams& Params)
	{
		UObject*                      (*OuterFunc)()     = Params.OuterFunc;
		UScriptStruct*                (*SuperFunc)()     = Params.SuperFunc;
		UScriptStruct::ICppStructOps* (*StructOpsFunc)() = (UScriptStruct::ICppStructOps* (*)())Params.StructOpsFunc;

		UObject*                      Outer     = OuterFunc     ? OuterFunc() : nullptr;
		UScriptStruct*                Super     = SuperFunc     ? SuperFunc() : nullptr;
		UScriptStruct::ICppStructOps* StructOps = StructOpsFunc ? StructOpsFunc() : nullptr;

		if (OutStruct)
		{
			return;
		}

		UScriptStruct* NewStruct = new(EC_InternalUseOnlyConstructor, Outer, UTF8_TO_TCHAR(Params.NameUTF8), Params.ObjectFlags) UScriptStruct(FObjectInitializer(), Super, StructOps, (EStructFlags)Params.StructFlags, Params.SizeOf, Params.AlignOf);
		OutStruct = NewStruct;

		ConstructUProperties(NewStruct, Params.PropertyArray, Params.NumProperties);

		NewStruct->StaticLink();

#if WITH_METADATA
		AddMetaData(NewStruct, Params.MetaDataArray, Params.NumMetaData);
#endif
	}

	void ConstructUPackage(UPackage*& OutPackage, const FPackageParams& Params)
	{
		if (OutPackage)
		{
			return;
		}

		UPackage* NewPackage = CastChecked<UPackage>(StaticFindObjectFast(UPackage::StaticClass(), nullptr, FName(UTF8_TO_TCHAR(Params.NameUTF8)), false, false));
		OutPackage = NewPackage;

#if WITH_METADATA
		AddMetaData(NewPackage, Params.MetaDataArray, Params.NumMetaData);
#endif

		NewPackage->SetPackageFlags(Params.PackageFlags);
		NewPackage->SetGuid(FGuid(Params.BodyCRC, Params.DeclarationsCRC, 0u, 0u));

		for (UObject* (*const *SingletonFunc)() = Params.SingletonFuncArray, *(*const *SingletonFuncEnd)() = SingletonFunc + Params.NumSingletons; SingletonFunc != SingletonFuncEnd; ++SingletonFunc)
		{
			(*SingletonFunc)();
		}
	}

	void ConstructUClass(UClass*& OutClass, const FClassParams& Params)
	{
		if (OutClass && (OutClass->ClassFlags & CLASS_Constructed))
		{
			return;
		}

		for (UObject* (*const *SingletonFunc)() = Params.DependencySingletonFuncArray, *(*const *SingletonFuncEnd)() = SingletonFunc + Params.NumDependencySingletons; SingletonFunc != SingletonFuncEnd; ++SingletonFunc)
		{
			(*SingletonFunc)();
		}

		UClass* NewClass = Params.ClassNoRegisterFunc();
		OutClass = NewClass;

		if (NewClass->ClassFlags & CLASS_Constructed)
		{
			return;
		}

		UObjectForceRegistration(NewClass);

		NewClass->ClassFlags |= (EClassFlags)(Params.ClassFlags | CLASS_Constructed);
		// Make sure the reference token stream is empty since it will be reconstructed later on
		// This should not apply to intrinsic classes since they emit native references before AssembleReferenceTokenStream is called.
		if ((NewClass->ClassFlags & CLASS_Intrinsic) != CLASS_Intrinsic)
		{
			check((NewClass->ClassFlags & CLASS_TokenStreamAssembled) != CLASS_TokenStreamAssembled);
			NewClass->ReferenceTokenStream.Empty();
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
			NewClass->DebugTokenMap.Empty();
#endif
		}
		NewClass->CreateLinkAndAddChildFunctionsToMap(Params.FunctionLinkArray, Params.NumFunctions);

		ConstructUProperties(NewClass, Params.PropertyArray, Params.NumProperties);

		if (Params.ClassConfigNameUTF8)
		{
			NewClass->ClassConfigName = FName(UTF8_TO_TCHAR(Params.ClassConfigNameUTF8));
		}

		NewClass->SetCppTypeInfoStatic(Params.CppClassInfo);

		if (int32 NumImplementedInterfaces = Params.NumImplementedInterfaces)
		{
			NewClass->Interfaces.Reserve(NumImplementedInterfaces);
			for (const FImplementedInterfaceParams* ImplementedInterface = Params.ImplementedInterfaceArray, *ImplementedInterfaceEnd = ImplementedInterface + NumImplementedInterfaces; ImplementedInterface != ImplementedInterfaceEnd; ++ImplementedInterface)
			{
				UClass* (*ClassFunc)() = ImplementedInterface->ClassFunc;
				UClass* InterfaceClass = ClassFunc ? ClassFunc() : nullptr;

				NewClass->Interfaces.Emplace(InterfaceClass, ImplementedInterface->Offset, ImplementedInterface->bImplementedByK2);
			}
		}

#if WITH_METADATA
		AddMetaData(NewClass, Params.MetaDataArray, Params.NumMetaData);
#endif

		NewClass->StaticLink();
	}
}
