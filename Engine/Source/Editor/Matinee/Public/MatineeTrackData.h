// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpTrack.h"

/*-----------------------------------------------------------------------------
	Interp Track Filters
-----------------------------------------------------------------------------*/

/**
 *	Interp track filter that accepts all interp tracks.
 */
class FAllTrackFilter
{
public:

	/**
	 * @param	Track	The interp track to check if suitable.
	 * @return	true	always.
	 */
	static FORCEINLINE bool IsSuitable( const UInterpTrack* Track )
	{
		return true;
	}
};

/**
 *	Interp track filter that accepts only selected interp tracks.
 */
class FSelectedTrackFilter
{
public:

	/**
	 * @param	Track	The interp track to check if suitable.
	 * @return	true	if the given track is selected.
	 */
	static FORCEINLINE bool IsSuitable( const UInterpTrack* Track )
	{
		return Track->IsSelected();
	}
};

/**
 * Interp track filter that accepts only tracks of the given template class type.
 */
template <class ClassType>
class FClassTypeTrackFilter
{
public:

	/**
	 * @param	Track	The interp track to check if suitable.
	 * @return	true	if the given track is of the template track type class; false, otherwise.
	 */
	static FORCEINLINE bool IsSuitable( const UInterpTrack* Track )
	{
		return Track->IsA(ClassType::StaticClass()) && Track->IsSelected();
	}
};

/** The default track filter for interp track iterators. */
typedef FAllTrackFilter		DefaultTrackFilter;

/*-----------------------------------------------------------------------------
	TInterpTrackIteratorBase
-----------------------------------------------------------------------------*/

/**
 * Base iterator for all interp track iterators.
 */
template <bool bConst, class Filter=DefaultTrackFilter>
class TInterpTrackIteratorBase
{
private:

	// Typedefs that allow for both const and non-const versions of this iterator without any code duplication.
	typedef typename TChooseClass<bConst,const UInterpGroup,UInterpGroup>::Result						GroupType;
	typedef typename TChooseClass<bConst,const TArray<UInterpGroup*>,TArray<UInterpGroup*> >::Result	GroupArrayType;
	typedef typename TChooseClass<bConst,TArray<UInterpGroup*>::TConstIterator,TArray<UInterpGroup*>::TIterator>::Result	GroupIteratorType;

	typedef typename TChooseClass<bConst,const UInterpTrack,UInterpTrack>::Result						TrackType;

public:

	/**
	 * @return	A pointer to the current interp track. Pointer is guaranteed to be valid as long as the iterator is still valid.
	 */
	FORCEINLINE TrackType* operator*()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		check( IsCurrentTrackValid() );
		return AllTracksInCurrentGroup[ TrackIteratorIndex ];
	}

	/**
	 * @return	A pointer to the current interp track. Pointer is guaranteed to be valid as long as the iterator is still valid.
	 */
	FORCEINLINE TrackType* operator->()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		check( IsCurrentTrackValid() );
		return AllTracksInCurrentGroup[TrackIteratorIndex];
	}

	/**
	 * @return	If the current track is a subtrack, returns the index of the subtrack in its parent track.  
	 *		If the track is not a subtrack, returns the index of the track in its owning group.
	 *  		Index is guaranteed to be valid as long as the iterator is valid.
	 */
	FORCEINLINE int32 GetTrackIndex()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		check( IsCurrentTrackValid() );

		// Get the track index for the current interp track.
		int32 TrackIndex = INDEX_NONE;
		UInterpTrack* CurrentTrack = AllTracksInCurrentGroup[ TrackIteratorIndex ];

		UInterpGroup* OwningGroup = Cast<UInterpGroup>( CurrentTrack->GetOuter() );
		if( OwningGroup )
		{
			// Track is not a subtrack, find its index directly from the group
			TrackIndex = OwningGroup->InterpTracks.Find( CurrentTrack );
		}
		else
		{
			// Track is a subtrack, find its index from its owning track.
			UInterpTrack* OwningTrack = CastChecked<UInterpTrack>( CurrentTrack->GetOuter() );
			TrackIndex = OwningTrack->SubTracks.Find( CurrentTrack );
		}

		return TrackIndex;
	}

	/**
	 * @return	The group the owns the current track. 
	 */
	FORCEINLINE GroupType* GetGroup()
	{
		return *GroupIt;
	}

	/**
	 * @return	true if the iterator has not reached the end. 
	 */
	FORCEINLINE operator bool()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		return GroupIt && IsCurrentTrackValid();
	}

	/**
	 * Increments the iterator to point to the next valid interp track or the end of the iterator.
	 */
	void operator++()
	{
		bool bFoundNextTrack = false;

		// Increment the interp track iterator instead of the group iterator. Incrementing 
		// the group iterator will change the group and list of selected tracks.
		++TrackIteratorIndex;

		// The current track index should never be below zero after incrementing. 
		check( TrackIteratorIndex >= 0 );

		// Keep iterating until we found another suitable track or we reached the end.
		while( !bFoundNextTrack && GroupIt )
		{

			// If we reached the end of the tracks for the current group, 
			// the we need to advance to the first track of the next group.
			if( TrackIteratorIndex == AllTracksInCurrentGroup.Num() )
			{
				// Advance to the next group. If this is the end 
				// of the iterator, the while loop will terminate.
				++GroupIt;
				TrackIteratorIndex = 0;
				if( GroupIt )
				{
					// Generate a new list of tracks in the current group.
					GetAllTracksInCurrentGroup( *GroupIt );
				}
			}
			// We haven't reached the end, so we can now determine the validity of the current track.
			else
			{
				// If the track is valid, then we found the next track.	
				if( Filter::IsSuitable( AllTracksInCurrentGroup[ TrackIteratorIndex ] ) )
				{
					bFoundNextTrack = true;
				}
				// Else, keep iterating through the tracks. 
				else
				{
					TrackIteratorIndex++;
				}
			}
		}
	}

