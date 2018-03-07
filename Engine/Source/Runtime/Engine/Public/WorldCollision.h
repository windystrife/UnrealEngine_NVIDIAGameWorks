// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Structs that are used for Async Trace functionality
// Mostly used by a batch of traces that you don't need a result right away

#pragma once 

#include "CoreMinimal.h"
#include "Async/TaskGraphInterfaces.h"
#include "CollisionQueryParams.h"

struct FOverlapDatum;
struct FTraceDatum;

/** Trace Data Structs that are used for Async Trace */

/** Trace Handle - unique ID struct that is returned once trace is requested for tracking purpose **/
struct FTraceHandle
{
	/** Handle is created by FrameNumber + Index of the request */
	union
	{
		uint64	_Handle;
		struct  
		{
			uint32 FrameNumber;
			uint32 Index;
		} _Data;
	};

	FTraceHandle() : _Handle(0) {};
	FTraceHandle(uint32 InFrameNumber, uint32 InIndex) 
	{
		_Data.FrameNumber = InFrameNumber;
		_Data.Index = InIndex;
	}

	friend inline uint32 GetTypeHash(const FTraceHandle& Handle)
	{
		return GetTypeHash(Handle._Handle);
	}

	bool operator==(FTraceHandle const& Other) const
	{
		return Other._Handle == _Handle;
	}

};

/** Types of Collision Shapes that are used by Trace **/
namespace ECollisionShape
{
	enum Type
	{
		Line,
		Box,
		Sphere,
		Capsule
	};
};

/** Collision Shapes that supports Sphere, Capsule, Box, or Line **/
struct FCollisionShape
{
	ECollisionShape::Type ShapeType;

	static FORCEINLINE CONSTEXPR float MinBoxExtent()				{ return KINDA_SMALL_NUMBER; }
	static FORCEINLINE CONSTEXPR float MinSphereRadius()			{ return KINDA_SMALL_NUMBER; }
	static FORCEINLINE CONSTEXPR float MinCapsuleRadius()			{ return KINDA_SMALL_NUMBER; }
	static FORCEINLINE CONSTEXPR float MinCapsuleAxisHalfHeight()	{ return KINDA_SMALL_NUMBER; }

	/** Union that supports up to 3 floats **/
	union
	{
		struct 
		{
			float HalfExtentX;
			float HalfExtentY;
			float HalfExtentZ;
		} Box;

		struct 
		{
			float Radius;
		} Sphere;

		struct 
		{
			float Radius;
			float HalfHeight;
		} Capsule;
	};

	FCollisionShape()
	{
		ShapeType = ECollisionShape::Line;
	}
 
	/** Is the shape currently a Line (Default)? */
	bool IsLine() const
	{
		return ShapeType == ECollisionShape::Line;
	}
	
	/** Is the shape currently a box? */
	bool IsBox() const
	{
		return ShapeType == ECollisionShape::Box;
	}
		
	/** Is the shape currently a sphere? */
	bool IsSphere() const
	{
		return ShapeType == ECollisionShape::Sphere;
	}	
		
	/** Is the shape currently a capsule? */
	bool IsCapsule() const
	{
		return ShapeType == ECollisionShape::Capsule;
	}

	/** Utility function to Set Box and dimension */
	void SetBox(const FVector& HalfExtent)
	{
		ShapeType = ECollisionShape::Box;
		Box.HalfExtentX = HalfExtent.X;		
		Box.HalfExtentY = HalfExtent.Y;		
		Box.HalfExtentZ = HalfExtent.Z;		
	}

	/** Utility function to set Sphere with Radius */
	void SetSphere(const float Radius)
	{
		ShapeType = ECollisionShape::Sphere;
		Sphere.Radius = Radius;
	}

	/** Utility function to set Capsule with Radius and Half Height */
	void SetCapsule(const float Radius, const float HalfHeight)
	{
		ShapeType = ECollisionShape::Capsule;
		Capsule.Radius = Radius;
		Capsule.HalfHeight = HalfHeight;
	}

