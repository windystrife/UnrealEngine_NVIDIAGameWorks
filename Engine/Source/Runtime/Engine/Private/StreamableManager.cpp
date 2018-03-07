// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/StreamableManager.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/PackageName.h"
#include "UObjectThreadContext.h"
#include "HAL/IConsoleManager.h"
#include "Tickable.h"

DEFINE_LOG_CATEGORY_STATIC(LogStreamableManager, Log, All);

// Default to 1 frame, this will cause the delegates to go off on the next tick to avoid recursion issues. Set higher to fake disk lag
static int32 GStreamableDelegateDelayFrames = 1;
static FAutoConsoleVariableRef CVarStreamableDelegateDelayFrames(
	TEXT("s.StreamableDelegateDelayFrames"),
	GStreamableDelegateDelayFrames,
	TEXT("Number of frames to delay StreamableManager delegates "),
	ECVF_Default
);

/** Helper class that defers streamable manager delegates until the next frame */
class FStreamableDelegateDelayHelper : public FTickableGameObject
{
public:

	/** Adds a delegate to deferred list */
	void AddDelegate(const FStreamableDelegate& Delegate, TSharedPtr<FStreamableHandle> AssociatedHandle)
	{
		DataLock.Lock();

		PendingDelegates.Emplace(Delegate, AssociatedHandle);

		DataLock.Unlock();
	}

	/** Calls all delegates, call from synchronous flushes */
	void FlushDelegates()
	{
		while (PendingDelegates.Num() > 0)
		{
			Tick(0.0f);
		}
	}

	// FTickableGameObject interface

	void Tick(float DeltaTime) override
	{
		if (PendingDelegates.Num() == 0)
		{
			return;
		}

		DataLock.Lock();

		TArray<FPendingDelegate> DelegatesToCall;
		DelegatesToCall.Reserve(PendingDelegates.Num());
		for (int32 DelegateIndex = 0; DelegateIndex < PendingDelegates.Num(); DelegateIndex++)
		{
			if (--PendingDelegates[DelegateIndex].DelayFrames <= 0)
			{
				// Add to call array and remove from tracking one
				DelegatesToCall.Emplace(PendingDelegates[DelegateIndex].Delegate, PendingDelegates[DelegateIndex].RelatedHandle);
				PendingDelegates.RemoveAt(DelegateIndex--);
			}
		}

		DataLock.Unlock();

		for (const FPendingDelegate& PendingDelegate : DelegatesToCall)
		{
			// Call delegates, these may add other deferred delegates
			PendingDelegate.Delegate.ExecuteIfBound();
		}

		// When DelegatesToCall falls out of scope it may delete the referenced handles
	}

	virtual bool IsTickable() const override
	{
		return true;
	}

	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}

	virtual bool IsTickableInEditor() const
	{
		return true;
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FStreamableDelegateDelayHelper, STATGROUP_Tickables);
	}

private:

	struct FPendingDelegate
	{
		/** Delegate to call on next frame */
		FStreamableDelegate Delegate;

		/** Handle related to delegates, needs to keep these around to avoid things GCing before the user callback goes off. This may be null */
		TSharedPtr<FStreamableHandle> RelatedHandle;

		/** Frames left to delay */
		int32 DelayFrames;

		FPendingDelegate(const FStreamableDelegate& InDelegate, TSharedPtr<FStreamableHandle> InHandle)
			: Delegate(InDelegate)
			, RelatedHandle(InHandle)
			, DelayFrames(GStreamableDelegateDelayFrames)
		{}
	};

	TArray<FPendingDelegate> PendingDelegates;

	FCriticalSection DataLock;
};

static FStreamableDelegateDelayHelper* StreamableDelegateDelayHelper = nullptr;

bool FStreamableHandle::BindCompleteDelegate(FStreamableDelegate NewDelegate)
{
	if (!IsLoadingInProgress())
	{
		// Too late!
		return false;
	}

	CompleteDelegate = NewDelegate;
	return true;
}

bool FStreamableHandle::BindCancelDelegate(FStreamableDelegate NewDelegate)
{
	if (!IsLoadingInProgress())
	{
		// Too late!
		return false;
	}

	CancelDelegate = NewDelegate;
	return true;
}

bool FStreamableHandle::BindUpdateDelegate(FStreamableUpdateDelegate NewDelegate)
{
	if (!IsLoadingInProgress())
	{
		// Too late!
		return false;
	}

	UpdateDelegate = NewDelegate;
	return true;
}

