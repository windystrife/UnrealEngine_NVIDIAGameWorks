// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Linker.cpp: Unreal object linker.
=============================================================================*/

#include "UObject/Linker.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "UObject/LinkerLoad.h"
#include "Misc/SecureHash.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "UObject/CoreRedirects.h"
#include "UObject/LinkerManager.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/DebugSerializationFlags.h"

DEFINE_LOG_CATEGORY(LogLinker);

#define LOCTEXT_NAMESPACE "Linker"

/*-----------------------------------------------------------------------------
	Helper functions.
-----------------------------------------------------------------------------*/
namespace Linker
{
	FORCEINLINE bool IsCorePackage(const FName& PackageName)
	{
		return PackageName == NAME_Core || PackageName == GLongCorePackageName;
	}
}

/**
 * Type hash implementation. 
 *
 * @param	Ref		Reference to hash
 * @return	hash value
 */
uint32 GetTypeHash( const FDependencyRef& Ref  )
{
	return PointerHash(Ref.Linker) ^ Ref.ExportIndex;
}

/*----------------------------------------------------------------------------
	FCompressedChunk.
----------------------------------------------------------------------------*/

FCompressedChunk::FCompressedChunk()
:	UncompressedOffset(0)
,	UncompressedSize(0)
,	CompressedOffset(0)
,	CompressedSize(0)
{
}

/** I/O function */
FArchive& operator<<(FArchive& Ar,FCompressedChunk& Chunk)
{
	Ar << Chunk.UncompressedOffset;
	Ar << Chunk.UncompressedSize;
	Ar << Chunk.CompressedOffset;
	Ar << Chunk.CompressedSize;
	return Ar;
}


/*----------------------------------------------------------------------------
	Items stored in Unreal files.
----------------------------------------------------------------------------*/

FGenerationInfo::FGenerationInfo(int32 InExportCount, int32 InNameCount)
: ExportCount(InExportCount), NameCount(InNameCount)
{}

/** I/O function
 * we use a function instead of operator<< so we can pass in the package file summary for version tests, since archive version hasn't been set yet
 */
void FGenerationInfo::Serialize(FArchive& Ar, const struct FPackageFileSummary& Summary)
{
	Ar << ExportCount << NameCount;
}

#if WITH_EDITORONLY_DATA
extern int32 GLinkerAllowDynamicClasses;
#endif

void FLinkerTables::SerializeSearchableNamesMap(FArchive& Ar)
{
#if WITH_EDITOR
	FArchive::FScopeSetDebugSerializationFlags S(Ar, DSF_IgnoreDiff, true);
#endif

	if (Ar.IsSaving())
	{
		// Sort before saving to keep order consistent
		SearchableNamesMap.KeySort(TLess<FPackageIndex>());

		for (TPair<FPackageIndex, TArray<FName> >& Pair : SearchableNamesMap)
		{
			Pair.Value.Sort();
		}
	}

	// Default Map serialize works fine
	Ar << SearchableNamesMap;
}

FName FLinker::GetExportClassName( int32 i )
{
	if (ExportMap.IsValidIndex(i))
	{
		FObjectExport& Export = ExportMap[i];
		if( !Export.ClassIndex.IsNull() )
		{
			return ImpExp(Export.ClassIndex).ObjectName;
		}
#if WITH_EDITORONLY_DATA
		else if (GLinkerAllowDynamicClasses && (Export.DynamicType == FObjectExport::EDynamicType::DynamicType))
		{
			static FName NAME_BlueprintGeneratedClass(TEXT("BlueprintGeneratedClass"));
			return NAME_BlueprintGeneratedClass;
		}
#else
		else if (Export.DynamicType == FObjectExport::EDynamicType::DynamicType)
		{
			return GetDynamicTypeClassName(*GetExportPathName(i));
		}
#endif
	}
	return NAME_Class;
}

