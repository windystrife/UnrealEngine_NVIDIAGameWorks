// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 Resource.h: Template for pooling resources using buckets.
 =============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "TickableObjectRenderThread.h"

/** A templated pool for resources that can only be freed at a 'safe' point in the frame. */
template<typename ResourceType, class ResourcePoolPolicy, class ResourceCreationArguments>
class TResourcePool
{
public:
	/** Constructor */
	TResourcePool()
	{
		
	}
	
	/** Constructor with policy argument
	 * @param InPolicy An initialised policy object
	 */
	TResourcePool(ResourcePoolPolicy InPolicy)
	: Policy(InPolicy)
	{
		
	}
	
	/** Destructor */
	virtual ~TResourcePool()
	{
		DrainPool(true);
	}
	
	/** Gets the size a pooled object will use when constructed from the pool.
	 * @param Args the argument object for construction.
	 * @returns The size for a pooled object created with Args
	 */
	uint32 PooledSizeForCreationArguments(ResourceCreationArguments Args)
	{
		const uint32 BucketIndex = Policy.GetPoolBucketIndex(Args);
		return Policy.GetPoolBucketSize(BucketIndex);
	}
	
	/** Creates a pooled resource.
	 * @param Args the argument object for construction.
	 * @returns An initialised resource.
	 */
	ResourceType CreatePooledResource(ResourceCreationArguments Args)
	{
		// Find the appropriate bucket based on size
		const uint32 BucketIndex = Policy.GetPoolBucketIndex(Args);
		TArray<FPooledResource>& PoolBucket = ResourceBuckets[BucketIndex];
		if (PoolBucket.Num() > 0)
		{
			// Reuse the last entry in this size bucket
			return PoolBucket.Pop().Resource;
		}
		else
		{
			// Nothing usable was found in the free pool, create a new resource
			return Policy.CreateResource(Args);
		}
	}
	
	/** Release a resource back into the pool.
	 * @param Resource The resource to return to the pool
	 */
	void ReleasePooledResource(const ResourceType& Resource)
	{
		FPooledResource NewEntry;
		NewEntry.Resource = Resource;
		NewEntry.FrameFreed = GFrameNumberRenderThread;
		NewEntry.CreationArguments = Policy.GetCreationArguments(Resource);
		
		// Add to this frame's array of free resources
		const int32 SafeFrameIndex = GFrameNumberRenderThread % ResourcePoolPolicy::NumSafeFrames;
		const uint32 BucketIndex = Policy.GetPoolBucketIndex(NewEntry.CreationArguments);
		
		SafeResourceBuckets[SafeFrameIndex][BucketIndex].Add(NewEntry);
	}
	
	/** Drain the pool of freed resources that need to be culled or prepared for reuse.
	 * @param bForceDrainAll Clear the pool of all free resources, rather than obeying the policy
	 */
	void DrainPool(bool bForceDrainAll)
	{
		uint32 NumToCleanThisFrame = ResourcePoolPolicy::NumToDrainPerFrame;
		uint32 CullAfterFramesNum = ResourcePoolPolicy::CullAfterFramesNum;
		
		if(!bForceDrainAll)
		{
			// Index of the bucket that is now old enough to be reused
			const int32 SafeFrameIndex = (GFrameNumberRenderThread + 1) % ResourcePoolPolicy::NumSafeFrames;
			
			// Merge the bucket into the free pool array
			for (int32 BucketIndex = 0; BucketIndex < ResourcePoolPolicy::NumPoolBuckets; BucketIndex++)
			{
				ResourceBuckets[BucketIndex].Append(SafeResourceBuckets[SafeFrameIndex][BucketIndex]);
				SafeResourceBuckets[SafeFrameIndex][BucketIndex].Reset();
			}
		}
		else
		{
			for(int32 FrameIndex = 0; FrameIndex < ResourcePoolPolicy::NumSafeFrames; FrameIndex++)
			{
				// Merge the bucket into the free pool array
				for (int32 BucketIndex = 0; BucketIndex < ResourcePoolPolicy::NumPoolBuckets; BucketIndex++)
				{
					ResourceBuckets[BucketIndex].Append(SafeResourceBuckets[FrameIndex][BucketIndex]);
					SafeResourceBuckets[FrameIndex][BucketIndex].Reset();
				}
			}
		}
		
		// Clean a limited number of old entries to reduce hitching when leaving a large level
		for (int32 BucketIndex = 0; BucketIndex < ResourcePoolPolicy::NumPoolBuckets; BucketIndex++)
		{
			for (int32 EntryIndex = ResourceBuckets[BucketIndex].Num() - 1; EntryIndex >= 0; EntryIndex--)
			{
				FPooledResource& PoolEntry = ResourceBuckets[BucketIndex][EntryIndex];
				
				// Clean entries that are unlikely to be reused
				if ((GFrameNumberRenderThread - PoolEntry.FrameFreed) > CullAfterFramesNum || bForceDrainAll)
				{
					Policy.FreeResource(ResourceBuckets[BucketIndex][EntryIndex].Resource);
					
					ResourceBuckets[BucketIndex].RemoveAtSwap(EntryIndex);
					
					--NumToCleanThisFrame;
					if (!bForceDrainAll && NumToCleanThisFrame == 0)
					{
						break;
					}
				}
			}
			
			if (!bForceDrainAll && NumToCleanThisFrame == 0)
			{
				break;
			}
		}
	}
	
private:
	/** Pooling policy for this resource */
	ResourcePoolPolicy Policy;
	
