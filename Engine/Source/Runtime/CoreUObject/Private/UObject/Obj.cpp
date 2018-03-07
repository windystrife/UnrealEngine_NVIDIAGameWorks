// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObj.cpp: Unreal object manager.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "Stats/Stats.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectHash.h"
#include "UObject/Object.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "Templates/Casts.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UObjectAnnotation.h"
#include "UObject/ReferenceChainSearch.h"
#include "Serialization/ArchiveCountMem.h"
#include "Serialization/ArchiveShowReferences.h"
#include "Serialization/ArchiveFindCulprit.h"
#include "Serialization/ArchiveTraceRoute.h"
#include "Misc/PackageName.h"
#include "Serialization/BulkData.h"
#include "UObject/LinkerLoad.h"
#include "Misc/RedirectCollector.h"


#include "Serialization/ArchiveDescribeReference.h"
#include "UObject/FindStronglyConnected.h"
#include "UObject/UObjectThreadContext.h"
#include "Misc/ExclusiveLoadPackageTimeTracker.h"
#include "Serialization/DeferredMessageLog.h"
#include "UObject/CoreRedirects.h"
#include "HAL/LowLevelMemTracker.h"

DEFINE_LOG_CATEGORY(LogObj);

/** Stat group for dynamic objects cycle counters*/


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Object manager internal variables.
/** Transient package.													*/
static UPackage*			GObjTransientPkg								= NULL;		

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Used to verify that the Super::BeginDestroyed chain is intact.			*/
	static TArray<UObject*,TInlineAllocator<16> >		DebugBeginDestroyed;
	/** Used to verify that the Super::FinishDestroyed chain is intact.			*/
	static TArray<UObject*,TInlineAllocator<16> >		DebugFinishDestroyed;
#endif

#if !UE_BUILD_SHIPPING
	/** Used for the "obj mark" and "obj markcheck" commands only			*/
	static FUObjectAnnotationSparseBool DebugMarkAnnotation;
	/** Used for the "obj invmark" and "obj invmarkcheck" commands only			*/
	static TArray<TWeakObjectPtr<UObject> >	DebugInvMarkWeakPtrs;
	static TArray<FString>			DebugInvMarkNames;
#endif

UObject::UObject( EStaticConstructor, EObjectFlags InFlags )
: UObjectBaseUtility(InFlags | (!(InFlags & RF_Dynamic) ? (RF_MarkAsNative | RF_MarkAsRootSet) : RF_NoFlags))
{
	EnsureNotRetrievingVTablePtr();
}

UObject::UObject(FVTableHelper& Helper)
{
	EnsureRetrievingVTablePtrDuringCtor(TEXT("UObject(FVTableHelper& Helper)"));
}

void UObject::EnsureNotRetrievingVTablePtr() const
{
	UE_CLOG(GIsRetrievingVTablePtr, LogCore, Fatal, TEXT("We are currently retrieving VTable ptr. Please use FVTableHelper constructor instead."));
}

UObject* UObject::CreateDefaultSubobject(FName SubobjectFName, UClass* ReturnType, UClass* ClassToCreateByDefault, bool bIsRequired, bool bAbstract, bool bIsTransient)
{
	FObjectInitializer* CurrentInitializer = FUObjectThreadContext::Get().TopInitializer();
	UE_CLOG(!CurrentInitializer, LogObj, Fatal, TEXT("No object initializer found during construction."));
	UE_CLOG(CurrentInitializer->Obj != this, LogObj, Fatal, TEXT("Using incorrect object initializer."));
	return CurrentInitializer->CreateDefaultSubobject(this, SubobjectFName, ReturnType, ClassToCreateByDefault, bIsRequired, bAbstract, bIsTransient);
}

UObject* UObject::CreateEditorOnlyDefaultSubobjectImpl(FName SubobjectName, UClass* ReturnType, bool bTransient)
{
	FObjectInitializer* CurrentInitializer = FUObjectThreadContext::Get().TopInitializer();
	return CurrentInitializer->CreateEditorOnlyDefaultSubobject(this, SubobjectName, ReturnType, bTransient);
}

void UObject::GetDefaultSubobjects(TArray<UObject*>& OutDefaultSubobjects)
{
	OutDefaultSubobjects.Reset();
	ForEachObjectWithOuter(this, [&OutDefaultSubobjects](UObject* Object)
	{
		if (Object->IsDefaultSubobject())
		{
			OutDefaultSubobjects.Add(Object);
		}
	}, false);
}

UObject* UObject::GetDefaultSubobjectByName(FName ToFind)
{
	UObject* Object = nullptr;
	// If it is safe use the faster StaticFindObjectFast rather than searching all the subobjects
	if (!GIsSavingPackage && !IsGarbageCollecting())
	{
		Object = StaticFindObjectFast(UObject::StaticClass(), this, ToFind);
		if (Object && !Object->IsDefaultSubobject())
		{
			Object = nullptr;
		}
	}
	else
	{
		TArray<UObject*> SubObjects;
		GetDefaultSubobjects(SubObjects);
		for (UObject* SubObject : SubObjects)
		{
			if (SubObject->GetFName() == ToFind)
			{
				Object = SubObject;
				break;
			}
		}
	}
	return Object;
}

bool UObject::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
#if WITH_EDITOR
	// This guarantees that if this UObject is actually renamed and changes packages
	// the metadata will be moved with it.
	FMetaDataUtilities::FMoveMetadataHelperContext MoveMetaData(this, true);
#endif //WITH_EDITOR

	// Check that we are not renaming a within object into an Outer of the wrong type, unless we're renaming the CDO of a Blueprint.
	if( NewOuter && !NewOuter->IsA(GetClass()->ClassWithin) && !HasAnyFlags(RF_ClassDefaultObject))	
	{
		UE_LOG(LogObj, Fatal, TEXT("Cannot rename %s into Outer %s as it is not of type %s"), 
			*GetFullName(), 
			*NewOuter->GetFullName(), 
			*GetClass()->ClassWithin->GetName() );
	}

	UObject* NameScopeOuter = (Flags & REN_ForceGlobalUnique) ? ANY_PACKAGE : NewOuter;

	// find an object with the same name and same class in the new outer
	bool bIsCaseOnlyChange = false;
	if (InName)
	{
		UObject* ExistingObject = StaticFindObject(/*Class=*/ NULL, NameScopeOuter ? NameScopeOuter : GetOuter(), InName, true);
		if (ExistingObject == this)
		{
			if (ExistingObject->GetName().Equals(InName, ESearchCase::CaseSensitive))
			{
				// The name is exactly the same - there's nothing to change
				return true;
			}
			else
			{
				// This rename has only changed the case, so we need to allow it to continue, but won't create a redirector (since the internal FName comparison ignores case)
				bIsCaseOnlyChange = true;
			}
		}
		else if (ExistingObject)
		{
			if (Flags & REN_Test)
			{
				return false;
			}
			else
			{
				UE_LOG(LogObj, Fatal,TEXT("Renaming an object (%s) on top of an existing object (%s) is not allowed"), *GetFullName(), *ExistingObject->GetFullName());
			}
		}
	}

	// if we are just testing, and there was no conflict, then return a success
	if (Flags & REN_Test)
	{
		return true;
	}

	if (!(Flags & REN_ForceNoResetLoaders))
	{
		ResetLoaders( GetOuter() );
	}
	FName OldName = GetFName();

	FName NewName;

	if (InName == nullptr)
	{
		// If null, null is passed in, then we are deliberately trying to get a new name
		// Otherwise if the outer is changing, try and maintain the name
		if (NewOuter && StaticFindObjectFastInternal(nullptr, NewOuter, OldName) == nullptr)
		{
			NewName = OldName;
		}
		else
		{
			NewName = MakeUniqueObjectName( NameScopeOuter ? NameScopeOuter : GetOuter(), GetClass() );
		}
	}
	else
	{
		NewName = FName(InName);
	}

	//UE_LOG(LogObj, Log,  TEXT("Renaming %s to %s"), *OldName.ToString(), *NewName.ToString() );

	if ( !(Flags & REN_NonTransactional) )
	{
		// Mark touched packages as dirty.
		if (Flags & REN_DoNotDirty)
		{
			// This will only mark dirty if in a transaction,
			// the object is transactional, and the object is
			// not in a PlayInEditor package.
			Modify(false);
		}
		else
		{
			// This will maintain previous behavior...
			// Which was to directly call MarkPackageDirty
			Modify(true);
		}
	}

	bool bCreateRedirector = false;
	UObject* OldOuter = GetOuter();

	if ( HasAnyFlags(RF_Public) )
	{
		const bool bUniquePathChanged	= ((NewOuter != NULL && OldOuter != NewOuter) || (OldName != NewName));
		const bool bRootPackage			= GetClass() == UPackage::StaticClass() && OldOuter == NULL;
		const bool bRedirectionAllowed = !FApp::IsGame() && ((Flags & REN_DontCreateRedirectors) == 0);

		// We need to create a redirector if we changed the Outer or Name of an object that can be referenced from other packages
		// [i.e. has the RF_Public flag] so that references to this object are not broken.
		bCreateRedirector = bRootPackage == false && bUniquePathChanged == true && bRedirectionAllowed == true && bIsCaseOnlyChange == false;
	}

	if( NewOuter )
	{
		if (!(Flags & REN_DoNotDirty))
		{
			NewOuter->MarkPackageDirty();
		}
	}

	LowLevelRename(NewName,NewOuter);

	// Create the redirector AFTER renaming the object. Two objects of different classes may not have the same fully qualified name.
	if (bCreateRedirector)
	{
		// Look for an existing redirector with the same name/class/outer in the old package.
		UObjectRedirector* Redirector = FindObject<UObjectRedirector>(OldOuter, *OldName.ToString(), /*bExactClass=*/ true);

		// If it does not exist, create it.
		if ( Redirector == NULL )
		{
			// create a UObjectRedirector with the same name as the old object we are redirecting
			Redirector = NewObject<UObjectRedirector>(OldOuter, OldName, RF_Standalone | RF_Public);
		}

		// point the redirector object to this object
		Redirector->DestinationObject = this;
	}

	PostRename(OldOuter, OldName);

	return true;
}


void UObject::PostLoad()
{
	// Note that it has propagated.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FUObjectThreadContext::Get().DebugPostLoad.RemoveSingle(this);
#endif

	/*
	By this point, all default properties have been loaded from disk
	for this object's class and all of its parent classes.  It is now
	safe to import config and localized data for "special" objects:
	- per-object config objects
	*/
	if( GetClass()->HasAnyClassFlags(CLASS_PerObjectConfig) )
	{
		LoadConfig();
	}
	CheckDefaultSubobjects();
}

#if WITH_EDITOR
void UObject::PreEditChange(UProperty* PropertyAboutToChange)
{
	Modify();
}


void UObject::PostEditChange(void)
{
	FPropertyChangedEvent EmptyPropertyUpdateStruct(NULL);
	this->PostEditChangeProperty(EmptyPropertyUpdateStruct);
}


void UObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(this, PropertyChangedEvent);
}


void UObject::PreEditChange( FEditPropertyChain& PropertyAboutToChange )
{
	const bool bIsEditingArchetypeProperty = HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && !FApp::IsGame();

	if (bIsEditingArchetypeProperty)
	{
		// this object must now be included in the undo/redo buffer (needs to be 
		// done prior to the following PreEditChange() call, in case it attempts 
		// to store this object in the undo/redo transaction buffer)
		SetFlags(RF_Transactional);
	}

	// forward the notification to the UProperty* version of PreEditChange
	PreEditChange(PropertyAboutToChange.GetActiveNode()->GetValue());

	FCoreUObjectDelegates::OnPreObjectPropertyChanged.Broadcast(this, PropertyAboutToChange);

	if (bIsEditingArchetypeProperty)
	{
		// Get a list of all objects which will be affected by this change; 
		TArray<UObject*> Objects;
		GetArchetypeInstances(Objects);
		PropagatePreEditChange(Objects, PropertyAboutToChange);
	}
}


void UObject::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	FPropertyChangedEvent PropertyEvent(PropertyChangedEvent.PropertyChain.GetActiveNode()->GetValue(), PropertyChangedEvent.ChangeType);

	// Set up array index per object map so that GetArrayIndex returns a valid result
	TArray<TMap<FString, int32>> ArrayIndexForProperty;
	if (PropertyChangedEvent.Property)
	{
		const FString PropertyName = PropertyChangedEvent.Property->GetName();
		const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(PropertyName);
		if (ArrayIndex != INDEX_NONE)
		{
			PropertyEvent.ObjectIteratorIndex = 0;
			ArrayIndexForProperty.AddDefaulted();
			ArrayIndexForProperty.Last().Add(PropertyName, ArrayIndex);
			PropertyEvent.SetArrayIndexPerObject(ArrayIndexForProperty);
		}
	}

	if( PropertyChangedEvent.PropertyChain.GetActiveMemberNode() )
	{
		PropertyEvent.SetActiveMemberProperty( PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue() );
	}

	// Propagate change to archetype instances first if necessary.
	if (!FApp::IsGame())
	{
		if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && PropertyChangedEvent.PropertyChain.GetActiveMemberNode() == PropertyChangedEvent.PropertyChain.GetHead())
	{
			// Get a list of all archetype instances
			TArray<UObject*> ArchetypeInstances;
			GetArchetypeInstances(ArchetypeInstances);

		// Propagate the editchange call to archetype instances
			PropagatePostEditChange(ArchetypeInstances, PropertyChangedEvent);
		}
		else if (GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			// Get a list of all outer's archetype instances
			TArray<UObject*> ArchetypeInstances;
			GetOuter()->GetArchetypeInstances(ArchetypeInstances);

			// Find UProperty describing this in Outer.
			for (UProperty* Property = GetOuter()->GetClass()->RefLink; Property != nullptr; Property = Property->NextRef)
			{
				if (this != *Property->ContainerPtrToValuePtr<UObject*>(GetOuter()))
				{
					continue;
				}

				// Since we found property, propagate PostEditChange to all relevant components of archetype instances.
				TArray<UObject*> ArchetypeComponentInstances;
				for (UObject* ArchetypeInstance : ArchetypeInstances)
				{
					if (UObject* ComponentInstance = *Property->ContainerPtrToValuePtr<UObject*>(ArchetypeInstance))
					{
						ArchetypeComponentInstances.Add(ComponentInstance);
					}
				}

				GetOuter()->PropagatePostEditChange(ArchetypeComponentInstances, PropertyChangedEvent);

				break;
			}
		}
	}

	PostEditChangeProperty(PropertyEvent);
}

bool UObject::CanEditChange( const UProperty* InProperty ) const
{
	const bool bIsMutable = !InProperty->HasAnyPropertyFlags( CPF_EditConst );
	return bIsMutable;
}

void UObject::PropagatePreEditChange( TArray<UObject*>& AffectedObjects, FEditPropertyChain& PropertyAboutToChange )
{
	TArray<UObject*> Instances;

	for ( int32 i = 0; i < AffectedObjects.Num(); i++ )
	{
		UObject* Obj = AffectedObjects[i];

		// in order to ensure that all objects are saved properly, only process the objects which have this object as their
		// ObjectArchetype since we are going to call Pre/PostEditChange on each object (which could potentially affect which data is serialized
		if ( Obj->GetArchetype() == this )
		{
			// add this object to the list that we're going to process
			Instances.Add(Obj);

			// remove this object from the input list so that when we pass the list to our instances they don't need to check those objects again.
			AffectedObjects.RemoveAt(i--);
		}
	}

	for ( int32 i = 0; i < Instances.Num(); i++ )
	{
		UObject* Obj = Instances[i];

		// this object must now be included in any undo/redo operations
		Obj->SetFlags(RF_Transactional);

		// This will call ClearComponents in the Actor case, so that we do not serialize more stuff than we need to.
		Obj->PreEditChange(PropertyAboutToChange);

		// now recurse into this object, saving its instances
		Obj->PropagatePreEditChange(AffectedObjects, PropertyAboutToChange);
	}
}

void UObject::PropagatePostEditChange( TArray<UObject*>& AffectedObjects, FPropertyChangedChainEvent& PropertyChangedEvent )
{
	TArray<UObject*> Instances;

	for ( int32 i = 0; i < AffectedObjects.Num(); i++ )
	{
		UObject* Obj = AffectedObjects[i];

		// in order to ensure that all objects are re-initialized properly, only process the objects which have this object as their
		// ObjectArchetype
		if ( Obj->GetArchetype() == this || Obj->GetOuter()->GetArchetype() == this )
		{
			// add this object to the list that we're going to process
			Instances.Add(Obj);

			// remove this object from the input list so that when we pass the list to our instances they don't need to check those objects again.
			AffectedObjects.RemoveAt(i--);
		}
	}

	check(PropertyChangedEvent.PropertyChain.GetActiveMemberNode() != nullptr);

	for ( int32 i = 0; i < Instances.Num(); i++ )
	{
		UObject* Obj = Instances[i];

		// notify the object that all changes are complete
		Obj->PostEditChangeChainProperty(PropertyChangedEvent);

		// now recurse into this object, loading its instances
		Obj->PropagatePostEditChange(AffectedObjects, PropertyChangedEvent);
	}
}

void UObject::PreEditUndo()
{
	PreEditChange(NULL);
}

void UObject::PostEditUndo()
{
	if( !IsPendingKill() )
	{
		PostEditChange();
	}
}

void UObject::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
	UObject::PostEditUndo();
}


bool UObject::IsSelectedInEditor() const
{
	return !IsPendingKill() && GSelectedObjectAnnotation.Get(this);
}

#endif // WITH_EDITOR


/**
	Helper class for tracking the list of classes excluded on a certain target system (client/server)
*/
struct FClassExclusionData
{
	TSet<FName> ExcludedClassNames;
	TSet<FName> ExcludedPackageShortNames;
	TSet<FName> CachedExcludeList;
	TSet<FName> CachedIncludeList;

	bool IsExcluded(UClass* InClass)
	{
		FName OriginalClassName = InClass->GetFName();

		if (CachedExcludeList.Contains(OriginalClassName))
		{
			return true;
		}

		if (CachedIncludeList.Contains(OriginalClassName))
		{
			return false;
		}

		auto ModuleShortNameFromClass = [](const UClass* Class) -> FName
		{
			return FName(*FPackageName::GetShortName(Class->GetOutermost()));
		};

		while (InClass != nullptr)
		{
			if(ExcludedPackageShortNames.Num() && ExcludedPackageShortNames.Contains(ModuleShortNameFromClass(InClass)))
			{
				UE_LOG(LogObj, Display, TEXT("Class %s is excluded because its module is excluded in the current platform"), *OriginalClassName.ToString());
				CachedExcludeList.Add(OriginalClassName);
				return true;
			}

			if (ExcludedClassNames.Contains(InClass->GetFName()))
			{
				CachedExcludeList.Add(OriginalClassName);
				return true;
			}

			InClass = InClass->GetSuperClass();
		}

		CachedIncludeList.Add(OriginalClassName);
		return false;
	}