EAsyncPackageState::Type FStreamableHandle::WaitUntilComplete(float Timeout)
{
	if (HasLoadCompleted())
	{
		return EAsyncPackageState::Complete;
	}

	// We need to recursively start any stalled handles
	TArray<TSharedRef<FStreamableHandle>> HandlesToStart;

	HandlesToStart.Add(AsShared());

	for (int32 i = 0; i < HandlesToStart.Num(); i++)
	{
		TSharedRef<FStreamableHandle> Handle = HandlesToStart[i];

		if (Handle->IsStalled())
		{
			// If we were stalled, start us now to avoid deadlocks
			UE_LOG(LogStreamableManager, Warning, TEXT("FStreamableHandle::WaitUntilComplete called on stalled handle %s, forcing load even though resources may not have been acquired yet"), *Handle->GetDebugName());
			Handle->StartStalledHandle();
		}

		for (TSharedPtr<FStreamableHandle> ChildHandle : Handle->ChildHandles)
		{
			if (ChildHandle.IsValid())
			{
				HandlesToStart.Add(ChildHandle.ToSharedRef());
			}
		}
	}

	EAsyncPackageState::Type State = ProcessAsyncLoadingUntilComplete([this]() { return HasLoadCompleted(); }, Timeout);
	
	if (State == EAsyncPackageState::Complete)
	{
		ensureMsgf(HasLoadCompleted() || WasCanceled(), TEXT("WaitUntilComplete failed for streamable handle %s, async loading is done but handle is not complete"), *GetDebugName());
	}

	return State;
}

void FStreamableHandle::GetRequestedAssets(TArray<FSoftObjectPath>& AssetList) const
{
	AssetList = RequestedAssets;

	// Check child handles
	for (TSharedPtr<FStreamableHandle> ChildHandle : ChildHandles)
	{
		TArray<FSoftObjectPath> ChildAssetList;

		ChildHandle->GetRequestedAssets(ChildAssetList);

		for (const FSoftObjectPath& ChildRef : ChildAssetList)
		{
			AssetList.AddUnique(ChildRef);
		}
	}
}

UObject* FStreamableHandle::GetLoadedAsset() const
{
	TArray<UObject *> LoadedAssets;

	GetLoadedAssets(LoadedAssets);

	if (LoadedAssets.Num() > 0)
	{
		return LoadedAssets[0];
	}

	return nullptr;
}

void FStreamableHandle::GetLoadedAssets(TArray<UObject *>& LoadedAssets) const
{
	if (HasLoadCompleted())
	{
		for (const FSoftObjectPath& Ref : RequestedAssets)
		{
			// Try manager, should be faster and will handle redirects better
			if (IsActive())
			{
				LoadedAssets.Add(OwningManager->GetStreamed(Ref));
			}
			else
			{
				LoadedAssets.Add(Ref.ResolveObject());
			}
		}

		// Check child handles
		for (TSharedPtr<FStreamableHandle> ChildHandle : ChildHandles)
		{
			for (const FSoftObjectPath& Ref : ChildHandle->RequestedAssets)
			{
				// Try manager, should be faster and will handle redirects better
				if (IsActive())
				{
					LoadedAssets.Add(OwningManager->GetStreamed(Ref));
				}
				else
				{
					LoadedAssets.Add(Ref.ResolveObject());
				}
			}
		}
	}
}

void FStreamableHandle::GetLoadedCount(int32& LoadedCount, int32& RequestedCount) const
{
	RequestedCount = RequestedAssets.Num();
	LoadedCount = RequestedCount - StreamablesLoading;
	
	// Check child handles
	for (TSharedPtr<FStreamableHandle> ChildHandle : ChildHandles)
	{
		int32 ChildRequestedCount = 0;
		int32 ChildLoadedCount = 0;

		ChildHandle->GetLoadedCount(ChildLoadedCount, ChildRequestedCount);

		LoadedCount += ChildLoadedCount;
		RequestedCount += ChildRequestedCount;
	}
}

float FStreamableHandle::GetProgress() const
{
	if (HasLoadCompleted())
	{
		return 1.0f;
	}

	int32 Loaded, Total;

	GetLoadedCount(Loaded, Total);

	return (float)Loaded / Total;
}

FStreamableManager* FStreamableHandle::GetOwningManager() const
{
	check(IsInGameThread());

	if (IsActive())
	{
		return OwningManager;
	}
	return nullptr;
}

void FStreamableHandle::CancelHandle()
{
	check(IsInGameThread());

	if (bReleased || bCanceled || !OwningManager)
	{
		// Too late to cancel it
		return;
	}

	if (bLoadCompleted)
	{
		ReleaseHandle();
		return;
	}

	bCanceled = true;

	TSharedRef<FStreamableHandle> SharedThis = AsShared();

	ExecuteDelegate(CancelDelegate, SharedThis);
	UnbindDelegates();

	// Remove from referenced list
	for (const FSoftObjectPath& AssetRef : RequestedAssets)
	{
		OwningManager->RemoveReferencedAsset(AssetRef, SharedThis);
	}

	// Remove from explicit list
	OwningManager->ManagedActiveHandles.Remove(SharedThis);

	// Remove child handles
	for (TSharedPtr<FStreamableHandle> ChildHandle : ChildHandles)
	{
		ChildHandle->ParentHandles.Remove(SharedThis);
	}

	ChildHandles.Empty();

	OwningManager = nullptr;

	if (ParentHandles.Num() > 0)
	{
		// Update any meta handles that are still active. Copy the array first as elements may be removed from original while iterating
		TArray<TWeakPtr<FStreamableHandle>> ParentHandlesCopy = ParentHandles;
		for (TWeakPtr<FStreamableHandle> WeakHandle : ParentHandlesCopy)
		{
			TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();

			if (Handle.IsValid())
			{
				Handle->UpdateCombinedHandle();
			}
		}
	}
}

