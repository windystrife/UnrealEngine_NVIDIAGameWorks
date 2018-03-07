// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectAnnotation.h: Unreal object annotation template
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectArray.h"
#include "Misc/ScopeLock.h"

/**
* FUObjectAnnotationSparse is a helper class that is used to store sparse, slow, temporary, editor only, external 
* or other low priority information about UObjects.
*
* There is a notion of a default annotation and UObjects default to this annotation and this takes no storage.
* 
* Annotations are automatically cleaned up when UObjects are destroyed.
* Annotation are not "garbage collection aware", so it isn't safe to store pointers to other UObjects in an 
* annotation unless external guarantees are made such that destruction of the other object removes the
* annotation.
* @param TAnnotation type of the annotation
* @param bAutoRemove if true, annotation will automatically be removed, otherwise in non-final builds it will verify that the annotation was removed by other means prior to destruction.
**/
template<typename TAnnotation, bool bAutoRemove>
class FUObjectAnnotationSparse : public FUObjectArray::FUObjectDeleteListener
{
public:

	/**
	 * Interface for FUObjectAllocator::FUObjectDeleteListener
	 *
	 * @param Object object that has been destroyed
	 * @param Index	index of object that is being deleted
	 */
	virtual void NotifyUObjectDeleted(const UObjectBase *Object, int32 Index)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!bAutoRemove)
		{
			FScopeLock AnntationMapLock(&AnnotationMapCritical);
			// in this case we are only verifying that the external assurances of removal are met
			check(!AnnotationMap.Find(Object));
		}
		else