	void UpdateExclusionList(const TArray<FString>& InClassNames, const TArray<FString>& InPackageShortNames)
	{
		ExcludedClassNames.Empty(InClassNames.Num());
		ExcludedPackageShortNames.Empty(InPackageShortNames.Num());
		CachedIncludeList.Empty();
		CachedExcludeList.Empty();

		for (const FString& ClassName : InClassNames)
		{
			ExcludedClassNames.Add(FName(*ClassName));
		}

		for (const FString& PkgName : InPackageShortNames)
		{
			ExcludedPackageShortNames.Add(FName(*PkgName));
		}
	}
};

FClassExclusionData GDedicatedServerExclusionList;
FClassExclusionData GDedicatedClientExclusionList;

bool UObject::NeedsLoadForServer() const
{
	return !GDedicatedServerExclusionList.IsExcluded(GetClass());
}

void UObject::UpdateClassesExcludedFromDedicatedServer(const TArray<FString>& InClassNames, const TArray<FString>& InModulesNames)
{
	GDedicatedServerExclusionList.UpdateExclusionList(InClassNames, InModulesNames);
}

bool UObject::NeedsLoadForClient() const
{
	return !GDedicatedClientExclusionList.IsExcluded(GetClass());
}

void UObject::UpdateClassesExcludedFromDedicatedClient(const TArray<FString>& InClassNames, const TArray<FString>& InModulesNames)
{
	GDedicatedClientExclusionList.UpdateExclusionList(InClassNames, InModulesNames);
}

bool UObject::CanCreateInCurrentContext(UObject* Template)
{
	check(Template);

	// Ded. server
	if (IsRunningDedicatedServer())
	{
		return Template->NeedsLoadForServer();
	}
	// Client only
	if (IsRunningClientOnly())
	{
		return Template->NeedsLoadForClient();
	}
	// Game, listen server etc.
	if (IsRunningGame())
	{
		return Template->NeedsLoadForClient() || Template->NeedsLoadForServer();
	}

	// other cases (e.g. editor)
	return true;
}


void UObject::GetArchetypeInstances( TArray<UObject*>& Instances )
{
	Instances.Empty();

	if ( HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject) )
	{
		// we need to evaluate CDOs as well, but nothing pending kill
		TArray<UObject*> IterObjects;
		{
			const bool bIncludeNestedObjects = true;
			GetObjectsOfClass(GetClass(), IterObjects, bIncludeNestedObjects, RF_NoFlags, EInternalObjectFlags::PendingKill);
		}

		// if this object is the class default object, any object of the same class (or derived classes) could potentially be affected
		if ( !HasAnyFlags(RF_ArchetypeObject) )
		{
			Instances.Reserve(IterObjects.Num()-1);
			for (UObject* It : IterObjects)
			{
				UObject* Obj = It;
				if ( Obj != this )
				{
					Instances.Add(Obj);
				}
			}
		}
		else
		{
			for (UObject* It : IterObjects)
			{
				UObject* Obj = It;
				
				// if this object is the correct type and its archetype is this object, add it to the list
				if ( Obj && Obj != this && Obj->IsBasedOnArchetype(this) )
				{
					Instances.Add(Obj);
				}
			}
		}
	}
}

void UObject::BeginDestroy()
{
	// Sanity assertion to ensure ConditionalBeginDestroy is the only code calling us.
	if( !HasAnyFlags(RF_BeginDestroyed) )
	{
		UE_LOG(LogObj, Fatal,
			TEXT("Trying to call UObject::BeginDestroy from outside of UObject::ConditionalBeginDestroy on object %s. Please fix up the calling code."),
			*GetName()
			);
	}

	LowLevelRename(NAME_None);

	// Remove from linker's export table.
	SetLinker( NULL, INDEX_NONE );

	// ensure BeginDestroy has been routed back to UObject::BeginDestroy.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DebugBeginDestroyed.RemoveSingle(this);
#endif
}


void UObject::FinishDestroy()
{
	if( !HasAnyFlags(RF_FinishDestroyed) )
	{
		UE_LOG(LogObj, Fatal,
			TEXT("Trying to call UObject::FinishDestroy from outside of UObject::ConditionalFinishDestroy on object %s. Please fix up the calling code."),
			*GetName()
			);
	}

	check( !GetLinker() );
	check( GetLinkerIndex()	== INDEX_NONE );

	DestroyNonNativeProperties();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DebugFinishDestroyed.RemoveSingle(this);
#endif
}


FString UObject::GetDetailedInfo() const
{
	FString Result;  
	if( this != nullptr )
	{
		Result = GetDetailedInfoInternal();
	}
	else
	{
		Result = TEXT("None");
	}
	return Result;  
}

#if WITH_ENGINE

#if DO_CHECK
// Used to check to see if a derived class actually implemented GetWorld() or not, but only on the game thread
bool bGetWorldOverridden = false;
#endif

class UWorld* UObject::GetWorld() const
{
	if (UObject* Outer = GetOuter())
	{
		return Outer->GetWorld();
	}

#if DO_CHECK
	if (IsInGameThread())
	{
		bGetWorldOverridden = false;
	}
#endif
	return nullptr;
}

class UWorld* UObject::GetWorldChecked(bool& bSupported) const
{
#if DO_CHECK
	const bool bGameThread = IsInGameThread();
	if (bGameThread)
	{
		bGetWorldOverridden = true;
	}
#endif

	UWorld* World = GetWorld();

#if DO_CHECK
	if (bGameThread && !bGetWorldOverridden)
	{
		static TSet<UClass*> ReportedClasses;

		UClass* UnsupportedClass = GetClass();
		if (!ReportedClasses.Contains(UnsupportedClass))
		{
			UClass* SuperClass = UnsupportedClass->GetSuperClass();
			FString ParentHierarchy = (SuperClass ? SuperClass->GetName() : TEXT(""));
			while (SuperClass->GetSuperClass())
			{
				SuperClass = SuperClass->GetSuperClass();
				ParentHierarchy += FString::Printf(TEXT(", %s"), *SuperClass->GetName());
			}

			ensureAlwaysMsgf(false, TEXT("Unsupported context object of class %s (SuperClass(es) - %s). You must add a way to retrieve a UWorld context for this class."), *UnsupportedClass->GetName(), *ParentHierarchy);

			ReportedClasses.Add(UnsupportedClass);
		}
	}

	bSupported = bGameThread ? bGetWorldOverridden : (World != nullptr);
	check(World && bSupported);
#else
	bSupported = World != nullptr;
#endif

	return World;
}

bool UObject::ImplementsGetWorld() const
{
#if DO_CHECK
	check(IsInGameThread());
	bGetWorldOverridden = true;
	GetWorld();
	return bGetWorldOverridden;
#else
	return true;
#endif	
}

#endif

#define PROFILE_ConditionalBeginDestroy (0)

#if PROFILE_ConditionalBeginDestroy

struct FTimeCnt
{
	float TotalTime;
	int32 Count;

	FTimeCnt()
		: TotalTime(0.0f)
		, Count(0)
	{
	}

	bool operator<(const FTimeCnt& Other) const 
	{
		return TotalTime > Other.TotalTime;
	}
};

static TMap<FName, FTimeCnt> MyProfile;
#endif

bool UObject::ConditionalBeginDestroy()
{
	check(IsValidLowLevel());
	if( !HasAnyFlags(RF_BeginDestroyed) )
	{
		SetFlags(RF_BeginDestroyed);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		checkSlow(!DebugBeginDestroyed.Contains(this));
		DebugBeginDestroyed.Add(this);
#endif

#if PROFILE_ConditionalBeginDestroy
		double StartTime = FPlatformTime::Seconds();
#endif

		BeginDestroy();

#if PROFILE_ConditionalBeginDestroy
		float ThisTime = float(FPlatformTime::Seconds() - StartTime);

		FTimeCnt& TimeCnt = MyProfile.FindOrAdd(GetClass()->GetFName());
		TimeCnt.Count++;
		TimeCnt.TotalTime += ThisTime;

		static float TotalTime = 0.0f;
		static int32 TotalCnt = 0;

		TotalTime += ThisTime;
		if ((++TotalCnt) % 1000 == 0)
		{
			UE_LOG(LogObj, Log, TEXT("ConditionalBeginDestroy %d cnt %fus"), TotalCnt, 1000.0f * 1000.0f * TotalTime / float(TotalCnt));

			MyProfile.ValueSort(TLess<FTimeCnt>());

			int32 NumPrint = 0;
			for (auto& Item : MyProfile)
			{
				UE_LOG(LogObj, Log, TEXT("    %6d cnt %6.2fus per   %6.2fms total  %s"), Item.Value.Count, 1000.0f * 1000.0f * Item.Value.TotalTime / float(Item.Value.Count), 1000.0f * Item.Value.TotalTime, *Item.Key.ToString());
				if (NumPrint++ > 30)
				{
					break;
				}
			}
		}
#endif


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if( DebugBeginDestroyed.Contains(this) )
		{
			// class might override BeginDestroy without calling Super::BeginDestroy();
			UE_LOG(LogObj, Fatal, TEXT("%s failed to route BeginDestroy"), *GetFullName() );
		}
#endif
		return true;
	}
	else 
	{
		return false;
	}
}

bool UObject::ConditionalFinishDestroy()
{
	check(IsValidLowLevel());
	if( !HasAnyFlags(RF_FinishDestroyed) )
	{
		SetFlags(RF_FinishDestroyed);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		checkSlow(!DebugFinishDestroyed.Contains(this));
		DebugFinishDestroyed.Add(this);
#endif
		FinishDestroy();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if( DebugFinishDestroyed.Contains(this) )
		{
			UE_LOG(LogObj, Fatal, TEXT("%s failed to route FinishDestroy"), *GetFullName() );
		}
#endif
		return true;
	}
	else 
	{
		return false;
	}
}

void UObject::ConditionalPostLoad()
{
	check(!GEventDrivenLoaderEnabled || !HasAnyFlags(RF_NeedLoad)); //@todoio Added this as "nicks rule"
									  // PostLoad only if the object needs it and has already been serialized
	//@todoio note this logic should be unchanged compared to main
	if (HasAnyFlags(RF_NeedPostLoad))
	{

		check(IsInGameThread() || HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject) || IsPostLoadThreadSafe() || IsA(UClass::StaticClass()))

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		checkSlow(!ThreadContext.DebugPostLoad.Contains(this));
		ThreadContext.DebugPostLoad.Add(this);
#endif
		ClearFlags( RF_NeedPostLoad );

		UObject* ObjectArchetype = GetArchetype();
		if ( ObjectArchetype != NULL )
		{
			//make sure our archetype executes ConditionalPostLoad first.
			ObjectArchetype->ConditionalPostLoad();
		}

		ConditionalPostLoadSubobjects();

		{
			FExclusiveLoadPackageTimeTracker::FScopedPostLoadTracker Tracker(this);

			if (HasAnyFlags(RF_ClassDefaultObject))
			{
				GetClass()->PostLoadDefaultObject(this);
			}
			else
			{
				LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(GetOutermost(), ELLMTagSet::Assets);
				LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(GetClass()->IsChildOf(UDynamicClass::StaticClass()) ? UDynamicClass::StaticClass() : GetClass(), ELLMTagSet::AssetClasses);

				PostLoad();

				LLM_PUSH_STATS_FOR_ASSET_TAGS();
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (ThreadContext.DebugPostLoad.Contains(this))
		{
			UE_LOG(LogObj, Fatal, TEXT("%s failed to route PostLoad.  Please call Super::PostLoad() in your <className>::PostLoad() function."), *GetFullName());
		}
#endif
	}
}

void UObject::PostLoadSubobjects( FObjectInstancingGraph* OuterInstanceGraph/*=NULL*/ )
{
	// if this class contains instanced object properties and a new object property has been added since this object was saved,
	// this object won't receive its own unique instance of the object assigned to the new property, since we don't instance object during loading
	// so go over all instanced object properties and look for cases where the value for that property still matches the default value.

	check(!GEventDrivenLoaderEnabled || !HasAnyFlags(RF_NeedLoad));

	if( GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference) )
	{
		UObject* ObjOuter = GetOuter();
		// make sure our Outer has already called ConditionalPostLoadSubobjects
		if (ObjOuter != NULL && ObjOuter->HasAnyFlags(RF_NeedPostLoadSubobjects) )
		{
			check(!GEventDrivenLoaderEnabled || !ObjOuter->HasAnyFlags(RF_NeedLoad));

			if (ObjOuter->HasAnyFlags(RF_NeedPostLoad) )
			{
				ObjOuter->ConditionalPostLoad();
			}
			else
			{
				ObjOuter->ConditionalPostLoadSubobjects();
			}
			if ( !HasAnyFlags(RF_NeedPostLoadSubobjects) )
			{
				// if calling ConditionalPostLoadSubobjects on our Outer resulted in ConditionalPostLoadSubobjects on this object, stop here
				return;
			}
		}

		// clear the flag so that we don't re-enter this method
		ClearFlags(RF_NeedPostLoadSubobjects);

		FObjectInstancingGraph CurrentInstanceGraph;

		FObjectInstancingGraph* InstanceGraph = OuterInstanceGraph;
		if ( InstanceGraph == NULL )
		{
			CurrentInstanceGraph.SetDestinationRoot(this);
			CurrentInstanceGraph.SetLoadingObject(true);

			// if we weren't passed an instance graph to use, create a new one and use that
			InstanceGraph = &CurrentInstanceGraph;
		}

		// this will be filled with the list of component instances which were serialized from disk
		TArray<UObject*> SerializedComponents;
		// fill the array with the component contained by this object that were actually serialized to disk through property references
		CollectDefaultSubobjects(SerializedComponents, false);

		// now, add all of the instanced components to the instance graph that will be used for instancing any components that have been added
		// to this object's archetype since this object was last saved
		for ( int32 ComponentIndex = 0; ComponentIndex < SerializedComponents.Num(); ComponentIndex++ )
		{
			UObject* PreviouslyInstancedComponent = SerializedComponents[ComponentIndex];
			InstanceGraph->AddNewInstance(PreviouslyInstancedComponent);
		}

		InstanceSubobjectTemplates(InstanceGraph);
	}
	else
	{
		// clear the flag so that we don't re-enter this method
		ClearFlags(RF_NeedPostLoadSubobjects);
	}
}


void UObject::ConditionalPostLoadSubobjects( FObjectInstancingGraph* OuterInstanceGraph/*=NULL*/ )
{
	if ( HasAnyFlags(RF_NeedPostLoadSubobjects) )
	{
		PostLoadSubobjects(OuterInstanceGraph);
	}
	CheckDefaultSubobjects();
}

void UObject::PreSave(const class ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectSaved.Broadcast(this);
#endif
}

bool UObject::CanModify() const
{
	return (!HasAnyFlags(RF_NeedInitialization) && !IsGarbageCollecting() && !GExitPurge && !IsUnreachable());
}

bool UObject::Modify( bool bAlwaysMarkDirty/*=true*/ )
{
	bool bSavedToTransactionBuffer = false;

	if (CanModify())
	{
		// Do not consider PIE world objects or script packages, as they should never end up in the
		// transaction buffer and we don't want to mark them dirty here either.
		if (GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor | PKG_ContainsScript | PKG_CompiledIn) == false || GetClass()->HasAnyClassFlags(CLASS_DefaultConfig | CLASS_Config))
		{
			// Attempt to mark the package dirty and save a copy of the object to the transaction
			// buffer. The save will fail if there isn't a valid transactor, the object isn't
			// transactional, etc.
			bSavedToTransactionBuffer = SaveToTransactionBuffer(this, bAlwaysMarkDirty);

			// If we failed to save to the transaction buffer, but the user requested the package
			// marked dirty anyway, do so
			if (!bSavedToTransactionBuffer && bAlwaysMarkDirty)
			{
				MarkPackageDirty();
			}
		}
#if WITH_EDITOR
		FCoreUObjectDelegates::BroadcastOnObjectModified(this);
#endif
	}

	return bSavedToTransactionBuffer;
}

bool UObject::IsSelected() const
{
#if WITH_EDITOR
	return IsSelectedInEditor();
#else
	return false;
#endif
}

void UObject::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	UClass *ObjClass = GetClass();
	if (!ObjClass->HasAnyClassFlags(CLASS_Intrinsic))
	{
		OutDeps.Add(ObjClass);

		if (!HasAnyFlags(RF_ClassDefaultObject) && ObjClass->GetDefaultsCount() > 0)
		{
			OutDeps.Add(ObjClass->GetDefaultObject());
		}
	}
}

void UObject::Serialize( FArchive& Ar )
{
	// These three items are very special items from a serialization standpoint. They aren't actually serialized.
	UClass *ObjClass = GetClass();
	UObject* LoadOuter = GetOuter();
	FName LoadName = GetFName();

	// Make sure this object's class's data is loaded.
	if(ObjClass->HasAnyFlags(RF_NeedLoad) )
	{
		Ar.Preload(ObjClass);

		// make sure this object's template data is loaded - the only objects
		// this should actually affect are those that don't have any defaults
		// to serialize.  for objects with defaults that actually require loading
		// the class default object should be serialized in FLinkerLoad::Preload, before
		// we've hit this code.
		if ( !HasAnyFlags(RF_ClassDefaultObject) && ObjClass->GetDefaultsCount() > 0 )
		{
			Ar.Preload(ObjClass->GetDefaultObject());
		}
	}

	// Special info.
	if( (!Ar.IsLoading() && !Ar.IsSaving() && !Ar.IsObjectReferenceCollector()) )
	{
		Ar << LoadName;
		if(!Ar.IsIgnoringOuterRef())
		{
			Ar << LoadOuter;
		}
		if ( !Ar.IsIgnoringClassRef() )
		{
			Ar << ObjClass;
		}
	}
	// Special support for supporting undo/redo of renaming and changing Archetype.
	else if( Ar.IsTransacting() )
	{
		if(!Ar.IsIgnoringOuterRef())
		{
			if(Ar.IsLoading())
			{
				Ar << LoadName << LoadOuter;

				// If the name we loaded is different from the current one,
				// unhash the object, change the name and hash it again.
				bool bDifferentName = GetFName() != NAME_None && LoadName != GetFName();
				bool bDifferentOuter = LoadOuter != GetOuter();
				if ( bDifferentName == true || bDifferentOuter == true )
				{
					LowLevelRename(LoadName,LoadOuter);
				}
			}
			else
			{
				Ar << LoadName << LoadOuter;
			}
		}
	}

	// Serialize object properties which are defined in the class.
	// Handle derived UClass objects (exact UClass objects are native only and shouldn't be touched)
	if (ObjClass != UClass::StaticClass())
	{
		SerializeScriptProperties(Ar);
	}

	// Keep track of pending kill
	if( Ar.IsTransacting() )
	{
		bool WasKill = IsPendingKill();
		if( Ar.IsLoading() )
		{
			Ar << WasKill;
			if (WasKill)
			{
				MarkPendingKill();
			}
			else
			{
				ClearPendingKill();
			}
		}
		else if( Ar.IsSaving() )
		{
			Ar << WasKill;
		}
	}

	// Serialize a GUID if this object has one mapped to it
	FLazyObjectPtr::PossiblySerializeObjectGuid(this, Ar);

	// Invalidate asset pointer caches when loading a new object
	if (Ar.IsLoading() )
	{
		FSoftObjectPath::InvalidateTag();
	}

	// Memory counting (with proper alignment to match C++)
	SIZE_T Size = GetClass()->GetStructureSize();
	Ar.CountBytes( Size, Size );
}