void FStreamableHandle::ReleaseHandle()
{
	check(IsInGameThread());

	if (bReleased || bCanceled)
	{
		// Too late to release it
		return;
	}

	check(OwningManager);

	if (bLoadCompleted)
	{
		bReleased = true;

		TSharedRef<FStreamableHandle> SharedThis = AsShared();

		// Remove from referenced list
		for (const FSoftObjectPath& AssetRef : RequestedAssets)
		{
			OwningManager->RemoveReferencedAsset(AssetRef, SharedThis);
		}

		// Remove from explicit list
		OwningManager->ManagedActiveHandles.Remove(SharedThis);

		// Remove child handles
		for (TSharedPtr<FStreamableHandle> ChildHandle : ChildHandles)
		{
			ChildHandle->ParentHandles.Remove(SharedThis);
		}

		ChildHandles.Empty();

		OwningManager = nullptr;
	}
	else
	{
		// Set to release on complete
		bReleaseWhenLoaded = true;
	}
}

void FStreamableHandle::StartStalledHandle()
{
	if (!bStalled || !IsActive())
	{
		// Cannot start
		return;
	}

	check(OwningManager);

	bStalled = false;
	OwningManager->StartHandleRequests(AsShared());
}

FStreamableHandle::~FStreamableHandle()
{
	check(IsInGameThread());

	if (IsActive())
	{
		bReleased = true;
		OwningManager = nullptr;
		
		// The weak pointers in FStreamable will be nulled, but they're fixed on next GC, and actively canceling is not safe as we're halfway destroyed
	}
}

void FStreamableHandle::CompleteLoad()
{
	// Only complete if it's still active
	if (IsActive())
	{
		bLoadCompleted = true;

		ExecuteDelegate(CompleteDelegate, AsShared());
		UnbindDelegates();

		if (ParentHandles.Num() > 0)
		{
			// Update any meta handles that are still active. Copy the array first as elements may be removed from original while iterating
			TArray<TWeakPtr<FStreamableHandle>> ParentHandlesCopy = ParentHandles;
			for (TWeakPtr<FStreamableHandle> WeakHandle : ParentHandlesCopy)
			{
				TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();

				if (Handle.IsValid())
				{
					Handle->UpdateCombinedHandle();
				}
			}
		}
	}
}

void FStreamableHandle::UpdateCombinedHandle()
{
	if (!IsActive())
	{
		return;
	}

	if (!ensure(IsCombinedHandle()))
	{
		return;
	}

	// Check all our children, complete if done
	bool bAllCompleted = true;
	bool bAllCanceled = true;
	for (TSharedPtr<FStreamableHandle> ChildHandle : ChildHandles)
	{
		if (ChildHandle->IsLoadingInProgress())
		{
			bAllCompleted = false;
		}
		if (!ChildHandle->WasCanceled())
		{
			bAllCanceled = false;
		}
	}

	// If all our sub handles were canceled, cancel us. Otherwise complete us if at least one was completed and there are none in progress
	if (bAllCanceled)
	{
		if (OwningManager)
		{
			OwningManager->PendingCombinedHandles.Remove(AsShared());
		}

		CancelHandle();
	}
	else if (bAllCompleted)
	{
		if (OwningManager)
		{
			OwningManager->PendingCombinedHandles.Remove(AsShared());
		}

		CompleteLoad();

		if (bReleaseWhenLoaded)
		{
			ReleaseHandle();
		}
	}
}

void FStreamableHandle::CallUpdateDelegate()
{
	UpdateDelegate.ExecuteIfBound(AsShared());

	// Update any meta handles that are still active
	for (TWeakPtr<FStreamableHandle> WeakHandle : ParentHandles)
	{
		TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();

		if (Handle.IsValid())
		{
			Handle->CallUpdateDelegate();
		}
	}
}

void FStreamableHandle::UnbindDelegates()
{
	CancelDelegate.Unbind();
	UpdateDelegate.Unbind();
	CompleteDelegate.Unbind();
}

void FStreamableHandle::AsyncLoadCallbackWrapper(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result, FSoftObjectPath TargetName)
{
	check(IsInGameThread());

	// Needed so we can bind with a shared pointer for safety
	if (OwningManager)
	{
		OwningManager->AsyncLoadCallback(TargetName);

		if (!HasLoadCompleted())
		{
			CallUpdateDelegate();
		}
	}
	else if (!bCanceled)
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("FStreamableHandle::AsyncLoadCallbackWrapper called on request %s with result %d with no active manager!"), *DebugName, (int32)Result);
	}
}

void FStreamableHandle::ExecuteDelegate(const FStreamableDelegate& Delegate, TSharedPtr<FStreamableHandle> AssociatedHandle)
{
	if (Delegate.IsBound())
	{
		if (!StreamableDelegateDelayHelper)
		{
			StreamableDelegateDelayHelper = new FStreamableDelegateDelayHelper;
		}

		StreamableDelegateDelayHelper->AddDelegate(Delegate, AssociatedHandle);
	}
}

