// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MRUFavoritesList : Helper class for handling MRU and favorited maps

=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "MRUList.h"

/** Simple class to represent a combined MRU and favorite map list */
class FMainMRUFavoritesList : public FMRUList
{
public:
	/** Constructor */
	UNREALED_API FMainMRUFavoritesList();
	
	/** Destructor */
	UNREALED_API ~FMainMRUFavoritesList();
	
	/** Populate MRU/Favorites list by reading saved values from the relevant INI file */
	virtual void ReadFromINI();

	/** Save off the state of the MRU and favorites lists to the relevant INI file */
	virtual void WriteToINI() const;

	/**
	 * Returns the number of favorites items
	 *
	 * @return	Number of favorites
	 */
	int32 GetNumFavorites() const
	{
		return FavoriteItems.Num();
	}

	/**
	 * Add a new file item to the favorites list
	 *
	 * @param	Item	Filename of the item to add to the favorites list
	 */
	UNREALED_API void AddFavoritesItem( const FString& Item );
	
	/**
	 * Remove a file from the favorites list
	 *
	 * @param	Item	Filename of the item to remove from the favorites list
	 */
	UNREALED_API void RemoveFavoritesItem( const FString& Item );

	/**
	 * Returns whether a filename is favorited or not
	 *
	 * @param	Item	Filename of the item to check
	 *
	 * @return	true if the provided item is in the favorite's list; false if it is not
	 */
	UNREALED_API bool ContainsFavoritesItem( const FString& Item );

	/**
	 * Return the favorites item specified by the provided index
	 *
	 * @param	ItemIndex	Index of the favorites item to return
	 *
	 * @return	The favorites item specified by the provided index
	 */
	UNREALED_API FString GetFavoritesItem( int32 ItemIndex ) const;

	/**
	 * Verifies that the favorites item specified by the provided index still exists. If it does not, the item
	 * is removed from the favorites list and the user is notified.
	 *
	 * @param	ItemIndex	Index of the favorites item to check
	 *
	 * @return	true if the item specified by the index was verified and still exists; false if it does not
	 */
	UNREALED_API bool VerifyFavoritesFile( int32 ItemIndex );

	/**
	 * Moves the specified favorites item to the head of the list
	 *
	 * @param	Item	Filename of the item to move
	 */
	UNREALED_API void MoveFavoritesItemToHead( const FString& Item );

private:

	/** Favorited items */
	TArray<FString> FavoriteItems;

	/** INI section to read/write favorite items to */
	static const FString FAVORITES_INI_SECTION;
};
