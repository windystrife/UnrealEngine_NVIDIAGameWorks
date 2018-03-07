// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "UObject/SoftObjectPtr.h"
#include "GCObject.h"

/** Defines FStreamableDelegate delegate interface */
DECLARE_DELEGATE(FStreamableDelegate);
DECLARE_DELEGATE_OneParam(FStreamableUpdateDelegate, TSharedRef<struct FStreamableHandle>);

/** A handle to a synchronous or async load. As long as the handle is Active, loaded assets will stay in memory */
struct ENGINE_API FStreamableHandle : public TSharedFromThis<FStreamableHandle, ESPMode::Fast>
{
	/** If this request has finished loading, meaning all available assets were loaded and delegate was called. If assets failed to load they will still be missing */
	bool HasLoadCompleted() const
	{
		return bLoadCompleted;
	}

	/** If this request was cancelled. Assets may still have been loaded, but delegate will not be called */
	bool WasCanceled() const
	{
		return bCanceled;
	}

	/** True if load is still ongoing and we haven't been cancelled */
	bool IsLoadingInProgress() const
	{
		return !bLoadCompleted && !bCanceled;
	}

	/** If this handle is still active, meaning it wasn't canceled or released */
	bool IsActive() const
	{
		return !bCanceled && !bReleased;
	}

	/** If this handle is stalled and waiting for another event to occur before it is actually requested */
	bool IsStalled() const
	{
		return bStalled;
	}

	/** Returns true if this is a combined handle that depends on child handles. */
	bool IsCombinedHandle() const
	{
		return bIsCombinedHandle;
	}

	/** Returns the debug name for this handle. */
	const FString& GetDebugName() const
	{
		return DebugName;
	}

	/** Returns the streaming priority. */
	TAsyncLoadPriority GetPriority() const
	{
		return Priority;
	}

	/** Release this handle. This can be called from normal gameplay code to indicate that the loaded assets are no longer needed. Will be called implicitly if all shared pointers
	to this handle are destroyed. If called before the completion delegate, the release will be delayed until after completion. */
	void ReleaseHandle();

	/** Cancel a request, callable from within the manager or externally. Will stop delegate from being called */
	void CancelHandle();

	/** Tells a stalled handle to start its actual request. */
	void StartStalledHandle();

	/** Bind delegate that is called when load completes, only works if loading is in progress. This will overwrite any already bound delegate! */
	bool BindCompleteDelegate(FStreamableDelegate NewDelegate);

	/** Bind delegate that is called if handle is canceled, only works if loading is in progress. This will overwrite any already bound delegate! */
	bool BindCancelDelegate(FStreamableDelegate NewDelegate);

	/** Bind delegate that is called periodically as delegate updates, only works if loading is in progress. This will overwrite any already bound delegate! */
	bool BindUpdateDelegate(FStreamableUpdateDelegate NewDelegate);

	/** Blocks until the requested assets have loaded. This pushes the requested asset to the top of the priority list, but does not flush all async loading, usually resulting
	in faster completion than a LoadObject call. */
	EAsyncPackageState::Type WaitUntilComplete(float Timeout = 0.0f);

	/** Gets list of assets references this load was started with. This will be the paths before redirectors, and not all of these are guaranteed to be loaded */
	void GetRequestedAssets(TArray<FSoftObjectPath>& AssetList) const;

	/** Adds all loaded assets if load has succeeded. Some entries will be null if loading failed */
	void GetLoadedAssets(TArray<UObject *>& LoadedAssets) const;

	/** Returns first asset in requested asset list, if it's been successfully loaded. This will fail if the asset failed to load */
	UObject* GetLoadedAsset() const;

	/** Returns number of assets that have completed loading out of initial list, failed loads will count as loaded */
	void GetLoadedCount(int32& LoadedCount, int32& RequestedCount) const;

	/** Returns progress as a value between 0.0 and 1.0. */
	float GetProgress() const;

	/** Get the StreamableManager for this handle */
	struct FStreamableManager* GetOwningManager() const;

	/** Calls a StreamableDelegate, this may defer execution to the next frame depending on CVars */
	static void ExecuteDelegate(const FStreamableDelegate& Delegate, TSharedPtr<FStreamableHandle> AssociatedHandle = nullptr);

	/** Destructor */
	~FStreamableHandle();

