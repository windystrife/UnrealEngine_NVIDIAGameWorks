// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/LinkerLoad.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Stats/StatsMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Misc/SlowTask.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/App.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/ObjectRedirector.h"
#include "Serialization/ArchiveAsync.h"
#include "Misc/PackageName.h"
#include "Blueprint/BlueprintSupport.h"
#include "Misc/SecureHash.h"
#include "ProfilingDebugging/DebuggingDefines.h"
#include "Logging/TokenizedMessage.h"
#include "UObject/LinkerPlaceholderBase.h"
#include "UObject/LinkerPlaceholderClass.h"
#include "UObject/LinkerPlaceholderExportObject.h"
#include "UObject/LinkerPlaceholderFunction.h"
#include "UObject/LinkerManager.h"
#include "Serialization/DeferredMessageLog.h"
#include "UObject/UObjectThreadContext.h"
#include "Serialization/AsyncLoading.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "HAL/ThreadHeartBeat.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Serialization/BulkData.h"
#include "Serialization/AsyncLoadingPrivate.h"
#include "UObject/CoreRedirects.h"

class FTexture2DResourceMem;

#define LOCTEXT_NAMESPACE "LinkerLoad"

DECLARE_STATS_GROUP_VERBOSE(TEXT("Linker Load"), STATGROUP_LinkerLoad, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("Linker Preload"),STAT_LinkerPreload,STATGROUP_LinkerLoad);
DECLARE_CYCLE_STAT(TEXT("Linker Precache"),STAT_LinkerPrecache,STATGROUP_LinkerLoad);
DECLARE_CYCLE_STAT(TEXT("Linker Serialize"),STAT_LinkerSerialize,STATGROUP_LinkerLoad);
DECLARE_CYCLE_STAT(TEXT("Linker Load Deferred"), STAT_LinkerLoadDeferred, STATGROUP_LinkerLoad);

DECLARE_STATS_GROUP( TEXT( "Linker Count" ), STATGROUP_LinkerCount, STATCAT_Advanced );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Linker Count"), STAT_LinkerCount, STATGROUP_LinkerCount);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Live Linker Count"), STAT_LiveLinkerCount, STATGROUP_LinkerCount);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Fixup editor-only flags time"), STAT_EditorOnlyFixupTime, STATGROUP_LinkerCount);

#if WITH_EDITORONLY_DATA
int32 GLinkerAllowDynamicClasses = 0;
static FAutoConsoleVariableRef CVarLinkerAllowDynamicClasses(
	TEXT("linker.AllowDynamicClasses"),
	GLinkerAllowDynamicClasses,
	TEXT("If true, linkers will attempt to use dynamic classes instead of class assets."),
	ECVF_Default
	);
#endif

UClass* FLinkerLoad::UTexture2DStaticClass = NULL;

FName FLinkerLoad::NAME_LoadErrors("LoadErrors");


/*----------------------------------------------------------------------------
Helpers
----------------------------------------------------------------------------*/

/**
* Test whether the given package index is a valid import or export in this package
*/
bool FLinkerLoad::IsValidPackageIndex(FPackageIndex InIndex)
{
	return (InIndex.IsImport() && ImportMap.IsValidIndex(InIndex.ToImport()))
		|| (InIndex.IsExport() && ExportMap.IsValidIndex(InIndex.ToExport()));
}

bool FLinkerLoad::bActiveRedirectsMapInitialized = false;



/**
* DEPRECATED: Replace with FCoreRedirects format for newly added ini entries
*
* Here is the format for the ClassRedirection:
*
*  ; Basic redirects
*  ;ActiveClassRedirects=(OldClassName="MyClass",NewClassName="NewNativePackage.MyClass")
*	ActiveClassRedirects=(OldClassName="CylinderComponent",NewClassName="CapsuleComponent")
*  Note: For class name redirects, the OldClassName must be the plain OldClassName, it cannot be OldPackage.OldClassName
*
*	; Keep both classes around, but convert any existing instances of that object to a particular class (insert into the inheritance hierarchy
*	;ActiveClassRedirects=(OldClassName="MyClass",NewClassName="MyClassParent",InstanceOnly="true")
*
*/

void FLinkerLoad::CreateActiveRedirectsMap(const FString& GEngineIniName)
{
	// Soft deprecated, replaced by FCoreRedirects, but it will still read the old format for the foreseeable future

	// mark that this has been done at least once
	bActiveRedirectsMapInitialized = true;

	if (GConfig)
	{
		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate( TEXT("/Script/Engine.Engine"), false, true, GEngineIniName );
		if (PackageRedirects)
		{
			TArray<FCoreRedirect> NewRedirects;
			FDeferredMessageLog RedirectErrors(NAME_LoadErrors);

			static FName ActiveClassRedirectsKey(TEXT("ActiveClassRedirects"));
			for( FConfigSection::TIterator It(*PackageRedirects); It; ++It )
			{
				if (It.Key() == ActiveClassRedirectsKey)
				{
					FName OldClassName = NAME_None;
					FName NewClassName = NAME_None;
					FName ObjectName = NAME_None;
					FName OldSubobjName = NAME_None;
					FName NewSubobjName = NAME_None;
					FName NewClassClass = NAME_None;
					FName NewClassPackage = NAME_None;

					bool bInstanceOnly = false;

					FParse::Bool( *It.Value().GetValue(), TEXT("InstanceOnly="), bInstanceOnly );
					FParse::Value( *It.Value().GetValue(), TEXT("ObjectName="), ObjectName );

					FParse::Value( *It.Value().GetValue(), TEXT("OldClassName="), OldClassName );
					FParse::Value( *It.Value().GetValue(), TEXT("NewClassName="), NewClassName );

					FParse::Value( *It.Value().GetValue(), TEXT("OldSubobjName="), OldSubobjName );
					FParse::Value( *It.Value().GetValue(), TEXT("NewSubobjName="), NewSubobjName );

					FParse::Value( *It.Value().GetValue(), TEXT("NewClassClass="), NewClassClass );
					FParse::Value( *It.Value().GetValue(), TEXT("NewClassPackage="), NewClassPackage );

					if (NewSubobjName != NAME_None || OldSubobjName != NAME_None)
					{
						check(OldSubobjName != NAME_None && OldClassName != NAME_None );
						FCoreRedirect* Redirect = new (NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Class, OldClassName.ToString(), OldClassName.ToString());
						Redirect->ValueChanges.Add(OldSubobjName.ToString(), NewSubobjName.ToString());
					}
					//instances only
					else if( bInstanceOnly )
					{
						// If NewClassName is none, register as removed instead
						if (NewClassName == NAME_None)
						{
							FCoreRedirect* Redirect = new (NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Option_InstanceOnly | ECoreRedirectFlags::Option_Removed, OldClassName.ToString(), NewClassName.ToString());
						}
						else
						{
							FCoreRedirect* Redirect = new (NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Option_InstanceOnly, OldClassName.ToString(), NewClassName.ToString());
						}
					}
					//objects only on a per-object basis
					else if( ObjectName != NAME_None )
					{
						UE_LOG(LogLinker, Warning, TEXT("Generic Object redirects are not supported with ActiveClassRedirects and never worked, move to new CoreRedirects system"));
					}
					//full redirect
					else
					{
						if (NewClassName.ToString().Find(TEXT("."), ESearchCase::CaseSensitive) != NewClassName.ToString().Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
						{
							RedirectErrors.Error(FText::Format(LOCTEXT("NestedRenameDisallowed", "{0} cannot contain a rename of nested objects for '{1}'; if you want to leave the outer alone, just specify the name with no path"), FText::FromName(ActiveClassRedirectsKey), FText::FromName(NewClassName)));
						}
						else
						{
							FCoreRedirect* Redirect = new(NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Class, OldClassName.ToString(), NewClassName.ToString());

							if (!NewClassClass.IsNone() || !NewClassPackage.IsNone())
							{
								Redirect->OverrideClassName = FCoreRedirectObjectName(NewClassClass, NAME_None, NewClassPackage);
							}
							else if (Redirect->NewName.ObjectName.ToString().StartsWith(TEXT("E"), ESearchCase::CaseSensitive))
							{
								// This might be an enum, so we have to register it
								FCoreRedirect* EnumRedirect = new(NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Enum, OldClassName.ToString(), NewClassName.ToString());
							}
							else
							{
								// This might be a struct redirect because many of them were registered incorrectly
								FCoreRedirect* StructRedirect = new(NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Struct, OldClassName.ToString(), NewClassName.ToString());
							}
						}
					}
				}	
				else if( It.Key() == TEXT("ActiveGameNameRedirects") )
				{
					FName OldGameName = NAME_None;
					FName NewGameName = NAME_None;

					FParse::Value( *It.Value().GetValue(), TEXT("OldGameName="), OldGameName );
					FParse::Value( *It.Value().GetValue(), TEXT("NewGameName="), NewGameName );

					FCoreRedirect* Redirect = new(NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Package, OldGameName.ToString(), NewGameName.ToString());
				}
				else if ( It.Key() == TEXT("ActiveStructRedirects") )
				{
					FName OldStructName = NAME_None;
					FName NewStructName = NAME_None;

					FParse::Value( *It.Value().GetValue(), TEXT("OldStructName="), OldStructName );
					FParse::Value( *It.Value().GetValue(), TEXT("NewStructName="), NewStructName );

					FCoreRedirect* Redirect = new(NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Struct, OldStructName.ToString(), NewStructName.ToString());
				}
				else if ( It.Key() == TEXT("ActivePluginRedirects") )
				{
					FString OldPluginName;
					FString NewPluginName;

					FParse::Value( *It.Value().GetValue(), TEXT("OldPluginName="), OldPluginName );
					FParse::Value( *It.Value().GetValue(), TEXT("NewPluginName="), NewPluginName );

					OldPluginName = FString(TEXT("/")) + OldPluginName + FString(TEXT("/"));
					NewPluginName = FString(TEXT("/")) + NewPluginName + FString(TEXT("/"));

					FCoreRedirect* Redirect = new(NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_MatchSubstring, OldPluginName, NewPluginName);
				}
				else if ( It.Key() == TEXT("KnownMissingPackages") )
				{
					FName KnownMissingPackage = NAME_None;

					FParse::Value( *It.Value().GetValue(), TEXT("PackageName="), KnownMissingPackage );

					FCoreRedirect* Redirect = new(NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_Removed, KnownMissingPackage.ToString(), FString());
				}
				else if (It.Key() == TEXT("TaggedPropertyRedirects"))
				{
					FName ClassName = NAME_None;
					FName OldPropertyName = NAME_None;
					FName NewPropertyName = NAME_None;

					FParse::Value(*It.Value().GetValue(), TEXT("ClassName="), ClassName);
					FParse::Value(*It.Value().GetValue(), TEXT("OldPropertyName="), OldPropertyName);
					FParse::Value(*It.Value().GetValue(), TEXT("NewPropertyName="), NewPropertyName);

					check(ClassName != NAME_None && OldPropertyName != NAME_None && NewPropertyName != NAME_None);

					FCoreRedirect* Redirect = new (NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Property, FCoreRedirectObjectName(OldPropertyName, ClassName, NAME_None), FCoreRedirectObjectName(NewPropertyName, ClassName, NAME_None));
				}
				else if (It.Key() == TEXT("EnumRedirects"))
				{
					const FString& ConfigValue = It.Value().GetValue();
					FName EnumName = NAME_None;
					FName OldEnumEntry = NAME_None;
					FName NewEnumEntry = NAME_None;

					FString OldEnumSubstring;
					FString NewEnumSubstring;

					FParse::Value(*ConfigValue, TEXT("EnumName="), EnumName);
					if (FParse::Value(*ConfigValue, TEXT("OldEnumEntry="), OldEnumEntry))
					{
						FParse::Value(*ConfigValue, TEXT("NewEnumEntry="), NewEnumEntry);
						check(EnumName != NAME_None && OldEnumEntry != NAME_None && NewEnumEntry != NAME_None);
						FCoreRedirect* Redirect = new(NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Enum, EnumName.ToString(), EnumName.ToString());
						Redirect->ValueChanges.Add(OldEnumEntry.ToString(), NewEnumEntry.ToString());
					}
					else if (FParse::Value(*ConfigValue, TEXT("OldEnumSubstring="), OldEnumSubstring))
					{
						UE_LOG(LogLinker, Warning, TEXT("OldEnumSubstring no longer supported! Replace with multiple entries or use the better syntax in the CoreRedirects section "));
					}
				}
			}

			FCoreRedirects::AddRedirectList(NewRedirects, GEngineIniName);
		}
	}
	else
	{
		UE_LOG(LogLinker, Warning, TEXT(" **** ACTIVE CLASS REDIRECTS UNABLE TO INITIALIZE! (mActiveClassRedirects) **** "));
	}
}

FScopedCreateImportCounter::FScopedCreateImportCounter(FLinkerLoad* Linker, int32 Index)
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	// Remember the old linker and index
	PreviousLinker = ThreadContext.SerializedImportLinker;
	PreviousIndex = ThreadContext.SerializedImportIndex;
	// Remember the current linker and index.
	ThreadContext.SerializedImportLinker = Linker;
	ThreadContext.SerializedImportIndex = Index;
}

FScopedCreateImportCounter::~FScopedCreateImportCounter()
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	// Restore old values
	ThreadContext.SerializedImportLinker = PreviousLinker;
	ThreadContext.SerializedImportIndex = PreviousIndex;
}


/** Helper struct to keep track of the CreateExport() entry/exit. */
struct FScopedCreateExportCounter
{
	/**
	 *	Constructor. Called upon CreateImport() entry.
	 *	@param Linker	- Current Linker
	 *	@param Index	- Index of the current Import
	 */
	FScopedCreateExportCounter(FLinkerLoad* Linker, int32 Index)
	{
		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		// Remember the old linker and index
		PreviousLinker = ThreadContext.SerializedExportLinker;
		PreviousIndex = ThreadContext.SerializedExportIndex;
		// Remember the current linker and index.
		ThreadContext.SerializedExportLinker = Linker;
		ThreadContext.SerializedExportIndex = Index;
	}

	/** Destructor. Called upon CreateImport() exit. */
	~FScopedCreateExportCounter()
	{
		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
		// Restore old values
		ThreadContext.SerializedExportLinker = PreviousLinker;
		ThreadContext.SerializedExportIndex = PreviousIndex;
	}

	/** Poreviously stored linker */
	FLinkerLoad* PreviousLinker;
	/** Previously stored index */
	int32 PreviousIndex;
};

/**
 * Exception save guard to ensure SerializedPackageLinker is reset after this
 * class goes out of scope.
 */
class FSerializedPackageLinkerGuard
{
	/** Pointer to restore to after going out of scope. */
	FLinkerLoad* PrevSerializedPackageLinker;
public:
	FSerializedPackageLinkerGuard() 
		: PrevSerializedPackageLinker(FUObjectThreadContext::Get().SerializedPackageLinker)
	{}
	~FSerializedPackageLinkerGuard() 
	{ 
		FUObjectThreadContext::Get().SerializedPackageLinker = PrevSerializedPackageLinker;
	}
};

namespace FLinkerDefs
{
	/** Number of progress steps for reporting status to a GUI while loading packages */
	const int32 TotalProgressSteps = 5;
}

/**
 * Creates a platform-specific ResourceMem. If an AsyncCounter is provided, it will allocate asynchronously.
 *
 * @param SizeX				Width of the stored largest mip-level
 * @param SizeY				Height of the stored largest mip-level
 * @param NumMips			Number of stored mips
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 * @param AsyncCounter		If specified, starts an async allocation. If NULL, allocates memory immediately.
 * @return					Platform-specific ResourceMem.
 */
static FTexture2DResourceMem* CreateResourceMem(int32 SizeX, int32 SizeY, int32 NumMips, uint32 Format, uint32 TexCreateFlags, FThreadSafeCounter* AsyncCounter)
{
	FTexture2DResourceMem* ResourceMem = NULL;
	return ResourceMem;
}

static inline int32 HashNames(FName Object, FName Class, FName Package)
{
	return Object.GetComparisonIndex() + 7 * Class.GetComparisonIndex() + 31 * FPackageName::GetShortFName(Package).GetComparisonIndex();
}

static FORCEINLINE bool IsCoreUObjectPackage(const FName& PackageName)
{
	return PackageName == NAME_CoreUObject || PackageName == GLongCoreUObjectPackageName || PackageName == NAME_Core || PackageName == GLongCorePackageName;
}

/*----------------------------------------------------------------------------
	FLinkerLoad.
----------------------------------------------------------------------------*/

void FLinkerLoad::StaticInit(UClass* InUTexture2DStaticClass)
{
	UTexture2DStaticClass = InUTexture2DStaticClass;
}

/**
 * Creates and returns a FLinkerLoad object.
 *
 * @param	Parent		Parent object to load into, can be NULL (most likely case)
 * @param	Filename	Name of file on disk to load
 * @param	LoadFlags	Load flags determining behavior
 *
 * @return	new FLinkerLoad object for Parent/ Filename
 */
FLinkerLoad* FLinkerLoad::CreateLinker(UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	// we don't want the linker permanently created with the 
	// DeferDependencyLoads flag (we also want to be able to determine if the 
	// linker already exists with that flag), so clear it before we attempt 
	// CreateLinkerAsync()
	// 
	// if this flag is present here, then we're most likely in a nested load and a 
	// blueprint up the load chain needed an asset (most likely a user-defined 
	// struct) loaded (we expect calls with LOAD_DeferDependencyLoads to be 
	// coming from LoadPackageInternal)
	uint32 const DeferredLoadFlag = (LoadFlags & LOAD_DeferDependencyLoads);
	LoadFlags &= ~LOAD_DeferDependencyLoads;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	FLinkerLoad* Linker = CreateLinkerAsync(Parent, Filename, LoadFlags
		, TFunction<void()>([](){})
		);
	{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
		// the linker could already have the DeferDependencyLoads flag present 
		// (if this linker was already created further up the load chain, and 
		// we're re-entering this to further finalize its creation)... we want 
		// to make sure the DeferDependencyLoads flag is supplied (if it was 
		// specified) for the duration of the Tick() below, because its call to 
		// FinalizeCreation() could invoke further dependency loads
		TGuardValue<uint32> LinkerLoadFlagGuard(Linker->LoadFlags, Linker->LoadFlags | DeferredLoadFlag);
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

		FSerializedPackageLinkerGuard Guard;
		FUObjectThreadContext::Get().SerializedPackageLinker = Linker;
		if (Linker->Tick(0.f, false, false) == LINKER_Failed)
		{
			return NULL;
		}
	}
	FCoreUObjectDelegates::PackageCreatedForLoad.Broadcast(Parent);
	return Linker;
}

/**
 * Looks for an existing linker for the given package, without trying to make one if it doesn't exist
 */
FLinkerLoad* FLinkerLoad::FindExistingLinkerForPackage(const UPackage* Package)
{
	FLinkerLoad* Linker = nullptr;
	if (Package)
	{
		Linker = Package->LinkerLoad;
	}
	return Linker;
}

FLinkerLoad* FLinkerLoad::FindExistingLinkerForImport(int32 Index) const
{
	const FObjectImport& Import = ImportMap[Index];
	if (Import.SourceLinker != nullptr)
	{
		return Import.SourceLinker;
	}
	else if (Import.XObject != nullptr)
	{
		if (FLinkerLoad* ObjLinker = Import.XObject->GetLinker())
		{
			return ObjLinker;
		}
	}

	FLinkerLoad* FoundLinker = nullptr;
	if (Import.OuterIndex.IsNull() && (Import.ClassName == NAME_Package))
	{
		FString PackageName = Import.ObjectName.ToString();
		if (UPackage* FoundPackage = FindObject<UPackage>(/*Outer =*/nullptr, *PackageName))
		{
			FoundLinker = FLinkerLoad::FindExistingLinkerForPackage(FoundPackage);
		}
	}
	else if (Import.OuterIndex.IsImport())
	{
		FoundLinker = FindExistingLinkerForImport(Import.OuterIndex.ToImport());
	}
	return FoundLinker;
}

/**
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * CAUTION:  This function is potentially DANGEROUS.  Should only be used when you're really, really sure you know what you're doing.
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 * Replaces OldObject's entry in its linker with NewObject, so that all subsequent loads of OldObject will return NewObject.
 * This is used to update instanced components that were serialized out, but regenerated during compile-on-load
 *
 * OldObject will be consigned to oblivion, and NewObject will take its place.
 *
 * WARNING!!!	This function is potentially very dangerous!  It should only be used at very specific times, and in very specific cases.
 *				If you're unsure, DON'T TRY TO USE IT!!!
 */
void FLinkerLoad::PRIVATE_PatchNewObjectIntoExport(UObject* OldObject, UObject* NewObject)
{
	// Cache off the old object's linker and export index.  We'll slide the new object in here.
	FLinkerLoad* OldObjectLinker = OldObject->GetLinker();
	// if this thing doesn't have a linker, then it wasn't loaded off disk and all of this is moot
	if (OldObjectLinker)
	{
		const int32 CachedLinkerIndex = OldObject->GetLinkerIndex();
		FObjectExport& ObjExport = OldObjectLinker->ExportMap[CachedLinkerIndex];

		// Detach the old object to make room for the new
		const EObjectFlags OldObjectFlags = OldObject->GetFlags();
		OldObject->ClearFlags(RF_NeedLoad|RF_NeedPostLoad);
		OldObject->SetLinker(nullptr, INDEX_NONE, true);

		// Copy flags from the old CDO.
		NewObject->SetFlags(OldObjectFlags);

		// Move the new object into the old object's slot, so any references to this object will now reference the new
		NewObject->SetLinker(OldObjectLinker, CachedLinkerIndex);
		ObjExport.Object = NewObject;

		TArray<UObject*>& ObjLoaded = FUObjectThreadContext::Get().ObjLoaded;
		// If the object was in the ObjLoaded queue (exported, but not yet serialized), swap out for our new object
		const int32 ObjLoadedIdx = ObjLoaded.Find(OldObject);
		if( ObjLoadedIdx != INDEX_NONE )
		{
			ObjLoaded[ObjLoadedIdx] = NewObject;
		}
	}
}

void FLinkerLoad::InvalidateExport(UObject* OldObject)
{
	FLinkerLoad* OldObjectLinker = OldObject->GetLinker();
	const int32 CachedLinkerIndex = OldObject->GetLinkerIndex();

	if (OldObjectLinker && OldObjectLinker->ExportMap.IsValidIndex(CachedLinkerIndex))
	{
		FObjectExport& ObjExport = OldObjectLinker->ExportMap[CachedLinkerIndex];
		ObjExport.bExportLoadFailed = true;
	}
}

FName FLinkerLoad::FindSubobjectRedirectName(const FName& Name, UClass* Class)
{
	const TMap<FString, FString>* ValueChanges = FCoreRedirects::GetValueRedirects(ECoreRedirectFlags::Type_Class, Class);

	if (ValueChanges)
	{
		const FString* NewInstanceName = ValueChanges->Find(Name.ToString());
		if (NewInstanceName)
		{
			return FName(**NewInstanceName);
		}
	}

	return FName();
}

/**
 * Creates a FLinkerLoad object for async creation. Tick has to be called manually till it returns
 * true in which case the returned linker object has finished the async creation process.
 *
 * @param	Parent		Parent object to load into, can be NULL (most likely case)
 * @param	Filename	Name of file on disk to load
 * @param	LoadFlags	Load flags determining behavior
 *
 * @return	new FLinkerLoad object for Parent/ Filename
 */
FLinkerLoad* FLinkerLoad::CreateLinkerAsync( UPackage* Parent, const TCHAR* Filename, uint32 LoadFlags 
	, TFunction<void()>&& InSummaryReadyCallback
	)
{
	check(Parent);

	// See whether there already is a linker for this parent/ linker root.
	FLinkerLoad* Linker = FindExistingLinkerForPackage(Parent);
	if (Linker)
	{
		if (GEventDrivenLoaderEnabled)
		{
		UE_LOG(LogStreaming, Fatal, TEXT("FLinkerLoad::CreateLinkerAsync: Found existing linker for '%s'"), *Parent->GetName());
		}
		else
		{
		UE_LOG(LogStreaming, Log, TEXT("FLinkerLoad::CreateLinkerAsync: Found existing linker for '%s'"), *Parent->GetName());
		}		
	}

	// Create a new linker if there isn't an existing one.
	if( Linker == NULL )
	{
		if (GEventDrivenLoaderEnabled && FApp::IsGame() && !GIsEditor)
		{
			LoadFlags |= LOAD_Async;
		}
		Linker = new FLinkerLoad(Parent, Filename, LoadFlags );
		Parent->LinkerLoad = Linker;
		if (GEventDrivenLoaderEnabled && Linker)
		{
			Linker->CreateLoader(Forward<TFunction<void()>>(InSummaryReadyCallback));
		}
	}
	
	check(Parent->LinkerLoad == Linker);

	return Linker;
}