/*----------------------------------------------------------------------------
	FLinker.
----------------------------------------------------------------------------*/
FLinker::FLinker(ELinkerType::Type InType, UPackage* InRoot, const TCHAR* InFilename)
: LinkerType(InType)
, LinkerRoot( InRoot )
, Filename( InFilename )
, FilterClientButNotServer(false)
, FilterServerButNotClient(false)
, ScriptSHA(nullptr)
{
	check(LinkerRoot);
	check(InFilename);

	if( !GIsClient && GIsServer)
	{
		FilterClientButNotServer = true;
	}
	if( GIsClient && !GIsServer)
	{
		FilterServerButNotClient = true;
	}
}

void FLinker::Serialize( FArchive& Ar )
{
	// This function is only used for counting memory, actual serialization uses a different path
	if( Ar.IsCountingMemory() )
	{
		// Can't use CountBytes as ExportMap is array of structs of arrays.
		Ar << ImportMap;
		Ar << ExportMap;
		Ar << DependsMap;
		Ar << SoftPackageReferenceList;
		Ar << GatherableTextDataMap;
		Ar << SearchableNamesMap;
	}
}

void FLinker::AddReferencedObjects(FReferenceCollector& Collector)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		Collector.AddReferencedObject(*(UObject**)&LinkerRoot);
	}
#endif
}

// FLinker interface.
/**
 * Return the path name of the UObject represented by the specified import. 
 * (can be used with StaticFindObject)
 * 
 * @param	ImportIndex	index into the ImportMap for the resource to get the name for
 *
 * @return	the path name of the UObject represented by the resource at ImportIndex
 */
FString FLinker::GetImportPathName(int32 ImportIndex)
{
	FString Result;
	for (FPackageIndex LinkerIndex = FPackageIndex::FromImport(ImportIndex); !LinkerIndex.IsNull();)
	{
		FObjectResource Resource = ImpExp(LinkerIndex);
		bool bSubobjectDelimiter=false;

		if (Result.Len() > 0 && GetClassName(LinkerIndex) != NAME_Package
			&& (Resource.OuterIndex.IsNull() || GetClassName(Resource.OuterIndex) == NAME_Package) )
		{
			bSubobjectDelimiter = true;
		}

		// don't append a dot in the first iteration
		if ( Result.Len() > 0 )
		{
			if ( bSubobjectDelimiter )
			{
				Result = FString(SUBOBJECT_DELIMITER) + Result;
			}
			else
			{
				Result = FString(TEXT(".")) + Result;
			}
		}

		Result = Resource.ObjectName.ToString() + Result;
		LinkerIndex = Resource.OuterIndex;
	}
	return Result;
}

/**
 * Return the path name of the UObject represented by the specified export.
 * (can be used with StaticFindObject)
 * 
 * @param	ExportIndex				index into the ExportMap for the resource to get the name for
 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
 * @param	bResolveForcedExports	if true, the package name part of the return value will be the export's original package,
 *									not the name of the package it's currently contained within.
 *
 * @return	the path name of the UObject represented by the resource at ExportIndex
 */
FString FLinker::GetExportPathName(int32 ExportIndex, const TCHAR* FakeRoot,bool bResolveForcedExports/*=false*/)
{
	FString Result;

	bool bForcedExport = false;
	for ( FPackageIndex LinkerIndex = FPackageIndex::FromExport(ExportIndex); !LinkerIndex.IsNull(); LinkerIndex = Exp(LinkerIndex).OuterIndex )
	{ 
		const FObjectExport Export = Exp(LinkerIndex);

		// don't append a dot in the first iteration
		if ( Result.Len() > 0 )
		{
			// if this export is not a UPackage but this export's Outer is a UPackage, we need to use subobject notation
			if ((Export.OuterIndex.IsNull() || GetExportClassName(Export.OuterIndex) == NAME_Package)
			  && GetExportClassName(LinkerIndex) != NAME_Package)
			{
				Result = FString(SUBOBJECT_DELIMITER) + Result;
			}
			else
			{
				Result = FString(TEXT(".")) + Result;
			}
		}
		Result = Export.ObjectName.ToString() + Result;
		bForcedExport = bForcedExport || Export.bForcedExport;
	}

	if ( bForcedExport && FakeRoot == NULL && bResolveForcedExports )
	{
		// Result already contains the correct path name for this export
		return Result;
	}

	return (FakeRoot ? FakeRoot : LinkerRoot->GetPathName()) + TEXT(".") + Result;
}

