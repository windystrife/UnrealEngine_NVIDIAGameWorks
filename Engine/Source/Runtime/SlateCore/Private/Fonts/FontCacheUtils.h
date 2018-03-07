// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "SlateGlobals.h"

typedef FString KeyType;
typedef FVector2D ValueType;

DECLARE_MEMORY_STAT_EXTERN(TEXT("Font Measure Memory"), STAT_SlateFontMeasureCacheMemory, STATGROUP_SlateMemory, SLATECORE_API);

/**
 * Basic Least Recently Used (LRU) cache
 */
class FLRUStringCache
{
public:
	FLRUStringCache( int32 InMaxNumElements )
		: LookupSet()
		, MostRecent(nullptr)
		, LeastRecent(nullptr)
		, MaxNumElements(InMaxNumElements)
	{ }

	~FLRUStringCache()
	{
		Empty();
	}

	/**
	 * Accesses an item in the cache.  
	 */
	FORCEINLINE const ValueType* AccessItem(const KeyType& Key)
	{
		CacheEntry** Entry = LookupSet.Find( Key );
		if( Entry )
		{
			MarkAsRecent( *Entry );
			return &((*Entry)->Value);
		}
		
		return nullptr;
	}

	void Add( const KeyType& Key, const ValueType& Value )
	{
		CacheEntry** Entry = LookupSet.Find( Key );
	
		// Make a new link
		if( !Entry )
		{
			if( LookupSet.Num() == MaxNumElements )
			{
				Eject();
				checkf( LookupSet.Num() < MaxNumElements, TEXT("Could not eject item from the LRU: (%d of %d), %s"), LookupSet.Num(), MaxNumElements, LeastRecent ? *LeastRecent->Key : TEXT("NULL"));
			}

			CacheEntry* NewEntry = new CacheEntry( Key, Value );

			// Link before the most recent so that we become the most recent
			NewEntry->Link(MostRecent);
			MostRecent = NewEntry;

			if( LeastRecent == nullptr )
			{
				LeastRecent = NewEntry;
			}

			STAT( uint32 CurrentMemUsage = LookupSet.GetAllocatedSize() );
			LookupSet.Add( NewEntry );
			STAT( uint32 NewMemUsage = LookupSet.GetAllocatedSize() );
			INC_MEMORY_STAT_BY( STAT_SlateFontMeasureCacheMemory,  NewMemUsage - CurrentMemUsage );
		}
		else
		{
			// Trying to add an existing value 
			CacheEntry* EntryPtr = *Entry;
			
			// Update the value
			EntryPtr->Value = Value;
			checkSlow( EntryPtr->Key == Key );
			// Mark as the most recent
			MarkAsRecent( EntryPtr );
		}
	}

	void Empty()
	{
		DEC_MEMORY_STAT_BY( STAT_SlateFontMeasureCacheMemory, LookupSet.GetAllocatedSize() );

		for( TSet<CacheEntry*, FCaseSensitiveStringKeyFuncs >::TIterator It(LookupSet); It; ++It )
		{
			CacheEntry* Entry = *It;
			// Note no need to unlink anything here. we are emptying the entire list
			delete Entry;
		}
		LookupSet.Empty();

		MostRecent = LeastRecent = nullptr;
	}
private:
	struct CacheEntry
	{
		KeyType Key;
		ValueType Value;
		CacheEntry* Next;
		CacheEntry* Prev;

		CacheEntry( const KeyType& InKey, const ValueType& InValue )
			: Key( InKey )
			, Value( InValue )
			, Next( nullptr )
			, Prev( nullptr )
		{
			INC_MEMORY_STAT_BY( STAT_SlateFontMeasureCacheMemory, Key.GetAllocatedSize()+sizeof(Value)+sizeof(Next)+sizeof(Prev) );
		}

		~CacheEntry()
		{
			DEC_MEMORY_STAT_BY( STAT_SlateFontMeasureCacheMemory, Key.GetAllocatedSize()+sizeof(Value)+sizeof(Next)+sizeof(Prev) );
		}

		FORCEINLINE void Link( CacheEntry* Before )
		{
			Next = Before;

			if( Before )
			{
				Before->Prev = this;
			}
		}

		FORCEINLINE void Unlink()
		{
			if( Prev )
			{
				Prev->Next = Next;
			}

			if( Next )
			{
				Next->Prev = Prev;
			}

			Prev = nullptr;
			Next = nullptr;
		}
	};

	struct FCaseSensitiveStringKeyFuncs : BaseKeyFuncs<CacheEntry*, FString>
	{
		FORCEINLINE static const KeyType& GetSetKey( const CacheEntry* Entry )
		{
			return Entry->Key;
		}

		FORCEINLINE static bool Matches(const KeyType& A,const KeyType& B)
		{
			return A.Equals( B, ESearchCase::CaseSensitive );
		}

		FORCEINLINE static uint32 GetKeyHash(const KeyType& Identifier)
		{
			return FCrc::StrCrc32( *Identifier );
		}
	};

	

	/**
	 * Marks the link as the the most recent
	 *
	 * @param Link	The link to mark as most recent
	 */
	FORCEINLINE void MarkAsRecent( CacheEntry* Entry )
	{
		checkSlow( LeastRecent && MostRecent );

		// If we are the least recent entry we are no longer the least recent
		// The previous least recent item is now the most recent.  If it is nullptr then this entry is the only item in the list
		if( Entry == LeastRecent && LeastRecent->Prev != nullptr )
		{
			LeastRecent = LeastRecent->Prev;
		}

		// No need to relink if we happen to already be accessing the most recent value
		if( Entry != MostRecent )
		{
			// Unlink from its current spot
			Entry->Unlink();

			// Relink before the most recent so that we become the most recent
			Entry->Link(MostRecent);
			MostRecent = Entry;
		}
	}

	/**
	 * Removes the least recent item from the cache
	 */
	FORCEINLINE void Eject()
	{
		CacheEntry* EntryToRemove = LeastRecent;
		// Eject the least recent, no more space
		check( EntryToRemove );
		STAT( uint32 CurrentMemUsage = LookupSet.GetAllocatedSize() );
		LookupSet.Remove( EntryToRemove->Key );
		STAT( uint32 NewMemUsage = LookupSet.GetAllocatedSize() );
		DEC_MEMORY_STAT_BY( STAT_SlateFontMeasureCacheMemory,  CurrentMemUsage - NewMemUsage );

		LeastRecent = LeastRecent->Prev;

		// Unlink the LRU
		EntryToRemove->Unlink();

		delete EntryToRemove;
	}

private:
	TSet< CacheEntry*, FCaseSensitiveStringKeyFuncs > LookupSet;
	/** Most recent item in the cache */
	CacheEntry* MostRecent;
	/** Least recent item in the cache */
	CacheEntry* LeastRecent;
	/** The maximum number of elements in the cache */
	int32 MaxNumElements;
};