void UObject::SerializeScriptProperties( FArchive& Ar ) const
{
	Ar.MarkScriptSerializationStart(this);
	if( HasAnyFlags(RF_ClassDefaultObject) )
	{
		Ar.StartSerializingDefaults();
	}

	UClass *ObjClass = GetClass();

	if( (Ar.IsLoading() || Ar.IsSaving()) && !Ar.WantBinaryPropertySerialization() )
	{
		//@todoio GetArchetype is pathological for blueprint classes and the event driven loader; the EDL already knows what the archetype is; just calling this->GetArchetype() tries to load some other stuff.
		UObject* DiffObject = Ar.GetArchetypeFromLoader(this);
		if (!DiffObject)
		{
			DiffObject = GetArchetype();
		}
#if WITH_EDITOR
		static const FBoolConfigValueHelper BreakSerializationRecursion(TEXT("StructSerialization"), TEXT("BreakSerializationRecursion"));
		const bool bBreakSerializationRecursion = BreakSerializationRecursion && Ar.IsLoading() && Ar.GetLinker();
#else 
		const bool bBreakSerializationRecursion = false;
#endif
#if WITH_EDITOR
		static const FName NAME_SerializeScriptProperties = FName(TEXT("SerializeScriptProperties"));
		FArchive::FScopeAddDebugData P(Ar, NAME_SerializeScriptProperties);
		FArchive::FScopeAddDebugData S(Ar, ObjClass->GetFName());
#endif
		ObjClass->SerializeTaggedProperties(Ar, (uint8*)this, HasAnyFlags(RF_ClassDefaultObject) ? ObjClass->GetSuperClass() : ObjClass, (uint8*)DiffObject, bBreakSerializationRecursion ? this : NULL);
	}
	else if ( Ar.GetPortFlags() != 0 && !Ar.ArUseCustomPropertyList )
	{
		//@todoio GetArchetype is pathological for blueprint classes and the event driven loader; the EDL already knows what the archetype is; just calling this->GetArchetype() tries to load some other stuff.
		UObject* DiffObject = Ar.GetArchetypeFromLoader(this);
		if (!DiffObject)
		{
			DiffObject = GetArchetype();
		}
		ObjClass->SerializeBinEx( Ar, const_cast<UObject *>(this), DiffObject, DiffObject ? DiffObject->GetClass() : NULL );
	}
	else
	{
		ObjClass->SerializeBin( Ar, const_cast<UObject *>(this) );
	}

	if( HasAnyFlags(RF_ClassDefaultObject) )
	{
		Ar.StopSerializingDefaults();
	}
	Ar.MarkScriptSerializationEnd(this);
}


void UObject::BuildSubobjectMapping(UObject* OtherObject, TMap<UObject*, UObject*>& ObjectMapping) const
{
	UPackage* ThisPackage = GetOutermost();
	UPackage* OtherPackage = OtherObject->GetOutermost();

	ForEachObjectWithOuter(this, [&](UObject* InSubObject)
	{
		if (ObjectMapping.Contains(InSubObject))
		{
			return;
		}

		FString NewSubObjectName = InSubObject->GetName();

		UClass* OtherSubObjectClass = InSubObject->GetClass();
		if (OtherSubObjectClass->ClassGeneratedBy && OtherSubObjectClass->ClassGeneratedBy->GetOutermost() == ThisPackage)
		{
			// This is a generated class type, so we actually need to use the new generated class type from the new package otherwise our type check will fail
			FString NewClassName = OtherSubObjectClass->GetPathName(ThisPackage);
			NewClassName = FString::Printf(TEXT("%s.%s"), *OtherPackage->GetName(), *NewClassName);

			OtherSubObjectClass = LoadObject<UClass>(OtherPackage, *NewClassName);
		}

		//UObject* OtherSubObject = StaticLoadObject(OtherSubObjectClass, OtherObject, *NewSubObjectName, nullptr, LOAD_Quiet | LOAD_NoRedirects, nullptr, true);
		UObject* OtherSubObject = StaticFindObjectFast(OtherSubObjectClass, OtherObject, *NewSubObjectName);
		ObjectMapping.Emplace(InSubObject, OtherSubObject);

		if (OtherSubObject)
		{
			InSubObject->BuildSubobjectMapping(OtherSubObject, ObjectMapping);
		}
	}, false, RF_NoFlags, EInternalObjectFlags::PendingKill);
}


void UObject::CollectDefaultSubobjects( TArray<UObject*>& OutSubobjectArray, bool bIncludeNestedSubobjects/*=false*/ )
{
	OutSubobjectArray.Empty();
	GetObjectsWithOuter(this, OutSubobjectArray, bIncludeNestedSubobjects);

	// Remove contained objects that are not subobjects.
	for ( int32 ComponentIndex = 0; ComponentIndex < OutSubobjectArray.Num(); ComponentIndex++ )
	{
		UObject* PotentialComponent = OutSubobjectArray[ComponentIndex];
		if (!PotentialComponent->IsDefaultSubobject())
		{
			OutSubobjectArray.RemoveAtSwap(ComponentIndex--);
		}
	}
}

/**
 * FSubobjectReferenceFinder.
 * Helper class used to collect default subobjects of other objects than the referencing object.
 */
class FSubobjectReferenceFinder : public FReferenceCollector
{
public:

	/**
	 * Constructor
	 *
	 * @param InSubobjectArray	Array to add subobject references to
	 * @param	InObject	Referencing object.
	 */
	FSubobjectReferenceFinder(TArray<const UObject*>& InSubobjectArray, UObject* InObject)
		:	ObjectArray(InSubobjectArray)
		, ReferencingObject(InObject)
	{
		check(ReferencingObject != NULL);
		FindSubobjectReferences();
	}

	/**
	 * Finds all default subobjects of other objects referenced by ReferencingObject.
	 */
	void FindSubobjectReferences()
	{
		if( !ReferencingObject->GetClass()->IsChildOf(UClass::StaticClass()) )
		{
			ReferencingObject->SerializeScriptProperties(GetVerySlowReferenceCollectorArchive());
		}
		ReferencingObject->CallAddReferencedObjects(*this);
	}

	// Begin FReferenceCollector interface.
	virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override
	{
		// Only care about unique default subobjects that are outside of the referencing object's outer chain.
		// Also ignore references to subobjects if they share the same Outer.
		// Ignore references from the subobject Outer's class (ComponentNameToDefaultObjectMap).
		if (InObject != NULL && InObject->IsDefaultSubobject() && ObjectArray.Contains(InObject) == false && InObject->IsIn(ReferencingObject) == false &&
			 (ReferencingObject->GetOuter() != InObject->GetOuter() && InObject != ReferencingObject->GetOuter()) &&
			 (InReferencingObject == NULL || (InReferencingObject != InObject->GetOuter()->GetClass() && ReferencingObject != InObject->GetOuter()->GetClass())))
		{
			check(InObject->IsValidLowLevel());
			ObjectArray.Add(InObject);
		}
	}
	virtual bool IsIgnoringArchetypeRef() const override { return true; }
	virtual bool IsIgnoringTransient() const override { return true; }
	// End FReferenceCollector interface.

protected:

	/** Stored reference to array of objects we add object references to. */
	TArray<const UObject*>&	ObjectArray;
	/** Object to check the references of. */
	UObject* ReferencingObject;
};


#define ALLOW_SUB_SUB_OBJECTS (1)
// if this is set to fatal, then we don't run any testing since it is time consuming.
DEFINE_LOG_CATEGORY_STATIC(LogCheckSubobjects, Fatal, All);

#define CompCheck(Pred) \
	if (!(Pred)) \
	{ \
		Result = false; \
		FPlatformMisc::DebugBreak(); \
		UE_LOG(LogCheckSubobjects, Log, TEXT("CompCheck %s failed."), TEXT(#Pred)); \
	} 

bool UObject::CanCheckDefaultSubObjects(bool bForceCheck, bool& bResult)
{
	bool bCanCheck = true;
	bResult = true;
	if (this == nullptr)
	{
		bResult = false; // these aren't in a suitable spot in their lifetime for testing
		bCanCheck = false;
	}
	if (bCanCheck && (HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects) || IsPendingKillOrUnreachable() || GIsDuplicatingClassForReinstancing))
	{
		bResult = true; // these aren't in a suitable spot in their lifetime for testing
		bCanCheck = false;
	}
	// If errors are suppressed, we will not take the time to run this test unless forced to.
	bCanCheck = bCanCheck && (bForceCheck || UE_LOG_ACTIVE(LogCheckSubobjects, Error));
	return bCanCheck;
}

bool UObject::CheckDefaultSubobjects(bool bForceCheck /*= false*/)
{
	bool Result = true;
	if (CanCheckDefaultSubObjects(bForceCheck, Result))
	{
		Result = CheckDefaultSubobjectsInternal();
	}
	return Result;
}

bool UObject::CheckDefaultSubobjectsInternal()
{
	bool Result = true;	

	CompCheck(this);
	UClass* ObjClass = GetClass();

	if (ObjClass != UFunction::StaticClass() && ObjClass->GetName() != TEXT("EdGraphPin"))
	{
		// Check for references to default subobjects of other objects.
		// There should never be a pointer to a subobject from outside of the outer (chain) it belongs to.
		TArray<const UObject*> OtherReferencedSubobjects;
		FSubobjectReferenceFinder DefaultSubobjectCollector(OtherReferencedSubobjects, this);
		for (int32 Index = 0; Index < OtherReferencedSubobjects.Num(); ++Index)
		{
			const UObject* TestObject = OtherReferencedSubobjects[Index];
			UE_LOG(LogCheckSubobjects, Error, TEXT("%s has a reference to default subobject (%s) of %s."), *GetFullName(), *TestObject->GetFullName(), *TestObject->GetOuter()->GetFullName());
		}
		CompCheck(OtherReferencedSubobjects.Num() == 0);
	}

#if 0 // usually overkill, but valid tests
	if (!HasAnyFlags(RF_ClassDefaultObject) && ObjClass->HasAnyClassFlags(CLASS_HasInstancedReference))
	{
		UObject *Archetype = GetArchetype();
		CompCheck(this != Archetype);
		Archetype->CheckDefaultSubobjects();
		if (Archetype != ObjClass->GetDefaultObject())
		{
			ObjClass->GetDefaultObject()->CheckDefaultSubobjects();
		}
	}
#endif

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		CompCheck(GetFName() == ObjClass->GetDefaultObjectName());
	}


	TArray<UObject *> AllCollectedComponents;
	CollectDefaultSubobjects(AllCollectedComponents, true);
	TArray<UObject *> DirectCollectedComponents;
	CollectDefaultSubobjects(DirectCollectedComponents, false);
		
	AllCollectedComponents.Sort();
	DirectCollectedComponents.Sort();

	CompCheck(ALLOW_SUB_SUB_OBJECTS || AllCollectedComponents == DirectCollectedComponents); // just say no to subobjects of subobjects

	return Result;
}

/**
 * Determines whether the specified object should load values using PerObjectConfig rules
 */
bool UsesPerObjectConfig( UObject* SourceObject )
{
	checkSlow(SourceObject);
	return (SourceObject->GetClass()->HasAnyClassFlags(CLASS_PerObjectConfig) && !SourceObject->HasAnyFlags(RF_ClassDefaultObject));
}

/**
 * Returns the file to load ini values from for the specified object, taking into account PerObjectConfig-ness
 */
FString GetConfigFilename( UObject* SourceObject )
{
	checkSlow(SourceObject);

#if 0 // If you find that you are sad this code is gone, please contact RobM or JoeG. We could not find a case for its existence
	// and it broke/made cumbersome perobjectconfig file specification for files that were outside the transient package
	if (UsesPerObjectConfig(SourceObject) && SourceObject->GetOutermost() != GetTransientPackage())
	{
		// if this is a PerObjectConfig object that is not contained by the transient package,
		// load the class's package's ini file
		FString PerObjectConfigName;
		FConfigCacheIni::LoadGlobalIniFile(PerObjectConfigName, *SourceObject->GetOutermost()->GetName());
		return PerObjectConfigName;
	}
	else
#endif
	{
		// otherwise look at the class to get the config name
		return SourceObject->GetClass()->GetConfigName();
	}
}

void UObject::FAssetRegistryTag::GetAssetRegistryTagsFromSearchableProperties(const UObject* Object, TArray<FAssetRegistryTag>& OutTags)
{
	TSet<FName> FoundSpecialStructs;

	check(nullptr != Object);
	for( TFieldIterator<UProperty> FieldIt( Object->GetClass() ); FieldIt; ++FieldIt )
	{
		FName TagName;
		FAssetRegistryTag::ETagType TagType = TT_Alphabetical;
		UStructProperty* StructProp = Cast<UStructProperty>(*FieldIt);

		if (StructProp && StructProp->Struct && IsUniqueAssetRegistryTagStruct(StructProp->Struct->GetFName(), TagType))
		{
			// Special unique structure type
			TagName = StructProp->Struct->GetFName();
			
			if (FoundSpecialStructs.Contains(TagName))
			{
				UE_LOG(LogObj, Error, TEXT("Object %s has more than one unique asset registry struct %s!"), *Object->GetPathName(), *TagName.ToString());
			}
			else
			{
				FoundSpecialStructs.Add(TagName);
			}
		}
		else if (FieldIt->HasAnyPropertyFlags(CPF_AssetRegistrySearchable))
		{
			TagName = FieldIt->GetFName();

			UClass* Class = FieldIt->GetClass();
			if (Class->IsChildOf(UIntProperty::StaticClass()) ||
				Class->IsChildOf(UFloatProperty::StaticClass()) ||
				Class->IsChildOf(UDoubleProperty::StaticClass()))
			{
				// ints and floats are always numerical
				TagType = FAssetRegistryTag::TT_Numerical;
			}
			else if (Class->IsChildOf(UByteProperty::StaticClass()))
			{
				// bytes are numerical, enums are alphabetical
				UByteProperty* ByteProp = static_cast<UByteProperty*>(*FieldIt);
				if (ByteProp->Enum)
				{
					TagType = FAssetRegistryTag::TT_Alphabetical;
				}
				else
				{
					TagType = FAssetRegistryTag::TT_Numerical;
				}
			}
			else if ( Class->IsChildOf(UEnumProperty::StaticClass()) )
			{
				// enums are alphabetical
				TagType = FAssetRegistryTag::TT_Alphabetical;
			}
			else if (Class->IsChildOf(UArrayProperty::StaticClass()) || Class->IsChildOf(UMapProperty::StaticClass()) || Class->IsChildOf(USetProperty::StaticClass())
				|| Class->IsChildOf(UStructProperty::StaticClass()) || Class->IsChildOf(UObjectPropertyBase::StaticClass()))
			{
				// Arrays/maps/sets/structs/objects are hidden, it is often too much information to display and sort
				TagType = FAssetRegistryTag::TT_Hidden;
			}
			else
			{
				// All other types are alphabetical
				TagType = FAssetRegistryTag::TT_Alphabetical;
			}
		}

		if (TagName != NAME_None)
		{

			FString PropertyStr;
			const uint8* PropertyAddr = FieldIt->ContainerPtrToValuePtr<uint8>(Object);
			FieldIt->ExportTextItem( PropertyStr, PropertyAddr, PropertyAddr, nullptr, PPF_None );

			OutTags.Add( FAssetRegistryTag(TagName, PropertyStr, TagType) );
		}
	}
}

bool UObject::FAssetRegistryTag::IsUniqueAssetRegistryTagStruct(FName StructName, ETagType& TagType)
{
	static const FName AssetBundleDataName("AssetBundleData"); // Name of FAssetBundleData in Engine

	if (StructName == AssetBundleDataName)
	{
		TagType = FAssetRegistryTag::TT_Hidden;
		return true;
	}

	return false;
}

const FName FPrimaryAssetId::PrimaryAssetTypeTag(TEXT("PrimaryAssetType"));
const FName FPrimaryAssetId::PrimaryAssetNameTag(TEXT("PrimaryAssetName"));

void UObject::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// Add ResourceSize if non-zero. GetResourceSize is not const because many override implementations end up calling Serialize on this pointers.
	const SIZE_T ResourceSize = const_cast<UObject*>(this)->GetResourceSizeBytes(EResourceSizeMode::Exclusive);
	if ( ResourceSize > 0 || ( !GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) && HasAnyFlags(RF_ClassDefaultObject) ) )
	{
		OutTags.Add( FAssetRegistryTag("ResourceSize", FString::Printf(TEXT("%d"), (ResourceSize + 512) / 1024), FAssetRegistryTag::TT_Numerical) );
	}

	// Add primary asset info if valid
	FPrimaryAssetId PrimaryAssetId = GetPrimaryAssetId();
	if (PrimaryAssetId.IsValid())
	{
		OutTags.Add(FAssetRegistryTag(FPrimaryAssetId::PrimaryAssetTypeTag, PrimaryAssetId.PrimaryAssetType.ToString(), UObject::FAssetRegistryTag::TT_Alphabetical));
		OutTags.Add(FAssetRegistryTag(FPrimaryAssetId::PrimaryAssetNameTag, PrimaryAssetId.PrimaryAssetName.ToString(), UObject::FAssetRegistryTag::TT_Alphabetical));
	}

	FAssetRegistryTag::GetAssetRegistryTagsFromSearchableProperties(this, OutTags);
}

const FName& UObject::SourceFileTagName()
{
	static const FName SourceFilePathName("AssetImportData");
	return SourceFilePathName;
}

#if WITH_EDITOR
void UObject::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
	OutMetadata.Add("ResourceSize",
		FAssetRegistryTagMetadata()
			.SetDisplayName(NSLOCTEXT("UObject", "Size", "Size"))
			.SetSuffix(NSLOCTEXT("UObject", "KilobytesSuffix", "Kb"))
			.SetTooltip(NSLOCTEXT("UObject", "SizeTooltip", "The size of the asset in kilobytes"))
		);
}
#endif

