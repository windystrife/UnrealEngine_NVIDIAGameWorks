// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceBindingReference.h"
#include "LevelSequenceLegacyObjectReference.h"
#include "UObject/Package.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneFwd.h"
#include "PackageName.h"
#include "Engine/World.h"

FLevelSequenceBindingReference::FLevelSequenceBindingReference(UObject* InObject, UObject* InContext)
{
	check(InContext && InObject);

	if (!InContext->IsA<UWorld>() && InObject->IsIn(InContext))
	{
		ObjectPath = InObject->GetPathName(InContext);
	}
	else
	{
		UPackage* ObjectPackage = InObject->GetOutermost();
		if (!ensure(ObjectPackage))
		{
			return;
		}

		FString PackageName = ObjectPackage->GetName();
#if WITH_EDITORONLY_DATA
		// If this is being set from PIE we need to remove the pie prefix and point to the editor object
		if (ObjectPackage->PIEInstanceID != INDEX_NONE)
		{
			FString PIEPrefix = FString::Printf(PLAYWORLD_PACKAGE_PREFIX TEXT("_%d_"), ObjectPackage->PIEInstanceID);
			PackageName.ReplaceInline(*PIEPrefix, TEXT(""));
		}
#endif
		
		FString FullPath = PackageName + TEXT(".") + InObject->GetPathName(ObjectPackage);
		ExternalObjectPath = FSoftObjectPath(FullPath);
	}
}

UObject* FLevelSequenceBindingReference::Resolve(UObject* InContext) const
{
	if (ExternalObjectPath.IsNull())
	{
		return FindObject<UObject>(InContext, *ObjectPath, false);
	}
	else
	{
		FSoftObjectPath TempPath = ExternalObjectPath;

#if WITH_EDITORONLY_DATA
		int32 ContextPlayInEditorID = InContext ? InContext->GetOutermost()->PIEInstanceID : INDEX_NONE;

		if (ContextPlayInEditorID != INDEX_NONE)
		{
			// We have an override PIE id, so set the global before entering
			TGuardValue<int32> PIEGuard(GPlayInEditorID, ContextPlayInEditorID);
			TempPath.FixupForPIE();
		}
		else
		{
			TempPath.FixupForPIE();
		}
#endif

		return TempPath.ResolveObject();
	}
}

void FLevelSequenceBindingReference::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading() && !PackageName_DEPRECATED.IsEmpty())
	{
		// This was saved as two strings, combine into one soft object path so it handles PIE and redirectors properly
		FString FullPath = PackageName_DEPRECATED + TEXT(".") + ObjectPath;

		ExternalObjectPath.SetPath(FullPath);
		ObjectPath.Reset();
		PackageName_DEPRECATED.Reset();
	}
}

UObject* ResolveByPath(UObject* InContext, const FString& InObjectPath)
{
	if (!InObjectPath.IsEmpty())
	{
		if (UObject* FoundObject = FindObject<UObject>(InContext, *InObjectPath, false))
		{
			return FoundObject;
		}

		if (UObject* FoundObject = FindObject<UObject>(ANY_PACKAGE, *InObjectPath, false))
		{
			return FoundObject;
		}
	}

	return nullptr;
}

UObject* FLevelSequenceLegacyObjectReference::Resolve(UObject* InContext) const
{
	if (ObjectId.IsValid() && InContext != nullptr)
	{
		int32 PIEInstanceID = InContext->GetOutermost()->PIEInstanceID;
		FUniqueObjectGuid FixedUpId = PIEInstanceID == -1 ? ObjectId : ObjectId.FixupForPIE(PIEInstanceID);

		if (PIEInstanceID != -1 && FixedUpId == ObjectId)
		{
			UObject* FoundObject = ResolveByPath(InContext, ObjectPath);
			if (FoundObject)
			{
				return FoundObject;
			}

			UE_LOG(LogMovieScene, Warning, TEXT("Attempted to resolve object with a PIE instance that has not been fixed up yet. This is probably due to a streamed level not being available yet."));
			return nullptr;
		}
		FLazyObjectPtr LazyPtr;
		LazyPtr = FixedUpId;

		if (UObject* FoundObject = LazyPtr.Get())
		{
			return FoundObject;
		}
	}

	return ResolveByPath(InContext, ObjectPath);
}

bool FLevelSequenceObjectReferenceMap::Serialize(FArchive& Ar)
{
	int32 Num = Map.Num();
	Ar << Num;

	if (Ar.IsLoading())
	{
		while(Num-- > 0)
		{
			FGuid Key;
			Ar << Key;

			FLevelSequenceLegacyObjectReference Value;
			Ar << Value;

			Map.Add(Key, Value);
		}
	}
	else if (Ar.IsSaving() || Ar.IsCountingMemory() || Ar.IsObjectReferenceCollector())
	{
		for (auto& Pair : Map)
		{
			Ar << Pair.Key;
			Ar << Pair.Value;
		}
	}
	return true;
}

bool FLevelSequenceBindingReferences::HasBinding(const FGuid& ObjectId) const
{
	return BindingIdToReferences.Contains(ObjectId);
}

void FLevelSequenceBindingReferences::AddBinding(const FGuid& ObjectId, UObject* InObject, UObject* InContext)
{
	BindingIdToReferences.FindOrAdd(ObjectId).References.Emplace(InObject, InContext);
}

void FLevelSequenceBindingReferences::RemoveBinding(const FGuid& ObjectId)
{
	BindingIdToReferences.Remove(ObjectId);
}

void FLevelSequenceBindingReferences::ResolveBinding(const FGuid& ObjectId, UObject* InContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	const FLevelSequenceBindingReferenceArray* ReferenceArray = BindingIdToReferences.Find(ObjectId);
	if (!ReferenceArray)
	{
		return;
	}

	for (const FLevelSequenceBindingReference& Reference : ReferenceArray->References)
	{
		UObject* ResolvedObject = Reference.Resolve(InContext);

		// if the resolved object does not have a valid world (e.g. world is being torn down), dont resolve
		if (ResolvedObject && ResolvedObject->GetWorld())
		{
			OutObjects.Add(ResolvedObject);
		}
	}
}


UObject* FLevelSequenceObjectReferenceMap::ResolveBinding(const FGuid& ObjectId, UObject* InContext) const
{
	const FLevelSequenceLegacyObjectReference* Reference = Map.Find(ObjectId);
	UObject* ResolvedObject = Reference ? Reference->Resolve(InContext) : nullptr;
	if (ResolvedObject != nullptr)
	{
		// if the resolved object does not have a valid world (e.g. world is being torn down), dont resolve
		return ResolvedObject->GetWorld() != nullptr ? ResolvedObject : nullptr;
	}
	return nullptr;
}