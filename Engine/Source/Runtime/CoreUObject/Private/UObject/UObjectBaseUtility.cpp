// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectBaseUtility.cpp: Unreal UObject functions that only depend on UObjectBase
=============================================================================*/

#include "UObject/UObjectBaseUtility.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "UObject/Interface.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/MallocProfiler.h"

/***********************/
/******** Names ********/
/***********************/

/**
 * Returns the fully qualified pathname for this object, in the format:
 * 'Outermost.[Outer:]Name'
 *
 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
 *
 * @note	safe to call on NULL object pointers!
 */
FString UObjectBaseUtility::GetPathName( const UObject* StopOuter/*=NULL*/ ) const
{
	FString Result;
	GetPathName(StopOuter, Result);
	return Result;
}

/**
 * Internal version of GetPathName() that eliminates unnecessary copies.
 */
void UObjectBaseUtility::GetPathName( const UObject* StopOuter, FString& ResultString ) const
{
	if( this != StopOuter && this != NULL )
	{
		UObject* ObjOuter = GetOuter();
		if (ObjOuter && ObjOuter != StopOuter )
		{
			ObjOuter->GetPathName( StopOuter, ResultString );

			// SUBOBJECT_DELIMITER is used to indicate that this object's outer is not a UPackage
			if (ObjOuter->GetClass() != UPackage::StaticClass()
			&& ObjOuter->GetOuter()->GetClass() == UPackage::StaticClass())
			{
				ResultString += SUBOBJECT_DELIMITER;
			}
			else
			{
				ResultString += TEXT(".");
			}
		}
		AppendName(ResultString);
	}
	else
	{
		ResultString += TEXT("None");
	}
}

/**
 * Returns the fully qualified pathname for this object as well as the name of the class, in the format:
 * 'ClassName Outermost.[Outer:]Name'.
 *
 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
 *
 * @note	safe to call on NULL object pointers!
 */
FString UObjectBaseUtility::GetFullName( const UObject* StopOuter/*=NULL*/ ) const
{
	FString Result;  
	if( this != nullptr )
	{
		Result.Empty(128);
		GetClass()->AppendName(Result);
		Result += TEXT(" ");
		GetPathName( StopOuter, Result );
		// could possibly put a Result.Shrink() here, but this isn't used much in a shipping game
	}
	else
	{
		Result += TEXT("None");
	}
	return Result;  
}


/**
 * Walks up the chain of packages until it reaches the top level, which it ignores.
 *
 * @param	bStartWithOuter		whether to include this object's name in the returned string
 * @return	string containing the path name for this object, minus the outermost-package's name
 */
FString UObjectBaseUtility::GetFullGroupName( bool bStartWithOuter ) const
{
	const UObjectBaseUtility* Obj = bStartWithOuter ? GetOuter() : this;
	return Obj ? Obj->GetPathName(GetOutermost()) : TEXT("");
}


/***********************/
/******** Outer ********/
/***********************/

/** 
 * Walks up the list of outers until it finds the highest one.
 *
 * @return outermost non NULL Outer.
 */
UPackage* UObjectBaseUtility::GetOutermost() const
{
	UObject* Top = (UObject*)this;
	for (;;)
	{
		UObject* CurrentOuter = Top->GetOuter();
		if (!CurrentOuter)
		{
			return CastChecked<UPackage>(Top);
		}
		Top = CurrentOuter;
	}
}

/** 
 * Finds the outermost package and marks it dirty
 */
bool UObjectBaseUtility::MarkPackageDirty() const
{
	// since transient objects will never be saved into a package, there is no need to mark a package dirty
	// if we're transient
	if ( !HasAnyFlags(RF_Transient) )
	{
		UPackage* Package = GetOutermost();

		if( Package != NULL	)
		{
			// It is against policy to dirty a map or package during load in the Editor, to enforce this policy
			// we explicitly disable the ability to dirty a package or map during load.  Commandlets can still
			// set the dirty state on load.
			if( IsRunningCommandlet() || 
				(GIsEditor && !GIsEditorLoadingPackage && !GIsCookerLoadingPackage && !GIsPlayInEditorWorld && !IsInAsyncLoadingThread()
#if WITH_HOT_RELOAD
				&& !GIsHotReload
#endif // WITH_HOT_RELOAD
#if WITH_EDITORONLY_DATA
				&& !Package->bIsCookedForEditor // Cooked packages can't be modified nor marked as dirty
#endif
				))
			{
				const bool bIsDirty = Package->IsDirty();

				// We prevent needless re-dirtying as this can be an expensive operation.
				if( !bIsDirty )
				{
					Package->SetDirtyFlag(true);
				}

				// Always call PackageMarkedDirtyEvent, even when the package is already dirty
				Package->PackageMarkedDirtyEvent.Broadcast(Package, bIsDirty);

				return true;
			}
			else
			{
				// notify the caller that the request to mark the package as dirty was suppressed
				return false;
			}
		}
	}
	return true;
}