bool UObject::IsAsset() const
{
	// Assets are not transient or CDOs. They must be public.
	const bool bHasValidObjectFlags = !HasAnyFlags(RF_Transient | RF_ClassDefaultObject) && HasAnyFlags(RF_Public) && !IsPendingKill();

	if ( bHasValidObjectFlags )
	{
		// Don't count objects embedded in other objects (e.g. font textures, sequences, material expressions)
		if ( UPackage* LocalOuterPackage = dynamic_cast<UPackage*>(GetOuter()) )
		{
			// Also exclude any objects found in the transient package, or in a package that is transient.
			return LocalOuterPackage != GetTransientPackage() && !LocalOuterPackage->HasAnyFlags(RF_Transient);
		}
	}

	return false;
}

FPrimaryAssetId UObject::GetPrimaryAssetId() const
{
	if (FCoreUObjectDelegates::GetPrimaryAssetIdForObject.IsBound() && IsAsset())
	{
		// Call global callback if bound
		return FCoreUObjectDelegates::GetPrimaryAssetIdForObject.Execute(this);
	}

	return FPrimaryAssetId();
}

bool UObject::IsLocalizedResource() const
{
	const UPackage* ObjPackage = GetOutermost();
	return ObjPackage && FPackageName::IsLocalizedPackage(ObjPackage->GetPathName());
}

bool UObject::IsSafeForRootSet() const
{
	if (IsInBlueprint())
	{
		return false;
	}

	// Exclude linkers from root set if we're using seekfree loading		
	if (!IsPendingKill())
	{
		return true;
	}
	return false;
}

void UObject::TagSubobjects(EObjectFlags NewFlags) 
{
	// Collect a list of all things this element owns
	TArray<UObject*> MemberReferences;
	FReferenceFinder ComponentCollector(MemberReferences, this, false, true, true, true);
	ComponentCollector.FindReferences(this);

	for( TArray<UObject*>::TIterator it(MemberReferences); it; ++it )
	{
		UObject* CurrentObject = *it;
		if( CurrentObject && !CurrentObject->HasAnyFlags(GARBAGE_COLLECTION_KEEPFLAGS) && !CurrentObject->IsRooted())
		{
			CurrentObject->SetFlags(NewFlags);
			CurrentObject->TagSubobjects(NewFlags);
		}
	}
}

void UObject::ReloadConfig( UClass* ConfigClass/*=NULL*/, const TCHAR* InFilename/*=NULL*/, uint32 PropagationFlags/*=LCPF_None*/, UProperty* PropertyToLoad/*=NULL*/ )
{
		if (!GIsEditor)
		{
			LoadConfig(ConfigClass, InFilename, PropagationFlags | UE4::LCPF_ReloadingConfigData | UE4::LCPF_ReadParentSections, PropertyToLoad);
		}
#if WITH_EDITOR
		else
		{
			// When in the editor, raise change events so that the UI will update correctly when object configs are reloaded.
			PreEditChange(NULL);
			LoadConfig(ConfigClass, InFilename, PropagationFlags | UE4::LCPF_ReloadingConfigData | UE4::LCPF_ReadParentSections, PropertyToLoad);
			PostEditChange();
		}
#endif // WITH_EDITOR
}

/** Checks if a section specified as a long package name can be found as short name in ini. */
#if !UE_BUILD_SHIPPING
void CheckMissingSection(const FString& SectionName, const FString& IniFilename)
{
	static TSet<FString> MissingSections;
	FConfigSection* Sec = GConfig->GetSectionPrivate(*SectionName, false, true, *IniFilename);
	if (!Sec && MissingSections.Contains(SectionName) == false)
	{
		FString ShortSectionName = FPackageName::GetShortName(SectionName);
		if (ShortSectionName != SectionName)
		{
			Sec = GConfig->GetSectionPrivate(*ShortSectionName, false, true, *IniFilename);
			if (Sec != NULL)
			{
				UE_LOG(LogObj, Fatal, TEXT("Short class section names (%s) are not supported, please use long name: %s"), *ShortSectionName, *SectionName);
			}
		}
		MissingSections.Add(SectionName);		
	}
}
#endif

void UObject::LoadConfig( UClass* ConfigClass/*=NULL*/, const TCHAR* InFilename/*=NULL*/, uint32 PropagationFlags/*=LCPF_None*/, UProperty* PropertyToLoad/*=NULL*/ )
{
	SCOPE_CYCLE_COUNTER(STAT_LoadConfig);

	// OriginalClass is the class that LoadConfig() was originally called on
	static UClass* OriginalClass = NULL;

	if( !ConfigClass )
	{
		// if no class was specified in the call, this is the OriginalClass
		ConfigClass = GetClass();
		OriginalClass = ConfigClass;
	}

	if( !ConfigClass->HasAnyClassFlags(CLASS_Config) )
	{
		return;
	}

	UClass* ParentClass = ConfigClass->GetSuperClass();
	if ( ParentClass != NULL )
	{
		if ( ParentClass->HasAnyClassFlags(CLASS_Config) )
		{
			if ( (PropagationFlags&UE4::LCPF_ReadParentSections) != 0 )
			{
				// call LoadConfig on the parent class
				LoadConfig( ParentClass, NULL, PropagationFlags, PropertyToLoad );

				// if we are also notifying child classes or instances, stop here as this object's properties will be imported as a result of notifying the others
				if ( (PropagationFlags & (UE4::LCPF_PropagateToChildDefaultObjects|UE4::LCPF_PropagateToInstances)) != 0 )
				{
					return;
				}
			}
			else if ( (PropagationFlags&UE4::LCPF_PropagateToChildDefaultObjects) != 0 )
			{
				// not propagating the call upwards, but we are propagating the call to all child classes
				for (TObjectIterator<UClass> It; It; ++It)
				{
					if (It->IsChildOf(ConfigClass))
					{
						// mask out the PropgateToParent and PropagateToChildren values
						It->GetDefaultObject()->LoadConfig(*It, NULL, (PropagationFlags&(UE4::LCPF_PersistentFlags|UE4::LCPF_PropagateToInstances)), PropertyToLoad);
					}
				}

				// LoadConfig() was called on this object during iteration, so stop here 
				return;
			}
			else if ( (PropagationFlags&UE4::LCPF_PropagateToInstances) != 0 )
			{
				// call LoadConfig() on all instances of this class (except the CDO)
				// Do not propagate this call to parents, and do not propagate to children or instances (would be redundant) 
				for (TObjectIterator<UObject> It; It; ++It)
				{
					if (It->IsA(ConfigClass))
					{
						if ( !GIsEditor )
						{
							// make sure to pass in the class so that OriginalClass isn't reset
							It->LoadConfig(It->GetClass(), NULL, (PropagationFlags&UE4::LCPF_PersistentFlags), PropertyToLoad);
						}
#if WITH_EDITOR
						else
						{
							It->PreEditChange(NULL);

							// make sure to pass in the class so that OriginalClass isn't reset
							It->LoadConfig(It->GetClass(), NULL, (PropagationFlags&UE4::LCPF_PersistentFlags), PropertyToLoad);

							It->PostEditChange();
						}
#endif // WITH_EDITOR
					}
				}
			}
		}
		else if ( (PropagationFlags&UE4::LCPF_PropagateToChildDefaultObjects) != 0 )
		{
			// we're at the base-most config class
			for ( TObjectIterator<UClass> It; It; ++It )
			{
				if ( It->IsChildOf(ConfigClass) )
				{
					if ( !GIsEditor )
					{
						// make sure to pass in the class so that OriginalClass isn't reset
						It->GetDefaultObject()->LoadConfig( *It, NULL, (PropagationFlags&(UE4::LCPF_PersistentFlags|UE4::LCPF_PropagateToInstances)), PropertyToLoad );
					}
#if WITH_EDITOR
					else
					{
						It->PreEditChange(NULL);

						// make sure to pass in the class so that OriginalClass isn't reset
						It->GetDefaultObject()->LoadConfig( *It, NULL, (PropagationFlags&(UE4::LCPF_PersistentFlags|UE4::LCPF_PropagateToInstances)), PropertyToLoad );

						It->PostEditChange();
					}
#endif // WITH_EDITOR
				}
			}

			return;
		}
		else if ( (PropagationFlags&UE4::LCPF_PropagateToInstances) != 0 )
		{
			for ( TObjectIterator<UObject> It; It; ++It )
			{
				if ( It->GetClass() == ConfigClass )
				{
					if ( !GIsEditor )
					{
						// make sure to pass in the class so that OriginalClass isn't reset
						It->LoadConfig(It->GetClass(), NULL, (PropagationFlags&UE4::LCPF_PersistentFlags), PropertyToLoad);
					}
#if WITH_EDITOR
					else
					{
						It->PreEditChange(NULL);

						// make sure to pass in the class so that OriginalClass isn't reset
						It->LoadConfig(It->GetClass(), NULL, (PropagationFlags&UE4::LCPF_PersistentFlags), PropertyToLoad);
						It->PostEditChange();
					}
#endif // WITH_EDITOR
				}
			}
		}
	}

	const FString Filename
	// if a filename was specified, always load from that file
	=	InFilename
		? InFilename
		: GetConfigFilename(this);

	const bool bPerObject = UsesPerObjectConfig(this);

	// does the class want to override the platform hierarchy (ignored if we passd in a specific ini file),
	// and if the name isn't the current running platform (no need to load extra files if already in GConfig)
	bool bUseConfigOverride = InFilename == nullptr && GetConfigOverridePlatform() != nullptr &&
		FCString::Stricmp(GetConfigOverridePlatform(), ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName())) != 0;
	FConfigFile OverrideConfig;
	if (bUseConfigOverride)
	{
		// load into a local ini file
		FConfigCacheIni::LoadLocalIniFile(OverrideConfig, *GetClass()->ClassConfigName.ToString(), true, GetConfigOverridePlatform());
	}


	FString ClassSection;
	FName LongCommitName;
	if ( bPerObject == true )
	{
		FString PathNameString;
		UObject* Outermost = GetOutermost();
		if ( Outermost == GetTransientPackage() )
		{
			PathNameString = GetName();
		}
		else
		{
			GetPathName(Outermost, PathNameString);
			LongCommitName = Outermost->GetFName();
		}
		ClassSection = PathNameString + TEXT(" ") + GetClass()->GetName();
	}

	// If any of my properties are class variables, then LoadConfig() would also be called for each one of those classes.
	// Since OrigClass is a static variable, if the value of a class variable is a class different from the current class, 
	// we'll lose our nice reference to the original class - and cause any variables which were declared after this class variable to fail 
	// the 'if (OriginalClass != Class)' check....better store it in a temporary place while we do the actual loading of our properties 
	UClass* MyOrigClass = OriginalClass;

	if ( PropertyToLoad == NULL )
	{
		UE_LOG(LogConfig, Verbose, TEXT("(%s) '%s' loading configuration from %s"), *ConfigClass->GetName(), *GetName(), *Filename);
	}
	else
	{
		UE_LOG(LogConfig, Verbose, TEXT("(%s) '%s' loading configuration for property %s from %s"), *ConfigClass->GetName(), *GetName(), *PropertyToLoad->GetName(), *Filename);
	}

	for ( UProperty* Property = ConfigClass->PropertyLink; Property; Property = Property->PropertyLinkNext )
	{
		if ( !Property->HasAnyPropertyFlags(CPF_Config) )
		{
			continue;
		}

		// if we're only supposed to load the value for a specific property, skip all others
		if ( PropertyToLoad != NULL && PropertyToLoad != Property )
		{
			continue;
		}

		// Don't load config properties that are marked editoronly if not in the editor
		if ((Property->PropertyFlags & CPF_EditorOnly) && !GIsEditor)
		{
			continue;
		}

		const bool bGlobalConfig = (Property->PropertyFlags&CPF_GlobalConfig) != 0;
		UClass* OwnerClass = Property->GetOwnerClass();

		UClass* BaseClass = bGlobalConfig ? OwnerClass : ConfigClass;
		if ( !bPerObject )
		{
			ClassSection = BaseClass->GetPathName();
			LongCommitName = BaseClass->GetOutermost()->GetFName();
		}

		// globalconfig properties should always use the owning class's config file
		// specifying a value for InFilename will override this behavior (as it does with normal properties)
		const FString& PropFileName = (bGlobalConfig && InFilename == NULL) ? OwnerClass->GetConfigName() : Filename;

		FString Key = Property->GetName();
		int32 PortFlags = 0;

#if WITH_EDITOR
		static FName ConsoleVariableFName(TEXT("ConsoleVariable"));
		const FString& CVarName = Property->GetMetaData(ConsoleVariableFName);
		if (!CVarName.IsEmpty())
		{
			Key = CVarName;
			PortFlags |= PPF_ConsoleVariable;
		}
#endif // #if WITH_EDITOR

		UE_LOG(LogConfig, Verbose, TEXT("   Loading value for %s from [%s]"), *Key, *ClassSection);
		UArrayProperty* Array = dynamic_cast<UArrayProperty*>( Property );
		if( Array == NULL )
		{
			for( int32 i=0; i<Property->ArrayDim; i++ )
			{
				if( Property->ArrayDim!=1 )
				{
					Key = FString::Printf(TEXT("%s[%i]"), *Property->GetName(), i);
				}

				FString Value;
				bool bFoundValue;
				if (bUseConfigOverride)
				{
					bFoundValue = OverrideConfig.GetString(*ClassSection, *Key, Value);
				}
				else
				{
					bFoundValue = GConfig->GetString(*ClassSection, *Key, Value, *PropFileName);
				}

				if (bFoundValue)
				{
					if (Property->ImportText(*Value, Property->ContainerPtrToValuePtr<uint8>(this, i), PortFlags, this) == NULL)
					{
						// this should be an error as the properties from the .ini / .int file are not correctly being read in and probably are affecting things in subtle ways
						UE_LOG(LogObj, Error, TEXT("LoadConfig (%s): import failed for %s in: %s"), *GetPathName(), *Property->GetName(), *Value);
					}
				}

#if !UE_BUILD_SHIPPING
				if (!bFoundValue && !FPlatformProperties::RequiresCookedData())
				{
					CheckMissingSection(ClassSection, PropFileName);
				}
#endif
			}
		}
		else
		{
			FConfigSection* Sec;
			if (bUseConfigOverride)
			{
				Sec = OverrideConfig.Find(*ClassSection);
			}
			else
			{
				Sec = GConfig->GetSectionPrivate(*ClassSection, false, true, *PropFileName);
			}

			FConfigSection* AltSec = NULL;
			//@Package name transition
			if( Sec )
			{
				TArray<FConfigValue> List;
				const FName KeyName(*Key, FNAME_Find);
				Sec->MultiFind(KeyName,List);

				// If we didn't find anything in the first section, try the alternate
				if ((List.Num() == 0) && AltSec)
				{
					AltSec->MultiFind(KeyName,List);
				}

				FScriptArrayHelper_InContainer ArrayHelper(Array, this);
				const int32 Size = Array->Inner->ElementSize;
				// Only override default properties if there is something to override them with.
				if ( List.Num() > 0 )
				{
					ArrayHelper.EmptyAndAddValues(List.Num());
					for( int32 i=List.Num()-1,c=0; i>=0; i--,c++ )
					{
						Array->Inner->ImportText( *List[i].GetValue(), ArrayHelper.GetRawPtr(c), PortFlags, this );
					}
				}
				else
				{
					int32 Index = 0;
					const FConfigValue* ElementValue = nullptr;
					do
					{
						// Add array index number to end of key
						FString IndexedKey = FString::Printf(TEXT("%s[%i]"), *Key, Index);

						// Try to find value of key
						const FName IndexedName(*IndexedKey,FNAME_Find);
						if (IndexedName == NAME_None)
						{
							break;
						}
						ElementValue = Sec->Find(IndexedName);

						// If found, import the element
						if ( ElementValue != nullptr )
						{
							// expand the array if necessary so that Index is a valid element
							ArrayHelper.ExpandForIndex(Index);
							Array->Inner->ImportText(*ElementValue->GetValue(), ArrayHelper.GetRawPtr(Index), PortFlags, this);
						}

						Index++;
					} while( ElementValue || Index < ArrayHelper.Num() );
				}
			}
#if !UE_BUILD_SHIPPING
			else if (!FPlatformProperties::RequiresCookedData())
			{
				CheckMissingSection(ClassSection, PropFileName);
			}
#endif
		}
	}

	// if we are reloading config data after the initial class load, fire the callback now
	if ( (PropagationFlags&UE4::LCPF_ReloadingConfigData) != 0 )
	{
		PostReloadConfig(PropertyToLoad);
	}
}

