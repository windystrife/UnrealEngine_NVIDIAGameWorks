// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StringTable.h"
#include "StringTableCore.h"
#include "StringTableRegistry.h"
#include "UObject/SoftObjectPtr.h"
#include "PackageName.h"
#include "GCObject.h"
#include "Misc/ScopeLock.h"
#include "Templates/Casts.h"
#include "SlateApplicationBase.h"
#include "Serialization/PropertyLocalizationDataGathering.h"

#if WITH_EDITORONLY_DATA
namespace
{
	static void GatherStringTableForLocalization(const UObject* const Object, FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
	{
		FStringTableConstRef StringTable = CastChecked<UStringTable>(Object)->GetStringTable();

		auto FindOrAddTextData = [&](const FString& InText) -> FGatherableTextData&
		{
			check(!InText.IsEmpty());

			FTextSourceData SourceData;
			SourceData.SourceString = InText;

			auto& GatherableTextDataArray = PropertyLocalizationDataGatherer.GetGatherableTextDataArray();
			FGatherableTextData* GatherableTextData = GatherableTextDataArray.FindByPredicate([&](const FGatherableTextData& Candidate)
			{
				return Candidate.NamespaceName.Equals(StringTable->GetNamespace(), ESearchCase::CaseSensitive)
					&& Candidate.SourceData.SourceString.Equals(SourceData.SourceString, ESearchCase::CaseSensitive)
					&& Candidate.SourceData.SourceStringMetaData == SourceData.SourceStringMetaData;
			});
			if (!GatherableTextData)
			{
				GatherableTextData = &GatherableTextDataArray[GatherableTextDataArray.AddDefaulted()];
				GatherableTextData->NamespaceName = StringTable->GetNamespace();
				GatherableTextData->SourceData = SourceData;
			}

			return *GatherableTextData;
		};

		const FString SourceLocation = Object->GetPathName();

		StringTable->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString) -> bool
		{
			if (!InSourceString.IsEmpty())
			{
				FGatherableTextData& GatherableTextData = FindOrAddTextData(InSourceString);

				FTextSourceSiteContext& SourceSiteContext = GatherableTextData.SourceSiteContexts[GatherableTextData.SourceSiteContexts.AddDefaulted()];
				SourceSiteContext.KeyName = InKey;
				SourceSiteContext.SiteDescription = SourceLocation;
				SourceSiteContext.IsEditorOnly = false;
				SourceSiteContext.IsOptional = false;

				StringTable->EnumerateMetaData(InKey, [&](const FName InMetaDataId, const FString& InMetaData)
				{
					SourceSiteContext.InfoMetaData.SetStringField(InMetaDataId.ToString(), InMetaData);
					return true; // continue enumeration
				});
			}

			return true; // continue enumeration
		});
	}
}
#endif