FString FLinker::GetImportFullName(int32 ImportIndex)
{
	return ImportMap[ImportIndex].ClassName.ToString() + TEXT(" ") + GetImportPathName(ImportIndex);
}

FString FLinker::GetExportFullName(int32 ExportIndex, const TCHAR* FakeRoot,bool bResolveForcedExports/*=false*/)
{
	FPackageIndex ClassIndex = ExportMap[ExportIndex].ClassIndex;
	FName ClassName = ClassIndex.IsNull() ? FName(NAME_Class) : ImpExp(ClassIndex).ObjectName;

	return ClassName.ToString() + TEXT(" ") + GetExportPathName(ExportIndex, FakeRoot, bResolveForcedExports);
}

/**
 * Tell this linker to start SHA calculations
 */
void FLinker::StartScriptSHAGeneration()
{
	// create it if needed
	if (ScriptSHA == NULL)
	{
		ScriptSHA = new FSHA1;
	}

	// make sure it's reset
	ScriptSHA->Reset();
}

/**
 * If generating a script SHA key, update the key with this script code
 *
 * @param ScriptCode Code to SHAify
 */
void FLinker::UpdateScriptSHAKey(const TArray<uint8>& ScriptCode)
{
	// if we are doing SHA, update it
	if (ScriptSHA && ScriptCode.Num())
	{
		ScriptSHA->Update((uint8*)ScriptCode.GetData(), ScriptCode.Num());
	}
}

/**
 * After generating the SHA key for all of the 
 *
 * @param OutKey Storage for the key bytes (20 bytes)
 */
void FLinker::GetScriptSHAKey(uint8* OutKey)
{
	check(ScriptSHA);

	// finish up the calculation, and return it
	ScriptSHA->Final();
	ScriptSHA->GetHash(OutKey);
}

FLinker::~FLinker()
{
	// free any SHA memory
	delete ScriptSHA;
}



/*-----------------------------------------------------------------------------
	Global functions
-----------------------------------------------------------------------------*/

void ResetLoaders(UObject* InPkg)
{
	if (IsAsyncLoading())
	{
		UE_LOG(LogLinker, Log, TEXT("ResetLoaders(%s) is flushing async loading"), *GetPathNameSafe(InPkg));
	}

	// Make sure we're not in the middle of loading something in the background.
	FlushAsyncLoading();
	FLinkerManager::Get().ResetLoaders(InPkg);
}

void DeleteLoaders()
{
	FLinkerManager::Get().DeleteLinkers();
}

void DeleteLoader(FLinkerLoad* Loader)
{
	FLinkerManager::Get().RemoveLinker(Loader);
}