void UObject::SaveConfig( uint64 Flags, const TCHAR* InFilename, FConfigCacheIni* Config/*=GConfig*/ )
{
	if( !GetClass()->HasAnyClassFlags(CLASS_Config) )
	{
		return;
	}

	uint32 PropagationFlags = UE4::LCPF_None;

	const FString Filename
	// if a filename was specified, always load from that file
	=	InFilename
		? InFilename
		: GetConfigFilename(this);

	// Determine whether the file we are writing is a default file config.
	const bool bIsADefaultIniWrite = Filename == GetDefaultConfigFilename() || Filename == GetGlobalUserConfigFilename();

	const bool bPerObject = UsesPerObjectConfig(this);
	FString Section;
	if ( bPerObject == true )
	{
		FString PathNameString;
		UObject* Outermost = GetOutermost();
		if ( Outermost == GetTransientPackage() )
		{
			PathNameString = GetName();
		}
		else
		{
			GetPathName(Outermost, PathNameString);
		}
		Section = PathNameString + TEXT(" ") + GetClass()->GetName();
	}

	UObject* CDO = GetClass()->GetDefaultObject();

	// only copy the values to the CDO if this is GConfig and we're not saving the CDO
	const bool bCopyValues = (this != CDO && Config == GConfig);

	for ( UProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext )
	{
		if ( !Property->HasAnyPropertyFlags(CPF_Config) )
		{
			continue;
		}

		if( (Property->PropertyFlags & Flags) == Flags )
		{
			UClass* BaseClass = GetClass();

			if (Property->PropertyFlags & CPF_GlobalConfig)
			{
				// call LoadConfig() on child classes if any of the properties were global config
				PropagationFlags |= UE4::LCPF_PropagateToChildDefaultObjects;
				BaseClass = Property->GetOwnerClass();
				if ( BaseClass != GetClass() )
				{
					// call LoadConfig() on parent classes only if the global config property was declared in a parent class
					PropagationFlags |= UE4::LCPF_ReadParentSections;
				}
			}

			FString Key				= Property->GetName();
			int32 PortFlags			= 0;

#if WITH_EDITOR
			static FName ConsoleVariableFName(TEXT("ConsoleVariable"));
			const FString& CVarName = Property->GetMetaData(ConsoleVariableFName);
			if (!CVarName.IsEmpty())
			{
				Key = CVarName;
				PortFlags |= PPF_ConsoleVariable;
			}
#endif // #if WITH_EDITOR

			if ( !bPerObject )
			{
				Section = BaseClass->GetPathName();
			}

			// globalconfig properties should always use the owning class's config file
			// specifying a value for InFilename will override this behavior (as it does with normal properties)
			const FString& PropFileName = ((Property->PropertyFlags & CPF_GlobalConfig) && InFilename == NULL) ? Property->GetOwnerClass()->GetConfigName() : Filename;

			// Properties that are the same as the parent class' defaults should not be saved to ini
			// Before modifying any key in the section, first check to see if it is different from the parent.
			const bool bIsPropertyInherited = Property->GetOwnerClass() != GetClass();
			const bool bShouldCheckIfIdenticalBeforeAdding = !GetClass()->HasAnyClassFlags(CLASS_ConfigDoNotCheckDefaults) && !bPerObject && bIsPropertyInherited;
			UObject* SuperClassDefaultObject = GetClass()->GetSuperClass()->GetDefaultObject();

			UArrayProperty* Array   = dynamic_cast<UArrayProperty*>( Property );
			if( Array )
			{
				if ( !bShouldCheckIfIdenticalBeforeAdding || !Property->Identical_InContainer(this, SuperClassDefaultObject) )
				{
					FConfigSection* Sec = Config->GetSectionPrivate( *Section, 1, 0, *PropFileName );
					check(Sec);
					Sec->Remove( *Key );

					// Default ini's require the array syntax to be applied to the property name
					FString CompleteKey = FString::Printf(TEXT("%s%s"), bIsADefaultIniWrite ? TEXT("+") : TEXT(""), *Key);

					FScriptArrayHelper_InContainer ArrayHelper(Array, this);
					for( int32 i=0; i<ArrayHelper.Num(); i++ )
					{
						FString	Buffer;
						Array->Inner->ExportTextItem( Buffer, ArrayHelper.GetRawPtr(i), ArrayHelper.GetRawPtr(i), this, PortFlags );
						Sec->Add(*CompleteKey, *Buffer);
					}
				}
				else if( Property->Identical_InContainer(this, SuperClassDefaultObject) )
				{
					// If we are not writing it to config above, we should make sure that this property isn't stagnant in the cache.
					FConfigSection* Sec = Config->GetSectionPrivate( *Section, 1, 0, *PropFileName );
					if( Sec )
					{
						Sec->Remove( *Key );
					}

				}
			}
			else
			{
				TCHAR TempKey[MAX_SPRINTF]=TEXT("");
				for( int32 Index=0; Index<Property->ArrayDim; Index++ )
				{
					if( Property->ArrayDim!=1 )
					{
						FCString::Sprintf( TempKey, TEXT("%s[%i]"), *Property->GetName(), Index );
						Key = TempKey;
					}

					if ( !bShouldCheckIfIdenticalBeforeAdding || !Property->Identical_InContainer(this, SuperClassDefaultObject, Index) )
					{
						FString	Value;
						Property->ExportText_InContainer( Index, Value, this, this, this, PortFlags );
						Config->SetString( *Section, *Key, *Value, *PropFileName );
					}
					else if( Property->Identical_InContainer(this, SuperClassDefaultObject, Index) )
					{
						// If we are not writing it to config above, we should make sure that this property isn't stagnant in the cache.
						FConfigSection* Sec = Config->GetSectionPrivate( *Section, 1, 0, *PropFileName );
						if( Sec )
						{
							Sec->Remove( *Key );
						}
					}
				}
			}

			if (bCopyValues)
			{
				void* ThisPropertyAddress = Property->ContainerPtrToValuePtr<void>( this );
				void* CDOPropertyAddr = Property->ContainerPtrToValuePtr<void>( CDO );

				Property->CopyCompleteValue( CDOPropertyAddr, ThisPropertyAddress );
			}
		}
	}

	// only write out the config file if this is GConfig
	if (Config == GConfig)
	{
		Config->Flush( 0 );
	}
}

static FString GetFinalOverridePlatform(const UObject* Obj)
{
	FString Platform;
	if (Obj->GetConfigOverridePlatform() != nullptr && FCString::Stricmp(Obj->GetConfigOverridePlatform(), ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName())) != 0)
	{
		Platform = Obj->GetConfigOverridePlatform();
	}
	return Platform;
}

FString UObject::GetDefaultConfigFilename() const
{
	FString OverridePlatform = GetFinalOverridePlatform(this);
	if (OverridePlatform.Len())
	{
		return FString::Printf(TEXT("%s%s/%s%s.ini"), *FPaths::SourceConfigDir(), *OverridePlatform, *OverridePlatform, *GetClass()->ClassConfigName.ToString());
	}
	return FString::Printf(TEXT("%sDefault%s.ini"), *FPaths::SourceConfigDir(), *GetClass()->ClassConfigName.ToString());
}

FString UObject::GetGlobalUserConfigFilename() const
{
	return FString::Printf(TEXT("%sUnreal Engine/Engine/Config/User%s.ini"), FPlatformProcess::UserSettingsDir(), *GetClass()->ClassConfigName.ToString());
}

// @todo ini: Verify per object config objects
void UObject::UpdateSingleSectionOfConfigFile(const FString& ConfigIniName)
{
	// create a sandbox FConfigCache
	FConfigCacheIni Config(EConfigCacheType::Temporary);

	// add an empty file to the config so it doesn't read in the original file (see FConfigCacheIni.Find())
	FConfigFile& NewFile = Config.Add(ConfigIniName, FConfigFile());

	// save the object properties to this file
	SaveConfig(CPF_Config, *ConfigIniName, &Config);

	ensureMsgf(Config.Num() == 1, TEXT("UObject::UpdateDefaultConfig() caused more files than expected in the Sandbox config cache!"));

	// do we need to use a special platform hierarchy?
	FString OverridePlatform = GetFinalOverridePlatform(this);

	// make sure SaveConfig wrote only to the file we expected
	NewFile.UpdateSections(*ConfigIniName, *GetClass()->ClassConfigName.ToString(), OverridePlatform.Len() ? *OverridePlatform : nullptr);

	// reload the file, so that it refresh the cache internally, unless a non-standard platform was used,
	// then we don't want to touch GConfig
	if (OverridePlatform.Len() == 0)
	{
		FString FinalIniFileName;
		GConfig->LoadGlobalIniFile(FinalIniFileName, *GetClass()->ClassConfigName.ToString(), NULL, true);
	}
}

void UObject::UpdateDefaultConfigFile(const FString& SpecificFileLocation)
{
	UpdateSingleSectionOfConfigFile(SpecificFileLocation.IsEmpty() ? GetDefaultConfigFilename() : SpecificFileLocation);
}

void UObject::UpdateGlobalUserConfigFile()
{
	UpdateSingleSectionOfConfigFile(GetGlobalUserConfigFilename());
}

void UObject::UpdateSinglePropertyInConfigFile(const UProperty* InProperty, const FString& InConfigIniName)
{
	// Arrays and ini files are a mine field, for now we don't support this.
	if (!InProperty->IsA(UArrayProperty::StaticClass()))
	{
		// create a sandbox FConfigCache
		FConfigCacheIni Config(EConfigCacheType::Temporary);

		// add an empty file to the config so it doesn't read in the original file (see FConfigCacheIni.Find())
		FConfigFile& NewFile = Config.Add(InConfigIniName, FConfigFile());

		// save the object properties to this file
		SaveConfig(CPF_Config, *InConfigIniName, &Config);

		// Take the saved section for this object and have the config system process and write out the one property we care about.
		ensureMsgf(Config.Num() == 1, TEXT("UObject::UpdateDefaultConfig() caused more files than expected in the Sandbox config cache!"));

		TArray<FString> Keys;
		NewFile.GetKeys(Keys);

		const FString SectionName = Keys[0];
		FString PropertyKey = InProperty->GetFName().ToString();
		
#if WITH_EDITOR
		static FName ConsoleVariableFName(TEXT("ConsoleVariable"));
		const FString& CVarName = InProperty->GetMetaData(ConsoleVariableFName);
		if (!CVarName.IsEmpty())
		{
			PropertyKey = CVarName;
		}
#endif // #if WITH_EDITOR

		// do we need to use a special platform hierarchy?
		FString OverridePlatform = GetFinalOverridePlatform(this);

		NewFile.UpdateSinglePropertyInSection(*InConfigIniName, *PropertyKey, *SectionName);

		// reload the file, so that it refresh the cache internally, unless a non-standard platform was used,
		// then we don't want to touch GConfig
		if (OverridePlatform.Len() == 0)
		{
			FString FinalIniFileName;
			GConfig->LoadGlobalIniFile(FinalIniFileName, *GetClass()->ClassConfigName.ToString(), NULL, true);
		}
	}
	else
	{
		UE_LOG(LogObj, Warning, TEXT("UObject::UpdateSinglePropertyInConfigFile does not support this property type."));
		return;
	}
}


void UObject::InstanceSubobjectTemplates( FObjectInstancingGraph* InstanceGraph )
{
	UClass *ObjClass = GetClass();
	if (ObjClass->HasAnyClassFlags(CLASS_HasInstancedReference) )
	{
		UObject *Archetype = GetArchetype();
		if (InstanceGraph)
		{
			ObjClass->InstanceSubobjectTemplates( this, Archetype, Archetype ? Archetype->GetClass() : NULL, this, InstanceGraph );
		}
		else
		{
			FObjectInstancingGraph TempInstanceGraph(this);
			ObjClass->InstanceSubobjectTemplates( this, Archetype, Archetype ? Archetype->GetClass() : NULL, this, &TempInstanceGraph );
		}
	}
	CheckDefaultSubobjects();
}



void UObject::ReinitializeProperties( UObject* SourceObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ )
{
	if ( SourceObject == NULL )
	{
		SourceObject = GetArchetype();
	}

	check(GetClass() == UObject::StaticClass() || (SourceObject && IsA(SourceObject->GetClass())));

	// Recreate this object based on the new archetype - using StaticConstructObject rather than manually tearing down and re-initializing
	// the properties for this object ensures that any cleanup required when an object is reinitialized from defaults occurs properly
	// for example, when re-initializing UPrimitiveComponents, the component must notify the rendering thread that its data structures are
	// going to be re-initialized
	StaticConstructObject_Internal( GetClass(), GetOuter(), GetFName(), GetFlags(), GetInternalFlags(), SourceObject, !HasAnyFlags(RF_ClassDefaultObject), InstanceGraph );
}


/*-----------------------------------------------------------------------------
   Shutdown.
-----------------------------------------------------------------------------*/

/**
 * After a critical error, shutdown all objects which require
 * mission-critical cleanup, such as restoring the video mode,
 * releasing hardware resources.
 */
static void StaticShutdownAfterError()
{
	if( UObjectInitialized() )
	{
		static bool bShutdown = false;
		if( bShutdown )
		{
			return;
		}
		bShutdown = true;
		UE_LOG(LogExit, Log, TEXT("Executing StaticShutdownAfterError") );

		for( FRawObjectIterator It; It; ++It )
		{
			UObject* Object = static_cast<UObject*>(It->Object);
			Object->ShutdownAfterError();
		}
	}
}

/*-----------------------------------------------------------------------------
   Command line.
-----------------------------------------------------------------------------*/
#include "UObject/ClassTree.h"

static void ShowIntrinsicClasses( FOutputDevice& Ar )
{
	FClassTree MarkedClasses(UObject::StaticClass());
	FClassTree UnmarkedClasses(UObject::StaticClass());

	for ( TObjectIterator<UClass> It; It; ++It )
	{
		if ( It->HasAnyClassFlags(CLASS_Native) )
		{
			if ( It->HasAllClassFlags(CLASS_Intrinsic) )
			{
				MarkedClasses.AddClass(*It);
			}
			else if ( !It->HasAnyClassFlags(CLASS_Parsed) )
			{
				UnmarkedClasses.AddClass(*It);
			}
		}
	}

	Ar.Logf(TEXT("INTRINSIC CLASSES WITH FLAG SET: %i classes"), MarkedClasses.Num());
	MarkedClasses.DumpClassTree(0, Ar);

	Ar.Logf(TEXT("INTRINSIC CLASSES WITHOUT FLAG SET: %i classes"), UnmarkedClasses.Num());
	UnmarkedClasses.DumpClassTree(0, Ar);
}

//
// Show the inheritance graph of all loaded classes.
//
static void ShowClasses( UClass* Class, FOutputDevice& Ar, int32 Indent )
{
	Ar.Logf( TEXT("%s%s (%d)"), FCString::Spc(Indent), *Class->GetName(), Class->GetPropertiesSize() );

	// Workaround for Visual Studio 2013 analyzer bug. Using a temporary directly in the range-for
	// errors if the analyzer is enabled.
	TObjectRange<UClass> Range;
	for( UClass* Obj : Range )
	{
		if( Obj->GetSuperClass() == Class )
		{
			ShowClasses( Obj, Ar, Indent+2 );
		}
	}
}

void UObject::OutputReferencers( FOutputDevice& Ar, FReferencerInformationList* Referencers/*=NULL*/ )
{
	bool bTempReferencers = false;
	if (!Referencers)
	{
		bTempReferencers = true;
		TArray<FReferencerInformation> InternalReferences;
		TArray<FReferencerInformation> ExternalReferences;

		RetrieveReferencers(&InternalReferences, &ExternalReferences);

		Referencers = new FReferencerInformationList(InternalReferences, ExternalReferences);
	}

	Ar.Log( TEXT("\r\n") );
	if ( Referencers->InternalReferences.Num() > 0 || Referencers->ExternalReferences.Num() > 0 )
	{
		if ( Referencers->ExternalReferences.Num() > 0 )
		{
			Ar.Logf( TEXT("External referencers of %s:\r\n"), *GetFullName() );

			for ( int32 RefIndex = 0; RefIndex < Referencers->ExternalReferences.Num(); RefIndex++ )
			{
				FReferencerInformation& RefInfo = Referencers->ExternalReferences[RefIndex];
				FString ObjectReachability = RefInfo.Referencer->GetFullName();

				if( RefInfo.Referencer->IsRooted() )
				{
					ObjectReachability += TEXT(" (root)");
				}
		
				if( RefInfo.Referencer->IsNative() )
				{
					ObjectReachability += TEXT(" (native)");
				}
		
				if( RefInfo.Referencer->HasAnyFlags(RF_Standalone) )
				{
					ObjectReachability += TEXT(" (standalone)");
				}

				Ar.Logf( TEXT("   %s (%i)\r\n"), *ObjectReachability, RefInfo.TotalReferences );
				for ( int32 i = 0; i < RefInfo.TotalReferences; i++ )
				{
					if ( i < RefInfo.ReferencingProperties.Num() )
					{
						const UProperty* Referencer = RefInfo.ReferencingProperties[i];
						Ar.Logf(TEXT("      %i) %s\r\n"), i, *Referencer->GetFullName());
					}
					else
					{
						Ar.Logf(TEXT("      %i) [[native reference]]\r\n"), i);
					}
				}
			}
		}

		if ( Referencers->InternalReferences.Num() > 0 )
		{
			if ( Referencers->ExternalReferences.Num() > 0 )
			{
				Ar.Log(TEXT("\r\n"));
			}

			Ar.Logf( TEXT("Internal referencers of %s:\r\n"), *GetFullName() );
			for ( int32 RefIndex = 0; RefIndex < Referencers->InternalReferences.Num(); RefIndex++ )
			{
				FReferencerInformation& RefInfo = Referencers->InternalReferences[RefIndex];

				Ar.Logf( TEXT("   %s (%i)\r\n"), *RefInfo.Referencer->GetFullName(), RefInfo.TotalReferences );
				for ( int32 i = 0; i < RefInfo.TotalReferences; i++ )
				{
					if ( i < RefInfo.ReferencingProperties.Num() )
					{
						const UProperty* Referencer = RefInfo.ReferencingProperties[i];
						Ar.Logf(TEXT("      %i) %s\r\n"), i, *Referencer->GetFullName());
					}
					else
					{
						Ar.Logf(TEXT("      %i) [[native reference]]\r\n"), i);
					}
				}
			}
		}
	}
	else
	{
		Ar.Logf(TEXT("%s is not referenced"), *GetFullName());
	}

	Ar.Logf(TEXT("\r\n") );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Ar.Logf(TEXT("Shortest reachability from root to %s:\r\n"), *GetFullName() );
	TMap<UObject*,UProperty*> Rt = FArchiveTraceRoute::FindShortestRootPath(this,true,GARBAGE_COLLECTION_KEEPFLAGS);

	FString RootPath = FArchiveTraceRoute::PrintRootPath(Rt, this);
	Ar.Log(*RootPath);

	Ar.Logf(TEXT("\r\n") );
#endif

	if (bTempReferencers)
	{
		delete Referencers;
	}
}

void UObject::RetrieveReferencers( TArray<FReferencerInformation>* OutInternalReferencers, TArray<FReferencerInformation>* OutExternalReferencers )
{
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;

		if ( Object == this )
		{
			// this one is pretty easy  :)
			continue;
		}

		FArchiveFindCulprit ArFind(this,Object,false);
		TArray<const UProperty*> Referencers;

		int32 Count = ArFind.GetCount(Referencers);
		if ( Count > 0 )
		{
			CA_SUPPRESS(6011)
			if ( Object->IsIn(this) )
			{
				if (OutInternalReferencers != NULL)
				{
					// manually allocate just one element - much slower but avoids slack which improves success rate on consoles
					OutInternalReferencers->Reserve(OutInternalReferencers->Num() + 1);
					new(*OutInternalReferencers) FReferencerInformation(Object, Count, Referencers);
				}
			}
			else
			{
				if (OutExternalReferencers != NULL)
				{
					// manually allocate just one element - much slower but avoids slack which improves success rate on consoles
					OutExternalReferencers->Reserve(OutExternalReferencers->Num() + 1);
					new(*OutExternalReferencers) FReferencerInformation(Object, Count, Referencers);
				}
			}
		}
	}
}

void UObject::ParseParms( const TCHAR* Parms )
{
	if( !Parms )
		return;
	for( TFieldIterator<UProperty> It(GetClass()); It; ++It )
	{
		if( It->GetOuter()!=UObject::StaticClass() )
		{
			FString Value;
			if( FParse::Value(Parms,*(FString(*It->GetName())+TEXT("=")),Value) )
			{
				It->ImportText( *Value, It->ContainerPtrToValuePtr<uint8>(this), 0, this );
			}
		}
	}
}

