// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"

struct FTabSpawnerEntry;

class FWorkspaceItem : public TSharedFromThis<FWorkspaceItem>
{
protected:
	struct FWorkspaceItemSort
	{
		FORCEINLINE bool operator()( const TSharedRef<FWorkspaceItem> A, const TSharedRef<FWorkspaceItem> B ) const
		{
			if ( A->GetChildItems().Num() > 0 )
			{
				if ( B->GetChildItems().Num() == 0 )
				{
					return true;
				}
			}
			else if ( B->GetChildItems().Num() > 0 )
			{
				return false;
			}
			return ( A->GetDisplayName().CompareTo( B->GetDisplayName() ) == -1 );
		}
	};

public:
	static TSharedRef<FWorkspaceItem> NewGroup( const FText& DisplayName, const FSlateIcon& Icon = FSlateIcon(), const bool bSortChildren = false )
	{
		return MakeShareable( new FWorkspaceItem( DisplayName, Icon, bSortChildren ) );
	}

	static TSharedRef<FWorkspaceItem> NewGroup( const FText& DisplayName, const FText& TooltipText, const FSlateIcon& Icon = FSlateIcon(), const bool bSortChildren = false )
	{
		return MakeShareable( new FWorkspaceItem( DisplayName, TooltipText, Icon, bSortChildren ) );
	}

	TSharedRef<FWorkspaceItem> AddGroup( const FText& InDisplayName, const FSlateIcon& InIcon = FSlateIcon(), const bool InSortChildren = false )
	{
		TSharedRef<FWorkspaceItem> NewItem = FWorkspaceItem::NewGroup(InDisplayName, InIcon, InSortChildren);
		AddItem( NewItem );
		return NewItem;
	}

	TSharedRef<FWorkspaceItem> AddGroup( const FText& InDisplayName, const FText& InTooltipText, const FSlateIcon& InIcon = FSlateIcon(), const bool InSortChildren = false )
	{
		TSharedRef<FWorkspaceItem> NewItem = FWorkspaceItem::NewGroup(InDisplayName, InTooltipText, InIcon, InSortChildren);
		AddItem( NewItem );
		return NewItem;
	}

	const FText& GetDisplayName() const
	{
		return DisplayName;
	}
	
	const FText& GetTooltipText() const
	{
		return TooltipText;
	}

	const FSlateIcon& GetIcon() const
	{
		return Icon;
	}

	const TArray< TSharedRef<FWorkspaceItem> >& GetChildItems() const
	{
		return ChildItems;
	}

	void AddItem( const TSharedRef<FWorkspaceItem>& ItemToAdd )
	{
		ItemToAdd->ParentItem = SharedThis(this);
		ChildItems.Add( ItemToAdd );

		// If desired of this menu, sort the children
		if ( bSortChildren )
		{
			SortChildren();
		}

		// If this is our first child, our parent item may need sorting, resort it now
		if ( ChildItems.Num() == 1 && ParentItem.IsValid() && ParentItem.Pin()->bSortChildren )
		{
			ParentItem.Pin()->SortChildren();
		}
	}

	void RemoveItem( const TSharedRef<FWorkspaceItem>& ItemToRemove )
	{
		ChildItems.Remove(ItemToRemove);
	}

	void ClearItems()
	{
		ChildItems.Reset();
	}

	void SortChildren()
	{
		ChildItems.Sort( FWorkspaceItemSort() );
	}

	virtual TSharedPtr<FTabSpawnerEntry> AsSpawnerEntry()
	{
		return TSharedPtr<FTabSpawnerEntry>();
	}


 	TSharedPtr<FWorkspaceItem> GetParent() const
 	{
 		return ParentItem.Pin();
 	}

	bool HasChildrenIn( const TArray< TWeakPtr<FTabSpawnerEntry> >& AllowedSpawners )
	{
		// Spawner Entries are leaves. If this is a spawner entry and it allowed in this menu, then 
		// any group containing this node is populated.
		const TSharedPtr<FTabSpawnerEntry> ThisAsSpawnerEntry = this->AsSpawnerEntry();
		bool bIsGroupPopulated = ThisAsSpawnerEntry.IsValid() && AllowedSpawners.Contains(ThisAsSpawnerEntry.ToSharedRef());

		// Look through all the children of this node and see if any of them are populated
		for ( int32 ChildIndex=0; !bIsGroupPopulated && ChildIndex < ChildItems.Num(); ++ChildIndex )
		{
			const TSharedRef<FWorkspaceItem>& ChildItem = ChildItems[ChildIndex];
			if ( ChildItem->HasChildrenIn(AllowedSpawners) )
			{
				bIsGroupPopulated = true;
			}
		}

		return bIsGroupPopulated;
	}

	virtual ~FWorkspaceItem()
	{
	}

protected:
	FWorkspaceItem( const FText& InDisplayName, const FSlateIcon& InIcon, const bool bInSortChildren )
		: Icon(InIcon)
		, DisplayName(InDisplayName)
		, bSortChildren(bInSortChildren)
	{
	}

	FWorkspaceItem( const FText& InDisplayName, const FText& InTooltipText, const FSlateIcon& InIcon, const bool bInSortChildren )
		: Icon(InIcon)
		, DisplayName(InDisplayName)
		, TooltipText(InTooltipText)
		, bSortChildren(bInSortChildren)
	{
	}

	FSlateIcon Icon;
	FText DisplayName;
	FText TooltipText;
	bool bSortChildren;

	TArray< TSharedRef<FWorkspaceItem> > ChildItems;

	TWeakPtr<FWorkspaceItem> ParentItem;
};
