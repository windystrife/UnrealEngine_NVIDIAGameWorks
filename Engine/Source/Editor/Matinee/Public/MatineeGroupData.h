// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Matinee/InterpGroup.h"

class UInterpTrack;

/*-----------------------------------------------------------------------------
	FInterpGroup
-----------------------------------------------------------------------------*/

struct FInterpGroupParentInfo
{
public:

	/** The parented interp group */
	UInterpGroup*	Group;

	/** The group's parent. Can be NULL */
	UInterpGroup*	Parent;

	/** The index of the interp group in the array of all interp groups. */
	int32				GroupIndex;

	/** Does the group have children? */
	bool			bHasChildren;

	/**
	 * Parameterized constructor.
	 *
	 * @param	InGroup	The group to collect parent info.
	 */
	explicit FInterpGroupParentInfo( UInterpGroup* InGroup )
		:	Group(InGroup)
		,	Parent(NULL)
		,	GroupIndex(INDEX_NONE)
		,	bHasChildren(false)
	{
		// As assumption is made that this structure 
		// should contain a non-NULL group.
		check(Group);
	}

	/**
	 * @return	true if the group has a parent.
	 */
	FORCEINLINE bool HasAParent() const
	{
		return (Parent != NULL);
	}

	/**
	 * @return	true if the group is a parent.
	 */
	FORCEINLINE bool IsAParent() const
	{
		return bHasChildren;
	}

	/**
	* @return	true if the group is a parent.
	*/
	bool IsParent( const FInterpGroupParentInfo& ParentCandidate ) const
	{
		// The group assigned to the group parent info should always be valid!
		check(ParentCandidate.Group);
		return (Parent == ParentCandidate.Group);
	}

	/**
	 * @param	Other	The other group parent info to compare against.
	 *
	 * @return	true if the group in this info is pointing to the same group in the other info.
	 */
	FORCEINLINE bool operator==(const FInterpGroupParentInfo& Other) const
	{
		// The group assigned to the group parent info should always be valid!
		check(Group && Other.Group);
		return (Group == Other.Group);
	}

private:

	/**
	 * Default constructor. Intentionally left Un-defined.
	 *
	 * @note	Use parameterized constructor.
	 */
	FInterpGroupParentInfo();
};

/*-----------------------------------------------------------------------------
	Interp Group Filters
-----------------------------------------------------------------------------*/

/**
 * Interp group filter that accepts all interp groups. 
 */
class FAllGroupsFilter
{
public:

	/**
	 * @param	InGroup	The group to check for validity. 
	 * @return	true	always.
	 */
	static bool IsSuitable( const UInterpGroup* InGroup )
	{
		return true;
	}
};

/**
 * Interp group filter that accepts only selected interp groups. 
 */
class FSelectedGroupFilter
{
public:

	/**
	 * @param	InGroup	The group to check for validity. 
	 * @return	true	if the given group is selected; false otherwise.
	 */
	static bool IsSuitable( const UInterpGroup* InGroup )
	{
		return InGroup->IsSelected();
	}
};

/**
 * Interp group filter that accepts only selected folders.
 */
class FSelectedFolderFilter
{
public:

	/**
	 * @param	InGroup	The group to check for validity. 
	 * @return	true	if the given group is selected and is a folder; false, otherwise.
	 */
	static bool IsSuitable( const UInterpGroup* InGroup )
	{
		return (InGroup->IsSelected() && InGroup->bIsFolder);
	}
};

/** The default group filter. */
typedef FAllGroupsFilter DefaultGroupFilter;


/*-----------------------------------------------------------------------------
	TInterpGroupIteratorBase
-----------------------------------------------------------------------------*/

/**
 * Implements the common behavior for all interp group iterators. 
 */
template <bool bConst, class GroupFilter=DefaultGroupFilter>
class TInterpGroupIteratorBase
{
private:

	// Typedefs that allow the iterator to be const or non-const without duplicating any code. 
	typedef typename TChooseClass<bConst,const UInterpGroup,UInterpGroup>::Result								GroupType;
	typedef typename TChooseClass<bConst,const TArray<UInterpGroup*>,TArray<UInterpGroup*> >::Result			GroupArrayType;
	typedef typename TChooseClass<bConst,typename GroupArrayType::TConstIterator,typename GroupArrayType::TIterator>::Result	GroupIteratorType;

	typedef typename TChooseClass<bConst,const UInterpTrack,UInterpTrack>::Result								TrackType;
	typedef typename TChooseClass<bConst,const TArray<UInterpTrack*>,TArray<UInterpTrack*> >::Result			TrackArrayType;

public:

	/**
	 * @return	true if the iterator has not reached the end; false, otherwise.
	 */
	FORCEINLINE operator bool()
	{
		return IsCurrentGroupValid();
	}

	/**
	 * @return	A pointer to the current interp group. Guaranteed to be valid (non-NULL) if the iterator is still valid. 
	 */
	FORCEINLINE GroupType* operator*()
	{
		// The current group must be valid to dereference.
		check( IsCurrentGroupValid() );
		return (*GroupIterator);
	}

	/**
	 * @return	A pointer to the current interp group. Guaranteed to be valid (non-NULL) if the iterator is still valid. 
	 */
	FORCEINLINE GroupType* operator->()
	{
		// The current group must be valid to dereference.
		check( IsCurrentGroupValid() );
		return (*GroupIterator);
	}

