// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/BufferArchive.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ObjectThumbnail.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/Object.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/PackageName.h"
#include "Serialization/BulkData.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/ObjectResource.h"
#include "UObject/Linker.h"
#include "UObject/LinkerLoad.h"
#include "UObject/LinkerSave.h"
#include "UObject/EditorObjectVersion.h"
#include "Blueprint/BlueprintSupport.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "UObject/Interface.h"
#include "Interfaces/ITargetPlatform.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/GCScopeLock.h"
#include "ProfilingDebugging/CookStats.h"
#include "UObject/DebugSerializationFlags.h"
#include "EnumProperty.h"
#include "BlueprintSupport.h"
#include "IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavePackage, Log, All);

static const int32 MAX_MERGED_COMPRESSION_CHUNKSIZE = 1024 * 1024;
static const FName WorldClassName = FName("World");
static const FName PrestreamPackageClassName = FName("PrestreamPackage");

#define VALIDATE_INITIALIZECORECLASSES 0
#define EXPORT_SORTING_DETAILED_LOGGING 0

// Uncomment this code to measure UObject::Serialize time taken per UClass type during save.
// This code tracks each type in a hash, so is too heavyweight to leave on in general,
// but can be really useful for telling what classes are causing the most save time.
#define ENABLE_PACKAGE_CLASS_SERIALIZATION_TIMES 0
// uncomment this code to measure UObject::PreSave time taken per uclass type during save
#define ENABLE_TAGEXPORTS_CLASS_PRESAVE_TIMES 0

#if ENABLE_COOK_STATS
#include "ProfilingDebugging/ScopedTimers.h"

namespace SavePackageStats
{
	static int32 NumPackagesSaved = 0;
	static double SavePackageTimeSec = 0.0;
	static double TagPackageExportsPresaveTimeSec = 0.0;
	static double TagPackageExportsTimeSec = 0.0;
	static double ResetLoadersForSaveTimeSec = 0.0;
	static double TagPackageExportsGetObjectsWithOuter = 0.0;
	static double TagPackageExportsGetObjectsWithMarks = 0.0;
	static double SerializeImportsTimeSec = 0.0;
	static double SortExportsSeekfreeInnerTimeSec = 0.0;
	static double SerializeExportsTimeSec = 0.0;
	static double SerializeBulkDataTimeSec = 0.0;
	static double AsyncWriteTimeSec = 0.0;
	static double MBWritten = 0.0;
	static TMap<FName, FCookStatsManager::TKeyValuePair<double, uint32>> PackageClassSerializeTimes;
	static TMap<FName, FCookStatsManager::TKeyValuePair<double, uint32>> TagExportSerializeTimes;
	static TMap<FName, FCookStatsManager::TKeyValuePair<double, uint32>> ClassPreSaveTimes;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		// Don't use FCookStatsManager::CreateKeyValueArray because there's just too many arguments. Don't need to overburden the compiler here.
		TArray<FCookStatsManager::StringKeyValue> StatsList;
		StatsList.Empty(15);
		#define ADD_COOK_STAT(Name) StatsList.Emplace(TEXT(#Name), Lex::ToString(Name))
		ADD_COOK_STAT(NumPackagesSaved);
		ADD_COOK_STAT(SavePackageTimeSec);
		ADD_COOK_STAT(TagPackageExportsPresaveTimeSec);
		ADD_COOK_STAT(TagPackageExportsTimeSec);
		ADD_COOK_STAT(ResetLoadersForSaveTimeSec);
		ADD_COOK_STAT(TagPackageExportsGetObjectsWithOuter);
		ADD_COOK_STAT(TagPackageExportsGetObjectsWithMarks);
		ADD_COOK_STAT(SerializeImportsTimeSec);
		ADD_COOK_STAT(SortExportsSeekfreeInnerTimeSec);
		ADD_COOK_STAT(SerializeExportsTimeSec);
		ADD_COOK_STAT(SerializeBulkDataTimeSec);
		ADD_COOK_STAT(AsyncWriteTimeSec);
		ADD_COOK_STAT(MBWritten);
		#undef ADD_COOK_STAT

		AddStat(TEXT("Package.Save"), StatsList);
		
		const FString TotalString = TEXT("Total");

		if (PackageClassSerializeTimes.Num() > 0)
		{
			// Sort the class serialize times in reverse order.
			typedef FCookStatsManager::TKeyValuePair<FName, FCookStatsManager::TKeyValuePair<double, uint32>> ClassSerializeTimeData;
			TArray<ClassSerializeTimeData> SerializeTimesArray;
			SerializeTimesArray.Empty(PackageClassSerializeTimes.Num());
			for (const auto& KV : PackageClassSerializeTimes)
			{
				SerializeTimesArray.Emplace(FCookStatsManager::MakePair(KV.Key, FCookStatsManager::MakePair(KV.Value.Key, KV.Value.Value)));
			}
			SerializeTimesArray.Sort([](const ClassSerializeTimeData& LHS, const ClassSerializeTimeData& RHS)
			{
				return LHS.Value.Key > RHS.Value.Key;
			});

			// always print at least the top n, but not anything < 0.1% of total save time.
			int ClassesLogged = 0;
			for (const auto& KV : SerializeTimesArray)
			{
				// since we're sorted on size already. Just find the first one below the threshold and stop there
				if (ClassesLogged >= 10 && KV.Value.Key < 0.001 * SavePackageTimeSec)
				{
					break;
				}
				const FString ClassName = KV.Key.ToString();
				AddStat(TEXT("Package.Serialize"), FCookStatsManager::CreateKeyValueArray(
					TEXT("Class"), ClassName, 
					TEXT("TimeSec"), KV.Value.Key, 
					TEXT("Calls"), KV.Value.Value));
				ClassesLogged++;
			}
		}

		if (TagExportSerializeTimes.Num() > 0)
		{
			// Sort the class serialize times in reverse order.
			typedef FCookStatsManager::TKeyValuePair<FName, FCookStatsManager::TKeyValuePair<double, uint32>> ClassSerializeTimeData;
			TArray<ClassSerializeTimeData> SerializeTimesArray;
			SerializeTimesArray.Empty(TagExportSerializeTimes.Num());
			for (const auto& KV : TagExportSerializeTimes)
			{
				SerializeTimesArray.Emplace(FCookStatsManager::MakePair(KV.Key, FCookStatsManager::MakePair(KV.Value.Key, KV.Value.Value)));
			}
			SerializeTimesArray.Sort([](const ClassSerializeTimeData& LHS, const ClassSerializeTimeData& RHS)
			{
				return LHS.Value.Key > RHS.Value.Key;
			});

			double TotalSerializeTime = 0.0;
			int TotalSerializeCalls = 0;

			// always print at least the top n, but not anything < 0.1% of total save time.
			int ClassesLogged = 0;
			for (const auto& KV : SerializeTimesArray)
			{
				TotalSerializeTime += KV.Value.Key;
				TotalSerializeCalls += KV.Value.Value;

				// since we're sorted on size already. Just find the first one below the threshold and stop there
				if (ClassesLogged >= 10 && KV.Value.Key < 0.001 * SavePackageTimeSec)
				{
					break;
				}
				const FString ClassName = KV.Key.ToString();
				AddStat(TEXT("Package.TagExportSerialize"), FCookStatsManager::CreateKeyValueArray(
					TEXT("Class"), ClassName,
					TEXT("TimeSec"), KV.Value.Key,
					TEXT("Calls"), KV.Value.Value));
				ClassesLogged++;
			}

			AddStat(TEXT("Package.TagExportSerialize"), FCookStatsManager::CreateKeyValueArray(
				TEXT("Class"), TotalString,
				TEXT("TimeSec"), TotalSerializeTime,
				TEXT("Calls"), TotalSerializeCalls));
		}

		if (ClassPreSaveTimes.Num() > 0)
		{
			// Sort the class serialize times in reverse order.
			typedef FCookStatsManager::TKeyValuePair<FName, FCookStatsManager::TKeyValuePair<double, uint32>> ClassSerializeTimeData;
			TArray<ClassSerializeTimeData> SerializeTimesArray;
			SerializeTimesArray.Empty(ClassPreSaveTimes.Num());
			for (const auto& KV : ClassPreSaveTimes)
			{
				SerializeTimesArray.Emplace(FCookStatsManager::MakePair(KV.Key, FCookStatsManager::MakePair(KV.Value.Key, KV.Value.Value)));
			}
			SerializeTimesArray.Sort([](const ClassSerializeTimeData& LHS, const ClassSerializeTimeData& RHS)
			{
				return LHS.Value.Key > RHS.Value.Key;
			});

			// always print at least the top n, but not anything < 0.1% of total save time.
			// even if we don't print them then add up the time spent inside presave so we can at least account for it
			double TotalPreSaveTime = 0.0;
			int TotalPreSaveCalls = 0;
			int ClassesLogged = 0;
			for (const auto& KV : SerializeTimesArray)
			{
				TotalPreSaveTime += KV.Value.Key;
				TotalPreSaveCalls += KV.Value.Value;

				// since we're sorted on size already. Just find the first one below the threshold and stop there
				if (ClassesLogged >= 10 && KV.Value.Key < 0.001 * SavePackageTimeSec)
				{
					break;
				}
				const FString ClassName = KV.Key.ToString();
				AddStat(TEXT("Package.PreSave"), FCookStatsManager::CreateKeyValueArray(
					TEXT("Class"), ClassName,
					TEXT("TimeSec"), KV.Value.Key,
					TEXT("Calls"), KV.Value.Value));
				ClassesLogged++;
			}
			
			AddStat(TEXT("Package.PreSave"), FCookStatsManager::CreateKeyValueArray(
				TEXT("Class"), TotalString,
				TEXT("TimeSec"), TotalPreSaveTime,
				TEXT("Calls"), TotalPreSaveCalls));
		}
	});
}
#endif

static bool HasUnsaveableOuter(UObject* InObj, UPackage* InSavingPackage)
{
	UObject* Obj = InObj;
	while (Obj)
	{
		if (Obj->GetClass()->HasAnyClassFlags(CLASS_Deprecated) && !Obj->HasAnyFlags(RF_ClassDefaultObject))
		{
			if (!InObj->IsPendingKill() && InObj->GetOutermost() == InSavingPackage)
			{
				UE_LOG(LogSavePackage, Warning, TEXT("%s has a deprecated outer %s, so it will not be saved"), *InObj->GetFullName(), *Obj->GetFullName());
			}
			return true; 
		}

		if(Obj->IsPendingKill())
		{
			return true;
		}

		if (Obj->HasAnyFlags(RF_Transient) && !Obj->IsNative())
		{
			return true;
		}

		Obj = Obj->GetOuter();
	}
	return false;
}

static void CheckObjectPriorToSave(FArchiveUObject& Ar, UObject* InObj, UPackage* InSavingPackage)
{
	if (!InObj)
	{
		return;
	}
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	if (!InObj->IsValidLowLevelFast() || !InObj->IsValidLowLevel())
	{
		UE_LOG(LogLinker, Fatal, TEXT("Attempt to save bogus object %p ThreadContext.SerializedObject=%s  SerializedProperty=%s"), (void*)InObj, *GetFullNameSafe(ThreadContext.SerializedObject), *GetFullNameSafe(Ar.GetSerializedProperty()));
		return;
	}
	// if the object class is abstract or has been marked as deprecated, mark this
	// object as transient so that it isn't serialized
	if ( InObj->GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) )
	{
		if ( !InObj->HasAnyFlags(RF_ClassDefaultObject) || InObj->GetClass()->HasAnyClassFlags(CLASS_Deprecated) )
		{
			InObj->SetFlags(RF_Transient);
		}
		if ( !InObj->HasAnyFlags(RF_ClassDefaultObject) && InObj->GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference) )
		{
			TArray<UObject*> ComponentReferences;
			FReferenceFinder ComponentCollector(ComponentReferences, InObj, false, true, true);
			ComponentCollector.FindReferences(InObj, ThreadContext.SerializedObject, Ar.GetSerializedProperty());

			for ( int32 Index = 0; Index < ComponentReferences.Num(); Index++ )
			{
				ComponentReferences[Index]->SetFlags(RF_Transient);
			}
		}
	}
	else if ( HasUnsaveableOuter(InObj, InSavingPackage) )
	{
		InObj->SetFlags(RF_Transient);
	}

	if ( InObj->HasAnyFlags(RF_ClassDefaultObject)
		&& (InObj->GetClass()->ClassGeneratedBy == nullptr || !InObj->GetClass()->HasAnyFlags(RF_Transient)) )
	{
		// if this is the class default object, make sure it's not
		// marked transient for any reason, as we need it to be saved
		// to disk (unless it's associated with a transient generated class)
		InObj->ClearFlags(RF_Transient);
	}
}

static bool EndSavingIfCancelled( FLinkerSave* Linker, const FString& TempFilename ) 
{
	if ( GWarn->ReceivedUserCancel() )
	{
		// free the file handle and delete the temporary file
		Linker->Detach();
		IFileManager::Get().Delete( *TempFilename );
		return true;
	}
	return false;
}

static FThreadSafeCounter OutstandingAsyncWrites;

void UPackage::WaitForAsyncFileWrites()
{
	while (OutstandingAsyncWrites.GetValue())
	{
		FPlatformProcess::Sleep(0.0f);
	}
}

struct FLargeMemoryDelete
{
	void operator()(uint8* Ptr) const
	{
		if (Ptr)
		{
			FMemory::Free(Ptr);
		}
	}
};

typedef TUniquePtr<uint8, FLargeMemoryDelete> FLargeMemoryPtr;

void AsyncWriteFile(FLargeMemoryPtr Data, const int64 DataSize, const TCHAR* Filename, const FDateTime& TimeStamp, bool bUseTempFilename = true)
{
	class FAsyncWriteWorker : public FNonAbandonableTask
	{
	public:
		/** Filename To write to**/
		FString Filename;
		/** Should we write to a temp file then move it */
		bool bUseTempFilename;
		/** Data for the file. Will be freed after write **/
		FLargeMemoryPtr Data;
		/** Size of data */
		const int64 DataSize;
		/** Timestamp to give the file. MinValue if shouldn't be modified */
		FDateTime FinalTimeStamp;

		/** Constructor
		*/
		FAsyncWriteWorker(const TCHAR* InFilename, FLargeMemoryPtr InData, const int64 InDataSize, const FDateTime& InTimeStamp, bool inbUseTempFilename)
			: Filename(InFilename)
			, bUseTempFilename(inbUseTempFilename)
			, Data(MoveTemp(InData))
			, DataSize(InDataSize)
			, FinalTimeStamp(InTimeStamp)
		{
		}
		
		/** Write the file  */
		void DoWork()
		{
			check(DataSize);
			FString TempFilename; 

			if ( bUseTempFilename )
			{
				TempFilename = FPaths::GetBaseFilename(Filename, false);
				TempFilename += TEXT(".t");
			}
			else
			{
				TempFilename = Filename;
			}
			
			// Open a file writer for saving
			FArchive* Ar = IFileManager::Get().CreateFileWriter(*TempFilename);
			if (Ar)
			{
				Ar->Serialize(Data.Get(), DataSize);
				delete Ar;
				
				// Clean-up the memory as soon as we save the file to reduce the memory footprint.
				Data.Reset();

				if (IFileManager::Get().FileSize(*TempFilename) == DataSize)
				{
					if ( bUseTempFilename )
					{
						if( !IFileManager::Get().Move(*Filename, *TempFilename, true, true, false, false))
						{
							UE_LOG(LogSavePackage, Fatal, TEXT("Could not move to %s."),*Filename);
						}

						// if everything worked, this is not necessary, but we will make every effort to avoid leaving junk in the cache
						if (FPaths::FileExists(TempFilename))
						{
							IFileManager::Get().Delete(*TempFilename);
						}
					}

					if (FinalTimeStamp != FDateTime::MinValue())
					{
						IFileManager::Get().SetTimeStamp(*Filename, FinalTimeStamp);
					}
				}
				else
				{
					UE_LOG(LogSavePackage, Fatal, TEXT("Could not save to %s!"),*TempFilename);
				}
			}
			else
			{
				UE_LOG(LogSavePackage, Fatal, TEXT("Could not write to %s!"),*TempFilename);
			}
			
			OutstandingAsyncWrites.Decrement();
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncWriteWorker, STATGROUP_ThreadPoolAsyncTasks);
		}

	};

	OutstandingAsyncWrites.Increment();
	(new FAutoDeleteAsyncTask<FAsyncWriteWorker>(Filename, MoveTemp(Data), DataSize, TimeStamp, bUseTempFilename))->StartBackgroundTask();
}

void AsyncWriteFileWithSplitExports(FLargeMemoryPtr Data, const int64 DataSize, const int64 HeaderSize, const TCHAR* Filename, const FDateTime& TimeStamp, bool bUseTempFilename = true)
{
	class FAsyncWriteWorkerWithSplitExports : public FNonAbandonableTask
	{
	public:
		/** Filename To write to**/
		FString Filename;
		/** Should we write to a temp file then move it */
		bool bUseTempFilename;
		/** Data for the file. Will be freed after write **/
		FLargeMemoryPtr Data;
		/** Size of data */
		const int64 DataSize;
		/** Size of data */
		const int64 HeaderSize;
		/** Timestamp to give the file. MinValue if shouldn't be modified */
		FDateTime FinalTimeStamp;

		/** Constructor
		*/
		FAsyncWriteWorkerWithSplitExports(const TCHAR* InFilename, FLargeMemoryPtr InData, const int64 InDataSize, const int64 InHeaderSize, const FDateTime& InTimeStamp, bool inbUseTempFilename)
			: Filename(InFilename)
			, bUseTempFilename(inbUseTempFilename)
			, Data(MoveTemp(InData))
			, DataSize(InDataSize)
			, HeaderSize(InHeaderSize)
			, FinalTimeStamp(InTimeStamp)
		{
		}

		/** Write the file  */
		void DoWork()
		{
			check(DataSize);
			FString BaseFilename;
			FString TempFilename;
			FString FilenameExports;
			FString TempFilenameExports;

			BaseFilename = FPaths::GetBaseFilename(Filename, false);
			FilenameExports = BaseFilename;
			FilenameExports += TEXT(".uexp");
			if (bUseTempFilename)
			{
				TempFilename = BaseFilename;
				TempFilename += TEXT(".t");
				TempFilenameExports = BaseFilename;
				TempFilenameExports += TEXT(".e");
			}
			else
			{
				TempFilename = Filename;
				TempFilenameExports = FilenameExports;
			}

			{
				FArchive* Ar = IFileManager::Get().CreateFileWriter(*TempFilename);
				if (Ar)
				{
					Ar->Serialize(Data.Get(), HeaderSize);
					delete Ar;
					Ar = nullptr;

					if (IFileManager::Get().FileSize(*TempFilename) == HeaderSize)
					{
						if (bUseTempFilename)
						{
							if (!IFileManager::Get().Move(*Filename, *TempFilename, true, true, false, false))
							{
								UE_LOG(LogSavePackage, Fatal, TEXT("Could not move to %s."), *Filename);
							}

							// if everything worked, this is not necessary, but we will make every effort to avoid leaving junk in the cache
							if (FPaths::FileExists(TempFilename))
							{
								IFileManager::Get().Delete(*TempFilename);
							}
						}

						if (FinalTimeStamp != FDateTime::MinValue())
						{
							IFileManager::Get().SetTimeStamp(*Filename, FinalTimeStamp);
						}
					}
					else
					{
						UE_LOG(LogSavePackage, Fatal, TEXT("Could not save to %s!"), *TempFilename);
					}
				}
				else
				{
					UE_LOG(LogSavePackage, Fatal, TEXT("Could not write to %s!"), *TempFilename);
				}
			}
			{
				FArchive* Ar = IFileManager::Get().CreateFileWriter(*TempFilenameExports);
				if (Ar)
				{
					Ar->Serialize(Data.Get() + HeaderSize, DataSize - HeaderSize);
					delete Ar;
					Ar = nullptr;

					// Clean-up the memory as soon as we save the file to reduce the memory footprint.
					Data.Reset();

					if (IFileManager::Get().FileSize(*TempFilenameExports) == DataSize - HeaderSize)
					{
						if (bUseTempFilename)
						{
							if (!IFileManager::Get().Move(*FilenameExports, *TempFilenameExports, true, true, false, false))
							{
								UE_LOG(LogSavePackage, Fatal, TEXT("Could not move to %s."), *Filename);
							}

							// if everything worked, this is not necessary, but we will make every effort to avoid leaving junk in the cache
							if (FPaths::FileExists(TempFilenameExports))
							{
								IFileManager::Get().Delete(*TempFilenameExports);
							}
						}

						if (FinalTimeStamp != FDateTime::MinValue())
						{
							IFileManager::Get().SetTimeStamp(*FilenameExports, FinalTimeStamp);
						}
					}
					else
					{
						UE_LOG(LogSavePackage, Fatal, TEXT("Could not save to %s!"), *TempFilenameExports);
					}
				}
				else
				{
					UE_LOG(LogSavePackage, Fatal, TEXT("Could not write to %s!"), *TempFilenameExports);
				}
			}
			OutstandingAsyncWrites.Decrement();
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncWriteWorkerWithSplitExports, STATGROUP_ThreadPoolAsyncTasks);
		}

	};

	OutstandingAsyncWrites.Increment();
	(new FAutoDeleteAsyncTask<FAsyncWriteWorkerWithSplitExports>(Filename, MoveTemp(Data), DataSize, HeaderSize, TimeStamp, bUseTempFilename))->StartBackgroundTask();
}

/** 
 * Helper object for all of the cross object state that is needed only while saving a package.
 */
class FSavePackageState
{
public:
	/** 
	 * Mark a FName as referenced.
	 * @param Name name to mark as referenced
	 */
	void MarkNameAsReferenced(const FName& Name)
	{
		// We need to store the FName without the number, as the number is stored separately by FLinkerSave 
		// and we don't want duplicate entries in the name table just because of the number
		const FName NameNoNumber(Name, 0);
		ReferencedNames.Add(NameNoNumber);
	}

#if WITH_EDITOR
	/**
	 * Names are cached before we replace objects for imports. So the names of replacements must be ealier.
	 */
	void AddReplacementsNames(UObject* Obj, const ITargetPlatform* TargetPlatform, const bool bIsCooking)
	{
		if (bIsCooking && TargetPlatform)
		{
			if (const IBlueprintNativeCodeGenCore* Coordinator = IBlueprintNativeCodeGenCore::Get())
			{
				const FCompilerNativizationOptions& NativizationOptions = Coordinator->GetNativizationOptionsForPlatform(TargetPlatform);
				if (const UClass* ReplObjClass = Coordinator->FindReplacedClassForObject(Obj, NativizationOptions))
				{
					MarkNameAsReferenced(ReplObjClass->GetFName());
				}

				FName ReplacedName;
				Coordinator->FindReplacedNameAndOuter(Obj, ReplacedName, NativizationOptions); //TODO: should we care about replaced outer ?
				if (ReplacedName != NAME_None)
				{
					MarkNameAsReferenced(ReplacedName);
				}
			}
		}
	}
#endif //WITH_EDITOR

	/** 
	 * Add the marked names to a linker
	 * @param Linker linker to save the marked names to
	 */
	void UpdateLinkerWithMarkedNames(FLinkerSave* Linker)
	{
		Linker->NameMap.Reserve(Linker->NameMap.Num() + ReferencedNames.Num());
		for (const FName& Name : ReferencedNames)
		{
			Linker->NameMap.Add(Name);
		}
	}

private:
	TSet<FName, FLinkerNamePairKeyFuncs> ReferencedNames;
};

static FSavePackageState* SavePackageState = nullptr;

/** 
 * Helper object to scope the package state in SavePackage()
 */
class FScopeSavePackageState
{
public:
	FScopeSavePackageState()
	{
		check(!SavePackageState);
		SavePackageState = new FSavePackageState;
	}
	~FScopeSavePackageState()
	{
		check(SavePackageState);
		delete SavePackageState;
		SavePackageState = nullptr;
	}
};

/** 
 * Returns if true if the object is editor-only:
 * - it's a package marked as PKG_EditorOnly
 * or
 * - it's inside a package marked as editor only
 * or 
 * - It's an editor only object
*/
bool IsEditorOnlyObject(const UObject* InObject, bool bCheckRecursive)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("IsEditorOnlyObject"), STAT_IsEditorOnlyObject, STATGROUP_LoadTime);

	// Configurable via ini setting
	static struct FCanStripEditorOnlyExportsAndImports
	{
		bool bCanStripEditorOnlyObjects;
		FCanStripEditorOnlyExportsAndImports()
			: bCanStripEditorOnlyObjects(true)
		{
			GConfig->GetBool(TEXT("Core.System"), TEXT("CanStripEditorOnlyExportsAndImports"), bCanStripEditorOnlyObjects, GEngineIni);
		}
		FORCEINLINE operator bool() const { return bCanStripEditorOnlyObjects; }
	} CanStripEditorOnlyExportsAndImports;
	if (!CanStripEditorOnlyExportsAndImports)
	{
		return false;
	}
	check(InObject);

	if (InObject->HasAnyMarks(OBJECTMARK_EditorOnly) || InObject->IsEditorOnly())
	{
		return true;
	}

	// If this is a package that is editor only or the object is in editor-only package,
	// the object is editor-only too.
	const bool bIsAPackage = InObject->IsA<UPackage>();
	const UPackage* Package = (bIsAPackage ? static_cast<const UPackage*>(InObject) : InObject->GetOutermost());
	if (Package && Package->HasAnyPackageFlags(PKG_EditorOnly))
	{
		return true;
	}

	if (bCheckRecursive && !InObject->IsNative())
	{
		UObject* Outer = InObject->GetOuter();
		if (Outer && Outer != Package)
		{
			if (IsEditorOnlyObject(Outer, true))
			{
				return true;
			}
		}
		const UStruct* InStruct = Cast<UStruct>(InObject);
		if (InStruct)
		{
			const UStruct* SuperStruct = InStruct->GetSuperStruct();
			if (SuperStruct && IsEditorOnlyObject(SuperStruct, true))
			{
				return true;
			}
		}
		else
		{
			if (IsEditorOnlyObject(InObject->GetClass(), true))
			{
				return true;
			}

			UObject* Archetype = InObject->GetArchetype();
			if (Archetype && IsEditorOnlyObject(Archetype, true))
			{
				return true;
			}
		}
	}
	return false;
}

/**
 * Marks object as not for client, not for server, or editor only. Recurses up outer/class chain as necessary
 */
