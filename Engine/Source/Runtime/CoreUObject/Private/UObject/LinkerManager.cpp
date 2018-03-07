// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinkerManager.h: Unreal object linker manager
=============================================================================*/
#include "UObject/LinkerManager.h"
#include "Internationalization/GatherableTextData.h"
#include "UObject/Package.h"
#include "UObject/ObjectResource.h"
#include "UObject/LinkerLoad.h"
#include "UObject/UObjectThreadContext.h"

FLinkerManager& FLinkerManager::Get()
{
	static TUniquePtr<FLinkerManager> Singleton = MakeUnique<FLinkerManager>();
	return *Singleton;
}

bool FLinkerManager::Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("LinkerLoadList")))
	{
		UE_LOG(LogLinker, Display, TEXT("ObjectLoaders: %d"), ObjectLoaders.Num());
#if !UE_BUILD_SHIPPING
		for (auto Linker : ObjectLoaders)
		{
			UE_LOG(LogLinker, Display, TEXT("%s"), *Linker->Filename);
		}
#endif
		UE_LOG(LogLinker, Display, TEXT("LoadersWithNewImports: %d"), LoadersWithNewImports.Num());
#if !UE_BUILD_SHIPPING
		for (auto Linker : LoadersWithNewImports)
		{
			UE_LOG(LogLinker, Display, TEXT("%s"), *Linker->Filename);
		}
#endif

#if !UE_BUILD_SHIPPING
		UE_LOG(LogLinker, Display, TEXT("LiveLinkers: %d"), LiveLinkers.Num());
		for (auto Linker : LiveLinkers)
		{
			UE_LOG(LogLinker, Display, TEXT("%s"), *Linker->Filename);
		}
#endif
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("LINKERS")))
	{
		Ar.Logf(TEXT("Linkers:"));
		for (auto Linker : ObjectLoaders)
		{
			int32 NameSize = 0;
			for (int32 j = 0; j < Linker->NameMap.Num(); j++)
			{
				if (Linker->NameMap[j] != NAME_None)
				{
					NameSize += FNameEntry::GetSize(*Linker->NameMap[j].ToString());
				}
			}
			Ar.Logf
				(
				TEXT("%s (%s): Names=%i (%iK/%iK) Text=%i (%iK) Imports=%i (%iK) Exports=%i (%iK) Gen=%i Bulk=%i"),
				*Linker->Filename,
				*Linker->LinkerRoot->GetFullName(),
				Linker->NameMap.Num(),
				Linker->NameMap.Num() * sizeof(FName) / 1024,
				NameSize / 1024,
				Linker->GatherableTextDataMap.Num(),
				Linker->GatherableTextDataMap.Num() * sizeof(FGatherableTextData) / 1024,
				Linker->ImportMap.Num(),
				Linker->ImportMap.Num() * sizeof(FObjectImport) / 1024,
				Linker->ExportMap.Num(),
				Linker->ExportMap.Num() * sizeof(FObjectExport) / 1024,
				Linker->Summary.Generations.Num(),
#if WITH_EDITOR
				Linker->BulkDataLoaders.Num()
#else
				0
#endif // WITH_EDITOR
				);
		}

		return true;
	}
	return false;
}