/**
 * Ticks an in-flight linker and spends InTimeLimit seconds on creation. This is a soft time limit used
 * if bInUseTimeLimit is true.
 *
 * @param	InTimeLimit		Soft time limit to use if bInUseTimeLimit is true
 * @param	bInUseTimeLimit	Whether to use a (soft) timelimit
 * @param	bInUseFullTimeLimit	Whether to use the entire time limit, even if blocked on I/O
 * 
 * @return	true if linker has finished creation, false if it is still in flight
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::Tick( float InTimeLimit, bool bInUseTimeLimit, bool bInUseFullTimeLimit )
{
	ELinkerStatus Status = LINKER_Loaded;

	if( bHasFinishedInitialization == false )
	{
		// Store variables used by functions below.
		TickStartTime		= FPlatformTime::Seconds();
		bTimeLimitExceeded	= false;
		bUseTimeLimit		= bInUseTimeLimit;
		bUseFullTimeLimit	= bInUseFullTimeLimit;
		TimeLimit			= InTimeLimit;

		do
		{
			bool bCanSerializePackageFileSummary = false;
			if (GEventDrivenLoaderEnabled)
			{
				check(Loader || bDynamicClassLinker);
				bCanSerializePackageFileSummary = true;
			}
			else
			{
				// Create loader, aka FArchive used for serialization and also precache the package file summary.
				// false is returned until any precaching is complete.
				SCOPED_LOADTIMER(LinkerLoad_CreateLoader);
				Status = CreateLoader(TFunction<void()>([]() {}));

				bCanSerializePackageFileSummary = (Status == LINKER_Loaded);
			}

			// Serialize the package file summary and presize the various arrays (name, import & export map)
			if (bCanSerializePackageFileSummary)
			{
				SCOPED_LOADTIMER(LinkerLoad_SerializePackageFileSummary);
				Status = SerializePackageFileSummary();
			}

			// Serialize the name map and register the names.
			if( Status == LINKER_Loaded )
			{
				SCOPED_LOADTIMER(LinkerLoad_SerializeNameMap);
				Status = SerializeNameMap();
			}

			// Serialize the gatherable text data map.
			if( Status == LINKER_Loaded )
			{
				SCOPED_LOADTIMER(LinkerLoad_SerializeGatherableTextDataMap);
				Status = SerializeGatherableTextDataMap();
			}

			// Serialize the import map.
			if( Status == LINKER_Loaded )
			{
				SCOPED_LOADTIMER(LinkerLoad_SerializeImportMap);
				Status = SerializeImportMap();
			}

			// Serialize the export map.
			if( Status == LINKER_Loaded )
			{
				SCOPED_LOADTIMER(LinkerLoad_SerializeExportMap);
				Status = SerializeExportMap();
			}

			// Fix up import map for backward compatible serialization.
			if( Status == LINKER_Loaded )
			{	
				SCOPED_LOADTIMER(LinkerLoad_FixupImportMap);
				Status = FixupImportMap();
			}

			// Fix up export map for object class conversion 
			if( Status == LINKER_Loaded )
			{	
				SCOPED_LOADTIMER(LinkerLoad_FixupExportMap);
				Status = FixupExportMap();
			}

			// Serialize the dependency map.
			if( Status == LINKER_Loaded )
			{
				SCOPED_LOADTIMER(LinkerLoad_SerializeDependsMap);
				Status = SerializeDependsMap();
			}

			// Hash exports.
			if( Status == LINKER_Loaded )
			{
				SCOPED_LOADTIMER(LinkerLoad_CreateExportHash);
				Status = CreateExportHash();
			}

			// Find existing objects matching exports and associate them with this linker.
			if( Status == LINKER_Loaded )
			{
				SCOPED_LOADTIMER(LinkerLoad_FindExistingExports);
				Status = FindExistingExports();
			}

			if (Status == LINKER_Loaded)
			{
				SCOPED_LOADTIMER(LinkerLoad_SerializePreloadDependencies);
				Status = SerializePreloadDependencies();
			}

			// Finalize creation process.
			if( Status == LINKER_Loaded )
			{
				SCOPED_LOADTIMER(LinkerLoad_FinalizeCreation);
				Status = FinalizeCreation();
			}
		}
		// Loop till we are done if no time limit is specified, or loop until the real time limit is up if we want to use full time
		while (Status == LINKER_TimedOut && 
			(!bUseTimeLimit || (bUseFullTimeLimit && !IsTimeLimitExceeded(TEXT("Checking Full Timer"))))
			);
	}

	if (Status == LINKER_Failed)
	{
		LinkerRoot->LinkerLoad = nullptr;
#if WITH_EDITOR

		delete LoadProgressScope;
		LoadProgressScope = nullptr;	
#endif
	}

	// Return whether we completed or not.
	return Status;
}

/**
 * Private constructor, passing arguments through from CreateLinker.
 *
 * @param	Parent		Parent object to load into, can be NULL (most likely case)
 * @param	Filename	Name of file on disk to load
 * @param	LoadFlags	Load flags determining behavior
 */
FLinkerLoad::FLinkerLoad(UPackage* InParent, const TCHAR* InFilename, uint32 InLoadFlags )
: FLinker(ELinkerType::Load, InParent, InFilename)
, LoadFlags(InLoadFlags)
, bHaveImportsBeenVerified(false)
, bDynamicClassLinker(false)
, TemplateForGetArchetypeFromLoader(nullptr)
, bForceSimpleIndexToObject(false)
, bLockoutLegacyOperations(false)
, bLoaderIsFArchiveAsync2(false)
, Loader(nullptr)
, AsyncRoot(nullptr)
, NameMapIndex(0)
, GatherableTextDataMapIndex(0)
, ImportMapIndex(0)
, ExportMapIndex(0)
, DependsMapIndex(0)
, ExportHashIndex(0)
, bHasSerializedPackageFileSummary(false)
, bHasFixedUpImportMap(false)
, bHasFoundExistingExports(false)
, bHasFinishedInitialization(false)
, bIsGatheringDependencies(false)
, bTimeLimitExceeded(false)
, bUseTimeLimit(false)
, bUseFullTimeLimit(false)
, IsTimeLimitExceededCallCount(0)
, TimeLimit(0.0f)
, TickStartTime(0.0)
, bFixupExportMapDone(false)
#if WITH_EDITOR
, bExportsDuplicatesFixed(false)
,	LoadProgressScope( nullptr )
#endif // WITH_EDITOR
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
,	bForceBlueprintFinalization(false)
,	DeferredCDOIndex(INDEX_NONE)
,	ResolvingDeferredPlaceholder(nullptr)
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
{
	FMemory::Memset(ExportHash, INDEX_NONE, sizeof(ExportHash));
	INC_DWORD_STAT(STAT_LinkerCount);
	INC_DWORD_STAT(STAT_LiveLinkerCount);
#if !UE_BUILD_SHIPPING
	FLinkerManager::Get().GetLiveLinkers().Add(this);
#endif

	OwnerThread = FPlatformTLS::GetCurrentThreadId();
}

FLinkerLoad::~FLinkerLoad()
{
#if !UE_BUILD_SHIPPING
	FLinkerManager::Get().GetLiveLinkers().Remove(this);
#endif

	UE_CLOG(!FUObjectThreadContext::Get().IsDeletingLinkers, LogLinker, Fatal, TEXT("Linkers can only be deleted by FLinkerManager."));

	// Detaches linker.
	Detach();

	DEC_DWORD_STAT(STAT_LiveLinkerCount);

#if WITH_EDITOR
	// Make sure this is deleted if it's still allocated
	delete LoadProgressScope;
#endif
	check(!Loader);
}

/**
 * Returns whether the time limit allotted has been exceeded, if enabled.
 *
 * @param CurrentTask	description of current task performed for logging spilling over time limit
 * @param Granularity	Granularity on which to check timing, useful in cases where FPlatformTime::Seconds is slow (e.g. PC)
 *
 * @return true if time limit has been exceeded (and is enabled), false otherwise (including if time limit is disabled)
 */
bool FLinkerLoad::IsTimeLimitExceeded( const TCHAR* CurrentTask, int32 Granularity )
{
	IsTimeLimitExceededCallCount++;
	if( !bTimeLimitExceeded 
	&&	bUseTimeLimit 
	&&  (IsTimeLimitExceededCallCount % Granularity) == 0 )
	{
		double CurrentTime = FPlatformTime::Seconds();
		bTimeLimitExceeded = CurrentTime - TickStartTime > TimeLimit;
		if (!FPlatformProperties::HasEditorOnlyData())
		{
			// Log single operations that take longer than timelimit.
			if( (CurrentTime - TickStartTime) > (2.5 * TimeLimit) )
			{
				UE_LOG(LogStreaming, Log, TEXT("FLinkerLoad: %s took (less than) %5.2f ms"), 
					CurrentTask, 
					(CurrentTime - TickStartTime) * 1000);
			}
		}
	}
	return bTimeLimitExceeded;
}