static void LogGetPackageLinkerError(FArchive* LinkerArchive, const TCHAR* InFilename, const FText& InFullErrorMessage, const FText& InSummaryErrorMessage, UObject* InOuter, uint32 LoadFlags)
{
	static FName NAME_LoadErrors("LoadErrors");
	struct Local
	{
		/** Helper function to output more detailed error info if available */
		static void OutputErrorDetail(FArchive* InLinkerArchive, const FName& LogName)
		{
			FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
			if (ThreadContext.SerializedObject && ThreadContext.SerializedImportLinker)
			{
				FMessageLog LoadErrors(LogName);

				TSharedRef<FTokenizedMessage> Message = LoadErrors.Info();
				Message->AddToken(FTextToken::Create(LOCTEXT("FailedLoad_Message", "Failed to load")));
				Message->AddToken(FAssetNameToken::Create(ThreadContext.SerializedImportLinker->GetImportPathName(ThreadContext.SerializedImportIndex)));
				Message->AddToken(FTextToken::Create(LOCTEXT("FailedLoad_Referenced", "Referenced by")));
				Message->AddToken(FUObjectToken::Create(ThreadContext.SerializedObject));
				UProperty* SerializedProperty = InLinkerArchive ? InLinkerArchive->GetSerializedProperty() : nullptr;
				if (SerializedProperty != nullptr)
				{
					FString PropertyPathName = SerializedProperty->GetPathName();
					Message->AddToken(FTextToken::Create(LOCTEXT("FailedLoad_Property", "Property")));
					Message->AddToken(FAssetNameToken::Create(PropertyPathName, FText::FromString( PropertyPathName) ) );
				}
			}
		}
	};

	FMessageLog LoadErrors(NAME_LoadErrors);

	// Display log error regardless LoadFlag settings
	SET_WARN_COLOR(COLOR_RED);
	if (LoadFlags & LOAD_NoWarn)
	{
		UE_LOG(LogLinker, Log, TEXT("%s"), *InFullErrorMessage.ToString());
	}
	else 
	{
		UE_LOG(LogLinker, Warning, TEXT("%s"), *InFullErrorMessage.ToString());
	}
	CLEAR_WARN_COLOR();
	if( GIsEditor && !IsRunningCommandlet() )
	{
		// if we don't want to be warned, skip the load warning
		if (!(LoadFlags & LOAD_NoWarn))
		{
			// we only want to output errors that content creators will be able to make sense of,
			// so any errors we cant get links out of we will just let be output to the output log (above)
			// rather than clog up the message log

			if(InFilename != NULL && InOuter != NULL)
			{
				// Output the summary error & the filename link. This might be something like "..\Content\Foo.upk Out of Memory"
				TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
				Message->AddToken(FAssetNameToken::Create(FPackageName::FilenameToLongPackageName(InFilename)));
				Message->AddToken(FTextToken::Create(FText::FromString(TEXT(":"))));
				Message->AddToken(FTextToken::Create(InSummaryErrorMessage));
				Message->AddToken(FAssetNameToken::Create(FPackageName::FilenameToLongPackageName(InOuter->GetPathName())));
			}

			Local::OutputErrorDetail(LinkerArchive, NAME_LoadErrors);
		}
	}
	else
	{
		if (!(LoadFlags & LOAD_NoWarn))
		{
			Local::OutputErrorDetail(LinkerArchive, NAME_LoadErrors);
		}

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("FileName"), FText::FromString(InFilename ? InFilename : InOuter ? *InOuter->GetName() : TEXT("NULL")));
		Arguments.Add(TEXT("ErrorMessage"), InFullErrorMessage);
		const FText Error = FText::Format(LOCTEXT("FailedLoad", "Failed to load '{FileName}': {ErrorMessage}"), Arguments);

		// @see ResavePackagesCommandlet
		if( FParse::Param(FCommandLine::Get(),TEXT("SavePackagesThatHaveFailedLoads")) == true )
		{
			LoadErrors.Warning(Error);
		}
		else
		{
			// Gracefully handle missing packages
			SafeLoadError(InOuter, LoadFlags, *Error.ToString());
		}
	}
}

static void LogGetPackageLinkerError(FLinkerLoad* Linker, const TCHAR* InFilename, const FText& InFullErrorMessage, const FText& InSummaryErrorMessage, UObject* InOuter, uint32 LoadFlags)
{
	LogGetPackageLinkerError(Linker ? Linker->Loader : nullptr, InFilename, InFullErrorMessage, InSummaryErrorMessage, InOuter, LoadFlags);
}