	/** Not safe to copy or duplicate */
	FStreamableHandle(const FStreamableHandle&) = delete;
	FStreamableHandle& operator=(const FStreamableHandle&) = delete;
private:
	friend struct FStreamableManager;
	friend struct FStreamable;

	/** Called from manager to complete the request */
	void CompleteLoad();

	/** Callback when async load finishes, it's here so we can use a shared pointer for callback safety */
	void AsyncLoadCallbackWrapper(const FName& PackageName, UPackage* LevelPackage, EAsyncLoadingResult::Type Result, FSoftObjectPath TargetName);

	/** Called on meta handle when a child handle has completed/canceled */
	void UpdateCombinedHandle();

	/** Call to call the update delegate if bound, will propagate to parents */
	void CallUpdateDelegate();

	/** Unbind all of our delegates, called after a cancel/load */
	void UnbindDelegates();

	/** True if this request has finished loading. It may still be active, or it may have been released */
	bool bLoadCompleted;

	/** True if this request was released, which will stop it from keeping hard GC references */
	bool bReleased;

	/** True if this request was explicitly canceled, which stops it from calling the completion delegate and immediately releases it */
	bool bCanceled;

	/** True if this handle has been created but not yet actually requested. This handle is probably waiting for a resource like a chunk to be available */
	bool bStalled;

	/** If true, this handle will be released when it finishes loading */
	bool bReleaseWhenLoaded;

	/** If true, this handle will be released when it finishes loading */
	bool bIsCombinedHandle;

	/** Delegate to call when streaming is completed */
	FStreamableDelegate CompleteDelegate;

	/** Delegate to call when streaming is canceled */
	FStreamableDelegate CancelDelegate;

	/** Called periodically during streaming to update progress UI */
	FStreamableUpdateDelegate UpdateDelegate;

	/** Name of this handle, passed in by caller to help in debugging */
	FString DebugName;

	/** The async priority for this request */
	TAsyncLoadPriority Priority;

	/** How many FStreamables is this waiting on to finish loading */
	int32 StreamablesLoading;

	/** List of assets that were referenced by this handle */
	TArray<FSoftObjectPath> RequestedAssets;

	/** List of handles this depends on, these will keep the child references alive */
	TArray<TSharedPtr<FStreamableHandle> > ChildHandles;

	/** Backpointer to handles that depend on this */
	TArray<TWeakPtr<FStreamableHandle> > ParentHandles;

	/** This is set at the time of creation, and will be cleared when request completes or is canceled */
	struct FStreamableManager* OwningManager;

	FStreamableHandle()
		: bLoadCompleted(false)
		, bReleased(false)
		, bCanceled(false)
		, bStalled(false)
		, bReleaseWhenLoaded(false)
		, bIsCombinedHandle(false)
		, Priority(0)
		, StreamablesLoading(0)
		, OwningManager(nullptr)
	{
	}
};

/** A native class for managing streaming assets in and keeping them in memory. AssetManager is the global singleton version of this with blueprint access */
struct ENGINE_API FStreamableManager : public FGCObject
{
	// Default priority for all async loads
	static const TAsyncLoadPriority DefaultAsyncLoadPriority = 0;
	// Priority to try and load immediately
	static const TAsyncLoadPriority AsyncLoadHighPriority = 100;

	/** 
	 * This is the primary streamable operation. Requests streaming of one or more target objects. When complete, a delegate function is called. Returns a Streamable Handle.
	 *
	 * @param TargetsToStream		Assets to load off disk
	 * @param DelegateToCall		Delegate to call when load finishes. Will be called on the next tick if asset is already loaded, or many seconds later
	 * @param Priority				Priority to pass to the streaming system, higher priority will be loaded first
	 * @param bManageActiveHandle	If true, the manager will keep the streamable handle active until explicitly released
	 * @param bStartStalled			If true, the handle will start in a stalled state and will not attempt to actually async load until StartStalledHandle is called on it
	 * @param DebugName				Name of this handle, will be reported in debug tools
	 */
	TSharedPtr<FStreamableHandle> RequestAsyncLoad(const TArray<FSoftObjectPath>& TargetsToStream, FStreamableDelegate DelegateToCall = FStreamableDelegate(), TAsyncLoadPriority Priority = DefaultAsyncLoadPriority, bool bManageActiveHandle = false, bool bStartStalled = false, const FString& DebugName = TEXT("RequestAsyncLoad ArrayDelegate"));
	TSharedPtr<FStreamableHandle> RequestAsyncLoad(const FSoftObjectPath& TargetToStream, FStreamableDelegate DelegateToCall = FStreamableDelegate(), TAsyncLoadPriority Priority = DefaultAsyncLoadPriority, bool bManageActiveHandle = false, bool bStartStalled = false, const FString& DebugName = TEXT("RequestAsyncLoad SingleDelegate"));