/**
 * Creates loader used to serialize content.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::CreateLoader(
	TFunction<void()>&& InSummaryReadyCallback
	)
{
	//DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::CreateLoader" ), STAT_LinkerLoad_CreateLoader, STATGROUP_LinkerLoad );

#if WITH_EDITOR

	if (!LoadProgressScope)
	{
		LoadProgressScope = new FScopedSlowTask(FLinkerDefs::TotalProgressSteps, NSLOCTEXT("Core", "GenericLoading", "Loading..."), ShouldReportProgress());
	}

#endif

	// This should have been initialized in InitUObject
	check(bActiveRedirectsMapInitialized);

	if( !Loader && !bDynamicClassLinker )
	{
#if WITH_EDITOR
		FFormatNamedArguments FeedbackArgs;
		FeedbackArgs.Add( TEXT("CleanFilename"), FText::FromString( FPaths::GetCleanFilename( *Filename ) ) );
		LoadProgressScope->DefaultMessage = FText::Format( NSLOCTEXT("Core", "LoadingFileWithFilename", "Loading file: {CleanFilename}..."), FeedbackArgs );
		LoadProgressScope->EnterProgressFrame();
#endif

		// Check if this linker was created for dynamic class package
		bDynamicClassLinker = GetConvertedDynamicPackageNameToTypeName().Contains(LinkerRoot->GetFName());
		if (bDynamicClassLinker
#if WITH_EDITORONLY_DATA
			&& GLinkerAllowDynamicClasses
#endif
			)
		{
			// In this case we can skip serializing PackageFileSummary and fill all the required info here
			CreateDynamicTypeLoader();
		}
		else
		{
			Loader = new FArchiveAsync2(*Filename
					, GEventDrivenLoaderEnabled ? Forward<TFunction<void()>>(InSummaryReadyCallback) : TFunction<void()>([]() {})
				);

			if (!Loader)
			{
				UE_LOG(LogLinker, Warning, TEXT("Error opening file '%s'."), *Filename);
				return LINKER_Failed;
			}

			if( Loader->IsError() )
			{
				delete Loader;
				Loader = nullptr;
				UE_LOG(LogLinker, Warning, TEXT("Error opening file '%s'."), *Filename);
				return LINKER_Failed;
			}
#if DEVIRTUALIZE_FLinkerLoad_Serialize
			ActiveFPLB = Loader->ActiveFPLB; // make sure my fast past loading is using the FAA2 fast path buffer
#endif

			bool bHasHashEntry = FSHA1::GetFileSHAHash(*Filename, NULL);
			if ((LoadFlags & LOAD_MemoryReader) || bHasHashEntry)
			{
				// force preload into memory if file has an SHA entry
				// Serialize data from memory instead of from disk.
				uint32	BufferSize = Loader->TotalSize();
				void*	Buffer = FMemory::Malloc(BufferSize);
				Loader->Serialize(Buffer, BufferSize);
				delete Loader;
				Loader = nullptr;
				if (bHasHashEntry)
				{
					// create buffer reader and spawn SHA verify when it gets closed
					Loader = new FBufferReaderWithSHA(Buffer, BufferSize, true, *Filename, true);
				}
				else
				{
					// create a buffer reader
					Loader = new FBufferReader(Buffer, BufferSize, true, true);
				}
			}
			else
			{
				bLoaderIsFArchiveAsync2 = true;
			}
		} 

		check(bDynamicClassLinker || Loader);
		check(bDynamicClassLinker || !Loader->IsError());

		//if( FLinkerLoad::FindExistingLinkerForPackage(LinkerRoot) )
		//{
		//	UE_LOG(LogLinker, Warning, TEXT("Linker for '%s' already exists"), *LinkerRoot->GetName() );
		//	return LINKER_Failed;
		//}

		// Set status info.
		ArUE4Ver = GPackageFileUE4Version;
		ArLicenseeUE4Ver = GPackageFileLicenseeUE4Version;
		ArEngineVer = FEngineVersion::Current();
		ArIsLoading = true;
		ArIsPersistent = true;

		// Reset all custom versions
		ResetCustomVersions();
	}
	else if (GEventDrivenLoaderEnabled)
	{
		check(0);
	}
	if (GEventDrivenLoaderEnabled)
	{
		return LINKER_TimedOut;
	}
	else
	{
		bool bExecuteNextStep = true;
		if( bHasSerializedPackageFileSummary == false )
		{
			if (bLoaderIsFArchiveAsync2)
			{
				bExecuteNextStep = GetFArchiveAsync2Loader()->ReadyToStartReadingHeader(bUseTimeLimit, bUseFullTimeLimit, TickStartTime, TimeLimit);
			}
			else
			{
				int64 Size = Loader->TotalSize();
				if (Size <= 0)
				{
					delete Loader;
					Loader = nullptr;
					UE_LOG(LogLinker, Warning, TEXT("Error opening file '%s'."), *Filename);
					return LINKER_Failed;
				}
				// Precache up to one ECC block before serializing package file summary.
				// If the package is partially compressed, we'll know that quickly and
				// end up discarding some of the precached data so we can re-fetch
				// and decompress it.
				static int64 MinimumReadSize = 32 * 1024;
				checkSlow(MinimumReadSize >= 2048 && MinimumReadSize <= 1024 * 1024); // not a hard limit, but we should be loading at least a reasonable amount of data
				int32 PrecacheSize = FMath::Min(MinimumReadSize, Size);
				check( PrecacheSize > 0 );
				// Wait till we're finished precaching before executing the next step.
				bExecuteNextStep = Loader->Precache(0, PrecacheSize);
			}
		}

		return (bExecuteNextStep && !IsTimeLimitExceeded( TEXT("creating loader") )) ? LINKER_Loaded : LINKER_TimedOut;
	}
}

/**
 * Serializes the package file summary.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::SerializePackageFileSummary()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::SerializePackageFileSummary" ), STAT_LinkerLoad_SerializePackageFileSummary, STATGROUP_LinkerLoad );

	if (bHasSerializedPackageFileSummary == false)
	{
		if (Loader->IsError())
		{
			UE_LOG(LogLinker, Warning, TEXT("The file '%s' contains unrecognizable data, check that it is of the expected type."), *Filename);
			return LINKER_Failed;
		}
		if (bLoaderIsFArchiveAsync2)
		{
			GetFArchiveAsync2Loader()->StartReadingHeader();
		}

#if WITH_EDITOR
		LoadProgressScope->EnterProgressFrame(1);
#endif
		// Read summary from file.
		*this << Summary;

		// Check tag.
		if( Summary.Tag != PACKAGE_FILE_TAG )
		{
			UE_LOG(LogLinker, Warning, TEXT("The file '%s' contains unrecognizable data, check that it is of the expected type."), *Filename );
			return LINKER_Failed;
		}

		// Validate the summary.
		if( Summary.GetFileVersionUE4() < VER_UE4_OLDEST_LOADABLE_PACKAGE)
		{
			UE_LOG(LogLinker, Warning, TEXT("The file %s was saved by a previous version which is not backwards compatible with this one. Min Required Version: %i  Package Version: %i"), *Filename, (int32)VER_UE4_OLDEST_LOADABLE_PACKAGE, Summary.GetFileVersionUE4() );
			return LINKER_Failed;
		}

		// Don't load packages that are only compatible with an engine version newer than the current one.
		if( !FEngineVersion::Current().IsCompatibleWith(Summary.CompatibleWithEngineVersion) )
		{
			UE_LOG(LogLinker, Warning, TEXT("Asset '%s' has been saved with engine version newer than current and therefore can't be loaded. CurrEngineVersion: %s AssetEngineVersion: %s"), *Filename, *FEngineVersion::Current().ToString(), *Summary.CompatibleWithEngineVersion.ToString() );
			return LINKER_Failed;
		}
		else if( !FPlatformProperties::RequiresCookedData() && !Summary.SavedByEngineVersion.HasChangelist() && FEngineVersion::Current().HasChangelist() )
		{
			// This warning can be disabled in ini with [Core.System] ZeroEngineVersionWarning=False
			static struct FInitZeroEngineVersionWarning
			{
				bool bDoWarn;
				FInitZeroEngineVersionWarning()
				{
					if (!GConfig->GetBool(TEXT("Core.System"), TEXT("ZeroEngineVersionWarning"), bDoWarn, GEngineIni))
					{
						bDoWarn = true;
					}
				}
				FORCEINLINE operator bool() const { return bDoWarn; }
			} ZeroEngineVersionWarningEnabled;			
			UE_CLOG(ZeroEngineVersionWarningEnabled, LogLinker, Warning, TEXT("Asset '%s' has been saved with empty engine version. The asset will be loaded but may be incompatible."), *Filename );
		}

		// Don't load packages that were saved with package version newer than the current one.
		if( (Summary.GetFileVersionUE4() > GPackageFileUE4Version) || (Summary.GetFileVersionLicenseeUE4() > GPackageFileLicenseeUE4Version) )
		{
			UE_LOG(LogLinker, Warning, TEXT("Unable to load package (%s) PackageVersion %i, MaxExpected %i : LicenseePackageVersion %i, MaxExpected %i."), *Filename, Summary.GetFileVersionUE4(), GPackageFileUE4Version, Summary.GetFileVersionLicenseeUE4(), GPackageFileLicenseeUE4Version );
			return LINKER_Failed;
		}

		// don't load packages that contain editor only data in builds that don't support that and vise versa
		if (!FPlatformProperties::HasEditorOnlyData() && !(Summary.PackageFlags & PKG_FilterEditorOnly))
		{
			UE_LOG(LogLinker, Warning, TEXT("Unable to load package (%s). Package contains EditorOnly data which is not supported by the current build."), *Filename );
			return LINKER_Failed;
		}

		// don't load packages that contain editor only data in builds that don't support that and vise versa
		if (FPlatformProperties::HasEditorOnlyData() && !!(Summary.PackageFlags & PKG_FilterEditorOnly))
		{
			// This warning can be disabled in ini with [Core.System] AllowCookedDataInEditorBuilds=False
			static struct FInitCookedDatataInEditorBuildsSupport
			{
				bool bAllowCookedData;
				FInitCookedDatataInEditorBuildsSupport()
				{
					if (!GConfig->GetBool(TEXT("Core.System"), TEXT("AllowCookedDataInEditorBuilds"), bAllowCookedData, GEngineIni))
					{
						bAllowCookedData = true;
					}
				}
				FORCEINLINE operator bool() const { return bAllowCookedData; }
			} AllowCookedDataInEditorBuilds;
			if (!AllowCookedDataInEditorBuilds)
			{
				UE_LOG(LogLinker, Warning, 
					TEXT("Unable to load package (%s). Package contains cooked data which is not supported by the current build. Set [Core.System] AllowCookedDataInEditorBuilds to true in Engine.ini to allow it."), 
					*Filename);
				return LINKER_Failed;
			}
		}

		if (FPlatformProperties::RequiresCookedData() && 
			Summary.PreloadDependencyCount > 0 && Summary.PreloadDependencyOffset > 0 &&
			!IsEventDrivenLoaderEnabledInCookedBuilds())
		{
			UE_LOG(LogLinker, Fatal, TEXT("Package %s contains preload dependency data but the current build does not support it. Make sure Event Driven Loader is enabled and rebuild the game executable."),
				*GetArchiveName())
		}

#if PLATFORM_WINDOWS
		if (!FPlatformProperties::RequiresCookedData() && 
			// We can't check the post tag if the file is an EDL cooked package
			!((Summary.PackageFlags & PKG_FilterEditorOnly) && Summary.PreloadDependencyCount > 0 && Summary.PreloadDependencyOffset > 0))
		{
			// check if this package version stored the 4-byte magic post tag
			// get the offset of the post tag
			int64 MagicOffset = TotalSize() - sizeof(uint32);
			// store the current file offset
			int64 OriginalOffset = Tell();

			uint32 Tag = 0;

			// seek to the post tag and serialize it
			Seek(MagicOffset);
			*this << Tag;

			if (Tag != PACKAGE_FILE_TAG)
			{
				UE_LOG(LogLinker, Warning, TEXT("Unable to load package (%s). Post Tag is not valid. File might be corrupted."), *Filename);
				return LINKER_Failed;
			}

			// seek back to the position after the package summary
			Seek(OriginalOffset);
		}
#endif // PLATFORM_WINDOWS

		// Check custom versions.
		const FCustomVersionContainer& LatestCustomVersions  = FCustomVersionContainer::GetRegistered();
		bool bCustomVersionIsLatest = false;
		if (Summary.bUnversioned)
		{
			// When unversioned, pretend we are the latest version
			bCustomVersionIsLatest = true;
		}
		else
		{
			bool bAllSavedVersionsMatch = true;
			const FCustomVersionSet&  PackageCustomVersions = Summary.GetCustomVersionContainer().GetAllVersions();
			for (auto It = PackageCustomVersions.CreateConstIterator(); It; ++It)
			{
				const FCustomVersion& SerializedCustomVersion = *It;

				const FCustomVersion* LatestVersion = LatestCustomVersions.GetVersion(SerializedCustomVersion.Key);
				if (!LatestVersion)
				{
					// Loading a package with custom integration that we don't know about!
					// Temporarily just warn and continue. @todo: this needs to be fixed properly
					UE_LOG(LogLinker, Warning, TEXT("Package %s was saved with a custom integration that is not present. Tag %s  Version %d"), *Filename, *SerializedCustomVersion.Key.ToString(), SerializedCustomVersion.Version);
					bAllSavedVersionsMatch = false;
				}
				else if (SerializedCustomVersion.Version > LatestVersion->Version)
				{
					// Loading a package with a newer custom version than the current one.
					UE_LOG(LogLinker, Error, TEXT("Package %s was saved with a newer custom version than the current. Tag %s  PackageVersion %d  MaxExpected %d"), *Filename, *SerializedCustomVersion.Key.ToString(), SerializedCustomVersion.Version, LatestVersion->Version);
					return LINKER_Failed;
				}
				else if (SerializedCustomVersion.Version != LatestVersion->Version)
				{
					bAllSavedVersionsMatch = false;
				}
			}

			const bool bSameNumberOfVersions = (PackageCustomVersions.Num() == LatestCustomVersions.GetAllVersions().Num());
			bCustomVersionIsLatest = bSameNumberOfVersions && bAllSavedVersionsMatch;
		}

		// Loader needs to be the same version.
		Loader->SetUE4Ver(Summary.GetFileVersionUE4());
		Loader->SetLicenseeUE4Ver(Summary.GetFileVersionLicenseeUE4());
		Loader->SetEngineVer(Summary.SavedByEngineVersion);

		ArUE4Ver = Summary.GetFileVersionUE4();
		ArLicenseeUE4Ver = Summary.GetFileVersionLicenseeUE4();
		ArEngineVer = Summary.SavedByEngineVersion;

		const FCustomVersionContainer& SummaryVersions = Summary.GetCustomVersionContainer();
		Loader->SetCustomVersions(SummaryVersions);
		SetCustomVersions(SummaryVersions);

		// Package has been stored compressed.

		UPackage* LinkerRootPackage = LinkerRoot;
		if( LinkerRootPackage )
		{
			// Preserve PIE package flag
			uint32 NewPackageFlags = Summary.PackageFlags;
			if (LinkerRootPackage->HasAnyPackageFlags(PKG_PlayInEditor))
			{
				NewPackageFlags |= PKG_PlayInEditor;
			}
			
			// Propagate package flags
			LinkerRootPackage->SetPackageFlagsTo(NewPackageFlags);

			// Propagate package folder name
			LinkerRootPackage->SetFolderName(*Summary.FolderName);

			// Propagate streaming install ChunkID
			LinkerRootPackage->SetChunkIDs(Summary.ChunkIDs);
			
			// Propagate package file size
			LinkerRootPackage->FileSize = TotalSize();

			// Propagate package Guid
			LinkerRootPackage->SetGuid( Summary.Guid );

			// Remember the linker versions
			LinkerRootPackage->LinkerPackageVersion = ArUE4Ver;
			LinkerRootPackage->LinkerLicenseeVersion = ArLicenseeUE4Ver;

			// Only set the custom version if it is not already latest.
			// If it is latest, we will compare against latest in GetLinkerCustomVersion
			if (!bCustomVersionIsLatest)
			{
				LinkerRootPackage->LinkerCustomVersion = SummaryVersions;
			}

#if WITH_EDITORONLY_DATA
			LinkerRootPackage->bIsCookedForEditor = !!(Summary.PackageFlags & PKG_FilterEditorOnly);
#endif
		}
		
		// Propagate fact that package cannot use lazy loading to archive (aka this).
		if( (Summary.PackageFlags & PKG_DisallowLazyLoading) )
		{
			ArAllowLazyLoading = false;
		}
		else
		{
			ArAllowLazyLoading = true;
		}

		// Slack everything according to summary.
		ImportMap					.Empty( Summary.ImportCount				);
		ExportMap					.Empty( Summary.ExportCount				);
		GatherableTextDataMap		.Empty( Summary.GatherableTextDataCount );
		NameMap						.Empty( Summary.NameCount				);
		// Depends map gets pre-sized in SerializeDependsMap if used.

		// Avoid serializing it again.
		bHasSerializedPackageFileSummary = true;
	}

	return !IsTimeLimitExceeded( TEXT("serializing package file summary") ) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Serializes the name table.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::SerializeNameMap()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::SerializeNameMap" ), STAT_LinkerLoad_SerializeNameMap, STATGROUP_LinkerLoad );

	// The name map is the first item serialized. We wait till all the header information is read
	// before any serialization. @todo async, @todo seamless: this could be spread out across name,
	// import and export maps if the package file summary contained more detailed information on
	// serialized size of individual entries.
	bool bFinishedPrecaching = true;

	if( NameMapIndex == 0 && Summary.NameCount > 0 )
	{
		Seek( Summary.NameOffset );
		// Make sure there is something to precache first.
		if( Summary.TotalHeaderSize > 0 )
		{
			// Precache name, import and export map.
			if (bLoaderIsFArchiveAsync2)
			{
				bFinishedPrecaching = GetFArchiveAsync2Loader()->ReadyToStartReadingHeader(bUseTimeLimit, bUseFullTimeLimit, TickStartTime, TimeLimit);
				check(!GEventDrivenLoaderEnabled || bFinishedPrecaching || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);
			}
			else
			{
				bFinishedPrecaching = Loader->Precache(Summary.NameOffset, Summary.TotalHeaderSize - Summary.NameOffset);
			}
		}
		// Backward compat code for VER_MOVED_EXPORTIMPORTMAPS_ADDED_TOTALHEADERSIZE.
		else
		{
			bFinishedPrecaching = true;
		}
	}

	while( bFinishedPrecaching && NameMapIndex < Summary.NameCount && !IsTimeLimitExceeded(TEXT("serializing name map"),100) )
	{
		SCOPED_LOADTIMER(LinkerLoad_SerializeNameMap_ProcessingEntries);

		// Read the name entry from the file.
		FNameEntrySerialized NameEntry(ENAME_LinkerConstructor);
		*this << NameEntry;

		// Add it to the name table with no splitting and no hash calculations
		NameMap.Add(FName(NameEntry));

		NameMapIndex++;
	}

	// Return whether we finished this step and it's safe to start with the next.
	return ((NameMapIndex == Summary.NameCount) && !IsTimeLimitExceeded( TEXT("serializing name map") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Serializes the gatherable text data container.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::SerializeGatherableTextDataMap(bool bForceEnableForCommandlet)
{
#if WITH_EDITORONLY_DATA
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::SerializeGatherableTextDataMap" ), STAT_LinkerLoad_SerializeGatherableTextDataMap, STATGROUP_LinkerLoad );

	// Skip serializing gatherable text data if we are using seekfree loading
	if( !bForceEnableForCommandlet && !GIsEditor )
	{
		return LINKER_Loaded;
	}

	if( GatherableTextDataMapIndex == 0 && Summary.GatherableTextDataCount > 0 )
	{
		Seek( Summary.GatherableTextDataOffset );
	}

	while( GatherableTextDataMapIndex < Summary.GatherableTextDataCount && !IsTimeLimitExceeded(TEXT("serializing gatherable text data map"),100) )
	{
		FGatherableTextData* GatherableTextData = new(GatherableTextDataMap)FGatherableTextData;
		*this << *GatherableTextData;
		GatherableTextDataMapIndex++;
	}

	return ((GatherableTextDataMapIndex == Summary.GatherableTextDataCount) && !IsTimeLimitExceeded( TEXT("serializing gatherable text data map") )) ? LINKER_Loaded : LINKER_TimedOut;
#endif

	return LINKER_Loaded;
}

/**
 * Serializes the import map.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::SerializeImportMap()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::SerializeImportMap" ), STAT_LinkerLoad_SerializeImportMap, STATGROUP_LinkerLoad );

	if( ImportMapIndex == 0 && Summary.ImportCount > 0 )
	{
		Seek( Summary.ImportOffset );
	}

	while( ImportMapIndex < Summary.ImportCount && !IsTimeLimitExceeded(TEXT("serializing import map"),100) )
	{
		FObjectImport* Import = new(ImportMap)FObjectImport;
		*this << *Import;
		ImportMapIndex++;
	}
	
	// Return whether we finished this step and it's safe to start with the next.
	return ((ImportMapIndex == Summary.ImportCount) && !IsTimeLimitExceeded( TEXT("serializing import map") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Fixes up the import map, performing remapping for backward compatibility and such.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::FixupImportMap()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::FixupImportMap" ), STAT_LinkerLoad_FixupImportMap, STATGROUP_LinkerLoad );

	if( bHasFixedUpImportMap == false )
	{
#if WITH_EDITOR
		LoadProgressScope->EnterProgressFrame(1);
#endif
		// Fix up imports, not required if everything is cooked.
		if (!FPlatformProperties::RequiresCookedData())
		{
			static const FName NAME_BlueprintGeneratedClass(TEXT("BlueprintGeneratedClass"));

			TArray<int32> PackageIndexesToClear;

			bool bDone = false;
			while (!bDone)
			{
				TArray<FName> NewPackageImports;

				bDone = true;
				for( int32 i=0; i<ImportMap.Num(); i++ )
				{
					FObjectImport& Import = ImportMap[i];

					// Compute class name first, as instance can override it
					const FCoreRedirect* ClassValueRedirect = nullptr;
					FCoreRedirectObjectName OldClassName(Import.ClassName, NAME_None, Import.ClassPackage), NewClassName;

					FCoreRedirects::RedirectNameAndValues(ECoreRedirectFlags::Type_Class, OldClassName, NewClassName, &ClassValueRedirect);

					if (ClassValueRedirect)
					{
						// Apply class value redirects before other redirects, to mirror old subobject order
						const FString* NewInstanceName = ClassValueRedirect->ValueChanges.Find(Import.ObjectName.ToString());
						if (NewInstanceName)
						{
							// Rename this import directly
							FString Was = GetImportFullName(i);
							Import.ObjectName = FName(**NewInstanceName);

							if (Import.ObjectName != NAME_None)
							{
								FString Now = GetImportFullName(i);
								UE_LOG(LogLinker, Verbose, TEXT("FLinkerLoad::FixupImportMap() - Renamed object from %s   to   %s"), *Was, *Now);
							}
							else
							{
								UE_LOG(LogLinker, Verbose, TEXT("FLinkerLoad::FixupImportMap() - Removed object %s"), *Was);
							}
						}
					}

					FCoreRedirectObjectName OldObjectName(GetImportPathName(i)), NewObjectName;
					ECoreRedirectFlags ObjectRedirectFlags = FCoreRedirects::GetFlagsForTypeName(Import.ClassPackage, Import.ClassName);
					const FCoreRedirect* ValueRedirect = nullptr;
					
					FCoreRedirects::RedirectNameAndValues(ObjectRedirectFlags, OldObjectName, NewObjectName, &ValueRedirect);

					if (ValueRedirect && ValueRedirect->OverrideClassName.IsValid())
					{
						// Override class name if found, even if the name didn't actually change
						NewClassName = ValueRedirect->OverrideClassName;
					}

					if (NewObjectName != OldObjectName)
					{
						if (Import.OuterIndex.IsNull())
						{
							// If this has no outer it's a package and we don't want to rename it, the subobject renames will handle creating the new package import
							// We do need to clear these at the end so it doesn't try to load nonexistent packages
							PackageIndexesToClear.Add(i);
						}
						else
						{
							// If right below package and package has changed, need to swap outer
							if (NewObjectName.OuterName == NAME_None && NewObjectName.PackageName != OldObjectName.PackageName)
							{
								FPackageIndex NewPackageIndex;

								if (FindImportPackage(NewObjectName.PackageName, NewPackageIndex))
								{
									// Already in import table, set it
									Import.OuterIndex = NewPackageIndex;
								}
								else
								{
									// Need to add package import and try again
									NewPackageImports.AddUnique(NewObjectName.PackageName);
									bDone = false;
									break;
								}
							}
#if WITH_EDITOR
							// If this is a class, set old name here 
							if (ObjectRedirectFlags == ECoreRedirectFlags::Type_Class)
							{
								Import.OldClassName = Import.ObjectName;
							}

#endif
							// Change object name
							Import.ObjectName = NewObjectName.ObjectName;

							UE_LOG(LogLinker, Verbose, TEXT("FLinkerLoad::FixupImportMap() - Renamed Object %s -> %s"), *LinkerRoot->GetName(), *OldObjectName.ToString(), *NewObjectName.ToString());
						}
					}

					if (NewClassName != OldClassName)
					{
						// Swap class if needed
						if (Import.ClassPackage != NewClassName.PackageName && !IsCoreUObjectPackage(NewClassName.PackageName))
						{
							FPackageIndex NewPackageIndex;

							if (!FindImportPackage(NewClassName.PackageName, NewPackageIndex))
							{
								// Need to add package import and try again
								NewPackageImports.AddUnique(NewClassName.PackageName);
								bDone = false;
								break;
							}
						}
#if WITH_EDITOR
						Import.OldClassName = Import.ClassName;
#endif
						// Change class name/package
						Import.ClassPackage = NewClassName.PackageName;
						Import.ClassName = NewClassName.ObjectName;

						// Also change CDO name if needed
						FString NewDefaultObjectName = Import.ObjectName.ToString();

						if (NewDefaultObjectName.StartsWith(DEFAULT_OBJECT_PREFIX))
						{
							NewDefaultObjectName = FString(DEFAULT_OBJECT_PREFIX);
							NewDefaultObjectName += NewClassName.ObjectName.ToString();
							Import.ObjectName = FName(*NewDefaultObjectName);
						}

						UE_LOG(LogLinker, Verbose, TEXT("FLinkerLoad::FixupImportMap() - Renamed Class %s -> %s"), *LinkerRoot->GetName(), *OldClassName.ToString(), *NewClassName.ToString());
					}	
				}

				// Add new packages, after loop iteration for safety
				for (FName NewPackage : NewPackageImports)
				{
					// We are adding a new import to the map as we need the new package dependency added to the works
					FObjectImport* NewImport = new (ImportMap) FObjectImport();

					NewImport->ClassName = NAME_Package;
					NewImport->ClassPackage = GLongCoreUObjectPackageName;
					NewImport->ObjectName = NewPackage;
					NewImport->OuterIndex = FPackageIndex();
					NewImport->XObject = 0;
					NewImport->SourceLinker = 0;
					NewImport->SourceIndex = -1;
				}
			}

			// Clear any packages that got renamed, once all children have been fixed up
			for (int32 PackageIndex : PackageIndexesToClear)
			{
				FObjectImport& Import = ImportMap[PackageIndex];
				check(Import.OuterIndex.IsNull());
				Import.ObjectName = NAME_None;
			}
		}
		// Avoid duplicate work in async case.
		bHasFixedUpImportMap = true;
	}
	return IsTimeLimitExceeded( TEXT("fixing up import map") ) ? LINKER_TimedOut : LINKER_Loaded;
}

/**
 * Serializes the export map.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::SerializeExportMap()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::SerializeExportMap" ), STAT_LinkerLoad_SerializeExportMap, STATGROUP_LinkerLoad );

	if( ExportMapIndex == 0 && Summary.ExportCount > 0 )
	{
		Seek( Summary.ExportOffset );
	}

	while( ExportMapIndex < Summary.ExportCount && !IsTimeLimitExceeded(TEXT("serializing export map"),100) )
	{
		FObjectExport* Export = new(ExportMap)FObjectExport;
		*this << *Export;
		Export->ThisIndex = FPackageIndex::FromExport(ExportMapIndex);
		Export->bWasFiltered = FilterExport(*Export);
		ExportMapIndex++;
	}

	// Return whether we finished this step and it's safe to start with the next.
	return ((ExportMapIndex == Summary.ExportCount) && !IsTimeLimitExceeded( TEXT("serializing export map") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Serializes the depends map.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::SerializeDependsMap()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::SerializeDependsMap" ), STAT_LinkerLoad_SerializeDependsMap, STATGROUP_LinkerLoad );

	// Skip serializing depends map if we are using seekfree loading
	if( FPlatformProperties::RequiresCookedData() 
	// or we are neither Editor nor commandlet
	|| !(GIsEditor || IsRunningCommandlet()) )
	{
		return LINKER_Loaded;
	}

	if ( Summary.DependsOffset == 0 )
	{
		// This package was saved baddly
		return LINKER_Loaded;
	}

	// depends map size is same as export map size
	if (DependsMapIndex == 0 && Summary.ExportCount > 0)
	{
		Seek(Summary.DependsOffset);

		// Pre-size array to avoid re-allocation of array of arrays!
		DependsMap.AddZeroed(Summary.ExportCount);
	}

	while (DependsMapIndex < Summary.ExportCount && !IsTimeLimitExceeded(TEXT("serializing depends map"), 100))
	{
		TArray<FPackageIndex>& Depends = DependsMap[DependsMapIndex];
		*this << Depends;
		DependsMapIndex++;
	}

	// Return whether we finished this step and it's safe to start with the next.
	return ((DependsMapIndex == Summary.ExportCount) && !IsTimeLimitExceeded( TEXT("serializing depends map") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
* Serializes the depends map.
*/
FLinkerLoad::ELinkerStatus FLinkerLoad::SerializePreloadDependencies()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FLinkerLoad::SerializePreloadDependencies"), STAT_LinkerLoad_SerializePreloadDependencies, STATGROUP_LinkerLoad);

	// Skip serializing depends map if this is the editor or the data is missing
	if (Summary.PreloadDependencyCount < 1 || Summary.PreloadDependencyOffset <= 0)
	{
		return LINKER_Loaded;
	}

	Seek(Summary.PreloadDependencyOffset);

	PreloadDependencies.Reserve(Summary.PreloadDependencyCount);
	//@todoio check endiness and fastpath this as a single serialize
	for (int32 Index = 0; Index < Summary.PreloadDependencyCount; Index++)
	{
		FPackageIndex Idx;
		*this << Idx;
		PreloadDependencies.Add(Idx);
	}
	// Return whether we finished this step and it's safe to start with the next.
	return !IsTimeLimitExceeded(TEXT("serialize preload dependencies")) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Serializes thumbnails
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::SerializeThumbnails( bool bForceEnableInGame/*=false*/ )
{
#if WITH_EDITORONLY_DATA
	// Skip serializing thumbnails if we are using seekfree loading
	if( !bForceEnableInGame && !GIsEditor )
	{
		return LINKER_Loaded;
	}

	if( Summary.ThumbnailTableOffset > 0 )
	{
		// Seek to the thumbnail table of contents
		Seek( Summary.ThumbnailTableOffset );


		// Load number of thumbnails
		int32 ThumbnailCount = 0;
		*this << ThumbnailCount;


		// Allocate a new thumbnail map if we need one
		if( !LinkerRoot->ThumbnailMap )
		{
			LinkerRoot->ThumbnailMap = MakeUnique<FThumbnailMap>();
		}


		// Load thumbnail names and file offsets
		TArray< FObjectFullNameAndThumbnail > ThumbnailInfoArray;
		for( int32 CurObjectIndex = 0; CurObjectIndex < ThumbnailCount; ++CurObjectIndex )
		{
			FObjectFullNameAndThumbnail ThumbnailInfo;

			FString ObjectClassName;
				// Newer packages always store the class name for each asset
				*this << ObjectClassName;

			// Object path
			FString ObjectPathWithoutPackageName;
			*this << ObjectPathWithoutPackageName;
			const FString ObjectPath( LinkerRoot->GetName() + TEXT( "." ) + ObjectPathWithoutPackageName );


			// Create a full name string with the object's class and fully qualified path
			const FString ObjectFullName( ObjectClassName + TEXT( " " ) + ObjectPath );
			ThumbnailInfo.ObjectFullName = FName( *ObjectFullName );

			// File offset for the thumbnail (already saved out.)
			*this << ThumbnailInfo.FileOffset;

			// Only bother loading thumbnails that don't already exist in memory yet.  This is because when we
			// go to load thumbnails that aren't in memory yet when saving packages we don't want to clobber
			// thumbnails that were freshly-generated during that editor session
			if( !LinkerRoot->ThumbnailMap->Contains( ThumbnailInfo.ObjectFullName ) )
			{
				// Add to list of thumbnails to load
				ThumbnailInfoArray.Add( ThumbnailInfo );
			}
		}



		// Now go and load and cache all of the thumbnails
		for( int32 CurObjectIndex = 0; CurObjectIndex < ThumbnailInfoArray.Num(); ++CurObjectIndex )
		{
			const FObjectFullNameAndThumbnail& CurThumbnailInfo = ThumbnailInfoArray[ CurObjectIndex ];


			// Seek to the location in the file with the image data
			Seek( CurThumbnailInfo.FileOffset );

			// Load the image data
			FObjectThumbnail LoadedThumbnail;
			LoadedThumbnail.Serialize( *this );

			// Store the data!
			LinkerRoot->ThumbnailMap->Add( CurThumbnailInfo.ObjectFullName, LoadedThumbnail );
		}
	}
#endif // WITH_EDITORONLY_DATA

	// Finished!
	return LINKER_Loaded;
}



/** 
 * Creates the export hash. This relies on the import and export maps having already been serialized.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::CreateExportHash()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::CreateExportHash" ), STAT_LinkerLoad_CreateExportHash, STATGROUP_LinkerLoad );

	// Zero initialize hash on first iteration.
	if( ExportHashIndex == 0 )
	{
		for( int32 i=0; i<ARRAY_COUNT(ExportHash); i++ )
		{
			ExportHash[i] = INDEX_NONE;
		}
	}

	// Set up export hash, potentially spread across several frames.
	while( ExportHashIndex < ExportMap.Num() && !IsTimeLimitExceeded(TEXT("creating export hash"),100) )
	{
		FObjectExport& Export = ExportMap[ExportHashIndex];

		const int32 iHash = HashNames( Export.ObjectName, GetExportClassName(ExportHashIndex), GetExportClassPackage(ExportHashIndex) ) & (ARRAY_COUNT(ExportHash)-1);
		Export.HashNext = ExportHash[iHash];
		ExportHash[iHash] = ExportHashIndex;

		ExportHashIndex++;
	}

	// Return whether we finished this step and it's safe to start with the next.
	return ((ExportHashIndex == ExportMap.Num()) && !IsTimeLimitExceeded( TEXT("creating export hash") )) ? LINKER_Loaded : LINKER_TimedOut;
}

/**
 * Finds existing exports in memory and matches them up with this linker. This is required for PIE to work correctly
 * and also for script compilation as saving a package will reset its linker and loading will reload/ replace existing
 * objects without a linker.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::FindExistingExports()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::FindExistingExports" ), STAT_LinkerLoad_FindExistingExports, STATGROUP_LinkerLoad );

	if( bHasFoundExistingExports == false )
	{
		// only look for existing exports in the editor after it has started up
#if WITH_EDITOR
		LoadProgressScope->EnterProgressFrame(1);
		if( GIsEditor && GIsRunning )
		{
			// Hunt down any existing objects and hook them up to this linker unless the user is either currently opening this
			// package manually via the generic browser or the package is a map package. We want to overwrite (aka load on top)
			// the objects in those cases, so don't try to find existing exports.
			//
			bool bContainsMap			= LinkerRoot ? LinkerRoot->ContainsMap() : false;
			bool bRequestFindExisting = FCoreUObjectDelegates::ShouldLoadOnTop.IsBound() ? !FCoreUObjectDelegates::ShouldLoadOnTop.Execute(Filename) : true;
			if( (!IsRunningCommandlet() && bRequestFindExisting && !bContainsMap) )
			{
				for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++)
				{
					FindExistingExport(ExportIndex);
				}
			}
		}
#endif // WITH_EDITOR

		// Avoid duplicate work in the case of async linker creation.
		bHasFoundExistingExports = true;
	}
	return IsTimeLimitExceeded( TEXT("finding existing exports") ) ? LINKER_TimedOut : LINKER_Loaded;
}

/**
 * Finalizes linker creation, adding linker to loaders array and potentially verifying imports.
 */
FLinkerLoad::ELinkerStatus FLinkerLoad::FinalizeCreation()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::FinalizeCreation" ), STAT_LinkerLoad_FinalizeCreation, STATGROUP_LinkerLoad );

	if( bHasFinishedInitialization == false )
	{
#if WITH_EDITOR
		LoadProgressScope->EnterProgressFrame(1);
#endif

		// Add this linker to the object manager's linker array.
		FLinkerManager::Get().AddLoader(this);

		// check if the package source matches the package filename's CRC (if it doesn't match, a user saved this package)
		if (Summary.PackageSource != FCrc::StrCrc_DEPRECATED(*FPaths::GetBaseFilename(Filename).ToUpper()))
		{
//			UE_LOG(LogLinker, Log, TEXT("Found a user created package (%s)"), *(FPaths::GetBaseFilename(Filename)));
		}

		if (GEventDrivenLoaderEnabled && AsyncRoot)
		{
			for (int32 ImportIndex = 0; ImportIndex < ImportMap.Num(); ++ImportIndex)
			{
				FPackageIndex Index = FPackageIndex::FromImport(ImportIndex);
				AsyncRoot->ObjectNameToImportOrExport.Add(Imp(Index).ObjectName, Index);
			}
			for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
			{
				FPackageIndex Index = FPackageIndex::FromExport(ExportIndex);
				AsyncRoot->ObjectNameToImportOrExport.Add(Exp(Index).ObjectName, Index);
			}
		}

		if (bLoaderIsFArchiveAsync2)
		{
			GetFArchiveAsync2Loader()->EndReadingHeader();
		}

		if ( !(LoadFlags & LOAD_NoVerify) )
		{
			Verify();
		}


		// Avoid duplicate work in the case of async linker creation.
		bHasFinishedInitialization = true;

#if WITH_EDITOR
		delete LoadProgressScope;
		LoadProgressScope = nullptr;
#endif
	}

	return IsTimeLimitExceeded( TEXT("finalizing creation") ) ? LINKER_TimedOut : LINKER_Loaded;
}

/**
 * Before loading anything objects off disk, this function can be used to discover
 * the object in memory. This could happen in the editor when you save a package (which
 * destroys the linker) and then play PIE, which would cause the Linker to be
 * recreated. However, the objects are still in memory, so there is no need to reload
 * them.
 *
 * @param ExportIndex	The index of the export to hunt down
 * @return The object that was found, or NULL if it wasn't found
 */