/** Internal object, one of these per object paths managed by this system */
struct FStreamable
{
	/** Hard Pointer to object */
	UObject* Target;
	/** If this object is currently being loaded */
	bool	bAsyncLoadRequestOutstanding;
	/** If this object failed to load, don't try again */
	bool	bLoadFailed;

	/** List of handles that are waiting for this to load */
	TArray< TSharedRef< FStreamableHandle> > LoadingHandles;

	/** List of handles that are keeping this streamable in memory */
	TArray< TWeakPtr< FStreamableHandle> > ActiveHandles;

	FStreamable()
		: Target(nullptr)
		, bAsyncLoadRequestOutstanding(false)
		, bLoadFailed(false)
	{
	}

	~FStreamable()
	{
		// Clear the loading handles 
		for (int32 i = 0; i < LoadingHandles.Num(); i++)
		{
			LoadingHandles[i]->StreamablesLoading--;
		}
		LoadingHandles.Empty();

		// Cancel active handles, this list includes the loading handles
		for (int32 i = 0; i < ActiveHandles.Num(); i++)
		{
			TSharedPtr<FStreamableHandle> ActiveHandle = ActiveHandles[i].Pin();

			if (ActiveHandle.IsValid())
			{
				// Full cancel isn't safe any more

				ActiveHandle->bCanceled = true;
				ActiveHandle->OwningManager = nullptr;

				if (!ActiveHandle->bReleased)
				{
					FStreamableHandle::ExecuteDelegate(ActiveHandle->CancelDelegate, ActiveHandle);
					ActiveHandle->UnbindDelegates();
				}
			}
		}
		ActiveHandles.Empty();
	}

	void AddLoadingRequest(TSharedRef<FStreamableHandle> NewRequest)
	{
		if (ActiveHandles.Contains(NewRequest))
		{
			ensureMsgf(!ActiveHandles.Contains(NewRequest), TEXT("Duplicate item added to StreamableRequest"));
			return;
		}
		
		ActiveHandles.Add(NewRequest);

		LoadingHandles.Add(NewRequest);
		NewRequest->StreamablesLoading++;
	}
};

FStreamableManager::FStreamableManager()
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FStreamableManager::OnPreGarbageCollect);
	bForceSynchronousLoads = false;
}

FStreamableManager::~FStreamableManager()
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().RemoveAll(this);

	for (TStreamableMap::TIterator It(StreamableItems); It; ++It)
	{
		delete It.Value();
		It.RemoveCurrent();
	}
}

void FStreamableManager::OnPreGarbageCollect()
{
	TSet<FSoftObjectPath> RedirectsToRemove;

	// Remove any streamables with no active handles, as GC may have freed them
	for (TStreamableMap::TIterator It(StreamableItems); It; ++It)
	{
		FStreamable* Existing = It.Value();

		// Remove invalid handles, the weak pointers may be pointing to removed handles
		for (int32 i = Existing->ActiveHandles.Num() - 1; i >= 0; i--)
		{
			TSharedPtr<FStreamableHandle> ActiveHandle = Existing->ActiveHandles[i].Pin();

			if (!ActiveHandle.IsValid())
			{
				Existing->ActiveHandles.RemoveAtSwap(i, 1, false);
			}
		}

		if (Existing->ActiveHandles.Num() == 0)
		{
			RedirectsToRemove.Add(It.Key());
			delete Existing;
			It.RemoveCurrent();
		}
	}

	if (RedirectsToRemove.Num() > 0)
	{
		for (TStreamableRedirects::TIterator It(StreamableRedirects); It; ++It)
		{
			if (RedirectsToRemove.Contains(It.Value().NewPath))
			{
				It.RemoveCurrent();
			}
		}
	}
}

void FStreamableManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	// If there are active streamable handles in the editor, this will cause the user to Force Delete, which is irritating but necessary because weak pointers cannot be used here
	for (TStreamableMap::TConstIterator It(StreamableItems); It; ++It)
	{
		FStreamable* Existing = It.Value();
		if (Existing->Target)
		{
			Collector.AddReferencedObject(Existing->Target);
		}
	}

	for (TStreamableRedirects::TIterator It(StreamableRedirects); It; ++It)
	{
		FRedirectedPath& Existing = It.Value();
		if (Existing.LoadedRedirector)
		{
			Collector.AddReferencedObject(Existing.LoadedRedirector);
		}
	}
}

FStreamable* FStreamableManager::FindStreamable(const FSoftObjectPath& Target) const
{
	FStreamable* Existing = StreamableItems.FindRef(Target);

	if (!Existing)
	{
		return StreamableItems.FindRef(ResolveRedirects(Target));
	}

	return Existing;
}