	/** Utility function to set Capsule from Extent data */
	void SetCapsule(const FVector& Extent)
	{
		ShapeType = ECollisionShape::Capsule;
		Capsule.Radius = FMath::Max(Extent.X, Extent.Y);
		Capsule.HalfHeight = Extent.Z;
	}
	
	/** Return true if nearly zero. If so, it will back out and use line trace instead */
	bool IsNearlyZero() const
	{
		switch (ShapeType)
		{
		case ECollisionShape::Box:
			{	
				return (Box.HalfExtentX <= FCollisionShape::MinBoxExtent() && Box.HalfExtentY <= FCollisionShape::MinBoxExtent() && Box.HalfExtentZ <= FCollisionShape::MinBoxExtent());
			}
		case  ECollisionShape::Sphere:
			{
				return (Sphere.Radius <= FCollisionShape::MinSphereRadius());
			}
		case ECollisionShape::Capsule:
			{
				// @Todo check height? It didn't check before, so I'm keeping this way for time being
				return (Capsule.Radius <= FCollisionShape::MinCapsuleRadius());
			}
		}

		return true;
	}

	/** Utility function to return Extent of the shape */
	FVector GetExtent() const
	{
		switch(ShapeType)
		{
		case ECollisionShape::Box:
			{
				return FVector(Box.HalfExtentX, Box.HalfExtentY, Box.HalfExtentZ);
			}
		case  ECollisionShape::Sphere:
			{
				return FVector(Sphere.Radius, Sphere.Radius,  Sphere.Radius);
			}
		case ECollisionShape::Capsule:
			{
				// @Todo check height? It didn't check before, so I'm keeping this way for time being
				return FVector(Capsule.Radius, Capsule.Radius, Capsule.HalfHeight);
			}
		}

		return FVector::ZeroVector;
	}

	/** Get distance from center of capsule to center of sphere ends */
	float GetCapsuleAxisHalfLength() const
	{
		ensure (ShapeType == ECollisionShape::Capsule);
		return FMath::Max<float>(Capsule.HalfHeight - Capsule.Radius, FCollisionShape::FCollisionShape::MinCapsuleAxisHalfHeight());
	}

	/** Utility function to get Box Extention */
	FVector GetBox() const
	{
		return FVector(Box.HalfExtentX, Box.HalfExtentY, Box.HalfExtentZ);
	}

	/** Utility function to get Sphere Radius */
	const float GetSphereRadius() const
	{
		return Sphere.Radius;
	}

	/** Utility function to get Capsule Radius */
	const float GetCapsuleRadius() const
	{
		return Capsule.Radius;
	}

	/** Utility function to get Capsule Half Height */
	const float GetCapsuleHalfHeight() const
	{
		return Capsule.HalfHeight;
	}

	/** Used by engine in multiple places. Since LineShape doesn't need any dimension, declare once and used by all codes. */
	static struct FCollisionShape LineShape;

	/** Static utility function to make a box */
	static FCollisionShape MakeBox(const FVector& BoxHalfExtent)
	{
		FCollisionShape BoxShape;
		BoxShape.SetBox(BoxHalfExtent);
		return BoxShape;
	}

	/** Static utility function to make a sphere */
	static FCollisionShape MakeSphere(const float SphereRadius)
	{
		FCollisionShape SphereShape;
		SphereShape.SetSphere(SphereRadius);
		return SphereShape;
	}

	/** Static utility function to make a capsule */
	static FCollisionShape MakeCapsule(const float CapsuleRadius, const float CapsuleHalfHeight)
	{
		FCollisionShape CapsuleShape;
		CapsuleShape.SetCapsule(CapsuleRadius, CapsuleHalfHeight);
		return CapsuleShape;
	}

	/** Static utility function to make a capsule */
	static FCollisionShape MakeCapsule(const FVector& Extent)
	{
		FCollisionShape CapsuleShape;
		CapsuleShape.SetCapsule(Extent);
		return CapsuleShape;
	}
};

/** 
 * Sets of Collision Parameters to run the async trace
 * 
 * It includes basic Query Parameter, Response Parameter, Object Query Parameter as well as Shape of collision testing 
 *
 */