protected:

	/**
	 * Constructor for the base interp track iterator. Only provided to 
	 * child classes so that this class isn't instantiated. 
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpTrackIteratorBase( GroupArrayType& InGroupArray )
		:	GroupArray(InGroupArray)
		,	GroupIt(InGroupArray)
		,	TrackIteratorIndex(INDEX_NONE)
	{
		if( GroupIt )
		{
			// Greate a new list of tracks in the current group
			GetAllTracksInCurrentGroup( *GroupIt );
		}
		++(*this);
	}

	/**
	 * @return	A pointer to the current interp track. Pointer is guaranteed to be valid as long as the iterator is valid.
	 */
	FORCEINLINE TrackType* GetCurrentTrack()
	{
		// The iterator must be pointing to a valid track, otherwise the iterator is considered invalid. 
		check( IsCurrentTrackValid() );
		
		return AllTracksInCurrentGroup[ TrackIteratorIndex ];
	}

	/**
	 * Removes the current interp track that the iterator is pointing to from the current interp group and updates the iteator.
	 */
	void RemoveCurrentTrack()
	{
		UInterpTrack* RemovedTrack = AllTracksInCurrentGroup[TrackIteratorIndex];

		// Removing it from the current group doesn't really do anything other than maintain our current array.  We need to remove it from the group/track that owns it
		AllTracksInCurrentGroup.RemoveAt( TrackIteratorIndex );

		UInterpGroup* OwningGroup = Cast<UInterpGroup>( RemovedTrack->GetOuter() );
		if( OwningGroup )
		{
			OwningGroup->InterpTracks.RemoveSingle( RemovedTrack );
			// Remove subtracks from tracks in current group and empty all subtracks.
			for( int32 SubTrackIndex = 0; SubTrackIndex < RemovedTrack->SubTracks.Num(); ++SubTrackIndex )
			{
				AllTracksInCurrentGroup.RemoveSingle( RemovedTrack->SubTracks[ SubTrackIndex ] );
			}
			RemovedTrack->SubTracks.Empty();
		}
		else
		{
			UInterpTrack* OwningTrack = CastChecked<UInterpTrack>( RemovedTrack->GetOuter() );
			OwningTrack->SubTracks.RemoveSingle( RemovedTrack );
		}
		// Move the track iterator back one so that it's pointing to the track before the removed track. 

		// WARNING: In the event that a non-accept-all track filter is set to this iterator, such as a selected 
		// track filter, this iterator could be invalid at this point until the iterator is moved 
		// forward. NEVER try to access the current track after calling this function.
		--TrackIteratorIndex;
	}

	/**
	 * @return true if the interp track that the iterator is currently pointing to is valid. 
	 */
	FORCEINLINE bool IsCurrentTrackValid() const
	{
		return ( AllTracksInCurrentGroup.IsValidIndex( TrackIteratorIndex ) && Filter::IsSuitable( AllTracksInCurrentGroup[TrackIteratorIndex] ) );
	}

	/**
	 * Decrements the interp track iterator to point to the previous valid interp track.
	 *
	 * @note	This function is protected because we currently don't support reverse iteration.
	 *			Specifically, TInterpTrackIterator::RemoveCurrent() assumes that the iterator 
	 *			will only move forward. 
	 */
	void operator--()
	{
		bool bFoundNextTrack = false;

		// Decrement the interp track iterator instead of the group iterator. Incrementing 
		// the group iterator will change the group and list of selected tracks.
		--TrackIteratorIndex;

		// Keep iterating until we found another suitable track or we reached the end.
		while( !bFoundNextTrack && GroupIt )
		{
			// When decrementing, we should never reach the end of the array tracks.
			check( TrackIteratorIndex < AllTracksInCurrentGroup.Num() );

			// If we reached the end of the tracks for the current group, 
			// the we need to advance to the first track of the next group.
			if( TrackIteratorIndex == INDEX_NONE )
			{
				// Advance to the next group. If this is the end 
				// of the iterator, the while loop will terminate.
				--GroupIt;

				if( GroupIt )
				{
					// Generate a new list of tracks in the current group now that the group has changed.
					GetAllTracksInCurrentGroup( *GroupIt );
					TrackIteratorIndex = ( AllTracksInCurrentGroup.Num() - 1);
				}
			}
			// We haven't reached the end, so we can now determine the validity of the current track.
			else
			{
				// If the track is valid, then we found the next track. So, stop iterating.	
				if( Filter::IsSuitable( AllTracksInCurrentGroup[TrackIteratorIndex] ) )
				{
					bFoundNextTrack = true;
				}
				// Else, keep iterating through the tracks. 
				else
				{
					--TrackIteratorIndex;
				}
			}
		}
	}