#endif
		{
			RemoveAnnotation(Object);
		}
	}

	/**
	 * Constructor, initializes to nothing
	 */
	FUObjectAnnotationSparse() :
		AnnotationCacheKey(NULL)
	{
		// default constructor is required to be default annotation
		check(AnnotationCacheValue.IsDefault());
	}

	/**
	 * Destructor, removes all annotations, which removes the annotation as a uobject destruction listener
	 */
	virtual ~FUObjectAnnotationSparse()
	{
		RemoveAllAnnotations();
	}

	/**
	 * Add an annotation to the annotation list. If the Annotation is the default, then the annotation is removed.
	 *
	 * @param Object		Object to annotate.
	 * @param Annotation	Annotation to associate with Object.
	 */
	void AddAnnotation(const UObjectBase *Object,TAnnotation Annotation)
	{
		check(Object);
		FScopeLock AnntationMapLock(&AnnotationMapCritical);
		AnnotationCacheKey = Object;
		AnnotationCacheValue = Annotation;
		if (Annotation.IsDefault())
		{
			RemoveAnnotation(Object); // adding the default annotation is the same as removing an annotation
		}
		else
		{
			if (AnnotationMap.Num() == 0)
			{
				// we are adding the first one, so if we are auto removing or verifying removal, register now
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (bAutoRemove)
#endif
				{
					GUObjectArray.AddUObjectDeleteListener(this);
				}
			}
			AnnotationMap.Add(AnnotationCacheKey,AnnotationCacheValue);
		}
	}
	/**
	 * Removes an annotation from the annotation list and returns the annotation if it had one 
	 *
	 * @param Object		Object to de-annotate.
	 * @return				Old annotation
	 */
	TAnnotation GetAndRemoveAnnotation(const UObjectBase *Object)
	{		
		check(Object);
		FScopeLock AnntationMapLock(&AnnotationMapCritical);
		AnnotationCacheKey = Object;
		AnnotationCacheValue = TAnnotation();
		const bool bHadElements = (AnnotationMap.Num() > 0);
		TAnnotation Result;
		AnnotationMap.RemoveAndCopyValue(AnnotationCacheKey, Result);
		if (bHadElements && AnnotationMap.Num() == 0)
		{
			// we are removing the last one, so if we are auto removing or verifying removal, unregister now
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bAutoRemove)
#endif
			{
				GUObjectArray.RemoveUObjectDeleteListener(this);
			}
		}
		return Result;
	}
	/**
	 * Removes an annotation from the annotation list. 
	 *
	 * @param Object		Object to de-annotate.
	 */
	void RemoveAnnotation(const UObjectBase *Object)
	{
		check(Object);
		FScopeLock AnntationMapLock(&AnnotationMapCritical);
		AnnotationCacheKey = Object;
		AnnotationCacheValue = TAnnotation();
		const bool bHadElements = (AnnotationMap.Num() > 0);
		AnnotationMap.Remove(AnnotationCacheKey);
		if (bHadElements && AnnotationMap.Num() == 0)
		{
			// we are removing the last one, so if we are auto removing or verifying removal, unregister now
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bAutoRemove)
#endif
			{
				GUObjectArray.RemoveUObjectDeleteListener(this);
			}
		}
	}
	/**
	 * Removes all annotation from the annotation list. 
	 *
	 */
	void RemoveAllAnnotations()
	{
		FScopeLock AnntationMapLock(&AnnotationMapCritical);
		AnnotationCacheKey = NULL;
		AnnotationCacheValue = TAnnotation();
		const bool bHadElements = (AnnotationMap.Num() > 0);
		AnnotationMap.Empty();
		if (bHadElements)
		{
			// we are removing the last one, so if we are auto removing or verifying removal, unregister now
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bAutoRemove)
#endif
			{
				GUObjectArray.RemoveUObjectDeleteListener(this);
			}
		}
	}

	/**
	 * Return the annotation associated with a uobject 
	 *
	 * @param Object		Object to return the annotation for
	 */
	FORCEINLINE TAnnotation GetAnnotation(const UObjectBase *Object)
	{
		check(Object);
		FScopeLock AnntationMapLock(&AnnotationMapCritical);
		if (Object != AnnotationCacheKey)
		{			
			AnnotationCacheKey = Object;
			TAnnotation* Entry = AnnotationMap.Find(AnnotationCacheKey);
			if (Entry)
			{
				AnnotationCacheValue = *Entry;
			}
			else
			{
				AnnotationCacheValue = TAnnotation();
			}
		}
		return AnnotationCacheValue;
	}

	/**
	 * Return the annotation map. Caution, this is for low level use 
	 * @return A mapping from UObjectBase to annotation for non-default annotations
	 */
	const TMap<const UObjectBase *,TAnnotation>& GetAnnotationMap() const
	{
		return AnnotationMap;
	}

	/** 
	 * Reserves memory for the annotation map for the specified number of elements, used to avoid reallocations. 
	 */
	void Reserve(int32 ExpectedNumElements)
	{
		FScopeLock AnntationMapLock(&AnnotationMapCritical);
		AnnotationMap.Empty(ExpectedNumElements);
	}

private:

	/**
	 * Map from live objects to an annotation
	 */
	TMap<const UObjectBase *,TAnnotation> AnnotationMap;
	FCriticalSection AnnotationMapCritical;

	/**
	 * Key for a one-item cache of the last lookup into AnnotationMap.
	 * Annotation are often called back-to-back so this is a performance optimization for that.
	 */
	const UObjectBase* AnnotationCacheKey;
	/**
	 * Value for a one-item cache of the last lookup into AnnotationMap.
	 */
	TAnnotation AnnotationCacheValue;

};


/**
* FUObjectAnnotationSparseSearchable is a helper class that is used to store sparse, slow, temporary, editor only, external 
* or other low priority information about UObjects...and also provides the ability to find a object based on the unique
* annotation. 
*
* All of the restrictions mentioned for FUObjectAnnotationSparse apply
* 
* @param TAnnotation type of the annotation
* @param bAutoRemove if true, annotation will automatically be removed, otherwise in non-final builds it will verify that the annotation was removed by other means prior to destruction.
**/
template<typename TAnnotation, bool bAutoRemove>
class FUObjectAnnotationSparseSearchable : public FUObjectAnnotationSparse<TAnnotation, bAutoRemove>
{
	typedef FUObjectAnnotationSparse<TAnnotation, bAutoRemove> Super;
public:
	/**
	 * Interface for FUObjectAllocator::FUObjectDeleteListener
	 *
	 * @param Object object that has been destroyed
	 * @param Index	index of object that is being deleted
	 */
	virtual void NotifyUObjectDeleted(const UObjectBase *Object, int32 Index)
	{
		RemoveAnnotation(Object);
	}