UObject* FLinkerLoad::FindExistingExport(int32 ExportIndex)
{
	check(ExportMap.IsValidIndex(ExportIndex));
	FObjectExport& Export = ExportMap[ExportIndex];

	// if we were already found, leave early
	if (Export.Object)
	{
		return Export.Object;
	}

	// find the outer package for this object, if it's already loaded
	UObject* OuterObject = NULL;
	if (Export.OuterIndex.IsNull())
	{
		// this export's outer is the UPackage root of this loader
		OuterObject = LinkerRoot;
	}
	else
	{
		// if we have a PackageIndex, then we are in a group or other object, and we should look for it
		OuterObject = FindExistingExport(Export.OuterIndex.ToExport());
	}

	// if we found one, keep going. if we didn't find one, then this package has never been loaded before
	if (OuterObject)
	{
		// find the class of this object
		UClass* TheClass;
		if (Export.ClassIndex.IsNull())
		{
			TheClass = UClass::StaticClass();
		}
		else
		{
			// Check if this object export is a non-native class, non-native classes are always exports.
			// If so, then use the outer object as a package.
			UObject* ClassPackage = Export.ClassIndex.IsExport() ? LinkerRoot : ANY_PACKAGE;

			TheClass = (UClass*)StaticFindObject(UClass::StaticClass(), ClassPackage, *ImpExp(Export.ClassIndex).ObjectName.ToString(), false);
		}

		// if the class exists, try to find the object
		if (TheClass)
		{
			TheClass->GetDefaultObject(); // build the CDO if it isn't already built
			Export.Object = StaticFindObject(TheClass, OuterObject, *Export.ObjectName.ToString(), 1);

			// if we found an object, set it's linker to us
			if (Export.Object)
			{
				Export.Object->SetLinker(this, ExportIndex);
			}
		}
	}

	return Export.Object;
}

void FLinkerLoad::Verify()
{
	if(!FApp::IsGame() || GIsEditor || IsRunningCommandlet())
	{
		if (!bHaveImportsBeenVerified)
		{
#if WITH_EDITOR
			FScopedSlowTask SlowTask(Summary.ImportCount, NSLOCTEXT("Core", "LinkerLoad_Imports", "Loading Imports"), ShouldReportProgress());
#endif
			// Validate all imports and map them to their remote linkers.
			for (int32 ImportIndex = 0; ImportIndex < Summary.ImportCount; ImportIndex++)
			{
				FObjectImport& Import = ImportMap[ImportIndex];

#if WITH_EDITOR
				SlowTask.EnterProgressFrame(1, FText::Format(NSLOCTEXT("Core", "LinkerLoad_LoadingImportName", "Loading Import '{0}'"), FText::FromString(Import.ObjectName.ToString())));
#endif
				VerifyImport( ImportIndex );
			}
		}
	}

	bHaveImportsBeenVerified = true;
}

FName FLinkerLoad::GetExportClassPackage( int32 i )
{
	FObjectExport& Export = ExportMap[ i ];
	if( Export.ClassIndex.IsImport() )
	{
		FObjectImport& Import = Imp(Export.ClassIndex);
		return ImpExp(Import.OuterIndex).ObjectName;
	}
	else if ( !Export.ClassIndex.IsNull() )
	{
		// the export's class is contained within the same package
		return LinkerRoot->GetFName();
	}
#if WITH_EDITORONLY_DATA
	else if (GLinkerAllowDynamicClasses && (Export.DynamicType == FObjectExport::EDynamicType::DynamicType))
	{
		static FName NAME_EnginePackage(TEXT("/Script/Engine"));
		return NAME_EnginePackage;
	}
#endif
	else
	{
		return GLongCoreUObjectPackageName;
	}
}

FString FLinkerLoad::GetArchiveName() const
{
	return *Filename;
}


/**
 * Recursively gathers the dependencies of a given export (the recursive chain of imports
 * and their imports, and so on)

 * @param ExportIndex Index into the linker's ExportMap that we are checking dependencies
 * @param Dependencies Array of all dependencies needed
 * @param bSkipLoadedObjects Whether to skip already loaded objects when gathering dependencies
 */
#if WITH_EDITORONLY_DATA
void FLinkerLoad::GatherExportDependencies(int32 ExportIndex, TSet<FDependencyRef>& Dependencies, bool bSkipLoadedObjects)
{
	// make sure we have dependencies
	// @todo: remove this check after all packages have been saved up to VER_ADDED_LINKER_DEPENDENCIES
	if (DependsMap.Num() == 0)
	{
		return;
	}

	// validate data
	check(DependsMap.Num() == ExportMap.Num());

	// get the list of imports the export needs
	TArray<FPackageIndex>& ExportDependencies = DependsMap[ExportIndex];

//UE_LOG(LogLinker, Warning, TEXT("Gathering dependencies for %s"), *GetExportFullName(ExportIndex));

	for (int32 DependIndex = 0; DependIndex < ExportDependencies.Num(); DependIndex++)
	{
		FPackageIndex ObjectIndex = ExportDependencies[DependIndex];

		// if it's an import, use the import version to recurse (which will add the export the import points to to the array)
		if (ObjectIndex.IsImport())
		{
			GatherImportDependencies(ObjectIndex.ToImport(), Dependencies, bSkipLoadedObjects);
		}
		else
		{
			int32 RefExportIndex = ObjectIndex.ToExport();
			FObjectExport& Export = ExportMap[RefExportIndex];

			if( (Export.Object) && ( bSkipLoadedObjects == true ) )
			{
				continue;
			}

			// fill out the ref
			FDependencyRef NewRef;
			NewRef.Linker = this;
			NewRef.ExportIndex = RefExportIndex;

			// Add to set and recurse if not already present.
			bool bIsAlreadyInSet = false;
			Dependencies.Add( NewRef, &bIsAlreadyInSet );
			if (!bIsAlreadyInSet && NewRef.Linker)
			{
				NewRef.Linker->GatherExportDependencies(RefExportIndex, Dependencies, bSkipLoadedObjects);
			}
		}
	}
}

/**
 * Recursively gathers the dependencies of a given import (the recursive chain of imports
 * and their imports, and so on). Will add itself to the list of dependencies

 * @param ImportIndex Index into the linker's ImportMap that we are checking dependencies
 * @param Dependencies Set of all dependencies needed
 * @param bSkipLoadedObjects Whether to skip already loaded objects when gathering dependencies
 */
void FLinkerLoad::GatherImportDependencies(int32 ImportIndex, TSet<FDependencyRef>& Dependencies, bool bSkipLoadedObjects)
{
	// get the import
	FObjectImport& Import = ImportMap[ImportIndex];

	// we don't need the top level package imports to be checked, since there is no real object associated with them
	if (Import.OuterIndex.IsNull())
	{
		return;
	}
	//	UE_LOG(LogLinker, Warning, TEXT("  Dependency import %s [%x, %d]"), *GetImportFullName(ImportIndex), Import.SourceLinker, Import.SourceIndex);

	// if the object already exists, we don't need this import
	if (Import.XObject)
	{
		return;
	}

	BeginLoad(TEXT("GatherImportDependencies"));

	// load the linker and find export in sourcelinker
	if (Import.SourceLinker == NULL || Import.SourceIndex == INDEX_NONE)
	{
#if DO_CHECK
		int32 NumObjectsBefore = GUObjectArray.GetObjectArrayNum();
#endif

		// temp storage we can ignore
		FString Unused;

		// remember that we are gathering imports so that VerifyImportInner will no verify all imports
		bIsGatheringDependencies = true;

		// if we failed to find the object, ignore this import
		// @todo: Tag the import to not be searched again
		VerifyImportInner(ImportIndex, Unused);

		// turn off the flag
		bIsGatheringDependencies = false;

		bool bIsValidImport =
			(Import.XObject != NULL && !Import.XObject->IsNative() && (!Import.XObject->HasAnyFlags(RF_ClassDefaultObject) || !(Import.XObject->GetClass()->HasAllFlags(EObjectFlags(RF_Public | RF_Transient)) && Import.XObject->GetClass()->IsNative()))) ||
			(Import.SourceLinker != NULL && Import.SourceIndex != INDEX_NONE);

		// make sure it succeeded
		if (!bIsValidImport)
		{
			// don't print out for intrinsic native classes
			if (!Import.XObject || !(Import.XObject->GetClass()->HasAnyClassFlags(CLASS_Intrinsic)))
			{
				UE_LOG(LogLinker, Warning, TEXT("VerifyImportInner failed [(%x, %d), (%x, %d)] for %s with linker: %s"), 
					Import.XObject, Import.XObject ? (Import.XObject->IsNative() ? 1 : 0) : 0, 
					Import.SourceLinker, Import.SourceIndex, 
					*GetImportFullName(ImportIndex), *this->Filename );
			}
			EndLoad();
			return;
		}

#if DO_CHECK && !NO_LOGGING
		// only object we should create are one FLinkerLoad for source linker
		if (GUObjectArray.GetObjectArrayNum() - NumObjectsBefore > 2)
		{
			UE_LOG(LogLinker, Warning, TEXT("Created %d objects checking %s"), GUObjectArray.GetObjectArrayNum() - NumObjectsBefore, *GetImportFullName(ImportIndex));
		}
#endif
	}

	// save off information BEFORE calling EndLoad so that the Linkers are still associated
	FDependencyRef NewRef;
	if (Import.XObject)
	{
		UE_LOG(LogLinker, Warning, TEXT("Using non-native XObject %s!!!"), *Import.XObject->GetFullName());
		NewRef.Linker = Import.XObject->GetLinker();
		NewRef.ExportIndex = Import.XObject->GetLinkerIndex();
	}
	else
	{
		NewRef.Linker = Import.SourceLinker;
		NewRef.ExportIndex = Import.SourceIndex;
	}

	EndLoad();

	// Add to set and recurse if not already present.
	bool bIsAlreadyInSet = false;
	Dependencies.Add( NewRef, &bIsAlreadyInSet );
	if (!bIsAlreadyInSet && NewRef.Linker)
	{
		NewRef.Linker->GatherExportDependencies(NewRef.ExportIndex, Dependencies, bSkipLoadedObjects);
	}
}
#endif

FLinkerLoad::EVerifyResult FLinkerLoad::VerifyImport(int32 ImportIndex)
{
	check(!GEventDrivenLoaderEnabled || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);

	FObjectImport& Import = ImportMap[ImportIndex];

	// keep a string of modifiers to add to the Editor Warning dialog
	FString WarningAppend;

	// try to load the object, but don't print any warnings on error (so we can try the redirector first)
	// note that a true return value here does not mean it failed or succeeded, just tells it how to respond to a further failure
	bool bCrashOnFail = VerifyImportInner(ImportIndex, WarningAppend);
	if (FPlatformProperties::HasEditorOnlyData() == false)
	{
		bCrashOnFail = false;
	}

	// by default, we haven't failed yet
	EVerifyResult Result = VERIFY_Success;

	// these checks find out if the VerifyImportInner was successful or not 
	if (Import.SourceLinker && Import.SourceIndex == INDEX_NONE && Import.XObject == NULL && !Import.OuterIndex.IsNull() && Import.ObjectName != NAME_ObjectRedirector)
	{
		// if we found the package, but not the object, look for a redirector
		FObjectImport OriginalImport = Import;
		Import.ClassName = NAME_ObjectRedirector;
		Import.ClassPackage = GLongCoreUObjectPackageName;

		// try again for the redirector
		VerifyImportInner(ImportIndex, WarningAppend);

		// if the redirector wasn't found, then it truly doesn't exist
		if (Import.SourceIndex == INDEX_NONE)
		{
			Result = VERIFY_Failed;
		}
		// otherwise, we found that the redirector exists
		else
		{
			// this notes that for any load errors we get that a ObjectRedirector was involved (which may help alleviate confusion
			// when people don't understand why it was trying to load an object that was redirected from or to)
			WarningAppend += LOCTEXT("LoadWarningSuffix_redirection", " [redirection]").ToString();

			// Create the redirector (no serialization yet)
			UObjectRedirector* Redir = dynamic_cast<UObjectRedirector*>(Import.SourceLinker->CreateExport(Import.SourceIndex));
			// this should probably never fail, but just in case
			if (!Redir)
			{
				Result = VERIFY_Failed;
			}
			else
			{
				// serialize in the properties of the redirector (to get the object the redirector point to)
				// Always load redirectors in case there was a circular dependency. This will allow inner redirector
				// references to always serialize fully here before accessing the DestinationObject
				check(!GEventDrivenLoaderEnabled || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);
				Redir->SetFlags(RF_NeedLoad);
				Preload(Redir);

				UObject* DestObject = Redir->DestinationObject;

				// check to make sure the destination obj was loaded,
				if ( DestObject == NULL )
				{
					Result = VERIFY_Failed;
				}
				// check that in fact it was the type we thought it should be
				else if ( DestObject->GetClass()->GetFName() != OriginalImport.ClassName

					// if the destination object is a CDO, allow class changes
					&&	!DestObject->HasAnyFlags(RF_ClassDefaultObject) )
				{
					Result = VERIFY_Failed;
					// if the destination is a ObjectRedirector you've most likely made a nasty circular loop
					if( Redir->DestinationObject->GetClass() == UObjectRedirector::StaticClass() )
					{
						WarningAppend += LOCTEXT("LoadWarningSuffix_circularredirection", " [circular redirection]").ToString();
					}
				}
				else
				{
					Result = VERIFY_Redirected;

					// now, fake our Import to be what the redirector pointed to
					Import.XObject = Redir->DestinationObject;
					FUObjectThreadContext::Get().ImportCount++;
					FLinkerManager::Get().AddLoaderWithNewImports(this);
				}
			}
		}

		// fix up the import. We put the original data back for the ClassName and ClassPackage (which are read off disk, and
		// are expected not to change)
		Import.ClassName = OriginalImport.ClassName;
		Import.ClassPackage = OriginalImport.ClassPackage;

		// if nothing above failed, then we are good to go
		if (Result != VERIFY_Failed)
		{
			// we update the runtime information (SourceIndex, SourceLinker) to point to the object the redirector pointed to
			Import.SourceIndex = Import.XObject->GetLinkerIndex();
			Import.SourceLinker = Import.XObject->GetLinker();
		}
		else
		{
			// put us back the way we were and peace out
			Import = OriginalImport;

			// if the original VerifyImportInner told us that we need to throw an exception if we weren't redirected,
			// then do the throw here
			if (bCrashOnFail)
			{
				UE_LOG(LogLinker, Fatal,  TEXT("Failed import: %s %s (file %s)"), *Import.ClassName.ToString(), *GetImportFullName(ImportIndex), *Import.SourceLinker->Filename );
				return Result;
			}
			// otherwise just printout warnings, and if in the editor, popup the EdLoadWarnings box
			else
			{
#if WITH_EDITOR
				// print warnings in editor, standalone game, or commandlet
				bool bSupressLinkerError = IsSuppressableBlueprintImportError(ImportIndex);
				if (!bSupressLinkerError)
				{
					FDeferredMessageLog LoadErrors(NAME_LoadErrors);
					// put something into the load warnings dialog, with any extra information from above (in WarningAppend)
					TSharedRef<FTokenizedMessage> TokenizedMessage = LoadErrors.Error(FText());
					TokenizedMessage->AddToken(FAssetNameToken::Create(LinkerRoot->GetName()));
					TokenizedMessage->AddToken(FTextToken::Create(FText::Format(LOCTEXT("ImportFailure", " : Failed import for {0}"), FText::FromName(GetImportClassName(ImportIndex)))));
					TokenizedMessage->AddToken(FAssetNameToken::Create(GetImportPathName(ImportIndex)));

					if (!WarningAppend.IsEmpty())
					{
						TokenizedMessage->AddToken(FTextToken::Create(FText::Format(LOCTEXT("ImportFailure_WarningIn", "{0} in {1}"),
							FText::FromString(WarningAppend),
							FText::FromString(LinkerRoot->GetName())))
							);
					}

					// Go through the depend map of the linker to find out what exports are referencing this import
					const FPackageIndex ImportPackageIndex = FPackageIndex::FromImport(ImportIndex);
					for (int32 CurrentExportIndex = 0; CurrentExportIndex < DependsMap.Num(); ++CurrentExportIndex)
					{
						const TArray<FPackageIndex>& DependsList = DependsMap[CurrentExportIndex];
						if (DependsList.Contains(ImportPackageIndex))
						{
							TokenizedMessage->AddToken(FTextToken::Create(
								FText::Format(LOCTEXT("ImportFailureExportReference", "Referenced by export {0}"),
									FText::FromName(GetExportClassName(CurrentExportIndex)))));
							TokenizedMessage->AddToken(FAssetNameToken::Create(GetExportPathName(CurrentExportIndex)));
						}
					}

					// try to get a pointer to the class of the original object so that we can display the class name of the missing resource
					UObject* ClassPackage = FindObject<UPackage>(nullptr, *Import.ClassPackage.ToString());
					UClass* FindClass = ClassPackage ? FindObject<UClass>(ClassPackage, *OriginalImport.ClassName.ToString()) : nullptr;

					// print warning about missing class
					if (!FindClass)
					{
						UE_LOG(LogLinker, Warning, TEXT("Missing Class %s for '%s' referenced by package '%s'.  Classes should not be removed if referenced by content; mark the class 'deprecated' instead."),
							*OriginalImport.ClassName.ToString(),
							*GetImportFullName(ImportIndex),
							*LinkerRoot->GetName());
					}
				}
#endif // WITH_EDITOR
			}
		}
	}

	return Result;
}

// Internal Load package call so that we can pass the linker that requested this package as an import dependency
UPackage* LoadPackageInternal(UPackage* InOuter, const TCHAR* InLongPackageName, uint32 LoadFlags, FLinkerLoad* ImportLinker);

/**
 * Safely verify that an import in the ImportMap points to a good object. This decides whether or not
 * a failure to load the object redirector in the wrapper is a fatal error or not (return value)
 *
 * @param	i	The index into this packages ImportMap to verify
 *
 * @return true if the wrapper should crash if it can't find a good object redirector to load
 */