static void ConditionallyExcludeObjectForTarget(UObject* Obj, EObjectMark ExcludedObjectMarks, const ITargetPlatform* TargetPlatform, const bool bIsCooking)
{
#if WITH_EDITOR
	if (!Obj || (ExcludedObjectMarks != OBJECTMARK_NOMARKS && Obj->HasAnyMarks(ExcludedObjectMarks)))
	{
		// No object or already marked
		return;
	}

	if (Obj->GetOutermost()->GetFName() == GLongCoreUObjectPackageName)
	{
		// Nothing in CoreUObject can be excluded
		return;
	}

	UObject* ObjOuter = Obj->GetOuter();
	UClass* ObjClass = Obj->GetClass();
	
	if (bIsCooking && TargetPlatform)
	{
		// Check for nativization replacement
		if (const IBlueprintNativeCodeGenCore* Coordinator = IBlueprintNativeCodeGenCore::Get())
		{
			const FCompilerNativizationOptions& NativizationOptions = Coordinator->GetNativizationOptionsForPlatform(TargetPlatform);
			FName UnusedName;
			if (UClass* ReplacedClass = Coordinator->FindReplacedClassForObject(Obj, NativizationOptions))
			{
				ObjClass = ReplacedClass;
			}
			if (UObject* ReplacedOuter = Coordinator->FindReplacedNameAndOuter(Obj, /*out*/UnusedName, NativizationOptions))
			{
				ObjOuter = ReplacedOuter;
			}
		}
	}

	EObjectMark InheritedMarks = OBJECTMARK_NOMARKS;

	auto InheritMark = [&InheritedMarks](UObject* ObjToCheck, EObjectMark MarkToCheck)
	{
		if (ObjToCheck->HasAnyMarks(MarkToCheck))
		{
			InheritedMarks = (EObjectMark)(InheritedMarks | MarkToCheck);
		}
	};

	// Recurse into parents, then compute inherited marks
	
	ConditionallyExcludeObjectForTarget(ObjClass, ExcludedObjectMarks, TargetPlatform, bIsCooking);
	InheritMark(ObjClass, OBJECTMARK_EditorOnly);
	InheritMark(ObjClass, OBJECTMARK_NotForClient);
	InheritMark(ObjClass, OBJECTMARK_NotForServer);

	if (ObjOuter)
	{
		ConditionallyExcludeObjectForTarget(ObjOuter, ExcludedObjectMarks, TargetPlatform, bIsCooking);
		InheritMark(ObjOuter, OBJECTMARK_EditorOnly);
		InheritMark(ObjOuter, OBJECTMARK_NotForClient);
		InheritMark(ObjOuter, OBJECTMARK_NotForServer);
	}

	// Check parent struct if we have one
	UStruct* ThisStruct = dynamic_cast<UStruct*>(Obj);
	if (ThisStruct && ThisStruct->GetSuperStruct())
	{
		UObject* SuperStruct = ThisStruct->GetSuperStruct();
		ConditionallyExcludeObjectForTarget(SuperStruct, ExcludedObjectMarks, TargetPlatform, bIsCooking);
		InheritMark(SuperStruct, OBJECTMARK_EditorOnly);
		InheritMark(SuperStruct, OBJECTMARK_NotForClient);
		InheritMark(SuperStruct, OBJECTMARK_NotForServer);
	}

	// Check archetype, this may not have been covered in the case of components
	UObject* Archetype = Obj->GetArchetype();

	if (Archetype)
	{
		ConditionallyExcludeObjectForTarget(Archetype, ExcludedObjectMarks, TargetPlatform, bIsCooking);
		InheritMark(Archetype, OBJECTMARK_EditorOnly);
		InheritMark(Archetype, OBJECTMARK_NotForClient);
		InheritMark(Archetype, OBJECTMARK_NotForServer);
	}

	if (Obj->HasAnyFlags(RF_ClassDefaultObject))
	{
		// If class is included, CDO must be included so only check inherited marks
		if (!Obj->HasAnyMarks(OBJECTMARK_EditorOnly) && (InheritedMarks & OBJECTMARK_EditorOnly))
		{
			Obj->Mark(OBJECTMARK_EditorOnly);
		}

		if (!Obj->HasAnyMarks(OBJECTMARK_NotForClient) && (InheritedMarks & OBJECTMARK_NotForClient))
		{
			Obj->Mark(OBJECTMARK_NotForClient);
		}

		if (!Obj->HasAnyMarks(OBJECTMARK_NotForServer) && (InheritedMarks & OBJECTMARK_NotForServer))
		{
			Obj->Mark(OBJECTMARK_NotForServer);
		}
	}
	else
	{
		if (!Obj->HasAnyMarks(OBJECTMARK_EditorOnly) && ((InheritedMarks & OBJECTMARK_EditorOnly) || IsEditorOnlyObject(Obj, false)))
		{
			Obj->Mark(OBJECTMARK_EditorOnly);
		}

		if (!Obj->HasAnyMarks(OBJECTMARK_NotForClient) && ((InheritedMarks & OBJECTMARK_NotForClient) || !Obj->NeedsLoadForClient()))
		{
			Obj->Mark(OBJECTMARK_NotForClient);
		}

		if (!Obj->HasAnyMarks(OBJECTMARK_NotForServer) && ((InheritedMarks & OBJECTMARK_NotForServer) || !Obj->NeedsLoadForServer()))
		{
			Obj->Mark(OBJECTMARK_NotForServer);
		}
	}

	// If NotForClient and NotForServer, it is implicitly editor only
	if (!Obj->HasAnyMarks(OBJECTMARK_EditorOnly) && Obj->HasAllMarks((EObjectMark)(OBJECTMARK_NotForClient|OBJECTMARK_NotForServer)))
	{
		Obj->Mark(OBJECTMARK_EditorOnly);
	}
#endif
}

/** For a CDO get all of the subobjects templates nested inside it or it's class */
static void GetCDOSubobjects(UObject* CDO, TArray<UObject*>& Subobjects)
{
	TArray<UObject*> CurrentSubobjects;
	TArray<UObject*> NextSubobjects;

	// Recursively search for subobjects. Only care about ones that have a full subobject chain as some nested objects are set wrong
	GetObjectsWithOuter(CDO->GetClass(), NextSubobjects, false);
	GetObjectsWithOuter(CDO, NextSubobjects, false);

	while (NextSubobjects.Num() > 0)
	{
		CurrentSubobjects = NextSubobjects;
		NextSubobjects.Empty();
		for (UObject* SubObj : CurrentSubobjects)
		{
			if (SubObj->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject))
			{
				Subobjects.Add(SubObj);
				GetObjectsWithOuter(SubObj, NextSubobjects, false);
			}
		}
	}
}

/**
 * Archive for tagging objects and names that must be exported
 * to the file.  It tags the objects passed to it, and recursively
 * tags all of the objects this object references.
 */
class FArchiveSaveTagExports : public FArchiveUObject
{
public:
	/**
	 * Constructor
	 * 
	 * @param	InOuter		the package to save
	 */
	FArchiveSaveTagExports( UPackage* InOuter )
	: Outer(InOuter)
	{
		ArIsSaving				= true;
		ArIsPersistent			= true;
		ArIsObjectReferenceCollector = true;
		ArShouldSkipBulkData	= true;
	}

	void ProcessBaseObject(UObject* BaseObject);
	virtual FArchive& operator<<(UObject*& Obj) override;
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override;

	/**
	 * Package we're currently saving.  Only objects contained
	 * within this package will be tagged for serialization.
	 */
	UPackage* Outer;

	virtual FString GetArchiveName() const;

private:

	TArray<UObject*> TaggedObjects;

	void ProcessTaggedObjects();
};

FString FArchiveSaveTagExports::GetArchiveName() const
{
	return Outer != nullptr
		? *FString::Printf(TEXT("SaveTagExports (%s)"), *Outer->GetName())
		: TEXT("SaveTagExports");
}

FArchive& FArchiveSaveTagExports::operator<<(FWeakObjectPtr& Value)
{
	if (IsEventDrivenLoaderEnabledInCookedBuilds() && IsCooking())
	{
		// Always serialize weak pointers for the purposes of object tagging
		UObject* Object = static_cast<UObject*>(Value.Get(true));
		*this << Object;
	}
	else
	{
		Value.Serialize(*this);
	}
	return *this;
}

FArchive& FArchiveSaveTagExports::operator<<(UObject*& Obj)
{
	check(Outer);

	// Check transient and pending kill flags for outers
	CheckObjectPriorToSave(*this, Obj, Outer);

	// Check outer chain for any exlcuded object marks
	const EObjectMark ExcludedObjectMarks = UPackage::GetExcludedObjectMarksForTargetPlatform(CookingTarget(), IsCooking());
	ConditionallyExcludeObjectForTarget(Obj, ExcludedObjectMarks, CookingTarget(), IsCooking());

	if (Obj && Obj->IsIn(Outer) && !Obj->HasAnyFlags(RF_Transient) && !Obj->HasAnyMarks((EObjectMark)(OBJECTMARK_TagExp | ExcludedObjectMarks)))
	{
		// It passed filtering so mark as export
		Obj->Mark(OBJECTMARK_TagExp);

		// First, serialize this object's archetype 
		UObject* Template = Obj->GetArchetype();
		*this << Template;

		// If this is a CDO, gather it's subobjects and serialize them
		if (Obj->HasAnyFlags(RF_ClassDefaultObject))
		{
			if (IsEventDrivenLoaderEnabledInCookedBuilds() && IsCooking())
			{
				// Gets all subobjects defined in a class, including the CDO, CDO components and blueprint-created components
				TArray<UObject*> ObjectTemplates;
				ObjectTemplates.Add(Obj);

				GetCDOSubobjects(Obj, ObjectTemplates);

				for (UObject* ObjTemplate : ObjectTemplates)
				{
					// Recurse into templates
					*this << ObjTemplate;
				}
			}
		}
	
		// NeedsLoadForEditor game is inherited to child objects, so check outer chain
		bool bNeedsLoadForEditorGame = false;
		for (UObject* OuterIt = Obj; OuterIt; OuterIt = OuterIt->GetOuter())
		{
			if (OuterIt->NeedsLoadForEditorGame())
			{
				bNeedsLoadForEditorGame = true;
				break;
			}
		}

		if(!bNeedsLoadForEditorGame && Obj->HasAnyFlags(RF_ClassDefaultObject))
		{
			bNeedsLoadForEditorGame = Obj->GetClass()->NeedsLoadForEditorGame();
		}

		if (!bNeedsLoadForEditorGame)
		{
			Obj->Mark(OBJECTMARK_NotAlwaysLoadedForEditorGame);
		}

		// Recurse with this object's class and package.
		UObject* Class  = Obj->GetClass();
		UObject* Parent = Obj->GetOuter();
		*this << Class << Parent;

		TaggedObjects.Add(Obj);
	}
	return *this;
}

/**
 * Serializes the specified object, tagging all objects it references.
 *
 * @param	BaseObject	the object that should be serialized; usually the package root or
 *						[in the case of a map package] the map's UWorld object.
 */
void FArchiveSaveTagExports::ProcessBaseObject(UObject* BaseObject)
{
	(*this) << BaseObject;
	ProcessTaggedObjects();
}

/**
 * Iterates over all objects which were encountered during serialization of the root object, serializing each one in turn.
 * Objects encountered during that serialization are then added to the array and iteration continues until no new objects are
 * added to the array.
 */
void FArchiveSaveTagExports::ProcessTaggedObjects()
{
	const int32 ArrayPreSize = 1024; // Was originally total number of objects, but this was unreasonably large
	TArray<UObject*> CurrentlyTaggedObjects;
	CurrentlyTaggedObjects.Empty(ArrayPreSize);
	while (TaggedObjects.Num())
	{
		CurrentlyTaggedObjects += TaggedObjects;
		TaggedObjects.Empty();

		for (int32 ObjIndex = 0; ObjIndex < CurrentlyTaggedObjects.Num(); ObjIndex++)
		{
			UObject* Obj = CurrentlyTaggedObjects[ObjIndex];

			if (Obj->HasAnyFlags(RF_ClassDefaultObject))
			{
				Obj->GetClass()->SerializeDefaultObject(Obj, *this);
			}
			// In the CDO case the above would serialize most of the references, including transient properties
			// but we still want to serialize the object using the normal path to collect all custom versions it might be using.
#if ENABLE_PACKAGE_CLASS_SERIALIZATION_TIMES
			auto& TimingInfo = SavePackageStats::TagExportSerializeTimes.FindOrAdd(Obj->GetClass()->GetFName());
			TimingInfo.Value++;
			FScopedDurationTimer SerializeTimer(TimingInfo.Key);
#endif
			Obj->Serialize(*this);
		}

		CurrentlyTaggedObjects.Empty(ArrayPreSize);
	}
}

/**
 * Archive for tagging objects and names that must be listed in the
 * file's imports table.
 */
class FArchiveSaveTagImports : public FArchiveUObject
{
public:
	FLinkerSave* Linker;
	TArray<UObject*> Dependencies;
	TArray<UObject*> NativeDependencies;
	TArray<UObject*> OtherImports;
	bool bIgnoreDependencies;

	/** Helper object to save/store state of bIgnoreDependencies */
	class FScopeIgnoreDependencies
	{
		FArchiveSaveTagImports& Archive;
		bool bScopedIgnoreDependencies;
		
	public:
		FScopeIgnoreDependencies(FArchiveSaveTagImports& InArchive)
			: Archive(InArchive)
			, bScopedIgnoreDependencies(InArchive.bIgnoreDependencies)
		{
			Archive.bIgnoreDependencies = true;
		}
		~FScopeIgnoreDependencies()
		{
			Archive.bIgnoreDependencies = bScopedIgnoreDependencies;
		}
	};

	FArchiveSaveTagImports(FLinkerSave* InLinker)
		: Linker(InLinker)
		, bIgnoreDependencies(false)
	{
		check(Linker);

		ArIsSaving				= true;
		ArIsPersistent			= true;
		ArIsObjectReferenceCollector = true;
		ArShouldSkipBulkData	= true;

		ArPortFlags = Linker->GetPortFlags();
		SetCookingTarget(Linker->CookingTarget());
	}

	virtual FArchive& operator<<(UObject*& Obj) override;
	virtual FArchive& operator<< (struct FWeakObjectPtr& Value) override;
	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override;
	virtual FArchive& operator<<(FSoftObjectPath& Value) override;
	virtual FArchive& operator<<(FName& Name) override;
	
	virtual void MarkSearchableName(const UObject* TypeObject, const FName& ValueName) const override;
	virtual FString GetArchiveName() const override;
};

FString FArchiveSaveTagImports::GetArchiveName() const
{
	if ( Linker != nullptr && Linker->LinkerRoot )
	{
		return FString::Printf(TEXT("SaveTagImports (%s)"), *Linker->LinkerRoot->GetName());
	}

	return TEXT("SaveTagImports");
}

FArchive& FArchiveSaveTagImports::operator<< (struct FWeakObjectPtr& Value)
{
	if (IsEventDrivenLoaderEnabledInCookedBuilds() && IsCooking())
	{
		// Always serialize weak pointers for the purposes of object tagging
		UObject* Object = static_cast<UObject*>(Value.Get(true));
		*this << Object;
	}
	else
	{
		Value.Serialize(*this);
	}
	return *this;
}

FArchive& FArchiveSaveTagImports::operator<<( UObject*& Obj )
{
	// Check transient and pending kill flags for outers
	CheckObjectPriorToSave(*this, Obj, nullptr);

	const EObjectMark ExcludedObjectMarks = UPackage::GetExcludedObjectMarksForTargetPlatform( CookingTarget(), IsCooking() );
	ConditionallyExcludeObjectForTarget(Obj, ExcludedObjectMarks, CookingTarget(), IsCooking());
	
	// Skip PendingKill objects and objects that don't pass the platform mark filter
	if (Obj && (ExcludedObjectMarks == OBJECTMARK_NOMARKS || !Obj->HasAnyMarks(ExcludedObjectMarks)))
	{
		bool bIsNative = Obj->IsNative();
		if( !Obj->HasAnyFlags(RF_Transient) || bIsNative)
		{
			const bool bIsTopLevelPackage = Obj->GetOuter() == nullptr && dynamic_cast<UPackage*>(Obj);
			UObject* Outer = Obj->GetOuter();

			// See if this is inside a native class
			while (!bIsNative && Outer)
			{
				if (dynamic_cast<UClass*>(Outer) && Outer->IsNative())
				{
					bIsNative = true;
				}
				Outer = Outer->GetOuter();
			}

			// We add objects as dependencies even if they're also exports
			if (!bIsTopLevelPackage && !bIgnoreDependencies)
			{
				TArray<UObject*>& DependencyArray = bIsNative ? NativeDependencies : Dependencies;
				if (DependencyArray.Contains(Obj))
				{
					return *this;
				}
				DependencyArray.Add(Obj);
			}
			
			if( !Obj->HasAnyMarks(OBJECTMARK_TagExp) )  
			{
				// Add into other imports list unless it's already there
				if (bIsTopLevelPackage || bIgnoreDependencies)
				{
					if (OtherImports.Contains(Obj))
					{
						return *this;
					}

					OtherImports.Add(Obj);
				}

				// Mark this object as an import
				Obj->Mark(OBJECTMARK_TagImp);
				UClass* ClassObj = Cast<UClass>(Obj);

				// Don't recurse into CDOs if we're already ignoring dependencies, we only want to recurse into our outer chain in that case
				if (IsEventDrivenLoaderEnabledInCookedBuilds() && IsCooking() && !bIsNative && !bIgnoreDependencies && ClassObj)
				{
					// We don't want to add this to Dependencies, we simply want it to be an import so that a serialization before creation dependency can be created to the CDO
					FScopeIgnoreDependencies IgnoreDependencies(*this);
					UObject* CDO = ClassObj->GetDefaultObject();

					if (CDO)
					{
						// Gets all subobjects defined in a class, including the CDO, CDO components and blueprint-created components
						TArray<UObject*> ObjectTemplates;
						ObjectTemplates.Add(CDO);

						GetCDOSubobjects(CDO, ObjectTemplates);

						for (UObject* ObjTemplate : ObjectTemplates)
						{
							// Recurse into templates
							*this << ObjTemplate;
						}
					}
#if WITH_EDITOR
					SavePackageState->AddReplacementsNames(Obj, CookingTarget(), IsCooking());
#endif //WITH_EDITOR
				}

				// Recurse into parent
				UObject* Parent = Obj->GetOuter();
#if WITH_EDITOR
				if (IsCooking() && CookingTarget())
				{
					if (const IBlueprintNativeCodeGenCore* Coordinator = IBlueprintNativeCodeGenCore::Get())
					{
						FName UnusedName;
						UObject* ReplacedOuter = Coordinator->FindReplacedNameAndOuter(Obj, /*out*/UnusedName, Coordinator->GetNativizationOptionsForPlatform(CookingTarget()));
						Parent = ReplacedOuter ? ReplacedOuter : Obj->GetOuter();
					}
				}
#endif //WITH_EDITOR
				if( Parent )
				{
					*this << Parent;
				}
			}
		}
	}
	return *this;
}

FArchive& FArchiveSaveTagImports::operator<<( FLazyObjectPtr& LazyObjectPtr)
{
	FUniqueObjectGuid ID;
	ID = LazyObjectPtr.GetUniqueID();
	return *this << ID;
}

FArchive& FArchiveSaveTagImports::operator<<(FSoftObjectPath& Value)
{
	if (Value.IsValid())
	{
		Value.SerializePath(*this);

		FString Path = Value.ToString();
		FName PackageName = FName(*FPackageName::ObjectPathToPackageName(Path));

		SavePackageState->MarkNameAsReferenced(PackageName);
		Linker->SoftPackageReferenceList.AddUnique(PackageName);	
	}
	return *this;
}

FArchive& FArchiveSaveTagImports::operator<<(FName& Name)
{
	SavePackageState->MarkNameAsReferenced(Name);
	return *this;
}

void FArchiveSaveTagImports::MarkSearchableName(const UObject* TypeObject, const FName& ValueName) const
{
	if (!TypeObject)
	{
		return;
	}

	if (!Dependencies.Contains(TypeObject))
	{
		// Serialize object to make sure it ends up in import table
		// This is doing a const cast to avoid backward compatibility issues
		FArchiveSaveTagImports* MutableArchive = const_cast<FArchiveSaveTagImports*>(this);
		UObject* TempObject = const_cast<UObject*>(TypeObject);
		(*MutableArchive) << TempObject;
	}

	// Manually mark the name as referenced, in case it got skipped due to delta serialization
	SavePackageState->MarkNameAsReferenced(ValueName);

	Linker->SearchableNamesObjectMap.FindOrAdd(TypeObject).AddUnique(ValueName);
}

/**
 * Find most likely culprit that caused the objects in the passed in array to be considered for saving.
 *
 * @param	BadObjects	array of objects that are considered "bad" (e.g. non- RF_Public, in different map package, ...)
 * @return	UObject that is considered the most likely culprit causing them to be referenced or NULL
 */
static void FindMostLikelyCulprit( TArray<UObject*> BadObjects, UObject*& MostLikelyCulprit, const UProperty*& PropertyRef )
{
	MostLikelyCulprit	= nullptr;

	// Iterate over all objects that are marked as unserializable/ bad and print out their referencers.
	for( int32 BadObjIndex=0; BadObjIndex<BadObjects.Num(); BadObjIndex++ )
	{
		UObject* Obj = BadObjects[BadObjIndex];

		UE_LOG(LogSavePackage, Warning, TEXT("\r\nReferencers of %s:"), *Obj->GetFullName() );
		
		FReferencerInformationList Refs;

		if (IsReferenced(Obj, RF_Public, EInternalObjectFlags::Native, true, &Refs))
		{
			for (int32 i = 0; i < Refs.ExternalReferences.Num(); i++)
			{
				UObject* RefObj = Refs.ExternalReferences[i].Referencer;
				if (RefObj->HasAnyMarks(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp)))
				{
					if (RefObj->GetFName() == NAME_PersistentLevel || RefObj->GetClass()->GetFName() == WorldClassName)
					{
						// these types of references should be ignored
						continue;
					}

					UE_LOG(LogSavePackage, Warning, TEXT("\t%s (%i refs)"), *RefObj->GetFullName(), Refs.ExternalReferences[i].TotalReferences);
					for (int32 j = 0; j < Refs.ExternalReferences[i].ReferencingProperties.Num(); j++)
					{
						const UProperty* Prop = Refs.ExternalReferences[i].ReferencingProperties[j];
						UE_LOG(LogSavePackage, Warning, TEXT("\t\t%i) %s"), j, *Prop->GetFullName());
						PropertyRef = Prop;
					}

					MostLikelyCulprit = Obj;
				}
			}
		}
	}
}

/**
 * Helper structure encapsulating functionality to sort a linker's name map according to the order of the names a package being conformed against.
 */
struct FObjectNameSortHelper
{
private:
	/** the linker that we're sorting names for */
	friend struct TDereferenceWrapper<FName, FObjectNameSortHelper>;

	/** Comparison function used by Sort */
	FORCEINLINE bool operator()( const FName& A, const FName& B ) const
	{
		return A.Compare(B) < 0;
	}

public:


	/**
	 * Sorts names according to the order in which they occur in the list of NameIndices.  If a package is specified to be conformed against, ensures that the order
	 * of the names match the order in which the corresponding names occur in the old package.
	 *
	 * @param	Linker				linker containing the names that need to be sorted
	 * @param	LinkerToConformTo	optional linker to conform against.
	 */
	void SortNames( FLinkerSave* Linker, FLinkerLoad* LinkerToConformTo=nullptr )
	{
		int32 SortStartPosition = 0;

		if ( LinkerToConformTo != nullptr )
		{
			SortStartPosition = LinkerToConformTo->NameMap.Num();
			TArray<FName> ConformedNameMap = LinkerToConformTo->NameMap;
			for ( int32 NameIndex = 0; NameIndex < Linker->NameMap.Num(); NameIndex++ )
			{
				FName& CurrentName = Linker->NameMap[NameIndex];
				if ( !ConformedNameMap.Contains(CurrentName) )
				{
					ConformedNameMap.Add(CurrentName);
				}
			}

			Linker->NameMap = ConformedNameMap;
			for ( int32 NameIndex = 0; NameIndex < Linker->NameMap.Num(); NameIndex++ )
			{
				FName& CurrentName = Linker->NameMap[NameIndex];
				SavePackageState->MarkNameAsReferenced(CurrentName);
			}
		}

		if ( SortStartPosition < Linker->NameMap.Num() )
		{
			Sort( &Linker->NameMap[SortStartPosition], Linker->NameMap.Num() - SortStartPosition, FObjectNameSortHelper() );
		}
	}
};

/**
 * Helper structure to encapsulate sorting a linker's import table according to the of the import table of the package being conformed against.
 */
struct FObjectImportSortHelper
{
private:
	/**
	 * Map of UObject => full name; optimization for sorting.
	 */
	TMap<UObject*,FString>			ObjectToFullNameMap;

	/** the linker that we're sorting names for */
	friend struct TDereferenceWrapper<FObjectImport,FObjectImportSortHelper>;

	/** Comparison function used by Sort */
	bool operator()( const FObjectImport& A, const FObjectImport& B ) const
	{
		int32 Result = 0;
		if ( A.XObject == nullptr )
		{
			Result = 1;
		}
		else if ( B.XObject == nullptr )
		{
			Result = -1;
		}
		else
		{
			const FString* FullNameA = ObjectToFullNameMap.Find(A.XObject);
			const FString* FullNameB = ObjectToFullNameMap.Find(B.XObject);
			checkSlow(FullNameA);
			checkSlow(FullNameB);

			Result = FCString::Stricmp(**FullNameA, **FullNameB);
		}

		return Result < 0;
	}

public:

	/**
	 * Sorts imports according to the order in which they occur in the list of imports.  If a package is specified to be conformed against, ensures that the order
	 * of the imports match the order in which the corresponding imports occur in the old package.
	 *
	 * @param	Linker				linker containing the imports that need to be sorted
	 * @param	LinkerToConformTo	optional linker to conform against.
	 */
	void SortImports( FLinkerSave* Linker, FLinkerLoad* LinkerToConformTo=nullptr )
	{
		int32 SortStartPosition=0;
		TArray<FObjectImport>& Imports = Linker->ImportMap;
		if ( LinkerToConformTo )
		{
			// intended to be a copy
			TArray<FObjectImport> Orig = Imports;
			Imports.Empty(Imports.Num());

			// this array tracks which imports from the new package exist in the old package
			TArray<uint8> Used;
			Used.AddZeroed(Orig.Num());

			TMap<FString,int32> OriginalImportIndexes;
			for ( int32 i = 0; i < Orig.Num(); i++ )
			{
				FObjectImport& Import = Orig[i];
				FString ImportFullName = Import.XObject->GetFullName();

				OriginalImportIndexes.Add( *ImportFullName, i );
				ObjectToFullNameMap.Add(Import.XObject, *ImportFullName);
			}

			for( int32 i=0; i<LinkerToConformTo->ImportMap.Num(); i++ )
			{
				// determine whether the new version of the package contains this export from the old package
				int32* OriginalImportPosition = OriginalImportIndexes.Find( *LinkerToConformTo->GetImportFullName(i) );
				if( OriginalImportPosition )
				{
					// this import exists in the new package as well,
					// create a copy of the FObjectImport located at the original index and place it
					// into the matching position in the new package's import map
					FObjectImport* NewImport = new(Imports) FObjectImport( Orig[*OriginalImportPosition] );
					check(NewImport->XObject == Orig[*OriginalImportPosition].XObject);
					Used[ *OriginalImportPosition ] = 1;
				}
				else
				{
					// this import no longer exists in the new package
					new(Imports)FObjectImport( nullptr );
				}
			}

			SortStartPosition = LinkerToConformTo->ImportMap.Num();
			for( int32 i=0; i<Used.Num(); i++ )
			{
				if( !Used[i] )
				{
					// the FObjectImport located at pos "i" in the original import table did not
					// exist in the old package - add it to the end of the import table
					new(Imports) FObjectImport( Orig[i] );
				}
			}
		}
		else
		{
			for ( int32 ImportIndex = 0; ImportIndex < Linker->ImportMap.Num(); ImportIndex++ )
			{
				const FObjectImport& Import = Linker->ImportMap[ImportIndex];
				if ( Import.XObject )
				{
					ObjectToFullNameMap.Add(Import.XObject, *Import.XObject->GetFullName());
				}
			}
		}

		if ( SortStartPosition < Linker->ImportMap.Num() )
		{
			Sort( &Linker->ImportMap[SortStartPosition], Linker->ImportMap.Num() - SortStartPosition, *this );
		}
	}
};

/**
 * Helper structure to encapsulate sorting a linker's export table alphabetically, taking into account conforming to other linkers.
 */
struct FObjectExportSortHelper
{
private:
	struct FObjectFullName
	{
	public:
		FObjectFullName(const UObject* Object, const UObject* Root)
		{
			ClassName = Object->GetClass()->GetFName();
			const UObject* Current = Object;
			while (Current != nullptr && Current != Root)
			{
				Path.Insert(Current->GetFName(), 0);
				Current = Current->GetOuter();
			}
		}

		FObjectFullName(FObjectFullName&& InFullName)
		{
			ClassName = InFullName.ClassName;
			Swap(Path, InFullName.Path);
		}
		FName ClassName;
		TArray<FName> Path;
	};

	bool bUseFObjectFullName;


	TMap<UObject*, FObjectFullName> ObjectToObjectFullNameMap;

	/**
	 * Map of UObject => full name; optimization for sorting.
	 */
	TMap<UObject*,FString>			ObjectToFullNameMap;

	/** the linker that we're sorting exports for */
	friend struct TDereferenceWrapper<FObjectExport,FObjectExportSortHelper>;

	/** Comparison function used by Sort */
	bool operator()( const FObjectExport& A, const FObjectExport& B ) const
	{
		int32 Result = 0;
		if ( A.Object == nullptr )
		{
			Result = 1;
		}
		else if ( B.Object == nullptr )
		{
			Result = -1;
		}
		else
		{
			if (bUseFObjectFullName)
			{
				const FObjectFullName* FullNameA = ObjectToObjectFullNameMap.Find(A.Object);
				const FObjectFullName* FullNameB = ObjectToObjectFullNameMap.Find(B.Object);
				checkSlow(FullNameA);
				checkSlow(FullNameB);

				if (FullNameA->ClassName != FullNameB->ClassName)
				{
					Result = FCString::Stricmp(*FullNameA->ClassName.ToString(), *FullNameB->ClassName.ToString());
				}
				else
				{
					int Num = FMath::Min(FullNameA->Path.Num(), FullNameB->Path.Num());
					for (int I = 0; I < Num; ++I)
					{
						if (FullNameA->Path[I] != FullNameB->Path[I])
						{
							Result = FCString::Stricmp(*FullNameA->Path[I].ToString(), *FullNameB->Path[I].ToString());
							break;
						}
					}
					if (Result == 0)
					{
						Result = FullNameA->Path.Num() - FullNameB->Path.Num();
					}
				}
			}
			else
			{
				const FString* FullNameA = ObjectToFullNameMap.Find(A.Object);
				const FString* FullNameB = ObjectToFullNameMap.Find(B.Object);
				checkSlow(FullNameA);
				checkSlow(FullNameB);

				Result = FCString::Stricmp(**FullNameA, **FullNameB);
			}
		}

		return Result < 0;
	}

public:

	FObjectExportSortHelper() : bUseFObjectFullName(false)
	{}

