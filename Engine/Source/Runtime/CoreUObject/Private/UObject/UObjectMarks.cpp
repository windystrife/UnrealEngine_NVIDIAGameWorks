// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectMarks.cpp: Unreal save marks annotation
=============================================================================*/

#include "UObject/UObjectMarks.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectIterator.h"

#include "UObject/UObjectAnnotation.h"


struct FObjectMark
{
	/**
	 * default contructor
	 * Default constructor must be the default item
	 */
	FObjectMark() :
		Marks(OBJECTMARK_NOMARKS)
	{
	}
	/**
	 * Intilialization constructor
	 * @param InMarks marks to initialize to
	 */
	FObjectMark(EObjectMark InMarks) :
		Marks(InMarks)
	{
	}
	/**
	 * Determine if this annotation is the default
	 * @return true is this is a default pair. We only check the linker because CheckInvariants rules out bogus combinations
	 */
	FORCEINLINE bool IsDefault()
	{
		return Marks == OBJECTMARK_NOMARKS;
	}

	/**
	 * Marks associated with an object
	 */
	EObjectMark				Marks; 

};

template <> struct TIsPODType<FObjectMark> { enum { Value = true }; };


/**
 * Annotation to relate objects with object marks
 */
static FUObjectAnnotationSparse<FObjectMark,true> MarkAnnotation;



/**
 * Adds marks to an object
 *
 * @param	Object	Object to add marks to
 * @param	Marks	Logical OR of OBJECTMARK_'s to apply 
 */
void MarkObject(const class UObjectBase* Object, EObjectMark Marks)
{
	MarkAnnotation.AddAnnotation(Object,FObjectMark(EObjectMark(MarkAnnotation.GetAnnotation(Object).Marks | Marks)));
}

/**
 * Removes marks from and object
 *
 * @param	Object	Object to remove marks from
 * @param	Marks	Logical OR of OBJECTMARK_'s to remove 
 */
void UnMarkObject(const class UObjectBase* Object, EObjectMark Marks)
{
	FObjectMark Annotation = MarkAnnotation.GetAnnotation(Object);
	if(Annotation.Marks & Marks)
	{
		MarkAnnotation.AddAnnotation(Object,FObjectMark(EObjectMark(Annotation.Marks & ~Marks)));
	}
}

void MarkAllObjects(EObjectMark Marks)
{
	for( FObjectIterator It; It; ++It )
	{
		MarkObject(*It,Marks);
	}
}

void UnMarkAllObjects(EObjectMark Marks)
{
	if (Marks == OBJECTMARK_ALLMARKS)
	{
		MarkAnnotation.RemoveAllAnnotations();
	}
	else
	{
		const TMap<const UObjectBase *, FObjectMark>& Map = MarkAnnotation.GetAnnotationMap();
		for (TMap<const UObjectBase *, FObjectMark>::TConstIterator It(Map); It; ++It)
		{
			if(It.Value().Marks & Marks)
			{
				MarkAnnotation.AddAnnotation((UObject*)It.Key(),FObjectMark(EObjectMark(It.Value().Marks & ~Marks)));
			}
		}
	}
}

bool ObjectHasAnyMarks(const class UObjectBase* Object, EObjectMark Marks)
{
	return !!(MarkAnnotation.GetAnnotation(Object).Marks & Marks);
}

bool ObjectHasAllMarks(const class UObjectBase* Object, EObjectMark Marks)
{
	return (MarkAnnotation.GetAnnotation(Object).Marks & Marks) == Marks;
}

void GetObjectsWithAllMarks(TArray<UObject *>& Results, EObjectMark Marks)
{
	// We don't want to return any objects that are currently being background loaded unless we're using the object iterator during async loading.
	EInternalObjectFlags ExclusionFlags = EInternalObjectFlags::Unreachable;
	if (!IsInAsyncLoadingThread())
	{
		ExclusionFlags |= EInternalObjectFlags::AsyncLoading;
	}
	const TMap<const UObjectBase *, FObjectMark>& Map = MarkAnnotation.GetAnnotationMap();
	Results.Empty(Map.Num());
	for (TMap<const UObjectBase *, FObjectMark>::TConstIterator It(Map); It; ++It)
	{
		if ((It.Value().Marks & Marks) == Marks)
		{
			UObject* Item = (UObject*)It.Key();
			if (!Item->HasAnyInternalFlags(ExclusionFlags))
			{
				Results.Add(Item);
			}
		}
	}
}

void GetObjectsWithAnyMarks(TArray<UObject *>& Results, EObjectMark Marks)
{
	// We don't want to return any objects that are currently being background loaded unless we're using the object iterator during async loading.
	EInternalObjectFlags ExclusionFlags = EInternalObjectFlags::Unreachable;
	if (!IsInAsyncLoadingThread())
	{
		ExclusionFlags |= EInternalObjectFlags::AsyncLoading;
	}
	const TMap<const UObjectBase *, FObjectMark>& Map = MarkAnnotation.GetAnnotationMap();
	Results.Empty(Map.Num());
	for (TMap<const UObjectBase *, FObjectMark>::TConstIterator It(Map); It; ++It)
	{
		if (It.Value().Marks & Marks)
		{
			UObject* Item = (UObject*)It.Key();
			if (!Item->HasAnyInternalFlags(ExclusionFlags))
			{
				Results.Add(Item);
			}
		}
	}
}

