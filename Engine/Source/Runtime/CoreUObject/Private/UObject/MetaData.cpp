// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/MetaData.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaData, Log, All);

//////////////////////////////////////////////////////////////////////////
// FMetaDataUtilities

#if WITH_EDITOR

FAutoConsoleCommand FMetaDataUtilities::DumpAllConsoleCommand = FAutoConsoleCommand(
	TEXT( "Metadata.Dump" ),
	TEXT( "Dump all MetaData" ),
	FConsoleCommandDelegate::CreateStatic( &FMetaDataUtilities::DumpAllMetaData ) );

void FMetaDataUtilities::DumpMetaData(UMetaData* Object)
{
	UE_LOG(LogMetaData, Log, TEXT("METADATA %s"), *Object->GetPathName());

	for (TMap< FWeakObjectPtr, TMap<FName, FString> >::TIterator It(Object->ObjectMetaDataMap); It; ++It)
	{
		TMap<FName, FString>& MetaDataValues = It.Value();
		for (TMap<FName, FString>::TIterator MetaDataIt(MetaDataValues); MetaDataIt; ++MetaDataIt)
		{
			FName Key = MetaDataIt.Key();
			if (Key != FName(TEXT("ToolTip")))
			{
				UE_LOG(LogMetaData, Log, TEXT("%s: %s=%s"), *It.Key().Get()->GetPathName(), *MetaDataIt.Key().ToString(), *MetaDataIt.Value());
			}
		}
	}

	for (TMap<FName, FString>::TIterator MetaDataIt(Object->RootMetaDataMap); MetaDataIt; ++MetaDataIt)
	{
		FName Key = MetaDataIt.Key();
		if (Key != FName(TEXT("ToolTip")))
		{
			UE_LOG(LogMetaData, Log, TEXT("Root: %s=%s"), *MetaDataIt.Key().ToString(), *MetaDataIt.Value());
		}
	}
}

void FMetaDataUtilities::DumpAllMetaData()
{
	for (TObjectIterator<UMetaData> It; It; ++It)
	{
		UMetaData* MetaData = *It;
		FMetaDataUtilities::DumpMetaData(MetaData);
	}
}

FMetaDataUtilities::FMoveMetadataHelperContext::FMoveMetadataHelperContext(UObject *SourceObject, bool bSearchChildren)
{
	// We only want to actually move things if we're in the editor
	if (GIsEditor)
	{
		check(SourceObject);
		UPackage* Package = SourceObject->GetOutermost();
		check(Package);
		OldPackage = Package;
		OldObject = SourceObject;
		bShouldSearchChildren = bSearchChildren;
	}
}

FMetaDataUtilities::FMoveMetadataHelperContext::~FMoveMetadataHelperContext()
{
	// We only want to actually move things if we're in the editor
	if (GIsEditor && (OldObject->GetOutermost() != OldPackage))
	{
		UMetaData *NewMetaData = OldObject->GetOutermost()->GetMetaData();
		UMetaData *OldMetaData = OldPackage->GetMetaData();
		TMap< FName, FString> *OldObjectMetaData = OldMetaData->ObjectMetaDataMap.Find(OldObject);
		if (OldObjectMetaData != NULL)
		{
			NewMetaData->SetObjectValues(OldObject, *OldObjectMetaData);
		}

		if (bShouldSearchChildren)
		{
			TArray<UObject*> Children;
			GetObjectsWithOuter(OldObject, Children, true);

			for ( auto ChildIt = Children.CreateConstIterator(); ChildIt; ++ChildIt )
			{
				UObject* Child = *ChildIt;
				TMap<FName, FString>* OldObjectMetaDataPtr = OldMetaData->ObjectMetaDataMap.Find(Child);
				if ( OldObjectMetaDataPtr )
				{
					NewMetaData->SetObjectValues(Child, *OldObjectMetaDataPtr);
				}
			}
		}
	}
}

#endif //WITH_EDITOR

//////////////////////////////////////////////////////////////////////////
// UMetaData implementation.

TMap<FName, FName> UMetaData::KeyRedirectMap;

IMPLEMENT_CORE_INTRINSIC_CLASS(UMetaData, UObject,
	{
	}
);