	/**
	 * Sorts exports alphabetically.  If a package is specified to be conformed against, ensures that the order
	 * of the exports match the order in which the corresponding exports occur in the old package.
	 *
	 * @param	Linker				linker containing the exports that need to be sorted
	 * @param	LinkerToConformTo	optional linker to conform against.
	 */
	void SortExports( FLinkerSave* Linker, FLinkerLoad* LinkerToConformTo=nullptr, bool InbUseFObjectFullName = false)
	{
		bUseFObjectFullName = InbUseFObjectFullName;

		int32 SortStartPosition=0;
		if ( LinkerToConformTo )
		{
			// build a map of object full names to the index into the new linker's export map prior to sorting.
			// we need to do a little trickery here to generate an object path name that will match what we'll get back
			// when we call GetExportFullName on the LinkerToConformTo's exports, due to localized packages and forced exports.
			const FString LinkerName = Linker->LinkerRoot->GetName();
			const FString PathNamePrefix = LinkerName + TEXT(".");

			// Populate object to current index map.
			TMap<FString,int32> OriginalExportIndexes;
			for( int32 ExportIndex=0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
			{
				const FObjectExport& Export = Linker->ExportMap[ExportIndex];
				if( Export.Object )
				{
					// get the path name for this object; if the object is contained within the package we're saving,
					// we don't want the returned path name to contain the package name since we'll be adding that on
					// to ensure that forced exports have the same outermost name as the non-forced exports
					FString ObjectPathName = Export.Object != Linker->LinkerRoot
						? Export.Object->GetPathName(Linker->LinkerRoot)
						: LinkerName;
						
					FString ExportFullName = Export.Object->GetClass()->GetName() + TEXT(" ") + PathNamePrefix + ObjectPathName;

					// Set the index (key) in the map to the index of this object into the export map.
					OriginalExportIndexes.Add( *ExportFullName, ExportIndex );
					if (bUseFObjectFullName)
					{
						FObjectFullName ObjectFullName(Export.Object, Linker->LinkerRoot);
						ObjectToObjectFullNameMap.Add(Export.Object, MoveTemp(ObjectFullName)); 
					}
					else
					{
						ObjectToFullNameMap.Add(Export.Object, *ExportFullName);
					}
				}
			}

			// backup the existing export list so we can empty the linker's actual list
			TArray<FObjectExport> OldExportMap = Linker->ExportMap;
			Linker->ExportMap.Empty(Linker->ExportMap.Num());

			// this array tracks which exports from the new package exist in the old package
			TArray<uint8> Used;
			Used.AddZeroed(OldExportMap.Num());

			for( int32 i = 0; i<LinkerToConformTo->ExportMap.Num(); i++ )
			{
				// determine whether the new version of the package contains this export from the old package
				FString ExportFullName = LinkerToConformTo->GetExportFullName(i, *LinkerName);
				int32* OriginalExportPosition = OriginalExportIndexes.Find( *ExportFullName );
				if( OriginalExportPosition )
				{
					// this export exists in the new package as well,
					// create a copy of the FObjectExport located at the original index and place it
					// into the matching position in the new package's export map
					FObjectExport* NewExport = new(Linker->ExportMap) FObjectExport( OldExportMap[*OriginalExportPosition] );
					check(NewExport->Object == OldExportMap[*OriginalExportPosition].Object);
					Used[ *OriginalExportPosition ] = 1;
				}
				else
				{

					// this export no longer exists in the new package; to ensure that the _LinkerIndex matches, add an empty entry to pad the list
					new(Linker->ExportMap)FObjectExport( nullptr );
					UE_LOG(LogSavePackage, Log, TEXT("No matching export found in new package for original export %i: %s"), i, *ExportFullName);
				}
			}



			SortStartPosition = LinkerToConformTo->ExportMap.Num();
			for( int32 i=0; i<Used.Num(); i++ )
			{
				if( !Used[i] )
				{
					// the FObjectExport located at pos "i" in the original export table did not
					// exist in the old package - add it to the end of the export table
					new(Linker->ExportMap) FObjectExport( OldExportMap[i] );
				}
			}

#if DO_GUARD_SLOW

			// sanity-check: make sure that all exports which existed in the linker before we sorted exist in the linker's export map now
			{
				TSet<UObject*> ExportObjectList;
				for( int32 ExportIndex=0; ExportIndex<Linker->ExportMap.Num(); ExportIndex++ )
				{
					ExportObjectList.Add(Linker->ExportMap[ExportIndex].Object);
				}

				for( int32 OldExportIndex=0; OldExportIndex<OldExportMap.Num(); OldExportIndex++ )
				{
					check(ExportObjectList.Contains(OldExportMap[OldExportIndex].Object));
				}
			}
#endif
		}
		else
		{
			for ( int32 ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
			{
				const FObjectExport& Export = Linker->ExportMap[ExportIndex];
				if ( Export.Object )
				{
					if (bUseFObjectFullName)
					{
						FObjectFullName ObjectFullName(Export.Object, nullptr);
						ObjectToObjectFullNameMap.Add(Export.Object, MoveTemp(ObjectFullName));
					}
					else
					{
						ObjectToFullNameMap.Add(Export.Object, *Export.Object->GetFullName());
					}
				}
			}
		}

		if ( SortStartPosition < Linker->ExportMap.Num() )
		{
			Sort( &Linker->ExportMap[SortStartPosition], Linker->ExportMap.Num() - SortStartPosition, *this );
		}
	}
};


class FExportReferenceSorter : public FArchiveUObject
{
	/**
	 * Verifies that all objects which will be force-loaded when the export at RelativeIndex is created and/or loaded appear in the sorted list of exports
	 * earlier than the export at RelativeIndex.
	 *
	 * Used for tracking down the culprit behind dependency sorting bugs.
	 *
	 * @param	RelativeIndex	the index into the sorted export list to check dependencies for
	 * @param	CheckObject		the object that will be force-loaded by the export at RelativeIndex
	 * @param	ReferenceType	the relationship between the object at RelativeIndex and CheckObject (archetype, class, etc.)
	 * @param	out_ErrorString	if incorrect sorting is detected, receives data containing more information about the incorrectly sorted object.
	 *
	 * @param	true if the export at RelativeIndex appears later than the exports associated with any objects that it will force-load; false otherwise.
	 */
	bool VerifyDependency( const int32 RelativeIndex, UObject* CheckObject, const FString& ReferenceType, FString& out_ErrorString )
	{
		bool bResult = false;

		checkf(ReferencedObjects.IsValidIndex(RelativeIndex), TEXT("Invalid index specified: %i (of %i)"), RelativeIndex, ReferencedObjects.Num());

		UObject* SourceObject = ReferencedObjects[RelativeIndex];
		checkf(SourceObject, TEXT("nullptr Object at location %i in ReferencedObjects list"), RelativeIndex);
		checkf(CheckObject, TEXT("CheckObject is nullptr for %s (%s)"), *SourceObject->GetFullName(), *ReferenceType);

		if ( SourceObject->GetOutermost() != CheckObject->GetOutermost() )
		{
			// not in the same package; therefore we can assume that the dependent object will exist
			bResult = true;
		}
		else
		{
			int32 OtherIndex = ReferencedObjects.Find(CheckObject);
			if ( OtherIndex != INDEX_NONE )
			{
				if ( OtherIndex < RelativeIndex )
				{
					bResult = true;
				}
				else
				{
					out_ErrorString = FString::Printf(TEXT("Sorting error detected (%s appears later in ReferencedObjects list)!  %i) %s   =>  %i) %s"), *ReferenceType, RelativeIndex,
						*SourceObject->GetFullName(), OtherIndex, *CheckObject->GetFullName());

					bResult = false;
				}
			}
			else
			{
				// the object isn't in the list of ReferencedObjects, which means it wasn't processed as a result of processing the source object; this
				// might indicate a bug, but might also just mean that the CheckObject was first referenced by an earlier export
				int32 ProcessedIndex = ProcessedObjects.Find(CheckObject);
				if ( ProcessedIndex != INDEX_NONE )
				{
					OtherIndex = ProcessedIndex;
					int32 SourceIndex = ProcessedObjects.Find(SourceObject);

					if ( OtherIndex < SourceIndex )
					{
						bResult = true;
					}
					else
					{
						out_ErrorString = FString::Printf(TEXT("Sorting error detected (%s was processed but not added to ReferencedObjects list)!  %i/%i) %s   =>  %i) %s"),
							*ReferenceType, RelativeIndex, SourceIndex, *SourceObject->GetFullName(), OtherIndex, *CheckObject->GetFullName());
						bResult = false;
					}
				}
				else
				{
					int32 SourceIndex = ProcessedObjects.Find(SourceObject);

					out_ErrorString = FString::Printf(TEXT("Sorting error detected (%s has not yet been processed)!  %i/%i) %s   =>  %s"),
						*ReferenceType, RelativeIndex, SourceIndex, *SourceObject->GetFullName(), *CheckObject->GetFullName());

					bResult = false;
				}
			}
		}

		return bResult;
	}

	/**
	 * Pre-initializes the list of processed objects with the boot-strap classes.
	 */
	void InitializeCoreClasses()
	{
#if 1
		check(CoreClasses.Num() == 0);
		check(ReferencedObjects.Num() == 0);
		check(ForceLoadObjects.Num() == 0);
		check(SerializedObjects.Num() == 0);
		check(bIgnoreFieldReferences == false);

		static bool bInitializedStaticCoreClasses = false;
		static TArray<UClass*> StaticCoreClasses;
		static TArray<UObject*> StaticCoreReferencedObjects;
		static TArray<UObject*> StaticProcessedObjects;
		static TArray<UObject*> StaticForceLoadObjects;
		static TSet<UObject*> StaticSerializedObjects;
		
		

		// Helper class to register FlushInitializedStaticCoreClasses callback on first SavePackage run
		struct FAddFlushInitalizedStaticCoreClasses
		{
			FAddFlushInitalizedStaticCoreClasses() 
			{
				FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddStatic(FlushInitalizedStaticCoreClasses);
			}
			/** Wrapper function to handle default parameter when used as function pointer */
			static void FlushInitalizedStaticCoreClasses()
			{
				bInitializedStaticCoreClasses = false;
			}
		};
		static FAddFlushInitalizedStaticCoreClasses MaybeAddAddFlushInitializedStaticCoreClasses;

#if VALIDATE_INITIALIZECORECLASSES
		bool bWasValid = bInitializedStaticCoreClasses;
		bInitializedStaticCoreClasses = false;
#endif

		if (!bInitializedStaticCoreClasses)
		{
			bInitializedStaticCoreClasses = true;


			// initialize the tracking maps with the core classes
			UClass* CoreClassList[] =
			{
				UObject::StaticClass(),
				UField::StaticClass(),
				UStruct::StaticClass(),
				UScriptStruct::StaticClass(),
				UFunction::StaticClass(),
				UEnum::StaticClass(),
				UClass::StaticClass(),
				UProperty::StaticClass(),
				UByteProperty::StaticClass(),
				UIntProperty::StaticClass(),
				UBoolProperty::StaticClass(),
				UFloatProperty::StaticClass(),
				UDoubleProperty::StaticClass(),
				UObjectProperty::StaticClass(),
				UClassProperty::StaticClass(),
				UInterfaceProperty::StaticClass(),
				UNameProperty::StaticClass(),
				UStrProperty::StaticClass(),
				UArrayProperty::StaticClass(),
				UTextProperty::StaticClass(),
				UStructProperty::StaticClass(),
				UDelegateProperty::StaticClass(),
				UInterface::StaticClass(),
				UMulticastDelegateProperty::StaticClass(),
				UWeakObjectProperty::StaticClass(),
				UObjectPropertyBase::StaticClass(),
				ULazyObjectProperty::StaticClass(),
				USoftObjectProperty::StaticClass(),
				USoftClassProperty::StaticClass(),
				UMapProperty::StaticClass(),
				USetProperty::StaticClass(),
				UEnumProperty::StaticClass()
			};

			for (UClass* CoreClass : CoreClassList)
			{
				CoreClasses.AddUnique(CoreClass);

				ReferencedObjects.Add(CoreClass);
				ReferencedObjects.Add(CoreClass->GetDefaultObject());
			}

			for (UClass* CoreClass : CoreClasses)
			{
				ProcessStruct(CoreClass);
			}

			CoreReferencesOffset = ReferencedObjects.Num();


#if VALIDATE_INITIALIZECORECLASSES
			if (bWasValid)
			{
				// make sure everything matches up 
				check(CoreClasses.Num() == StaticCoreClasses.Num());
				check(ReferencedObjects.Num() == StaticCoreReferencedObjects.Num());
				check(ProcessedObjects.Num() == StaticProcessedObjects.Num());
				check(ForceLoadObjects.Num() == StaticForceLoadObjects.Num());
				check(SerializedObjects.Num() == StaticSerializedObjects.Num());
				
				
				for (int I = 0; I < CoreClasses.Num(); ++I)
				{
					check(CoreClasses[I] == StaticCoreClasses[I]);
				}
				for (int I = 0; I < ReferencedObjects.Num(); ++I)
				{
					check(ReferencedObjects[I] == StaticCoreReferencedObjects[I]);
				}
				for (const auto& ProcessedObject : ProcessedObjects.ObjectsSet)
				{
					check(ProcessedObject.Value == StaticProcessedObjects.Find(ProcessedObject.Key));
				}
				for (int I = 0; I < ForceLoadObjects.Num(); ++I)
				{
					check(ForceLoadObjects[I] == StaticForceLoadObjects[I]);
				}
				for (const auto& SerializedObject : SerializedObjects)
				{
					check(StaticSerializedObjects.Find(SerializedObject));
				}
			}
#endif

			StaticCoreClasses = CoreClasses;
			StaticCoreReferencedObjects = ReferencedObjects;
			StaticProcessedObjects = ProcessedObjects;
			StaticForceLoadObjects = ForceLoadObjects;
			StaticSerializedObjects = SerializedObjects;

			check(CurrentClass == nullptr);
			check(CurrentInsertIndex == INDEX_NONE);
		}
		else
		{
			CoreClasses = StaticCoreClasses;
			ReferencedObjects = StaticCoreReferencedObjects;
			ProcessedObjects = StaticProcessedObjects;
			ForceLoadObjects = StaticForceLoadObjects;
			SerializedObjects = StaticSerializedObjects;

			CoreReferencesOffset = StaticCoreReferencedObjects.Num();
		}

#else
		// initialize the tracking maps with the core classes
		UClass* CoreClassList[] =
		{
			UObject::StaticClass(),
			UField::StaticClass(),
			UStruct::StaticClass(),
			UScriptStruct::StaticClass(),
			UFunction::StaticClass(),
			UEnum::StaticClass(),
			UClass::StaticClass(),
			UProperty::StaticClass(),
			UByteProperty::StaticClass(),
			UIntProperty::StaticClass(),
			UBoolProperty::StaticClass(),
			UFloatProperty::StaticClass(),
			UDoubleProperty::StaticClass(),
			UObjectProperty::StaticClass(),
			UClassProperty::StaticClass(),
			UInterfaceProperty::StaticClass(),
			UNameProperty::StaticClass(),
			UStrProperty::StaticClass(),
			UArrayProperty::StaticClass(),
			UTextProperty::StaticClass(),
			UStructProperty::StaticClass(),
			UDelegateProperty::StaticClass(),
			UInterface::StaticClass(),
			UMulticastDelegateProperty::StaticClass(),
			UWeakObjectProperty::StaticClass(),
			UObjectPropertyBase::StaticClass(),
			ULazyObjectProperty::StaticClass(),
			USoftObjectProperty::StaticClass(),
			USoftClassProperty::StaticClass(),
			UMapProperty::StaticClass(),
			USetProperty::StaticClass(),
			UEnumProperty::StaticClass()
		};

		for (UClass* CoreClass : CoreClassList)
		{
			CoreClasses.AddUnique(CoreClass);

			ReferencedObjects.Add(CoreClass);
			ReferencedObjects.Add(CoreClass->GetDefaultObject());
		}

		for (UClass* CoreClass : CoreClasses)
		{
			ProcessStruct(CoreClass);
		}

		CoreReferencesOffset = ReferencedObjects.Num();
#endif
	}

	/**
	 * Adds an object to the list of referenced objects, ensuring that the object is not added more than one.
	 *
	 * @param	Object			the object to add to the list
	 * @param	InsertIndex		the index to insert the object into the export list
	 */
	void AddReferencedObject( UObject* Object, int32 InsertIndex )
	{
		if ( Object != nullptr && !ReferencedObjects.Contains(Object) )
		{
			ReferencedObjects.Insert(Object, InsertIndex);
		}
	}

	/**
	 * Handles serializing and calculating the correct insertion point for an object that will be force-loaded by another object (via an explicit call to Preload).
	 * If the RequiredObject is a UStruct or true is specified for bProcessObject, the RequiredObject will be inserted into the list of exports just before the object
	 * that has a dependency on this RequiredObject.
	 *
	 * @param	RequiredObject		the object which must be created and loaded first
	 * @param	bProcessObject		normally, only the class and archetype for non-UStruct objects are inserted into the list;  specify true to override this behavior
	 *								if RequiredObject is going to be force-loaded, rather than just created
	 */
	void HandleDependency( UObject* RequiredObject, bool bProcessObject=false )
	{
		if ( RequiredObject != nullptr )
		{
			check(CurrentInsertIndex!=INDEX_NONE);

			const int32 PreviousReferencedObjectCount = ReferencedObjects.Num();
			const int32 PreviousInsertIndex = CurrentInsertIndex;

			if (!PackageToSort || RequiredObject->GetOutermost() == PackageToSort)
			{
				// Don't compute prerequisites for objects outside the package, this will recurse into all native properties
				if (UStruct* RequiredObjectStruct = dynamic_cast<UStruct*>(RequiredObject))
				{
					// if this is a struct/class/function/state, it may have a super that needs to be processed first
					ProcessStruct(RequiredObjectStruct);
				}
				else if (bProcessObject)
				{
					// this means that RequiredObject is being force-loaded by the referencing object, rather than simply referenced
					ProcessObject(RequiredObject);
				}
				else
				{
					// only the object's class and archetype are force-loaded, so only those objects need to be in the list before
					// whatever object was referencing RequiredObject
					if (ProcessedObjects.Find(RequiredObject->GetOuter()) == INDEX_NONE)
					{
						HandleDependency(RequiredObject->GetOuter());
					}

					// class is needed before archetype, but we need to process these in reverse order because we are inserting into the list.
					ProcessObject(RequiredObject->GetArchetype());
					ProcessStruct(RequiredObject->GetClass());
				}
			}
			// InsertIndexOffset is the amount the CurrentInsertIndex was incremented during the serialization of SuperField; we need to
			// subtract out this number to get the correct location of the new insert index
			const int32 InsertIndexOffset = CurrentInsertIndex - PreviousInsertIndex;
			const int32 InsertIndexAdvanceCount = (ReferencedObjects.Num() - PreviousReferencedObjectCount) - InsertIndexOffset;
			if ( InsertIndexAdvanceCount > 0 )
			{
				// if serializing SuperField added objects to the list of ReferencedObjects, advance the insertion point so that
				// subsequence objects are placed into the list after the SuperField and its dependencies.
				CurrentInsertIndex += InsertIndexAdvanceCount;
			}
		}
	}

public:
	/**
	 * Constructor
	 */
	FExportReferenceSorter()
		: FArchiveUObject(), CurrentInsertIndex(INDEX_NONE), CoreReferencesOffset(INDEX_NONE), bIgnoreFieldReferences(false), CurrentClass(nullptr)
	{
		ArIsObjectReferenceCollector = true;
		ArIsPersistent = true;
		ArIsSaving = true;

		InitializeCoreClasses();
	}

	/**
	 * Verifies that the sorting algorithm is working correctly by checking all objects in the ReferencedObjects array to make sure that their
	 * required objects appear in the list first
	 */
	void VerifySortingAlgorithm()
	{
		FString ErrorString;
		for ( int32 VerifyIndex = CoreReferencesOffset; VerifyIndex < ReferencedObjects.Num(); VerifyIndex++ )
		{
			UObject* Object = ReferencedObjects[VerifyIndex];
			
			// first, make sure that the object's class and archetype appear earlier in the list
			UClass* ObjectClass = Object->GetClass();
			if ( !VerifyDependency(VerifyIndex, ObjectClass, TEXT("Class"), ErrorString) )
			{
				UE_LOG(LogSavePackage, Log, TEXT("%s"), *ErrorString);
			}

			UObject* ObjectArchetype = Object->GetArchetype();
			if ( ObjectArchetype != nullptr && !VerifyDependency(VerifyIndex, ObjectArchetype, TEXT("Archetype"), ErrorString) )
			{
				UE_LOG(LogSavePackage, Log, TEXT("%s"), *ErrorString);
			}

			// UObjectRedirectors are always force-loaded as the loading code needs immediate access to the object pointed to by the Redirector
			UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(Object);
			if ( Redirector != nullptr && Redirector->DestinationObject != nullptr )
			{
				// the Redirector does not force-load the destination object, so we only need its class and archetype.
				UClass* RedirectorDestinationClass = Redirector->DestinationObject->GetClass();
				if ( !VerifyDependency(VerifyIndex, RedirectorDestinationClass, TEXT("Redirector DestinationObject Class"), ErrorString) )
				{
					UE_LOG(LogSavePackage, Log, TEXT("%s"), *ErrorString);
				}

				UObject* RedirectorDestinationArchetype = Redirector->DestinationObject->GetArchetype();
				if ( RedirectorDestinationArchetype != nullptr 
				&& !VerifyDependency(VerifyIndex, RedirectorDestinationArchetype, TEXT("Redirector DestinationObject Archetype"), ErrorString) )
				{
					UE_LOG(LogSavePackage, Log, TEXT("%s"), *ErrorString);
				}
			}
		}
	}

	/**
	 * Clears the list of encountered objects; should be called if you want to re-use this archive.
	 */
	void Clear()
	{
		ReferencedObjects.RemoveAt(CoreReferencesOffset, ReferencedObjects.Num() - CoreReferencesOffset);
	}

	/**
	 * Get the list of new objects which were encountered by this archive; excludes those objects which were passed into the constructor
	 */
	void GetExportList( TArray<UObject*>& out_Exports, UPackage* OuterPackage, bool bIncludeCoreClasses=false )
	{
		PackageToSort = OuterPackage;
		if ( !bIncludeCoreClasses )
		{
			const int32 NumReferencedObjects = ReferencedObjects.Num() - CoreReferencesOffset;
			if ( NumReferencedObjects > 0 )
			{
				int32 OutputIndex = out_Exports.Num();

				out_Exports.AddUninitialized(NumReferencedObjects);
				for ( int32 RefIndex = CoreReferencesOffset; RefIndex < ReferencedObjects.Num(); RefIndex++ )
				{
					out_Exports[OutputIndex++] = ReferencedObjects[RefIndex];
				}
			}
		}
		else
		{
			out_Exports += ReferencedObjects;
		}
	}

	/** 
	 * UObject serialization operator
	 *
	 * @param	Object	an object encountered during serialization of another object
	 *
	 * @return	reference to instance of this class
	 */
	FArchive& operator<<( UObject*& Object )
	{
		// we manually handle class default objects, so ignore those here
		if ( Object != nullptr && !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			if ( ProcessedObjects.Find(Object) == INDEX_NONE )
			{
				// if this object is not a UField, it is an object instance that is referenced through script or defaults (when processing classes) or
				// through an normal object reference (when processing the non-class exports).  Since classes and class default objects
				// are force-loaded (and thus, any objects referenced by the class's script or defaults will be created when the class
				// is force-loaded), we'll need to be sure that the referenced object's class and archetype are inserted into the list
				// of exports before the class, so that when CreateExport is called for this object reference we don't have to seek.
				// Note that in the non-UField case, we don't actually need the object itself to appear before the referencing object/class because it won't
				// be force-loaded (thus we don't need to add the referenced object to the ReferencedObject list)
				if (dynamic_cast<UField*>(Object))
				{
					// when field processing is enabled, ignore any referenced classes since a class's class and CDO are both intrinsic and
					// attempting to deal with them here will only cause problems
					if ( !bIgnoreFieldReferences && !dynamic_cast<UClass*>(Object) )
					{
						if ( CurrentClass == nullptr || Object->GetOuter() != CurrentClass )
						{
							if ( UStruct* StructObject = dynamic_cast<UStruct*>(Object) )
							{
								// if this is a struct/class/function/state, it may have a super that needs to be processed first (Preload force-loads UStruct::SuperField)
								ProcessStruct(StructObject);
							}
							else
							{
								// byte properties that are enum references need their enums loaded first so that config importing works
								{
									UEnum* Enum = nullptr;

									if (UEnumProperty* EnumProp = dynamic_cast<UEnumProperty*>(Object))
									{
										Enum = EnumProp->GetEnum();
									}
									else
									{
								UByteProperty* ByteProp = dynamic_cast<UByteProperty*>(Object);
										if (ByteProp && ByteProp->Enum)
										{
											Enum = ByteProp->Enum;
										}
									}

									if (Enum)
								{
										HandleDependency(Enum, /*bProcessObject =*/true);
								}
								}

								// a normal field - property, enum, const; just insert it into the list and keep going
								ProcessedObjects.Add(Object);
								
								AddReferencedObject(Object, CurrentInsertIndex);
								if ( !SerializedObjects.Contains(Object) )
								{
									SerializedObjects.Add(Object);
									Object->Serialize(*this);
								}
							}
						}
					}
				}
				else
				{
					// since normal references to objects aren't force-loaded, 
					// we do not need to pass true for bProcessObject by default
					// (true would indicate that Object must be inserted into 
					// the sorted export list before the object that contains 
					// this object reference - i.e. the object we're currently
					// serializing)
					// 
					// sometimes (rarely) this is the case though, so we use 
					// ForceLoadObjects to determine if the object we're 
					// serializing would force load Object (if so, it'll come 
					// first in the ExportMap)
					bool const bProcessObject = ForceLoadObjects.Contains(Object);
					HandleDependency(Object, bProcessObject);
				}
			}
		}

		return *this;
	}

	/**
	 * Adds a normal object to the list of sorted exports.  Ensures that any objects which will be force-loaded when this object is created or loaded are inserted into
	 * the list before this object.
	 *
	 * @param	Object	the object to process.
	 */
	void ProcessObject( UObject* Object )
	{
		// we manually handle class default objects, so ignore those here
		if ( Object != nullptr )
		{
			if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
			{
				if ( ProcessedObjects.Find(Object) == INDEX_NONE )
				{
					ProcessedObjects.Add(Object);

					const bool bRecursiveCall = CurrentInsertIndex != INDEX_NONE;
					if ( !bRecursiveCall )
					{
						CurrentInsertIndex = ReferencedObjects.Num();
					}

					// when an object is created (CreateExport), its class and archetype will be force-loaded, so we'll need to make sure that those objects
					// are placed into the list before this object so that when CreateExport calls Preload on these objects, no seeks occur
					// The object's Outer isn't force-loaded, but it will be created before the current object, so we'll need to ensure that its archetype & class
					// are placed into the list before this object.
					HandleDependency(Object->GetClass(), true);
					HandleDependency(Object->GetOuter());
					HandleDependency(Object->GetArchetype(), true);

					// UObjectRedirectors are always force-loaded as the loading code needs immediate access to the object pointed to by the Redirector
					UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(Object);
					if ( Redirector != nullptr && Redirector->DestinationObject != nullptr )
					{
						// the Redirector does not force-load the destination object, so we only need its class and archetype.
						HandleDependency(Redirector->DestinationObject);
					}

					// now we add this object to the list
					AddReferencedObject(Object, CurrentInsertIndex);

					// then serialize the object - any required references encountered during serialization will be inserted into the list before this object, but after this object's
					// class and archetype
					if ( !SerializedObjects.Contains(Object) )
					{
						SerializedObjects.Add(Object);
						Object->Serialize(*this);
					}

					if ( !bRecursiveCall )
					{
						CurrentInsertIndex = INDEX_NONE;
					}
				}
			}
		}
	}

	/**
	 * Adds a UStruct object to the list of sorted exports.  Handles serialization and insertion for any objects that will be force-loaded by this struct (via an explicit call to Preload).
	 *
	 * @param	StructObject	the struct to process
	 */
	void ProcessStruct( UStruct* StructObject )
	{
		if ( StructObject != nullptr )
		{
			if ( ProcessedObjects.Find(StructObject) == INDEX_NONE )
			{
				ProcessedObjects.Add(StructObject);

				const bool bRecursiveCall = CurrentInsertIndex != INDEX_NONE;
				if ( !bRecursiveCall )
				{
					CurrentInsertIndex = ReferencedObjects.Num();
				}

				// this must be done after we've established a CurrentInsertIndex
				HandleDependency(StructObject->GetInheritanceSuper());

				// insert the class/function/state/struct into the list
				AddReferencedObject(StructObject, CurrentInsertIndex);
				if ( !SerializedObjects.Contains(StructObject) )
				{
					const bool bPreviousIgnoreFieldReferences = bIgnoreFieldReferences;

					// first thing to do is collect all actual objects referenced by this struct's script or defaults
					// so we turn off field serialization so that we don't have to worry about handling this struct's fields just yet
					bIgnoreFieldReferences = true;

					// most often, we don't want/need object references getting  
					// recorded as dependencies, but some structs (classes) 
					// require certain non-field objects be prioritized in the 
					// ExportMap earlier (see UClass::GetRequiredPreloadDependencies() 
					// for more details)... this array records/holds those 
					// required sub-objects
					TArray<UObject*> StructForceLoadObjects;

					bool const bIsClassObject = (dynamic_cast<UClass*>(StructObject) != nullptr);
					if (bIsClassObject)
					{
						UClass* AsClass = (UClass*)StructObject;
						AsClass->GetRequiredPreloadDependencies(StructForceLoadObjects);
						check(!StructForceLoadObjects.Num()); //@todoio GetRequiredPreloadDependencies is dead code, remove
					}
					// append rather than replace (in case we're nested in a 
					// recursive call)... adding these to ForceLoadObjects 
					// ensures that any reference to a StructForceLoadObjects
					// object gets recorded in the ExportMap before StructObject
					// (see operator<<, where we utilize ForceLoadObjects)
					ForceLoadObjects.Append(StructForceLoadObjects);
					int32 const ForceLoadCount = StructForceLoadObjects.Num();

					SerializedObjects.Add(StructObject);
					StructObject->Serialize(*this);

					// remove (pop) rather than empty, in case ClassForceLoadObjects
					// had entries from a previous call to this function up that chain
					ForceLoadObjects.RemoveAt(ForceLoadObjects.Num() - ForceLoadCount, ForceLoadCount);

					// at this point, any objects which were referenced through this struct's script or defaults will be in the list of exports, and 
					// the CurrentInsertIndex will have been advanced so that the object processed will be inserted just before this struct in the array
					// (i.e. just after class/archetypes for any objects which were referenced by this struct's script)

					// now re-enable field serialization and process the struct's properties, functions, enums, structs, etc.  They will be inserted into the list
					// just ahead of the struct itself, so that those objects are created first during seek-free loading.
					bIgnoreFieldReferences = false;
					
					// invoke the serialize operator rather than calling Serialize directly so that the object is handled correctly (i.e. if it is a struct, then we should
					// call ProcessStruct, etc. and all this logic is already contained in the serialization operator)
					if (!bIsClassObject)
					{
						// before processing the Children reference, set the CurrentClass to the class which contains this StructObject so that we
						// don't inadvertently serialize other fields of the owning class too early.
						CurrentClass = StructObject->GetOwnerClass();
					}					

					(*this) << (UObject*&)StructObject->Children;
					CurrentClass = nullptr; //-V519

					(*this) << (UObject*&)StructObject->Next;

					bIgnoreFieldReferences = bPreviousIgnoreFieldReferences;
				}

				// Preload will force-load the class default object when called on a UClass object, so make sure that the CDO is always immediately after its class
				// in the export list; we can't resolve this circular reference, but hopefully we the CDO will fit into the same memory block as the class during 
				// seek-free loading.
				UClass* ClassObject = dynamic_cast<UClass*>(StructObject);
				if ( ClassObject != nullptr )
				{
					UObject* CDO = ClassObject->GetDefaultObject();
					ensureMsgf(nullptr != CDO, TEXT("Error: Invalid CDO in class %s"), *GetPathNameSafe(ClassObject));
					if ((ProcessedObjects.Find(CDO) == INDEX_NONE) && (nullptr != CDO))
					{
						ProcessedObjects.Add(CDO);

						if ( !SerializedObjects.Contains(CDO) )
						{
							SerializedObjects.Add(CDO);
							CDO->Serialize(*this);
						}

						int32 ClassIndex = ReferencedObjects.Find(ClassObject);
						check(ClassIndex != INDEX_NONE);

						// we should be the only one adding CDO's to the list, so this assertion is to catch cases where someone else
						// has added the CDO to the list (as it will probably be in the wrong spot).
						check(!ReferencedObjects.Contains(CDO) || CoreClasses.Contains(ClassObject));
						AddReferencedObject(CDO, ClassIndex + 1);
					}
				}

				if ( !bRecursiveCall )
				{
					CurrentInsertIndex = INDEX_NONE;
				}
			}
		}
	}

private:

	/**
	 * The index into the ReferencedObjects array to insert new objects
	 */
	int32 CurrentInsertIndex;

	/**
	 * The index into the ReferencedObjects array for the first object not referenced by one of the core classes
	 */
	int32 CoreReferencesOffset;

	/**
	 * The classes which are pre-added to the array of ReferencedObjects.  Used for resolving a number of circular dependecy issues between
	 * the boot-strap classes.
	 */
	TArray<UClass*> CoreClasses;

	/**
	 * The list of objects that have been evaluated by this archive so far.
	 */
	/*struct FOrderedObjectSet
	{
		TMap<UObject*, int32> ObjectsSet;
		// TArray<UObject*> ObjectsList;

		int32 Add(UObject* Object)
		{
			// int32 Index = ObjectsList.Add(Object);
			int32 Index = ObjectsSet.Num(); // never use the list anyway so no point in even having it
			ObjectsSet.Add(Object, Index);
			return Index;
		}

		inline int32 Find(UObject* Object) const
		{
			const int32 *Index = ObjectsSet.Find(Object);
			if (Index)
			{
				return *Index;
			}
			return INDEX_NONE;
		}
		inline int32 Num() const
		{
			return ObjectsSet.Num();
		}
	};*/
	TArray<UObject*> ProcessedObjects;
	

	/**
	 * The list of objects that have been serialized; used to prevent calling Serialize on an object more than once.
	 */
	TSet<UObject*> SerializedObjects;

	/**
	 * The list of new objects that were encountered by this archive
	 */
	TArray<UObject*> ReferencedObjects;

	/**
	 * Controls whether to process UField objects encountered during serialization of an object.
	 */
	bool bIgnoreFieldReferences;

	/**
	 * The UClass currently being processed.  This is used to prevent serialization of a UStruct's Children member causing other fields of the same class to be processed too early due
	 * to being referenced (directly or indirectly) by that field.  For example, if a class has two functions which both have a struct parameter of a struct type which is declared in the same class,
	 * the struct would be inserted into the list immediately before the first function processed.  The second function would be inserted into the list just before the struct.  At runtime,
	 * the "second" function would be created first, which would end up force-loading the struct.  This would cause an unacceptible seek because the struct appears later in the export list, thus
	 * hasn't been created yet.
	 */
	UClass* CurrentClass;

	/** 
	 * This is a list of objects that would be force loaded by a struct/class 
	 * currently being handled by ProcessStruct() (meaning that they should be
	 * prioritized in the target ExportMap, before the struct).
	 */
	TArray<UObject*> ForceLoadObjects;

	/** Package to constrain checks to */
	UPackage* PackageToSort;
};

/**
 * Helper structure encapsulating functionality to sort a linker's export map to allow seek free
 * loading by creating the exports in the order they are in the export map.
 */
struct FObjectExportSeekFreeSorter
{
	/**
	 * Sorts exports in passed in linker in order to avoid seeking when creating them in order and also
	 * conform the order to an already existing linker if non- NULL.
	 *
	 * @param	Linker				LinkerSave to sort export map
	 * @param	LinkerToConformTo	LinkerLoad to conform LinkerSave to if non- NULL
	 */
	void SortExports( FLinkerSave* Linker, FLinkerLoad* LinkerToConformTo )
	{
		SortArchive.SetCookingTarget(Linker->CookingTarget());

		int32					FirstSortIndex = LinkerToConformTo ? LinkerToConformTo->ExportMap.Num() : 0;
		TMap<UObject*,int32>	OriginalExportIndexes;

		// Populate object to current index map.
		for( int32 ExportIndex=FirstSortIndex; ExportIndex<Linker->ExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& Export = Linker->ExportMap[ExportIndex];
			if( Export.Object )
			{
				// Set the index (key) in the map to the index of this object into the export map.
				OriginalExportIndexes.Add( Export.Object, ExportIndex );
			}
		}

		bool bRetrieveInitialReferences = true;

		// Now we need to sort the export list according to the order in which objects will be loaded.  For the sake of simplicity, 
		// process all classes first so they appear in the list first (along with any objects those classes will force-load) 
		for( int32 ExportIndex=FirstSortIndex; ExportIndex<Linker->ExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& Export = Linker->ExportMap[ExportIndex];
			if( UClass* ExportObjectClass = dynamic_cast<UClass*>(Export.Object) )
			{
				SortArchive.Clear();
				SortArchive.ProcessStruct(ExportObjectClass);
#if EXPORT_SORTING_DETAILED_LOGGING
				TArray<UObject*> ReferencedObjects;
				SortArchive.GetExportList(ReferencedObjects, Linker->LinkerRoot, bRetrieveInitialReferences);

				UE_LOG(LogSavePackage, Log, TEXT("Referenced objects for (%i) %s in %s"), ExportIndex, *Export.Object->GetFullName(), *Linker->LinkerRoot->GetName());
				for ( int32 RefIndex = 0; RefIndex < ReferencedObjects.Num(); RefIndex++ )
				{
					UE_LOG(LogSavePackage, Log, TEXT("\t%i) %s"), RefIndex, *ReferencedObjects[RefIndex]->GetFullName());
				}
				if ( ReferencedObjects.Num() > 1 )
				{
					// insert a blank line to make the output more readable
					UE_LOG(LogSavePackage, Log, TEXT(""));
				}

				SortedExports += ReferencedObjects;
#else
				SortArchive.GetExportList(SortedExports, Linker->LinkerRoot, bRetrieveInitialReferences);
#endif
				bRetrieveInitialReferences = false;
			}

		}

#if EXPORT_SORTING_DETAILED_LOGGING
		UE_LOG(LogSavePackage, Log, TEXT("*************   Processed %i classes out of %i possible exports for package %s.  Beginning second pass...   *************"), SortedExports.Num(), Linker->ExportMap.Num() - FirstSortIndex, *Linker->LinkerRoot->GetName());
#endif

		// All UClasses, CDOs, functions, properties, etc. are now in the list - process the remaining objects now
		for ( int32 ExportIndex = FirstSortIndex; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& Export = Linker->ExportMap[ExportIndex];
			if ( Export.Object )
			{
				SortArchive.Clear();
				SortArchive.ProcessObject(Export.Object);
#if EXPORT_SORTING_DETAILED_LOGGING
				TArray<UObject*> ReferencedObjects;
				SortArchive.GetExportList(ReferencedObjects, Linker->LinkerRoot, bRetrieveInitialReferences);

				UE_LOG(LogSavePackage, Log, TEXT("Referenced objects for (%i) %s in %s"), ExportIndex, *Export.Object->GetFullName(), *Linker->LinkerRoot->GetName());
				for ( int32 RefIndex = 0; RefIndex < ReferencedObjects.Num(); RefIndex++ )
				{
					UE_LOG(LogSavePackage, Log, TEXT("\t%i) %s"), RefIndex, *ReferencedObjects[RefIndex]->GetFullName());
				}
				if ( ReferencedObjects.Num() > 1 )
				{
					// insert a blank line to make the output more readable
					UE_LOG(LogSavePackage, Log, TEXT(""));
				}

				SortedExports += ReferencedObjects;
#else
				SortArchive.GetExportList(SortedExports, Linker->LinkerRoot, bRetrieveInitialReferences);
#endif
				bRetrieveInitialReferences = false;
			}
		}

#if EXPORT_SORTING_DETAILED_LOGGING
		SortArchive.VerifySortingAlgorithm();
#endif
		// Back up existing export map and empty it so we can repopulate it in a sorted fashion.
		TArray<FObjectExport> OldExportMap = Linker->ExportMap;
		Linker->ExportMap.Empty( OldExportMap.Num() );

		// Add exports that can't be re-jiggled as they are part of the exports of the to be
		// conformed to Linker.
		for( int32 ExportIndex=0; ExportIndex<FirstSortIndex; ExportIndex++ )
		{
			Linker->ExportMap.Add( OldExportMap[ExportIndex] );
		}

		// Create new export map from sorted exports.
		for( int32 ObjectIndex=0; ObjectIndex<SortedExports.Num(); ObjectIndex++ )
		{
			// See whether this object was part of the to be sortable exports map...
			UObject* Object		= SortedExports[ObjectIndex];
			int32* ExportIndexPtr	= OriginalExportIndexes.Find( Object );
			if( ExportIndexPtr )
			{
				// And add it if it has been.
				Linker->ExportMap.Add( OldExportMap[*ExportIndexPtr] );
			}
		}

		// Manually add any new NULL exports last as they won't be in the SortedExportsObjects list. 
		// A NULL Export.Object can occur if you are e.g. saving an object in the game that is 
		// OBJECTMARK_NotForClient.
		for( int32 ExportIndex=FirstSortIndex; ExportIndex<OldExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& Export = OldExportMap[ExportIndex];
			if( Export.Object == nullptr )
			{
				Linker->ExportMap.Add( Export );
			}
		}
	}

private:
	/**
	 * Archive for sorting an objects references according to the order in which they'd be loaded.
	 */
	FExportReferenceSorter SortArchive;

	/** Array of regular objects encountered by CollectExportsInOrderOfUse					*/
	TArray<UObject*>	SortedExports;
};

// helper class for clarification, encapsulation, and elimination of duplicate code
struct FPackageExportTagger
{
	UObject*		Base;
	EObjectFlags	TopLevelFlags;
	UObject*		Outer;
	const class ITargetPlatform* TargetPlatform;

	FPackageExportTagger(UObject* CurrentBase, EObjectFlags CurrentFlags, UObject* InOuter, const class ITargetPlatform* InTargetPlatform)
	:	Base(CurrentBase)
	,	TopLevelFlags(CurrentFlags)
	,	Outer(InOuter)
	,	TargetPlatform(InTargetPlatform)
	{}

	void TagPackageExports( FArchiveSaveTagExports& ExportTagger, bool bRoutePresave )
	{
		// Route PreSave on Base and serialize it for export tagging.
		if( Base )
		{
			if ( bRoutePresave )
			{
#if ENABLE_TAGEXPORTS_CLASS_PRESAVE_TIMES
				auto& TimingInfo = SavePackageStats::ClassPreSaveTimes.FindOrAdd(Base->GetClass()->GetFName());
				TimingInfo.Value++;
				FScopedDurationTimer SerializeTimer(TimingInfo.Key);
#endif
				Base->PreSave(TargetPlatform);
			}

			ExportTagger.ProcessBaseObject(Base);
		}
		TArray<UObject *> ObjectsInOuter;
		{
			COOK_STAT(FScopedDurationTimer SerializeTimer(SavePackageStats::TagPackageExportsGetObjectsWithOuter));
			GetObjectsWithOuter(Outer, ObjectsInOuter);
		}
		// Serialize objects to tag them as OBJECTMARK_TagExp.
		for( int32 Index = 0; Index < ObjectsInOuter.Num(); Index++ )
		{
			UObject* Obj = ObjectsInOuter[Index];
			if( Obj->HasAnyFlags(TopLevelFlags) && Obj->IsIn(Outer) )
			{
				ExportTagger.ProcessBaseObject(Obj);
			}
		}
		if ( bRoutePresave )
		{
			// Route PreSave.
			{
				TArray<UObject*> TagExpObjects;
				{
					COOK_STAT(FScopedDurationTimer SerializeTimer(SavePackageStats::TagPackageExportsGetObjectsWithMarks));
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
				}
				for(int32 Index = 0; Index < TagExpObjects.Num(); Index++)
				{
					UObject* Obj = TagExpObjects[Index];
#if ENABLE_TAGEXPORTS_CLASS_PRESAVE_TIMES
					auto& TimingInfo = SavePackageStats::ClassPreSaveTimes.FindOrAdd(Obj->GetClass()->GetFName());
					TimingInfo.Value++;
					FScopedDurationTimer SerializeTimer(TimingInfo.Key);
#endif
					check(Obj->HasAnyMarks(OBJECTMARK_TagExp));
					//@warning: Objects created from within PreSave will NOT have PreSave called on them!!!
					Obj->PreSave(TargetPlatform);
				}
			}
		}
	}
};


/** checks whether it is valid to conform NewPackage to OldPackage
 * i.e, that there are no incompatible changes between the two
 * @warning: this function needs to load objects from the old package to do the verification
 * it's very important that it cleans up after itself to avoid conflicts with e.g. script compilation
 * @param NewPackage - the new package being saved
 * @param OldLinker - linker of the old package to conform against
 * @param Error - log device to send any errors
 * @return whether NewPackage can be conformed
 */
static bool ValidateConformCompatibility(UPackage* NewPackage, FLinkerLoad* OldLinker, FOutputDevice* Error)
{
	// various assumptions made about Core and its contents prevent loading a version mapped to a different name from working correctly
	// Core script typically doesn't have any replication related definitions in it anyway
	if (NewPackage->GetFName() == NAME_CoreUObject || NewPackage->GetFName() == GLongCoreUObjectPackageName)
	{
		return true;
	}

	// save the RF_TagGarbageTemp flag for all objects so our use of it doesn't clobber anything
	TMap<UObject*, uint8> ObjectFlagMap;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		ObjectFlagMap.Add(*It, (It->GetFlags() & RF_TagGarbageTemp) ? 1 : 0);
	}

	// this is needed to successfully find intrinsic classes/properties
	OldLinker->LoadFlags |= LOAD_NoWarn | LOAD_Quiet | LOAD_FindIfFail;

	// unfortunately, to get at the classes and their properties we will also need to load the default objects
	// but the remapped package won't be bound to its native instance, so we need to manually copy the constructors
	// so that classes with their own Serialize() implementations are loaded correctly
	BeginLoad();
	for (int32 i = 0; i < OldLinker->ExportMap.Num(); i++)
	{
		UClass* NewClass = (UClass*)StaticFindObjectFast(UClass::StaticClass(), NewPackage, OldLinker->ExportMap[i].ObjectName, true, false);
		UClass* OldClass = static_cast<UClass*>(OldLinker->Create(UClass::StaticClass(), OldLinker->ExportMap[i].ObjectName, OldLinker->LinkerRoot, LOAD_None, false));
		if (OldClass != nullptr && NewClass != nullptr && OldClass->IsNative() && NewClass->IsNative())
		{
			OldClass->ClassConstructor = NewClass->ClassConstructor;
			OldClass->ClassVTableHelperCtorCaller = NewClass->ClassVTableHelperCtorCaller;
			OldClass->ClassAddReferencedObjects = NewClass->ClassAddReferencedObjects;
		}
	}
	EndLoad();

	bool bHadCompatibilityErrors = false;
	// check for illegal change of networking flags on class fields
	for (int32 i = 0; i < OldLinker->ExportMap.Num(); i++)
	{
		if (OldLinker->GetExportClassName(i) == NAME_Class)
		{
			// load the object so we can analyze it
			BeginLoad();
			UClass* OldClass = static_cast<UClass*>(OldLinker->Create(UClass::StaticClass(), OldLinker->ExportMap[i].ObjectName, OldLinker->LinkerRoot, LOAD_None, false));
			EndLoad();
			if (OldClass != nullptr)
			{
				UClass* NewClass = FindObjectFast<UClass>(NewPackage, OldClass->GetFName(), true, false);
				if (NewClass != nullptr)
				{
					for (TFieldIterator<UField> OldFieldIt(OldClass,EFieldIteratorFlags::ExcludeSuper); OldFieldIt; ++OldFieldIt)
					{
						for (TFieldIterator<UField> NewFieldIt(NewClass,EFieldIteratorFlags::ExcludeSuper); NewFieldIt; ++NewFieldIt)
						{
							if (OldFieldIt->GetFName() == NewFieldIt->GetFName())
							{
								UProperty* OldProp = dynamic_cast<UProperty*>(*OldFieldIt);
								UProperty* NewProp = dynamic_cast<UProperty*>(*NewFieldIt);
								if (OldProp != nullptr && NewProp != nullptr)
								{
									if ((OldProp->PropertyFlags & CPF_Net) != (NewProp->PropertyFlags & CPF_Net))
									{
										Error->Logf(ELogVerbosity::Error, TEXT("Network flag mismatch for property %s"), *NewProp->GetPathName());
										bHadCompatibilityErrors = true;
									}
								}
								else
								{
									UFunction* OldFunc = dynamic_cast<UFunction*>(*OldFieldIt);
									UFunction* NewFunc = dynamic_cast<UFunction*>(*NewFieldIt);
									if (OldFunc != nullptr && NewFunc != nullptr)
									{
										if ((OldFunc->FunctionFlags & (FUNC_Net | FUNC_NetServer | FUNC_NetClient)) != (NewFunc->FunctionFlags & (FUNC_Net | FUNC_NetServer | FUNC_NetClient)))
										{
											Error->Logf(ELogVerbosity::Error, TEXT("Network flag mismatch for function %s"), *NewFunc->GetPathName());
											bHadCompatibilityErrors = true;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	// delete all of the newly created objects from the old package by marking everything else and deleting all unmarked objects
	for (TObjectIterator<UObject> It; It; ++It)
	{
		It->SetFlags(RF_TagGarbageTemp);
	}
	for (int32 i = 0; i < OldLinker->ExportMap.Num(); i++)
	{
		if (OldLinker->ExportMap[i].Object != nullptr)
		{
			OldLinker->ExportMap[i].Object->ClearFlags(RF_TagGarbageTemp);
		}
	}
	CollectGarbage(RF_TagGarbageTemp, true);

	// restore RF_TagGarbageTemp flag value
	for (TMap<UObject*, uint8>::TIterator It(ObjectFlagMap); It; ++It)
	{
		UObject* Obj = It.Key();
		check(Obj->IsValidLowLevel()); // if this crashes we deleted something we shouldn't have
		if (It.Value())
		{
			Obj->SetFlags(RF_TagGarbageTemp);
		}
		else
		{
			Obj->ClearFlags(RF_TagGarbageTemp);
		}
	}
	// verify that we cleaned up after ourselves
	for (int32 i = 0; i < OldLinker->ExportMap.Num(); i++)
	{
		checkf(OldLinker->ExportMap[i].Object == nullptr, TEXT("Conform validation code failed to clean up after itself! Surviving object: %s"), *OldLinker->ExportMap[i].Object->GetPathName());
	}

	// finally, abort if there were any errors
	return !bHadCompatibilityErrors;
}

EObjectMark UPackage::GetExcludedObjectMarksForTargetPlatform( const class ITargetPlatform* TargetPlatform, const bool bIsCooking )
{
	EObjectMark ObjectMarks = OBJECTMARK_NOMARKS;

	if( TargetPlatform && bIsCooking )
	{
		if (!TargetPlatform->HasEditorOnlyData())
		{
			ObjectMarks = (EObjectMark)(ObjectMarks | OBJECTMARK_EditorOnly);
		}
		
		const bool bIsServerOnly = TargetPlatform->IsServerOnly();
		const bool bIsClientOnly = TargetPlatform->IsClientOnly();

		if( bIsServerOnly )
		{
			ObjectMarks = (EObjectMark)(ObjectMarks | OBJECTMARK_NotForServer);
		}
		else if( bIsClientOnly )
		{
			ObjectMarks = (EObjectMark)(ObjectMarks | OBJECTMARK_NotForClient);
		}
	}

	return ObjectMarks;
}

#if WITH_EDITOR
/**
 * Helper function to sort export objects by fully qualified names.
 */
bool ExportObjectSorter(const UObject& Lhs, const UObject& Rhs)
{
	// Check names first.
	if (Lhs.GetFName() < Rhs.GetFName())
	{
		return true;
	}

	if (Lhs.GetFName() > Rhs.GetFName())
	{
		return false;
	}

	// Names equal, compare class names.
	if (Lhs.GetClass()->GetFName() < Rhs.GetClass()->GetFName())
	{
		return true;
	}

	if (Lhs.GetClass()->GetFName() > Rhs.GetClass()->GetFName())
	{
		return false;
	}

	// Compare by outers if they exist.
	if (Lhs.GetOuter() && Rhs.GetOuter())
	{
		return Lhs.GetOuter()->GetFName() < Rhs.GetOuter()->GetFName();
	}

	if (Lhs.GetOuter())
	{
		return true;
	}

	return false;
}

/**
* Helper equality comparator for export objects. Compares by names, class names and outer names.
*/
bool ExportEqualityComparator(UObject* Lhs, UObject* Rhs)
{
	check(Lhs && Rhs);
	return Lhs->GetOuter() == Rhs->GetOuter()
		&& Lhs->GetClass() == Rhs->GetClass()
		&& Lhs->GetFName() == Rhs->GetFName();
}

/**
 * Remove OBJECTMARK_TagExp from duplicated objects.
 */
TMap<UObject*, UObject*> UnmarkExportTagFromDuplicates()
{
	TMap<UObject*, UObject*> RedirectDuplicatesToOriginals;
	TArray<UObject*> Objects;
	GetObjectsWithAnyMarks(Objects, OBJECTMARK_TagExp);

	Objects.Sort(ExportObjectSorter);

	int32 LastUniqueObjectIndex = 0;
	for (int32 CurrentObjectIndex = 1; CurrentObjectIndex < Objects.Num(); ++CurrentObjectIndex)
	{
		UObject* LastUniqueObject = Objects[LastUniqueObjectIndex];
		UObject* CurrentObject = Objects[CurrentObjectIndex];

		// Check if duplicates with different pointers
		if (LastUniqueObject != CurrentObject
			// but matching names
			&& ExportEqualityComparator(LastUniqueObject, CurrentObject))
		{
			// Don't export duplicates.
			CurrentObject->UnMark(OBJECTMARK_TagExp);
			RedirectDuplicatesToOriginals.Add(CurrentObject, LastUniqueObject);
		}
		else
		{
			LastUniqueObjectIndex = CurrentObjectIndex;
		}
	}

	return RedirectDuplicatesToOriginals;
}

COREUOBJECT_API extern bool GOutputCookingWarnings;

class FDiffSerializeArchive : public FLargeMemoryWriter
{
private:
	FArchive *TestArchive;
	TArray<FName> DebugDataStack;
	bool bDisable;
public:


	FDiffSerializeArchive(const FName& InFilename, FArchive *InTestArchive) : FLargeMemoryWriter(0, true, InFilename), TestArchive(InTestArchive)
	{ 
		ArDebugSerializationFlags = DSF_IgnoreDiff;
		bDisable = false;
	}

	virtual void Serialize(void* InData, int64 Num) override
	{
		TArray<int8> TestMemory;

		if (TestArchive)
		{
			int64 Pos = FMath::Min(FLargeMemoryWriter::Tell(), TestArchive->TotalSize());
			TestArchive->Seek(Pos);
			TestMemory.AddZeroed(Num);
			int64 ReadSize = FMath::Min(Num, TestArchive->TotalSize() - Pos);
			TestArchive->Serialize((void*)TestMemory.GetData(), ReadSize);

			if (!(ArDebugSerializationFlags&DSF_IgnoreDiff) && (!bDisable))
			{
				if (FMemory::Memcmp((void*)TestMemory.GetData(), InData, Num) != 0)
				{
					// get the calls debug callstack and 
					FString DebugStackString;
					for (const auto& DebugData : DebugDataStack)
					{
						DebugStackString += DebugData.ToString();
						DebugStackString += TEXT("->");
					}

					UE_LOG(LogSavePackage, Warning, TEXT("Diff cooked package archive recognized a difference %lld Filename %s, stack %s "), Pos, *GetArchiveName(), *DebugStackString);

					// only log one message per archive, from this point the entire package is probably messed up
					bDisable = true;
				}
			}
		}
		FLargeMemoryWriter::Serialize(InData, Num);
	}

	virtual void PushDebugDataString(const FName& DebugData) override
	{
		DebugDataStack.Add(DebugData);
	}
	virtual void PopDebugDataString() override
	{
		DebugDataStack.Pop();
	}

	virtual FString GetArchiveName() const override
	{
		return TestArchive->GetArchiveName();
	}
};

#endif


struct FEDLCookChecker
{
	bool bIsActive;
	TMultiMap<FString, FString> ImportToImportingPackage;
	TSet<FString> Exports;
	TMultiMap<FString, FString> NodePrereqs;

	FEDLCookChecker()
		: bIsActive(false)
	{
	}

	void Reset()
	{
		ImportToImportingPackage.Empty();
		Exports.Empty();
		NodePrereqs.Empty();
		bIsActive = false;
	}

	void AddImport(UObject* Import, UPackage* ImportingPackage)
	{
		if (bIsActive)
		{
			if (!Import->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn))
			{
				ImportToImportingPackage.Add(Import->GetFullName(), ImportingPackage->GetPathName());
			}
		}
	}
	void AddExport(UObject* Export)
	{
		if (bIsActive)
		{
			Exports.Add(Export->GetFullName());
			AddArc(Export, false, Export, true); // every export must be created before it can be serialize...these arcs are implicit and not listed in any table.
		}
	}

	void AddArc(UObject* DepObject, bool bDepIsSerialize, UObject* Export, bool bExportIsSerialize)
	{
		if (bIsActive)
		{
			NodePrereqs.Add(NodeName(Export, bExportIsSerialize), NodeName(DepObject, bDepIsSerialize));
		}
	}

	bool CheckForCyclesInner(TSet<FString>& Visited, TSet<FString>& Stack, const FString& Visit, FString& FailNode)
	{
		bool bResult = false;
		if (Stack.Contains(Visit))
		{
			FailNode = Visit;
			bResult = true;
		}
		else
		{
			bool bWasAlreadyTested = false;
			Visited.Add(Visit, &bWasAlreadyTested);
			if (!bWasAlreadyTested)
			{
				Stack.Add(Visit);
				for (auto It = NodePrereqs.CreateConstKeyIterator(Visit); !bResult && It; ++It)
				{
					bResult = CheckForCyclesInner(Visited, Stack, It.Value(), FailNode);
				}
				Stack.Remove(Visit);
			}
		}
		UE_CLOG(bResult && Stack.Contains(FailNode), LogSavePackage, Error, TEXT("Cycle Node %s"), *Visit);
		return bResult;
	}

	void Verify()
	{
		if (bIsActive && Exports.Num())
		{
			double StartTime = FPlatformTime::Seconds();
			
 			// imports to things that are not exports...
 			for (auto& Pair : ImportToImportingPackage)
 			{
 				if (!Exports.Contains(Pair.Key))
 				{
 					UE_LOG(LogSavePackage, Warning, TEXT("%s imported %s, but it was never saved as an export."), *Pair.Value, *Pair.Key);
 				}
 			}
			// cycles in the dep graph
			TSet<FString> Visited;
			TSet<FString> Stack;
			bool bHadCycle = false;
			for (FString& Export : Exports)
			{
				FString FailNode;
				if (CheckForCyclesInner(Visited, Stack, Prefix(true) + Export, FailNode))
				{
					UE_LOG(LogSavePackage, Error, TEXT("----- %s contained a cycle (listed above)."), *FailNode);
					bHadCycle = true;
				}
			}
			if (bHadCycle)
			{
				UE_LOG(LogSavePackage, Fatal, TEXT("EDL dep graph contained a cycle (see errors, above). This is fatal at runtime so it is fatal at cook time."));
			}
			UE_LOG(LogSavePackage, Display, TEXT("Took %fs to verify the EDL loading graph."), float(FPlatformTime::Seconds() - StartTime));
		}
		Reset();
	}

	static FString Prefix(bool bDepIsSerialize)
	{
		return FString(bDepIsSerialize ? TEXT("Serialize:") : TEXT("Create:"));
	}

	static FString NodeName(UObject* DepObject, bool bDepIsSerialize)
	{
		return Prefix(bDepIsSerialize) + DepObject->GetFullName();
	}
};

static FEDLCookChecker GEDLCookChecker;

void StartSavingEDLCookInfoForVerification()
{
	GEDLCookChecker.Reset();
	GEDLCookChecker.bIsActive = IsEventDrivenLoaderEnabledInCookedBuilds();
}

void VerifyEDLCookInfo()
{
	GEDLCookChecker.Verify();
}


extern FGCCSyncObject GGarbageCollectionGuardCritical;

FSavePackageResultStruct UPackage::Save(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename,
	FOutputDevice* Error, FLinkerLoad* Conform, bool bForceByteSwapping, bool bWarnOfLongFilename, uint32 SaveFlags, 
	const class ITargetPlatform* TargetPlatform, const FDateTime&  FinalTimeStamp, bool bSlowTask)
{
	COOK_STAT(FScopedDurationTimer FuncSaveTimer(SavePackageStats::SavePackageTimeSec));
	COOK_STAT(SavePackageStats::NumPackagesSaved++);

#if WITH_EDITOR
	TMap<UObject*, UObject*> ReplacedImportOuters;
#endif //WITH_EDITOR

	if (FPlatformProperties::HasEditorOnlyData())
	{
		if (GIsSavingPackage)
		{
			ensureMsgf(false, TEXT("Recursive SavePackage() is not supported"));
			return ESavePackageResult::Error;
		}

		// Sanity checks
		check(InOuter);
		check(Filename);

		const bool bIsCooking = TargetPlatform != nullptr;
#if WITH_EDITORONLY_DATA
		if (bIsCooking && (!(SaveFlags & ESaveFlags::SAVE_KeepEditorOnlyCookedPackages)))
		{
			static struct FCanSkipEditorReferencedPackagesWhenCooking
			{
				bool bCanSkipEditorReferencedPackagesWhenCooking;
				FCanSkipEditorReferencedPackagesWhenCooking()
					: bCanSkipEditorReferencedPackagesWhenCooking(true)
				{
					GConfig->GetBool(TEXT("Core.System"), TEXT("CanSkipEditorReferencedPackagesWhenCooking"), bCanSkipEditorReferencedPackagesWhenCooking, GEngineIni);
				}
				FORCEINLINE operator bool() const { return bCanSkipEditorReferencedPackagesWhenCooking; }
			} CanSkipEditorReferencedPackagesWhenCooking;

			// Don't save packages marked as editor-only.
			if (CanSkipEditorReferencedPackagesWhenCooking && InOuter->IsLoadedByEditorPropertiesOnly())
			{
				UE_CLOG(!(SaveFlags & SAVE_NoError), LogSavePackage, Display, TEXT("Package loaded by editor-only properties: %s. Package will not be saved."), *InOuter->GetName());
				return ESavePackageResult::ReferencedOnlyByEditorOnlyData;
			}
			else if (InOuter->HasAnyPackageFlags(PKG_EditorOnly))
			{
				UE_CLOG(!(SaveFlags & SAVE_NoError), LogSavePackage, Display, TEXT("Package marked as editor-only: %s. Package will not be saved."), *InOuter->GetName());
				return ESavePackageResult::ReferencedOnlyByEditorOnlyData;
			}
		}
#endif
		// if we are cooking we should be doing it in the editor
		// otherwise some other assumptions are bad
		check(!bIsCooking || WITH_EDITOR);
#if WITH_EDITOR
		if (!bIsCooking)
		{
			// Attempt to create a backup of this package before it is saved, if applicable
			if (FCoreUObjectDelegates::AutoPackageBackupDelegate.IsBound())
			{
				FCoreUObjectDelegates::AutoPackageBackupDelegate.Execute(*InOuter);
			}
		}

#endif	// #if WITH_EDITOR

		// do any path replacements on the source DestFile
		const FString NewPath = FString(Filename);

		// point to the new version of the path
		Filename = *NewPath;

		// We need to fulfill all pending streaming and async loading requests to then allow us to lock the global IO manager. 
		// The latter implies flushing all file handles which is a pre-requisite of saving a package. The code basically needs 
		// to be sure that we are not reading from a file that is about to be overwritten and that there is no way we might 
		// start reading from the file till we are done overwriting it.
		FlushAsyncLoading();

		(*GFlushStreamingFunc)();

		uint32 Time = 0; CLOCK_CYCLES(Time);
		int64 TotalPackageSizeUncompressed = 0;
		
		// Make sure package is fully loaded before saving. 
		if (!Base && !InOuter->IsFullyLoaded())
		{
			if (!(SaveFlags & SAVE_NoError))
			{
				// We cannot save packages that aren't fully loaded as it would clobber existing not loaded content.
				FText ErrorText;
				if (InOuter->ContainsMap())
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Name"), FText::FromString(NewPath));
					ErrorText = FText::Format(NSLOCTEXT("SavePackage", "CannotSaveMapPartiallyLoaded", "Map '{Name}' cannot be saved as it has only been partially loaded"), Arguments);
				}
				else
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Name"), FText::FromString(NewPath));
					ErrorText = FText::Format(NSLOCTEXT("SavePackage", "CannotSaveAssetPartiallyLoaded", "Asset '{Name}' cannot be saved as it has only been partially loaded"), Arguments);
				}
				Error->Logf(ELogVerbosity::Warning, TEXT("%s"), *ErrorText.ToString());
			}
			return ESavePackageResult::Error;
		}

		// Make sure package is allowed to be saved.
		if (!TargetPlatform && FCoreUObjectDelegates::IsPackageOKToSaveDelegate.IsBound())
		{
			bool bIsOKToSave = FCoreUObjectDelegates::IsPackageOKToSaveDelegate.Execute(InOuter, Filename, Error);
			if (!bIsOKToSave)
			{
				if (!(SaveFlags & SAVE_NoError))
				{
					FText ErrorText;
					if (InOuter->ContainsMap())
					{
						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("Name"), FText::FromString(NewPath));
						ErrorText = FText::Format(NSLOCTEXT("SavePackage", "MapSaveNotAllowed", "Map '{Name}' is not allowed to save (see log for reason)"), Arguments);
					}
					else
					{
						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("Name"), FText::FromString(NewPath));
						ErrorText = FText::Format(NSLOCTEXT("SavePackage", "AssetSaveNotAllowed", "Asset '{Name}' is not allowed to save (see log for reason)"), Arguments);
					}
					Error->Logf(ELogVerbosity::Warning, TEXT("%s"), *ErrorText.ToString());
				}
				return ESavePackageResult::Error;
			}
		}

		// if we're conforming, validate that the packages are compatible
		if (Conform != nullptr && !ValidateConformCompatibility(InOuter, Conform, Error))
		{
			if (!(SaveFlags & SAVE_NoError))
			{
				FText ErrorText;
				if (InOuter->ContainsMap())
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Name"), FText::FromString(NewPath));
					ErrorText = FText::Format(NSLOCTEXT("SavePackage", "CannotSaveMapConformIncompatibility", "Conformed Map '{Name}' cannot be saved as it is incompatible with the original"), Arguments);
				}
				else
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Name"), FText::FromString(NewPath));
					ErrorText = FText::Format(NSLOCTEXT("SavePackage", "CannotSaveAssetConformIncompatibility", "Conformed Asset '{Name}' cannot be saved as it is incompatible with the original"), Arguments);
				}
				Error->Logf(ELogVerbosity::Error, TEXT("%s"), *ErrorText.ToString());
			}
			return ESavePackageResult::Error;
		}

		const bool FilterEditorOnly = InOuter->HasAnyPackageFlags(PKG_FilterEditorOnly);

		// Route PreSaveRoot to allow e.g. the world to attach components for the persistent level.
		bool bCleanupIsRequired = false;
		if (Base)
		{
			bCleanupIsRequired = Base->PreSaveRoot(Filename);
		}

		const FString BaseFilename = FPaths::GetBaseFilename(Filename);
		// Make temp file. CreateTempFilename guarantees unique, non-existing filename.
		// The temp file will be saved in the game save folder to not have to deal with potentially too long paths.
		// Since the temp filename may include a 32 character GUID as well, limit the user prefix to 32 characters.
		FString TempFilename;
		TempFilename = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), *BaseFilename.Left(32));

		// Init.
		FString CleanFilename = FPaths::GetCleanFilename(Filename);

		FFormatNamedArguments Args;
		Args.Add(TEXT("CleanFilename"), FText::FromString(CleanFilename));

		FText StatusMessage = FText::Format(NSLOCTEXT("Core", "SavingFile", "Saving file: {CleanFilename}..."), Args);

		const int32 TotalSaveSteps = 33;
		FScopedSlowTask SlowTask(TotalSaveSteps, StatusMessage, bSlowTask);
		SlowTask.MakeDialog(SaveFlags & SAVE_FromAutosave ? true : false);

		SlowTask.EnterProgressFrame();

		bool Success = true;
		bool bRequestStub = false;
		{
			COOK_STAT(FScopedDurationTimer SaveTimer(SavePackageStats::ResetLoadersForSaveTimeSec));
			ResetLoadersForSave(InOuter, Filename);
		}
		SlowTask.EnterProgressFrame();

		// Untag all objects and names.
		UnMarkAllObjects();

		TArray<UObject*> CachedObjects;

		// structure to track what every export needs to import (native only)
		TMap<UObject*, TArray<UObject*> > NativeObjectDependencies;

		// Size of serialized out package in bytes. This is before compression.
		int32 PackageSize = INDEX_NONE;
		{
			FScopeSavePackageState ScopeSavePackageState; // allocates the save package state and deletes it when this goes out of scope

			uint32 ComparisonFlags = PPF_DeepCompareInstances;

			// Export objects (tags them as OBJECTMARK_TagExp).
			FArchiveSaveTagExports ExportTaggerArchive( InOuter );
			ExportTaggerArchive.SetPortFlags( ComparisonFlags );
			ExportTaggerArchive.SetCookingTarget(TargetPlatform);
			
			check( ExportTaggerArchive.IsCooking() == !!TargetPlatform );
			check( ExportTaggerArchive.IsCooking() == bIsCooking );

			// Tag exports and route presave.
			FPackageExportTagger PackageExportTagger(Base, TopLevelFlags, InOuter, TargetPlatform);
			{
				COOK_STAT(FScopedDurationTimer SaveTimer(SavePackageStats::TagPackageExportsPresaveTimeSec));
				PackageExportTagger.TagPackageExports(ExportTaggerArchive, true);
				ExportTaggerArchive.SetFilterEditorOnly(FilterEditorOnly);
			}
		
#if USE_STABLE_LOCALIZATION_KEYS
			if (GIsEditor)
			{
				// We need to ensure that we have a package localization namespace as the package loading will need it
				// We need to do this before entering the GIsSavingPackage block as it may change the package meta-data
				TextNamespaceUtil::EnsurePackageNamespace(InOuter);
			}
#endif // USE_STABLE_LOCALIZATION_KEYS

			{
				check(!IsGarbageCollecting());
				// set GIsSavingPackage here as it is now illegal to create any new object references; they potentially wouldn't be saved correctly								
				struct FScopedSavingFlag
				{
					FScopedSavingFlag() 
					{ 
						// We need the same lock as GC so that no StaticFindObject can happen in parallel to saveing a package
						GGarbageCollectionGuardCritical.GCLock();
						GIsSavingPackage = true; 
					}
					~FScopedSavingFlag() 
					{ 
						GIsSavingPackage = false; 
						GGarbageCollectionGuardCritical.GCUnlock();
					}
				} IsSavingFlag;

			
				{
					COOK_STAT(FScopedDurationTimer SaveTimer(SavePackageStats::TagPackageExportsTimeSec));
					// Clear OBJECTMARK_TagExp again as we need to redo tagging below.
					UnMarkAllObjects((EObjectMark)(OBJECTMARK_TagExp|OBJECTMARK_EditorOnly));
			
					// We need to serialize objects yet again to tag objects that were created by PreSave as OBJECTMARK_TagExp.
					PackageExportTagger.TagPackageExports( ExportTaggerArchive, false );
				}

				// Kick off any Precaching required for the target platform to save these objects
				// only need to do this if we are cooking a different platform then the one which is currently running
				// TODO: if save package is canceled then call ClearCache on each object

#if WITH_EDITOR
				if ( bIsCooking )
				{
					TArray<UObject*> TagExpObjects;
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
					for ( int Index =0; Index < TagExpObjects.Num(); ++Index)
					{
						UObject *ExpObject = TagExpObjects[Index];
						if ( ExpObject->HasAnyMarks( OBJECTMARK_TagExp ) )
						{
							ExpObject->BeginCacheForCookedPlatformData( TargetPlatform );
							CachedObjects.Add( ExpObject );
						}
					}
				}
#endif

				SlowTask.EnterProgressFrame();

				// structure to track what every export needs to import
				TMap<UObject*, TArray<UObject*> > ObjectDependencies;

				// and a structure to track non-redirector references
				TSet<UObject*> DependenciesReferencedByNonRedirectors;
		
				/** If true, we are going to save to disk async to save time. */
				bool bSaveAsync = !!(SaveFlags & SAVE_Async);

				bool bSaveUnversioned = !!(SaveFlags & SAVE_Unversioned);

				FLinkerSave* Linker = nullptr;
				
#if WITH_EDITOR
				FString DiffCookedPackagesPath;
				// if we are cooking and we have diff cooked packages on the commandline then do some special stuff

				if ((!!TargetPlatform) && FParse::Value(FCommandLine::Get(), TEXT("DiffCookedPackages="), DiffCookedPackagesPath))
				{
					FString TestArchiveFilename = Filename;
					// TestArchiveFilename.ReplaceInline(TEXT("Cooked"), TEXT("CookedDiff"));
					DiffCookedPackagesPath.ReplaceInline(TEXT("\\"), TEXT("/"));
					FString CookedPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() + TEXT("Cooked/"));
					CookedPath.ReplaceInline(TEXT("\\"), TEXT("/"));
					TestArchiveFilename.ReplaceInline(*CookedPath, *DiffCookedPackagesPath);
					
					FArchive* TestArchive = IFileManager::Get().CreateFileReader(*TestArchiveFilename); 
					FArchive* Saver = new FDiffSerializeArchive(InOuter->FileName, TestArchive);
					Linker = new FLinkerSave(InOuter, Saver, bForceByteSwapping);
				}
				else 
#endif
				if (bSaveAsync)
				{
					// Allocate the linker with a memory writer, forcing byte swapping if wanted.
					Linker = new FLinkerSave(InOuter, bForceByteSwapping, bSaveUnversioned);
				}
				else
				{
					// Allocate the linker, forcing byte swapping if wanted.
					Linker = new FLinkerSave(InOuter, *TempFilename, bForceByteSwapping, bSaveUnversioned);
				}

#if WITH_EDITOR
				if (!!TargetPlatform)
				{
					Linker->SetDebugSerializationFlags(DSF_EnableCookerWarnings | Linker->GetDebugSerializationFlags());
				}
#endif

				// Use the custom versions we had previously gleaned from the export tag pass
				Linker->Summary.SetCustomVersionContainer(ExportTaggerArchive.GetCustomVersions());

				Linker->SetPortFlags(ComparisonFlags);
				Linker->SetFilterEditorOnly( FilterEditorOnly );
				Linker->SetCookingTarget(TargetPlatform);

				// Make sure the package has the same version as the linker
				InOuter->LinkerPackageVersion = Linker->UE4Ver();
				InOuter->LinkerLicenseeVersion = Linker->LicenseeUE4Ver();
				InOuter->LinkerCustomVersion = Linker->GetCustomVersions();

				if (EndSavingIfCancelled(Linker, TempFilename)) 
				{ 
					return ESavePackageResult::Canceled; 
				}
				SlowTask.EnterProgressFrame();
			
				// keep a list of objects that would normally have gone into the dependency map, but since they are from cross-level dependencies, won't be found in the import map
				TArray<UObject*> DependenciesToIgnore;

				// When cooking, strip export objects that 
				//	are both not for client and not for server by default
				//	are for client if target is client only
				//	are for server if target is server only
				if (Linker->IsCooking())
				{
					TArray<UObject*> TagExpObjects;
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);

					const EObjectMark ExcludedObjectMarks = UPackage::GetExcludedObjectMarksForTargetPlatform(TargetPlatform, Linker->IsCooking());
					if (Linker->IsCooking() && ExcludedObjectMarks != OBJECTMARK_NOMARKS)
					{
						// Make sure that nothing is in the export table that should have been filtered out
						for (UObject* ObjExport : TagExpObjects)
						{
							if (!ensureMsgf(!ObjExport->HasAnyMarks(ExcludedObjectMarks), TEXT("Object %s is marked for export, but has excluded mark!"), *ObjExport->GetPathName()))
							{
								ObjExport->UnMark(OBJECTMARK_TagExp);
							}
						}
						GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
					}

					// Exports got filtered out already if they're not for this platform
					if (TagExpObjects.Num() == 0)
					{
						UE_CLOG(!(SaveFlags & SAVE_NoError), LogSavePackage, Display, TEXT("No exports found (or all exports are editor-only) for %s. Package will not be saved."), *BaseFilename);
						return ESavePackageResult::ContainsEditorOnlyData;
					}

#if WITH_EDITOR
					if (bIsCooking && TargetPlatform)
					{
						if (const IBlueprintNativeCodeGenCore* Coordinator = IBlueprintNativeCodeGenCore::Get())
						{
							EReplacementResult ReplacmentResult = Coordinator->IsTargetedForReplacement(InOuter, Coordinator->GetNativizationOptionsForPlatform(TargetPlatform));
							if (ReplacmentResult == EReplacementResult::ReplaceCompletely)
							{
								if (IsEventDrivenLoaderEnabledInCookedBuilds() && TargetPlatform)
								{
									// the package isn't actually in the export map, but that is ok, we add it as export anyway for error checking
									GEDLCookChecker.AddExport(InOuter);

									for (UObject* ObjExport : TagExpObjects)
									{
										// Register exports, these will exist at runtime because they are compiled in
										GEDLCookChecker.AddExport(ObjExport);
									}
								}

								UE_LOG(LogSavePackage, Display, TEXT("Package %s contains assets, that were converted into native code. Package will not be saved."), *InOuter->GetName());
								return ESavePackageResult::ReplaceCompletely;
							}
							else if (ReplacmentResult == EReplacementResult::GenerateStub)
							{
								bRequestStub = true;
							}
						}
					}
#endif
				}

				// Import objects & names.
				TSet<UPackage*> PrestreamPackages;
				{
					TArray<UObject*> TagExpObjects;
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
					for(int32 Index = 0; Index < TagExpObjects.Num(); Index++)
					{
						UObject* Obj = TagExpObjects[Index];
						check(Obj->HasAnyMarks(OBJECTMARK_TagExp));

						// Build list.
						FArchiveSaveTagImports ImportTagger(Linker);
						ImportTagger.SetPortFlags(ComparisonFlags);
						ImportTagger.SetFilterEditorOnly(FilterEditorOnly);

						UClass* Class = Obj->GetClass();

						if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
						{
							Class->SerializeDefaultObject(Obj, ImportTagger);
						}
						else
						{
							Obj->Serialize( ImportTagger );
						}

						ImportTagger << Class;

						UObject* Template = Obj->GetArchetype();
						if (Template)
						{
							// If we're not cooking for the event driven loader, exclude the CDO
							if (Template != Class->GetDefaultObject() || (IsEventDrivenLoaderEnabledInCookedBuilds() && TargetPlatform))
							{
								ImportTagger << Template;
							}
						}

						if (IsEventDrivenLoaderEnabledInCookedBuilds() && TargetPlatform)
						{
							TArray<UObject*> Deps;
							Obj->GetPreloadDependencies(Deps);
							for (UObject* Dep : Deps)
							{
								// We assume nothing in coreuobject ever loads assets in a constructor
								if (Dep && Dep->GetOutermost()->GetFName() != GLongCoreUObjectPackageName)
								{
									// We want to tag these as imports, but not as dependencies
									FArchiveSaveTagImports::FScopeIgnoreDependencies IgnoreDependencies(ImportTagger);
									ImportTagger << Dep;
								}
							}
							static const IConsoleVariable* ProcessPrestreamingRequests = IConsoleManager::Get().FindConsoleVariable(TEXT("s.ProcessPrestreamingRequests"));
							if (ProcessPrestreamingRequests->GetInt())
							{
								Deps.Reset();
								Obj->GetPrestreamPackages(Deps);
								for (UObject* Dep : Deps)
								{
									if (Dep)
									{
										UPackage* Pkg = Dep->GetOutermost();
										if (!Pkg->HasAnyPackageFlags(PKG_CompiledIn) && Obj->HasAnyMarks(OBJECTMARK_TagExp))
										{
											PrestreamPackages.Add(Pkg);
										}
									}
								}
							}
						}

						if( Obj->IsIn(GetTransientPackage()) )
						{
							UE_LOG(LogSavePackage, Fatal, TEXT("%s"), *FString::Printf( TEXT("Transient object imported: %s"), *Obj->GetFullName() ) );
						}

						// add the list of dependencies to the dependency map
						ObjectDependencies.Add(Obj, ImportTagger.Dependencies);
						NativeObjectDependencies.Add(Obj, ImportTagger.NativeDependencies);

						if (Obj->GetClass() != UObjectRedirector::StaticClass())
						{
							for ( auto DepIt = ImportTagger.Dependencies.CreateConstIterator(); DepIt; ++DepIt )
							{
								UObject* DependencyObject = *DepIt;
								if ( DependencyObject )
								{
									DependenciesReferencedByNonRedirectors.Add(DependencyObject);
								}
							}
						}
					}
				}
				if (PrestreamPackages.Num())
				{
					TSet<UPackage*> KeptPrestreamPackages;
					for (UPackage* Pkg : PrestreamPackages)
					{
						if (!Pkg->HasAnyMarks(OBJECTMARK_TagImp))
						{
							Pkg->Mark(OBJECTMARK_TagImp);
							KeptPrestreamPackages.Add(Pkg);
						}
					}
					Exchange(PrestreamPackages, KeptPrestreamPackages);
				}

#if WITH_EDITOR
				// Remove TagExp from duplicate objects.
				TMap<UObject*, UObject*> DuplicateRedirects = UnmarkExportTagFromDuplicates();
#endif // WITH_EDITOR

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				bool bCanCacheGatheredText = false;
				if ( !(Linker->Summary.PackageFlags & PKG_FilterEditorOnly) )
				{
					// Gathers from the given package
					EPropertyLocalizationGathererResultFlags GatherableTextResultFlags = EPropertyLocalizationGathererResultFlags::Empty;
					FPropertyLocalizationDataGatherer(Linker->GatherableTextDataMap, InOuter, GatherableTextResultFlags);

					// We can only cache packages that don't contain script data, as script data is very volatile and can only be safely gathered after it's been compiled (which happens automatically on asset load)
					bCanCacheGatheredText = !(GatherableTextResultFlags & EPropertyLocalizationGathererResultFlags::HasScript);
				}

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				TArray<UObject*> PrivateObjects;
				TArray<UObject*> ObjectsInOtherMaps;
				TArray<UObject*> LevelObjects;

				// Tag the names for all relevant object, classes, and packages.
				{
					TArray<UObject*> TagExpImpObjects;
					GetObjectsWithAnyMarks(TagExpImpObjects, EObjectMark(OBJECTMARK_TagExp|OBJECTMARK_TagImp));
					for(int32 Index = 0; Index < TagExpImpObjects.Num(); Index++)
					{
						UObject* Obj = TagExpImpObjects[Index];
						check(Obj->HasAnyMarks(EObjectMark(OBJECTMARK_TagExp|OBJECTMARK_TagImp)));

						SavePackageState->MarkNameAsReferenced(Obj->GetFName());
#if WITH_EDITOR
						SavePackageState->AddReplacementsNames(Obj, TargetPlatform, bIsCooking);
#endif //WITH_EDITOR
						if( Obj->GetOuter() )
						{
							SavePackageState->MarkNameAsReferenced(Obj->GetOuter()->GetFName());
						}

						if( Obj->HasAnyMarks(OBJECTMARK_TagImp) )
						{
							SavePackageState->MarkNameAsReferenced(Obj->GetClass()->GetFName());
							check(Obj->GetClass()->GetOuter());
							SavePackageState->MarkNameAsReferenced(Obj->GetClass()->GetOuter()->GetFName());
						
							// if a private object was marked by the cooker, it will be in memory on load, and will be found. helps with some objects
							// from a package in a package being moved into Startup_int.xxx, but not all
							// Imagine this:
							// Package P:
							//   - A (private)
							//   - B (public, references A)
							//   - C (public, references A)
							// Map M:
							//   - MObj (references B)
							// Startup Package S:
							//   - SObj (references C)
							// When Startup is cooked, it will pull in C and A. When M is cooked, it will pull in B, but not A, because
							// A was already marked by the cooker. M.xxx now has a private import to A, which is normally illegal, hence
							// the OBJECTMARK_MarkedByCooker check below
							UPackage* ObjPackage = Obj->GetOutermost();
							if (PrestreamPackages.Contains(ObjPackage))
							{
								SavePackageState->MarkNameAsReferenced(PrestreamPackageClassName);
								// These are not errors
								UE_LOG(LogSavePackage, Display, TEXT("Prestreaming package %s "), *ObjPackage->GetPathName()); //-V595
								continue;
							}


							if( !Obj->HasAnyFlags(RF_Public) && !Obj->HasAnyFlags(RF_Transient))
							{
								if (!IsEventDrivenLoaderEnabledInCookedBuilds() || !TargetPlatform || !ObjPackage->HasAnyPackageFlags(PKG_CompiledIn))
								{
									PrivateObjects.Add(Obj);
								}
							}

							// See whether the object we are referencing is in another map package.
							if( ObjPackage && ObjPackage->ContainsMap() )
							{
								if ( ObjPackage != Obj && Obj->GetFName() != NAME_PersistentLevel && Obj->GetClass()->GetFName() != WorldClassName )
								{
									ObjectsInOtherMaps.Add(Obj);

									if ( DependenciesReferencedByNonRedirectors.Contains(Obj) )
									{
										UE_LOG(LogSavePackage, Warning, TEXT( "Obj in another map: %s"), *Obj->GetFullName() );

										if (!(SaveFlags & SAVE_NoError))
										{
											Error->Logf(ELogVerbosity::Warning, TEXT("%s"), *FText::Format( NSLOCTEXT( "Core", "SavePackageObjInAnotherMap", "Object '{0}' is in another map" ), FText::FromString( *Obj->GetFullName() ) ).ToString() );
										}
									}
								}
								else
								{
									LevelObjects.Add(Obj);
								}
							}

						}
					}
				}

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				if ( LevelObjects.Num() > 0 && ObjectsInOtherMaps.Num() == 0 )
				{
					ObjectsInOtherMaps = LevelObjects;
				}

				// It is allowed for redirectors to reference objects in other maps.
				// Form the list of objects that erroneously reference another map.
				TArray<UObject*> IllegalObjectsInOtherMaps;
				for ( auto ObjIt = ObjectsInOtherMaps.CreateConstIterator(); ObjIt; ++ObjIt )
				{
					if ( DependenciesReferencedByNonRedirectors.Contains(*ObjIt) )
					{
						IllegalObjectsInOtherMaps.Add(*ObjIt);
					}
				}

				// The graph is linked to objects in a different map package!
				if (IllegalObjectsInOtherMaps.Num() )
				{
					UObject* MostLikelyCulprit = nullptr;
					const UProperty* PropertyRef = nullptr;

					// construct a string containing up to the first 5 objects problem objects
					FString ObjectNames;
					int32 MaxNamesToDisplay = 5;
					bool DisplayIsLimited = true;

					if (IllegalObjectsInOtherMaps.Num() < MaxNamesToDisplay)
					{
						MaxNamesToDisplay = IllegalObjectsInOtherMaps.Num();
						DisplayIsLimited = false;
					}

					for (int32 Idx = 0; Idx < MaxNamesToDisplay; Idx++)
					{
						ObjectNames += IllegalObjectsInOtherMaps[Idx]->GetName() + TEXT("\n");
					}
					
					// if there are more than 5 items we indicated this by adding "..." at the end of the list
					if (DisplayIsLimited)
					{
						ObjectNames += TEXT("...\n");
					}

					Args.Empty();
					Args.Add( TEXT("FileName"), FText::FromString( Filename ) );
					Args.Add( TEXT("ObjectNames"), FText::FromString( ObjectNames ) );
					const FText Message = FText::Format( NSLOCTEXT("Core", "LinkedToObjectsInOtherMap_FindCulpritQ", "Can't save {FileName}: Graph is linked to object(s) in external map.\nExternal Object(s):\n{ObjectNames}  \nTry to find the chain of references to that object (may take some time)?"), Args );

					FString CulpritString = TEXT( "Unknown" );
					if (FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes)
					{
						FindMostLikelyCulprit(IllegalObjectsInOtherMaps, MostLikelyCulprit, PropertyRef);
						if (MostLikelyCulprit != nullptr && PropertyRef != nullptr)
						{
							CulpritString = FString::Printf(TEXT("%s (%s)"), *MostLikelyCulprit->GetFullName(), *PropertyRef->GetName());
						}
						else if (MostLikelyCulprit != nullptr)
						{
							CulpritString = FString::Printf(TEXT("%s (Unknown property)"), *MostLikelyCulprit->GetFullName());
						}
					}

					// Free the file handle and delete the temporary file
					Linker->Detach();
					IFileManager::Get().Delete( *TempFilename );
					if (!(SaveFlags & SAVE_NoError))
					{
						Error->Logf(ELogVerbosity::Warning, TEXT("Can't save %s: Graph is linked to object %s in external map"), Filename, *CulpritString);
					}
					return ESavePackageResult::Error;
				}

				// The graph is linked to private objects!
				if (PrivateObjects.Num())
				{
					UObject* MostLikelyCulprit = nullptr;
					const UProperty* PropertyRef = nullptr;
					
					// construct a string containing up to the first 5 objects problem objects
					FString ObjectNames;
					int32 MaxNamesToDisplay = 5;
					bool DisplayIsLimited = true;

					if (PrivateObjects.Num() < MaxNamesToDisplay)
					{
						MaxNamesToDisplay = PrivateObjects.Num();
						DisplayIsLimited = false;
					}

					for (int32 Idx = 0; Idx < MaxNamesToDisplay; Idx++)
					{
						ObjectNames += PrivateObjects[Idx]->GetName() + TEXT("\n");
					}

					// if there are more than 5 items we indicated this by adding "..." at the end of the list
					if (DisplayIsLimited)
					{
						ObjectNames += TEXT("...\n");
					}

					Args.Empty();
					Args.Add( TEXT("FileName"), FText::FromString( Filename ) );
					Args.Add( TEXT("ObjectNames"), FText::FromString( ObjectNames ) );
					const FText Message = FText::Format( NSLOCTEXT("Core", "LinkedToPrivateObjectsInOtherPackage_FindCulpritQ", "Can't save {FileName}: Graph is linked to private object(s) in an external package.\nExternal Object(s):\n{ObjectNames}  \nTry to find the chain of references to that object (may take some time)?"), Args );

					FString CulpritString = TEXT( "Unknown" );
					if (FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes)
					{
						FindMostLikelyCulprit(PrivateObjects, MostLikelyCulprit, PropertyRef);
						CulpritString = FString::Printf(TEXT("%s (%s)"),
							(MostLikelyCulprit != nullptr) ? *MostLikelyCulprit->GetFullName() : TEXT("(unknown culprit)"),
							(PropertyRef != nullptr) ? *PropertyRef->GetName() : TEXT("unknown property ref"));
					}

					// free the file handle and delete the temporary file
					Linker->Detach();
					IFileManager::Get().Delete(*TempFilename);
					if (!(SaveFlags & SAVE_NoError))
					{
						Error->Logf(ELogVerbosity::Warning, TEXT("Can't save %s: Graph is linked to external private object %s"), Filename, *CulpritString);
					}
					return ESavePackageResult::Error;
				}

				// Write fixed-length file summary to overwrite later.
				if( Conform )
				{
					// Conform to previous generation of file.
					UE_LOG(LogSavePackage, Log,  TEXT("Conformal save, relative to: %s, Generation %i"), *Conform->Filename, Conform->Summary.Generations.Num()+1 );
					Linker->Summary.Guid        = Conform->Summary.Guid;
					Linker->Summary.Generations = Conform->Summary.Generations;
				}
				else if (SaveFlags & SAVE_KeepGUID)
				{
					// First generation file, keep existing GUID
					Linker->Summary.Guid = InOuter->Guid;
					Linker->Summary.Generations = TArray<FGenerationInfo>();
				}
				else
				{
					// First generation file.
					Linker->Summary.Guid = FGuid::NewGuid();
					Linker->Summary.Generations = TArray<FGenerationInfo>();

					// make sure the UPackage's copy of the GUID is up to date
					InOuter->Guid = Linker->Summary.Guid;
				}
				new(Linker->Summary.Generations)FGenerationInfo(0, 0);

				*Linker << Linker->Summary;
				int32 OffsetAfterPackageFileSummary = Linker->Tell();
		
				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();


				// Build NameMap.
				Linker->Summary.NameOffset = Linker->Tell();
				SavePackageState->UpdateLinkerWithMarkedNames(Linker);

#if WITH_EDITOR
				if ( GOutputCookingWarnings )
				{
					// check the name list for uniqueobjectnamefor cooking
					static FName NAME_UniqueObjectNameForCooking(TEXT("UniqueObjectNameForCooking"));

					for (const auto& NameInUse : Linker->NameMap)
					{
						if (NameInUse.GetComparisonIndex() == NAME_UniqueObjectNameForCooking.GetComparisonIndex())
						{
							//UObject *Object = FindObject<UObject>( ANY_PACKAGE, *NameInUse.ToString());

							// error
							// check(Object);
							UE_LOG(LogSavePackage, Warning, TEXT("Saving object into cooked package %s which was created at cook time, Object Name %s"), Filename, *NameInUse.ToString());
						}
					}
				}
#endif

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Sort names.
				FObjectNameSortHelper NameSortHelper;
				NameSortHelper.SortNames( Linker, Conform );

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Save names.
				{
#if WITH_EDITOR
					FArchive::FScopeSetDebugSerializationFlags S(*Linker, DSF_IgnoreDiff, true);
#endif
					Linker->Summary.NameCount = Linker->NameMap.Num();
					for (int32 i = 0; i < Linker->NameMap.Num(); i++)
					{
						*Linker << *const_cast<FNameEntry*>(Linker->NameMap[i].GetDisplayNameEntry());
						Linker->NameIndices.Add(Linker->NameMap[i], i);
					}
				}
				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				Linker->Summary.GatherableTextDataOffset = 0;
				Linker->Summary.GatherableTextDataCount = 0;
				if ( !(Linker->Summary.PackageFlags & PKG_FilterEditorOnly) && bCanCacheGatheredText )
				{
					// The Editor version is used as part of the check to see if a package is too old to use the gather cache, so we always have to add it if we have gathered loc for this asset
					Linker->UsingCustomVersion(FEditorObjectVersion::GUID);

					Linker->Summary.GatherableTextDataOffset = Linker->Tell();

					// Save gatherable text data.
					Linker->Summary.GatherableTextDataCount = Linker->GatherableTextDataMap.Num();
					for (FGatherableTextData& GatherableTextData : Linker->GatherableTextDataMap)
					{
						*Linker << GatherableTextData;
					}
				}

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Build ImportMap.
				{
					TArray<UObject*> TagImpObjects;

					const EObjectMark ExcludedObjectMarks = UPackage::GetExcludedObjectMarksForTargetPlatform(TargetPlatform, Linker->IsCooking());
					GetObjectsWithAnyMarks(TagImpObjects, OBJECTMARK_TagImp);

					if (Linker->IsCooking() && ExcludedObjectMarks != OBJECTMARK_NOMARKS)
					{
						// Make sure that nothing is in the import table that should have been filtered out
						for (UObject* ObjImport : TagImpObjects)
						{
							if (!ensureMsgf(!ObjImport->HasAnyMarks(ExcludedObjectMarks), TEXT("Object %s is marked for import, but has excluded mark!"), *ObjImport->GetPathName()))
							{
								ObjImport->UnMark(OBJECTMARK_TagImp);
							}
						}
						GetObjectsWithAnyMarks(TagImpObjects, OBJECTMARK_TagImp);
					}

					for(int32 Index = 0; Index < TagImpObjects.Num(); Index++)
					{
						UObject* Obj = TagImpObjects[Index];
						check(Obj->HasAnyMarks(OBJECTMARK_TagImp));
						UClass* ObjClass = Obj->GetClass();
#if WITH_EDITOR
						FName ReplacedName = NAME_None;
						if (bIsCooking && TargetPlatform)
						{
							if (const IBlueprintNativeCodeGenCore* Coordinator = IBlueprintNativeCodeGenCore::Get())
							{
								const FCompilerNativizationOptions& NativizationOptions = Coordinator->GetNativizationOptionsForPlatform(TargetPlatform);
								if (UClass* ReplacedClass = Coordinator->FindReplacedClassForObject(Obj, NativizationOptions))
								{
									ObjClass = ReplacedClass;
								}
								if (UObject* ReplacedOuter = Coordinator->FindReplacedNameAndOuter(Obj, /*out*/ReplacedName, NativizationOptions))
								{
									ReplacedImportOuters.Add(Obj, ReplacedOuter);
								}
							}
						}
#endif //WITH_EDITOR
						FObjectImport* LocObjectImport = new(Linker->ImportMap)FObjectImport(Obj, ObjClass);

						if (PrestreamPackages.Contains((UPackage*)Obj))
						{
							LocObjectImport->ClassName = PrestreamPackageClassName;
						}
#if WITH_EDITOR
						if (ReplacedName != NAME_None)
						{
							LocObjectImport->ObjectName = ReplacedName;
						}
#endif //WITH_EDITOR
					}
				}


				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// sort and conform imports
				FObjectImportSortHelper ImportSortHelper;
				ImportSortHelper.SortImports( Linker, Conform );
				Linker->Summary.ImportCount = Linker->ImportMap.Num();

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				
				// Build ExportMap.
				{
					TArray<UObject*> TagExpObjects;
					GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
					for(int32 Index = 0; Index < TagExpObjects.Num(); Index++)
					{
						UObject* Obj = TagExpObjects[Index];
						check(Obj->HasAnyMarks(OBJECTMARK_TagExp));
						new( Linker->ExportMap )FObjectExport( Obj );
					}
				}

#if WITH_EDITOR
				if (GOutputCookingWarnings)
				{
					// check the name list for uniqueobjectnamefor cooking
					static FName NAME_UniqueObjectNameForCooking(TEXT("UniqueObjectNameForCooking"));

					for (const auto& Export : Linker->ExportMap)
					{
						const auto& NameInUse = Export.ObjectName;
						if (NameInUse.GetComparisonIndex() == NAME_UniqueObjectNameForCooking.GetComparisonIndex())
						{
							UObject* Outer = Export.Object->GetOuter();
							UE_LOG(LogSavePackage, Warning, TEXT(" into cooked package %s which was created at cook time, Object Name %s, Full Path %s, Class %s, Outer %s, Outer class %s"), Filename, *NameInUse.ToString(), *Export.Object->GetFullName(), *Export.Object->GetClass()->GetName(), Outer ? *Outer->GetName() : TEXT("None"), Outer ? *Outer->GetClass()->GetName() : TEXT("None") );
						}
					}
				}
#endif

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Sort exports alphabetically and conform the export table (if necessary)
				FObjectExportSortHelper ExportSortHelper;
				ExportSortHelper.SortExports( Linker, Conform );
				
				// Sort exports for seek-free loading.
				{
					COOK_STAT(FScopedDurationTimer SaveTimer(SavePackageStats::SortExportsSeekfreeInnerTimeSec));
					FObjectExportSeekFreeSorter SeekFreeSorter;
					SeekFreeSorter.SortExports( Linker, Conform );
				}

				Linker->Summary.ExportCount = Linker->ExportMap.Num();

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Pre-size depends map.
				Linker->DependsMap.AddZeroed( Linker->ExportMap.Num() );

				// track import and export object linker index
				TMap<UObject*, FPackageIndex> ImportToIndexMap;
				TMap<UObject*, FPackageIndex> ExportToIndexMap;
				for (int32 ImpIndex = 0; ImpIndex < Linker->ImportMap.Num(); ImpIndex++)
				{
					ImportToIndexMap.Add(Linker->ImportMap[ImpIndex].XObject, FPackageIndex::FromImport(ImpIndex));
				}

				for (int32 ExpIndex = 0; ExpIndex < Linker->ExportMap.Num(); ExpIndex++)
				{
					ExportToIndexMap.Add(Linker->ExportMap[ExpIndex].Object, FPackageIndex::FromExport(ExpIndex));
				}

				// go back over the (now sorted) exports and fill out the DependsMap
				for (int32 ExpIndex = 0; ExpIndex < Linker->ExportMap.Num(); ExpIndex++)
				{
					UObject* Object = Linker->ExportMap[ExpIndex].Object;
					// sorting while conforming can create NULL export map entries, so skip those depends map
					if (Object == nullptr)
					{
						UE_LOG(LogSavePackage, Warning, TEXT("Object is missing for an export, unable to save dependency map. Most likely this is caused my conforming against a package that is missing this object. See log for more info"));
						if (!(SaveFlags & SAVE_NoError))
						{
							Error->Logf(ELogVerbosity::Warning, TEXT("%s"), *FText::Format( NSLOCTEXT( "Core", "SavePackageObjectIsMissingExport", "Object is missing for an export, unable to save dependency map for asset '{0}'. Most likely this is caused my conforming against a asset that is missing this object. See log for more info" ), FText::FromString( FString( Filename ) ) ).ToString() );
						}
						continue;
					}

					// add a dependency map entry also
					TArray<FPackageIndex>& DependIndices = Linker->DependsMap[ExpIndex];
					// find all the objects needed by this export
					TArray<UObject*>* SrcDepends = ObjectDependencies.Find(Object);
					checkf(SrcDepends,TEXT("Couldn't find dependency map for %s"), *Object->GetFullName());

					// go through each object and...
					for (int32 DependIndex = 0; DependIndex < SrcDepends->Num(); DependIndex++)
					{
						UObject* DependentObject = (*SrcDepends)[DependIndex];

						FPackageIndex DependencyIndex;

						// if the dependency is in the same pacakge, we need to save an index into our ExportMap
						if (DependentObject->GetOutermost() == Linker->LinkerRoot)
						{
							// ... find the associated ExportIndex
							DependencyIndex = ExportToIndexMap.FindRef(DependentObject);
						}
						// otherwise we need to save an index into the ImportMap
						else
						{
							// ... find the associated ImportIndex
							DependencyIndex = ImportToIndexMap.FindRef(DependentObject);
						}
					
#if WITH_EDITOR
						// If we still didn't find index, maybe it was a duplicate export which got removed.
						// Check if we have a redirect to original.
						if (DependencyIndex.IsNull() && DuplicateRedirects.Contains(DependentObject))
						{
							UObject** const RedirectObj = DuplicateRedirects.Find(DependentObject);
							if (RedirectObj)
							{
								DependencyIndex = ExportToIndexMap.FindRef(*RedirectObj);
							}
						}
#endif
						// if we didn't find it (FindRef returns 0 on failure, which is good in this case), then we are in trouble, something went wrong somewhere
						checkf(!DependencyIndex.IsNull(), TEXT("Failed to find dependency index for %s (%s)"), *DependentObject->GetFullName(), *Object->GetFullName());

						// add the import as an import for this export
						DependIndices.Add(DependencyIndex);
					}
				}


				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Set linker reverse mappings.
				// also set netplay required data for any UPackages in the export map
				for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
				{
					UObject* Object = Linker->ExportMap[i].Object;
					if( Object )
					{
						Linker->ObjectIndicesMap.Add(Object, FPackageIndex::FromExport(i));

						UPackage* Package = dynamic_cast<UPackage*>(Object);
						if (Package != nullptr)
						{
							Linker->ExportMap[i].PackageFlags = Package->GetPackageFlags();
							if (!Package->HasAnyPackageFlags(PKG_ServerSideOnly))
							{
								Linker->ExportMap[i].PackageGuid = Package->GetGuid();
							}
						}
					}
				}

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// If this is a map package, make sure there is a world or level in the export map.
				if ( InOuter->ContainsMap() )
				{
					bool bContainsMap = false;
					for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
					{
						UObject* Object = Linker->ExportMap[i].Object;
						if ( Object )
						{
							const FString ExportClassName = Object->GetClass()->GetName();
							if( ExportClassName == TEXT("World") || ExportClassName == TEXT("Level") )
							{
								bContainsMap = true;
								break;
							}
						}
					}
					if (!bContainsMap)
					{
						ensureMsgf(false, TEXT("Attempting to save a map package '%s' that does not contain a map object."), *InOuter->GetName());
						UE_LOG(LogSavePackage, Error, TEXT("Attempting to save a map package '%s' that does not contain a map object."), *InOuter->GetName());

						if (!(SaveFlags & SAVE_NoError))
						{
							Error->Logf(ELogVerbosity::Warning, TEXT("%s"), *FText::Format( NSLOCTEXT( "Core", "SavePackageNoMap", "Attempting to save a map asset '{0}' that does not contain a map object" ), FText::FromString( FString( Filename ) ) ).ToString() );
						}
						Success = false;
					}
				}


				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				for( int32 i=0; i<Linker->ImportMap.Num(); i++ )
				{
					UObject* Object = Linker->ImportMap[i].XObject;
					if( Object != nullptr )
					{
						const FPackageIndex PackageIndex = FPackageIndex::FromImport(i);
						//ensure(!Linker->ObjectIndicesMap.Contains(Object)); // this ensure will fail
						Linker->ObjectIndicesMap.Add(Object, PackageIndex);
					}
					else
					{
						// the only reason we should ever have a NULL object in the import is when this package is being conformed against another package, and the new
						// version of the package no longer has this import
						checkf(Conform != NULL, TEXT("NULL XObject for import %i - Object: %s Class: %s"), i, *Linker->ImportMap[i].ObjectName.ToString(), *Linker->ImportMap[i].ClassName.ToString());
					}
				}
				if (IsEventDrivenLoaderEnabledInCookedBuilds() && TargetPlatform)
				{
					GEDLCookChecker.AddExport(InOuter); // the package isn't actually in the export map, but that is ok, we add it as export anyway for error checking
					for (int32 i = 0; i < Linker->ImportMap.Num(); i++)
					{
						UObject* Object = Linker->ImportMap[i].XObject;
						if (Object != nullptr)
						{
							GEDLCookChecker.AddImport(Object, InOuter);
						}
					}
				}

				// Convert the searchable names map from UObject to packageindex
				for (const TPair<const UObject *, TArray<FName>>& SearchableNamePair : Linker->SearchableNamesObjectMap)
				{
					const FPackageIndex PackageIndex = Linker->MapObject(SearchableNamePair.Key);

					// This should always be in the imports already
					if (ensure(!PackageIndex.IsNull()))
					{
						Linker->SearchableNamesMap.FindOrAdd(PackageIndex) = SearchableNamePair.Value;
					}
				}
				// Clear temporary map
				Linker->SearchableNamesObjectMap.Empty();

				SlowTask.EnterProgressFrame();

				// Find components referenced by exports.

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Save dummy import map, overwritten later.
				Linker->Summary.ImportOffset = Linker->Tell();
				for( int32 i=0; i<Linker->ImportMap.Num(); i++ )
				{
					FObjectImport& Import = Linker->ImportMap[ i ];
					*Linker << Import;
				}
				int32 OffsetAfterImportMap = Linker->Tell();


				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Save dummy export map, overwritten later.
				Linker->Summary.ExportOffset = Linker->Tell();
				for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
				{
					FObjectExport& Export = Linker->ExportMap[ i ];
					*Linker << Export;
				}
				int32 OffsetAfterExportMap = Linker->Tell();


				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				if (Linker->IsCooking())
				{
					//@todo optimization, this should just be stripped entirely from cooked packages
					TArray<FPackageIndex> Depends; // empty array
					Linker->Summary.DependsOffset = Linker->Tell();
					for (int32 i = 0; i < Linker->ExportMap.Num(); i++)
					{
						*Linker << Depends;
					}
				}
				else
				{
					// save depends map (no need for later patching)
					check(Linker->DependsMap.Num() == Linker->ExportMap.Num());
					Linker->Summary.DependsOffset = Linker->Tell();
					for (int32 i = 0; i < Linker->ExportMap.Num(); i++)
					{
						TArray<FPackageIndex>& Depends = Linker->DependsMap[i];
						*Linker << Depends;
					}
				}


				if (EndSavingIfCancelled(Linker, TempFilename)) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Only save string asset and searchable name map if saving for editor
				if (!(Linker->Summary.PackageFlags & PKG_FilterEditorOnly))
				{
					// Save soft package references
					Linker->Summary.SoftPackageReferencesOffset = Linker->Tell();
					Linker->Summary.SoftPackageReferencesCount = Linker->SoftPackageReferenceList.Num();
					{
#if WITH_EDITOR
						FArchive::FScopeSetDebugSerializationFlags S(*Linker, DSF_IgnoreDiff, true);
#endif
						for (FName& SoftPackageName : Linker->SoftPackageReferenceList)
						{
							*Linker << SoftPackageName;
						}
					}

					// Save searchable names map
					Linker->Summary.SearchableNamesOffset = Linker->Tell();
					Linker->SerializeSearchableNamesMap(*Linker);
				}
				else
				{
					Linker->Summary.SoftPackageReferencesCount = 0;
					Linker->Summary.SoftPackageReferencesOffset = 0;
					Linker->Summary.SearchableNamesOffset = 0;
				}

				// Save thumbnails
				UPackage::SaveThumbnails( InOuter, Linker );

				// Save asset registry data so the editor can search for information about assets in this package
				UPackage::SaveAssetRegistryData( InOuter, Linker );

				// Save level information used by World browser
				UPackage::SaveWorldLevelInfo( InOuter, Linker );


				for (int32 i = 0; i < Linker->ExportMap.Num(); i++)
				{
					FObjectExport& Export = Linker->ExportMap[i];
					if (Export.Object)
					{
						// Set class index.
						// If this is *exactly* a UClass, store null instead; for anything else, including UClass-derived classes, map it
						UClass* ObjClass = Export.Object->GetClass();
						if (ObjClass != UClass::StaticClass())
						{
							Export.ClassIndex = Linker->MapObject(ObjClass);
							check(!Export.ClassIndex.IsNull());
						}
						else
						{
							Export.ClassIndex = FPackageIndex();
						}

						if (IsEventDrivenLoaderEnabledInCookedBuilds() && TargetPlatform)
						{
							UObject* Archetype = Export.Object->GetArchetype();
							check(Archetype);
							check(Archetype->IsA(Export.Object->HasAnyFlags(RF_ClassDefaultObject) ? ObjClass->GetSuperClass() : ObjClass));
							Export.TemplateIndex = Linker->MapObject(Archetype);
							UE_CLOG(Export.TemplateIndex.IsNull(), LogSavePackage, Fatal, TEXT("%s was an archetype of %s but returned a null index mapping the object."), *Archetype->GetFullName(), *Export.Object->GetFullName());
							check(!Export.TemplateIndex.IsNull());
						}

						// Set the parent index, if this export represents a UStruct-derived object
						if (UStruct* Struct = dynamic_cast<UStruct*>(Export.Object))
						{
							if (Struct->GetSuperStruct() != nullptr)
							{
								Export.SuperIndex = Linker->MapObject(Struct->GetSuperStruct());
								checkf(!Export.SuperIndex.IsNull(),
									TEXT("Export Struct (%s) of type (%s) inheriting from (%s) of type (%s) has not mapped super struct."),
									*GetPathNameSafe(Struct),
									*(Struct->GetClass()->GetName()),
									*GetPathNameSafe(Struct->GetSuperStruct()),
									*(Struct->GetSuperStruct()->GetClass()->GetName())
								);
							}
							else
							{
								Export.SuperIndex = FPackageIndex();
							}
						}
						else
						{
							Export.SuperIndex = FPackageIndex();
						}

						// Set FPackageIndex for this export's Outer. If the export's Outer
						// is the UPackage corresponding to this package's LinkerRoot, the
						if (Export.Object->GetOuter() != InOuter)
						{
							check(Export.Object->GetOuter());
							checkf(Export.Object->GetOuter()->IsIn(InOuter),
								TEXT("Export Object (%s) Outer (%s) mismatch."),
								*(Export.Object->GetPathName()),
								*(Export.Object->GetOuter()->GetPathName()));
							Export.OuterIndex = Linker->MapObject(Export.Object->GetOuter());
							checkf(!Export.OuterIndex.IsImport(),
								TEXT("Export Object (%s) Outer (%s) is an Import."),
								*(Export.Object->GetPathName()),
								*(Export.Object->GetOuter()->GetPathName()));

							if (Linker->IsCooking() && IsEventDrivenLoaderEnabledInCookedBuilds())
							{
								// Only packages are allowed to have no outer
								ensureMsgf(Export.OuterIndex != FPackageIndex() || Export.Object->IsA(UPackage::StaticClass()), TEXT("Export %s has no valid outer when cooking!"), *Export.Object->GetPathName());
							}
						}
						else
						{
							// this export's Outer is the LinkerRoot for this package
							Export.OuterIndex = FPackageIndex();
						}
					}
				}

				Linker->Summary.PreloadDependencyOffset = Linker->Tell();
				Linker->Summary.PreloadDependencyCount = -1;

				if (Linker->IsCooking() && IsEventDrivenLoaderEnabledInCookedBuilds())
				{
					const EObjectMark ExcludedObjectMarks = UPackage::GetExcludedObjectMarksForTargetPlatform(Linker->CookingTarget(), Linker->IsCooking());
					Linker->Summary.PreloadDependencyCount = 0;

					auto IncludeObjectAsDependency = [Linker, ExcludedObjectMarks](int32 CallSite, TSet<FPackageIndex>& AddTo, UObject* ToTest, UObject* ForObj, bool bMandatory, bool bOnlyIfInLinkerTable)
					{
						// Skip transient, editor only, and excluded client/server objects
						if (ToTest)
						{
							UPackage* Outermost = ToTest->GetOutermost();
							check(Outermost);
							if (Outermost->GetFName() == GLongCoreUObjectPackageName)
							{
								return; // We assume nothing in coreuobject ever loads assets in a constructor
							}
							FPackageIndex Index = Linker->MapObject(ToTest);
							if (Index.IsNull() && bOnlyIfInLinkerTable)
							{
								return;
							}
							if (!Index.IsNull() && (ToTest->HasAllFlags(RF_Transient) && !ToTest->IsNative()))
							{
								UE_LOG(LogSavePackage, Warning, TEXT("A dependency '%s' of '%s' is in the linker table, but is transient. We will keep the dependency anyway (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
							}
							if (!Index.IsNull() && ToTest->IsPendingKill())
							{
								UE_LOG(LogSavePackage, Warning, TEXT("A dependency '%s' of '%s' is in the linker table, but is pending kill. We will keep the dependency anyway (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
							}
							bool bNotFiltered = (ExcludedObjectMarks == OBJECTMARK_NOMARKS || !ToTest->HasAnyMarks(ExcludedObjectMarks)) && (!(Linker->Summary.PackageFlags & PKG_FilterEditorOnly) || !IsEditorOnlyObject(ToTest, false));
							if (bMandatory && !bNotFiltered)
							{
								UE_LOG(LogSavePackage, Warning, TEXT("A dependency '%s' of '%s' was filtered, but is mandatory. This indicates a problem with editor only stripping. We will keep the dependency anyway (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
								bNotFiltered = true;
							}
							if (bNotFiltered)
							{
								if (!Index.IsNull())
								{
									AddTo.Add(Index);
									return;
								}
								else
								{
									UE_CLOG(Outermost->HasAnyPackageFlags(PKG_CompiledIn), LogSavePackage, Verbose, TEXT("A compiled in dependency '%s' of '%s' was not actually in the linker tables and so will be ignored (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
									UE_CLOG(!Outermost->HasAnyPackageFlags(PKG_CompiledIn), LogSavePackage, Fatal, TEXT("A dependency '%s' of '%s' was not actually in the linker tables and so will be ignored (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
								}
							}
							check(!bMandatory);
						}
					};

					auto IncludeIndexAsDependency = [Linker](TSet<FPackageIndex>& AddTo, FPackageIndex Dep)
					{
						if (!Dep.IsNull())
						{
							UObject* ToTest = Dep.IsExport() ? Linker->Exp(Dep).Object : Linker->Imp(Dep).XObject;
							if (ToTest)
							{
								UPackage* Outermost = ToTest->GetOutermost();
								if (Outermost && Outermost->GetFName() != GLongCoreUObjectPackageName) // We assume nothing in coreuobject ever loads assets in a constructor
								{
									AddTo.Add(Dep);
								}
							}
						}
					};


					TArray<UObject*> Subobjects;
					TArray<UObject*> Deps;
					for (int32 i = 0; i < Linker->ExportMap.Num(); i++)
					{
						FObjectExport& Export = Linker->ExportMap[i];
						if (Export.Object)
						{
							GEDLCookChecker.AddExport(Export.Object);
							TSet<FPackageIndex> SerializationBeforeCreateDependencies;
							{
								IncludeIndexAsDependency(SerializationBeforeCreateDependencies, Export.ClassIndex);
								UObject* CDO = Export.Object->GetArchetype();
								IncludeObjectAsDependency(1, SerializationBeforeCreateDependencies, CDO, Export.Object, true, false);
								Subobjects.Reset();
								GetObjectsWithOuter(CDO, Subobjects);
								for (UObject* SubObj : Subobjects)
								{
									// Only include subobject archetypes
									if (SubObj->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject))
									{
										while (SubObj->HasAnyFlags(RF_Transient)) // transient components are stripped by the ICH, so find the one it will really use at runtime
										{
											UObject* SubObjArch = SubObj->GetArchetype();
											if (SubObjArch->GetClass()->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic))
											{
												break;
											}
											SubObj = SubObjArch;
										}
										if (!SubObj->IsPendingKill())
										{
											IncludeObjectAsDependency(2, SerializationBeforeCreateDependencies, SubObj, Export.Object, false, false);
										}
									}
								}
							}
							TSet<FPackageIndex> SerializationBeforeSerializationDependencies;
							{
								Deps.Reset();
								Export.Object->GetPreloadDependencies(Deps);

								for (UObject* Obj : Deps)
								{
									IncludeObjectAsDependency(3, SerializationBeforeSerializationDependencies, Obj, Export.Object, false, true);
								}
								if (Export.Object->HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
								{
									UObject *Outer = Export.Object->GetOuter();
									if (!Outer->IsA(UPackage::StaticClass()))
									{
										IncludeObjectAsDependency(4, SerializationBeforeSerializationDependencies, Outer, Export.Object, true, false);
									}
								}
								if (Export.Object->IsA(UClass::StaticClass()))
								{
									// we need to load archetypes of our subobjects before we load the class
									UObject* CDO = CastChecked<UClass>(Export.Object)->GetDefaultObject();
									Subobjects.Reset();
									GetObjectsWithOuter(CDO, Subobjects);
									for (UObject* SubObj : Subobjects)
									{
										// Only include subobject archetypes
										if (SubObj->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject))
										{
											SubObj = SubObj->GetArchetype();
											while (SubObj->HasAnyFlags(RF_Transient)) // transient components are stripped by the ICH, so find the one it will really use at runtime
											{
												UObject* SubObjArch = SubObj->GetArchetype();
												if (SubObjArch->GetClass()->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic))
												{
													break;
												}
												SubObj = SubObjArch;
											}
											if (!SubObj->IsPendingKill())
											{
												IncludeObjectAsDependency(5, SerializationBeforeSerializationDependencies, SubObj, Export.Object, false, false);
											}
										}
									}
								}
							}

							TSet<FPackageIndex> CreateBeforeSerializationDependencies;
							{
								UClass* Class = Cast<UClass>(Export.Object);
								UObject* ClassCDO = Class ? Class->GetDefaultObject() : nullptr;
								{
									TArray<FPackageIndex>& Depends = Linker->DependsMap[i];
									for (FPackageIndex Dep : Depends)
									{
										UObject* ToTest = Dep.IsExport() ? Linker->Exp(Dep).Object : Linker->Imp(Dep).XObject;
										if (ToTest != ClassCDO)
										{
											IncludeIndexAsDependency(CreateBeforeSerializationDependencies, Dep);
										}
									}
								}
								{
									TArray<UObject*>& NativeDeps = NativeObjectDependencies[Export.Object];
									for (UObject* ToTest : NativeDeps)
									{
										if (ToTest != ClassCDO)
										{
											IncludeObjectAsDependency(6, CreateBeforeSerializationDependencies, ToTest, Export.Object, false, true);
										}
									}
								}
							}
							TSet<FPackageIndex> CreateBeforeCreateDependencies;
							{
								IncludeIndexAsDependency(CreateBeforeCreateDependencies, Export.OuterIndex);
								IncludeIndexAsDependency(CreateBeforeCreateDependencies, Export.SuperIndex);
							}

							auto AddArcForDepChecking = [Linker, &Export](bool bExportIsSerialize, FPackageIndex Dep, bool bDepIsSerialize)
							{
								check(Export.Object);
								check(!Dep.IsNull());
								UObject* DepObject = Dep.IsExport() ? Linker->Exp(Dep).Object : Linker->Imp(Dep).XObject;
								check(DepObject);

								Linker->DepListForErrorChecking.Add(Dep);

								GEDLCookChecker.AddArc(DepObject, bDepIsSerialize, Export.Object, bExportIsSerialize);
							};

							for (FPackageIndex Index : SerializationBeforeSerializationDependencies)
							{
								if (SerializationBeforeCreateDependencies.Contains(Index))
								{
									continue; // if the other thing must be serialized before we create, then this is a redundant dep
								}
								if (Export.FirstExportDependency == -1)
								{
									Export.FirstExportDependency = Linker->Summary.PreloadDependencyCount;
									check(Export.SerializationBeforeSerializationDependencies == 0 && Export.CreateBeforeSerializationDependencies == 0 && Export.SerializationBeforeCreateDependencies == 0 && Export.CreateBeforeCreateDependencies == 0);
								}
								Linker->Summary.PreloadDependencyCount++;
								Export.SerializationBeforeSerializationDependencies++;
								*Linker << Index;
								AddArcForDepChecking(true, Index, true);
							}
							for (FPackageIndex Index : CreateBeforeSerializationDependencies)
							{
								if (SerializationBeforeCreateDependencies.Contains(Index))
								{
									continue; // if the other thing must be serialized before we create, then this is a redundant dep
								}
								if (SerializationBeforeSerializationDependencies.Contains(Index))
								{
									continue; // if the other thing must be serialized before we serialize, then this is a redundant dep
								}
								if (CreateBeforeCreateDependencies.Contains(Index))
								{
									continue; // if the other thing must be created before we are created, then this is a redundant dep
								}
								if (Export.FirstExportDependency == -1)
								{
									Export.FirstExportDependency = Linker->Summary.PreloadDependencyCount;
									check(Export.SerializationBeforeSerializationDependencies == 0 && Export.CreateBeforeSerializationDependencies == 0 && Export.SerializationBeforeCreateDependencies == 0 && Export.CreateBeforeCreateDependencies == 0);
								}
								Linker->Summary.PreloadDependencyCount++;
								Export.CreateBeforeSerializationDependencies++;
								*Linker << Index;
								AddArcForDepChecking(true, Index, false);
							}
							for (FPackageIndex Index : SerializationBeforeCreateDependencies)
							{
								if (Export.FirstExportDependency == -1)
								{
									Export.FirstExportDependency = Linker->Summary.PreloadDependencyCount;
									check(Export.SerializationBeforeSerializationDependencies == 0 && Export.CreateBeforeSerializationDependencies == 0 && Export.SerializationBeforeCreateDependencies == 0 && Export.CreateBeforeCreateDependencies == 0);
								}
								Linker->Summary.PreloadDependencyCount++;
								Export.SerializationBeforeCreateDependencies++;
								*Linker << Index;
								AddArcForDepChecking(false, Index, true);
							}
							for (FPackageIndex Index : CreateBeforeCreateDependencies)
							{
								if (Export.FirstExportDependency == -1)
								{
									Export.FirstExportDependency = Linker->Summary.PreloadDependencyCount;
									check(Export.SerializationBeforeSerializationDependencies == 0 && Export.CreateBeforeSerializationDependencies == 0 && Export.SerializationBeforeCreateDependencies == 0 && Export.CreateBeforeCreateDependencies == 0);
								}
								Linker->Summary.PreloadDependencyCount++;
								Export.CreateBeforeCreateDependencies++;
								*Linker << Index;
								AddArcForDepChecking(false, Index, false);
							}
						}
					}
					UE_LOG(LogSavePackage, Verbose, TEXT("Saved %d dependencies for %d exports."), Linker->Summary.PreloadDependencyCount, Linker->ExportMap.Num());
				}

				
				Linker->Summary.TotalHeaderSize	= Linker->Tell();

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame(1, NSLOCTEXT("Core", "ProcessingExports", "ProcessingExports..."));

				// look for this package in the list of packages to generate script SHA for 
				TArray<uint8>* ScriptSHABytes = FLinkerSave::PackagesToScriptSHAMap.Find(*FPaths::GetBaseFilename(Filename));

				// if we want to generate the SHA key, start tracking script writes
				if (ScriptSHABytes)
				{
					Linker->StartScriptSHAGeneration();
				}

				{
					COOK_STAT(FScopedDurationTimer SaveTimer(SavePackageStats::SerializeExportsTimeSec));
#if WITH_EDITOR
					FArchive::FScopeSetDebugSerializationFlags S(*Linker, DSF_IgnoreDiff, true);
#endif
					FScopedSlowTask ExportScope(Linker->ExportMap.Num());

					// Save exports.
					int32 LastExportSaveStep = 0;
					for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
					{
						if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
						{ 
							return ESavePackageResult::Canceled;
						}
						ExportScope.EnterProgressFrame();

						FObjectExport& Export = Linker->ExportMap[i];
						if( Export.Object )
						{
							// Save the object data.
							Export.SerialOffset = Linker->Tell();
							Linker->CurrentlySavingExport = FPackageIndex::FromExport(i);
							// UE_LOG(LogSavePackage, Log, TEXT("export %s for %s"), *Export.Object->GetFullName(), *Linker->CookingTarget()->PlatformName());
							if ( Export.Object->HasAnyFlags(RF_ClassDefaultObject) )
							{
								Export.Object->GetClass()->SerializeDefaultObject(Export.Object, *Linker);
							}
							else
							{
#if ENABLE_PACKAGE_CLASS_SERIALIZATION_TIMES
								auto& TimingInfo = SavePackageStats::PackageClassSerializeTimes.FindOrAdd(Export.Object->GetClass()->GetFName());
								TimingInfo.Value++;
								FScopedDurationTimer SerializeTimer(TimingInfo.Key);
#endif
								Export.Object->Serialize( *Linker );

#if WITH_EDITOR
								if (Linker->IsCooking())
								{
									Export.Object->CookAdditionalFiles(Filename, Linker->CookingTarget());
								}
#endif
							}
							Linker->CurrentlySavingExport = FPackageIndex();
							Export.SerialSize = Linker->Tell() - Export.SerialOffset;

							// Mark object as having been saved.
							Export.Object->Mark(OBJECTMARK_Saved);
						}
					}
				}

				// if we want to generate the SHA key, get it out now that the package has finished saving
				if (ScriptSHABytes && Linker->ContainsCode())
				{
					// make space for the 20 byte key
					ScriptSHABytes->Empty(20);
					ScriptSHABytes->AddUninitialized(20);

					// retrieve it
					Linker->GetScriptSHAKey(ScriptSHABytes->GetData());
				}

				
				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame(1, NSLOCTEXT("Core", "SerializingBulkData", "Serializing bulk data"));

				// now we write all the bulkdata that is supposed to be at the end of the package
				// and fix up the offset
				int64 StartOfBulkDataArea = Linker->Tell();
				Linker->Summary.BulkDataStartOffset = StartOfBulkDataArea;

				if (Linker->BulkDataToAppend.Num() > 0)
				{
					COOK_STAT(FScopedDurationTimer SaveTimer(SavePackageStats::SerializeBulkDataTimeSec));

					FScopedSlowTask BulkDataFeedback(Linker->BulkDataToAppend.Num());

					FArchive* TargetArchive = Linker;
					FArchive* BulkArchive = nullptr;
					uint32 ExtraBulkDataFlags = 0;

					static struct FUseSeperateBulkDataFiles
					{
						bool bEnable;
						FUseSeperateBulkDataFiles()
						{
							if (!GConfig->GetBool(TEXT("Core.System"), TEXT("UseSeperateBulkDataFiles"), bEnable, GEngineIni))
							{
								bEnable = false;
							}
							if (IsEventDrivenLoaderEnabledInCookedBuilds())
							{
								// Always split bulk data when splitting cooked files
								bEnable = true;
							}
						}
					} ShouldUseSeperateBulkDataFiles;

					const bool bShouldUseSeparateBulkFile = ShouldUseSeperateBulkDataFiles.bEnable && Linker->IsCooking();

					const FString BulkFilename = FPaths::ChangeExtension(Filename, TEXT(".ubulk"));

					if (bShouldUseSeparateBulkFile)
					{
						ExtraBulkDataFlags = BULKDATA_PayloadInSeperateFile;
						if ( bSaveAsync )
						{
							TargetArchive = BulkArchive = new FBufferArchive(true);
						}
						else
						{
							TargetArchive = BulkArchive = IFileManager::Get().CreateFileWriter(*BulkFilename);
						}
					}

					for (int32 i=0; i < Linker->BulkDataToAppend.Num(); ++i)
					{
						BulkDataFeedback.EnterProgressFrame();

						FLinkerSave::FBulkDataStorageInfo& BulkDataStorageInfo = Linker->BulkDataToAppend[i];

						// Set bulk data flags to what they were during initial serialization (they might have changed after that)
						const uint32 OldBulkDataFlags = BulkDataStorageInfo.BulkData->GetBulkDataFlags();
						uint32 ModifiedBulkDataFlags = BulkDataStorageInfo.BulkDataFlags | ExtraBulkDataFlags;
						BulkDataStorageInfo.BulkData->SetBulkDataFlags(ModifiedBulkDataFlags);

						int64 BulkStartOffset = TargetArchive->Tell();
						int64 StoredBulkStartOffset = BulkStartOffset - StartOfBulkDataArea;

						BulkDataStorageInfo.BulkData->SerializeBulkData(*TargetArchive, BulkDataStorageInfo.BulkData->Lock(LOCK_READ_ONLY));

						int64 BulkEndOffset = TargetArchive->Tell();
						int64 LinkerEndOffset = Linker->Tell();

						int32 SizeOnDisk = (int32)(BulkEndOffset - BulkStartOffset);
				
						Linker->Seek(BulkDataStorageInfo.BulkDataFlagsPos);
						*Linker << ModifiedBulkDataFlags;

						Linker->Seek(BulkDataStorageInfo.BulkDataOffsetInFilePos);
						*Linker << StoredBulkStartOffset;

						Linker->Seek(BulkDataStorageInfo.BulkDataSizeOnDiskPos);
						*Linker << SizeOnDisk;

						Linker->Seek(LinkerEndOffset);

						// Restore BulkData flags to before serialization started
						BulkDataStorageInfo.BulkData->ClearBulkDataFlags(0xFFFFFFFF);
						BulkDataStorageInfo.BulkData->SetBulkDataFlags(OldBulkDataFlags);
						BulkDataStorageInfo.BulkData->Unlock();
					}

					if (BulkArchive)
					{
						TotalPackageSizeUncompressed += BulkArchive->TotalSize();
						BulkArchive->Close();
						if ( bSaveAsync )
						{
							FBufferArchive* BulkBuffer = (FBufferArchive*)(BulkArchive);

							int64 DataSize = BulkBuffer->TotalSize();

							// TODO - update the BulkBuffer code to write into a FLargeMemoryWriter so we can 
							// take ownership of the data
							uint8* Data = new uint8[DataSize];
							FMemory::Memcpy(Data, BulkBuffer->GetData(), DataSize);							

							FLargeMemoryPtr DataPtr = FLargeMemoryPtr(Data);
		
							if ( BulkBuffer->Num() > 0 )
							{
								AsyncWriteFile(MoveTemp(DataPtr), DataSize, *BulkFilename, FDateTime::MinValue(), false);
							}
						}
						delete BulkArchive;
					}
				}

				Linker->BulkDataToAppend.Empty();
			
				// write the package post tag
				uint32 Tag = PACKAGE_FILE_TAG;
				*Linker << Tag;

				// We capture the package size before the first seek to work with archives that don't report the file
				// size correctly while the file is still being written to.
				PackageSize = Linker->Tell();

				// Save the import map.
				Linker->Seek( Linker->Summary.ImportOffset );
				for( int32 i=0; i<Linker->ImportMap.Num(); i++ )
				{
					FObjectImport& Import = Linker->ImportMap[ i ];
					if ( Import.XObject )
					{
					// Set the package index.
						if( Import.XObject->GetOuter() )
						{
							if ( Import.XObject->GetOuter()->IsIn(InOuter) )
							{
								if (!Import.XObject->HasAllFlags(RF_Transient) || !Import.XObject->IsNative())
								{
									UE_LOG(LogSavePackage, Warning, TEXT("Bad Object=%s"),*Import.XObject->GetFullName());
								}
								else
								{
									// if an object is marked RF_Transient and native, it is either an intrinsic class or
									// a property of an intrinsic class.  Only properties of intrinsic classes will have
									// an Outer that passes the check for "GetOuter()->IsIn(InOuter)" (thus ending up in this
									// block of code).  Just verify that the Outer for this property is also marked RF_Transient and Native
									check(Import.XObject->GetOuter()->HasAllFlags(RF_Transient) && Import.XObject->GetOuter()->IsNative());
								}
							}
							check(!Import.XObject->GetOuter()->IsIn(InOuter) || Import.XObject->HasAllFlags(RF_Transient) || Import.XObject->IsNative());
#if WITH_EDITOR
							UObject** ReplacedOuter = ReplacedImportOuters.Find(Import.XObject);
							if (ReplacedOuter && *ReplacedOuter)
							{
								Import.OuterIndex = Linker->MapObject(*ReplacedOuter);
								ensure(Import.OuterIndex != FPackageIndex());
							}
							else
#endif
							{
								Import.OuterIndex = Linker->MapObject(Import.XObject->GetOuter());
							}

							if (Linker->IsCooking() && IsEventDrivenLoaderEnabledInCookedBuilds())
							{
								// Only package imports are allowed to have no outer
								ensureMsgf(Import.OuterIndex != FPackageIndex() || Import.ClassName == NAME_Package, TEXT("Import %s has no valid outer when cooking!"), *Import.XObject->GetPathName());
							}
						}
					}
					else
					{
						checkf(Conform != nullptr, TEXT("NULL XObject for import %i - Object: %s Class: %s"), i, *Import.ObjectName.ToString(), *Import.ClassName.ToString());
					}

					// Save it.
					*Linker << Import;
				}
				

				check( Linker->Tell() == OffsetAfterImportMap );

				// Save the export map.
				Linker->Seek(Linker->Summary.ExportOffset);
				{
#if WITH_EDITOR
					FArchive::FScopeSetDebugSerializationFlags S(*Linker, DSF_IgnoreDiff, true);
#endif
					for (int32 i = 0; i < Linker->ExportMap.Num(); i++)
					{
						*Linker << Linker->ExportMap[i];
					}
				}
				check( Linker->Tell() == OffsetAfterExportMap );

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				FFormatNamedArguments NamedArgs;
				NamedArgs.Add( TEXT("CleanFilename"), FText::FromString( CleanFilename ) );
				SlowTask.DefaultMessage = FText::Format( NSLOCTEXT("Core", "Finalizing", "Finalizing: {CleanFilename}..."), NamedArgs );

				//@todo: remove ExportCount and NameCount - no longer used
				Linker->Summary.Generations.Last().ExportCount = Linker->Summary.ExportCount;
				Linker->Summary.Generations.Last().NameCount   = Linker->Summary.NameCount;	

				// create the package source (based on developer or user created)
	#if (UE_BUILD_SHIPPING && WITH_EDITOR)
				Linker->Summary.PackageSource = FMath::Rand() * FMath::Rand();
	#else
				Linker->Summary.PackageSource = FCrc::StrCrc_DEPRECATED(*FPaths::GetBaseFilename(Filename).ToUpper());
	#endif

				// Flag package as requiring localization gather if the archive requires localization gathering.
				Linker->LinkerRoot->ThisRequiresLocalizationGather(Linker->RequiresLocalizationGather());
				
				// Update package flags from package, in case serialization has modified package flags.
				Linker->Summary.PackageFlags = Linker->LinkerRoot->GetPackageFlags() & ~PKG_NewlyCreated;

				Linker->Seek(0);
				*Linker << Linker->Summary;
				check( Linker->Tell() == OffsetAfterPackageFileSummary );

				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				// Detach archive used for saving, closing file handle.
				if (!bSaveAsync)
				{
					Linker->Detach();
				}
				UNCLOCK_CYCLES(Time);
				UE_LOG(LogSavePackage, Log,  TEXT("Save=%.2fms"), FPlatformTime::ToMilliseconds(Time) );
		
				if ( EndSavingIfCancelled( Linker, TempFilename ) ) 
				{ 
					return ESavePackageResult::Canceled;
				}
				SlowTask.EnterProgressFrame();

				if( Success == true )
				{
					// Compress the temporarily file to destination.
					if (bSaveAsync)
					{
						UE_LOG(LogSavePackage, Log, TEXT("Async saving from memory to '%s'"), *NewPath);

						// Detach archive used for memory saving.
						FLargeMemoryWriter* Writer = (FLargeMemoryWriter*)(Linker->Saver);
						int64 DataSize = Writer->TotalSize();

						COOK_STAT(FScopedDurationTimer SaveTimer(SavePackageStats::AsyncWriteTimeSec));
						TotalPackageSizeUncompressed += DataSize;

						FLargeMemoryPtr DataPtr(Writer->GetData());
						Writer->ReleaseOwnership();
						if (IsEventDrivenLoaderEnabledInCookedBuilds() && Linker->IsCooking())
						{
							AsyncWriteFileWithSplitExports(MoveTemp(DataPtr), DataSize, Linker->Summary.TotalHeaderSize, *NewPath, FinalTimeStamp);
						}
						else
						{
							AsyncWriteFile(MoveTemp(DataPtr), DataSize, *NewPath, FinalTimeStamp);
						}

						Linker->Detach();
					}
					// Move the temporary file.
					else
					{
						UE_LOG(LogSavePackage, Log,  TEXT("Moving '%s' to '%s'"), *TempFilename, *NewPath );
						TotalPackageSizeUncompressed += IFileManager::Get().FileSize(*TempFilename);
						Success = IFileManager::Get().Move( *NewPath, *TempFilename );
						if (FinalTimeStamp != FDateTime::MinValue())
						{
							IFileManager::Get().SetTimeStamp(*NewPath, FinalTimeStamp);
						}
					}

					if( Success == false )
					{
						if (SaveFlags & SAVE_NoError)
						{
							UE_LOG(LogSavePackage, Warning, TEXT("%s"), *FString::Printf( TEXT("Error saving '%s'"), Filename ) );
						}
						else
						{
							UE_LOG(LogSavePackage, Error, TEXT("%s"), *FString::Printf( TEXT("Error saving '%s'"), Filename ) );
							Error->Logf(ELogVerbosity::Warning, TEXT("%s"), *FText::Format( NSLOCTEXT( "Core", "SaveWarning", "Error saving '{0}'" ), FText::FromString( FString( Filename ) ) ).ToString() );
						}
					}
					else
					{
						// Mark exports and the package as RF_Loaded after they've been serialized
						// This is to ensue that newly created packages are properly marked as loaded (since they now exist on disk and 
						// in memory in the exact same state).
						for (auto& Export : Linker->ExportMap)
						{
							if (Export.Object)
							{
								Export.Object->SetFlags(RF_WasLoaded|RF_LoadCompleted);
							}
						}
						if (Linker->LinkerRoot)
						{
							// And finally set the flag on the package itself.
							Linker->LinkerRoot->SetFlags(RF_WasLoaded|RF_LoadCompleted);
						}

						// Clear dirty flag if desired
						if (!(SaveFlags & SAVE_KeepDirty))
						{
							InOuter->SetDirtyFlag(false);
						}
						
						// Update package FileSize value
						InOuter->FileSize = IFileManager::Get().FileSize(Filename);

						// Warn about long package names, which may be bad for consoles with limited filename lengths.
						if( bWarnOfLongFilename == true )
						{
							int32 MaxFilenameLength = MAX_UNREAL_FILENAME_LENGTH;

							// If the name is of the form "_LOC_xxx.ext", remove the loc data before the length check
							FString CleanBaseFilename = BaseFilename;
							if( CleanBaseFilename.Find( "_LOC_" ) == BaseFilename.Len() - 8 )
							{
								CleanBaseFilename = BaseFilename.LeftChop( 8 );
							}

							if( CleanBaseFilename.Len() > MaxFilenameLength )
							{
								if (SaveFlags & SAVE_NoError)
								{
									UE_LOG(LogSavePackage, Warning, TEXT("%s"), *FString::Printf( TEXT("Filename '%s' is too long; this may interfere with cooking for consoles.  Unreal filenames should be no longer than %s characters."), *BaseFilename, MaxFilenameLength ) );
								}
								else
								{
									FFormatNamedArguments Arguments;
									Arguments.Add(TEXT("FileName"), FText::FromString( BaseFilename ));
									Arguments.Add(TEXT("MaxLength"), FText::AsNumber( MaxFilenameLength ));
									Error->Logf(ELogVerbosity::Warning, TEXT("%s"), *FText::Format( NSLOCTEXT( "Core", "Error_FilenameIsTooLongForCooking", "Filename '{FileName}' is too long; this may interfere with cooking for consoles.  Unreal filenames should be no longer than {MaxLength} characters." ), Arguments ).ToString() );
								}
							}
						}
					}

					// Delete the temporary file.
					IFileManager::Get().Delete( *TempFilename );
				}
				COOK_STAT(SavePackageStats::MBWritten += ((double)TotalPackageSizeUncompressed) / 1024.0 / 1024.0);

				SlowTask.EnterProgressFrame();
			}

			// Route PostSaveRoot to allow e.g. the world to detach components for the persistent level that were
			// attached in PreSaveRoot.
			if( Base )
			{
				Base->PostSaveRoot( bCleanupIsRequired );
			}

			SlowTask.EnterProgressFrame();
			
#if WITH_EDITOR
			for ( int CachedObjectIndex = 0; CachedObjectIndex < CachedObjects.Num(); ++CachedObjectIndex )
			{
				CachedObjects[CachedObjectIndex]->ClearCachedCookedPlatformData(TargetPlatform);
			}
#endif

		}
		if( Success == true )
		{
			// Package has been save, so unmark NewlyCreated flag.
			InOuter->ClearPackageFlags(PKG_NewlyCreated);

			// send a message that the package was saved
			UPackage::PackageSavedEvent.Broadcast(Filename, InOuter);
		}

		// We're done!
		SlowTask.EnterProgressFrame();

		UE_LOG(LogSavePackage, Display, TEXT("Finished SavePackage %s"), Filename);

		if (Success)
		{
			if (bRequestStub)
			{
				return FSavePackageResultStruct(ESavePackageResult::GenerateStub, TotalPackageSizeUncompressed);
			}
			else
			{
				return FSavePackageResultStruct(ESavePackageResult::Success, TotalPackageSizeUncompressed);
			}
		}
		else
		{
			if (bRequestStub)
			{
				UE_LOG(LogSavePackage, Warning, TEXT("C++ stub requested, but package failed to save, may cause compile errors: %s"), Filename);
			}
			return ESavePackageResult::Error;
		}
	}
	else
	{
		return ESavePackageResult::Error;
	}
}

bool UPackage::SavePackage(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename,
	FOutputDevice* Error, FLinkerLoad* Conform, bool bForceByteSwapping, bool bWarnOfLongFilename, uint32 SaveFlags,
	const class ITargetPlatform* TargetPlatform, const FDateTime&  FinalTimeStamp, bool bSlowTask)
{
	const FSavePackageResultStruct Result = Save(InOuter, Base, TopLevelFlags, Filename, Error, Conform, bForceByteSwapping,
		bWarnOfLongFilename, SaveFlags, TargetPlatform, FinalTimeStamp, bSlowTask);
	return Result == ESavePackageResult::Success;
}

/**
 * Static: Saves thumbnail data for the specified package outer and linker
 *
 * @param	InOuter							the outer to use for the new package
 * @param	Linker							linker we're currently saving with
 */
void UPackage::SaveThumbnails( UPackage* InOuter, FLinkerSave* Linker )
{
	Linker->Summary.ThumbnailTableOffset = 0;

	// Do we have any thumbnails to save?
	if( !(Linker->Summary.PackageFlags & PKG_FilterEditorOnly) && InOuter->HasThumbnailMap() )
	{
		const FThumbnailMap& PackageThumbnailMap = InOuter->GetThumbnailMap();


		// Figure out which objects have thumbnails.  Note that we only want to save thumbnails
		// for objects that are actually in the export map.  This is so that we avoid saving out
		// thumbnails that were cached for deleted objects and such.
		TArray< FObjectFullNameAndThumbnail > ObjectsWithThumbnails;
		for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
		{
			FObjectExport& Export = Linker->ExportMap[i];
			if( Export.Object )
			{
				const FName ObjectFullName( *Export.Object->GetFullName() );
				const FObjectThumbnail* ObjectThumbnail = PackageThumbnailMap.Find( ObjectFullName );
		
				// if we didn't find the object via full name, try again with ??? as the class name, to support having
				// loaded old packages without going through the editor (ie cooking old packages)
				if (ObjectThumbnail == nullptr)
				{
					// can't overwrite ObjectFullName, so that we add it properly to the map
					FName OldPackageStyleObjectFullName = FName(*FString::Printf(TEXT("??? %s"), *Export.Object->GetPathName()));
					ObjectThumbnail = PackageThumbnailMap.Find(OldPackageStyleObjectFullName);
				}
				if( ObjectThumbnail != nullptr )
				{
					// IMPORTANT: We save all thumbnails here, even if they are a shared (empty) thumbnail!
					// Empty thumbnails let us know that an asset is in a package without having to
					// make a linker for it.
					ObjectsWithThumbnails.Add( FObjectFullNameAndThumbnail( ObjectFullName, ObjectThumbnail ) );
				}
			}
		}

		// preserve thumbnail rendered for the level
		const FObjectThumbnail* ObjectThumbnail = PackageThumbnailMap.Find(FName(*InOuter->GetFullName()));
		if (ObjectThumbnail != nullptr)
		{
			ObjectsWithThumbnails.Add( FObjectFullNameAndThumbnail(FName(*InOuter->GetFullName()), ObjectThumbnail ) );
		}
		
		// Do we have any thumbnails?  If so, we'll save them out along with a table of contents
		if( ObjectsWithThumbnails.Num() > 0 )
		{
			// Save out the image data for the thumbnails
			for( int32 CurObjectIndex = 0; CurObjectIndex < ObjectsWithThumbnails.Num(); ++CurObjectIndex )
			{
				FObjectFullNameAndThumbnail& CurObjectThumb = ObjectsWithThumbnails[ CurObjectIndex ];

				// Store the file offset to this thumbnail
				CurObjectThumb.FileOffset = Linker->Tell();

				// Serialize the thumbnail!
				FObjectThumbnail* SerializableThumbnail = const_cast< FObjectThumbnail* >( CurObjectThumb.ObjectThumbnail );
				SerializableThumbnail->Serialize( *Linker );
			}


			// Store the thumbnail table of contents
			{
				Linker->Summary.ThumbnailTableOffset = Linker->Tell();

				// Save number of thumbnails
				int32 ThumbnailCount = ObjectsWithThumbnails.Num();
				*Linker << ThumbnailCount;

				// Store a list of object names along with the offset in the file where the thumbnail is stored
				for( int32 CurObjectIndex = 0; CurObjectIndex < ObjectsWithThumbnails.Num(); ++CurObjectIndex )
				{
					const FObjectFullNameAndThumbnail& CurObjectThumb = ObjectsWithThumbnails[ CurObjectIndex ];

					// Object name
					const FString ObjectFullName = CurObjectThumb.ObjectFullName.ToString();

					// Break the full name into it's class and path name parts
					const int32 FirstSpaceIndex = ObjectFullName.Find( TEXT( " " ) );
					check( FirstSpaceIndex != INDEX_NONE && FirstSpaceIndex > 0 );
					FString ObjectClassName = ObjectFullName.Left( FirstSpaceIndex );
					const FString ObjectPath = ObjectFullName.Mid( FirstSpaceIndex + 1 );

					// Remove the package name from the object path since that will be implicit based
					// on the package file name
					FString ObjectPathWithoutPackageName = ObjectPath.Mid( ObjectPath.Find( TEXT( "." ) ) + 1 );

					// Store both the object's class and path name (relative to it's package)
					*Linker << ObjectClassName;
					*Linker << ObjectPathWithoutPackageName;

					// File offset for the thumbnail (already saved out.)
					int32 FileOffset = CurObjectThumb.FileOffset;
					*Linker << FileOffset;
				}
			}
		}
	}

	// if content browser isn't enabled, clear the thumbnail map so we're not using additional memory for nothing
	if ( !GIsEditor || IsRunningCommandlet() )
	{
		InOuter->ThumbnailMap.Reset();
	}
}

void UPackage::SaveAssetRegistryData( UPackage* InOuter, FLinkerSave* Linker )
{
	// Make a copy of the tag map
	TArray<UObject*> AssetObjects;

	if( !(Linker->Summary.PackageFlags & PKG_FilterEditorOnly) )
	{
		// Find any exports which are not in the tag map
		for( int32 i=0; i<Linker->ExportMap.Num(); i++ )
		{
			FObjectExport& Export = Linker->ExportMap[i];
			if( Export.Object && Export.Object->IsAsset() )
			{
				AssetObjects.Add(Export.Object);
			}
		}
	}

	// Store the asset registry offser in the file
	Linker->Summary.AssetRegistryDataOffset = Linker->Tell();

	// Save the number of objects in the tag map
	int32 ObjectCount = AssetObjects.Num();
	*Linker << ObjectCount;

	// If there are any Asset Registry tags, add them to the summary
	for (int32 ObjectIdx = 0; ObjectIdx < AssetObjects.Num(); ++ObjectIdx)
	{
		const UObject* Object = AssetObjects[ObjectIdx];

		// Exclude the package name in the object path, we just need to know the path relative to the outermost
		FString ObjectPath = Object->GetPathName(Object->GetOutermost());
		FString ObjectClassName = Object->GetClass()->GetName();
		
		TArray<FAssetRegistryTag> Tags;
		Object->GetAssetRegistryTags(Tags);

		int32 TagCount = Tags.Num();

		*Linker << ObjectPath;
		*Linker << ObjectClassName;
		*Linker << TagCount;
				
		for (TArray<FAssetRegistryTag>::TConstIterator TagIter(Tags); TagIter; ++TagIter)
		{
			FString Key = TagIter->Name.ToString();
			FString Value = TagIter->Value;
			*Linker << Key;
			*Linker << Value;
		}
	}
}

void UPackage::SaveWorldLevelInfo( UPackage* InOuter, FLinkerSave* Linker )
{
	Linker->Summary.WorldTileInfoDataOffset = 0;
	
	if(InOuter->WorldTileInfo.IsValid())
	{
		Linker->Summary.WorldTileInfoDataOffset = Linker->Tell();
		*Linker << *(InOuter->WorldTileInfo);
	}
}

bool UPackage::IsEmptyPackage (UPackage* Package, const UObject* LastReferencer)
{
	// Don't count null or volatile packages as empty, just let them be NULL or get GCed
	if ( Package != nullptr )
	{
		// Make sure the package is fully loaded before determining if it is empty
		if( !Package->IsFullyLoaded() )
		{
			Package->FullyLoad();
		}

		if (LastReferencer != nullptr)
		{
			// See if there will be no remaining non-redirector, non-metadata objects within this package other than LastReferencer
			for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
			{
				UObject* Object = *ObjIt;
				if ( Object->IsIn(Package)								// Is inside this package
					&& Object->IsAsset()								// Is an asset
					&& Object != LastReferencer )						// Is not the object being deleted
				{
					// The package contains at least one more UObject
					return false;
				}
			}
		}
		else
		{
			// See if there are no more objects within this package other than redirectors and meta data objects
			for (TObjectIterator<UObject> ObjIt; ObjIt; ++ObjIt)
			{
				UObject* Object = *ObjIt;
				if ( Object->IsIn(Package)								// Is inside this package
					&& Object->IsAsset() )								// Is an asset
				{
					// The package contains at least one more UObject
					return false;
				}
			}
		}

		// There are no more valid UObjects with this package as an outer
		return true;
	}

	// Invalid package
	return false;
}