	/**
	 * Destructor, removes all annotations, which removes the annotation as a uobject destruction listener
	 */
	virtual ~FUObjectAnnotationSparseSearchable()
	{
		RemoveAllAnnotations();
	}

	/**
	 * Find the UObject associated with a given annotation
	 *
	 * @param Annotation	Annotation to find
	 * @return				Object associated with this annotation or NULL if none found
	 */
	UObject *Find(TAnnotation Annotation)
	{
		FScopeLock InverseAnntationMapLock(&InverseAnnotationMapCritical);
		checkSlow(!Annotation.IsDefault()); // it is not legal to search for the default annotation
		return (UObject *)InverseAnnotationMap.FindRef(Annotation);
	}

	/**
	 * Add an annotation to the annotation list. If the Annotation is the default, then the annotation is removed.
	 *
	 * @param Object		Object to annotate.
	 * @param Annotation	Annotation to associate with Object.
	 */
	void AddAnnotation(const UObjectBase *Object,TAnnotation Annotation)
	{
		FScopeLock InverseAnntationMapLock(&InverseAnnotationMapCritical);
		if (Annotation.IsDefault())
		{
			RemoveAnnotation(Object); // adding the default annotation is the same as removing an annotation
		}
		else
		{
			TAnnotation ExistingAnnotation = this->GetAnnotation(Object);
			int32 NumExistingRemoved = InverseAnnotationMap.Remove(ExistingAnnotation);
			checkSlow(NumExistingRemoved == 0);

			Super::AddAnnotation(Object, Annotation);
			// should not exist in the mapping; we require uniqueness
			int32 NumRemoved = InverseAnnotationMap.Remove(Annotation);
			checkSlow(NumRemoved == 0);
			InverseAnnotationMap.Add(Annotation, Object);
		}
	}
	/**
	 * Removes an annotation from the annotation list. 
	 *
	 * @param Object		Object to de-annotate.
	 */
	void RemoveAnnotation(const UObjectBase *Object)
	{
		FScopeLock InverseAnntationMapLock(&InverseAnnotationMapCritical);
		TAnnotation Annotation = this->GetAndRemoveAnnotation(Object);
		if (Annotation.IsDefault())
		{
			// should not exist in the mapping
			checkSlow(!InverseAnnotationMap.Find(Annotation));
		}
		else
		{
			int32 NumRemoved = InverseAnnotationMap.Remove(Annotation);
			checkSlow(NumRemoved == 1);
		}
	}
	/**
	 * Removes all annotation from the annotation list. 
	 *
	 */
	void RemoveAllAnnotations()
	{
		FScopeLock InverseAnntationMapLock(&InverseAnnotationMapCritical);
		Super::RemoveAllAnnotations();
		InverseAnnotationMap.Empty();
	}


private:

	/**
	 * Inverse Map annotation to live object
	 */
	TMap<TAnnotation, const UObjectBase *> InverseAnnotationMap;
	FCriticalSection InverseAnnotationMapCritical;
};


struct FBoolAnnotation
{
	/**
	 * default constructor
	 * Default constructor must be the default item
	 */
	FBoolAnnotation() :
		Mark(false)
	{
	}
	/**
	 * Initialization constructor
	 * @param InMarks marks to initialize to
	 */
	FBoolAnnotation(bool InMark) :
		Mark(InMark)
	{
	}
	/**
	 * Determine if this annotation
	 * @return true is this is a default pair. We only check the linker because CheckInvariants rules out bogus combinations
	 */
	FORCEINLINE bool IsDefault()
	{
		return !Mark;
	}

	/**
	 * bool associated with an object
	 */
	bool				Mark; 

};

template <> struct TIsPODType<FBoolAnnotation> { enum { Value = true }; };