FStreamable* FStreamableManager::StreamInternal(const FSoftObjectPath& InTargetName, TAsyncLoadPriority Priority, TSharedRef<FStreamableHandle> Handle)
{
	check(IsInGameThread());
	UE_LOG(LogStreamableManager, Verbose, TEXT("Asynchronous load %s"), *InTargetName.ToString());

	FSoftObjectPath TargetName = ResolveRedirects(InTargetName);
	FStreamable* Existing = StreamableItems.FindRef(TargetName);
	if (Existing)
	{
		if (Existing->bAsyncLoadRequestOutstanding)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Already in progress %s"), *TargetName.ToString());
			check(!Existing->Target); // should not be a load request unless the target is invalid
			ensure(IsAsyncLoading()); // Nothing should be pending if there is no async loading happening

			// Don't return as we potentially want to sync load it
		}
		if (Existing->Target)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Already Loaded %s"), *TargetName.ToString());
			return Existing;
		}
	}
	else
	{
		Existing = StreamableItems.Add(TargetName, new FStreamable());
	}

	if (!Existing->bAsyncLoadRequestOutstanding)
	{
		FindInMemory(TargetName, Existing);
	}
	
	if (!Existing->Target)
	{
		// Disable failed flag as it may have been added at a later point
		Existing->bLoadFailed = false;

		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();

		// If async loading isn't safe or it's forced on, we have to do a sync load which will flush all async loading
		if (GIsInitialLoad || ThreadContext.IsInConstructor > 0 || bForceSynchronousLoads)
		{
			FRedirectedPath RedirectedPath;
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Static loading %s"), *TargetName.ToString());
			Existing->Target = StaticLoadObject(UObject::StaticClass(), nullptr, *TargetName.ToString());
			// need to manually detect redirectors because the above call only expects to load a UObject::StaticClass() type
			while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Existing->Target))
			{
				if (!RedirectedPath.LoadedRedirector)
				{
					RedirectedPath.LoadedRedirector = Redirector;
				}
				Existing->Target = Redirector->DestinationObject;
			}
			if (Existing->Target)
			{
				UE_LOG(LogStreamableManager, Verbose, TEXT("     Static loaded %s"), *Existing->Target->GetFullName());
				FSoftObjectPath PossiblyNewName(Existing->Target->GetPathName());
				if (PossiblyNewName != TargetName)
				{
					UE_LOG(LogStreamableManager, Verbose, TEXT("     Which redirected to %s"), *PossiblyNewName.ToString());
					RedirectedPath.NewPath = PossiblyNewName;
					StreamableRedirects.Add(TargetName, RedirectedPath);
					StreamableItems.Add(PossiblyNewName, Existing);
					StreamableItems.Remove(TargetName);
					TargetName = PossiblyNewName; // we are done with the old name
				}
			}
			else
			{
				Existing->bLoadFailed = true;
				UE_LOG(LogStreamableManager, Log, TEXT("Failed attempt to load %s"), *TargetName.ToString());
			}
			Existing->bAsyncLoadRequestOutstanding = false;
		}
		else
		{
			// We always queue a new request in case the existing one gets cancelled
			FString Package = TargetName.ToString();
			int32 FirstDot = Package.Find(TEXT("."));
			if (FirstDot != INDEX_NONE)
			{
				Package = Package.Left(FirstDot);
			}

			Existing->bAsyncLoadRequestOutstanding = true;
			Existing->bLoadFailed = false;
			LoadPackageAsync(Package, FLoadPackageAsyncDelegate::CreateSP(Handle, &FStreamableHandle::AsyncLoadCallbackWrapper, TargetName), Priority);
		}
	}
	return Existing;
}

void FStreamableManager::SimpleAsyncLoad(const FSoftObjectPath& Target, TAsyncLoadPriority Priority)
{
	RequestAsyncLoad(Target, FStreamableDelegate(), Priority, true);
}

