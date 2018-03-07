// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LazyObjectPtr.cpp: Guid-based lazy pointer to UObject
=============================================================================*/

#include "UObject/LazyObjectPtr.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectAnnotation.h"


/** Annotation associating objects with their guids **/
static FUObjectAnnotationSparseSearchable<FUniqueObjectGuid,true> GuidAnnotation;

#define MAX_PIE_INSTANCES 10
static TMap<FGuid, FGuid> PIEGuidMap[MAX_PIE_INSTANCES];

/*-----------------------------------------------------------------------------
	FUniqueObjectGuid
-----------------------------------------------------------------------------*/

FUniqueObjectGuid::FUniqueObjectGuid(const class UObject* InObject)
	: Guid(GuidAnnotation.GetAnnotation(InObject).Guid)
{
}

FUniqueObjectGuid FUniqueObjectGuid::FixupForPIE(int32 PlayInEditorID) const
{
	FUniqueObjectGuid Temp(*this);

	check(PlayInEditorID != -1)
	const FGuid *FoundGuid = PIEGuidMap[PlayInEditorID % MAX_PIE_INSTANCES].Find(Temp.GetGuid());

	if (FoundGuid)
	{
		Temp = *FoundGuid;
	}
	return Temp;
}

UObject* FUniqueObjectGuid::ResolveObject() const
{
	UObject* Result = GuidAnnotation.Find(*this);
	return Result;
}

FString FUniqueObjectGuid::ToString() const
{
	return Guid.ToString(EGuidFormats::UniqueObjectGuid);
}

void FUniqueObjectGuid::FromString(const FString& From)
{
	TArray<FString> Split;
	Split.Empty(4);
	if( From.ParseIntoArray( Split, TEXT("-"), false ) == 4 )
	{
		Guid.A=FParse::HexNumber(*Split[0]);
		Guid.B=FParse::HexNumber(*Split[1]);
		Guid.C=FParse::HexNumber(*Split[2]);
		Guid.D=FParse::HexNumber(*Split[3]);
	}
	else
	{
		Guid.Invalidate();
	}
}

FUniqueObjectGuid FUniqueObjectGuid::GetOrCreateIDForObject(const class UObject *Object)
{
	check(Object);
	checkSlow(IsInGameThread());
	FUniqueObjectGuid ObjectGuid(Object);
	if (!ObjectGuid.IsValid())
	{
		ObjectGuid.Guid = FGuid::NewGuid();
		GuidAnnotation.AddAnnotation(Object, ObjectGuid);
		Object->MarkPackageDirty();
	}
	return ObjectGuid;
}

FThreadSafeCounter FUniqueObjectGuid::CurrentAnnotationTag(1);

/*-----------------------------------------------------------------------------------------------------------
	FLazyObjectPtr
-------------------------------------------------------------------------------------------------------------*/

void FLazyObjectPtr::PossiblySerializeObjectGuid(UObject *Object, FArchive& Ar)
{
	if (Ar.IsSaving() || Ar.IsCountingMemory())
	{
		FUniqueObjectGuid Guid = GuidAnnotation.GetAnnotation(Object);
		bool HasGuid = Guid.IsValid();
		Ar << HasGuid;
		if (HasGuid)
		{
			if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
			{
				if (Object->GetName().StartsWith(TEXT("CorePointerTestBP3")))
				{
					static volatile int32 xx = 0;
					xx++;
				}
				check(GPlayInEditorID != -1);
				FGuid &FoundGuid = PIEGuidMap[GPlayInEditorID % MAX_PIE_INSTANCES].FindOrAdd(Guid.GetGuid());
				if (!FoundGuid.IsValid())
				{
					Guid = FoundGuid = FGuid::NewGuid();
				}
				else
				{
					Guid = FoundGuid;
				}
			}

			Ar << Guid;
		}
	}
	else if (Ar.IsLoading())
	{
		bool HasGuid = false;
		Ar << HasGuid;
		if (HasGuid)
		{
			FUniqueObjectGuid Guid;
			Ar << Guid;

			// Don't try and resolve GUIDs when loading a package for diffing
			const UPackage* Package = Object->GetOutermost();
			const bool bLoadedForDiff = Package->HasAnyPackageFlags(PKG_ForDiffing);
			if (!bLoadedForDiff && (!(Ar.GetPortFlags() & PPF_Duplicate) || (Ar.GetPortFlags() & PPF_DuplicateForPIE)))
			{
				check(!Guid.IsDefault());
				UObject* OtherObject = Guid.ResolveObject();
				if (OtherObject != Object) // on undo/redo, the object (potentially) already exists
				{
					const bool bDuplicate = OtherObject != nullptr;
					const bool bReassigning = FParse::Param(FCommandLine::Get(), TEXT("AssignNewMapGuids"));

					if (bDuplicate || bReassigning)
					{
						if (!bReassigning && OtherObject && OtherObject->HasAnyFlags(RF_NewerVersionExists))
						{
							GuidAnnotation.RemoveAnnotation(OtherObject);
							GuidAnnotation.AddAnnotation(Object, Guid);
						}
						else
						{
							if (!bReassigning)
							{
								// Always warn for non-map packages, skip map packages in PIE or game
								const bool bInGame = FApp::IsGame() || Package->HasAnyPackageFlags(PKG_PlayInEditor);

								UE_CLOG(!Package->ContainsMap() || !bInGame, LogUObjectGlobals, Warning,
									TEXT("Guid referenced by %s is already used by %s, which should never happen in the editor but could happen at runtime with duplicate level loading or PIE"),
									*Object->GetFullName(), *OtherObject->GetFullName());
							}
							else
							{
								UE_LOG(LogUObjectGlobals, Warning, TEXT("Assigning new Guid to %s"), *Object->GetFullName());
							}
							// This guid is in use, which should never happen in the editor but could happen at runtime with duplicate level loading or PIE. If so give it an invalid GUID and don't add to the annotation map.
							Guid = FGuid();
						}
					}
					else
					{
						GuidAnnotation.AddAnnotation(Object, Guid);
					}
					FUniqueObjectGuid::InvalidateTag();
				}
			}
		}
	}
}

void FLazyObjectPtr::ResetPIEFixups()
{
	check(GPlayInEditorID != -1);
	PIEGuidMap[GPlayInEditorID % MAX_PIE_INSTANCES].Reset();
}