/**
* FUObjectAnnotationSparseBool is a specialization of FUObjectAnnotationSparse for bools, slow, temporary, editor only, external 
* or other low priority bools about UObjects.
*
* @todo UE4 this should probably be reimplemented from scratch as a TSet instead of essentially a map to a value that is always true anyway.
**/
class FUObjectAnnotationSparseBool : private FUObjectAnnotationSparse<FBoolAnnotation,true>
{
public:
	/**
	 * Sets this bool annotation to true for this object
	 *
	 * @param Object		Object to annotate.
	 */
	FORCEINLINE void Set(const UObjectBase *Object)
	{
		this->AddAnnotation(Object,FBoolAnnotation(true));
	}
	/**
	 * Removes an annotation from the annotation list. 
	 *
	 * @param Object		Object to de-annotate.
	 */
	FORCEINLINE void Clear(const UObjectBase *Object)
	{
		this->RemoveAnnotation(Object);
	}

	/**
	 * Removes all bool annotations 
	 *
	 */
	FORCEINLINE void ClearAll()
	{
		this->RemoveAllAnnotations();
	}

	/**
	 * Return the bool annotation associated with a uobject 
	 *
	 * @param Object		Object to return the bool annotation for
	 */
	FORCEINLINE bool Get(const UObjectBase *Object)
	{
		return this->GetAnnotation(Object).Mark;
	}

	/** 
	 * Reserves memory for the annotation map for the specified number of elements, used to avoid reallocations. 
	 */
	FORCEINLINE void Reserve(int32 ExpectedNumElements)
	{
		FUObjectAnnotationSparse<FBoolAnnotation,true>::Reserve(ExpectedNumElements);
	}

	FORCEINLINE int32 Num() const
	{
		return this->GetAnnotationMap().Num();
	}
};


/**
* FUObjectAnnotationDense is a helper class that is used to store dense, fast, temporary, editor only, external 
* or other tangential information about UObjects.
*
* There is a notion of a default annotation and UObjects default to this annotation.
* 
* Annotations are automatically returned to the default when UObjects are destroyed.
* Annotation are not "garbage collection aware", so it isn't safe to store pointers to other UObjects in an 
* annotation unless external guarantees are made such that destruction of the other object removes the
* annotation.
* @param TAnnotation type of the annotation
* @param bAutoRemove if true, annotation will automatically be removed, otherwise in non-final builds it will verify that the annotation was removed by other means prior to destruction.
**/
template<typename TAnnotation, bool bAutoRemove>
class FUObjectAnnotationDense : public FUObjectArray::FUObjectDeleteListener
{
public:

	/**
	 * Interface for FUObjectAllocator::FUObjectDeleteListener
	 *
	 * @param Object object that has been destroyed
	 * @param Index	index of object that is being deleted
	 */
	virtual void NotifyUObjectDeleted(const UObjectBase *Object, int32 Index)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!bAutoRemove)
		{
			// in this case we are only verifying that the external assurances of removal are met
			check(Index >= AnnotationArray.Num() || AnnotationArray[Index].IsDefault());
		}
		else
