// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "SceneOutlinerPublicTypes.h"
#include "Misc/EnumRange.h"
#include "ISceneOutlinerColumn.h"

class ISceneOutliner;

namespace SceneOutliner
{

/** Types of actor data we can display in a 'custom' tree column */
namespace ECustomColumnMode
{
	enum Type
	{
		/** Empty column -- doesn't display anything */
		None = 0,

		/** Class */
		Class,

		/** Level */
		Level,

		/** Layer */
		Layer,

		/** The socket the actor is attached to. */
		Socket,

		/** Actor's internal name (FName) */
		InternalName,

		/** Actor's number of uncached lights */
		UncachedLights,

		// ---

		/** Number of options */
		Count
	};
}

/**
 * A custom column for the SceneOutliner which is capable of displaying a variety of Actor details
 */
class FActorInfoColumn : public ISceneOutlinerColumn
{

public:

	/**
	 *	Constructor
	 */
	FActorInfoColumn( ISceneOutliner& Outliner, ECustomColumnMode::Type InDefaultCustomColumnMode = ECustomColumnMode::Class );

	virtual ~FActorInfoColumn() {}

	static FName GetID() { return FBuiltInColumnTypes::ActorInfo(); }

	//////////////////////////////////////////////////////////////////////////
	// Begin ISceneOutlinerColumn Implementation

	virtual FName GetColumnID() override;

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;

	virtual const TSharedRef< SWidget > ConstructRowWidget( FTreeItemRef TreeItem, const STableRow<FTreeItemPtr>& Row ) override;

	virtual void PopulateSearchStrings( const ITreeItem& Item, TArray< FString >& OutSearchStrings ) const override;

	virtual bool SupportsSorting() const override;

	virtual void SortItems(TArray<FTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const override;
	
	// End ISceneOutlinerColumn Implementation
	//////////////////////////////////////////////////////////////////////////

	FText GetTextForItem( const TWeakPtr<ITreeItem> TreeItem ) const;

private:

	TSharedPtr<SWidget> ConstructClassHyperlink( ITreeItem& TreeItem );

	void OnModeChanged( TSharedPtr< ECustomColumnMode::Type > NewSelection, ESelectInfo::Type SelectInfo );

	EVisibility GetColumnDataVisibility( bool bIsClassHyperlink ) const;

	FText MakeComboText( const ECustomColumnMode::Type& Mode ) const;

	FText MakeComboToolTipText( const ECustomColumnMode::Type& Mode );

	TSharedRef< ITableRow > MakeComboButtonItemWidget( TSharedPtr< ECustomColumnMode::Type > Mode, const TSharedRef<STableViewBase> & );

	FText GetSelectedMode() const;

private:

	/** 
	 * Current custom column mode.  This is used for displaying a bit of extra data about the actors, as well as
	 * allowing the user to search by additional criteria 
	 */
	mutable ECustomColumnMode::Type CurrentMode;

	/** A list of available custom column modes for Slate */
	static TArray< TSharedPtr< ECustomColumnMode::Type > > ModeOptions;

	/** Weak reference to the outliner widget that owns our list */
	TWeakPtr< ISceneOutliner > SceneOutlinerWeak;
};


}	// namespace SceneOutliner

ENUM_RANGE_BY_COUNT(SceneOutliner::ECustomColumnMode::Type, SceneOutliner::ECustomColumnMode::Count)