/**
* Determines whether this object is a template object
*
* @return	true if this object is a template object (owned by a UClass)
*/
bool UObjectBaseUtility::IsTemplate( EObjectFlags TemplateTypes ) const
{
	for (const UObjectBaseUtility* TestOuter = this; TestOuter; TestOuter = TestOuter->GetOuter() )
	{
		if ( TestOuter->HasAnyFlags(TemplateTypes) )
			return true;
	}

	return false;
}


/**
 * Traverses the outer chain searching for the next object of a certain type.  (T must be derived from UObject)
 *
 * @param	Target class to search for
 * @return	a pointer to the first object in this object's Outer chain which is of the correct type.
 */
UObject* UObjectBaseUtility::GetTypedOuter(UClass* Target) const
{
	UObject* Result = NULL;
	for ( UObject* NextOuter = GetOuter(); Result == NULL && NextOuter != NULL; NextOuter = NextOuter->GetOuter() )
	{
		if ( NextOuter->IsA(Target) )
		{
			Result = NextOuter;
		}
	}
	return Result;
}

/*-----------------------------------------------------------------------------
	UObject accessors that depend on UClass.
-----------------------------------------------------------------------------*/

/**
 * @return	true if the specified object appears somewhere in this object's outer chain.
 */
bool UObjectBaseUtility::IsIn( const UObject* SomeOuter ) const
{
	for( UObject* It=GetOuter(); It; It=It->GetOuter() )
	{
		if( It==SomeOuter )
		{
			return true;
		}
	}
	return SomeOuter==NULL;
}

/**
 * Find out if this object is inside (has an outer) that is of the specified class
 * @param SomeBaseClass	The base class to compare against
 * @return True if this object is in an object of the given type.
 */
bool UObjectBaseUtility::IsInA( const UClass* SomeBaseClass ) const
{
	for (const UObjectBaseUtility* It=this; It; It = It->GetOuter())
	{
		if (It->IsA(SomeBaseClass))
		{
			return true;
		}
	}
	return SomeBaseClass == NULL;
}

/**
* Checks whether this object's top-most package has any of the specified flags
*
* @param	CheckFlagMask	a bitmask of EPackageFlags values to check for
*
* @return	true if the PackageFlags member of this object's top-package has any bits from the mask set.
*/
bool UObjectBaseUtility::RootPackageHasAnyFlags( uint32 CheckFlagMask ) const
{
	return GetOutermost()->HasAnyPackageFlags(CheckFlagMask);
}

/***********************/
/******** Class ********/
/***********************/

/**
 * @return	true if this object is of the specified type.
 */
#if UCLASS_FAST_ISA_COMPARE_WITH_OUTERWALK || UCLASS_FAST_ISA_IMPL == UCLASS_ISA_OUTERWALK
	bool UObjectBaseUtility::IsA( const UClass* SomeBase ) const
	{
		UE_CLOG(!SomeBase, LogObj, Fatal, TEXT("IsA(NULL) cannot yield meaningful results"));

		UClass* ThisClass = GetClass();

		bool bOldResult = false;
		for ( UClass* TempClass=ThisClass; TempClass; TempClass=TempClass->GetSuperClass() )
		{
			if ( TempClass == SomeBase )
			{
				bOldResult = true;
				break;
			}
		}

	#if UCLASS_FAST_ISA_IMPL == UCLASS_ISA_INDEXTREE
		bool bNewResult = ThisClass->IsAUsingFastTree(*SomeBase);
	#elif UCLASS_FAST_ISA_IMPL == UCLASS_ISA_CLASSARRAY
		bool bNewResult = ThisClass->IsAUsingClassArray(*SomeBase);
	#endif

	#if UCLASS_FAST_ISA_COMPARE_WITH_OUTERWALK
		ensureMsgf(bOldResult == bNewResult, TEXT("New cast code failed"));
	#endif

		return bOldResult;
	}