struct FCollisionParameters
{
	/** Collision Trace Parameters **/
	struct FCollisionQueryParams		CollisionQueryParam;
	struct FCollisionResponseParams		ResponseParam;
	struct FCollisionObjectQueryParams	ObjectQueryParam;

	/** Contains Collision Shape data including dimension of the shape **/
	struct FCollisionShape CollisionShape;

	FCollisionParameters()
		: CollisionQueryParam(NAME_None, TStatId())
	{
	}
};

/** 
 * Base Async Trace Data Struct for both overlap and trace 
 * 
 * Contains basic data that will need for handling trace 
 * such as World, Collision parameters and so on. 
 */
struct FBaseTraceDatum
{
	/** Physics World this trace will run in ***/
	TWeakObjectPtr<UWorld> PhysWorld;

	/** Collection of collision parameters */
	FCollisionParameters	CollisionParams;

	/** Collsion Trace Channel that this trace is running*/
	ECollisionChannel TraceChannel;

	/** Framecount when requested is made*/
	uint32	FrameNumber; 

	/** User data */
	uint32	UserData;

	FBaseTraceDatum() {};

	/** Set functions for each Shape type */
	void Set(UWorld * World, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& Param, const struct FCollisionResponseParams &InResponseParam, const struct FCollisionObjectQueryParams& InObjectQueryParam, 
		ECollisionChannel Channel, uint32 InUserData, int32 FrameCounter);
};

struct FTraceDatum;
struct FOverlapDatum;

/**
 * This is Trace/Sweep Delegate that can be used if you'd like to get notified whenever available
 * Otherwise, you'll have to query manually using your TraceHandle 
 * 
 * @param	FTraceHandle	TraceHandle that is returned when requested
 * @param	FTraceDatum		TraceDatum that includes input/output
 */
DECLARE_DELEGATE_TwoParams( FTraceDelegate, const FTraceHandle&, FTraceDatum &);
/**
 * This is Overlap Delegate that can be used if you'd like to get notified whenever available
 * Otherwise, you'll have to query manually using your TraceHandle
 * 
 * @param	FTHandle		TraceHandle that is returned when requestsed
 * @param	FOverlapDatum	OverlapDatum that includes input/output
 */
DECLARE_DELEGATE_TwoParams( FOverlapDelegate, const FTraceHandle&, FOverlapDatum &);

/** Enum to indicate type of test to perfom */
enum class EAsyncTraceType : uint8
{
	/** Return whether the trace succeeds or fails (using bBlockingHit flag on FHitResult), but gives no info about what you hit or where. Good for fast occlusion tests. */
	Test,
	/** Returns a single blocking hit */
	Single,
	/** Returns a single blocking hit, plus any overlapping hits up to that point */
	Multi
};

/**
 * Trace/Sweep Data structure for async trace
 * 
 * This saves request information by main thread and result will be filled up by worker thread
 */
struct FTraceDatum : public FBaseTraceDatum
{
	/** 
	 * Input of the trace/sweep request. Filled up by main thread 
	 * 
	 * Start/End of the trace
	 * The Shape is defined in FBaseTraceDatum
	 */
	FVector Start;
	FVector End;
	/** Delegate to be set if you want Delegate to be called when the output is available. Filled up by requester (main thread) **/
	FTraceDelegate Delegate;

	/** Output of the overlap request. Filled up by worker thread */
	TArray<struct FHitResult> OutHits;

	/** Whether to do test, single or multi test */
	EAsyncTraceType TraceType;

	FTraceDatum() {}

	/** Set Trace Datum for each shape type **/
	FTraceDatum(UWorld * World, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Param, const struct FCollisionResponseParams &InResponseParam, const struct FCollisionObjectQueryParams& InObjectQueryParam, 
		ECollisionChannel Channel, uint32 InUserData, EAsyncTraceType InTraceType,	const FVector& InStart, const FVector& InEnd, FTraceDelegate * InDelegate, int32 FrameCounter)
	{
		Set(World, CollisionShape, Param, InResponseParam, InObjectQueryParam, Channel, InUserData, FrameCounter);
		Start = InStart;
		End = InEnd;
		if (InDelegate)
		{
			Delegate = *InDelegate;
		}
		else
		{
			Delegate.Unbind();
		}
		OutHits.Reset();
		TraceType = InTraceType;
	}
};

