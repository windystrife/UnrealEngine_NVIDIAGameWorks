// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveUObject.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Serialization/SerializedPropertyScope.h"
#include "UObject/UnrealType.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogArchiveUObject, Log, All);

/*----------------------------------------------------------------------------
	FArchiveUObject.
----------------------------------------------------------------------------*/
/**
 * Lazy object pointer serialization.  Lazy object pointers only have weak references to objects and
 * won't serialize the object when gathering references for garbage collection.  So in many cases, you
 * don't need to bother serializing lazy object pointers.  However, serialization is required if you
 * want to load and save your object.
 */
FArchive& FArchiveUObject::operator<<(FLazyObjectPtr& Value)
{
	FArchive& Ar = *this;
	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want weak object pointers to keep objects from being garbage
	// collected.  That would defeat the whole purpose of a weak object pointer!
	// However, when modifying both kinds of references we want to serialize and writeback the updated value.
	// We only want to write the modified value during reference fixup if the data is loaded
	if (!IsObjectReferenceCollector() || IsModifyingWeakAndStrongReferences())
	{
#if WITH_EDITORONLY_DATA
		// When transacting, just serialize as a guid since the object may
		// not be in memory and you don't want to save a nullptr in this case.
		if (IsTransacting())
		{
			if (Ar.IsLoading())
			{
				// Reset before serializing to clear the internal weak pointer. 
				Value.Reset();
			}
			Ar << Value.GetUniqueID();
		}
		else
#endif
		{
			UObject* Object = Value.Get();

			Ar << Object;

			if (IsLoading() || (Object && IsModifyingWeakAndStrongReferences()))
			{
				Value = Object;
			}
		}
	}
	return Ar;
}

FArchive& FArchiveUObject::operator<<(FSoftObjectPtr& Value)
{
	FArchive& Ar = *this;
	if (Ar.IsSaving() || Ar.IsLoading())
	{
		// Reset before serializing to clear the internal weak pointer. 
		Value.ResetWeakPtr();
		Ar << Value.GetUniqueID();
	}
	else if (!IsObjectReferenceCollector() || IsModifyingWeakAndStrongReferences())
	{
		// Treat this like a weak pointer object, as we are doing something like replacing references in memory
		UObject* Object = Value.Get();

		Ar << Object;

		if (IsLoading() || (Object && IsModifyingWeakAndStrongReferences()))
		{
			Value = Object;
		}
	}

	return Ar;
}

FArchive& FArchiveUObject::operator<<(FSoftObjectPath& Value)
{
	Value.SerializePath(*this);
	return *this;
}

FArchive& FArchiveUObject::operator<<(FWeakObjectPtr& Value)
{
	Value.Serialize(*this);
	return *this;
}

/*----------------------------------------------------------------------------
	FObjectAndNameAsStringProxyArchive.
----------------------------------------------------------------------------*/
/**
 * Serialize the given UObject* as an FString
 */
FArchive& FObjectAndNameAsStringProxyArchive::operator<<(UObject*& Obj)
{
	if (IsLoading())
	{
		// load the path name to the object
		FString LoadedString;
		InnerArchive << LoadedString;
		// look up the object by fully qualified pathname
		Obj = FindObject<UObject>(nullptr, *LoadedString, false);
		// If we couldn't find it, and we want to load it, do that
		if(!Obj && bLoadIfFindFails)
		{
			Obj = LoadObject<UObject>(nullptr, *LoadedString);
		}
	}
	else
	{
		// save out the fully qualified object name
		FString SavedString(Obj->GetPathName());
		InnerArchive << SavedString;
	}
	return *this;
}

FArchive& FObjectAndNameAsStringProxyArchive::operator<<(FWeakObjectPtr& Obj)
{
	Obj.Serialize(*this);
	return *this;
}

#if WITH_EDITORONLY_DATA
void FSerializedPropertyScope::PushEditorOnlyProperty()
{
	if (Property && Property->IsEditorOnlyProperty())
	{
		Ar.PushEditorOnlyProperty();
	}
}
void FSerializedPropertyScope::PopEditorOnlyProperty()
{
	if (Property && Property->IsEditorOnlyProperty())
	{
		Ar.PopEditorOnlyProperty();
	}
}
#endif

void FArchiveReplaceObjectRefBase::SerializeObject(UObject* ObjectToSerialize)
{
	// Simple FReferenceCollector proxy for FArchiveReplaceObjectRefBase
	class FReplaceObjectRefCollector : public FReferenceCollector
	{
		FArchive& Ar;
		bool bAllowReferenceElimination;
	public:
		FReplaceObjectRefCollector(FArchive& InAr)
			: Ar(InAr)
			, bAllowReferenceElimination(true)
		{
		}
		virtual bool IsIgnoringArchetypeRef() const override
		{
			return Ar.IsIgnoringArchetypeRef();
		}
		virtual bool IsIgnoringTransient() const override
		{
			return false;
		}
		virtual void AllowEliminatingReferences(bool bAllow) override
		{
			bAllowReferenceElimination = bAllow;
		}
		virtual void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override
		{
			if (bAllowReferenceElimination)
			{
				UProperty* NewSerializedProperty = const_cast<UProperty*>(InReferencingProperty);
				FSerializedPropertyScope SerializedPropertyScope(Ar, NewSerializedProperty ? NewSerializedProperty : Ar.GetSerializedProperty());
				Ar << InObject;
			}
		}
	} ReplaceRefCollector(*this);

	// serialization for class default objects must be deterministic (since class 
	// default objects may be serialized during script compilation while the script
	// and C++ versions of a class are not in sync), so use SerializeTaggedProperties()
	// rather than the native Serialize() function
	UClass* ObjectClass = ObjectToSerialize->GetClass();
	if (ObjectToSerialize->HasAnyFlags(RF_ClassDefaultObject))
	{		
		StartSerializingDefaults();
		if (!WantBinaryPropertySerialization() && (IsLoading() || IsSaving()))
		{
			ObjectClass->SerializeTaggedProperties(*this, (uint8*)ObjectToSerialize, ObjectClass, nullptr);
		}
		else
		{
			ObjectClass->SerializeBin(*this, ObjectToSerialize);
		}
		StopSerializingDefaults();
	}
	else
	{
		ObjectToSerialize->Serialize(*this);
	}
	ObjectClass->CallAddReferencedObjects(ObjectToSerialize, ReplaceRefCollector);
}