#endif
		{
			RemoveAnnotation(Index);
		}
	}

	/**
	 * Destructor, removes all annotations, which removes the annotation as a uobject destruction listener
	 */
	virtual ~FUObjectAnnotationDense()
	{
		RemoveAllAnnotations();
	}

	/**
	 * Add an annotation to the annotation list. If the Annotation is the default, then the annotation is removed.
	 *
	 * @param Object		Object to annotate.
	 * @param Annotation	Annotation to associate with Object.
	 */
	void AddAnnotation(const UObjectBase *Object,TAnnotation Annotation)
	{
		check(Object);
		AddAnnotation(GUObjectArray.ObjectToIndex(Object),Annotation);
	}
	/**
	 * Add an annotation to the annotation list. If the Annotation is the default, then the annotation is removed.
	 *
	 * @param Index			Index of object to annotate.
	 * @param Annotation	Annotation to associate with Object.
	 */
	void AddAnnotation(int32 Index,TAnnotation Annotation)
	{
		check(Index >= 0);
		if (Annotation.IsDefault())
		{
			RemoveAnnotation(Index); // adding the default annotation is the same as removing an annotation
		}
		else
		{
			FScopeLock AnnotationArrayLock(&AnnotationArrayCritical);
			if (AnnotationArray.Num() == 0)
			{
				// we are adding the first one, so if we are auto removing or verifying removal, register now
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (bAutoRemove)
#endif
				{
					GUObjectArray.AddUObjectDeleteListener(this);
				}
			}
			if (Index >= AnnotationArray.Num())
			{
				int32 AddNum = 1 + Index - AnnotationArray.Num();
				int32 Start = AnnotationArray.AddUninitialized(AddNum);
				while (AddNum--) 
				{
					new (AnnotationArray.GetData() + Start++) TAnnotation();
				}
			}
			AnnotationArray[Index] = Annotation;
		}
	}
	/**
	 * Removes an annotation from the annotation list. 
	 *
	 * @param Object		Object to de-annotate.
	 */
	void RemoveAnnotation(const UObjectBase *Object)
	{
		check(Object);
		RemoveAnnotation(GUObjectArray.ObjectToIndex(Object));
	}
	/**
	 * Removes an annotation from the annotation list. 
	 *
	 * @param Object		Object to de-annotate.
	 */
	void RemoveAnnotation(int32 Index)
	{
		check(Index >= 0);
		FScopeLock AnnotationArrayLock(&AnnotationArrayCritical);
		if (Index <  AnnotationArray.Num())
		{
			AnnotationArray[Index] = TAnnotation();
		}
	}
	/**
	 * Removes all annotation from the annotation list. 
	 *
	 */
	void RemoveAllAnnotations()
	{
		FScopeLock AnnotationArrayLock(&AnnotationArrayCritical);
		bool bHadElements = (AnnotationArray.Num() > 0);
		AnnotationArray.Empty();
		if (bHadElements)
		{
			// we are removing the last one, so if we are auto removing or verifying removal, unregister now
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bAutoRemove)
#endif
			{
				GUObjectArray.RemoveUObjectDeleteListener(this);
			}
		}
	}

	/**
	 * Return the annotation associated with a uobject 
	 *
	 * @param Object		Object to return the annotation for
	 */
	FORCEINLINE TAnnotation GetAnnotation(const UObjectBase *Object)
	{
		check(Object);
		return GetAnnotation(GUObjectArray.ObjectToIndex(Object));
	}

	/**
	 * Return the annotation associated with a uobject 
	 *
	 * @param Index		Index of the annotation to return
	 */
	FORCEINLINE TAnnotation GetAnnotation(int32 Index)
	{
		check(Index >= 0);
		FScopeLock AnnotationArrayLock(&AnnotationArrayCritical);
		if (Index < AnnotationArray.Num())
		{
			return AnnotationArray[Index];
		}
		return TAnnotation();
	}

	/**
	 * Return the annotation associated with a uobject 
	 *
	 * @param Object		Object to return the annotation for
	 * @return				Reference to annotation.
	 */
	FORCEINLINE TAnnotation& GetAnnotationRef(const UObjectBase *Object)
	{
		check(Object);
		return GetAnnotationRef(GUObjectArray.ObjectToIndex(Object));
	}

	/**
	 * Return the annotation associated with a uobject. Adds one if the object has
	 * no annotation yet.
	 *
	 * @param Index		Index of the annotation to return
	 * @return			Reference to the annotation.
	 */
	FORCEINLINE TAnnotation& GetAnnotationRef(int32 Index)
	{
		FScopeLock AnnotationArrayLock(&AnnotationArrayCritical);
		if (Index >= AnnotationArray.Num())
		{
			AddAnnotation(Index, TAnnotation());
		}
		return AnnotationArray[Index];
	}

private:

	/**
	 * Map from live objects to an annotation
	 */
	TArray<TAnnotation> AnnotationArray;
	FCriticalSection AnnotationArrayCritical;

};