UObject* FStreamableManager::SynchronousLoad(FSoftObjectPath const& Target)
{
	return LoadSynchronous(Target, true);
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestAsyncLoad(const TArray<FSoftObjectPath>& TargetsToStream, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority, bool bManageActiveHandle, bool bStartStalled, const FString& DebugName)
{
	// Schedule a new callback, this will get called when all related async loads are completed
	TSharedRef<FStreamableHandle> NewRequest = MakeShareable(new FStreamableHandle());
	NewRequest->CompleteDelegate = DelegateToCall;
	NewRequest->OwningManager = this;
	NewRequest->RequestedAssets = TargetsToStream;
	NewRequest->DebugName = DebugName;
	NewRequest->Priority = Priority;

	// Remove null requests

	for (int32 i = NewRequest->RequestedAssets.Num() - 1; i >= 0 ; i--)
	{
		FSoftObjectPath& TargetName = NewRequest->RequestedAssets[i];
		if (TargetName.IsNull())
		{
			// Remove null entries
			NewRequest->RequestedAssets.RemoveAt(i);
			continue;
		}
		else if (FPackageName::IsShortPackageName(TargetName.ToString()))
		{
			UE_LOG(LogStreamableManager, Error, TEXT("RequestAsyncLoad called with invalid package name %s"), *TargetName.ToString());
			NewRequest->CancelHandle();
			return nullptr;
		}
	}

	if (NewRequest->RequestedAssets.Num() == 0)
	{
		// Original array was empty or all null
		UE_LOG(LogStreamableManager, Error, TEXT("RequestAsyncLoad called with empty or only null assets!"));
		NewRequest->CancelHandle();
		return nullptr;
	} 
	else if (NewRequest->RequestedAssets.Num() != TargetsToStream.Num())
	{
		FString RequestedSet;

		for (const FSoftObjectPath& Asset : TargetsToStream)
		{
			if (!RequestedSet.IsEmpty())
			{
				RequestedSet += TEXT(", ");
			}
			RequestedSet += Asset.ToString();
		}

		// Some valid, some null
		UE_LOG(LogStreamableManager, Warning, TEXT("RequestAsyncLoad called with both valid and null assets, null assets removed from %s!"), *RequestedSet);
	}

	// Remove any duplicates
	TSet<FSoftObjectPath> TargetSet(NewRequest->RequestedAssets);

	if (TargetSet.Num() != NewRequest->RequestedAssets.Num())
	{
#if UE_BUILD_DEBUG
		FString RequestedSet;

		for (const FSoftObjectPath& Asset : NewRequest->RequestedAssets)
		{
			if (!RequestedSet.IsEmpty())
			{
				RequestedSet += TEXT(", ");
			}
			RequestedSet += Asset.ToString();
		}

		UE_LOG(LogStreamableManager, Verbose, TEXT("RequestAsyncLoad called with duplicate assets, duplicates removed from %s!"), *RequestedSet);
#endif

		NewRequest->RequestedAssets = TargetSet.Array();
	}

	if (bManageActiveHandle)
	{
		// This keeps a reference around until explicitly released
		ManagedActiveHandles.Add(NewRequest);
	}

	if (bStartStalled)
	{
		NewRequest->bStalled = true;
	}
	else
	{
		StartHandleRequests(NewRequest);
	}

	return NewRequest;
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestAsyncLoad(const FSoftObjectPath& TargetToStream, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority, bool bManageActiveHandle, bool bStartStalled, const FString& DebugName)
{
	return RequestAsyncLoad(TArray<FSoftObjectPath>{TargetToStream}, DelegateToCall, Priority, bManageActiveHandle, bStartStalled, DebugName);
}

TSharedPtr<FStreamableHandle>  FStreamableManager::RequestAsyncLoad(const TArray<FSoftObjectPath>& TargetsToStream, TFunction<void()>&& Callback, TAsyncLoadPriority Priority, bool bManageActiveHandle, bool bStartStalled, const FString& DebugName)
{
	return RequestAsyncLoad(TargetsToStream, FStreamableDelegate::CreateLambda( MoveTemp( Callback ) ), Priority, bManageActiveHandle, bStartStalled, DebugName);
}

TSharedPtr<FStreamableHandle>  FStreamableManager::RequestAsyncLoad(const FSoftObjectPath& TargetToStream, TFunction<void()>&& Callback, TAsyncLoadPriority Priority, bool bManageActiveHandle, bool bStartStalled, const FString& DebugName)
{
	return RequestAsyncLoad(TargetToStream, FStreamableDelegate::CreateLambda( MoveTemp( Callback ) ), Priority, bManageActiveHandle, bStartStalled, DebugName);
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestSyncLoad(const TArray<FSoftObjectPath>& TargetsToStream, bool bManageActiveHandle, const FString& DebugName)
{
	// If in async loading thread or from callback always do sync as recursive tick is unsafe
	// If in EDL always do sync as EDL internally avoids flushing
	// Otherwise, only do a sync load if there are no background sync loads, this is faster but will cause a sync flush
	bForceSynchronousLoads = IsInAsyncLoadingThread() || IsEventDrivenLoaderEnabled() || !IsAsyncLoading();

	// Do an async load and wait to complete. In some cases this will do a sync load due to safety issues
	TSharedPtr<FStreamableHandle> Request = RequestAsyncLoad(TargetsToStream, FStreamableDelegate(), AsyncLoadHighPriority, bManageActiveHandle, false, DebugName);

	bForceSynchronousLoads = false;

	if (Request.IsValid())
	{
		EAsyncPackageState::Type Result = Request->WaitUntilComplete();

		ensureMsgf(Result == EAsyncPackageState::Complete, TEXT("RequestSyncLoad of %s resulted in bad async load result %d"), *DebugName, Result);
		ensureMsgf(Request->HasLoadCompleted(), TEXT("RequestSyncLoad of %s completed early, not actually completed!"), *DebugName);
	}

	return Request;
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestSyncLoad(const FSoftObjectPath& TargetToStream, bool bManageActiveHandle, const FString& DebugName)
{
	return RequestSyncLoad(TArray<FSoftObjectPath>{TargetToStream}, bManageActiveHandle, DebugName);
}

void FStreamableManager::StartHandleRequests(TSharedRef<FStreamableHandle> Handle)
{
	TArray<FStreamable *> ExistingStreamables;
	ExistingStreamables.Reserve(Handle->RequestedAssets.Num());

	for (int32 i = 0; i < Handle->RequestedAssets.Num(); i++)
	{
		FStreamable* Existing = StreamInternal(Handle->RequestedAssets[i], Handle->Priority, Handle);
		check(Existing);

		ExistingStreamables.Add(Existing);
		Existing->AddLoadingRequest(Handle);
	}

	// Go through and complete loading anything that's already in memory, this may call the callback right away
	for (int32 i = 0; i < Handle->RequestedAssets.Num(); i++)
	{
		FStreamable* Existing = ExistingStreamables[i];

		if (Existing && (Existing->Target || Existing->bLoadFailed))
		{
			Existing->bAsyncLoadRequestOutstanding = false;

			CheckCompletedRequests(Handle->RequestedAssets[i], Existing);
		}
	}
}

UObject* FStreamableManager::LoadSynchronous(const FSoftObjectPath& Target, bool bManageActiveHandle, TSharedPtr<FStreamableHandle>* RequestHandlePointer)
{
	TSharedPtr<FStreamableHandle> Request = RequestSyncLoad(Target, bManageActiveHandle, FString::Printf(TEXT("LoadSynchronous of %s"), *Target.ToString()));

	if (RequestHandlePointer)
	{
		(*RequestHandlePointer) = Request;
	}

	if (Request.IsValid())
	{
		UObject* Result = Request->GetLoadedAsset();

		if (!Result)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("LoadSynchronous failed for load of %s! File is missing or there is a loading system problem"), *Target.ToString());
		}

		return Result;
	}

	return nullptr;
}

void FStreamableManager::FindInMemory( FSoftObjectPath& InOutTargetName, struct FStreamable* Existing )
{
	check(Existing);
	check(!Existing->bAsyncLoadRequestOutstanding);
	UE_LOG(LogStreamableManager, Verbose, TEXT("     Searching in memory for %s"), *InOutTargetName.ToString());
	Existing->Target = StaticFindObject(UObject::StaticClass(), nullptr, *InOutTargetName.ToString());

	if (Existing->Target && Existing->Target->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
	{
		// This can get called from PostLoad on async loaded objects, if it is we do not want to return partially loaded objects and instead want to register for their full load
		Existing->Target = nullptr;
	}

	UObjectRedirector* Redir = Cast<UObjectRedirector>(Existing->Target);
	
	FRedirectedPath RedirectedPath;
	RedirectedPath.LoadedRedirector = Redir;

	while (Redir)
	{
		Existing->Target = Redir->DestinationObject;
		Redir = Cast<UObjectRedirector>(Existing->Target);
		UE_LOG(LogStreamableManager, Verbose, TEXT("     Found redirect %s"), *Redir->GetFullName());
		if (!Existing->Target)
		{
			Existing->bLoadFailed = true;
			UE_LOG(LogStreamableManager, Warning, TEXT("Destination of redirector was not found %s -> %s."), *InOutTargetName.ToString(), *Redir->GetFullName());
		}
		else
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Redirect to %s"), *Redir->DestinationObject->GetFullName());
		}
	}
	if (Existing->Target)
	{
		FSoftObjectPath PossiblyNewName(Existing->Target->GetPathName());
		if (InOutTargetName != PossiblyNewName)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Name changed to %s"), *PossiblyNewName.ToString());
			RedirectedPath.NewPath = PossiblyNewName;
			StreamableRedirects.Add(InOutTargetName, RedirectedPath);
			StreamableItems.Add(PossiblyNewName, Existing);
			StreamableItems.Remove(InOutTargetName);
			InOutTargetName = PossiblyNewName; // we are done with the old name
		}
		UE_LOG(LogStreamableManager, Verbose, TEXT("     Found in memory %s"), *Existing->Target->GetFullName());
		Existing->bLoadFailed = false;
	}
}

void FStreamableManager::AsyncLoadCallback(FSoftObjectPath TargetName)
{
	check(IsInGameThread());

	FStreamable* Existing = FindStreamable(TargetName);

	UE_LOG(LogStreamableManager, Verbose, TEXT("Stream Complete callback %s"), *TargetName.ToString());
	if (Existing)
	{
		if (Existing->bAsyncLoadRequestOutstanding)
		{
			Existing->bAsyncLoadRequestOutstanding = false;
			if (!Existing->Target)
			{
				FindInMemory(TargetName, Existing);
			}

			CheckCompletedRequests(TargetName, Existing);
		}
		else
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("AsyncLoadCallback called for %s when not waiting on a load request, was loaded early by sync load"), *TargetName.ToString());
		}
		if (Existing->Target)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("    Found target %s"), *Existing->Target->GetFullName());
		}
		else
		{
			// Async load failed to find the object
			Existing->bLoadFailed = true;
			UE_LOG(LogStreamableManager, Verbose, TEXT("    Failed async load."), *TargetName.ToString());
		}
	}
	else
	{
		UE_LOG(LogStreamableManager, Error, TEXT("Can't find streamable for %s in AsyncLoadCallback!"), *TargetName.ToString());
	}
}