bool FLinkerLoad::VerifyImportInner(const int32 ImportIndex, FString& WarningSuffix)
{
	check(!GEventDrivenLoaderEnabled || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);

	check(IsLoading());

	FObjectImport& Import = ImportMap[ImportIndex];

#if WITH_EDITOR
	FScopedSlowTask SlowTask(100, FText::Format(NSLOCTEXT("Core", "VerifyPackage_Scope", "Verifying '{0}'"), FText::FromName(Import.ObjectName)), ShouldReportProgress());
#endif

	if
	(	(Import.SourceLinker && Import.SourceIndex != INDEX_NONE)
	||	Import.ClassPackage	== NAME_None
	||	Import.ClassName	== NAME_None
	||	Import.ObjectName	== NAME_None )
	{
		// Already verified, or not relevent in this context.
		return false;
	}

	bool SafeReplace = false;
	UObject* Pkg=NULL;
	UPackage* TmpPkg=NULL;

	// Find or load the linker load that contains the FObjectExport for this import
	if (Import.OuterIndex.IsNull() && Import.ClassName!=NAME_Package )
	{
		UE_LOG(LogLinker, Error, TEXT("%s has an inappropriate outermost, it was probably saved with a deprecated outer (file: %s)"), *Import.ObjectName.ToString(), *Filename);
		Import.SourceLinker = NULL;
		return false;
	}
	else if( Import.OuterIndex.IsNull() )
	{
		// our Outer is a UPackage
		check(Import.ClassName==NAME_Package);
		uint32 InternalLoadFlags = LoadFlags & (LOAD_NoVerify|LOAD_NoWarn|LOAD_Quiet);

		// Check if the package has already been fully loaded, then we can skip the linker.
		bool bWasFullyLoaded = false;
		if (FPlatformProperties::RequiresCookedData())
		{
			TmpPkg = FindObjectFast<UPackage>(NULL, Import.ObjectName);
			bWasFullyLoaded = TmpPkg && TmpPkg->IsFullyLoaded();
		}

#if WITH_EDITOR
		SlowTask.EnterProgressFrame(30);
#endif

		if (!bWasFullyLoaded)
		{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			// when LOAD_DeferDependencyLoads is in play, we usually head off 
			// dependency loads before we get to this point, but there are two 
			// cases where we can reach here intentionally: 
			//
			//   1) the package we're attempting to load is native (and thusly,
			//      LoadPackageInternal() should fail, and retrun null)
			//
			//   2) the package we're attempting to load is a user defined 
			//      struct asset, which we need to load because the blueprint 
			//      class's layout depends on the struct's size... in this case, 
			//      we choke off circular loads by propagating this flag along 
			//      to the struct linker (so it doesn't load any blueprints)
			InternalLoadFlags |= (LoadFlags & LOAD_DeferDependencyLoads);
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

			// we now fully load the package that we need a single export from - however, we still use CreatePackage below as it handles all cases when the package
			// didn't exist (native only), etc		
			TmpPkg = LoadPackageInternal(NULL, *Import.ObjectName.ToString(), InternalLoadFlags | LOAD_IsVerifying, this);
		}

#if WITH_EDITOR
		SlowTask.EnterProgressFrame(30);
#endif

		// following is the original VerifyImport code
		// @todo linkers: This could quite possibly be cleaned up
		if (TmpPkg == NULL)
		{
			TmpPkg = CreatePackage( NULL, *Import.ObjectName.ToString() );
		}

		// if we couldn't create the package or it is 
		// to be linked to any other package's ImportMaps
		if ( !TmpPkg || TmpPkg->HasAnyPackageFlags(PKG_Compiling) )
		{
			return false;
		}

		// while gathering dependencies, there is no need to verify all of the imports for the entire package
		if (bIsGatheringDependencies)
		{
			InternalLoadFlags |= LOAD_NoVerify;
		}

#if WITH_EDITOR
		SlowTask.EnterProgressFrame(40);
#endif

		// Get the linker if the package hasn't been fully loaded already.
		if (!bWasFullyLoaded)
		{
			Import.SourceLinker = GetPackageLinker( TmpPkg, NULL, InternalLoadFlags, NULL, NULL );
		}
	}
	else
	{
		// this resource's Outer is not a UPackage
		checkf(Import.OuterIndex.IsImport(),TEXT("Outer for Import %s (%i) is not an import - OuterIndex:%i"), *GetImportFullName(ImportIndex), ImportIndex, Import.OuterIndex.ForDebugging());

#if WITH_EDITOR
		SlowTask.EnterProgressFrame(50);
#endif

		VerifyImport( Import.OuterIndex.ToImport() );

		FObjectImport& OuterImport = Imp(Import.OuterIndex);

		if (!OuterImport.SourceLinker && OuterImport.XObject)
		{
			FObjectImport* Top;
			for (Top = &OuterImport;	Top->OuterIndex.IsImport(); Top = &Imp(Top->OuterIndex))
			{
				// for loop does what we need
			}

			auto* Package = dynamic_cast<UPackage*>(Top->XObject);
			if (Package && Package->HasAnyPackageFlags(PKG_InMemoryOnly))
			{
				// This is an import to a memory-only package, just search for it in the package.
				TmpPkg = Package;
			}
		}

		// Copy the SourceLinker from the FObjectImport for our Outer if the SourceLinker hasn't been set yet,
		// Otherwise we may be overwriting a re-directed linker and SourceIndex is already from the redirected one.
		// This can only happen in non-cooked builds though.
		if (FPlatformProperties::RequiresCookedData() || !Import.SourceLinker)
		{
			Import.SourceLinker = OuterImport.SourceLinker;
		}

#if WITH_EDITOR
		SlowTask.EnterProgressFrame(50);
#endif

		//check(Import.SourceLinker);
		//@todo what does it mean if we don't have a SourceLinker here?
		if( Import.SourceLinker )
		{
			FObjectImport* Top;
			for (Top = &Import;	Top->OuterIndex.IsImport(); Top = &Imp(Top->OuterIndex))
			{
				// for loop does what we need
			}

			// Top is now pointing to the top-level UPackage for this resource
			Pkg = CreatePackage(NULL, *Top->ObjectName.ToString() );

			// Find this import within its existing linker.
			int32 iHash = HashNames( Import.ObjectName, Import.ClassName, Import.ClassPackage) & (ARRAY_COUNT(ExportHash)-1);

			//@Package name transition, if we can match without shortening the names, then we must not take a shortened match
			bool bMatchesWithoutShortening = false;
			FName TestName = Import.ClassPackage;
			
			for( int32 j=Import.SourceLinker->ExportHash[iHash]; j!=INDEX_NONE; j=Import.SourceLinker->ExportMap[j].HashNext )
			{
				if (!Import.SourceLinker->ExportMap.IsValidIndex(j))
				{
					UE_LOG(LogLinker, Error, TEXT("Invalid index [%d/%d] while attempting to import '%s' with LinkerRoot '%s'"), j, Import.SourceLinker->ExportMap.Num(), *Import.ObjectName.ToString(), *GetNameSafe(Import.SourceLinker->LinkerRoot));
					break;
				}
				else
				{
					FObjectExport& SourceExport = Import.SourceLinker->ExportMap[ j ];
					if
						(
						SourceExport.ObjectName == Import.ObjectName
						&&	Import.SourceLinker->GetExportClassName(j) == Import.ClassName
						&&  Import.SourceLinker->GetExportClassPackage(j) == Import.ClassPackage 
						)
					{
						bMatchesWithoutShortening = true;
						break;
					}
				}
			}
			if (!bMatchesWithoutShortening)
			{
				TestName = FPackageName::GetShortFName(TestName);
			}

			for( int32 j=Import.SourceLinker->ExportHash[iHash]; j!=INDEX_NONE; j=Import.SourceLinker->ExportMap[j].HashNext )
			{
				if (!ensureMsgf(Import.SourceLinker->ExportMap.IsValidIndex(j), TEXT("Invalid index [%d/%d] while attempting to import '%s' with LinkerRoot '%s'"),
					j, Import.SourceLinker->ExportMap.Num(), *Import.ObjectName.ToString(), *GetNameSafe(Import.SourceLinker->LinkerRoot)))
				{
					break;
				}
				else
				{
					FObjectExport& SourceExport = Import.SourceLinker->ExportMap[ j ];
					if
					(	
						SourceExport.ObjectName==Import.ObjectName               
						&&	Import.SourceLinker->GetExportClassName(j)==Import.ClassName
						&&  (bMatchesWithoutShortening ? Import.SourceLinker->GetExportClassPackage(j) : FPackageName::GetShortFName(Import.SourceLinker->GetExportClassPackage(j))) == TestName 
					)
					{
						// at this point, SourceExport is an FObjectExport in another linker that looks like it
						// matches the FObjectImport we're trying to load - double check that we have the correct one
						if( Import.OuterIndex.IsImport() )
						{
							// OuterImport is the FObjectImport for this resource's Outer
							if( OuterImport.SourceLinker )
							{
								// if the import for our Outer doesn't have a SourceIndex, it means that
								// we haven't found a matching export for our Outer yet.  This should only
								// be the case if our Outer is a top-level UPackage
								if( OuterImport.SourceIndex==INDEX_NONE )
								{
									// At this point, we know our Outer is a top-level UPackage, so
									// if the FObjectExport that we found has an Outer that is
									// not a linker root, this isn't the correct resource
									if( !SourceExport.OuterIndex.IsNull() )
									{
										continue;
									}
								}

								// The import for our Outer has a matching export - make sure that the import for
								// our Outer is pointing to the same export as the SourceExport's Outer
								else if( FPackageIndex::FromExport(OuterImport.SourceIndex) != SourceExport.OuterIndex )
								{
									continue;
								}
							}
						}
						if( !(SourceExport.ObjectFlags & RF_Public) )
						{
							SafeReplace = SafeReplace || (GIsEditor && !IsRunningCommandlet());

							// determine if this find the thing that caused this import to be saved into the map
							FPackageIndex FoundIndex = FPackageIndex::FromImport(ImportIndex);
							for ( int32 i = 0; i < Summary.ExportCount; i++ )
							{
								FObjectExport& Export = ExportMap[i];
								if ( Export.SuperIndex == FoundIndex )
								{
									UE_LOG(LogLinker, Log, TEXT("Private import was referenced by export '%s' (parent)"), *Export.ObjectName.ToString());
									SafeReplace = false;
								}
								else if ( Export.ClassIndex == FoundIndex )
								{
									UE_LOG(LogLinker, Log, TEXT("Private import was referenced by export '%s' (class)"), *Export.ObjectName.ToString());
									SafeReplace = false;
								}
								else if ( Export.OuterIndex == FoundIndex )
								{
									UE_LOG(LogLinker, Log, TEXT("Private import was referenced by export '%s' (outer)"), *Export.ObjectName.ToString());
									SafeReplace = false;
								}
							}
							for ( int32 i = 0; i < Summary.ImportCount; i++ )
							{
								if ( i != ImportIndex )
								{
									FObjectImport& TestImport = ImportMap[i];
									if ( TestImport.OuterIndex == FoundIndex )
									{
										UE_LOG(LogLinker, Log, TEXT("Private import was referenced by import '%s' (outer)"), *Import.ObjectName.ToString());
										SafeReplace = false;
									}
								}
							}

							if ( !SafeReplace )
							{
								UE_LOG(LogLinker, Warning, TEXT("%s"), *FString::Printf( TEXT("Can't import private object %s %s"), *Import.ClassName.ToString(), *GetImportFullName(ImportIndex) ) );
								return false;
							}
							else
							{
								FString Suffix = LOCTEXT("LoadWarningSuffix_privateobject", " [private]").ToString();
								if ( !WarningSuffix.Contains(Suffix) )
								{
									WarningSuffix += Suffix;
								}
								break;
							}
						}

						// Found the FObjectExport for this import
						Import.SourceIndex = j;
						break;
					}
				}
			}
		}
	}

	bool bCameFromMemoryOnlyPackage = false;
	if (!Pkg && TmpPkg && TmpPkg->HasAnyPackageFlags(PKG_InMemoryOnly))
	{
		Pkg = TmpPkg; // this is a package that exists in memory only, so that is the package to search regardless of FindIfFail
		bCameFromMemoryOnlyPackage = true;

		if (IsCoreUObjectPackage(Import.ClassPackage) && Import.ClassName == NAME_Package && !TmpPkg->GetOuter())
		{
			if (Import.ObjectName == TmpPkg->GetFName())
			{
				// except if we are looking for _the_ package...in which case we are looking for TmpPkg, so we are done
				Import.XObject = TmpPkg;
				FUObjectThreadContext::Get().ImportCount++;
				FLinkerManager::Get().AddLoaderWithNewImports(this);
				return false;
			}
		}
	}

	if( (Pkg == NULL) && ((LoadFlags & LOAD_FindIfFail) != 0) )
	{
		Pkg = ANY_PACKAGE;
	}

	// If not found in file, see if it's a public native transient class or field.
	if( Import.SourceIndex==INDEX_NONE && Pkg!=NULL )
	{
		UObject* ClassPackage = FindObject<UPackage>( NULL, *Import.ClassPackage.ToString() );
		if( ClassPackage )
		{
			UClass* FindClass = FindObject<UClass>( ClassPackage, *Import.ClassName.ToString() );
			if( FindClass )
			{
				UObject* FindOuter			= Pkg;

				if ( Import.OuterIndex.IsImport() )
				{
					// if this import corresponds to an intrinsic class, OuterImport's XObject will be NULL if this import
					// belongs to the same package that the import's class is in; in this case, the package is the correct Outer to use
					// for finding this object
					// otherwise, this import represents a field of an intrinsic class, and OuterImport's XObject should be non-NULL (the object
					// that contains the field)
					FObjectImport& OuterImport	= Imp(Import.OuterIndex);
					if ( OuterImport.XObject != NULL )
					{
						FindOuter = OuterImport.XObject;
					}
				}

				UObject* FindObject = FindImport(FindClass, FindOuter, *Import.ObjectName.ToString());
				// Reference to in memory-only package's object, native transient class or CDO of such a class.
				bool bIsInMemoryOnlyOrNativeTransient = bCameFromMemoryOnlyPackage || (FindObject != NULL && ((FindObject->IsNative() && FindObject->HasAllFlags(RF_Public | RF_Transient)) || (FindObject->HasAnyFlags(RF_ClassDefaultObject) && FindObject->GetClass()->IsNative() && FindObject->GetClass()->HasAllFlags(RF_Public | RF_Transient))));
				// Check for structs which have been moved to another header (within the same class package).
				if (!FindObject && bIsInMemoryOnlyOrNativeTransient && FindClass == UScriptStruct::StaticClass())
				{
					FindObject = StaticFindObject( FindClass, ANY_PACKAGE, *Import.ObjectName.ToString(), true );
					if (FindObject && FindOuter->GetOutermost() != FindObject->GetOutermost())
					{
						// Limit the results to the same package.I
						FindObject = NULL;
					}
				}
				if (FindObject != NULL && ((LoadFlags & LOAD_FindIfFail) || bIsInMemoryOnlyOrNativeTransient))
				{
					Import.XObject = FindObject;
					FUObjectThreadContext::Get().ImportCount++;
					FLinkerManager::Get().AddLoaderWithNewImports(this);
				}
				else
				{
					SafeReplace = true;
				}
			}
			else
			{
				SafeReplace = true;
			}
		}

		if (!Import.XObject && !SafeReplace)
		{
			return true;
		}
	}
	return false;
}

UObject* FLinkerLoad::CreateExportAndPreload(int32 ExportIndex, bool bForcePreload /* = false */)
{
	UObject *Object = CreateExport(ExportIndex);
	if (Object && (bForcePreload || dynamic_cast<UClass*>(Object) || Object->IsTemplate() || dynamic_cast<UObjectRedirector*>(Object)))
	{
		Preload(Object);
	}

	return Object;
}

UClass* FLinkerLoad::GetExportLoadClass(int32 Index)
{
	FObjectExport& Export = ExportMap[Index];

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	// VerifyImport() runs the risk of loading up another package, and we can't 
	// have that when we're explicitly trying to block dependency loads...
	// if this needs a class from another package, IndexToObject() should return 
	// a ULinkerPlaceholderClass instead
	if (Export.ClassIndex.IsImport() && !(LoadFlags & LOAD_DeferDependencyLoads))
#else  // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (Export.ClassIndex.IsImport())
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	{
		// @TODO: I believe IndexToObject() -> CreateImport() will verify this  
		//        for us, if it has to; so is this necessary?
		VerifyImport(Export.ClassIndex.ToImport());
	}

	UClass* ExportClass = (UClass*)IndexToObject(Export.ClassIndex);
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	check(!Export.ClassIndex.IsImport() || !(LoadFlags & LOAD_DeferDependencyLoads) || 
		(ExportClass && ExportClass->HasAnyClassFlags(CLASS_Native)) || (Cast<ULinkerPlaceholderClass>(ExportClass) != nullptr));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	return ExportClass;
}

int32 FLinkerLoad::LoadMetaDataFromExportMap(bool bForcePreload)
{
	UMetaData* MetaData = nullptr;
	int32 MetaDataIndex = INDEX_NONE;

	// Try to find MetaData and load it first as other objects can depend on it.
	for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
	{
		if (ExportMap[ExportIndex].ObjectName == NAME_PackageMetaData)
		{
			MetaData = Cast<UMetaData>(CreateExportAndPreload(ExportIndex, bForcePreload));
			MetaDataIndex = ExportIndex;
			break;
		}
	}

	// If not found then try to use old name and rename.
	if (MetaDataIndex == INDEX_NONE)
	{
		for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
		{
			if (ExportMap[ExportIndex].ObjectName == *UMetaData::StaticClass()->GetName())
			{
				UObject* Object = CreateExportAndPreload(ExportIndex, bForcePreload);
				Object->Rename(*FName(NAME_PackageMetaData).ToString(), NULL, REN_ForceNoResetLoaders);

				MetaData = Cast<UMetaData>(Object);
				MetaDataIndex = ExportIndex;
				break;
			}
		}
	}

	// Make sure the meta-data is referenced by its package to avoid premature GC
	if (LinkerRoot)
	{
		LinkerRoot->MetaData = MetaData;
	}

	return MetaDataIndex;
}

/**
 * Loads all objects in package.
 *
 * @param bForcePreload	Whether to explicitly call Preload (serialize) right away instead of being
 *						called from EndLoad()
 */