/**
* FUObjectAnnotationDenseBool is custom annotation that tracks a bool per UObject. 
**/
class FUObjectAnnotationDenseBool : public FUObjectArray::FUObjectDeleteListener
{
	typedef uint32 TBitType;
	enum {BitsPerElement = sizeof(TBitType) * 8};
public:

	/**
	 * Interface for FUObjectAllocator::FUObjectDeleteListener
	 *
	 * @param Object object that has been destroyed
	 * @param Index	index of object that is being deleted
	 */
	virtual void NotifyUObjectDeleted(const UObjectBase *Object, int32 Index)
	{
		RemoveAnnotation(Index);
	}

	/**
	 * Destructor, removes all annotations, which removes the annotation as a uobject destruction listener
	 */
	virtual ~FUObjectAnnotationDenseBool()
	{
		RemoveAllAnnotations();
	}

	/**
	 * Sets this bool annotation to true for this object
	 *
	 * @param Object		Object to annotate.
	 */
	FORCEINLINE void Set(const UObjectBase *Object)
	{
		checkSlow(Object);
		int32 Index = GUObjectArray.ObjectToIndex(Object);
		checkSlow(Index >= 0);
		if (AnnotationArray.Num() == 0)
		{
			GUObjectArray.AddUObjectDeleteListener(this);
		}
		if (Index >= AnnotationArray.Num() * BitsPerElement)
		{
			int32 AddNum = 1 + Index - AnnotationArray.Num() * BitsPerElement;
			int32 AddElements = (AddNum +  BitsPerElement - 1) / BitsPerElement;
			checkSlow(AddElements);
			AnnotationArray.AddZeroed(AddElements);
			checkSlow(Index < AnnotationArray.Num() * BitsPerElement);
		}
		AnnotationArray[Index / BitsPerElement] |= TBitType(TBitType(1) << (Index % BitsPerElement));
	}
	/**
	 * Removes an annotation from the annotation list. 
	 *
	 * @param Object		Object to de-annotate.
	 */
	FORCEINLINE void Clear(const UObjectBase *Object)
	{
		checkSlow(Object);
		int32 Index = GUObjectArray.ObjectToIndex(Object);
		RemoveAnnotation(Index);
	}

	/**
	 * Removes all bool annotations 
	 *
	 */
	FORCEINLINE void ClearAll()
	{
		RemoveAllAnnotations();
	}

	/**
	 * Return the bool annotation associated with a uobject 
	 *
	 * @param Object		Object to return the bool annotation for
	 */
	FORCEINLINE bool Get(const UObjectBase *Object)
	{
		checkSlow(Object);
		int32 Index = GUObjectArray.ObjectToIndex(Object);
		checkSlow(Index >= 0);
		if (Index < AnnotationArray.Num() * BitsPerElement)
		{
			return !!(AnnotationArray[Index / BitsPerElement] & TBitType(TBitType(1) << (Index % BitsPerElement)));
		}
		return false;
	}

private:
	/**
	 * Removes an annotation from the annotation list. 
	 *
	 * @param Object		Object to de-annotate.
	 */
	void RemoveAnnotation(int32 Index)
	{
		checkSlow(Index >= 0);
		if (Index <  AnnotationArray.Num() * BitsPerElement)
		{
			AnnotationArray[Index / BitsPerElement] &= ~TBitType(TBitType(1) << (Index % BitsPerElement));
		}
	}
	/**
	 * Removes all annotation from the annotation list. 
	 *
	 */
	void RemoveAllAnnotations()
	{
		bool bHadElements = (AnnotationArray.Num() > 0);
		AnnotationArray.Empty();
		if (bHadElements)
		{
			GUObjectArray.RemoveUObjectDeleteListener(this);
		}
	}

	/**
	 * Map from live objects to an annotation
	 */
	TArray<TBitType> AnnotationArray;

};



// Definition is in UObjectGlobals.cpp
extern COREUOBJECT_API FUObjectAnnotationSparseBool GSelectedObjectAnnotation;