	/** Lambda Wrappers. Be aware that Callback may go off multiple seconds in the future. */
	TSharedPtr<FStreamableHandle> RequestAsyncLoad(const TArray<FSoftObjectPath>& TargetsToStream, TFunction<void()>&& Callback, TAsyncLoadPriority Priority = DefaultAsyncLoadPriority, bool bManageActiveHandle = false, bool bStartStalled = false, const FString& DebugName = TEXT("RequestAsyncLoad ArrayLambda"));
	TSharedPtr<FStreamableHandle> RequestAsyncLoad(const FSoftObjectPath& TargetToStream, TFunction<void()>&& Callback, TAsyncLoadPriority Priority = DefaultAsyncLoadPriority, bool bManageActiveHandle = false, bool bStartStalled = false, const FString& DebugName = TEXT("RequestAsyncLoad SingleLambda"));

	/** 
	 * Synchronously load a set of assets, and return a handle. This can be very slow and may stall the game thread for several seconds.
	 * 
	 * @param TargetsToStream		Assets to load off disk
	 * @param bManageActiveHandle	If true, the manager will keep the streamable handle active until explicitly released
	 * @param DebugName				Name of this handle, will be reported in debug tools
	 */
	TSharedPtr<FStreamableHandle> RequestSyncLoad(const TArray<FSoftObjectPath>& TargetsToStream, bool bManageActiveHandle = false, const FString& DebugName = TEXT("RequestSyncLoad Array"));
	TSharedPtr<FStreamableHandle> RequestSyncLoad(const FSoftObjectPath& TargetToStream, bool bManageActiveHandle = false, const FString& DebugName = TEXT("RequestSyncLoad Single"));

	/** 
	 * Synchronously load the referred asset and return the loaded object, or nullptr if it can't be found. This can be very slow and may stall the game thread for several seconds.
	 * 
	 * @param Target				Specific asset to load off disk
	 * @param bManageActiveHandle	If true, the manager will keep the streamable handle active until explicitly released
	 * @param RequestHandlePointer	If non-null, this will set the handle to the handle used to make this request. This useful for later releasing the handle
	 */
	UObject* LoadSynchronous(const FSoftObjectPath& Target, bool bManageActiveHandle = false, TSharedPtr<FStreamableHandle>* RequestHandlePointer = nullptr);

	/** Typed wrappers */
	template< typename T >
	T* LoadSynchronous(const FSoftObjectPath& Target, bool bManageActiveHandle = false, TSharedPtr<FStreamableHandle>* RequestHandlePointer = nullptr)
	{
		return Cast<T>(LoadSynchronous(Target, bManageActiveHandle, RequestHandlePointer) );
	}

	template< typename T >
	T* LoadSynchronous(const TSoftObjectPtr<T>& Target, bool bManageActiveHandle = false, TSharedPtr<FStreamableHandle>* RequestHandlePointer = nullptr)
	{
		return Cast<T>(LoadSynchronous(Target.ToSoftObjectPath(), bManageActiveHandle, RequestHandlePointer));
	}

	template< typename T >
	TSubclassOf<T> LoadSynchronous(const TSoftClassPtr<T>& Target, bool bManageActiveHandle = false, TSharedPtr<FStreamableHandle>* RequestHandlePointer = nullptr)
	{
		TSubclassOf<T> ReturnClass;
		ReturnClass = Cast<UClass>(LoadSynchronous(Target.ToSoftObjectPath(), bManageActiveHandle, RequestHandlePointer));
		return ReturnClass;
	}

	/** 
	 * Creates a combined handle, which will wait for other handles to complete before completing.
	 * The child handles will be held as hard references as long as this handle is active.
	 * 
	 * @param ChildHandles			List of handles to wrap into this one
	 * @param DebugName				Name of this handle, will be reported in debug tools
	 */
	TSharedPtr<FStreamableHandle> CreateCombinedHandle(const TArray<TSharedPtr<FStreamableHandle> >& ChildHandles, const FString& DebugName = TEXT("CreateCombinedHandle"));

