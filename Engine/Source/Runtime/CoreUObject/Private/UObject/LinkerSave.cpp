// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/LinkerSave.h"
#include "HAL/FileManager.h"
#include "Serialization/LargeMemoryWriter.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "Internationalization/TextPackageNamespaceUtil.h"

/*----------------------------------------------------------------------------
	FLinkerSave.
----------------------------------------------------------------------------*/

/** A mapping of package name to generated script SHA keys */
TMap<FString, TArray<uint8> > FLinkerSave::PackagesToScriptSHAMap;

FLinkerSave::FLinkerSave(UPackage* InParent, const TCHAR* InFilename, bool bForceByteSwapping, bool bInSaveUnversioned)
:	FLinker(ELinkerType::Save, InParent, InFilename )
,	Saver(nullptr)
{
	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Create file saver.
		Saver = IFileManager::Get().CreateFileWriter( InFilename, 0 );
		if( !Saver )
		{
			UE_LOG(LogLinker, Fatal, TEXT("%s"), *FString::Printf( TEXT("Error opening file '%s'."), InFilename ) );
		}

		UPackage* Package = dynamic_cast<UPackage*>(LinkerRoot);

		// Set main summary info.
		Summary.Tag           = PACKAGE_FILE_TAG;
		Summary.SetFileVersions( GPackageFileUE4Version, GPackageFileLicenseeUE4Version, bInSaveUnversioned );
		Summary.SavedByEngineVersion = FEngineVersion::Current();
		Summary.CompatibleWithEngineVersion = FEngineVersion::CompatibleWith();
		Summary.PackageFlags = Package ? (Package->GetPackageFlags() & ~PKG_NewlyCreated) : 0;

		if (Package)
		{
			Summary.FolderName = Package->GetFolderName().ToString();
			Summary.ChunkIDs = Package->GetChunkIDs();
		}

		// Set status info.
		ArIsSaving				= 1;
		ArIsPersistent			= 1;
		ArForceByteSwapping		= bForceByteSwapping;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			SetLocalizationNamespace(TextNamespaceUtil::GetPackageNamespace(LinkerRoot));
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
	}
}


FLinkerSave::FLinkerSave(UPackage* InParent, FArchive *InSaver, bool bForceByteSwapping, bool bInSaveUnversioned)
: FLinker(ELinkerType::Save, InParent, TEXT("$$Memory$$"))
, Saver(nullptr)
{
	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Create file saver.
		Saver = InSaver;
		check(Saver);
#if WITH_EDITOR
		ArDebugSerializationFlags = Saver->ArDebugSerializationFlags;
#endif
		

		UPackage* Package = dynamic_cast<UPackage*>(LinkerRoot);

		// Set main summary info.
		Summary.Tag = PACKAGE_FILE_TAG;
		Summary.SetFileVersions(GPackageFileUE4Version, GPackageFileLicenseeUE4Version, bInSaveUnversioned);
		Summary.SavedByEngineVersion = FEngineVersion::Current();
		Summary.CompatibleWithEngineVersion = FEngineVersion::CompatibleWith();
		Summary.PackageFlags = Package ? (Package->GetPackageFlags() & ~PKG_NewlyCreated) : 0;

		if (Package)
		{
			Summary.FolderName = Package->GetFolderName().ToString();
			Summary.ChunkIDs = Package->GetChunkIDs();
		}

		// Set status info.
		ArIsSaving = 1;
		ArIsPersistent = 1;
		ArForceByteSwapping = bForceByteSwapping;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			SetLocalizationNamespace(TextNamespaceUtil::GetPackageNamespace(LinkerRoot));
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
	}
}

FLinkerSave::FLinkerSave(UPackage* InParent, bool bForceByteSwapping, bool bInSaveUnversioned )
:	FLinker(ELinkerType::Save, InParent,TEXT("$$Memory$$"))
,	Saver(nullptr)
{
	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Create file saver.
		Saver = new FLargeMemoryWriter( 0, false, InParent->FileName );
		check(Saver);

		UPackage* Package = dynamic_cast<UPackage*>(LinkerRoot);

		// Set main summary info.
		Summary.Tag           = PACKAGE_FILE_TAG;
		Summary.SetFileVersions( GPackageFileUE4Version, GPackageFileLicenseeUE4Version, bInSaveUnversioned );
		Summary.SavedByEngineVersion = FEngineVersion::Current();
		Summary.CompatibleWithEngineVersion = FEngineVersion::CompatibleWith();
		Summary.PackageFlags = Package ? (Package->GetPackageFlags() & ~PKG_NewlyCreated) : 0;

		if (Package)
		{
			Summary.FolderName = Package->GetFolderName().ToString();
			Summary.ChunkIDs = Package->GetChunkIDs();
		}

		// Set status info.
		ArIsSaving				= 1;
		ArIsPersistent			= 1;
		ArForceByteSwapping		= bForceByteSwapping;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			SetLocalizationNamespace(TextNamespaceUtil::GetPackageNamespace(LinkerRoot));
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
	}
}
/**
 * Detaches file saver and hence file handle.
 */