void FLinkerLoad::LoadAllObjects(bool bForcePreload)
{
#if WITH_EDITOR
	FScopedSlowTask SlowTask(ExportMap.Num(), NSLOCTEXT("Core", "LinkerLoad_LoadingObjects", "Loading Objects"), ShouldReportProgress());
	SlowTask.Visibility = ESlowTaskVisibility::Invisible;
#endif

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	// if we're re-entering a call to LoadAllObjects() while DeferDependencyLoads
	// is set, then we're not doing our job (we're risking an export needing 
	// another external asset)... if this is hit, then we're most likely already
	// in this function (for this linker) further up the load chain; it should 
	// finish the loads there
	check((LoadFlags & LOAD_DeferDependencyLoads) == 0);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	if ((LoadFlags & LOAD_Async) != 0)
	{
		bForcePreload = true;
	}

	double StartTime = FPlatformTime::Seconds();

	// MetaData object index in this package.
	int32 MetaDataIndex = INDEX_NONE;

	if(!FPlatformProperties::RequiresCookedData())
	{
		MetaDataIndex = LoadMetaDataFromExportMap(bForcePreload);
	}
	
#if USE_STABLE_LOCALIZATION_KEYS
	if (GIsEditor && (LoadFlags & LOAD_ForDiff))
	{
		// If this package is being loaded for diffing, then we need to force it to have a unique package localization ID to avoid in-memory identity conflicts
		// Note: We set this on the archive first as finding/loading the meta-data (which ForcePackageNamespace does) may trigger the load of some objects within this package
		const FString PackageLocalizationId = FGuid::NewGuid().ToString();
		SetLocalizationNamespace(PackageLocalizationId);
		TextNamespaceUtil::ForcePackageNamespace(LinkerRoot, PackageLocalizationId);
	}
#endif // USE_STABLE_LOCALIZATION_KEYS

	// Tick the heartbeat if we're loading on the game thread
	const bool bShouldTickHeartBeat = IsInGameThread();

	for(int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
	{
#if WITH_EDITOR
		SlowTask.EnterProgressFrame(1);
#endif
		if (ExportIndex == MetaDataIndex)
		{
			continue;
		}

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
		// this is here to prevent infinite recursion; if IsExportBeingResolved() 
		// returns true, then that means the export's class is currently being 
		// force-generated... in that scenario, the export's Object member would 
		// not have been set yet, and the call below to CreateExport() would put 
		// us right back here in the same situation (CreateExport() needs the 
		// export's Object set in order to return early... it's what makes this 
		// function reentrant)
		//
		// since we don't actually use the export object here at this point, 
		// then it is safe to skip over it (it's already being created further 
		// up the callstack, so don't worry about it being missed)
		if (IsExportBeingResolved(ExportIndex))
		{
			continue;
		}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

		UObject* LoadedObject = CreateExportAndPreload(ExportIndex, bForcePreload);

		if(!GEventDrivenLoaderEnabled || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
		{
			// DynamicClass could be created without calling CreateImport. The imported objects will be required later when a CDO is created.
			if (UDynamicClass* DynamicClass = Cast<UDynamicClass>(LoadedObject))
			{
				for (int32 ImportIndex = 0; ImportIndex < ImportMap.Num(); ++ImportIndex)
				{
					CreateImport(ImportIndex);
				}
			}
		}

		// If needed send a heartbeat, but no need to do it too often
		if (bShouldTickHeartBeat && (ExportIndex % 10) == 0)
		{
			FThreadHeartBeat::Get().HeartBeat();
		}
	}

	// Mark package as having been fully loaded.
	if( LinkerRoot )
	{
		LinkerRoot->MarkAsFullyLoaded();
	}
}

/**
 * Returns the ObjectName associated with the resource indicated.
 * 
 * @param	ResourceIndex	location of the object resource
 *
 * @return	ObjectName for the FObjectResource at ResourceIndex, or NAME_None if not found
 */
FName FLinkerLoad::ResolveResourceName( FPackageIndex ResourceIndex )
{
	if (ResourceIndex.IsNull())
	{
		return NAME_None;
	}
	return ImpExp(ResourceIndex).ObjectName;
}

// Find the index of a specified object without regard to specific package.
int32 FLinkerLoad::FindExportIndex( FName ClassName, FName ClassPackage, FName ObjectName, FPackageIndex ExportOuterIndex )
{
	int32 iHash = HashNames( ObjectName, ClassName, ClassPackage ) & (ARRAY_COUNT(ExportHash)-1);

	for( int32 i=ExportHash[iHash]; i!=INDEX_NONE; i=ExportMap[i].HashNext )
	{
		if (!ensureMsgf(ExportMap.IsValidIndex(i), TEXT("Invalid index [%d/%d] while attempting to find export index '%s' LinkerRoot '%s'"), i, ExportMap.Num(), *ObjectName.ToString(), *GetNameSafe(LinkerRoot)))
		{
			break;
		}
		else
		{
			if
			(  (ExportMap[i].ObjectName  ==ObjectName                              )
				&& (GetExportClassPackage(i) ==ClassPackage                            )
				&& (GetExportClassName   (i) ==ClassName                               ) 
				&& (ExportMap[i].OuterIndex  ==ExportOuterIndex 
				|| ExportOuterIndex.IsImport()) // this is very not legit to be passing INDEX_NONE into this function to mean "ignore"
			)
			{
				return i;
			}
		}
	}
	
	// If an object with the exact class wasn't found, look for objects with a subclass of the requested class.
	for(int32 ExportIndex = 0;ExportIndex < ExportMap.Num();ExportIndex++)
	{
		FObjectExport&	Export = ExportMap[ExportIndex];

		if(Export.ObjectName == ObjectName && (ExportOuterIndex.IsImport() || Export.OuterIndex == ExportOuterIndex)) // this is very not legit to be passing INDEX_NONE into this function to mean "ignore"
		{
			UClass*	ExportClass = dynamic_cast<UClass*>(IndexToObject(Export.ClassIndex));

			// See if this export's class inherits from the requested class.
			for(UClass* ParentClass = ExportClass;ParentClass;ParentClass = ParentClass->GetSuperClass())
			{
				if(ParentClass->GetFName() == ClassName)
				{
					return ExportIndex;
				}
			}
		}
	}

	return INDEX_NONE;
}

/**
 * Function to create the instance of, or verify the presence of, an object as found in this Linker.
 *
 * @param ObjectClass	The class of the object
 * @param ObjectName	The name of the object
 * @param Outer			Find the object inside this outer (and only directly inside this outer, as we require fully qualified names)
 * @param LoadFlags		Flags used to determine if the object is being verified or should be created
 * @param Checked		Whether or not a failure will throw an error
 * @return The created object, or (UObject*)-1 if this is just verifying
 */
UObject* FLinkerLoad::Create( UClass* ObjectClass, FName ObjectName, UObject* Outer, uint32 InLoadFlags, bool Checked )
{
	// We no longer handle a NULL outer, which used to mean look in any outer, but we need fully qualified names now
	// The other case where this was NULL is if you are calling StaticLoadObject on the top-level package, but
	// you should be using LoadPackage. If for some weird reason you need to load the top-level package with this,
	// then I believe you'd want to set OuterIndex to 0 when Outer is NULL, but then that could get confused with
	// loading A.A (they both have OuterIndex of 0, according to Ron)
	check(Outer);

	int32 OuterIndex = INDEX_NONE;

	// if the outer is the outermost of the package, then we want OuterIndex to be 0, as objects under the top level
	// will have an OuterIndex to 0
	if (Outer == Outer->GetOutermost())
	{
		OuterIndex = 0;
	}
	// otherwise get the linker index of the outer to be the outer index that we look in
	else
	{
		OuterIndex = Outer->GetLinkerIndex();
		// we _need_ the linker index of the outer to look in, which means that the outer must have been actually 
		// loaded off disk, and not just CreatePackage'd
		check(OuterIndex != INDEX_NONE);
	}

	FPackageIndex OuterPackageIndex;

	if (OuterIndex)
	{
		OuterPackageIndex = FPackageIndex::FromExport(OuterIndex);
	}

	int32 Index = FindExportIndex(ObjectClass->GetFName(), ObjectClass->GetOuter()->GetFName(), ObjectName, OuterPackageIndex);
	if (Index != INDEX_NONE)
	{
		return (InLoadFlags & LOAD_Verify) ? INVALID_OBJECT : CreateExport(Index);
	}

	// since we didn't find it, see if we can find an object redirector with the same name
	// Are we allowed to follow redirects?
	if( !( InLoadFlags & LOAD_NoRedirects ) )
	{
		Index = FindExportIndex(UObjectRedirector::StaticClass()->GetFName(), NAME_CoreUObject, ObjectName, OuterPackageIndex);
		if (Index == INDEX_NONE)
		{
			Index = FindExportIndex(UObjectRedirector::StaticClass()->GetFName(), GLongCoreUObjectPackageName, ObjectName, OuterPackageIndex);			
		}

		// if we found a redirector, create it, and move on down the line
		if (Index != INDEX_NONE)
		{
			// create the redirector,
			UObjectRedirector* Redir = (UObjectRedirector*)CreateExport(Index);
			Preload(Redir);
			// if we found what it was point to, then return it
			if (Redir->DestinationObject && Redir->DestinationObject->IsA(ObjectClass))
			{
				// and return the object we are being redirected to
				return Redir->DestinationObject;
			}
		}
	}


// Set this to 1 to find nonqualified names anyway
#define FIND_OBJECT_NONQUALIFIED 0
// Set this to 1 if you want to see what it would have found previously. This is useful for fixing up hundreds
// of now-illegal references in script code.
#define DEBUG_PRINT_NONQUALIFIED_RESULT 1

#if DEBUG_PRINT_NONQUALIFIED_RESULT || FIND_OBJECT_NONQUALIFIED
	Index = FindExportIndex(ObjectClass->GetFName(), ObjectClass->GetOuter()->GetFName(), ObjectName, FPackageIndex::FromImport(0));// this is very not legit to be passing INDEX_NONE into this function to mean "ignore"
	if (Index != INDEX_NONE)
	{
#if DEBUG_PRINT_NONQUALIFIED_RESULT
		UE_LOG(LogLinker, Warning, TEXT("Using a non-qualified name (would have) found: %s"), *GetExportFullName(Index));
#endif
#if FIND_OBJECT_NONQUALIFIED
		return (InLoadFlags & LOAD_Verify) ? INVALID_OBJECT : CreateExport(Index);
#endif
	}
#endif


	// if we are checking for failure cases, and we failed, throw an error
	if( Checked )
	{
		UE_LOG(LogLinker, Warning, TEXT("%s"), *FString::Printf( TEXT("%s %s not found for creation"), *ObjectClass->GetName(), *ObjectName.ToString() ));
	}
	return NULL;
}

/**
 * Serialize the object data for the specified object from the unreal package file.  Loads any
 * additional resources required for the object to be in a valid state to receive the loaded
 * data, such as the object's Outer, Class, or ObjectArchetype.
 *
 * When this function exits, Object is guaranteed to contain the data stored that was stored on disk.
 *
 * @param	Object	The object to load data for.  If the data for this object isn't stored in this
 *					FLinkerLoad, routes the call to the appropriate linker.  Data serialization is 
 *					skipped if the object has already been loaded (as indicated by the RF_NeedLoad flag
 *					not set for the object), so safe to call on objects that have already been loaded.
 *					Note that this function assumes that Object has already been initialized against
 *					its template object.
 *					If Object is a UClass and the class default object has already been created, calls
 *					Preload for the class default object as well.
 */

void FLinkerLoad::Preload( UObject* Object )
{

	//check(IsValidLowLevel());
	check(Object);

	// Preload the object if necessary.
	if (Object->HasAnyFlags(RF_NeedLoad))
	{
		if (Object->GetLinker() == this)
		{
			check(!GEventDrivenLoaderEnabled || !bLockoutLegacyOperations || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			bool const bIsNonNativeObject = !Object->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn);
			// we can determine that this is a blueprint class/struct by checking if it 
			// is a class/struct object AND if it is not native (blueprint 
			// structs/classes are the only asset package structs/classes we have)
			bool const bIsBlueprintClass = (Cast<UClass>(Object) != nullptr) && bIsNonNativeObject;
			bool const bIsBlueprintStruct = (Cast<UScriptStruct>(Object) != nullptr) && bIsNonNativeObject;
			// to avoid cyclic dependency issues, we want to defer all external loads 
			// that MAY rely on this class/struct (meaning all other blueprint packages)  
			bool const bDeferDependencyLoads = (bIsBlueprintClass || bIsBlueprintStruct) && FBlueprintSupport::UseDeferredDependencyLoading();

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			// we should NEVER be pre-loading another blueprint class when the 
			// DeferDependencyLoads flag is set (some other blueprint class/struct is  
			// already being loaded further up the load chain, and this could introduce  
			// a circular load)
			//
			// NOTE: we do allow Preload() calls for structs (because we need a struct 
			//       loaded to determine its size), but structs will be prevented from 
			//       further loading any of its BP class dependencies (we pass along the 
			//       LOAD_DeferDependencyLoads flag)
			check(!bIsBlueprintClass || !Object->HasAnyFlags(RF_NeedLoad) || !(LoadFlags & LOAD_DeferDependencyLoads));
			// right now there are no known scenarios where someone requests a Preloa() 
			// on a temporary ULinkerPlaceholderExportObject
			check(!Object->IsA<ULinkerPlaceholderExportObject>());
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			// Because of delta serialization, we require that a parent's CDO be 
			// fully serialized before its children's CDOs are created. However, 
			// due to cyclic parent/child dependencies, we have some cases where 
			// the linker breaks that expected behavior. In those cases, we 
			// defer the child's initialization (i.e. defer copying of parent  
			// property values, etc.), and wait until we can guarantee that the 
			// parent CDO has been fully loaded.
			//
			// In a normal scenario, the order of property initialization is:
			// Creation (zeroed) -> Initialization (copied super's values) -> Serialization (overridden values loaded)
			// When the initialization has been deferred we have to make sure to
			// defer serialization here as well (don't worry, it will be invoked 
			// again from FinalizeBlueprint()->ResolveDeferredExports())
			if (Object->HasAnyFlags(RF_ClassDefaultObject) && FDeferredObjInitializerTracker::IsCdoDeferred(Object->GetClass()))
			{
				return;
			}
			// if this is an inherited sub-object on a CDO, and that CDO has had
			// its initialization deferred (for reasons explained above), then 
			// we shouldn't serialize in data for this quite yet... not until 
			// its owner has had a chaTnce to initialize itself (because, as part
			// of CDO initialization, inherited sub-objects get filled in with 
			// values inherited from the super)
			else if (Object->HasAnyFlags(RF_DefaultSubObject|RF_InheritableComponentTemplate) && FDeferredObjInitializerTracker::DeferSubObjectPreload(Object))
			{
				// don't worry, FDeferredObjInitializerTracker::DeferSubObjectPreload() 
				// should have cached this object, and it will run Preload() on 
				// this later (once the super CDO has been initialized)
				return;
			}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

			SCOPE_CYCLE_COUNTER(STAT_LinkerPreload);
			FScopeCycleCounterUObject PreloadScope(Object, GET_STATID(STAT_LinkerPreload));
			UClass* Cls = NULL;
			
			// If this is a struct, make sure that its parent struct is completely loaded
			if( UStruct* Struct = dynamic_cast<UStruct*>(Object) )
			{
				Cls = dynamic_cast<UClass*>(Object);
				if( Struct->GetSuperStruct() )
				{
					Preload( Struct->GetSuperStruct() );
				}
			}

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			TGuardValue<uint32> LoadFlagsGuard(LoadFlags, LoadFlags);			
			if (bDeferDependencyLoads)
			{
				LoadFlags |= LOAD_DeferDependencyLoads;
			}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

			// make sure this object didn't get loaded in the above Preload call
			if (Object->HasAnyFlags(RF_NeedLoad))
			{
				// grab the resource for this Object
				int32 const ExportIndex = Object->GetLinkerIndex();
				FObjectExport& Export = ExportMap[ExportIndex];
				check(Export.Object==Object);

				const int64 SavedPos = Loader->Tell();

				// move to the position in the file where this object's data
				// is stored
				Loader->Seek(Export.SerialOffset);

				FArchiveAsync2* FAA2 = GetFArchiveAsync2Loader();

				{
					SCOPE_CYCLE_COUNTER(STAT_LinkerPrecache);
					// tell the file reader to read the raw data from disk
					if (FAA2)
					{
						bool bReady = FAA2->Precache(Export.SerialOffset, Export.SerialSize, bUseTimeLimit, bUseFullTimeLimit, TickStartTime, TimeLimit);
						UE_CLOG(!(bReady || !bUseTimeLimit || !FPlatformProperties::RequiresCookedData()), LogLinker, Warning, TEXT("Hitch on async loading of %s; this export was not properly precached."), *Object->GetFullName());
					}
					else
					{
						Loader->Precache(Export.SerialOffset, Export.SerialSize);
					}
				}

				// mark the object to indicate that it has been loaded
				Object->ClearFlags ( RF_NeedLoad );

				{
					SCOPE_CYCLE_COUNTER(STAT_LinkerSerialize);
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
					// communicate with FLinkerPlaceholderBase, what object is currently serializing in
					FScopedPlaceholderContainerTracker SerializingObjTracker(Object);
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

					if (Object->HasAnyFlags(RF_ClassDefaultObject))
					{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
						if ((LoadFlags & LOAD_DeferDependencyLoads) != 0)
						{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
							check((DeferredCDOIndex == INDEX_NONE) || (DeferredCDOIndex == ExportIndex));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
							
							// since serializing the CDO can introduce circular 
							// dependencies, we want to stave that off until 
							// we're ready to handle those 
							DeferredCDOIndex = ExportIndex;
							// don't need to actually "consume" the data through
							// serialization though (since we seek back to 
							// SavedPos later on)

							// reset the flag and return (don't worry, we make
							// sure to force load this later)
							check(!GEventDrivenLoaderEnabled || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);
							Object->SetFlags(RF_NeedLoad);
							return;
						}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

						FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
						// Maintain the current SerializedObjects.
						UObject* PrevSerializedObject = ThreadContext.SerializedObject;
						ThreadContext.SerializedObject = Object;

						Object->GetClass()->SerializeDefaultObject(Object, *this);
						Object->SetFlags(RF_LoadCompleted);

						ThreadContext.SerializedObject = PrevSerializedObject;
					}
					else
					{

#if WITH_EDITOR
						static const FName NAME_UObjectSerialize = FName(TEXT("UObject::Serialize, Name, ClassName"));
						FArchive::FScopeAddDebugData P(*this, NAME_UObjectSerialize);
						FArchive::FScopeAddDebugData N(*this, Object->GetFName());
						FArchive::FScopeAddDebugData C(*this, Object->GetClass()->GetFName());
#endif

						FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
						// Maintain the current SerializedObjects.
						UObject* PrevSerializedObject = ThreadContext.SerializedObject;
						ThreadContext.SerializedObject = Object;
						Object->Serialize( *this );
						Object->SetFlags(RF_LoadCompleted);
						ThreadContext.SerializedObject = PrevSerializedObject;
					}
				}

				{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
					SCOPE_CYCLE_COUNTER(STAT_LinkerLoadDeferred);
					if ((LoadFlags & LOAD_DeferDependencyLoads) != (*LoadFlagsGuard & LOAD_DeferDependencyLoads))
					{
						if (bIsBlueprintStruct)
						{
							ResolveDeferredDependencies((UScriptStruct*)Object); 
							// user-defined-structs don't have classes/CDOs, so 
							// we don't have to call FinalizeBlueprint() (to 
							// serialize/regenerate them)
						}
						else
						{
							UClass* ObjectAsClass = (UClass*)Object;
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
							check(bIsBlueprintClass);
							// since class serialization reads in the class's CDO, then we can be certain that the CDO export object exists 
							// (and DeferredExportIndex should reference it); FinalizeBlueprint() depends on DeferredExportIndex being set 
							// (and since ResolveDeferredDependencies() can recurse into FinalizeBlueprint(), we check it here, before the 
							// resolve is handled)
							//
							// however, sometimes DeferredExportIndex doesn't get set at all (we have to utilize FindCDOExportIndex() to set
							// it), and that happens when the class's ClassGeneratedBy is serialized in null... this will happen for cooked 
							// builds (because Blueprints are editor-only objects)
							check((DeferredCDOIndex != INDEX_NONE) || FPlatformProperties::RequiresCookedData());

							if (DeferredCDOIndex == INDEX_NONE)
							{
								DeferredCDOIndex = FindCDOExportIndex(ObjectAsClass);
								check(DeferredCDOIndex != INDEX_NONE);
							}
#else  // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
							// just because DeferredCDOIndex wasn't set (in cooked/PIE scenarios) doesn't mean that we don't need it 
							// (FinalizeBlueprint() relies on it being set), so here we make sure we flag the CDO so it gets resolved
							if (DeferredCDOIndex == INDEX_NONE)
							{
								DeferredCDOIndex = FindCDOExportIndex(ObjectAsClass);
							}
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

							ResolveDeferredDependencies(ObjectAsClass);
							FinalizeBlueprint(ObjectAsClass);
						}
					}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
				}

				// Make sure we serialized the right amount of stuff.
				int64 Pos = Tell();
				int64 SizeSerialized = Pos - Export.SerialOffset;
				if( SizeSerialized != Export.SerialSize )
				{
					if (Object->GetClass()->HasAnyClassFlags(CLASS_Deprecated))
					{
						UE_LOG(LogLinker, Warning, TEXT("%s"), *FString::Printf( TEXT("%s: Serial size mismatch: Got %d, Expected %d"), *Object->GetFullName(), (int32)SizeSerialized, Export.SerialSize ) );
					}
					else
					{
						UE_LOG(LogLinker, Fatal, TEXT("%s"), *FString::Printf( TEXT("%s: Serial size mismatch: Got %d, Expected %d"), *Object->GetFullName(), (int32)SizeSerialized, Export.SerialSize ) );
					}
				}

				Loader->Seek( SavedPos );

				// if this is a UClass object and it already has a class default object
				if ( Cls != NULL && Cls->GetDefaultsCount() )
				{
					// make sure that the class default object is completely loaded as well
					Preload(Cls->GetDefaultObject());
				}

#if WITH_EDITOR
				// Check if this object's class has been changed by ActiveClassRedirects.
				FName OldClassName;
				if (Export.OldClassName != NAME_None && Object->GetClass()->GetFName() != Export.OldClassName)
				{
					// This happens when the class has changed only for object instance.
					OldClassName = Export.OldClassName;
				}
				else if (Export.ClassIndex.IsImport())
				{
					// Check if the class has been renamed / replaced in the import map.
					const FObjectImport& ClassImport = Imp(Export.ClassIndex);
					if (ClassImport.OldClassName != NAME_None && ClassImport.OldClassName != Object->GetClass()->GetFName())
					{
						OldClassName = ClassImport.OldClassName;
					}
				}
				else if (Export.ClassIndex.IsExport())
				{
					// Handle blueprints. This is slightly different from the other cases as we're looking for the first 
					// native super of the blueprint class (first import).
					const FObjectExport* ClassExport = NULL;
					for (ClassExport = &Exp(Export.ClassIndex); ClassExport->SuperIndex.IsExport(); ClassExport = &Exp(Export.SuperIndex));
					if (ClassExport->SuperIndex.IsImport())
					{
						const FObjectImport& ClassImport = Imp(ClassExport->SuperIndex);
						if (ClassImport.OldClassName != NAME_None)
						{
							OldClassName = ClassImport.OldClassName;
						}
					}
				}
				if (OldClassName != NAME_None)
				{
					// Notify if the object's class has changed as a result of active class redirects.
					Object->LoadedFromAnotherClass(OldClassName);
				}
#endif

				// It's ok now to call PostLoad on blueprint CDOs
				if (Object->HasAnyFlags(RF_ClassDefaultObject) && Object->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
				{
					Object->SetFlags(RF_NeedPostLoad|RF_WasLoaded);
					FUObjectThreadContext::Get().ObjLoaded.Add(Object);
				}
			}
		}
		else if( Object->GetLinker() )
		{
			// Send to the object's linker.
			Object->GetLinker()->Preload( Object );
		}
	}
}

/**
 * Builds a string containing the full path for a resource in the export table.
 *
 * @param OutPathName		[out] Will contain the full path for the resource
 * @param ResourceIndex		Index of a resource in the export table
 */
void FLinkerLoad::BuildPathName( FString& OutPathName, FPackageIndex ResourceIndex ) const
{
	if ( ResourceIndex.IsNull() )
	{
		return;
	}
	const FObjectResource& Resource = ImpExp(ResourceIndex);
	BuildPathName( OutPathName, Resource.OuterIndex );
	if ( OutPathName.Len() > 0 )
	{
		OutPathName += TEXT('.');
	}
	OutPathName += Resource.ObjectName.ToString();
}

/**
 * Checks if the specified export should be loaded or not.
 * Performs similar checks as CreateExport().
 *
 * @param ExportIndex	Index of the export to check
 * @return				true of the export should be loaded
 */
bool FLinkerLoad::WillTextureBeLoaded( UClass* Class, int32 ExportIndex )
{
	const FObjectExport& Export = ExportMap[ ExportIndex ];

	// Already loaded?
	if ( Export.Object || FilterExport(Export))  // it was "not for" in all acceptable positions
	{
		return false;
	}

	// Build path name
	FString PathName;
	PathName.Reserve(256);
	BuildPathName( PathName, FPackageIndex::FromExport(ExportIndex) );

	UObject* ExistingTexture = StaticFindObjectFastExplicit( Class, Export.ObjectName, PathName, false, RF_NoFlags );
	if ( ExistingTexture )
	{
		return false;
	}
	else
	{
		return true;
	}
}

UObject* FLinkerLoad::CreateExport( int32 Index )
{
	FScopedCreateExportCounter ScopedCounter( this, Index );
	FDeferredMessageLog LoadErrors(NAME_LoadErrors);

	// Map the object into our table.
	FObjectExport& Export = ExportMap[ Index ];

	// Check whether we already loaded the object and if not whether the context flags allow loading it.
	if( !Export.Object && !FilterExport(Export) ) // for some acceptable position, it was not "not for" 
	{
		check(!GEventDrivenLoaderEnabled || !bLockoutLegacyOperations || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);
		check(Export.ObjectName!=NAME_None || !(Export.ObjectFlags&RF_Public));
		check(IsLoading());

		if (Export.DynamicType == FObjectExport::EDynamicType::DynamicType)
		{
			// Export is a dynamic type, construct it using registered native functions
			Export.Object = ConstructDynamicType(*GetExportPathName(Index), EConstructDynamicType::CallZConstructor);
			if (Export.Object)
			{
				Export.Object->SetLinker(this, Index);
				if (UDynamicClass* DynamicClass = Cast<UDynamicClass>(Export.Object))
				{
					// Dynamic Class doesn't require/use pre-loading (or post-loading), but at this point the class is not fully initialized. 
					// The CDO is created (in a custom code) at the end of loading (when it's safe to solve cyclic dependencies).
					if (!DynamicClass->GetDefaultObject(false))
					{
						FUObjectThreadContext::Get().ObjLoaded.Add(Export.Object);
					}
				}
			}
			return Export.Object;
		}

		UClass* LoadClass = GetExportLoadClass(Index);
		if( !LoadClass && !Export.ClassIndex.IsNull() ) // Hack to load packages with classes which do not exist.
		{
			return NULL;
		}

		if (Export.DynamicType == FObjectExport::EDynamicType::ClassDefaultObject)
		{
			if (LoadClass)
			{
				ensure(Cast<UDynamicClass>(LoadClass));
				Export.Object = LoadClass->GetDefaultObject(true);
				return Export.Object;
			}
			else
			{
				UE_LOG(LogLinker, Warning, TEXT("CreateExport: Failed to create CDO %s because class is not found"), *Export.ObjectName.ToString());
				return NULL;
			}
		}

#if WITH_EDITOR
		// NULL (None) active class redirect.
		if( !LoadClass && Export.ObjectName.IsNone() && Export.ClassIndex.IsNull() && !Export.OldClassName.IsNone() )
		{
			return NULL;
		}
#endif
		if( !LoadClass )
		{
			LoadClass = UClass::StaticClass();
		}

		UObjectRedirector* LoadClassRedirector = dynamic_cast<UObjectRedirector*>(LoadClass);
		if( LoadClassRedirector)
		{
			// mark this export as unloadable (so that other exports that
			// reference this one won't continue to execute the above logic), then return NULL
			Export.bExportLoadFailed = true;

			// otherwise, return NULL and let the calling code determine what to do
			FString OuterName = Export.OuterIndex.IsNull() ? LinkerRoot->GetFullName() : GetFullImpExpName(Export.OuterIndex);
			UE_LOG(LogLinker, Warning, TEXT("CreateExport: Failed to load Outer for resource because its class is a redirector '%s': %s"), *Export.ObjectName.ToString(), *OuterName);
			return NULL;
		}

		check(LoadClass);
		check(dynamic_cast<UClass*>(LoadClass) != NULL);

		// Check for a valid superstruct while there is still time to safely bail, if this export has one
		if( !Export.SuperIndex.IsNull() )
		{
			UStruct* SuperStruct = (UStruct*)IndexToObject( Export.SuperIndex );
			if( !SuperStruct )
			{
				if( LoadClass->IsChildOf(UFunction::StaticClass()) )
				{
					// In the case of a function object, the outer should be the function's class. For Blueprints, loading
					// the outer class may also invalidate this entry in the export map. In that case, we won't actually be
					// keeping the function object around, so there's no need to warn here about the missing parent object.
					UObject* ObjOuter = IndexToObject(Export.OuterIndex);
					if (ObjOuter && !Export.bExportLoadFailed)
					{
						UClass* FuncClass = Cast<UClass>(ObjOuter);
						if (FuncClass && FuncClass->ClassGeneratedBy && !FuncClass->ClassGeneratedBy->HasAnyFlags(RF_BeingRegenerated))
						{
							// If this is a function (NOT being regenerated) whose parent has been removed, give it a NULL parent, as we would have in the script compiler.
							UE_LOG(LogLinker, Display, TEXT("CreateExport: Failed to load Parent for %s; removing parent information, but keeping function"), *GetExportFullName(Index));
						}
					}

					Export.SuperIndex = FPackageIndex();
				}
				else
				{
					if (!FLinkerLoad::IsKnownMissingPackage(*GetExportFullName(Index)))
					{
						UE_LOG(LogLinker, Warning, TEXT("CreateExport: Failed to load Parent for %s"), *GetExportFullName(Index));
					}
					return NULL;
				}
			}
			else
			{
				// SuperStruct needs to be fully linked so that UStruct::Link will have access to UObject::SuperStruct->PropertySize. 
				// There are other attempts to force our super struct to load, and I have not verified that they can all be removed
				// in favor of this one:
				if (!SuperStruct->HasAnyFlags(RF_LoadCompleted | RF_Dynamic)
					&& !SuperStruct->IsNative()
					&& SuperStruct->GetLinker()
					&& Export.SuperIndex.IsImport())
				{
					const UClass* AsClass = dynamic_cast<UClass*>(SuperStruct);
					if (AsClass && !AsClass->ClassDefaultObject)
					{
						check(!GEventDrivenLoaderEnabled || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);
						SuperStruct->SetFlags(RF_NeedLoad);
						Preload(SuperStruct);
					}
				}
			}
		}

		// Only UClass objects and UProperty objects of intrinsic classes can have Native flag set. Those property objects are never
		// serialized so we only have to worry about classes. If we encounter an object that is not a class and has Native flag set
		// we warn about it and remove the flag.
		if( (Export.ObjectFlags & RF_MarkAsNative) != 0 && !LoadClass->IsChildOf(UField::StaticClass()) )
		{
			UE_LOG(LogLinker, Warning,TEXT("%s %s has RF_MarkAsNative set but is not a UField derived class"),*LoadClass->GetName(),*Export.ObjectName.ToString());
			// Remove RF_MarkAsNative;
			Export.ObjectFlags = EObjectFlags(Export.ObjectFlags & ~RF_MarkAsNative);
		}

		if ( !LoadClass->HasAnyClassFlags(CLASS_Intrinsic) )
		{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			if (LoadClass->HasAnyFlags(RF_NeedLoad))
			{
				Preload(LoadClass);
			}
			else if ((Export.Object == nullptr) && !(Export.ObjectFlags & RF_ClassDefaultObject))
			{
				bool const bExportWasDeferred = DeferExportCreation(Index);
				if (bExportWasDeferred)
				{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
					check(Export.Object != nullptr);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
					return Export.Object;
				}				
			}
			else if (Cast<ULinkerPlaceholderExportObject>(Export.Object))
			{
				return Export.Object;
			}
#else  // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			Preload(LoadClass);
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

			// Check if the Preload() above caused the class to be regenerated (LoadClass will be out of date), and refresh the LoadClass pointer if that is the case
			if( LoadClass->HasAnyClassFlags(CLASS_NewerVersionExists) )
			{
				if( Export.ClassIndex.IsImport() )
				{
					FObjectImport& ClassImport = Imp(Export.ClassIndex);
					ClassImport.XObject = NULL;
				}

				LoadClass = (UClass*)IndexToObject( Export.ClassIndex );
			}

			if ( LoadClass->HasAnyClassFlags(CLASS_Deprecated) && GIsEditor && !IsRunningCommandlet() && !FApp::IsGame() )
			{
				if ( (Export.ObjectFlags&RF_ClassDefaultObject) == 0 )
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("ObjectName"), FText::FromString(GetExportFullName(Index)));
					Arguments.Add(TEXT("ClassName"), FText::FromString(LoadClass->GetPathName()));
					LoadErrors.Warning(FText::Format(LOCTEXT("LoadedDeprecatedClassInstance", "{ObjectName}: class {ClassName} has been deprecated."), Arguments));
				}
			}
		}

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		// we're going to have troubles if we're attempting to create an export 
		// for a placeholder class past this point... placeholder-classes should
		// have generated an export-placeholder in the above 
		// !LoadClass->HasAnyClassFlags(CLASS_Intrinsic) block (with the call to
		// DeferExportCreation)
		check(Cast<ULinkerPlaceholderClass>(LoadClass) == nullptr);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

		// detect cases where a class has been made transient when there are existing instances of this class in content packages,
		// and this isn't the class default object; when this happens, it can cause issues which are difficult to debug since they'll
		// only appear much later after this package has been loaded
		if ( LoadClass->HasAnyClassFlags(CLASS_Transient) && (Export.ObjectFlags&RF_ClassDefaultObject) == 0 && (Export.ObjectFlags&RF_ArchetypeObject) == 0 )
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("PackageName"), FText::FromString(Filename));
			Arguments.Add(TEXT("ObjectName"), FText::FromName(Export.ObjectName));
			Arguments.Add(TEXT("ClassName"), FText::FromString(LoadClass->GetPathName()));
			//@todo - should this actually be an assertion?
			LoadErrors.Warning(FText::Format(LOCTEXT("LoadingTransientInstance", "Attempting to load an instance of a transient class from disk - Package:'{PackageName}'  Object:'{ObjectName}'  Class:'{ClassName}'"), Arguments));
		}

		
		// Find or create the object's Outer.
		UObject* ThisParent = NULL;
		if( !Export.OuterIndex.IsNull() )
		{
			ThisParent = IndexToObject(Export.OuterIndex);
		}
		else if( Export.bForcedExport )
		{
			// Create the forced export in the TopLevel instead of LinkerRoot. Please note that CreatePackage
			// will find and return an existing object if one exists and only create a new one if there doesn't.
			Export.Object = CreatePackage( NULL, *Export.ObjectName.ToString() );
			check(Export.Object);
			FUObjectThreadContext::Get().ForcedExportCount++;
		}
		else
		{
			ThisParent = LinkerRoot;
		}

		// If loading the object's Outer caused the object to be loaded or if it was a forced export package created
		// above, return it.
		if( Export.Object != NULL )
		{
			return Export.Object;
		}

		// If we should have an outer but it doesn't exist because it was filtered out, we should silently be filtered out too
		if (Export.OuterIndex.IsExport() && ThisParent == nullptr && ExportMap[Export.OuterIndex.ToExport()].bWasFiltered)
		{
			Export.bWasFiltered = true;
			return nullptr;
		}

		// If outer was a redirector or an object that doesn't exist (but wasn't filtered) then log a warning
		UObjectRedirector* ParentRedirector = dynamic_cast<UObjectRedirector*>(ThisParent);
		if( ThisParent == NULL || ParentRedirector)
		{
			// mark this export as unloadable (so that other exports that
			// reference this one won't continue to execute the above logic), then return NULL
			Export.bExportLoadFailed = true;

			// otherwise, return NULL and let the calling code determine what to do
			const FString OuterName = Export.OuterIndex.IsNull() ? LinkerRoot->GetFullName() : GetFullImpExpName(Export.OuterIndex);

			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ObjectName"), FText::FromName(Export.ObjectName));
			Arguments.Add(TEXT("OuterName"), FText::FromString(OuterName));

			if (ParentRedirector)
			{
				LoadErrors.Warning(FText::Format(LOCTEXT("CreateExportFailedToLoadOuterIsRedirector", "CreateExport: Failed to load Outer for resource because it is a redirector '{ObjectName}': {OuterName}"), Arguments));
			}
			else
			{
				LoadErrors.Warning(FText::Format(LOCTEXT("CreateExportFailedToLoadOuter", "CreateExport: Failed to load Outer for resource '{ObjectName}': {OuterName}"), Arguments));
			}

			return nullptr;
		}

		// Find the Archetype object for the one we are loading.
		UObject* Template = UObject::GetArchetypeFromRequiredInfo(LoadClass, ThisParent, Export.ObjectName, Export.ObjectFlags);

		checkf(Template, TEXT("Failed to get template for class %s. ExportName=%s"), *LoadClass->GetPathName(), *Export.ObjectName.ToString());
		checkfSlow(((Export.ObjectFlags&RF_ClassDefaultObject)!=0 || Template->IsA(LoadClass)), TEXT("Mismatch between template %s and load class %s.  If this is a legacy blueprint or map, it may need to be resaved with bRecompileOnLoad turned off."), *Template->GetPathName(), *LoadClass->GetPathName());
		
		// we also need to ensure that the template has set up any instances
		Template->ConditionalPostLoadSubobjects();

		// Try to find existing object first in case we're a forced export to be able to reconcile. Also do it for the
		// case of async loading as we cannot in-place replace objects.

		UObject* ActualObjectWithTheName = StaticFindObjectFastInternal(nullptr, ThisParent, Export.ObjectName, true);
		
		// Find object after making sure it isn't already set. This would be bad as the code below NULLs it in a certain
		// case, which if it had been set would cause a linker detach mismatch.
		check(Export.Object == nullptr);
		if (ActualObjectWithTheName && (ActualObjectWithTheName->GetClass() == LoadClass))
		{
			Export.Object = ActualObjectWithTheName;
		}

		// Object is found in memory.
		if (Export.Object)
		{
			// Mark that we need to dissociate forced exports later on if we are a forced export.
			if (Export.bForcedExport)
			{
				FUObjectThreadContext::Get().ForcedExportCount++;
			}
			// Associate linker with object to avoid detachment mismatches.
			else
			{
				Export.Object->SetLinker(this, Index);

				// If this object was allocated but never loaded (components created by a constructor) make sure it gets loaded
				// Don't do this for any packages that have previously fully loaded as they may have in memory changes
				FUObjectThreadContext::Get().ObjLoaded.AddUnique(Export.Object);
				if (!Export.Object->HasAnyFlags(RF_LoadCompleted) && !LinkerRoot->IsFullyLoaded())
				{
					check(!GEventDrivenLoaderEnabled || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);

					if (Export.Object->HasAnyFlags(RF_ClassDefaultObject))
					{
						// Class default objects cannot have PostLoadSubobjects called on them
						Export.Object->SetFlags(RF_NeedLoad | RF_NeedPostLoad | RF_WasLoaded);
					}
					else
					{
						Export.Object->SetFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WasLoaded);
					}
				}
			}
			return Export.Object;
		}

		// In cases when an object has been consolidated but its package hasn't been saved, look for UObjectRedirector before
		// constructing the object and loading it again from disk (the redirector hasn't been saved yet so it's not part of the package)