void FStreamableManager::CheckCompletedRequests(const FSoftObjectPath& Target, struct FStreamable* Existing)
{
	static int32 RecursiveCount = 0;

	ensure(RecursiveCount == 0);

	RecursiveCount++;

	// Release these handles at end
	TArray<TSharedRef<FStreamableHandle>> HandlesToComplete;
	TArray<TSharedRef<FStreamableHandle>> HandlesToRelease;

	for (TSharedRef<FStreamableHandle> Handle : Existing->LoadingHandles)
	{
		ensure(Handle->WasCanceled() || Handle->OwningManager == this);

		// Decrement related requests, and call delegate if all are done and request is still active
		Handle->StreamablesLoading--;
		if (Handle->StreamablesLoading == 0)
		{
			if (Handle->bReleaseWhenLoaded)
			{
				HandlesToRelease.Add(Handle);
			}

			HandlesToComplete.Add(Handle);
		}		
	}
	Existing->LoadingHandles.Empty();

	for (TSharedRef<FStreamableHandle> Handle : HandlesToComplete)
	{
		Handle->CompleteLoad();
	}

	for (TSharedRef<FStreamableHandle> Handle : HandlesToRelease)
	{
		Handle->ReleaseHandle();
	}

	// HandlesToRelease might get deleted when function ends

	RecursiveCount--;
}