/** Customized version of FPackageName::DoesPackageExist that takes dynamic native class packages into account */
static bool DoesPackageExistForGetPackageLinker(const FString& LongPackageName, const FGuid* Guid, FString& OutFilename)
{
	if (
#if WITH_EDITORONLY_DATA
		GLinkerAllowDynamicClasses && 
#endif
		GetConvertedDynamicPackageNameToTypeName().Contains(*LongPackageName))
	{
		OutFilename = FPackageName::LongPackageNameToFilename(LongPackageName);
		return true;
	}
	else
	{
		return FPackageName::DoesPackageExist(LongPackageName, Guid, &OutFilename);
	}
}

FString GetPrestreamPackageLinkerName(const TCHAR* InLongPackageName, bool bExistSkip)
{
	FString NewFilename;
	if (InLongPackageName)
	{
		FString PackageName(InLongPackageName);
		if (!FPackageName::TryConvertFilenameToLongPackageName(InLongPackageName, PackageName))
		{
			return FString();
		}
		UPackage* ExistingPackage = bExistSkip ? FindObject<UPackage>(nullptr, *PackageName) : nullptr;
		if (ExistingPackage)
		{
			return FString(); // we won't load this anyway, don't prestream
		}
		
		const bool DoesNativePackageExist = DoesPackageExistForGetPackageLinker(PackageName, nullptr, NewFilename);

		if ( !DoesNativePackageExist )
		{
			return FString();
		}
	}
	return NewFilename;
}