#if WITH_EDITOR
		if ( GIsEditor && GIsRunning && !Export.Object )
		{
			UObjectRedirector* Redirector = (UObjectRedirector*)StaticFindObject(UObjectRedirector::StaticClass(), ThisParent, *Export.ObjectName.ToString(), 1);
			if (Redirector && Redirector->DestinationObject && Redirector->DestinationObject->IsA(LoadClass))
			{
				// A redirector has been found, replace this export with it.
				LoadClass = UObjectRedirector::StaticClass();
				// Create new import for UObjectRedirector class
				FObjectImport* RedirectorImport = new( ImportMap )FObjectImport( UObjectRedirector::StaticClass() );
				FLinkerManager::Get().AddLoaderWithNewImports(this);
				FUObjectThreadContext::Get().ImportCount++;
				Export.ClassIndex = FPackageIndex::FromImport(ImportMap.Num() - 1);
				Export.Object = Redirector;
				Export.Object->SetLinker( this, Index );
				// Return the redirector. It will be handled properly by the calling code
				return Export.Object;
			}
		}
#endif // WITH_EDITOR

		if (ActualObjectWithTheName && !ActualObjectWithTheName->GetClass()->IsChildOf(LoadClass))
		{
			UE_LOG(LogLinker, Error, TEXT("Failed import: class '%s' name '%s' outer '%s'. There is another object (of '%s' class) at the path."),
				*LoadClass->GetName(), *Export.ObjectName.ToString(), *ThisParent->GetName(), *ActualObjectWithTheName->GetClass()->GetName());
			return NULL;
		}

		// Create the export object, marking it with the appropriate flags to
		// indicate that the object's data still needs to be loaded.
		EObjectFlags ObjectLoadFlags = Export.ObjectFlags;
		// if we are loading objects just to verify an object reference during script compilation,
		if (!GVerifyObjectReferencesOnly
		||	(ObjectLoadFlags&RF_ClassDefaultObject) != 0					// only load this object if it's a class default object
		||	LinkerRoot->HasAnyPackageFlags(PKG_ContainsScript)		// or we're loading an existing package and it's a script package
		||	ThisParent->IsTemplate(RF_ClassDefaultObject)			// or if its a subobject template in a CDO
		||	LoadClass->IsChildOf(UField::StaticClass())				// or if it is a UField
		||	LoadClass->IsChildOf(UObjectRedirector::StaticClass()))	// or if its a redirector to another object
		{
			ObjectLoadFlags = EObjectFlags(ObjectLoadFlags |RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_WasLoaded);
		}

		FName NewName = Export.ObjectName;


		// If we are about to create a CDO, we need to ensure that all parent sub-objects are loaded
		// to get default value initialization to work.
		if ((ObjectLoadFlags & RF_ClassDefaultObject) != 0)
		{
			UClass* SuperClass = LoadClass->GetSuperClass();
			if (SuperClass && !SuperClass->IsNative())
			{
				UObject* SuperCDO = SuperClass->GetDefaultObject();
				TArray<UObject*> SuperSubObjects;
				GetObjectsWithOuter(SuperCDO, SuperSubObjects, /*bIncludeNestedObjects=*/ false, /*ExclusionFlags=*/ RF_NoFlags, /*InternalExclusionFlags=*/ EInternalObjectFlags::Native);

				for (UObject* SubObject : SuperSubObjects)
				{
					// Matching behavior in UBlueprint::ForceLoad to ensure that the subobject is actually loaded:
					if (SubObject->HasAnyFlags(RF_NeedLoad) || !SubObject->HasAnyFlags(RF_LoadCompleted))
					{
						SubObject->SetFlags(RF_NeedLoad);
						Preload(SubObject);
					}
				}

				// Preload may have already created this object.
				if (Export.Object)
				{
					return Export.Object;
				}
			}
		}

		LoadClass->GetDefaultObject();

		Export.Object = StaticConstructObject_Internal
		(
			LoadClass,
			ThisParent,
			NewName,
			ObjectLoadFlags,
			EInternalObjectFlags::None,
			Template
		);
		if (FPlatformProperties::RequiresCookedData())
		{
			if (GIsInitialLoad || GUObjectArray.IsOpenForDisregardForGC())
		{
			Export.Object->AddToRoot();
		}
		}
		
		LoadClass = Export.Object->GetClass(); // this may have changed if we are overwriting a CDO component

		if (NewName != Export.ObjectName)
		{
			// create a UObjectRedirector with the same name as the old object we are redirecting
			UObjectRedirector* Redir = NewObject<UObjectRedirector>(Export.Object->GetOuter(), Export.ObjectName, RF_Standalone | RF_Public);
			// point the redirector object to this object
			Redir->DestinationObject = Export.Object;
		}
		
		if( Export.Object )
		{
			bool const bIsBlueprintCDO = ((Export.ObjectFlags & RF_ClassDefaultObject) != 0) && LoadClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			const bool bDeferCDOSerialization = bIsBlueprintCDO && ((LoadFlags & LOAD_DeferDependencyLoads) != 0);
			if (bDeferCDOSerialization)			
			{
				// if LOAD_DeferDependencyLoads is set, then we're already
				// serializing the blueprint's class somewhere up the chain... 
				// we don't want the class regenerated while it in the middle of
				// serializing
				DeferredCDOIndex = Index;
				return Export.Object;
			}
			else 
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
			// Check to see if LoadClass is a blueprint, which potentially needs 
			// to be refreshed and regenerated.  If so, regenerate and patch it 
			// back into the export table
			if( !LoadClass->bCooked && bIsBlueprintCDO && (LoadClass->GetOutermost() != GetTransientPackage()) )
			{			
				{
					// For classes that are about to be regenerated, make sure we register them with the linker, so future references to this linker index will be valid
					const EObjectFlags OldFlags = Export.Object->GetFlags();
					Export.Object->ClearFlags(RF_NeedLoad|RF_NeedPostLoad);
					Export.Object->SetLinker( this, Index, false );
					Export.Object->SetFlags(OldFlags);
				}

				if ( RegenerateBlueprintClass(LoadClass, Export.Object) )
				{
					return Export.Object;
				}
			}
			else
			{
				// we created the object, but the data stored on disk for this object has not yet been loaded,
				// so add the object to the list of objects that need to be loaded, which will be processed
				// in EndLoad()
				Export.Object->SetLinker( this, Index );
				FUObjectThreadContext::Get().ObjLoaded.Add(Export.Object);
			}
		}
		else
		{
			UE_LOG(LogLinker, Warning, TEXT("FLinker::CreatedExport failed to construct object %s %s"), *LoadClass->GetName(), *Export.ObjectName.ToString() );
		}

		if ( Export.Object != NULL )
		{
			// If it's a struct or class, set its parent.
			if( UStruct* Struct = dynamic_cast<UStruct*>(Export.Object) )
			{
				if ( !Export.SuperIndex.IsNull() )
				{
					UStruct* SuperStruct = (UStruct*)IndexToObject(Export.SuperIndex);
					if (ULinkerPlaceholderFunction* Function = Cast<ULinkerPlaceholderFunction>(SuperStruct))
					{
						Function->AddDerivedFunction(Struct);
					}
					else
					{
						Struct->SetSuperStruct((UStruct*)IndexToObject(Export.SuperIndex));
					}
				}

				// If it's a class, bind it to C++.
				if( UClass* ClassObject = dynamic_cast<UClass*>(Export.Object) )
				{
#if WITH_EDITOR
					// Before we serialize the class, begin a scoped class 
					// dependency gather to create a list of other classes that 
					// may need to be recompiled
					//
					// Even with "deferred dependency loading" turned on, we 
					// still need this... one class/blueprint will always be 
					// fully regenerated before another (there is no changing 
					// that); so dependencies need to be recompiled later (with
					// all the regenerated classes in place)
					FScopedClassDependencyGather DependencyHelper(ClassObject);
#endif //WITH_EDITOR

					ClassObject->Bind();

					// Preload classes on first access.  Note that this may update the Export.Object, so ClassObject is not guaranteed to be valid after this point
					// If we're async loading on a cooked build we can skip this as there's no chance we will need to recompile the class. 
					// Preload will be called during async package tick when the data has been precached
					if( !FPlatformProperties::RequiresCookedData() )
					{
						Preload( Export.Object );
					}
				}
			}
	
			// Mark that we need to dissociate forced exports later on.
			if( Export.bForcedExport )
			{
				FUObjectThreadContext::Get().ForcedExportCount++;
			}
		}
	}
	return Export.bExportLoadFailed ? nullptr : Export.Object;
}

bool FLinkerLoad::IsImportNative(const int32 Index) const
{
	const FObjectImport& Import = ImportMap[Index];

	bool bIsImportNative = false;
	// if this import has a linker, then it belongs to some (non-native) asset package 
	if (Import.SourceLinker == nullptr)
	{
		if (!Import.OuterIndex.IsNull())
		{
			// need to check the package that this import belongs to, so recurse
			// up then import's outer chain
			bIsImportNative = IsImportNative(Import.OuterIndex.ToImport());
		}
		else if (UPackage* ExistingPackage = FindObject<UPackage>(/*Outer =*/nullptr, *Import.ObjectName.ToString()))
		{
			// @TODO: what if the package's outer isn't null... what does that mean?
			bIsImportNative = !ExistingPackage->GetOuter() && ExistingPackage->HasAnyPackageFlags(PKG_CompiledIn);
		}
	}

	return bIsImportNative;
}

// Return the loaded object corresponding to an import index; any errors are fatal.
UObject* FLinkerLoad::CreateImport( int32 Index )
{
	check(!GEventDrivenLoaderEnabled || !bLockoutLegacyOperations || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);

	FScopedCreateImportCounter ScopedCounter( this, Index );
	FObjectImport& Import = ImportMap[ Index ];
	
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	// if this Import could possibly introduce a circular load (and we're 
	// actively trying to avoid that at this point in the load process), then 
	// this will stub in the Import with a placeholder object, to be replace 
	// later on (this will return true if the import was actually deferred)
	DeferPotentialCircularImport(Index); 
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	// Imports can have no name if they were filtered out due to package redirects, skip in that case
	if (Import.XObject == nullptr && Import.ObjectName != NAME_None)
	{
		if (!GIsEditor && !IsRunningCommandlet())
		{
			// Try to find existing version in memory first.
			if( UPackage* ClassPackage = FindObjectFast<UPackage>( NULL, Import.ClassPackage, false, false ) )
			{
				if( UClass*	FindClass = FindObjectFast<UClass>( ClassPackage, Import.ClassName, false, false ) ) // 
				{
					// Make sure the class has been loaded and linked before creating a CDO.
					// This is an edge case, but can happen if a blueprint package has not finished creating exports for a class
					// during async loading, and another package creates the class via CreateImport while in cooked builds because
					// we don't call preload immediately after creating a class in CreateExport like in non-cooked builds.
					Preload( FindClass );

					FindClass->GetDefaultObject(); // build the CDO if it isn't already built
					UObject*	FindObject		= NULL;
	
					// Import is a toplevel package.
					if( Import.OuterIndex.IsNull() )
					{
						FindObject = CreatePackage(NULL, *Import.ObjectName.ToString());
					}
					// Import is regular import/ export.
					else
					{
						// Find the imports' outer.
						UObject* FindOuter = NULL;
						// Import.
						if( Import.OuterIndex.IsImport() )
						{
							FObjectImport& OuterImport = Imp(Import.OuterIndex);
							// Outer already in memory.
							if( OuterImport.XObject )
							{
								FindOuter = OuterImport.XObject;
							}
							// Outer is toplevel package, create/ find it.
							else if( OuterImport.OuterIndex.IsNull() )
							{
								FindOuter = CreatePackage( NULL, *OuterImport.ObjectName.ToString() );
							}
							// Outer is regular import/ export, use IndexToObject to potentially recursively load/ find it.
							else
							{
								FindOuter = IndexToObject( Import.OuterIndex );
							}
						}
						// Export.
						else 
						{
							// Create/ find the object's outer.
							FindOuter = IndexToObject( Import.OuterIndex );
						}
						if (!FindOuter)
						{
							// This can happen when deleting native properties or restructing blueprints. If there is an actual problem it will be caught when trying to resolve the outer itself
							FString OuterName = Import.OuterIndex.IsNull() ? LinkerRoot->GetFullName() : GetFullImpExpName(Import.OuterIndex);
							UE_LOG(LogLinker, Verbose, TEXT("CreateImport: Failed to load Outer for resource '%s': %s"), *Import.ObjectName.ToString(), *OuterName);
							return NULL;
						}
	
						// Find object now that we know it's class, outer and name.
						FindObject = FindImportFast(FindClass, FindOuter, Import.ObjectName);
						if (UDynamicClass* FoundDynamicClass = Cast<UDynamicClass>(FindObject))
						{
							if(0 == (FoundDynamicClass->ClassFlags & CLASS_Constructed))
							{
								// This class wasn't fully constructed yet. It will be properly constructed in CreateExport. 
								FindObject = nullptr;
							}
						}
					}

					if( FindObject )
					{		
						// Associate import and indicate that we associated an import for later cleanup.
						Import.XObject = FindObject;
						FUObjectThreadContext::Get().ImportCount++;
						FLinkerManager::Get().AddLoaderWithNewImports(this);
					}
				}
			}
		}

		if( Import.XObject == NULL )
		{
			EVerifyResult VerifyImportResult = VERIFY_Success;
			if( Import.SourceLinker == NULL )
			{
				VerifyImportResult = VerifyImport(Index);
			}
			if(Import.SourceIndex != INDEX_NONE)
			{
				check(Import.SourceLinker);
				// VerifyImport may have already created the import and SourceIndex has changed to point to the actual redirected object.
				// This can only happen in non-cooked builds since cooked builds don't have redirects and other cases are valid.
				// We also don't want to call CreateExport only when there was an actual redirector involved.
				if (FPlatformProperties::RequiresCookedData() || !Import.XObject || VerifyImportResult != VERIFY_Redirected)
				{
					Import.XObject = Import.SourceLinker->CreateExport(Import.SourceIndex);
				}
				// If an object has been replaced (consolidated) in the editor and its package hasn't been saved yet
				// it's possible to get UbjectRedirector here as the original export is dynamically replaced
				// with the redirector (the original object has been deleted but the data on disk hasn't been updated)
#if WITH_EDITOR
				if( GIsEditor )
				{
					UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(Import.XObject);
					if( Redirector )
					{
						Import.XObject = Redirector->DestinationObject;
					}
				}
#endif
				FUObjectThreadContext::Get().ImportCount++;
				FLinkerManager::Get().AddLoaderWithNewImports(this);
			}
		}

		if (Import.XObject == nullptr)
		{
			const FString OuterName = Import.OuterIndex.IsNull() ? LinkerRoot->GetFullName() : GetFullImpExpName(Import.OuterIndex);
			UE_LOG(LogLinker, Verbose, TEXT("Failed to resolve import named %s in %s"), *Import.ObjectName.ToString(), *OuterName);
		}
	}
	return Import.XObject;
}



// Map an import/export index to an object; all errors here are fatal.
UObject* FLinkerLoad::IndexToObject( FPackageIndex Index )
{
	if( Index.IsExport() )
	{
		#if PLATFORM_DESKTOP
			// Show a message box indicating, possible, corrupt data (desktop platforms only)
			if ( !ExportMap.IsValidIndex( Index.ToExport() ) )
			{
				FText ErrorMessage, ErrorCaption;
				GConfig->GetText(TEXT("/Script/Engine.Engine"),
									TEXT("SerializationOutOfBoundsErrorMessage"),
									ErrorMessage,
									GEngineIni);
				GConfig->GetText(TEXT("/Script/Engine.Engine"),
					TEXT("SerializationOutOfBoundsErrorMessageCaption"),
					ErrorCaption,
					GEngineIni);

				UE_LOG( LogLinker, Error, TEXT("Invalid export object index=%d while reading %s. File is most likely corrupted. Please verify your installation."), Index.ToExport(), *Filename );

				if (GLog)
				{
					GLog->Flush();
				}

				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage.ToString(), *ErrorCaption.ToString());

				check(false);
			}
		#else
			{
				UE_CLOG( !ExportMap.IsValidIndex( Index.ToExport() ), LogLinker, Fatal, TEXT("Invalid export object index=%d while reading %s. File is most likely corrupted. Please verify your installation."), Index.ToExport(), *Filename );
			}
		#endif

		return CreateExport( Index.ToExport() );
	}
	else if( Index.IsImport() )
	{
		#if PLATFORM_DESKTOP
			// Show a message box indicating, possible, corrupt data (desktop platforms only)
			if ( !ImportMap.IsValidIndex( Index.ToImport() ) )
			{
				FText ErrorMessage, ErrorCaption;
				GConfig->GetText(TEXT("/Script/Engine.Engine"),
									TEXT("SerializationOutOfBoundsErrorMessage"),
									ErrorMessage,
									GEngineIni);
				GConfig->GetText(TEXT("/Script/Engine.Engine"),
					TEXT("SerializationOutOfBoundsErrorMessageCaption"),
					ErrorCaption,
					GEngineIni);

				UE_LOG( LogLinker, Error, TEXT("Invalid import object index=%d while reading %s. File is most likely corrupted. Please verify your installation."), Index.ToImport(), *Filename );

				if (GLog)
				{
					GLog->Flush();
				}

				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage.ToString(), *ErrorCaption.ToString());

				check(false);
			}
		#else
			{
				UE_CLOG( !ImportMap.IsValidIndex( Index.ToImport() ), LogLinker, Fatal, TEXT("Invalid import object index=%d while reading %s. File is most likely corrupted. Please verify your installation."), Index.ToImport(), *Filename );
			}
		#endif

		return CreateImport( Index.ToImport() );
	}
	else 
	{
		return nullptr;
	}
}



// Detach an export from this linker.
void FLinkerLoad::DetachExport( int32 i )
{
	FObjectExport& E = ExportMap[ i ];
	check(E.Object);
	if( !E.Object->IsValidLowLevel() )
	{
		UE_LOG(LogLinker, Fatal, TEXT("Linker object %s %s.%s is invalid"), *GetExportClassName(i).ToString(), *LinkerRoot->GetName(), *E.ObjectName.ToString() );
	}
	{
		const FLinkerLoad* ActualLinker = E.Object->GetLinker();
		// TODO: verify the condition
		const bool DynamicType = !ActualLinker
			&& (E.Object->HasAnyFlags(RF_Dynamic)
			|| (E.Object->GetClass()->HasAnyFlags(RF_Dynamic) && E.Object->HasAnyFlags(RF_ClassDefaultObject) ));
		if ((ActualLinker != this) && !DynamicType)
		{
			UObject* Object = E.Object;
			UE_LOG(LogLinker, Log, TEXT("Object            : %s"), *Object->GetFullName());
			//UE_LOG(LogLinker, Log, TEXT("Object Linker     : %s"), *Object->GetLinker()->GetFullName() );
			UE_LOG(LogLinker, Log, TEXT("Linker LinkerRoot : %s"), Object->GetLinker() ? *Object->GetLinker()->LinkerRoot->GetFullName() : TEXT("None"));
			//UE_LOG(LogLinker, Log, TEXT("Detach Linker     : %s"), *GetFullName() );
			UE_LOG(LogLinker, Log, TEXT("Detach LinkerRoot : %s"), *LinkerRoot->GetFullName());
			UE_LOG(LogLinker, Fatal, TEXT("Linker object %s %s.%s mislinked!"), *GetExportClassName(i).ToString(), *LinkerRoot->GetName(), *E.ObjectName.ToString());
		}
	}

	if (E.Object->GetLinkerIndex() == -1)
	{
		UE_LOG(LogLinker, Warning, TEXT("Linker object %s %s.%s was already detached."), *GetExportClassName(i).ToString(), *LinkerRoot->GetName(), *E.ObjectName.ToString());
	}
	else
	{
		checkf(E.Object->GetLinkerIndex() == i, TEXT("Mismatched linker index in FLinkerLoad::DetachExport for %s in %s. Linker index was supposed to be %d, was %d"), *GetExportClassName(i).ToString(), *LinkerRoot->GetName(), i, E.Object->GetLinkerIndex());
	}
	ExportMap[i].Object->SetLinker( NULL, INDEX_NONE );
}

void FLinkerLoad::LoadAndDetachAllBulkData()
{
#if WITH_EDITOR
	// Detach all lazy loaders.
	const bool bEnsureAllBulkDataIsLoaded = true;
	DetachAllBulkData(bEnsureAllBulkDataIsLoaded);
#endif
}

void FLinkerLoad::Detach()
{
#if WITH_EDITOR
	// Detach all lazy loaders.
	const bool bEnsureAllBulkDataIsLoaded = false;
	DetachAllBulkData(bEnsureAllBulkDataIsLoaded);
#endif

	// Detach all objects linked with this linker.
	for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
	{	
		if (ExportMap[ExportIndex].Object)
		{
			DetachExport(ExportIndex);
		}
	}

	// Remove from object manager, if it has been added.
	FLinkerManager::Get().RemoveLoader(this);
	FLinkerManager::Get().RemoveLoaderWithNewImports(this);
	if (!FPlatformProperties::HasEditorOnlyData())
	{
		FUObjectThreadContext::Get().DelayedLinkerClosePackages.Remove(this);
	}

	if (Loader)
	{
		delete Loader;
		Loader = nullptr;
	}	

	// Empty out no longer used arrays.
	NameMap.Empty();
	GatherableTextDataMap.Empty();
	ImportMap.Empty();
	ExportMap.Empty();

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	ResetDeferredLoadingState();
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	// Make sure we're never associated with LinkerRoot again.
	if (LinkerRoot)
	{
		LinkerRoot->LinkerLoad = nullptr;
		LinkerRoot = nullptr;
	}
	if (AsyncRoot)
	{
		AsyncRoot->DetachLinker();
		AsyncRoot = nullptr;
	}
}

#if WITH_EDITOR
/**
 * Attaches/ associates the passed in bulk data object with the linker.
 *
 * @param	Owner		UObject owning the bulk data
 * @param	BulkData	Bulk data object to associate
 */