/**
 * Maps object flag to human-readable string.
 */
class FObjectFlag
{
public:
	EObjectFlags	ObjectFlag;
	const TCHAR*	FlagName;
	FObjectFlag(EObjectFlags InObjectFlag, const TCHAR* InFlagName)
		:	ObjectFlag( InObjectFlag )
		,	FlagName( InFlagName )
	{}
};

/**
 * Initializes the singleton list of object flags.
 */
static TArray<FObjectFlag> PrivateInitObjectFlagList()
{
	TArray<FObjectFlag> ObjectFlagList;
#ifdef	DECLARE_OBJECT_FLAG
#error DECLARE_OBJECT_FLAG already defined
#else
#define DECLARE_OBJECT_FLAG( ObjectFlag ) ObjectFlagList.Add( FObjectFlag( RF_##ObjectFlag, TEXT(#ObjectFlag) ) );
	DECLARE_OBJECT_FLAG( ClassDefaultObject )
	DECLARE_OBJECT_FLAG( ArchetypeObject )
	DECLARE_OBJECT_FLAG( Transactional )
	DECLARE_OBJECT_FLAG( Public	)
	DECLARE_OBJECT_FLAG( TagGarbageTemp )
	DECLARE_OBJECT_FLAG( NeedLoad )
	DECLARE_OBJECT_FLAG( Transient )
	DECLARE_OBJECT_FLAG( Standalone )
	DECLARE_OBJECT_FLAG( BeginDestroyed )
	DECLARE_OBJECT_FLAG( FinishDestroyed )
	DECLARE_OBJECT_FLAG( NeedPostLoad )
#undef DECLARE_OBJECT_FLAG
#endif
	return ObjectFlagList;
}
/**
 * Dumps object flags from the selected objects to debugf.
 */
static void PrivateDumpObjectFlags(UObject* Object, FOutputDevice& Ar)
{
	static TArray<FObjectFlag> SObjectFlagList = PrivateInitObjectFlagList();

	if ( Object )
	{
		FString Buf( FString::Printf( TEXT("%s:\t"), *Object->GetFullName() ) );
		for ( int32 FlagIndex = 0 ; FlagIndex < SObjectFlagList.Num() ; ++FlagIndex )
		{
			const FObjectFlag& CurFlag = SObjectFlagList[ FlagIndex ];
			if ( Object->HasAnyFlags( CurFlag.ObjectFlag ) )
			{
				Buf += FString::Printf( TEXT("%s "), CurFlag.FlagName );
			}
		}
		Ar.Logf( TEXT("%s"), *Buf );
	}
}

/**
 * Recursively visits all object properties and dumps object flags.
 */
static void PrivateRecursiveDumpFlags(UStruct* Struct, void* Data, FOutputDevice& Ar)
{
	check(Data != NULL);
	for( TFieldIterator<UProperty> It(Struct); It; ++It )
	{
		if ( It->GetOwnerClass()->GetPropertiesSize() != sizeof(UObject) )
		{
			for( int32 i=0; i<It->ArrayDim; i++ )
			{
				uint8* Value = It->ContainerPtrToValuePtr<uint8>(Data, i);
				UObjectPropertyBase* Prop = dynamic_cast<UObjectPropertyBase*>(*It);
				if(Prop)
				{
					UObject* Obj = Prop->GetObjectPropertyValue(Value);
					PrivateDumpObjectFlags( Obj, Ar );
				}
				else if( UStructProperty* StructProperty = dynamic_cast<UStructProperty*>(*It) )
				{
					PrivateRecursiveDumpFlags( StructProperty->Struct, Value, Ar );
				}
			}
		}
	}
}

/** 
 * Performs the work for "SET" and "SETNOPEC".
 *
 * @param	Str						reset of console command arguments
 * @param	Ar						output device to use for logging
 * @param	bNotifyObjectOfChange	whether to notify the object about to be changed via Pre/PostEditChange
 */
static void PerformSetCommand( const TCHAR* Str, FOutputDevice& Ar, bool bNotifyObjectOfChange )
{
	// Set a class default variable.
	TCHAR ObjectName[256], PropertyName[256];
	if (FParse::Token(Str, ObjectName, ARRAY_COUNT(ObjectName), true) && FParse::Token(Str, PropertyName, ARRAY_COUNT(PropertyName), true))
	{
		UClass* Class = FindObject<UClass>(ANY_PACKAGE, ObjectName);
		if (Class != NULL)
		{
			UProperty* Property = FindField<UProperty>(Class, PropertyName);
			if (Property != NULL)
			{
				while (*Str == ' ')
				{
					Str++;
				}
				GlobalSetProperty(Str, Class, Property, bNotifyObjectOfChange);
			}
			else
			{
				UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("Unrecognized property %s on class %s"), PropertyName, ObjectName));
			}
		}
		else
		{
			UObject* Object = FindObject<UObject>(ANY_PACKAGE, ObjectName);
			if (Object != NULL)
			{
				UProperty* Property = FindField<UProperty>(Object->GetClass(), PropertyName);
				if (Property != NULL)
				{
					while (*Str == ' ')
					{
						Str++;
					}

#if WITH_EDITOR
					if (!Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange)
					{
						Object->PreEditChange(Property);
					}
#endif // WITH_EDITOR
					Property->ImportText(Str, Property->ContainerPtrToValuePtr<uint8>(Object), 0, Object);
#if WITH_EDITOR
					if (!Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange)
					{
						FPropertyChangedEvent PropertyEvent(Property);
						Object->PostEditChangeProperty(PropertyEvent);
					}
#endif // WITH_EDITOR
				}
			}
			else
			{
				UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("Unrecognized class or object %s"), ObjectName));
			}
		}
	}
	else 
	{
		UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("Unexpected input); format is 'set [class or object name] [property name] [value]")));
	}
}

/** Helper structure for property listing console command */
struct FListPropsWildcardPiece
{
	FString Str;
	bool bMultiChar;
	FListPropsWildcardPiece(const FString& InStr, bool bInMultiChar)
		: Str(InStr), bMultiChar(bInMultiChar)
	{}
};

void ParseFunctionFlags(uint32 Flags, TArray<const TCHAR*>& Results)
{
	const TCHAR* FunctionFlags[32] =
	{
		TEXT("Final"),					// FUNC_Final
		TEXT("0x00000002"),
		TEXT("BlueprintAuthorityOnly"),	// FUNC_BlueprintAuthorityOnly
		TEXT("BlueprintCosmetic"),		// FUNC_BlueprintCosmetic
		TEXT("0x00000010"),
		TEXT("0x00000020"),
		TEXT("Net"),					// FUNC_Net
		TEXT("NetReliable"),			// FUNC_NetReliable
		TEXT("NetRequest"),				// FUNC_NetRequest
		TEXT("Exec"),					// FUNC_Exec
		TEXT("Native"),					// FUNC_Native
		TEXT("Event"),					// FUNC_Event
		TEXT("NetResponse"),			// FUNC_NetResponse
		TEXT("Static"),					// FUNC_Static
		TEXT("NetMulticast"),			// FUNC_NetMulticast
		TEXT("0x00008000"),
		TEXT("MulticastDelegate"),		// FUNC_MulticastDelegate
		TEXT("Public"),					// FUNC_Public
		TEXT("Private"),				// FUNC_Private
		TEXT("Protected"),				// FUNC_Protected
		TEXT("Delegate"),				// FUNC_Delegate
		TEXT("NetServer"),				// FUNC_NetServer
		TEXT("HasOutParms"),			// FUNC_HasOutParms
		TEXT("HasDefaults"),			// FUNC_HasDefaults
		TEXT("NetClient"),				// FUNC_NetClient
		TEXT("DLLImport"),				// FUNC_DLLImport
		TEXT("BlueprintCallable"),		// FUNC_BlueprintCallable
		TEXT("BlueprintEvent"),			// FUNC_BlueprintEvent
		TEXT("BlueprintPure"),			// FUNC_BlueprintPure
		TEXT("0x20000000"),
		TEXT("Const")					// FUNC_Const
		TEXT("0x80000000"),
	};

	for (int32 i = 0; i < 32; ++i)
	{
		const uint32 Mask = 1U << i;
		if ((Flags & Mask) != 0)
		{
			Results.Add(FunctionFlags[i]);
		}
	}
}


TArray<const TCHAR*> ParsePropertyFlags(uint64 Flags)
{
	TArray<const TCHAR*> Results;

	static const TCHAR* PropertyFlags[] =
	{
		TEXT("CPF_Edit"),
		TEXT("CPF_ConstParm"),
		TEXT("CPF_BlueprintVisible"),
		TEXT("CPF_ExportObject"),
		TEXT("CPF_BlueprintReadOnly"),
		TEXT("CPF_Net"),
		TEXT("CPF_EditFixedSize"),
		TEXT("CPF_Parm"),
		TEXT("CPF_OutParm"),
		TEXT("CPF_ZeroConstructor"),
		TEXT("CPF_ReturnParm"),
		TEXT("CPF_DisableEditOnTemplate"),
		TEXT("0x0000000000001000"),
		TEXT("CPF_Transient"),
		TEXT("CPF_Config"),
		TEXT("0x0000000000008000"),
		TEXT("CPF_DisableEditOnInstance"),
		TEXT("CPF_EditConst"),
		TEXT("CPF_GlobalConfig"),
		TEXT("CPF_InstancedReference"),
		TEXT("0x0000000000100000"),
		TEXT("CPF_DuplicateTransient"),
		TEXT("CPF_SubobjectReference"),
		TEXT("0x0000000000800000"),
		TEXT("CPF_SaveGame"),	
		TEXT("CPF_NoClear"),
		TEXT("0x0000000004000000"),
		TEXT("CPF_ReferenceParm"),
		TEXT("CPF_BlueprintAssignable"),
		TEXT("CPF_Deprecated"),
		TEXT("CPF_IsPlainOldData"),
		TEXT("CPF_RepSkip"),
		TEXT("CPF_RepNotify"),
		TEXT("CPF_Interp"),
		TEXT("CPF_NonTransactional"),
		TEXT("CPF_EditorOnly"),
		TEXT("CPF_NoDestructor"),
		TEXT("0x0000002000000000"),
		TEXT("CPF_AutoWeak"),
		TEXT("CPF_ContainsInstancedReference"),
		TEXT("CPF_AssetRegistrySearchable"),
		TEXT("CPF_SimpleDisplay"),
		TEXT("CPF_AdvancedDisplay"),
		TEXT("CPF_Protected"),
		TEXT("CPF_BlueprintCallable"),
		TEXT("CPF_BlueprintAuthorityOnly"),
		TEXT("CPF_TextExportTransient"),
		TEXT("CPF_NonPIEDuplicateTransient"),
		TEXT("CPF_ExposeOnSpawn"),
		TEXT("CPF_PersistentInstance"),
		TEXT("CPF_UObjectWrapper"),
		TEXT("CPF_HasGetValueTypeHash"),
		TEXT("CPF_NativeAccessSpecifierPublic"),
		TEXT("CPF_NativeAccessSpecifierProtected"),
		TEXT("CPF_NativeAccessSpecifierPrivate"),
		TEXT("CPF_SkipSerialization"),
	};

	for (const TCHAR* FlagName : PropertyFlags)
	{
		if (Flags & 1)
		{
			Results.Add(FlagName);
		}

		Flags >>= 1;
	}

	return Results;
}

// #UObject: 2014-09-15 Move to ObjectCommads.cpp or ObjectExec.cpp
bool StaticExec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TCHAR *Str = Cmd;

	if( FParse::Command(&Str,TEXT("GET")) )
	{
		// Get a class default variable.
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;
		if
		(	FParse::Token( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
		&&	(Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL )
		{
			if
			(	FParse::Token( Str, PropertyName, ARRAY_COUNT(PropertyName), 1 )
			&&	(Property=FindField<UProperty>( Class, PropertyName))!=NULL )
			{
				FString	Temp;
				if( Class->GetDefaultsCount() )
				{
					Property->ExportText_InContainer( 0, Temp, (uint8*)Class->GetDefaultObject(), (uint8*)Class->GetDefaultObject(), Class, PPF_IncludeTransient );
				}
				Ar.Log( *Temp );
			}
			else
			{
				UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("Unrecognized property %s"), PropertyName ));
			}
		}
		else
		{
			UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("Unrecognized class %s"), ClassName ));
		}
		return true;
	}
	else if (FParse::Command(&Str, TEXT("LISTPROPS")))
	{
		// list all properties of the specified class that match the specified wildcard string
		TCHAR ClassName[256];
		UClass* Class;
		FString PropWildcard;

		if ( FParse::Token(Str, ClassName, ARRAY_COUNT(ClassName), 1) &&
			(Class = FindObject<UClass>(ANY_PACKAGE, ClassName)) != NULL &&
			FParse::Token(Str, PropWildcard, true) )
		{
			// split up the search string by wildcard symbols
			TArray<FListPropsWildcardPiece> WildcardPieces;
			bool bFound;
			do
			{
				bFound = false;
				int32 AsteriskPos = PropWildcard.Find(TEXT("*"));
				int32 QuestionPos = PropWildcard.Find(TEXT("?"));
				if (AsteriskPos != INDEX_NONE || QuestionPos != INDEX_NONE)
				{
					if (AsteriskPos != INDEX_NONE && (QuestionPos == INDEX_NONE || QuestionPos > AsteriskPos))
					{
						new(WildcardPieces) FListPropsWildcardPiece(PropWildcard.Left(AsteriskPos), true);
						PropWildcard = PropWildcard.Right(PropWildcard.Len() - AsteriskPos - 1);
						bFound = true;
					}
					else if (QuestionPos != INDEX_NONE)
					{
						new(WildcardPieces) FListPropsWildcardPiece(PropWildcard.Left(QuestionPos), false);
						PropWildcard = PropWildcard.Right(PropWildcard.Len() - QuestionPos - 1);
						bFound = true;
					}
				}
			} while (bFound);
			bool bEndedInConstant = (PropWildcard.Len() > 0);
			if (bEndedInConstant)
			{
				new(WildcardPieces) FListPropsWildcardPiece(PropWildcard, false);
			}

			// search for matches
			int32 Count = 0;
			for (TFieldIterator<UProperty> It(Class); It; ++It)
			{
				UProperty* Property = *It;

				Ar.Logf(TEXT("    Prop %s"), *FString::Printf(TEXT("%s at offset %d; %dx %d bytes of type %s"), *Property->GetName(), Property->GetOffset_ForDebug(), Property->ArrayDim, Property->ElementSize, *Property->GetClass()->GetName()));

				for (const TCHAR* Flag : ParsePropertyFlags(Property->PropertyFlags))
				{
					Ar.Logf(TEXT("      Flag %s"), Flag);
				}
			}
			for (TFieldIterator<UProperty> It(Class); It; ++It)
			{
				FString Match = It->GetName();
				bool bResult = true;
				for (int32 i = 0; i < WildcardPieces.Num(); i++)
				{
					if (WildcardPieces[i].Str.Len() > 0)
					{
						int32 Pos = Match.Find(WildcardPieces[i].Str, ESearchCase::IgnoreCase);
						if (Pos == INDEX_NONE || (i == 0 && Pos != 0))
						{
							bResult = false;
							break;
						}
						else if (i > 0 && !WildcardPieces[i - 1].bMultiChar && Pos != 1)
						{
							bResult = false;
							break;
						}

						Match = Match.Right(Match.Len() - Pos - WildcardPieces[i].Str.Len());
					}
				}
				if (bResult)
				{
					// validate ending wildcard, if any
					if (bEndedInConstant)
					{
						bResult = (Match.Len() == 0);
					}
					else if (!WildcardPieces.Last().bMultiChar)
					{
						bResult = (Match.Len() == 1);
					}

					if (bResult)
					{
						FString ExtraInfo;
						if (UStructProperty* StructProperty = dynamic_cast<UStructProperty*>(*It))
						{
							ExtraInfo = *StructProperty->Struct->GetName();
						}
						else if (UClassProperty* ClassProperty = dynamic_cast<UClassProperty*>(*It))
						{
							ExtraInfo = FString::Printf(TEXT("SubclassOf<%s>"), *ClassProperty->MetaClass->GetName());
						}
						else if (USoftClassProperty* SoftClassProperty = dynamic_cast<USoftClassProperty*>(*It))
						{
							ExtraInfo = FString::Printf(TEXT("SoftClassPtr<%s>"), *SoftClassProperty->MetaClass->GetName());
						}
						else if (UObjectPropertyBase* ObjectPropertyBase = dynamic_cast<UObjectPropertyBase*>(*It))
						{
							ExtraInfo = *ObjectPropertyBase->PropertyClass->GetName();
						}
						else
						{
							ExtraInfo = It->GetClass()->GetName();
						}
						Ar.Logf(TEXT("%i) %s (%s)"), Count, *It->GetName(), *ExtraInfo);
						Count++;
					}
				}
			}
			if (Count == 0)
			{
				Ar.Logf(TEXT("- No matches"));
			}
		}
		else
		{
			UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("ListProps: expected format is 'ListProps [class] [wildcard]")));
		}

		return true;
	}
	else if (FParse::Command(&Str, TEXT("GETALL")))
	{
		// iterate through all objects of the specified type and return the value of the specified property for each object
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;

		if ( FParse::Token(Str,ClassName,ARRAY_COUNT(ClassName), 1) &&
			(Class=FindObject<UClass>( ANY_PACKAGE, ClassName)) != NULL )
		{
			FParse::Token(Str,PropertyName,ARRAY_COUNT(PropertyName),1);
			{
				Property=FindField<UProperty>(Class,PropertyName);
				{
					int32 cnt = 0;
					UObject* LimitOuter = NULL;

					const bool bHasOuter = FCString::Strifind(Str,TEXT("OUTER=")) ? true : false;
					ParseObject<UObject>(Str,TEXT("OUTER="),LimitOuter,ANY_PACKAGE);

					// Check for a specific object name
					TCHAR ObjNameStr[256];
					FName ObjName(NAME_None);
					if (FParse::Value(Str,TEXT("NAME="),ObjNameStr,ARRAY_COUNT(ObjNameStr)))
					{
						ObjName = FName(ObjNameStr);
					}
					
					if( bHasOuter && !LimitOuter )
					{
						UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("Failed to find outer %s"), FCString::Strifind(Str,TEXT("OUTER=")) ));
					}
					else
					{
						bool bShowDefaultObjects = FParse::Command(&Str,TEXT("SHOWDEFAULTS"));
						bool bShowPendingKills = FParse::Command(&Str, TEXT("SHOWPENDINGKILLS"));
						bool bShowDetailedInfo = FParse::Command(&Str, TEXT("DETAILED"));
						for ( FObjectIterator It; It; ++It )
						{
							UObject* CurrentObject = *It;

							if ( LimitOuter != NULL && !CurrentObject->IsIn(LimitOuter) )
							{
								continue;
							}

							if ( CurrentObject->IsTemplate(RF_ClassDefaultObject) && bShowDefaultObjects == false )
							{
								continue;
							}

							if (ObjName != NAME_None && CurrentObject->GetFName() != ObjName)
							{
								continue;
							}

							if ( (bShowPendingKills || !CurrentObject->IsPendingKill()) && CurrentObject->IsA(Class) )
							{
								if (!Property)
								{
									if (bShowDetailedInfo)
									{
										Ar.Logf(TEXT("%i) %s %s"), cnt++, *CurrentObject->GetFullName(),*CurrentObject->GetDetailedInfo() );
									}
									else
									{
										Ar.Logf(TEXT("%i) %s"), cnt++, *CurrentObject->GetFullName());
									}
									continue;
								}
								if ( Property->ArrayDim > 1 || dynamic_cast<UArrayProperty*>(Property) != NULL )
								{
									uint8* BaseData = Property->ContainerPtrToValuePtr<uint8>(CurrentObject);
									Ar.Logf(TEXT("%i) %s.%s ="), cnt++, *CurrentObject->GetFullName(), *Property->GetName());

									int32 ElementCount = Property->ArrayDim;

									UProperty* ExportProperty = Property;
									if ( Property->ArrayDim == 1 )
									{
										UArrayProperty* ArrayProp = dynamic_cast<UArrayProperty*>(Property);
										FScriptArrayHelper ArrayHelper(ArrayProp, BaseData);

										BaseData = ArrayHelper.GetRawPtr();
										ElementCount = ArrayHelper.Num();
										ExportProperty = ArrayProp->Inner;
									}

									int32 ElementSize = ExportProperty->ElementSize;
									for ( int32 ArrayIndex = 0; ArrayIndex < ElementCount; ArrayIndex++ )
									{
										FString ResultStr;
										uint8* ElementData = BaseData + ArrayIndex * ElementSize;
										ExportProperty->ExportTextItem(ResultStr, ElementData, NULL, CurrentObject, PPF_IncludeTransient);

										if (bShowDetailedInfo)
										{
											Ar.Logf(TEXT("\t%i: %s %s"), ArrayIndex, *ResultStr, *CurrentObject->GetDetailedInfo());
										}
										else
										{
											Ar.Logf(TEXT("\t%i: %s"), ArrayIndex, *ResultStr);
										}
									}
								}
								else
								{
									uint8* BaseData = (uint8*)CurrentObject;
									FString ResultStr;
									for (int32 i = 0; i < Property->ArrayDim; i++)
									{
										Property->ExportText_InContainer(i, ResultStr, BaseData, BaseData, CurrentObject, PPF_IncludeTransient);
									}

									if (bShowDetailedInfo)
									{
										Ar.Logf(TEXT("%i) %s.%s = %s %s"), cnt++, *CurrentObject->GetFullName(), *Property->GetName(), *ResultStr, *CurrentObject->GetDetailedInfo() );
									}
									else
									{
										Ar.Logf(TEXT("%i) %s.%s = %s"), cnt++, *CurrentObject->GetFullName(), *Property->GetName(), *ResultStr);
									}
								}
							}
						}
					}
				}
			}
		}
		else
		{
			UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("Unrecognized class %s"), ClassName ));
		}
		return true;
	}
	else if (FParse::Command(&Str, TEXT("GETALLSTATE")))
	{
		// iterate through all objects of the specified class and log the state they're in
		TCHAR ClassName[256];
		UClass* Class;

		if ( FParse::Token(Str, ClassName, ARRAY_COUNT(ClassName), 1) &&
			(Class = FindObject<UClass>(ANY_PACKAGE, ClassName)) != NULL )
		{
			bool bShowPendingKills = FParse::Command(&Str, TEXT("SHOWPENDINGKILLS"));
			int32 cnt = 0;
			for (TObjectIterator<UObject> It; It; ++It)
			{
				if ((bShowPendingKills || !It->IsPendingKill()) && It->IsA(Class))
				{
					Ar.Logf( TEXT("%i) %s"), cnt++, *It->GetFullName() );
				}
			}
		}
		else
		{
			UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("Unrecognized class %s"), ClassName));
		}
		return true;
	}
	else if( FParse::Command(&Str,TEXT("SET")) )
	{
		PerformSetCommand( Str, Ar, true );
		return true;
	}
	else if( FParse::Command(&Str,TEXT("SETNOPEC")) )
	{
		PerformSetCommand( Str, Ar, false );
		return true;
	}