//
// Find or create the linker for a package.
//
FLinkerLoad* GetPackageLinker
(
	UPackage*		InOuter,
	const TCHAR*	InLongPackageName,
	uint32			LoadFlags,
	UPackageMap*	Sandbox,
	FGuid*			CompatibleGuid
)
{
	// See if there is already a linker for this package.
	FLinkerLoad* Result = FLinkerLoad::FindExistingLinkerForPackage(InOuter);

	// Try to load the linker.
	// See if the linker is already loaded.
	if (Result)
	{
		return Result;
	}

	UPackage* CreatedPackage = nullptr;

	FString NewFilename;
	if( !InLongPackageName )
	{
		// Resolve filename from package name.
		if( !InOuter )
		{
			// try to recover from this instead of throwing, it seems recoverable just by doing this
			FText ErrorText(LOCTEXT("PackageResolveFailed", "Can't resolve asset name"));
			LogGetPackageLinkerError(Result, InLongPackageName, ErrorText, ErrorText, InOuter, LoadFlags);
			return nullptr;
		}
	
		// Allow delegates to resolve this package
		FString PackageNameToCreate = InOuter->GetName();

		// Process any package redirects
		{
			const FCoreRedirectObjectName NewPackageName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, *PackageNameToCreate));
			PackageNameToCreate = NewPackageName.PackageName.ToString();
		}

		// Do not resolve packages that are in memory
		if (!InOuter->HasAnyPackageFlags(PKG_InMemoryOnly))
		{
			PackageNameToCreate = FPackageName::GetDelegateResolvedPackagePath(PackageNameToCreate);
		}

		// The editor must not redirect packages for localization. We also shouldn't redirect script or in-memory packages.
		FString PackageNameToLoad = PackageNameToCreate;
		if (!(GIsEditor || InOuter->HasAnyPackageFlags(PKG_InMemoryOnly) || FPackageName::IsScriptPackage(PackageNameToLoad)))
		{
			PackageNameToLoad = FPackageName::GetLocalizedPackagePath(PackageNameToLoad);
		}

		// Verify that the file exists.
		const bool DoesPackageExist = DoesPackageExistForGetPackageLinker(PackageNameToLoad, CompatibleGuid, NewFilename);
		if ( !DoesPackageExist )
		{
			// In memory-only packages have no linker and this is ok.
			if (!(LoadFlags & LOAD_AllowDll) && !InOuter->HasAnyPackageFlags(PKG_InMemoryOnly) && !FLinkerLoad::IsKnownMissingPackage(InOuter->GetFName()))
			{
				FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("AssetName"), FText::FromString(PackageNameToLoad));
				Arguments.Add(TEXT("PackageName"), FText::FromString(ThreadContext.SerializedPackageLinker ? *(ThreadContext.SerializedPackageLinker->Filename) : TEXT("NULL")));
				LogGetPackageLinkerError(Result, ThreadContext.SerializedPackageLinker ? *ThreadContext.SerializedPackageLinker->Filename : nullptr,
											FText::Format(LOCTEXT("PackageNotFound", "Can't find file for asset '{AssetName}' while loading {PackageName}."), Arguments),
											LOCTEXT("PackageNotFoundShort", "Can't find file for asset."),
											InOuter,
											LoadFlags);
			}

			return nullptr;
		}
	}
	else
	{
		FString PackageNameToCreate;
		if (!FPackageName::TryConvertFilenameToLongPackageName(InLongPackageName, PackageNameToCreate))
		{
			// try to recover from this instead of throwing, it seems recoverable just by doing this
			FText ErrorText(LOCTEXT("PackageResolveFailed", "Can't resolve asset name"));
			LogGetPackageLinkerError(Result, InLongPackageName, ErrorText, ErrorText, InOuter, LoadFlags);
			return nullptr;
		}

		// Process any package redirects
		{
			const FCoreRedirectObjectName NewPackageName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, *PackageNameToCreate));
			PackageNameToCreate = NewPackageName.PackageName.ToString();
		}

		// Allow delegates to resolve this path
		PackageNameToCreate = FPackageName::GetDelegateResolvedPackagePath(PackageNameToCreate);

		// The editor must not redirect packages for localization. We also shouldn't redirect script packages.
		FString PackageNameToLoad = PackageNameToCreate;
		if (!(GIsEditor || FPackageName::IsScriptPackage(PackageNameToLoad)))
		{
			PackageNameToLoad = FPackageName::GetLocalizedPackagePath(PackageNameToLoad);
		}

		UPackage* ExistingPackage = FindObject<UPackage>(nullptr, *PackageNameToCreate);
		if (ExistingPackage)
		{
			if (!ExistingPackage->GetOuter() && ExistingPackage->HasAnyPackageFlags(PKG_InMemoryOnly))
			{
				// This is a memory-only in package and so it has no linker and this is ok.
				return nullptr;
			}
		}

		// Verify that the file exists.
		const bool DoesPackageExist = DoesPackageExistForGetPackageLinker(PackageNameToLoad, CompatibleGuid, NewFilename);
		if( !DoesPackageExist )
		{
			if (!FLinkerLoad::IsKnownMissingPackage(InLongPackageName))
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Filename"), FText::FromString(InLongPackageName));

				// try to recover from this instead of throwing, it seems recoverable just by doing this
				LogGetPackageLinkerError(Result, InLongPackageName, FText::Format(LOCTEXT("FileNotFound", "Can't find file '{Filename}'"), Arguments), LOCTEXT("FileNotFoundShort", "Can't find file"), InOuter, LoadFlags);
			}
			return nullptr;
		}

#if WITH_EDITORONLY_DATA
		// Make sure the package name matches the name on disk
		FPackageName::FixPackageNameCase(PackageNameToCreate, FPaths::GetExtension(NewFilename));