class FStringTableEngineBridge : public IStringTableEngineBridge, public FGCObject
{
public:
	static void Initialize()
	{
		static FStringTableEngineBridge Instance;
		InstancePtr = &Instance;
	}

private:
	//~ IStringTableEngineInterop interface
	virtual void RedirectAndLoadStringTableAssetImpl(FName& InOutTableId, const EStringTableLoadingPolicy InLoadingPolicy) override
	{
		FSoftObjectPath StringTableAssetReference = GetAssetReference(InOutTableId);
		if (StringTableAssetReference.IsValid())
		{
			UStringTable* StringTableAsset = Cast<UStringTable>(StringTableAssetReference.ResolveObject());
			if ((!StringTableAsset && InLoadingPolicy != EStringTableLoadingPolicy::Find) || (StringTableAsset && StringTableAsset->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad) && InLoadingPolicy == EStringTableLoadingPolicy::FindOrFullyLoad))
			{
				StringTableAsset = Cast<UStringTable>(StringTableAssetReference.TryLoad());
			}
			if (StringTableAsset)
			{
				InOutTableId = StringTableAsset->GetStringTableId();

				// Prevent the string table from being GC'd
				{
					FScopeLock KeepAliveStringTablesLock(&KeepAliveStringTablesCS);
					KeepAliveStringTables.AddUnique(StringTableAsset);
				}
			}
		}
	}

	virtual void CollectStringTableAssetReferencesImpl(const FName InTableId, FArchive& InAr) override
	{
		check(InAr.IsObjectReferenceCollector());

		UStringTable* StringTableAsset = FStringTableRegistry::Get().FindStringTableAsset(InTableId);
		InAr << StringTableAsset;
	}

	static FSoftObjectPath GetAssetReference(const FName InTableId)
	{
		const FString StringTableAssetName = InTableId.ToString();

		FString StringTablePackageName = StringTableAssetName;
		{
			int32 DotIndex = INDEX_NONE;
			if (StringTablePackageName.FindChar(TEXT('.'), DotIndex))
			{
				StringTablePackageName = StringTablePackageName.Left(DotIndex);
			}
		}

		FSoftObjectPath StringTableAssetReference;
		if (FPackageName::IsValidLongPackageName(StringTablePackageName, /*bIncludeReadOnlyRoots*/true) && FPackageName::DoesPackageExist(StringTablePackageName))
		{
			StringTableAssetReference.SetPath(StringTableAssetName);
		}
		
		return StringTableAssetReference;
	}

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FScopeLock KeepAliveStringTablesLock(&KeepAliveStringTablesCS);
		Collector.AddReferencedObjects(KeepAliveStringTables);
	}

private:
	/** Array of string table assets that have been loaded and should be kept alive */
	TArray<UStringTable*> KeepAliveStringTables;

	/** Critical section preventing concurrent modification of KeepAliveStringTables */
	mutable FCriticalSection KeepAliveStringTablesCS;
};

UStringTable::UStringTable()
	: StringTable(FStringTable::NewStringTable())
	, StringTableId(*GetPathName())
{
	StringTable->SetOwnerAsset(this);
	StringTable->IsLoaded(!HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));

	StringTable->SetNamespace(GetName());

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FStringTableRegistry::Get().RegisterStringTable(GetStringTableId(), StringTable.ToSharedRef());
	}

#if WITH_EDITORONLY_DATA
	{ static const FAutoRegisterLocalizationDataGatheringCallback AutomaticRegistrationOfLocalizationGatherer(UStringTable::StaticClass(), &GatherStringTableForLocalization); }
#endif
}

void UStringTable::InitializeEngineBridge()
{
	FStringTableEngineBridge::Initialize();
}

void UStringTable::FinishDestroy()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FStringTableRegistry::Get().UnregisterStringTable(GetStringTableId());
	}
	StringTable.Reset();

	Super::FinishDestroy();
}

void UStringTable::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	StringTable->Serialize(Ar);
}

void UStringTable::PostLoad()
{
	Super::PostLoad();

	StringTable->IsLoaded(true);

	if (FSlateApplicationBase::IsInitialized())
	{
		// Ensure all invalidation panels are updated now that the string data is loaded
		FSlateApplicationBase::Get().InvalidateAllWidgets();
	}
}

bool UStringTable::Rename(const TCHAR* NewName, UObject* NewOuter, ERenameFlags Flags)
{
	const bool bRenamed = Super::Rename(NewName, NewOuter, Flags);
	if (bRenamed && !HasAnyFlags(RF_ClassDefaultObject))
	{
		FStringTableRegistry::Get().UnregisterStringTable(GetStringTableId());
		StringTableId = *GetPathName();
		FStringTableRegistry::Get().RegisterStringTable(GetStringTableId(), StringTable.ToSharedRef());
	}
	return bRenamed;
}

FName UStringTable::GetStringTableId() const
{
	return StringTableId;
}

FStringTableConstRef UStringTable::GetStringTable() const
{
	return StringTable.ToSharedRef();
}

FStringTableRef UStringTable::GetMutableStringTable() const
{
	return StringTable.ToSharedRef();
}