#if !UE_BUILD_SHIPPING
	else if( FParse::Command(&Str,TEXT("LISTFUNCS")) )
	{
		// LISTFUNCS <classname>
		TCHAR ClassName[256];

		if (FParse::Token(Str, ClassName, ARRAY_COUNT(ClassName), true))
		{
			//if ( (Property=FindField<UProperty>(Class,PropertyName)) != NULL )
			UClass* Class = FindObject<UClass>(ANY_PACKAGE, ClassName);

			if (Class != NULL)
			{
				Ar.Logf(TEXT("Listing functions introduced in class %s (class flags = 0x%08X)"), ClassName, (uint32)Class->GetClassFlags());
				for (TFieldIterator<UFunction> It(Class); It; ++It)
				{
					UFunction* Function = *It;

					FString FunctionName = Function->GetName();
					Ar.Logf(TEXT("Function %s"), *FunctionName);
				}
			}
			else
			{
				Ar.Logf(TEXT("Could not find any classes named %s"), ClassName);
			}
		}
	}
	else if( FParse::Command(&Str,TEXT("LISTFUNC")) )
	{
		// LISTFUNC <classname> <functionname>
		TCHAR ClassName[256];
		TCHAR FunctionName[256];
		if (FParse::Token(Str, ClassName, ARRAY_COUNT(ClassName), true) && FParse::Token(Str, FunctionName, ARRAY_COUNT(FunctionName), true))
		{
			UClass* Class = FindObject<UClass>(ANY_PACKAGE, ClassName);

			if (Class != NULL)
			{
				UFunction* Function = FindField<UFunction>(Class, FunctionName);

				if (Function != NULL)
				{
					Ar.Logf(TEXT("Processing function %s"), *Function->GetName());

					// Global properties
					if (Function->GetSuperFunction() != NULL)
					{
						Ar.Logf(TEXT("  Has super function (overrides a base class function)"));
					}

					// Flags
					TArray<const TCHAR*> Flags;
					ParseFunctionFlags(Function->FunctionFlags, Flags);
					for (int32 i = 0; i < Flags.Num(); ++i)
					{
						Ar.Logf(TEXT("  Flag %s"), Flags[i]);
					}

					
					// Parameters
					Ar.Logf(TEXT("  %d parameters taking up %d bytes, with return value at offset %d"), Function->NumParms, Function->ParmsSize, Function->ReturnValueOffset);
					for (TFieldIterator<UProperty> It(Function); It; ++It)
					{
						if (It->PropertyFlags & CPF_Parm)
						{
							UProperty* Property = *It;
							
							Ar.Logf(TEXT("    Parameter %s"), *FString::Printf(TEXT("%s at offset %d; %dx %d bytes of type %s"), *Property->GetName(), Property->GetOffset_ForDebug(), Property->ArrayDim, Property->ElementSize, *Property->GetClass()->GetName()));

							for (const TCHAR* Flag : ParsePropertyFlags(Property->PropertyFlags))
							{
								Ar.Logf(TEXT("      Flag %s"), Flag);
							}
						}
					}

					// Locals
					Ar.Logf(TEXT("  Total stack size %d bytes"), Function->PropertiesSize );

					for (TFieldIterator<UProperty> It(Function); It; ++It)
					{
						if ((It->PropertyFlags & CPF_Parm) == 0)
						{							
							UProperty* Property = *It;

							Ar.Logf(TEXT("    Local %s"), *FString::Printf(TEXT("%s at offset %d; %dx %d bytes of type %s"), *Property->GetName(), Property->GetOffset_ForDebug(), Property->ArrayDim, Property->ElementSize, *Property->GetClass()->GetName()));

							for (const TCHAR* Flag : ParsePropertyFlags(Property->PropertyFlags))
							{
								Ar.Logf(TEXT("      Flag %s"), Flag);
							}
						}
					}

					if (Function->Script.Num() > 0)
					{
						Ar.Logf(TEXT("  Has %d bytes of script bytecode"), Function->Script.Num());
					}
				}
			}
		}

		return true;
	}
	else if( FParse::Command(&Str,TEXT("OBJ")) )
	{
		if( FParse::Command(&Str,TEXT("CYCLES")) )
		{
			// find all cycles in the reference graph

			FFindStronglyConnected IndexSet;
			IndexSet.FindAllCycles();
			int32 MaxNum = 0;
			int32 TotalNum = 0;
			int32 TotalCnt = 0;
			for (int32 Index = 0; Index < IndexSet.Components.Num(); Index++)
			{
				TArray<UObject*>& StronglyConnected = IndexSet.Components[Index];
				MaxNum = FMath::Max<int32>(StronglyConnected.Num(), MaxNum);
				if (StronglyConnected.Num() > 1)
				{
					TotalNum += StronglyConnected.Num();
					TotalCnt++;
				}
			}
			// poor mans sort
			for (int32 CurrentNum = MaxNum; CurrentNum > 1; CurrentNum--)
			{
				for (int32 Index = 0; Index < IndexSet.Components.Num(); Index++)
				{
					TArray<UObject*>& StronglyConnected = IndexSet.Components[Index];
					if (StronglyConnected.Num() == CurrentNum)
					{
						Ar.Logf(TEXT("------------------------------------------------------------------------"));
						for (int32 IndexInner = 0; IndexInner < StronglyConnected.Num(); IndexInner++)
						{
							Ar.Logf(TEXT("%s"),*StronglyConnected[IndexInner]->GetFullName());
						}
						Ar.Logf(TEXT("    simple cycle ------------------"));
						TArray<UObject*>& SimpleCycle = IndexSet.SimpleCycles[Index];
						for (int32 IndexDescribe = 0; IndexDescribe < SimpleCycle.Num(); IndexDescribe++)
						{
							int32 Other = IndexDescribe + 1 < SimpleCycle.Num() ? IndexDescribe + 1 : 0;
							Ar.Logf(TEXT("    %s -> %s"), *SimpleCycle[Other]->GetFullName(), *SimpleCycle[IndexDescribe]->GetFullName());
							FArchiveDescribeReference(SimpleCycle[Other], SimpleCycle[IndexDescribe], Ar);
						}
					}
				}
			}

			Ar.Logf(TEXT("------------------------------------------------------------------------"));
			Ar.Logf(TEXT("%d total objects, %d total edges."), IndexSet.AllObjects.Num(), IndexSet.AllEdges.Num());
			Ar.Logf(TEXT("Non-permanent: %d objects, %d edges, %d strongly connected components, %d objects are included in cycles."), IndexSet.TempObjects.Num(), IndexSet.Edges.Num(), TotalCnt, TotalNum);
			return true;
		}
		else if (FParse::Command(&Str, TEXT("VERIFYCOMPONENTS")))
		{
			Ar.Logf(TEXT("------------------------------------------------------------------------------"));

			for (FObjectIterator It; It; ++It)
			{
				UObject* Target = *It;

				// Skip objects that are trashed
				if ((Target->GetOutermost() == GetTransientPackage())
					|| Target->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists)
					|| Target->IsPendingKill())
				{
					continue;
				}

				TArray<UObject*> SubObjects;
				GetObjectsWithOuter(Target, SubObjects);

				TArray<FString> Errors;

				for (UObject* SubObjIt : SubObjects)
				{
					const UObject* SubObj = SubObjIt;
					const UClass* SubObjClass = SubObj->GetClass();
					const FString SubObjName = SubObj->GetName();

					if (SubObj->IsPendingKill())
					{
						continue;
					}

					if (SubObjClass->HasAnyClassFlags(CLASS_NewerVersionExists))
					{
						Errors.Add(FString::Printf(TEXT("  - %s has a stale class"), *SubObjName));
					}

					if (SubObjClass->GetOutermost() == GetTransientPackage())
					{
						Errors.Add(FString::Printf(TEXT("  - %s has a class in the transient package"), *SubObjName));
					}

					if (SubObj->GetOutermost() != Target->GetOutermost())
					{
						Errors.Add(FString::Printf(TEXT("  - %s has a different outer than its parent"), *SubObjName));
					}
					
					if (SubObj->GetName().Find(TEXT("TRASH_")) != INDEX_NONE)
					{
						Errors.Add(FString::Printf(TEXT("  - %s is TRASH'd"), *SubObjName));
					}

					if (SubObj->GetName().Find(TEXT("REINST_")) != INDEX_NONE)
					{
						Errors.Add(FString::Printf(TEXT("  - %s is a REINST"), *SubObjName));
					}
				}

				if (Errors.Num() > 0)
				{
					Ar.Logf(TEXT("Errors for %s"), *Target->GetName());

					for (const FString& ErrorStr : Errors)
					{
						Ar.Logf(TEXT("  - %s"), *ErrorStr);
					}
				}
			}

			Ar.Logf(TEXT("------------------------------------------------------------------------------"));
			return true;
		}
		else if( FParse::Command(&Str,TEXT("TRANSACTIONAL")) )
		{
			int32 Num=0;
			int32 NumTransactional=0;
			for( FObjectIterator It; It; ++It )
			{
				Num++;
				if (It->HasAnyFlags(RF_Transactional))
				{
					NumTransactional++;
				}
				UE_LOG(LogObj, Log, TEXT("%1d %s"),(int32)It->HasAnyFlags(RF_Transactional),*It->GetFullName());
			}
			UE_LOG(LogObj, Log, TEXT("%d/%d"),NumTransactional,Num);
			return true;
		}
		else if( FParse::Command(&Str,TEXT("MARK")) )
		{
			UE_LOG(LogObj, Log,  TEXT("Marking objects") );
			for( FObjectIterator It; It; ++It )
			{
				DebugMarkAnnotation.Set(*It);
			}
			return true;
		}
		else if( FParse::Command(&Str,TEXT("MARKCHECK")) )
		{
			UE_LOG(LogObj, Log,  TEXT("Unmarked (new) objects:") );
			for( FObjectIterator It; It; ++It )
			{
				if(!DebugMarkAnnotation.Get(*It))
				{
					UE_LOG(LogObj, Log,  TEXT("%s"), *It->GetFullName() );
				}
			}
			return true;
		}
		else if( FParse::Command(&Str,TEXT("INVMARK")) )
		{
			UE_LOG(LogObj, Log,  TEXT("InvMarking existing objects") );
			DebugInvMarkWeakPtrs.Empty();
			DebugInvMarkNames.Empty();
			for( FObjectIterator It; It; ++It )
			{
				DebugInvMarkWeakPtrs.Add(TWeakObjectPtr<>(*It));
				DebugInvMarkNames.Add(It->GetFullName());
			}
			return true;
		}
		else if( FParse::Command(&Str,TEXT("INVMARKCHECK")) )
		{
			UE_LOG(LogObj, Log,  TEXT("Objects that were deleted:") );
			for (int32 Old = 0; Old < DebugInvMarkNames.Num(); Old++)
			{
				UObject *Object = DebugInvMarkWeakPtrs[Old].Get();
				if (Object)
				{
					check(TWeakObjectPtr<>(Object) == DebugInvMarkWeakPtrs[Old]);
					check(Object->GetFullName() == DebugInvMarkNames[Old]);
					check(!DebugInvMarkWeakPtrs[Old].IsStale());
					check(DebugInvMarkWeakPtrs[Old].IsValid());
				}
				else
				{
					check(DebugInvMarkWeakPtrs[Old].IsStale());
					check(!DebugInvMarkWeakPtrs[Old].IsValid());
					UE_LOG(LogObj, Log,  TEXT("%s"), *DebugInvMarkNames[Old]);
				}
			}
			return true;
		}
		else if( FParse::Command(&Str,TEXT("REFS")) )
		{
			UObject* Object;
			if (ParseObject(Str,TEXT("NAME="),Object,ANY_PACKAGE))
			{
				uint32 SearchModeFlags = FReferenceChainSearch::ESearchMode::PrintResults;

				FString Tok;
				while(FParse::Token(Str, Tok, false))
				{
					if (FCString::Stricmp(*Tok, TEXT("shortest")) == 0)
					{
						if ( !!(SearchModeFlags&FReferenceChainSearch::ESearchMode::Longest) )
						{
							UE_LOG(LogObj, Log, TEXT("Specifing 'shortest' AND 'longest' is invalid. Ignoring this occurence of 'shortest'."));
						}
						SearchModeFlags |= FReferenceChainSearch::ESearchMode::Shortest;
					}
					else if (FCString::Stricmp(*Tok, TEXT("longest")) == 0)
					{
						if ( !!(SearchModeFlags&FReferenceChainSearch::ESearchMode::Shortest) )
						{
							UE_LOG(LogObj, Log, TEXT("Specifing 'shortest' AND 'longest' is invalid. Ignoring this occurence of 'longest'."));
						}
						SearchModeFlags |= FReferenceChainSearch::ESearchMode::Longest;
					}
					else if (FCString::Stricmp(*Tok, TEXT("external")) == 0)
					{
						SearchModeFlags |= FReferenceChainSearch::ESearchMode::ExternalOnly;
					}
					else if (FCString::Stricmp(*Tok, TEXT("direct")) == 0)
					{
						SearchModeFlags |= FReferenceChainSearch::ESearchMode::Direct;
					}
				}
				

				FReferenceChainSearch RefChainSearch(Object, SearchModeFlags);
			}
			else
			{
				UE_LOG(LogObj, Log, TEXT("Couldn't find object."));
			}
			return true;
		}
		else if (FParse::Command(&Str, TEXT("SINGLEREF")))
		{
			bool bListClass = false;
			UClass* Class;
			UClass* ReferencerClass = NULL;
			FString ReferencerName;
			if (ParseObject<UClass>(Str, TEXT("CLASS="), Class, ANY_PACKAGE) == false)
			{
				Class = UObject::StaticClass();
				bListClass = true;
			}
			if (ParseObject<UClass>(Str, TEXT("REFCLASS="), ReferencerClass, ANY_PACKAGE) == false)
			{
				ReferencerClass = NULL;
			}
			TCHAR TempStr[1024];
			if (FParse::Value(Str, TEXT("REFNAME="), TempStr, ARRAY_COUNT(TempStr)))
			{
				ReferencerName = TempStr;
			}

			for (TObjectIterator<UObject> It; It; ++It)
			{
				UObject* Object = *It;
				if ((Object->IsA(Class)) && (Object->IsTemplate() == false) && (Object->HasAnyFlags(RF_ClassDefaultObject) == false))
				{
					TArray<FReferencerInformation> OutExternalReferencers;
					Object->RetrieveReferencers(NULL, &OutExternalReferencers);

					if (OutExternalReferencers.Num() == 1)
					{
						FReferencerInformation& Info = OutExternalReferencers[0];
						UObject* RefObj = Info.Referencer;
						if (RefObj)
						{
							bool bDumpIt = true;
							if (ReferencerName.Len() > 0)
							{
								if (RefObj->GetName() != ReferencerName)
								{
									bDumpIt = false;
								}
							}
							if (ReferencerClass)
							{
								if (RefObj->IsA(ReferencerClass) == false)
								{
									bDumpIt = false;
								}
							}

							if (bDumpIt)
							{
								FArchiveCountMem Count(Object);

								// Get the 'old-style' resource size and the truer resource size
								const SIZE_T ResourceSize = It->GetResourceSizeBytes(EResourceSizeMode::Inclusive);
								const SIZE_T TrueResourceSize = It->GetResourceSizeBytes(EResourceSizeMode::Exclusive);
								
								if (bListClass)
								{
									Ar.Logf(TEXT("%64s: %64s, %8d,%8d,%8d,%8d"), *(Object->GetClass()->GetName()), *(Object->GetPathName()),
											(int32)Count.GetNum(), (int32)Count.GetMax(), (int32)ResourceSize, (int32)TrueResourceSize);
								}
								else
								{
									Ar.Logf(TEXT("%64s, %8d,%8d,%8d,%8d"), *(Object->GetPathName()),
										(int32)Count.GetNum(), (int32)Count.GetMax(), (int32)ResourceSize, (int32)TrueResourceSize);
								}
								Ar.Logf(TEXT("\t%s"), *(RefObj->GetPathName()));
							}
						}
					}
				}
			}
			return true;
		}
		else if( FParse::Command(&Str,TEXT("CLASSES")) )
		{
			ShowClasses( UObject::StaticClass(), Ar, 0 );
			return true;
		}
		else if( FParse::Command(&Str,TEXT("INTRINSICCLASSES")) )
		{
			ShowIntrinsicClasses(Ar);
			return true;
		}
		else if( FParse::Command(&Str,TEXT("DEPENDENCIES")) )
		{
			UPackage* Pkg;
			if( ParseObject<UPackage>(Str,TEXT("PACKAGE="),Pkg,NULL) )
			{
				TArray<UObject*> Exclude;

				// check if we want to ignore references from any packages
				for( int32 i=0; i<16; i++ )
				{
					TCHAR Temp[MAX_SPRINTF]=TEXT("");
					FCString::Sprintf( Temp, TEXT("EXCLUDE%i="), i );
					FName F;
					if( FParse::Value(Str,Temp,F) )
						Exclude.Add( CreatePackage(NULL,*F.ToString()) );
				}
				Ar.Logf( TEXT("Dependencies of %s:"), *Pkg->GetPathName() );

				bool Dummy=0;

				// Should we recurse into inner packages?
				bool bRecurse = FParse::Bool(Str, TEXT("RECURSE"), Dummy);

				// Iterate through the object list
				for( FObjectIterator It; It; ++It )
				{
					// if this object is within the package specified, serialize the object
					// into a specialized archive which logs object names encountered during
					// serialization -- rjp
					if ( It->IsIn(Pkg) )
					{
						if ( It->GetOuter() == Pkg )
						{
							FArchiveShowReferences ArShowReferences( Ar, Pkg, *It, Exclude );
						}
						else if ( bRecurse )
						{
							// Two options -
							// a) this object is a function or something (which we don't care about)
							// b) this object is inside a group inside the specified package (which we do care about)
							UObject* CurrentObject = *It;
							UObject* CurrentOuter = It->GetOuter();
							while ( CurrentObject && CurrentOuter )
							{
								// this object is a UPackage (a group inside a package)
								// abort
								if ( CurrentObject->GetClass() == UPackage::StaticClass() )
									break;

								// see if this object's outer is a UPackage
								if ( CurrentOuter->GetClass() == UPackage::StaticClass() )
								{
									// if this object's outer is our original package, the original object (It)
									// wasn't inside a group, it just wasn't at the base level of the package
									// (its Outer wasn't the Pkg, it was something else e.g. a function, state, etc.)
									/// ....just skip it
									if ( CurrentOuter == Pkg )
										break;

									// otherwise, we've successfully found an object that was in the package we
									// were searching, but would have been hidden within a group - let's log it
									FArchiveShowReferences ArShowReferences( Ar, CurrentOuter, CurrentObject, Exclude );
									break;
								}

								CurrentObject = CurrentOuter;
								CurrentOuter = CurrentObject->GetOuter();
							}
						}
					}
				}
			}
			else
				UE_LOG(LogObj, Log, TEXT("Package wasn't found."));
			return true;
		}
		else if( FParse::Command(&Str,TEXT("BULK")) )
		{
			FUntypedBulkData::DumpBulkDataUsage( Ar );
			return true;
		}
		else if( FParse::Command(&Str,TEXT("LISTCONTENTREFS")) )
		{
			UClass*	Class		= NULL;
			UClass*	ListClass	= NULL;
			ParseObject<UClass>(Str, TEXT("CLASS="		), Class,		ANY_PACKAGE );
			ParseObject<UClass>(Str, TEXT("LISTCLASS="  ), ListClass,	ANY_PACKAGE );
		
			if( Class )
			{
				/** Helper class for only finding object references we "care" about. See operator << for details. */
				struct FArchiveListRefs : public FArchiveUObject
				{
					/** Set of objects ex and implicitly referenced by root based on criteria in << operator. */
					TSet<UObject*> ReferencedObjects;
					
					/** 
					 * Constructor, performing serialization of root object.
					 */
					FArchiveListRefs( UObject* InRootObject )
					{
						ArIsObjectReferenceCollector = true;
						RootObject = InRootObject;
						RootObject->Serialize( *this );
					}

				private:
					/** Src/ root object to serialize. */
					UObject* RootObject;

					// The serialize operator is private as we don't support changing RootObject. */
					FArchive& operator<<( UObject*& Object )
					{
						if ( Object != NULL )
						{
							// Avoid serializing twice.
							if( ReferencedObjects.Find( Object ) == NULL )
							{
								ReferencedObjects.Add( Object );

								// Recurse if we're in the same package.
								if( RootObject->GetOutermost() == Object->GetOutermost() 
								// Or if package doesn't contain script.
								||	!Object->GetOutermost()->HasAnyPackageFlags(PKG_ContainsScript) )
								{
									// Serialize object. We don't want to use the << operator here as it would call 
									// this function again instead of serializing members.
									Object->Serialize( *this );
								}
							}
						}							
						return *this;
					}
				};

				// Create list of object references.
				FArchiveListRefs ListRefsAr(Class);

				// Give a choice of whether we want sorted list in more human read-able format or whether we want to list in Excel.
				bool bShouldListAsCSV = FParse::Param( Str, TEXT("CSV") );

				// If specified only lists objects not residing in script packages.
				bool bShouldOnlyListContent = !FParse::Param( Str, TEXT("LISTSCRIPTREFS") );

				// Sort refs by class name (un-qualified name).
				struct FSortUObjectByClassName
				{
					FORCEINLINE bool operator()( const UObject& A, const UObject& B ) const
					{
						return A.GetClass()->GetName() <= B.GetClass()->GetName();
					}
				};
				ListRefsAr.ReferencedObjects.Sort( FSortUObjectByClassName() );
				
				if( bShouldListAsCSV )
				{
					UE_LOG(LogObj, Log, TEXT(",Class,Object"));
				}
				else
				{
					UE_LOG(LogObj, Log, TEXT("Dumping references for %s"),*Class->GetFullName());
				}

				// Iterate over references and dump them to log. Either in CSV format or sorted by class.
				for( TSet<UObject*>::TConstIterator It(ListRefsAr.ReferencedObjects); It; ++It ) 
				{
					UObject* ObjectReference = *It;
					// Only list certain class if specified.
					if( (!ListClass || ObjectReference->GetClass() == ListClass)
					// Only list non-script objects if specified.
					&&	(!bShouldOnlyListContent || !ObjectReference->GetOutermost()->HasAnyPackageFlags(PKG_ContainsScript))
					// Exclude the transient package.
					&&	ObjectReference->GetOutermost() != GetTransientPackage() )
					{
						if( bShouldListAsCSV )
						{
							UE_LOG(LogObj, Log, TEXT(",%s,%s"),*ObjectReference->GetClass()->GetPathName(),*ObjectReference->GetPathName());
						}
						else
						{
							UE_LOG(LogObj, Log, TEXT("   %s"),*ObjectReference->GetFullName());
						}
					}
				}
			}
		}
		else if ( FParse::Command(&Str,TEXT("FLAGS")) )
		{
			// Dump all object flags for objects rooted at the named object.
			TCHAR ObjectName[NAME_SIZE];
			UObject* Obj = NULL;
			if ( FParse::Token(Str,ObjectName,ARRAY_COUNT(ObjectName), 1) )
			{
				Obj = FindObject<UObject>(ANY_PACKAGE,ObjectName);
			}

			if ( Obj )
			{
				PrivateDumpObjectFlags( Obj, Ar );
				PrivateRecursiveDumpFlags( Obj->GetClass(), Obj, Ar );
			}

			return true;
		}
		else if (FParse::Command(&Str, TEXT("REP")))
		{
			// Lists all the properties of a class marked for replication
			// Usage:  OBJ REP CLASS=PlayerController
			UClass* Cls;

			if( ParseObject<UClass>( Str, TEXT("CLASS="), Cls, ANY_PACKAGE ) )
			{
				Ar.Logf(TEXT("=== Replicated properties for class: %s==="), *Cls->GetName());
				for ( TFieldIterator<UProperty> It(Cls); It; ++It )
				{
					if( (It->GetPropertyFlags() & CPF_Net) != 0 )
					{
						if( (It->GetPropertyFlags() & CPF_RepNotify) != 0 )
						{
							Ar.Logf(TEXT("   %s <%s>"), *It->GetName(), *It->RepNotifyFunc.ToString());
						}
						else
						{
							Ar.Logf(TEXT("   %s"), *It->GetName());
						}
					}
				}
			}
			else
			{
				UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("No class objects found using command '%s'"), Cmd));
			}

			return true;
		}
		else return false;
	}
	// For reloading config on a particular object
	else if( FParse::Command(&Str,TEXT("RELOADCONFIG")) ||
		FParse::Command(&Str,TEXT("RELOADCFG")))
	{
		TCHAR ClassName[256];
		// Determine the object/class name
		if (FParse::Token(Str,ClassName,ARRAY_COUNT(ClassName),1))
		{
			// Try to find a corresponding class
			UClass* ClassToReload = FindObject<UClass>(ANY_PACKAGE,ClassName);
			if (ClassToReload)
			{
				ClassToReload->ReloadConfig();
			}
			else
			{
				// If the class is missing, search for an object with that name
				UObject* ObjectToReload = FindObject<UObject>(ANY_PACKAGE,ClassName);
				if (ObjectToReload)
				{
					ObjectToReload->ReloadConfig();
				}
			}
		}
		return true;
	}