#endif


/**
 * Finds the most-derived class which is a parent of both TestClass and this object's class.
 *
 * @param	TestClass	the class to find the common base for
 */
const UClass* UObjectBaseUtility::FindNearestCommonBaseClass( const UClass* TestClass ) const
{
	const UClass* Result = NULL;

	if ( TestClass != NULL )
	{
		const UClass* CurrentClass = GetClass();

		// early out if it's the same class or one is the parent of the other
		// (the check for TestClass->IsChildOf(CurrentClass) returns true if TestClass == CurrentClass
		if ( TestClass->IsChildOf(CurrentClass) )
		{
			Result = CurrentClass;
		}
		else if ( CurrentClass->IsChildOf(TestClass) )
		{
			Result = TestClass;
		}
		else
		{
			// find the nearest parent of TestClass which is also a parent of CurrentClass
			for ( UClass* Cls = TestClass->GetSuperClass(); Cls; Cls = Cls->GetSuperClass() )
			{
				if ( CurrentClass->IsChildOf(Cls) )
				{
					Result = Cls;
					break;
				}
			}
		}
	}

	// at this point, Result should only be NULL if TestClass is NULL
	checkfSlow(Result != NULL || TestClass == NULL, TEXT("No common base class found for object '%s' with TestClass '%s'"), *GetFullName(), *TestClass->GetFullName());
	return Result;
}


/**
 * Returns a pointer to this object safely converted to a pointer to the specified interface class.
 *
 * @param	InterfaceClass	the interface class to use for the returned type
 *
 * @return	a pointer that can be assigned to a variable of the interface type specified, or NULL if this object's
 *			class doesn't implement the interface indicated.  Will be the same value as 'this' if the interface class
 *			isn't native.
 */
void* UObjectBaseUtility::GetInterfaceAddress( UClass* InterfaceClass )
{
	void* Result = NULL;

	if ( InterfaceClass != NULL && InterfaceClass->HasAnyClassFlags(CLASS_Interface) && InterfaceClass != UInterface::StaticClass() )
	{
		// Script interface
		if ( !InterfaceClass->HasAnyClassFlags(CLASS_Native) )
		{
			if ( GetClass()->ImplementsInterface(InterfaceClass) )
			{
				// if it isn't a native interface, the address won't be different
				Result = this;
			}
		}
		// Native interface
		else
		{
			for( UClass* CurrentClass=GetClass(); Result == NULL && CurrentClass != NULL; CurrentClass = CurrentClass->GetSuperClass() )
			{
				for (TArray<FImplementedInterface>::TIterator It(CurrentClass->Interfaces); It; ++It)
				{
					// See if this is the implementation we are looking for, and it was done natively, not in K2
					FImplementedInterface& ImplInterface = *It;
					if ( !ImplInterface.bImplementedByK2 && ImplInterface.Class->IsChildOf(InterfaceClass) )
					{
						Result = (uint8*)this + It->PointerOffset;
						break;
					}
				}
			}
		}
	}

	return Result;
}

void* UObjectBaseUtility::GetNativeInterfaceAddress(UClass* InterfaceClass)
{
	check(InterfaceClass != NULL);
	check(InterfaceClass->HasAllClassFlags(CLASS_Interface | CLASS_Native));
	check(InterfaceClass != UInterface::StaticClass());

	for( UClass* CurrentClass=GetClass(); CurrentClass; CurrentClass = CurrentClass->GetSuperClass() )
	{
		for (auto It = CurrentClass->Interfaces.CreateConstIterator(); It; ++It)
		{
			// See if this is the implementation we are looking for, and it was done natively, not in K2
			auto& ImplInterface = *It;
			if ( !ImplInterface.bImplementedByK2 && ImplInterface.Class->IsChildOf(InterfaceClass) )
			{
				if ( It->PointerOffset )
				{
					return (uint8*)this + It->PointerOffset;
				}
			}
		}
	}

	return NULL;
}