private:

	/** The array of all interp groups currently in the interp editor. */
	GroupArrayType& GroupArray;

	/** The iterator that iterates over all interp groups in the interp editor. */
	GroupIteratorType GroupIt;

	/** The index of the interp track that iterator is currently pointing to. */
	int32 TrackIteratorIndex;

	/** A list of all tracks in the current group */
	TArray<UInterpTrack*> AllTracksInCurrentGroup;
	/** 
	 * Default construction intentionally left undefined to prevent instantiation! 
	 *
	 * @note	To use this class, you must call the parameterized constructor.
	 */
	TInterpTrackIteratorBase();

	/** Gets all subtracks in the current track (recursive)*/
	void GetAllSubTracksInTrack( UInterpTrack* Track )
	{
		for( int32 SubTrackIdx = 0; SubTrackIdx < Track->SubTracks.Num(); ++SubTrackIdx )
		{
			UInterpTrack* SubTrack = Track->SubTracks[ SubTrackIdx ];
			AllTracksInCurrentGroup.Add( SubTrack );
			GetAllSubTracksInTrack( SubTrack );
		}
	}

	/** Gets all tracks that are in the current group. */
	void GetAllTracksInCurrentGroup( GroupType* CurrentGroup )
	{
		AllTracksInCurrentGroup.Empty();
		if( CurrentGroup )
		{
			for( int32 TrackIdx = 0; TrackIdx < CurrentGroup->InterpTracks.Num(); ++TrackIdx )
			{
				UInterpTrack* Track = CurrentGroup->InterpTracks[ TrackIdx ];
				AllTracksInCurrentGroup.Add( Track );
				GetAllSubTracksInTrack( Track );
			}
		}
	}
};

/*-----------------------------------------------------------------------------
	TInterpTrackIterator / TInterpTrackConstIterator
-----------------------------------------------------------------------------*/

/**
 * Implements a modifiable interp track iterator with option to specify the filter type. 
 */
template <class FilterType=DefaultTrackFilter>
class TInterpTrackIterator : public TInterpTrackIteratorBase<false,FilterType>
{
public:

	/**
	 * Constructor to make a modifiable interp track iterator.
	 * 
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpTrackIterator( TArray<UInterpGroup*>& InGroupArray )
		:	TInterpTrackIteratorBase<false,FilterType>(InGroupArray)
	{
	}

	/**
	 * Removes the interp track that the iterator is currently pointing to. 
	 *
	 * @warning Do not dereference this iterator after calling this function until the iterator has been moved forward. 
	 */
	void RemoveCurrent()
	{
		this->RemoveCurrentTrack();
	}