	/**
	 * @return	The index of current interp group that the iterator is pointing to.
	 */
	FORCEINLINE int32 GetGroupIndex() const
	{
		return GroupIterator.GetIndex();
	}

	/**
	 * Increments the iterator to point to the next valid interp group or may reach the end of the iterator. 
	 */
	void operator++()
	{
		bool bFoundNextGroup = false;

		++GroupIterator;

		// Keep iterating until we found a group that is 
		// valid or we reached the end of the iterator.
		while( !bFoundNextGroup && GroupIterator )
		{
			// The next valid group is found if it passes the group filter. 
			if( GroupFilter::IsSuitable(*GroupIterator) )
			{
				bFoundNextGroup = true;
			}
			else
			{
				++GroupIterator;
			}
		}
	}

protected:

	/**
	 * Constructor for the base group iterator. MUST be called by any derived classes.
	 * Intentionally, left protected to prevent instantiation of this type.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpGroupIteratorBase( GroupArrayType& InGroupArray )
		:	GroupArray(InGroupArray)
		,	GroupIterator(InGroupArray)
	{
		// It's possible that the first group in the array isn't suitable based on the group filter. 
		// So, we need to make sure we are pointing to the first suitable group.
		if( GroupIterator && !GroupFilter::IsSuitable( *GroupIterator ) )
		{
			++(*this);
		}
	}

	/**
	 * Removes the current interp groups from the interp group array and updates the iterator.
	 */
	void RemoveCurrentGroup()
	{
		// Remove the group itself from the array of interp groups.
		GroupArray.RemoveAt(GroupIterator.GetIndex());

		// Move the group iterator back one so that it's pointing to the track before the removed track. 

		// WARNING: In the event that a non-accept-all group filter is set to this iterator, such as a selected 
		// group filter, this iterator could be invalid at this point until the iterator is moved 
		// forward. NEVER try to access the current group after calling this function.
		--GroupIterator;
	}

	/**
	 * @return	true if the current interp group that the iterator is pointing to is valid. 
	 */
	FORCEINLINE bool IsCurrentGroupValid() const
	{
		return ( GroupIterator && GroupFilter::IsSuitable(*GroupIterator) );
	}

private:

	/** The array of all interp groups currently in the interp editor */
	GroupArrayType& GroupArray;

	/** Iterator that iterates over the interp groups */
	GroupIteratorType GroupIterator;

	/** 
	 * Default constructor. Intentionally left undefined to prevent instantiation of this class type. 
	 *
	 * @note	Instances of this class can only be instantiated by using the parameterized constructor. 
	 */
	TInterpGroupIteratorBase();
};

/*-----------------------------------------------------------------------------
	TInterpGroupIterator / TInterpGroupConstIterator
-----------------------------------------------------------------------------*/

/**
 * Implements a modifiable interp group iterator that iterates over that groups that pass the provided filter. 
 */
template<class GroupFilter=DefaultGroupFilter>
class TInterpGroupIterator : public TInterpGroupIteratorBase<false, GroupFilter>
{
private:

	typedef TInterpGroupIteratorBase<false, GroupFilter> Super;

public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpGroupIterator( TArray<UInterpGroup*>& InGroupArray )
		:	Super(InGroupArray)
	{
	}

	/**
	 * Removes the interp group and associated track selection slots that the iterator is currently pointing to from the selection map.
	 *
	 * @warning Do not dereference this iterator after calling this function until the iterator has been moved forward. 
	 */
	void RemoveCurrent()
	{
		this->RemoveCurrentGroup();
	}

private:

	/** 
	 * Default constructor. Intentionally left un-defined to prevent instantiation of this class type. 
	 *
	 * @note	Instances of this class can only be instantiated by using the parameterized constructor. 
	 */
	TInterpGroupIterator();
};

/**
 * Implements a non-modifiable interp group iterator that iterates over that groups that pass the provided filter. 
 */
template<class GroupFilter=DefaultGroupFilter>
class TInterpGroupConstIterator : public TInterpGroupIteratorBase<true, GroupFilter>
{
private:

	typedef TInterpGroupIteratorBase<true, GroupFilter> Super;

public:

	/**
	 * Constructor for this class.
	 *
	 * @param	InGroupArray	The array of all interp groups currently in the interp editor.
	 */
	explicit TInterpGroupConstIterator( const TArray<UInterpGroup*>& InGroupArray )
		:	Super(InGroupArray)
	{
	}

private:

	/** 
	 * Default constructor. Intentionally left undefined to prevent instantiation of this class type. 
	 *
	 * @note	Instances of this class can only be instantiated by using the parameterized constructor. 
	 */
	TInterpGroupConstIterator();
};

// These iterators can iterate over all interp groups, regardless of selection states. 
typedef TInterpGroupIterator<>								FGroupIterator;
typedef TInterpGroupConstIterator<>							FGroupConstIterator;

// These iterators only iterate over selected interp groups. 
typedef TInterpGroupIterator<FSelectedGroupFilter>			FSelectedGroupIterator;
typedef TInterpGroupConstIterator<FSelectedGroupFilter>		FSelectedGroupConstIterator;

// These iterators only iterate over selected folders. 
typedef TInterpGroupIterator<FSelectedFolderFilter>			FSelectedFolderIterator;
typedef TInterpGroupConstIterator<FSelectedFolderFilter>	FSelectedFolderConstIterator;