void FLinkerSave::Detach()
{
	if (Saver)
	{
		delete Saver;
	}
	Saver = nullptr;
}

FLinkerSave::~FLinkerSave()
{
	// Detach file saver/ handle.
	Detach();
}

int32 FLinkerSave::MapName(const FName& Name) const
{
	const int32* IndexPtr = NameIndices.Find(Name);

	if (IndexPtr)
	{
		return *IndexPtr;
	}

	return INDEX_NONE;
}

FPackageIndex FLinkerSave::MapObject( const UObject* Object ) const
{
	if (Object)
	{
		const FPackageIndex *Found = ObjectIndicesMap.Find(Object);
		if (Found)
		{
			if (IsEventDrivenLoaderEnabledInCookedBuilds() &&
				IsCooking() && CurrentlySavingExport.IsExport() &&
				Object->GetOutermost()->GetFName() != GLongCoreUObjectPackageName && // We assume nothing in coreuobject ever loads assets in a constructor
				*Found != CurrentlySavingExport) // would be weird, but I can't be a dependency on myself
			{
				const FObjectExport& SavingExport = Exp(CurrentlySavingExport);
				bool bFoundDep = false;
				if (SavingExport.FirstExportDependency >= 0)
				{
					int32 NumDeps = SavingExport.CreateBeforeCreateDependencies + SavingExport.CreateBeforeSerializationDependencies + SavingExport.SerializationBeforeCreateDependencies + SavingExport.SerializationBeforeSerializationDependencies;
					for (int32 DepIndex = SavingExport.FirstExportDependency; DepIndex < SavingExport.FirstExportDependency + NumDeps; DepIndex++)
					{
						if (DepListForErrorChecking[DepIndex] == *Found)
						{
							bFoundDep = true;
						}
					}
				}
				if (!bFoundDep)
				{
					if (SavingExport.Object && SavingExport.Object->IsA(UClass::StaticClass()) && CastChecked<UClass>(SavingExport.Object)->GetDefaultObject() == Object)
					{
						bFoundDep = true; // the class is saving a ref to the CDO...which doesn't really work or do anything useful, but it isn't an error
					}
				}
				if (!bFoundDep)
				{
					UE_LOG(LogLinker, Fatal, TEXT("Attempt to map an object during save that was not listed as a dependency. Saving Export %d %s in %s. Missing Dep on %s %s."),
						CurrentlySavingExport.ForDebugging(), *SavingExport.ObjectName.ToString(), *GetArchiveName(),
						Found->IsExport() ? TEXT("Export") : TEXT("Import"), *ImpExp(*Found).ObjectName.ToString()
						);
				}
			}

			return *Found;
		}
	}
	return FPackageIndex();
}

void FLinkerSave::Seek( int64 InPos )
{
	Saver->Seek( InPos );
}

int64 FLinkerSave::Tell()
{
	return Saver->Tell();
}

void FLinkerSave::Serialize( void* V, int64 Length )
{
#if WITH_EDITOR
	Saver->ArDebugSerializationFlags = ArDebugSerializationFlags;
#endif
	Saver->Serialize( V, Length );
}
	

FString FLinkerSave::GetArchiveName() const
{
	return Saver->GetArchiveName();
}

FArchive& FLinkerSave::operator<<( FName& InName )
{
	int32 Save = MapName(InName);
	ensure(Save != INDEX_NONE);
	int32 Number = InName.GetNumber();
	FArchive& Ar = *this;
	return Ar << Save << Number;
}

FArchive& FLinkerSave::operator<<( UObject*& Obj )
{
	FPackageIndex Save;
	if (Obj)
	{
		Save = MapObject(Obj);
	}
	return *this << Save;
}

FArchive& FLinkerSave::operator<<(FLazyObjectPtr& LazyObjectPtr)
{
	FUniqueObjectGuid ID;
	ID = LazyObjectPtr.GetUniqueID();
	return *this << ID;
}

FArchive& FLinkerSave::operator<<(FSoftObjectPtr& SoftObjectPtr)
{
	FSoftObjectPath ID;
	UObject *Object = SoftObjectPtr.Get();

	if (Object)
	{
		// Use object in case name has changed. 
		ID = FSoftObjectPath(Object);
	}
	else
	{
		ID = SoftObjectPtr.GetUniqueID();
	}

	ID.Serialize(*this);
	return *this;
}