void UMetaData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

	if( Ar.IsSaving() )
	{
		// Remove entries belonging to destructed objects
		for (TMap< FWeakObjectPtr, TMap<FName, FString> >::TIterator It(ObjectMetaDataMap); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
	
	if (!Ar.IsLoading())
	{
		Ar << ObjectMetaDataMap;
		Ar << RootMetaDataMap;
	}
	else
	{
		{
			TMap< FWeakObjectPtr, TMap<FName, FString> > TempMap;
			Ar << TempMap;

			const bool bLoadFromLinker = (NULL != Ar.GetLinker());
			if (bLoadFromLinker && HasAnyFlags(RF_LoadCompleted))
			{
				UE_LOG(LogMetaData, Verbose, TEXT("Metadata was already loaded by linker. %s"), *GetFullName());
			}
			else
			{
				if (bLoadFromLinker && ObjectMetaDataMap.Num())
				{
					UE_LOG(LogMetaData, Verbose, TEXT("Metadata: Some values, filled while serialization, may be lost. %s"), *GetFullName());
				}
				Swap(ObjectMetaDataMap, TempMap);
			}
		}

		if (Ar.CustomVer(FEditorObjectVersion::GUID) >= FEditorObjectVersion::RootMetaDataSupport)
		{
			TMap<FName, FString> TempMap;
			Ar << TempMap;

			const bool bLoadFromLinker = (NULL != Ar.GetLinker());
			if (bLoadFromLinker && HasAnyFlags(RF_LoadCompleted))
			{
				UE_LOG(LogMetaData, Verbose, TEXT("Root metadata was already loaded by linker. %s"), *GetFullName());
			}
			else
			{
				if (bLoadFromLinker && RootMetaDataMap.Num())
				{
					UE_LOG(LogMetaData, Verbose, TEXT("Metadata: Some root values, filled while serialization, may be lost. %s"), *GetFullName());
				}
				Swap(RootMetaDataMap, TempMap);
			}
		}

		// Run redirects on loaded keys
		InitializeRedirectMap();

		for (TMap< FWeakObjectPtr, TMap<FName, FString> >::TIterator ObjectIt(ObjectMetaDataMap); ObjectIt; ++ObjectIt)
		{
			TMap<FName, FString>& CurrentMap = ObjectIt.Value();
			for (TMap<FName, FString>::TIterator PairIt(CurrentMap); PairIt; ++PairIt)
			{
				const FName OldKey = PairIt.Key();
				const FName NewKey = KeyRedirectMap.FindRef(OldKey);
				if (NewKey != NAME_None)
				{
					const FString Value = PairIt.Value();

					PairIt.RemoveCurrent();
					CurrentMap.Add(NewKey, Value);

					UE_LOG(LogMetaData, Verbose, TEXT("Remapping old metadata key '%s' to new key '%s' on object '%s'."), *OldKey.ToString(), *NewKey.ToString(), *ObjectIt.Key().Get()->GetPathName());
				}
			}
		}

		for (TMap<FName, FString>::TIterator PairIt(RootMetaDataMap); PairIt; ++PairIt)
		{
			const FName OldKey = PairIt.Key();
			const FName NewKey = KeyRedirectMap.FindRef(OldKey);
			if (NewKey != NAME_None)
			{
				const FString Value = PairIt.Value();

				PairIt.RemoveCurrent();
				RootMetaDataMap.Add(NewKey, Value);

				UE_LOG(LogMetaData, Verbose, TEXT("Remapping old metadata key '%s' to new key '%s' on root."), *OldKey.ToString(), *NewKey.ToString());
			}
		}
	}

#if 0 && WITH_EDITOR
	FMetaDataUtilities::DumpMetaData(this);
#endif
}

/**
 * Return the value for the given key in the given property
 * @param Object the object to lookup the metadata for
 * @param Key The key to lookup
 * @return The value if found, otherwise an empty string
 */
const FString& UMetaData::GetValue(const UObject* Object, FName Key)
{
	// if not found, return a static empty string
	static FString EmptyString;

	// every key needs to be valid
	if (Key == NAME_None)
	{
		return EmptyString;
	}

	// look up the existing map if we have it
	TMap<FName, FString>* ObjectValues = ObjectMetaDataMap.Find(Object);

	// if not, return empty
	if (ObjectValues == NULL)
	{
		return EmptyString;
	}

	// look for the property
	FString* ValuePtr = ObjectValues->Find(Key);
	
	// if we didn't find it, return NULL
	if (!ValuePtr)
	{
		return EmptyString;
	}

	// if we found it, return the pointer to the character data
	return *ValuePtr;

}

/**
 * Return the value for the given key in the given property
 * @param Object the object to lookup the metadata for
 * @param Key The key to lookup
 * @return The value if found, otherwise an empty string
 */
const FString& UMetaData::GetValue(const UObject* Object, const TCHAR* Key)
{
	// only find names, don't bother creating a name if it's not already there
	// (GetValue will return an empty string if Key is NAME_None)
	return GetValue(Object, FName(Key, FNAME_Find));
}

/**
 * Return whether or not the Key is in the meta data
 * @param Object the object to lookup the metadata for
 * @param Key The key to query for existence
 * @return true if found
 */
bool UMetaData::HasValue(const UObject* Object, FName Key)
{
	// every key needs to be valid
	if (Key == NAME_None)
	{
		return false;
	}

	// look up the existing map if we have it
	TMap<FName, FString>* ObjectValues = ObjectMetaDataMap.Find(Object);

	// if not, return false
	if (ObjectValues == NULL)
	{
		return false;
	}

	// if we had the map, see if we had the key
	return ObjectValues->Find(Key) != NULL;
}

/**
 * Return whether or not the Key is in the meta data
 * @param Object the object to lookup the metadata for
 * @param Key The key to query for existence
 * @return true if found
 */
bool UMetaData::HasValue(const UObject* Object, const TCHAR* Key)
{
	// only find names, don't bother creating a name if it's not already there
	// (HasValue will return false if Key is NAME_None)
	return HasValue(Object, FName(Key, FNAME_Find));
}

/**
 * Is there any metadata for this property?
 * @param Object the object to lookup the metadata for
 * @return TrUE if the property has any metadata at all
 */
bool UMetaData::HasObjectValues(const UObject* Object)
{
	return ObjectMetaDataMap.Contains(Object);
}

/**
 * Set the key/value pair in the Property's metadata
 * @param Object the object to set the metadata for
 * @Values The metadata key/value pairs
 */
void UMetaData::SetObjectValues(const UObject* Object, const TMap<FName, FString>& ObjectValues)
{
	ObjectMetaDataMap.Add(const_cast<UObject*>(Object), ObjectValues);
}

/**
 * Set the key/value pair in the Property's metadata
 * @param Object the object to set the metadata for
 * @param Key A key to set the data for
 * @param Value The value to set for the key
 * @Values The metadata key/value pairs
 */
void UMetaData::SetValue(const UObject* Object, FName Key, const TCHAR* Value)
{
	check(Key != NAME_None);

	if (!HasAllFlags(RF_LoadCompleted))
	{
		UE_LOG(LogMetaData, Error, TEXT("MetaData::SetValue called before meta data is completely loaded. %s'"), *GetPathName());
	}

	// look up the existing map if we have it
	TMap<FName, FString>* ObjectValues = ObjectMetaDataMap.Find(Object);

	// if not, create an empty map
	if (ObjectValues == NULL)
	{
		ObjectValues = &ObjectMetaDataMap.Add(const_cast<UObject*>(Object), TMap<FName, FString>());
	}

	// set the value for the key
	ObjectValues->Add(Key, Value);
}

// Set the Key/Value pair in the Object's metadata
void UMetaData::SetValue(const UObject* Object, const TCHAR* Key, const TCHAR* Value)
{
	SetValue(Object, FName(Key), Value);
}

void UMetaData::RemoveValue(const UObject* Object, const TCHAR* Key)
{
	RemoveValue(Object, FName(Key));
}

void UMetaData::RemoveValue(const UObject* Object, FName Key)
{
	check(Key != NAME_None);

	TMap<FName, FString>* ObjectValues = ObjectMetaDataMap.Find(Object);
	if (ObjectValues)
	{
		// set the value for the key
		ObjectValues->Remove(Key);
	}
}

TMap<FName, FString>* UMetaData::GetMapForObject(const UObject* Object)
{
	check(Object);
	UPackage* Package = Object->GetOutermost();
	check(Package);
	UMetaData* Metadata = Package->GetMetaData();
	check(Metadata);

	TMap<FName, FString>* Map = Metadata->ObjectMetaDataMap.Find(Object);
	return Map;
}

void UMetaData::CopyMetadata(UObject* SourceObject, UObject* DestObject)
{
	check(SourceObject);
	check(DestObject);

	// First get the source map
	TMap<FName, FString>* SourceMap = GetMapForObject(SourceObject);
	if (!SourceMap)
		return;

	// Then get the metadata for the destination
	UPackage* DestPackage = DestObject->GetOutermost();
	check(DestPackage);
	UMetaData* DestMetadata = DestPackage->GetMetaData();
	check(DestMetadata);

	// Iterate through source map, setting each key/value pair in the destination
	for (const auto& It : *SourceMap)
	{
		DestMetadata->SetValue(DestObject, *It.Key.ToString(), *It.Value);
	}
}

/**
 * Removes any metadata entries that are to objects not inside the same package as this UMetaData object.
 */
void UMetaData::RemoveMetaDataOutsidePackage()
{
	TArray<FWeakObjectPtr> ObjectsToRemove;

	// Get the package that this MetaData is in
	UPackage* MetaDataPackage = GetOutermost();

	// Iterate over all entries..
	for (TMap< FWeakObjectPtr, TMap<FName, FString> >::TIterator It(ObjectMetaDataMap); It; ++It)
	{
		FWeakObjectPtr ObjPtr = It.Key();
		// See if its package is not the same as the MetaData's, or is invalid
		if( !ObjPtr.IsValid() || (ObjPtr.Get()->GetOutermost() != MetaDataPackage))
		{
			// Add to list of things to remove
			ObjectsToRemove.Add(ObjPtr);
		}
	}

	// Go through and remove any objects that need it
	for(int32 i=0; i<ObjectsToRemove.Num(); i++)
	{
		FWeakObjectPtr ObjPtr = ObjectsToRemove[i];

		UObject* ObjectToRemove = ObjPtr.Get();
		if ((ObjectToRemove != NULL) && (ObjectToRemove->GetOutermost() != GetTransientPackage()))
		{
			UE_LOG(LogMetaData, Log, TEXT("Removing '%s' ref from Metadata '%s'"), *ObjectToRemove->GetPathName(), *GetPathName());
		}
		ObjectMetaDataMap.Remove( ObjPtr );
	}
}

bool UMetaData::NeedsLoadForEditorGame() const
{
	return true;
}

void UMetaData::InitializeRedirectMap()
{
	static bool bKeyRedirectMapInitialized = false;

	if (!bKeyRedirectMapInitialized)
	{
		if (GConfig)
		{
			const FName MetadataRedirectsName(TEXT("MetadataRedirects"));

			FConfigSection* PackageRedirects = GConfig->GetSectionPrivate(TEXT("CoreUObject.Metadata"), false, true, GEngineIni);
			if (PackageRedirects)
			{
				for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
				{
					if (It.Key() == MetadataRedirectsName)
					{
						FName OldKey = NAME_None;
						FName NewKey = NAME_None;

						FParse::Value(*It.Value().GetValue(), TEXT("OldKey="), OldKey);
						FParse::Value(*It.Value().GetValue(), TEXT("NewKey="), NewKey);

						check(OldKey != NewKey);
						check(OldKey != NAME_None);
						check(NewKey != NAME_None);
						check(!KeyRedirectMap.Contains(OldKey));
						check(!KeyRedirectMap.Contains(NewKey));

						KeyRedirectMap.Add(OldKey, NewKey);
					}			
				}
			}
			bKeyRedirectMapInitialized = true;
		}
	}
}

FName UMetaData::GetRemappedKeyName(FName OldKey)
{
	InitializeRedirectMap();
	return KeyRedirectMap.FindRef(OldKey);
}