void FStreamableManager::RemoveReferencedAsset(const FSoftObjectPath& Target, TSharedRef<FStreamableHandle> Handle)
{
	if (Target.IsNull())
	{
		return;
	}

	ensureMsgf(Handle->OwningManager == this, TEXT("RemoveReferencedAsset called on wrong streamable manager for target %s"), *Target.ToString());

	FStreamable* Existing = FindStreamable(Target);

	// This should always be in the active handles list
	if (ensureMsgf(Existing, TEXT("Failed to find existing streamable for %s"), *Target.ToString()))
	{
		ensureMsgf(Existing->ActiveHandles.Remove(Handle) > 0, TEXT("Failed to remove active handle for %s"), *Target.ToString());

		// Try removing from loading list if it's still there, this won't call the callback as it's being called from cancel
		if (Existing->LoadingHandles.Remove(Handle))
		{
			Handle->StreamablesLoading--;

			if (Existing->LoadingHandles.Num() == 0)
			{
				// All requests cancelled, remove loading flag
				Existing->bAsyncLoadRequestOutstanding = false;
			}
		}
	}
}

bool FStreamableManager::IsAsyncLoadComplete(const FSoftObjectPath& Target) const
{
	// Failed loads count as success
	check(IsInGameThread());
	FStreamable* Existing = FindStreamable(Target);
	return !Existing || !Existing->bAsyncLoadRequestOutstanding;
}

UObject* FStreamableManager::GetStreamed(const FSoftObjectPath& Target) const
{
	check(IsInGameThread());
	FStreamable* Existing = FindStreamable(Target);
	if (Existing && Existing->Target)
	{
		return Existing->Target;
	}
	return nullptr;
}

void FStreamableManager::Unload(const FSoftObjectPath& Target)
{
	check(IsInGameThread());

	TArray<TSharedRef<FStreamableHandle>> HandleList;

	if (GetActiveHandles(Target, HandleList, true))
	{
		for (TSharedRef<FStreamableHandle> Handle : HandleList)
		{
			Handle->ReleaseHandle();
		}
	}
	else
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("Attempt to unload %s, but it isn't loaded"), *Target.ToString());
	}
}

TSharedPtr<FStreamableHandle> FStreamableManager::CreateCombinedHandle(const TArray<TSharedPtr<FStreamableHandle> >& ChildHandles, const FString& DebugName)
{
	if (!ensure(ChildHandles.Num() > 0))
	{
		return nullptr;
	}

	TSharedRef<FStreamableHandle> NewRequest = MakeShareable(new FStreamableHandle());
	NewRequest->OwningManager = this;
	NewRequest->bIsCombinedHandle = true;
	NewRequest->DebugName = DebugName;

	for (TSharedPtr<FStreamableHandle> ChildHandle : ChildHandles)
	{
		if (!ensure(ChildHandle.IsValid()))
		{
			return nullptr;
		}

		ensure(ChildHandle->OwningManager == this);

		ChildHandle->ParentHandles.Add(NewRequest);
		NewRequest->ChildHandles.Add(ChildHandle);
	}

	// Add to pending list so these handles don't free when not referenced
	PendingCombinedHandles.Add(NewRequest);

	// This may already be complete
	NewRequest->UpdateCombinedHandle();

	return NewRequest;
}

bool FStreamableManager::GetActiveHandles(const FSoftObjectPath& Target, TArray<TSharedRef<FStreamableHandle>>& HandleList, bool bOnlyManagedHandles) const
{
	check(IsInGameThread());
	FStreamable* Existing = FindStreamable(Target);
	if (Existing && Existing->ActiveHandles.Num() > 0)
	{
		for (TWeakPtr<FStreamableHandle> WeakHandle : Existing->ActiveHandles)
		{
			TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();

			if (Handle.IsValid())
			{
				ensure(Handle->OwningManager == this);

				TSharedRef<FStreamableHandle> HandleRef = Handle.ToSharedRef();
				if (!bOnlyManagedHandles || ManagedActiveHandles.Contains(HandleRef))
				{
					HandleList.Add(HandleRef);
				}
			}
		}
		return HandleList.Num() > 0;
	}

	return false;
}

FSoftObjectPath FStreamableManager::ResolveRedirects(const FSoftObjectPath& Target) const
{
	FRedirectedPath const* Redir = StreamableRedirects.Find(Target);
	if (Redir)
	{
		check(Target != Redir->NewPath);
		UE_LOG(LogStreamableManager, Verbose, TEXT("Redirected %s -> %s"), *Target.ToString(), *Redir->NewPath.ToString());
		return Redir->NewPath;
	}
	return Target;
}