void FLinkerLoad::AttachBulkData( UObject* Owner, FUntypedBulkData* BulkData )
{
	check( BulkDataLoaders.Find(BulkData)==INDEX_NONE );
	BulkDataLoaders.Add( BulkData );
}

/**
 * Detaches the passed in bulk data object from the linker.
 *
 * @param	BulkData	Bulk data object to detach
 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
 */
void FLinkerLoad::DetachBulkData( FUntypedBulkData* BulkData, bool bEnsureBulkDataIsLoaded )
{
	int32 RemovedCount = BulkDataLoaders.Remove( BulkData );
	if( RemovedCount!=1 )
	{	
		UE_LOG(LogLinker, Fatal, TEXT("Detachment inconsistency: %i (%s)"), RemovedCount, *Filename );
	}
	BulkData->DetachFromArchive( this, bEnsureBulkDataIsLoaded );
}

/**
 * Detaches all attached bulk  data objects.
 *
 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
 */
void FLinkerLoad::DetachAllBulkData(bool bEnsureAllBulkDataIsLoaded)
{
	auto BulkDataToDetach = BulkDataLoaders;
	for (auto BulkData : BulkDataToDetach)
	{
		check( BulkData );
		BulkData->DetachFromArchive(this, bEnsureAllBulkDataIsLoaded);
	}
	BulkDataLoaders.Empty();
}

#endif // WITH_EDITOR

FArchive& FLinkerLoad::operator<<( UObject*& Object )
{
	FPackageIndex Index;
	FArchive& Ar = *this;
	Ar << Index;

	if (GEventDrivenLoaderEnabled && bForceSimpleIndexToObject)
	{
		check(Ar.IsLoading() && AsyncRoot);
		Object = AsyncRoot->EventDrivenIndexToObject(Index, false);
		return *this;
	}

	UObject* Temporary = NULL;
	Temporary = IndexToObject( Index );

#if WITH_EDITORONLY_DATA	
	// When loading mark all packages that are accessed by non editor-only properties as being required at runtime.
	if (Ar.IsLoading() && Temporary && !Ar.IsEditorOnlyPropertyOnTheStack())
	{
		const bool bReferenceFromOutsideOfThePackage = Temporary->GetOutermost() != LinkerRoot;
		const bool bIsAClass = Temporary->IsA(UClass::StaticClass());
		const bool bReferencingPackageIsNotEditorOnly = bReferenceFromOutsideOfThePackage && !LinkerRoot->IsLoadedByEditorPropertiesOnly();
		if (bReferencingPackageIsNotEditorOnly || bIsAClass)
		{
			// The package that caused this object to be loaded is not marked as editor-only, neighter is any of the referencing properties.
			Temporary->GetOutermost()->SetLoadedByEditorPropertiesOnly(false);
		}
		else if (bReferenceFromOutsideOfThePackage && !bIsAClass)
		{
			// In this case the object is being accessed by object property from a package that's marked as editor-only, however
			// since we're in the middle of loading, we can't be sure that the editor-only package will still be marked as editor-only
			// after loading has finished (this is due to the fact how objects are being processed in EndLoad).
			// So we need to remember which packages have been kept marked as editor-only by which package so that after all
			// objects have been serialized we can go back and make sure the LinkerRoot package is still marked as editor-only and if not,
			// remove the flag from all packages that are marked as such because of it.
			FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
			TSet<FName>& PackagesMarkedEditorOnly = ThreadContext.PackagesMarkedEditorOnlyByOtherPackage.FindOrAdd(LinkerRoot->GetFName());
			if (!PackagesMarkedEditorOnly.Contains(Temporary->GetOutermost()->GetFName()))
			{
				PackagesMarkedEditorOnly.Add(Temporary->GetOutermost()->GetFName());
			}
		}
	}
#endif

	Object = Temporary;
	return *this;
}

void FLinkerLoad::BadNameIndexError(NAME_INDEX NameIndex)
{
	UE_LOG(LogLinker, Error, TEXT("Bad name index %i/%i"), NameIndex, NameMap.Num());
}

/**
 * Called when an object begins serializing property data using script serialization.
 */
void FLinkerLoad::MarkScriptSerializationStart( const UObject* Obj )
{
	if (Obj && Obj->GetLinker() == this)
	{
		int32 Index = Obj->GetLinkerIndex();
		if (ExportMap.IsValidIndex(Index))
		{
			FObjectExport& Export = ExportMap[Index];
			Export.ScriptSerializationStartOffset = Tell();
		}
	}
}

/**
 * Called when an object stops serializing property data using script serialization.
 */
void FLinkerLoad::MarkScriptSerializationEnd( const UObject* Obj )
{
	if (Obj && Obj->GetLinker() == this)
	{
		int32 Index = Obj->GetLinkerIndex();
		if (ExportMap.IsValidIndex(Index))
		{
			FObjectExport& Export = ExportMap[Index];
			Export.ScriptSerializationEndOffset = Tell();
		}
	}
}

bool FLinkerLoad::FindImportPackage(FName PackageName, FPackageIndex& PackageIdx)
{
	for (int32 ImportMapIdx = 0; ImportMapIdx < ImportMap.Num(); ImportMapIdx++)
	{
		if (ImportMap[ImportMapIdx].ObjectName == PackageName && ImportMap[ImportMapIdx].ClassName == NAME_Package)
		{
			PackageIdx = FPackageIndex::FromImport(ImportMapIdx);
			return true;
		}
	}

	return false;
}

/**
 * Locates the class adjusted index and its package adjusted index for a given class name in the import map
 */
bool FLinkerLoad::FindImportClassAndPackage( FName ClassName, FPackageIndex &ClassIdx, FPackageIndex &PackageIdx )
{
	for ( int32 ImportMapIdx = 0; ImportMapIdx < ImportMap.Num(); ImportMapIdx++ )
	{
		if ( ImportMap[ImportMapIdx].ObjectName == ClassName && ImportMap[ImportMapIdx].ClassName == NAME_Class )
		{
			ClassIdx = FPackageIndex::FromImport(ImportMapIdx);
			PackageIdx = ImportMap[ImportMapIdx].OuterIndex;
			return true;
		}
	}

	return false;
}


UObject* FLinkerLoad::GetArchetypeFromLoader(const UObject* Obj)
{
	if (GEventDrivenLoaderEnabled)
	{
	check(!TemplateForGetArchetypeFromLoader || FUObjectThreadContext::Get().SerializedObject == Obj);
	return TemplateForGetArchetypeFromLoader;
}
	else
	{
		return FArchiveUObject::GetArchetypeFromLoader(Obj);
	}
}


/**
* Attempts to find the index for the given class object in the import list and adds it + its package if it does not exist
*/
bool FLinkerLoad::CreateImportClassAndPackage( FName ClassName, FName PackageName, FPackageIndex &ClassIdx, FPackageIndex &PackageIdx )
{
	//look for an existing import first
	//might as well look for the package at the same time ...
	bool bPackageFound = false;		
	for ( int32 ImportMapIdx = 0; ImportMapIdx < ImportMap.Num(); ImportMapIdx++ )
	{
		//save one iteration by checking for the package in this loop
		if( PackageName != NAME_None && ImportMap[ImportMapIdx].ClassName == NAME_Package && ImportMap[ImportMapIdx].ObjectName == PackageName )
		{
			bPackageFound = true;
			PackageIdx = FPackageIndex::FromImport(ImportMapIdx);
		}
		if ( ImportMap[ImportMapIdx].ObjectName == ClassName && ImportMap[ImportMapIdx].ClassName == NAME_Class )
		{
			ClassIdx = FPackageIndex::FromImport(ImportMapIdx);
			PackageIdx = ImportMap[ImportMapIdx].OuterIndex;
			return true;
		}
	}

	//an existing import couldn't be found, so add it
	//first add the needed package if it didn't already exist in the import map
	if( !bPackageFound )
	{
		int32 Index = ImportMap.AddUninitialized();
		ImportMap[Index].ClassName = NAME_Package;
		ImportMap[Index].ClassPackage = GLongCoreUObjectPackageName;
		ImportMap[Index].ObjectName = PackageName;
		ImportMap[Index].OuterIndex = FPackageIndex();
		ImportMap[Index].XObject = 0;
		ImportMap[Index].SourceLinker = 0;
		ImportMap[Index].SourceIndex = -1;
		PackageIdx = FPackageIndex::FromImport(Index);
	}
	{
		//now add the class import
		int32 Index = ImportMap.AddUninitialized();
		ImportMap[Index].ClassName = NAME_Class;
		ImportMap[Index].ClassPackage = GLongCoreUObjectPackageName;
		ImportMap[Index].ObjectName = ClassName;
		ImportMap[Index].OuterIndex = PackageIdx;
		ImportMap[Index].XObject = 0;
		ImportMap[Index].SourceLinker = 0;
		ImportMap[Index].SourceIndex = -1;
		ClassIdx = FPackageIndex::FromImport(Index);
	}

	return true;
}

TArray<FName> FLinkerLoad::FindPreviousNamesForClass(FString CurrentClassPath, bool bIsInstance)
{
	TArray<FName> OldNames;
	TArray<FCoreRedirectObjectName> OldObjectNames;

	if (FCoreRedirects::FindPreviousNames(ECoreRedirectFlags::Type_Class, FCoreRedirectObjectName(CurrentClassPath), OldObjectNames))
	{
		for (FCoreRedirectObjectName& OldObjectName : OldObjectNames)
		{
			OldNames.AddUnique(OldObjectName.ObjectName);
		}
	}

	if (bIsInstance)
	{
		OldObjectNames.Empty();
		if (FCoreRedirects::FindPreviousNames(ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Option_InstanceOnly, FCoreRedirectObjectName(CurrentClassPath), OldObjectNames))
		{
			for (FCoreRedirectObjectName& OldObjectName : OldObjectNames)
			{
				OldNames.AddUnique(OldObjectName.ObjectName);
			}
		}
	}

	return OldNames;
}

FName FLinkerLoad::FindNewNameForEnum(const FName OldEnumName)
{
	FCoreRedirectObjectName OldName = FCoreRedirectObjectName(OldEnumName, NAME_None, NAME_None);
	FCoreRedirectObjectName NewName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Enum, OldName);

	if (NewName != OldName)
	{
		return NewName.ObjectName;
	}
	return NAME_None;
}

FName FLinkerLoad::FindNewNameForStruct(const FName OldStructName)
{
	FCoreRedirectObjectName OldName = FCoreRedirectObjectName(OldStructName, NAME_None, NAME_None);
	FCoreRedirectObjectName NewName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Struct, OldName);

	if (NewName != OldName)
	{
		return NewName.ObjectName;
	}
	return NAME_None;
}

FName FLinkerLoad::FindNewNameForClass(FName OldClassName, bool bIsInstance)
{
	FCoreRedirectObjectName OldName = FCoreRedirectObjectName(OldClassName, NAME_None, NAME_None);
	FCoreRedirectObjectName NewName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, OldName);

	if (NewName != OldName)
	{
		return NewName.ObjectName;
	}

	if (bIsInstance)
	{
		// Also check instance types
		NewName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Option_InstanceOnly, OldName);

		if (NewName != OldName)
		{
			return NewName.ObjectName;
		}
	}
	return NAME_None;
}

bool FLinkerLoad::IsKnownMissingPackage(FName PackageName)
{
	return FCoreRedirects::IsKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, PackageName));
}

void FLinkerLoad::AddKnownMissingPackage(FName PackageName)
{
	FCoreRedirects::AddKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, PackageName));
}

bool FLinkerLoad::RemoveKnownMissingPackage(FName PackageName)
{
	return FCoreRedirects::RemoveKnownMissing(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, PackageName));
}

void FLinkerLoad::AddGameNameRedirect(const FName OldName, const FName NewName)
{
	TArray<FCoreRedirect> NewRedirects;
	NewRedirects.Emplace(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, OldName), FCoreRedirectObjectName(NAME_None, NAME_None, NewName));
	FCoreRedirects::AddRedirectList(NewRedirects, TEXT("AddGameNameRedirect"));
}

#if WITH_EDITOR

/**
* Checks if exports' indexes and names are equal.
*/
bool AreObjectExportsEqualForDuplicateChecks(const FObjectExport& Lhs, const FObjectExport& Rhs)
{
	return Lhs.ObjectName == Rhs.ObjectName
		&& Lhs.ClassIndex == Rhs.ClassIndex
		&& Lhs.OuterIndex == Rhs.OuterIndex;
}

/**
 * Helper function to sort ExportMap for duplicate checks.
 */
bool ExportMapSorter(const FObjectExport& Lhs, const FObjectExport& Rhs)
{
	// Check names first.
	if (Lhs.ObjectName < Rhs.ObjectName)
	{
		return true;
	}

	if (Lhs.ObjectName > Rhs.ObjectName)
	{
		return false;
	}

	// Names are equal, check classes.
	if (Lhs.ClassIndex < Rhs.ClassIndex)
	{
		return true;
	}

	if (Lhs.ClassIndex > Rhs.ClassIndex)
	{
		return false;
	}

	// Class names are equal as well, check outers.
	return Lhs.OuterIndex < Rhs.OuterIndex;
}

void FLinkerLoad::ReplaceExportIndexes(const FPackageIndex& OldIndex, const FPackageIndex& NewIndex)
{
	for (auto& Export : ExportMap)
	{
		if (Export.ClassIndex == OldIndex)
		{
			Export.ClassIndex = NewIndex;
		}

		if (Export.SuperIndex == OldIndex)
		{
			Export.SuperIndex = NewIndex;
		}

		if (Export.OuterIndex == OldIndex)
		{
			Export.OuterIndex = NewIndex;
		}
	}
}

void FLinkerLoad::FixupDuplicateExports()
{
	// We need to operate on copy to avoid incorrect indexes after sorting
	auto ExportMapSorted = ExportMap;
	ExportMapSorted.Sort(ExportMapSorter);

	// ClassIndex, SuperIndex, OuterIndex
	int32 LastUniqueExportIndex = 0;
	for (int32 SortedIndex = 1; SortedIndex < ExportMapSorted.Num(); ++SortedIndex)
	{
		const FObjectExport& Original = ExportMapSorted[LastUniqueExportIndex];
		const FObjectExport& Duplicate = ExportMapSorted[SortedIndex];

		if (AreObjectExportsEqualForDuplicateChecks(Original, Duplicate))
		{
			// Duplicate entry found. Look through all Exports and update their ClassIndex, SuperIndex and OuterIndex
			// to point on original export instead of duplicate.
			const FPackageIndex& DuplicateIndex = Duplicate.ThisIndex;
			const FPackageIndex& OriginalIndex = Original.ThisIndex;
			ReplaceExportIndexes(DuplicateIndex, OriginalIndex);

			// Mark Duplicate as null, so we don't load it.
			Exp(Duplicate.ThisIndex).ThisIndex = FPackageIndex();
		}
		else
		{
			LastUniqueExportIndex = SortedIndex;
		}
	}
}
#endif // WITH_EDITOR

/**
* Allows object instances to be converted to other classes upon loading a package
*/
FLinkerLoad::ELinkerStatus FLinkerLoad::FixupExportMap()
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FLinkerLoad::FixupExportMap" ), STAT_LinkerLoad_FixupExportMap, STATGROUP_LinkerLoad );

#if WITH_EDITOR
	if (UE4Ver() < VER_UE4_SKIP_DUPLICATE_EXPORTS_ON_SAVE_PACKAGE && !bExportsDuplicatesFixed)
	{
		FixupDuplicateExports();
		bExportsDuplicatesFixed = true;
	}
#endif // WITH_EDITOR

	// No need to fixup exports if everything is cooked.
	if (!FPlatformProperties::RequiresCookedData())
	{
		if (bFixupExportMapDone)
		{
			return LINKER_Loaded;
		}

		for ( int32 ExportMapIdx = 0; ExportMapIdx < ExportMap.Num(); ExportMapIdx++ )
		{
			FObjectExport &Export = ExportMap[ExportMapIdx];
			if (!IsValidPackageIndex(Export.ClassIndex))
			{
				UE_LOG(LogLinker, Warning, TEXT("Bad class index found on export %d"), ExportMapIdx);
				return LINKER_Failed;
			}
			FName NameClass = GetExportClassName(ExportMapIdx);
			FName NamePackage = GetExportClassPackage(ExportMapIdx);
			FString StrObjectName = Export.ObjectName.ToString();

			// ActorComponents outered to a BlueprintGeneratedClass (or even older ones that are outered to Blueprint) need to be marked RF_Public, but older content was 
			// not created as such.  This updates the ExportTable such that they are correctly flagged when created and when other packages validate their imports.
			if (UE4Ver() < VER_UE4_BLUEPRINT_GENERATED_CLASS_COMPONENT_TEMPLATES_PUBLIC)
			{
				if ((Export.ObjectFlags & RF_Public) == 0)
				{
					static const FName NAME_BlueprintGeneratedClass("BlueprintGeneratedClass");
					static const FName NAME_Blueprint("Blueprint");
					const FName OuterClassName = GetExportClassName(Export.OuterIndex);
					if (OuterClassName == NAME_BlueprintGeneratedClass || OuterClassName == NAME_Blueprint)
					{
						static const UClass* ActorComponentClass = FindObjectChecked<UClass>(ANY_PACKAGE, TEXT("ActorComponent"), true);
						static const FString BPGeneratedClassPostfix(TEXT("_C"));
						const FString NameClassString = NameClass.ToString();
						UClass* Class = FindObject<UClass>(ANY_PACKAGE, *NameClassString);

						// It is (obviously) a component if the class is a child of actor component
						// and (almost certainly) a component if the class cannot be loaded but it ends in _C meaning it was generated from a blueprint
						// However, it (probably) isn't safe to load the blueprint class, so we just check the _C and it is (probably) good enough
						if (    ((Class != nullptr) && Class->IsChildOf(ActorComponentClass))
							 || ((Class == nullptr) && NameClassString.EndsWith(BPGeneratedClassPostfix)))
						{
							Export.ObjectFlags |= RF_Public;
						}
					}
				}
			}

			// Look for subobject redirects and instance redirects
			FCoreRedirectObjectName OldClassName(NameClass, NAME_None, NamePackage);
				
			const TMap<FString, FString>* ValueChanges = FCoreRedirects::GetValueRedirects(ECoreRedirectFlags::Type_Class, OldClassName);

			if (ValueChanges)
			{
				// Apply class value redirects before other redirects, to mirror old subobject order
				const FString* NewInstanceName = ValueChanges->Find(Export.ObjectName.ToString());
				if (NewInstanceName)
				{
					// Rename this import directly
					FString Was = GetExportFullName(ExportMapIdx);
					Export.ObjectName = FName(**NewInstanceName);

					if (Export.ObjectName != NAME_None)
					{
						FString Now = GetExportFullName(ExportMapIdx);
						UE_LOG(LogLinker, Verbose, TEXT("FLinkerLoad::FixupExportMap() - Renamed object from %s   to   %s"), *Was, *Now);
					}
					else
					{
						Export.bExportLoadFailed = true;
						UE_LOG(LogLinker, Verbose, TEXT("FLinkerLoad::FixupExportMap() - Removed object %s"), *Was);
					}
				}
			}

			// Never modify the default object instances
			if (!StrObjectName.StartsWith(DEFAULT_OBJECT_PREFIX))
			{
				FCoreRedirectObjectName NewClassInstanceName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Option_InstanceOnly, OldClassName);

				bool bClassInstanceDeleted = FCoreRedirects::IsKnownMissing(ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Option_InstanceOnly, OldClassName);
				if (bClassInstanceDeleted)
				{
					UE_LOG(LogLinker, Log, TEXT("FLinkerLoad::FixupExportMap() - Pkg<%s> [Obj<%s> Cls<%s> ClsPkg<%s>] -> removed"), *LinkerRoot->GetName(),
						*Export.ObjectName.ToString(), *NameClass.ToString(), *NamePackage.ToString());

					Export.ClassIndex = FPackageIndex();
					Export.OuterIndex = FPackageIndex();
					Export.ObjectName = NAME_None;
#if WITH_EDITOR
					Export.OldClassName = NameClass;
#endif
				}
				else if (NewClassInstanceName != OldClassName)
				{
					FPackageIndex NewClassIndex;
					FPackageIndex NewPackageIndex;

					if (CreateImportClassAndPackage(NewClassInstanceName.ObjectName, NewClassInstanceName.PackageName, NewClassIndex, NewPackageIndex))
					{
						Export.ClassIndex = NewClassIndex;
#if WITH_EDITOR
						Export.OldClassName = NameClass;
#endif
						UE_LOG(LogLinker, Log, TEXT("FLinkerLoad::FixupExportMap() - Pkg<%s> [Obj<%s> Cls<%s> ClsPkg<%s>] -> [Obj<%s> Cls<%s> ClsPkg<%s>]"), *LinkerRoot->GetName(),
							*Export.ObjectName.ToString(), *NameClass.ToString(), *NamePackage.ToString(),
							*Export.ObjectName.ToString(), *NewClassInstanceName.ObjectName.ToString(), *NewClassInstanceName.PackageName.ToString());
					}
					else
					{
						UE_LOG(LogLinker, Log, TEXT("FLinkerLoad::FixupExportMap() - object redirection failed at %s"), *Export.ObjectName.ToString());
					}
				}
			}
		}
		bFixupExportMapDone = true;
		return !IsTimeLimitExceeded( TEXT("fixing up export map") ) ? LINKER_Loaded : LINKER_TimedOut;
	}
	else
	{
		return LINKER_Loaded;
	}
}

void FLinkerLoad::FlushCache()
{
	if (Loader)
	{
		Loader->FlushCache();
	}
}

bool FLinkerLoad::HasAnyObjectsPendingLoad() const
{
	for (const FObjectExport& Export : ExportMap)
	{
		if (Export.Object && Export.Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad))
		{
			return true;
		}
	}
	return false;
}

bool FLinkerLoad::AttachExternalReadDependency(FExternalReadCallback& ReadCallback)
{
	ExternalReadDependencies.Add(ReadCallback);
	return true;
}

bool FLinkerLoad::FinishExternalReadDependencies(double InTimeLimit)
{
	double LocalStartTime = FPlatformTime::Seconds();
	double RemainingTime = InTimeLimit;
	
	while (ExternalReadDependencies.Num())
	{
		FExternalReadCallback& ReadCallback = ExternalReadDependencies.Last();
		
		bool bFinished = ReadCallback(RemainingTime);
		
		checkf(RemainingTime > 0.0 || bFinished, TEXT("FExternalReadCallback must be finished when RemainingTime is zero"));

		if (bFinished)
		{
			ExternalReadDependencies.RemoveAt(ExternalReadDependencies.Num() - 1);
		}

		// Update remaining time
		if (RemainingTime > 0.0)
		{
			RemainingTime-= (FPlatformTime::Seconds() - LocalStartTime);
			if (RemainingTime <= 0.0)
			{
				return false;
			}
		}
	}
	return true;
}

#if WITH_EDITORONLY_DATA
/** Performs a fixup on packages' editor-only flag */
void FixupPackageEditorOnlyFlag(FName PackageThatGotEditorOnlyFlagCleared, bool bRecursive)
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	STAT(double ThisTime = 0);
	{
		SCOPE_SECONDS_COUNTER(ThisTime);

		// Now go through all packages that were marked as editor-only at load time
		// and if they're no longer marked as such, make sure that all packages that
		// were marked as editor-only because of that package, are now also marked as not editor-only.
		TSet<FName>* PackagesMarkedEditorOnlyByThisPackage = ThreadContext.PackagesMarkedEditorOnlyByOtherPackage.Find(PackageThatGotEditorOnlyFlagCleared);
		if (PackagesMarkedEditorOnlyByThisPackage)
		{			
			for (FName& PackageName : *PackagesMarkedEditorOnlyByThisPackage)
			{
				UPackage* EditorOnlyPackage = FindObjectFast<UPackage>(nullptr, PackageName);
				if (EditorOnlyPackage && EditorOnlyPackage->IsLoadedByEditorPropertiesOnly())
				{
					// Now we will recursively unset the flag on all other packages
					EditorOnlyPackage->SetLoadedByEditorPropertiesOnly(false, true);
				}
			}
			ThreadContext.PackagesMarkedEditorOnlyByOtherPackage.Remove(PackageThatGotEditorOnlyFlagCleared);
		}
	}
	if (!bRecursive)
	{
		INC_FLOAT_STAT_BY(STAT_EditorOnlyFixupTime, ThisTime);
	}
}
#endif

#undef LOCTEXT_NAMESPACE