void FLinkerManager::ResetLoaders(UObject* InPkg)
{
	// Top level package to reset loaders for.
	UObject*		TopLevelPackage = InPkg ? InPkg->GetOutermost() : NULL;

	// Find loader/ linker associated with toplevel package. We do this upfront as Detach resets LinkerRoot.
	if (TopLevelPackage)
	{
		// Linker to reset/ detach.
		auto LinkerToReset = FLinkerLoad::FindExistingLinkerForPackage(CastChecked<UPackage>(TopLevelPackage));
		if (LinkerToReset)
		{
			{
#if THREADSAFE_UOBJECTS
				FScopeLock ObjectLoadersLock(&ObjectLoadersCritical);
#endif
				for (auto Linker : ObjectLoaders)
				{
					// Detach LinkerToReset from other linker's import table.
					if (Linker->LinkerRoot != TopLevelPackage)
					{
						for (auto& Import : Linker->ImportMap)
						{
							if (Import.SourceLinker == LinkerToReset)
							{
								Import.SourceLinker = NULL;
								Import.SourceIndex = INDEX_NONE;
							}
						}
					}
					else
					{
						check(Linker == LinkerToReset);
					}
				}
			}
			// Detach linker, also removes from array and sets LinkerRoot to NULL.
			LinkerToReset->LoadAndDetachAllBulkData();
			LinkerToReset->Detach();
			RemoveLinker(LinkerToReset);
			LinkerToReset = nullptr;
		}
	}
	else
	{
		// We just want a copy here
		TSet<FLinkerLoad*> LinkersToDetach;
		GetLoaders(LinkersToDetach);
		for (auto Linker : LinkersToDetach)
		{
			// Detach linker, also removes from array and sets LinkerRoot to NULL.
			Linker->LoadAndDetachAllBulkData();
			Linker->Detach();
			RemoveLinker(Linker);
		}
	}
}

void FLinkerManager::DissociateImportsAndForcedExports()
{
	int32& ImportCount = FUObjectThreadContext::Get().ImportCount;
	if (ImportCount != 0)
	{
		// In cooked builds linkers don't stick around long enough to make this worthwhile
		TSet<FLinkerLoad*> LocalLoadersWithNewImports;
		GetLoadersWithNewImportsAndEmpty(LocalLoadersWithNewImports);
		if (LocalLoadersWithNewImports.Num())
		{
			for (FLinkerLoad* Linker : LocalLoadersWithNewImports)
			{
				for (int32 ImportIndex = 0; ImportIndex < Linker->ImportMap.Num(); ImportIndex++)
				{
					FObjectImport& Import = Linker->ImportMap[ImportIndex];
					if (Import.XObject && !Import.XObject->IsNative())
					{
						Import.XObject = nullptr;
					}
					Import.SourceLinker = nullptr;
					// when the SourceLinker is reset, the SourceIndex must also be reset, or recreating
					// an import that points to a redirector will fail to find the redirector
					Import.SourceIndex = INDEX_NONE;
				}
			}
		}
		ImportCount = 0;
	}

	int32& ForcedExportCount = FUObjectThreadContext::Get().ForcedExportCount;
	if (ForcedExportCount)
	{		
		TSet<FLinkerLoad*> LocalLoaders;
		GetLoaders(LocalLoaders);
		for (FLinkerLoad* Linker : LocalLoaders)
		{
			//@todo optimization: only dissociate exports for loaders that had forced exports created
			//@todo optimization: since the last time this function was called.
			for (auto& Export : Linker->ExportMap)
			{
				if (Export.Object && Export.bForcedExport)
				{
					Export.Object->SetLinker(nullptr, INDEX_NONE);
					Export.Object = nullptr;
				}
			}
		}	
		ForcedExportCount = 0;
	}	
}

void FLinkerManager::DeleteLinkers()
{
	check(IsInGameThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FLinkerManager_DeleteLinkers);

	TArray<FLinkerLoad*> CleanupArray;
	{
#if THREADSAFE_UOBJECTS
		FScopeLock PendingCleanupListLock(&PendingCleanupListCritical);
#endif
		CleanupArray = PendingCleanupList.Array();
		PendingCleanupList.Empty();
	}

	// Note that even though DeleteLinkers can only be called on the main thread,
	// we store the IsDeletingLinkers in TLS so that we're sure nothing on
	// another thread can delete linkers except FLinkerManager at the time
	// we enter this loop.
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	ThreadContext.IsDeletingLinkers = true;
	for (FLinkerLoad* Linker : CleanupArray)
	{
		delete Linker;
	}
	ThreadContext.IsDeletingLinkers = false;
}

void FLinkerManager::RemoveLinker(FLinkerLoad* Linker)
{
#if THREADSAFE_UOBJECTS
	FScopeLock PendingCleanupListLock(&PendingCleanupListCritical);
#endif
	if (Linker && !PendingCleanupList.Contains(Linker))
	{
		PendingCleanupList.Add(Linker);
	}
}