#endif

		// Create the package with the provided long package name.
		UPackage* FilenamePkg = (ExistingPackage ? ExistingPackage : (CreatedPackage = CreatePackage(nullptr, *PackageNameToCreate)));
		if (FilenamePkg && FilenamePkg != ExistingPackage && (LoadFlags & LOAD_PackageForPIE))
		{
			check(FilenamePkg);
			FilenamePkg->SetPackageFlags(PKG_PlayInEditor);
		}

		// If no package specified, use package from file.
		if (!InOuter)
		{
			if( !FilenamePkg )
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Filename"), FText::FromString(InLongPackageName));
				LogGetPackageLinkerError(Result, InLongPackageName, FText::Format(LOCTEXT("FilenameToPackage", "Can't convert filename '{Filename}' to asset name"), Arguments), LOCTEXT("FilenameToPackageShort", "Can't convert filename to asset name"), InOuter, LoadFlags);
				return nullptr;
			}
			InOuter = FilenamePkg;
			Result = FLinkerLoad::FindExistingLinkerForPackage(InOuter);
		}
		else if (InOuter != FilenamePkg) //!!should be tested and validated in new UnrealEd
		{
			// Loading a new file into an existing package, so reset the loader.
			//UE_LOG(LogLinker, Log,  TEXT("New File, Existing Package (%s, %s)"), *InOuter->GetFullName(), *FilenamePkg->GetFullName() );
			ResetLoaders( InOuter );
		}
	}

#if 0
	// Make sure the package is accessible in the sandbox.
	if( Sandbox && !Sandbox->SupportsPackage(InOuter) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("AssetName"), FText::FromString(InOuter->GetName()));

		LogGetPackageLinkerError(Result, InLongPackageName, FText::Format(LOCTEXT("Sandbox", "Asset '{AssetName}' is not accessible in this sandbox"), Arguments), LOCTEXT("SandboxShort", "Asset is not accessible in this sandbox"), InOuter, LoadFlags);
		return nullptr;
	}
#endif

	// Create new linker.
	if( !Result )
	{
		check(IsLoading());

		// we will already have found the filename above
		check(NewFilename.Len() > 0);

		Result = FLinkerLoad::CreateLinker( InOuter, *NewFilename, LoadFlags );
	}

	if ( !Result && CreatedPackage )
	{
		// kill it with fire
		CreatedPackage->MarkPendingKill();
		/*static int32 FailedPackageLoadIndex = 0;
		++FailedPackageLoadIndex;
		FString FailedLinkerLoad = FString::Printf(TEXT("/Temp/FailedLinker_%s_%d"), *NewFilename, FailedPackageLoadIndex);
		
		CreatedPackage->Rename(*FailedLinkerLoad);*/
	}

	// Verify compatibility.
	if (Result && CompatibleGuid && Result->Summary.Guid != *CompatibleGuid)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("AssetName"), FText::FromString(InOuter->GetName()));

		// This should never fire, because FindPackageFile should never return an incompatible file
		LogGetPackageLinkerError(Result, InLongPackageName, FText::Format(LOCTEXT("PackageVersion", "Asset '{AssetName}' version mismatch"), Arguments), LOCTEXT("PackageVersionShort", "Asset version mismatch"), InOuter, LoadFlags);
		return nullptr;
	}

	return Result;
}

/**
 * 
 * Ensure thumbnails are loaded and then reset the loader in preparation for a package save
 *
 * @param	InOuter			The outer for the package we are saving
 * @param	Filename		The filename we are saving too
 */
void ResetLoadersForSave(UObject* InOuter, const TCHAR *Filename)
{
	UPackage* Package = dynamic_cast<UPackage*>(InOuter);
	// If we have a loader for the package, unload it to prevent conflicts if we are resaving to the same filename
	FLinkerLoad* Loader = FLinkerLoad::FindExistingLinkerForPackage(Package);
	// This is the loader corresponding to the package we're saving.
	if( Loader )
	{
		// Before we save the package, make sure that we load up any thumbnails that aren't already
		// in memory so that they won't be wiped out during this save
		Loader->SerializeThumbnails();

		// Compare absolute filenames to see whether we're trying to save over an existing file.
		if( FPaths::ConvertRelativePathToFull(Filename) == FPaths::ConvertRelativePathToFull( Loader->Filename ) )
		{
			// Detach all exports from the linker and dissociate the linker.
			ResetLoaders( InOuter );
		}
	}
}

#undef LOCTEXT_NAMESPACE