	// Describes a Resource in the free pool.
	struct FPooledResource
	{
		/** The actual resource */
		ResourceType Resource;
		/** The arguments used to create the resource */
		ResourceCreationArguments CreationArguments;
		/** The frame the resource was freed */
		uint32 FrameFreed;
	};
	
	// Pool of free Resources, indexed by bucket for constant size search time.
	TArray<FPooledResource> ResourceBuckets[ResourcePoolPolicy::NumPoolBuckets];
	
	// Resources that have been freed more recently than NumSafeFrames ago.
	TArray<FPooledResource> SafeResourceBuckets[ResourcePoolPolicy::NumSafeFrames][ResourcePoolPolicy::NumPoolBuckets];
};

/** A resource pool that automatically handles render-thread resources */
template<typename ResourceType, class ResourcePoolPolicy, class ResourceCreationArguments>
class TRenderResourcePool : public TResourcePool<ResourceType, ResourcePoolPolicy, ResourceCreationArguments>, public FTickableObjectRenderThread, public FRenderResource
{
public:
	/** Constructor */
	TRenderResourcePool() :
		FTickableObjectRenderThread(false)
	{
	}
	
	/** Constructor with policy argument
	 * @param InPolicy An initialised policy object
	 */
	TRenderResourcePool(ResourcePoolPolicy InPolicy) :
		TResourcePool<ResourceType, ResourcePoolPolicy, ResourceCreationArguments>(InPolicy),
		FTickableObjectRenderThread(false)
	{
	}
	
	/** Destructor */
	virtual ~TRenderResourcePool()
	{
	}
	
	/** Creates a pooled resource.
	 * @param Args the argument object for construction.
	 * @returns An initialised resource or the policy's NullResource if not initialised.
	 */
	ResourceType CreatePooledResource(ResourceCreationArguments Args)
	{
		ensure(IsInRenderingThread());

		if (IsInitialized())
		{
			return TResourcePool<ResourceType, ResourcePoolPolicy, ResourceCreationArguments>::CreatePooledResource(Args);
		}
		else
		{
			return ResourceType();
		}
	}
	
	/** Release a resource back into the pool.
	 * @param Resource The resource to return to the pool
	 */
	void ReleasePooledResource(ResourceType Resource)
	{
		ensure(IsInRenderingThread());

		if (IsInitialized())
		{
			TResourcePool<ResourceType, ResourcePoolPolicy, ResourceCreationArguments>::ReleasePooledResource(Resource);
		}
	}
	
public: // From FTickableObjectRenderThread
	virtual void Tick( float DeltaTime ) override
	{
		ensure(IsInRenderingThread());

		TResourcePool<ResourceType, ResourcePoolPolicy, ResourceCreationArguments>::DrainPool(false);
	}
	
	virtual bool IsTickable() const override
	{
		return true;
	}
	
	virtual bool NeedsRenderingResumedForRenderingThreadTick() const override
	{
		return true;
	}
	
public: // From FRenderResource
	virtual void InitRHI() override
	{
		FTickableObjectRenderThread::Register();
	}

	virtual void ReleaseRHI() override
	{
		FTickableObjectRenderThread::Unregister();
		TResourcePool<ResourceType, ResourcePoolPolicy, ResourceCreationArguments>::DrainPool(true);
	}
};