	/** 
	 * Gets list of handles that are directly referencing this asset, returns true if any found.
	 * Combined Handles will not be returned by this function.
	 *
	 * @param Target					Asset to get active handles for 
	 * @param HandleList				Fill in list of active handles
	 * @param bOnlyManagedHandles		If true, only return handles that are managed by this manager, other active handles are skipped
	 */
	bool GetActiveHandles(const FSoftObjectPath& Target, TArray<TSharedRef<FStreamableHandle>>& HandleList, bool bOnlyManagedHandles = false) const;

	/** Returns true if all pending async loads have finished for this target */
	bool IsAsyncLoadComplete(const FSoftObjectPath& Target) const;

	/** This will release any managed active handles pointing to the target string asset reference, even if they include other requested assets in the same load */
	void Unload(const FSoftObjectPath& Target);

	DEPRECATED(4.16, "Call LoadSynchronous with bManageActiveHandle=true instead if you want the manager to keep the handle alive")
	UObject* SynchronousLoad(FSoftObjectPath const& Target);

	template< typename T >
	DEPRECATED(4.16, "Call LoadSynchronous with bManageActiveHandle=true instead if you want the manager to keep the handle alive")
	T* SynchronousLoadType(FSoftObjectPath const& Target)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Cast< T >(SynchronousLoad(Target));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	DEPRECATED(4.16, "Call RequestAsyncLoad with bManageActiveHandle=true instead if you want the manager to keep the handle alive")
	void SimpleAsyncLoad(const FSoftObjectPath& Target, TAsyncLoadPriority Priority = DefaultAsyncLoadPriority);

	DEPRECATED(4.16, "AddStructReferencedObjects is no longer necessary, as it is a GCObject now")
	void AddStructReferencedObjects(class FReferenceCollector& Collector) const {}

	/** Add referenced objects to stop them from GCing */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	FStreamableManager();
	~FStreamableManager();

	/** Not safe to copy or duplicate */
	FStreamableManager(const FStreamableManager&) = delete;
	FStreamableManager& operator=(const FStreamableManager&) = delete;
private:
	friend FStreamableHandle;

	void RemoveReferencedAsset(const FSoftObjectPath& Target, TSharedRef<FStreamableHandle> Handle);
	void StartHandleRequests(TSharedRef<FStreamableHandle> Handle);
	FSoftObjectPath ResolveRedirects(const FSoftObjectPath& Target) const;
	void FindInMemory(FSoftObjectPath& InOutTarget, struct FStreamable* Existing);
	struct FStreamable* FindStreamable(const FSoftObjectPath& Target) const;
	struct FStreamable* StreamInternal(const FSoftObjectPath& Target, TAsyncLoadPriority Priority, TSharedRef<FStreamableHandle> Handle);
	UObject* GetStreamed(const FSoftObjectPath& Target) const;
	void CheckCompletedRequests(const FSoftObjectPath& Target, struct FStreamable* Existing);

	void OnPreGarbageCollect();
	void AsyncLoadCallback(FSoftObjectPath Request);

	/** Map of paths to streamable objects, this will be the post-redirector name */
	typedef TMap<FSoftObjectPath, struct FStreamable*> TStreamableMap;
	TStreamableMap StreamableItems;

	/** Map of redirected paths */
	struct FRedirectedPath
	{
		/** The path of the non-redirector object loaded */
		FSoftObjectPath NewPath;

		/** The redirector that was loaded off disk, need to keep this around for path resolves until this redirect is freed */
		UObjectRedirector* LoadedRedirector;

		FRedirectedPath() : LoadedRedirector(nullptr) {}
	};
	typedef TMap<FSoftObjectPath, FRedirectedPath> TStreamableRedirects;
	TStreamableRedirects StreamableRedirects;

	/** List of explicitly held handles */
	TArray<TSharedRef<FStreamableHandle>> ManagedActiveHandles;

	/** List of combined handles that are still loading, these need to be here to avoid them being deleted */
	TArray<TSharedRef<FStreamableHandle>> PendingCombinedHandles;

	/** If True, temporarily force synchronous loading */
	bool bForceSynchronousLoads;
};