	/**
	 * Moves the location of the iterator up or down by one.
	 * 
	 * @note	This function needs to be called every time an interp track is moved up or down by one. 
	 * @param	Value	The amount to move the iterator by. Must be either -1 or 1. 
	 */
	void MoveIteratorBy( int32 Value )
	{
		check( FMath::Abs(Value) == 1 );

		( Value == 1 ) ? ++(*this) : --(*this);
	}

private:

	/** 
	 * Default constructor. 
	 * Intentionally left undefined to prevent instantiation using the default constructor.
	 * 
	 * @note	Must use parameterized constructor when making an instance of this class.
	 */
	TInterpTrackIterator();
};

/**
 * Implements a non-modifiable iterator with option to specify the filter type. 
 */
template <class FilterType=DefaultTrackFilter>
class TInterpTrackConstIterator : public TInterpTrackIteratorBase<true,FilterType>
{
public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpTrackConstIterator( const TArray<UInterpGroup*>& InGroupArray )
		:	TInterpTrackIteratorBase<true,FilterType>(InGroupArray)
	{
	}

private:

	/** 
	 * Default constructor. 
	 * Intentionally left undefined to prevent instantiation using the default constructor.
	 * 
	 * @note	Must use parameterized constructor when making an instance of this class.
	 */
	TInterpTrackConstIterator();
};

// These iterators will iterate over all tracks. 
typedef TInterpTrackIterator<>							FAllTracksIterator;
typedef TInterpTrackConstIterator<>						FAllTracksConstIterator;

// These iterator will iterate over only selected tracks.
typedef TInterpTrackIterator<FSelectedTrackFilter>		FSelectedTrackIterator;
typedef TInterpTrackConstIterator<FSelectedTrackFilter>	FSelectedTrackConstIterator;


/*-----------------------------------------------------------------------------
	TTrackClassTypeIterator / TTrackClassTypeConstIterator
-----------------------------------------------------------------------------*/

/**
 * Implements a modifiable interp track iterator that only iterates over interp tracks of the given UClass.
 */
template <class ChildTrackType>
class TTrackClassTypeIterator : public TInterpTrackIterator< FClassTypeTrackFilter<ChildTrackType> >
{
private:

	typedef TInterpTrackIterator< FClassTypeTrackFilter<ChildTrackType> > Super;

public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TTrackClassTypeIterator( TArray<UInterpGroup*>& InGroupArray )
		:	Super(InGroupArray)
	{
		// If the user didn't pass in a UInterpTrack derived class, then 
		// they probably made a typo or are using the wrong iterator.
		check( ChildTrackType::StaticClass()->IsChildOf(UInterpTrack::StaticClass()) );
	}

	/**
	 * @return	A pointer to the interp track of the given UClass that the iterator is currently pointing to. Guaranteed to be valid if the iterator is valid. 
	 */
	FORCEINLINE ChildTrackType* operator*()
	{
		return CastChecked<ChildTrackType>(this->GetCurrentTrack());
	}

	/**
	 * @return	A pointer to the interp track of the given UClass that the iterator is currently pointing to. Guaranteed to be valid if the iterator is valid. 
	 */
	FORCEINLINE ChildTrackType* operator->()
	{
		return CastChecked<ChildTrackType>(this->GetCurrentTrack());
	}
};


/**
 * Implements a non-modifiable interp track iterator that only iterates over interp tracks of the given UClass.
 */
template <class ChildTrackType>
class TTrackClassTypeConstIterator : public TInterpTrackConstIterator< FClassTypeTrackFilter<ChildTrackType> >
{
private:

	typedef TInterpTrackIterator< FClassTypeTrackFilter<ChildTrackType> > Super;

public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TTrackClassTypeConstIterator( const TArray<UInterpGroup*>& InGroupArray )
		:	Super(InGroupArray)
	{
		// If the user didn't pass in a UInterpTrack derived class, then 
		// they probably made a typo or are using the wrong iterator.
		check( ChildTrackType::StaticClass()->IsChildOf(UInterpTrack::StaticClass()) );
	}

	/**
	 * @return	A pointer to the interp track of the given UClass that the iterator is currently pointing to. Guaranteed to be valid if the iterator is valid. 
	 */
	FORCEINLINE const ChildTrackType* operator*()
	{
		return CastChecked<ChildTrackType>(this->GetCurrentTrack());
	}

	/**
	 * @return	A pointer to the interp track of the given UClass that the iterator is currently pointing to. Guaranteed to be valid if the iterator is valid. 
	 */
	FORCEINLINE const ChildTrackType* operator->()
	{
		return CastChecked<ChildTrackType>(this->GetCurrentTrack());
	}
};