bool UObjectBaseUtility::IsDefaultSubobject() const
{
	const bool bIsInstanced = GetOuter() && (GetOuter()->HasAnyFlags(RF_ClassDefaultObject) || ((UObject*)this)->GetArchetype() != GetClass()->GetDefaultObject(false));
	return bIsInstanced;
}

#if STATS && USE_MALLOC_PROFILER
void FScopeCycleCounterUObject::TrackObjectForMallocProfiling(const UObjectBaseUtility *InObject)
{
	// Get the package name from the outermost item (if available - can't use GetOutermost here)
	FName PackageName;
	if (InObject->GetOuter())
	{
		UObjectBaseUtility* Top = InObject->GetOuter();
		for (;;)
		{
			UObjectBaseUtility* CurrentOuter = Top->GetOuter();
			if (!CurrentOuter)
			{
				PackageName = Top->GetFName();
				break;
			}
			Top = CurrentOuter;
		}
	}

	// Get the class name (if available)
	FName ClassName;
	if (InObject->GetClass())
	{
		ClassName = InObject->GetClass()->GetFName();
	}

	TrackObjectForMallocProfiling(PackageName, ClassName, InObject->GetFName());
}
void FScopeCycleCounterUObject::TrackObjectForMallocProfiling(const FName InPackageName, const FName InClassName, const FName InObjectName)
{
	static const TCHAR PackageTagCategory[] = TEXT("Package:");
	static const TCHAR ObjectTagCategory[] = TEXT("Object:");
	static const TCHAR ClassTagCategory[] = TEXT("Class:");

	// We use an array rather than an FString to try and minimize heap allocations
	TArray<TCHAR, TInlineAllocator<256>> ScratchSpaceBuffer;

	auto AppendNameToBuffer = [&](const FName InName)
	{
		const FNameEntry* NameEntry = InName.GetDisplayNameEntry();
		if (NameEntry->IsWide())
		{
			const WIDECHAR* NameCharPtr = NameEntry->GetWideName();
			while (*NameCharPtr != 0)
			{
				ScratchSpaceBuffer.Add((TCHAR)*NameCharPtr++);
			}
		}
		else
		{
			const ANSICHAR* NameCharPtr = NameEntry->GetAnsiName();
			while (*NameCharPtr != 0)
			{
				ScratchSpaceBuffer.Add((TCHAR)*NameCharPtr++);
			}
		}
	};

	if (!InPackageName.IsNone())
	{
		// "Package:/Path/To/Package"
		ScratchSpaceBuffer.Reset();
		ScratchSpaceBuffer.Append(PackageTagCategory, ARRAY_COUNT(PackageTagCategory) - 1);
		AppendNameToBuffer(InPackageName);
		ScratchSpaceBuffer.Add(0);
		PackageTag = FName(ScratchSpaceBuffer.GetData());
		GMallocProfiler->AddTag(PackageTag);

		// "Object:/Path/To/Package/ObjectName"
		ScratchSpaceBuffer.Reset();
		ScratchSpaceBuffer.Append(ObjectTagCategory, ARRAY_COUNT(ObjectTagCategory) - 1);
		AppendNameToBuffer(InPackageName);
		ScratchSpaceBuffer.Add(TEXT('/'));
		AppendNameToBuffer(InObjectName);
		ScratchSpaceBuffer.Add(0);
		ObjectTag = FName(ScratchSpaceBuffer.GetData());
		GMallocProfiler->AddTag(ObjectTag);
	}

	if (!InClassName.IsNone())
	{
		// "Class:ClassName"
		ScratchSpaceBuffer.Reset();
		ScratchSpaceBuffer.Append(ClassTagCategory, ARRAY_COUNT(ClassTagCategory) - 1);
		AppendNameToBuffer(InClassName);
		ScratchSpaceBuffer.Add(0);
		ClassTag = FName(ScratchSpaceBuffer.GetData());
		GMallocProfiler->AddTag(ClassTag);
	}
}
void FScopeCycleCounterUObject::UntrackObjectForMallocProfiling()
{
	if (!PackageTag.IsNone())
	{
		GMallocProfiler->RemoveTag(PackageTag);
	}

	if (!ClassTag.IsNone())
	{
		GMallocProfiler->RemoveTag(ClassTag);
	}

	if (!ObjectTag.IsNone())
	{
		GMallocProfiler->RemoveTag(ObjectTag);
	}
}
#endif