#endif // !UE_BUILD_SHIPPING
	// Route to self registering exec handlers.
	else if(FSelfRegisteringExec::StaticExec( InWorld, Cmd,Ar ))
	{
		return true;
	}
	
	return false; // Not executed
}

/*-----------------------------------------------------------------------------
	StaticInit & StaticExit.
-----------------------------------------------------------------------------*/

void StaticUObjectInit();
void InitUObject();
void StaticExit();

void PreInitUObject()
{
	// Deprecated.
}

void InitUObject()
{
	// Initialize redirects map
	for (const TPair<FString,FConfigFile>& It : *GConfig)
	{
		FCoreRedirects::ReadRedirectsFromIni(It.Key);
		FLinkerLoad::CreateActiveRedirectsMap(It.Key);
	}

	FCoreDelegates::OnShutdownAfterError.AddStatic(StaticShutdownAfterError);
	FCoreDelegates::OnExit.AddStatic(StaticExit);
	FModuleManager::Get().OnProcessLoadedObjectsCallback().AddStatic(ProcessNewlyLoadedUObjects);

	struct Local
	{
		static bool IsPackageLoaded( FName PackageName )
		{
			return FindPackage( NULL, *PackageName.ToString() ) != NULL;
		}
	};
	FModuleManager::Get().IsPackageLoadedCallback().BindStatic(Local::IsPackageLoaded);
	


#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FCoreUObjectDelegates::StringAssetReferenceLoaded.BindRaw(&GRedirectCollector, &FRedirectCollector::OnStringAssetReferenceLoaded);
	FCoreUObjectDelegates::StringAssetReferenceSaving.BindRaw(&GRedirectCollector, &FRedirectCollector::OnStringAssetReferenceSaved);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

	// Object initialization.
	StaticUObjectInit();
}

//
// Init the object manager and allocate tables.
//
void StaticUObjectInit()
{
	UObjectBaseInit();

	// Allocate special packages.
	GObjTransientPkg = NewObject<UPackage>(nullptr, TEXT("/Engine/Transient"), RF_Transient);
	GObjTransientPkg->AddToRoot();

	if( FParse::Param( FCommandLine::Get(), TEXT("VERIFYGC") ) )
	{
		GShouldVerifyGCAssumptions = true;
	}
	if( FParse::Param( FCommandLine::Get(), TEXT("NOVERIFYGC") ) )
	{
		GShouldVerifyGCAssumptions = false;
	}

	UE_LOG(LogInit, Log, TEXT("Object subsystem initialized") );
}

// Internal cleanup functions
void CleanupGCArrayPools();
void CleanupLinkerAnnotations();

//
// Shut down the object manager.
//
void StaticExit()
{
	check(FUObjectThreadContext::Get().ObjLoaded.Num() == 0);
	if (UObjectInitialized() == false)
	{
		return;
	}

	// Delete all linkers are pending destroy
	DeleteLoaders();

	// Cleanup root.
	if (GObjTransientPkg != NULL)
	{
		GObjTransientPkg->RemoveFromRoot();
		GObjTransientPkg = NULL;
	}

	IncrementalPurgeGarbage( false );

	// Keep track of how many objects there are for GC stats as we simulate a mark pass.
	extern int32 GObjectCountDuringLastMarkPhase;
	GObjectCountDuringLastMarkPhase = 0;

	// Tag all non template & class objects as unreachable. We can't use object iterators for this as they ignore certain objects.
	//
	// Excluding class default, archetype and class objects allows us to not have to worry about fixing issues with initialization 
	// and certain CDO objects like UNetConnection and UChildConnection having members with arrays that point to the same data and 
	// will be double freed if destroyed. Hacky, but much cleaner and lower risk than trying to fix the root cause behind it all. 
	// We need the exit purge for closing network connections and such and only operating on instances of objects is sufficient for 
	// this purpose.
	for ( FRawObjectIterator It; It; ++It )
	{
		// Valid object.
		GObjectCountDuringLastMarkPhase++;

		FUObjectItem* ObjItem = *It;
		checkSlow(ObjItem);
		UObject* Obj = static_cast<UObject*>(ObjItem->Object);
		if (Obj && !Obj->IsA<UField>()) // Skip Structures, properties, etc.. They could be still necessary while GC.
		{
			// Mark as unreachable so purge phase will kill it.
			ObjItem->SetUnreachable();
		}
	}

	// Fully purge all objects, not using time limit.
	GExitPurge					= true;

	// Route BeginDestroy. This needs to be a separate pass from marking as RF_Unreachable as code might rely on RF_Unreachable to be 
	// set on all objects that are about to be deleted. One example is FLinkerLoad detaching textures - the SetLinker call needs to 
	// not kick off texture streaming.
	//
	for ( FRawObjectIterator It; It; ++It )
	{
		FUObjectItem* ObjItem = *It;
		checkSlow(ObjItem);
		if (ObjItem->IsUnreachable())
		{
			// Begin the object's asynchronous destruction.
			UObject* Obj = static_cast<UObject*>(ObjItem->Object);
			Obj->ConditionalBeginDestroy();
		}
	}

	IncrementalPurgeGarbage( false );

	{
		//Repeat GC for every object, including structures and properties.
		for (FRawObjectIterator It; It; ++It)
		{
			// Mark as unreachable so purge phase will kill it.
			It->SetUnreachable();
		}

		for (FRawObjectIterator It; It; ++It)
		{
			FUObjectItem* ObjItem = *It;
			checkSlow(ObjItem);
			if (ObjItem->IsUnreachable())
			{
				// Begin the object's asynchronous destruction.
				UObject* Obj = static_cast<UObject*>(ObjItem->Object);
				Obj->ConditionalBeginDestroy();
			}
		}

		IncrementalPurgeGarbage(false);
	}

	UObjectBaseShutdown();
	// Empty arrays to prevent falsely-reported memory leaks.
	FUObjectThreadContext::Get().ObjLoaded.Empty();
	FDeferredMessageLog::Cleanup();
	CleanupGCArrayPools();
	CleanupLinkerAnnotations();

	UE_LOG(LogExit, Log, TEXT("Object subsystem successfully closed.") );
}


/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

//
// Return the static transient package.
//
UPackage* GetTransientPackage()
{
	return GObjTransientPkg;
}

/*-----------------------------------------------------------------------------
	Replication.
-----------------------------------------------------------------------------*/
		
/** Returns properties that are replicated for the lifetime of the actor channel */
void UObject::GetLifetimeReplicatedProps( TArray< class FLifetimeProperty > & OutLifetimeProps ) const
{

}

/** Called right before receiving a bunch */
void UObject::PreNetReceive()
{

}

/** Called right after receiving a bunch */
void UObject::PostNetReceive()
{

}

/** Called right before being marked for destruction due to network replication */
void UObject::PreDestroyFromReplication()
{

}

/** IsNameStableForNetworking means an object can be referred to its path name (relative to outer) over the network */
bool UObject::IsNameStableForNetworking() const
{
	return HasAnyFlags(RF_WasLoaded | RF_DefaultSubObject) || IsNative() || IsDefaultSubobject();
}

/** IsFullNameStableForNetworking means an object can be referred to its full path name over the network */
bool UObject::IsFullNameStableForNetworking() const
{
	if ( GetOuter() != NULL && !GetOuter()->IsNameStableForNetworking() )
	{
		return false;	// If any outer isn't stable, we can't consider the full name stable
	}

	return IsNameStableForNetworking();
}

/** IsSupportedForNetworking means an object can be referenced over the network */
bool UObject::IsSupportedForNetworking() const
{
	return IsFullNameStableForNetworking();
}