/**
 * Overlap Data structure for async trace
 * 
 * This saves request information by main thread and result will be filled up by worker thread
 */
struct FOverlapDatum : FBaseTraceDatum
{
	/** 
	 * Input of the overlap request. Filled up by main thread 
	 *
	 * Position/Rotation data of overlap request. 
	 * The Shape is defined in FBaseTraceDatum
	 */
	FVector Pos;
	FQuat	Rot;
	/** Delegate to be set if you want Delegate to be called when the output is available. Filled up by requester (main thread) **/
	FOverlapDelegate Delegate;

	/** Output of the overlap request. Filled up by worker thread */
	TArray<struct FOverlapResult> OutOverlaps;

	FOverlapDatum() {}

	FOverlapDatum(UWorld * World, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Param, const struct FCollisionResponseParams &InResponseParam, const struct FCollisionObjectQueryParams& InObjectQueryParam, 
		ECollisionChannel Channel, uint32 InUserData,
		const FVector& InPos, const FQuat& InRot, FOverlapDelegate * InDelegate, int32 FrameCounter)
	{
		Set(World, CollisionShape, Param, InResponseParam, InObjectQueryParam, Channel, InUserData, FrameCounter);
		Pos = InPos;
		Rot = InRot;
		if (InDelegate)
		{
			Delegate = *InDelegate;
		}
		else
		{
			Delegate.Unbind();
		}
		OutOverlaps.Reset();
	}
};

#define ASYNC_TRACE_BUFFER_SIZE 64

/**
 * Trace Data that one Thread can handle per type. For now ASYNC_TRACE_BUFFER_SIZE is 64. 
 */
template <typename T>
struct TTraceThreadData
{
	T Buffer[ASYNC_TRACE_BUFFER_SIZE];
};

/** 
 * Contains all Async Trace Result for one frame. 
 * 
 * We use double buffer for trace data pool. FrameNumber % 2 = is going to be the one collecting NEW data 
 * Check FWorldAsyncTraceState to see how this is used. For now it is only double buffer, but it can be more. 
 */
struct AsyncTraceData : FNoncopyable
{
	/**
	 * Data Buffer for each trace type - one for Trace/Sweep and one for Overlap
	 *
	 * FTraceThreadData is one atomic data size for thread
	 * so once that's filled up, this will be sent to thread
	 * if you need more, it will allocate new FTraceThreadData and add to TArray
	 *
	 * however when we calculate index of buffer, we calculate continuously, 
	 * meaning if TraceData(1).Buffer[50] will have 1*ASYNC_TRACE_BUFFER_SIZE + 50 as index
	 * in order to give UNIQUE INDEX to every data in this buffer
	 */
	TArray<TUniquePtr<TTraceThreadData<FTraceDatum>>>			TraceData;
	TArray<TUniquePtr<TTraceThreadData<FOverlapDatum>>>			OverlapData;

	/** Datum entries in TraceData are persistent for efficiency. This is the number of them that are actually in use (rather than TraceData.Num()). */
	int32 NumQueuedTraceData;
	/** Datum entries in OverlapData are persistent for efficiency. This is the number of them that are actually in use (rather than OverlapData.Num()). */
	int32 NumQueuedOverlapData;

	/**
	 * if Execution is all done, set this to be true
	 * 
	 * when reinitialize bAsyncAllowed is true, once execution is done, this will set to be false
	 * this is to find cases where execution is already done but another request is made again within the same frame. 
	 */
	bool					bAsyncAllowed; 

	/**  Thread completion event for batch **/
	FGraphEventArray		AsyncTraceCompletionEvent;

	AsyncTraceData() 
		: NumQueuedTraceData(0)
		, NumQueuedOverlapData(0)
		, bAsyncAllowed(false)
	{}
};


extern ECollisionChannel DefaultCollisionChannel;
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogCollision, Warning, All);
