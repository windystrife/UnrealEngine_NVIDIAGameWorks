// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ProfilerCommon.h"
#include "ProfilerSample.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SComboBox.h"
#include "Misc/TextFilter.h"
#include "Misc/FilterCollection.h"

class FGroupOrStatNode;
class FProfilerSession;

/** Type definition for shared pointers to instances of FGroupOrStatNode. */
typedef TSharedPtr<class FGroupOrStatNode> FGroupOrStatNodePtr;

/** Type definition for shared references to instances of FGroupOrStatNode. */
typedef TSharedRef<class FGroupOrStatNode> FGroupOrStatNodeRef;

/** Type definition for shared references to const instances of FGroupOrStatNode. */
typedef TSharedRef<const class FGroupOrStatNode> FGroupOrStatNodeRefConst;

/** Type definition for weak references to instances of FGroupOrStatNode. */
typedef TWeakPtr<class FGroupOrStatNode> FGroupOrStatNodeWeak;


/** Enumerates types of grouping or sorting for the group and stat nodes. */
struct EStatGroupingOrSortingMode
{
	enum Type
	{
		/** Group name, taken from the metadata. */
		GroupName,
		/** Stat name, taken from the metadata. */
		StatName,
		/** Stat type, taken from the metadata. */
		StatType,
		/** Current stat value, taken from the profiler session. */
		StatValue,

		/** Invalid enum type, may be used as a number of enumerations. */
		InvalidOrMax,
	};

	/**
	 * @param StatGroupingOrSortingMode - The value to get the text for.
	 *
	 * @return text representation of the specified EStatGroupingOrSortingMode value.
	 */
	static FText ToName( const Type StatGroupingOrSortingMode );

	/**
	 * @param StatGroupingOrSortingMode - The value to get the text for.
	 *
	 * @return text representation with more detailed explanation of the specified EStatGroupingOrSortingMode value.
	 */
	static FText ToDescription( const Type StatGroupingOrSortingMode );

	/**
	 * @param StatGroupingOrSortingMode - The value to get the brush name for.
	 *
	 * @return brush name of the specified EStatGroupingOrSortingMode value.
	 */
	static FName ToBrushName( const Type StatGroupingOrSortingMode );
};


/** The group and stat filter collection - used for updating the list of groups and stats. */
typedef TFilterCollection< const FGroupOrStatNodePtr& > FGroupAndStatFilterCollection;

/** The group and stat text based filter - used for updating the list of groups and stats. */
typedef TTextFilter< const FGroupOrStatNodePtr& > FGroupAndStatTextFilter;


/**
 * Class used to store information about stat and group metadata.
 */
class FGroupOrStatNode
	: public TSharedFromThis<FGroupOrStatNode>
{
public:
	/** Initialization constructor for the stat node. */
	FGroupOrStatNode( const FName InMetaGroupName, const FName InName, uint32 InStatID, const EProfilerSampleTypes::Type InStatType )
		: MetaGroupName( InMetaGroupName )
		, Name( InName )
		, StatID( InStatID )
		, StatType( InStatType )
		, bForceExpandGroupNode( false )
	{}

	/** Initialization constructor for the group node. */
	FGroupOrStatNode( const FName InGroupName )
		: Name( InGroupName )
		, StatID( 0 )
		, StatType( EProfilerSampleTypes::InvalidOrMax )
		, bForceExpandGroupNode( false )
	{}

	/** Sorts children using the specified class instance. */
	template< typename TSortingClass >
	void SortChildren( const TSortingClass& Instance )
	{
		ChildrenPtr.Sort( Instance );
	}

	/** Adds specified child to the children and sets group for it. */
	FORCEINLINE_DEBUGGABLE void AddChildAndSetGroupPtr( const FGroupOrStatNodePtr& ChildPtr ) 
	{
		ChildPtr->GroupPtr = AsShared();
		ChildrenPtr.Add( ChildPtr );
	}

	/** Adds specified child to the filtered children. */
	FORCEINLINE_DEBUGGABLE void AddFilteredChild( const FGroupOrStatNodePtr& ChildPtr ) 
	{
		FilteredChildrenPtr.Add( ChildPtr );
	}

	/** Clears filtered children. */
	void ClearFilteredChildren()
	{
		FilteredChildrenPtr.Reset();
	}

	/**
	 * @return a const reference to the child nodes of this group.
	 */
	FORCEINLINE_DEBUGGABLE const TArray<FGroupOrStatNodePtr>& GetChildren() const 
	{ 
		return ChildrenPtr; 
	}

	/**
	 * @return a const reference to the child nodes that should be visible to the UI based on filtering.
	 */
	FORCEINLINE_DEBUGGABLE const TArray<FGroupOrStatNodePtr>& GetFilteredChildren() const 
	{ 
		return FilteredChildrenPtr; 
	}

	/**
	 * @return true, if this node is a group node.
	 */
	bool IsGroup() const
	{
		return StatType == EProfilerSampleTypes::InvalidOrMax;
	}

	/**
	 * @return a weak reference to the group of this stat node, may be invalid.
	 */
	FGroupOrStatNodeWeak GetGroupPtr() const
	{
		return GroupPtr;
	}

	/**
	 * @return a name of the fake group that this stat node belongs to.
	 */
	const FName& GetGroupName() const
	{
		return GroupPtr.Pin()->Name;
	}

	/**
	 * @return a name of the group that this stat node belongs to, taken from the metadata.
	 */
	const FName& GetMetaGropName() const
	{
		return MetaGroupName;
	}

	/**
	 * @return a name of this node, group or stat.
	 */
	const FName& GetName() const
	{
		return Name;
	}

	/**
	 * @return an ID of this stat, valid only for stat nodes.
	 */
	const uint32 GetStatID() const
	{
		return StatID;
	}

	/**
	 * @return a type of this stat, valid only for stat nodes.
	 */
	const EProfilerSampleTypes::Type& GetStatType() const
	{
		return StatType;
	}

protected:
	/** Children of this node. */
	TArray<FGroupOrStatNodePtr> ChildrenPtr;

	/** Filtered children of this node. */
	TArray<FGroupOrStatNodePtr> FilteredChildrenPtr;

	/** A weak pointer to the group/parent of this node. */
	FGroupOrStatNodeWeak GroupPtr;

	// Last frame stat value ??, selected frame value??

	/** The name of the group that this stat belongs to, based on the stat metadata, only valid for stat nodes. */
	const FName MetaGroupName;

	/** The name of this stat/group. */
	const FName Name;

	// TODO: Until fully switched to FName based stats2
	/** The ID of this stat. */
	const uint32 StatID;
	
	/** Holds the type of this stat, for the group this is InvalidOrMax. */
	const EProfilerSampleTypes::Type StatType;

public:
	/** Whether this group node should be expanded when the text filtering is enabled. */
	bool bForceExpandGroupNode;
};


/** Helper struct that contains sorting classes. */
struct FGroupAndStatSorting
{
	/** For sorting by stat name. */
	struct ByStatName
	{
		FORCEINLINE_DEBUGGABLE bool operator()( const FGroupOrStatNodePtr& A, const FGroupOrStatNodePtr& B ) const 
		{
			return A->GetName() < B->GetName();
		}
	};

	/** For sorting by group name. */
	struct ByGroupName
	{
		FORCEINLINE_DEBUGGABLE bool operator()( const FGroupOrStatNodePtr& A, const FGroupOrStatNodePtr& B ) const 
		{
			return A->GetGroupName() < B->GetGroupName();
		}
	};

	/** For sorting by stat type, if stat type is the same then sort by name. */
	struct ByStatType
	{
		FORCEINLINE_DEBUGGABLE bool operator()( const FGroupOrStatNodePtr& A, const FGroupOrStatNodePtr& B ) const 
		{
			const EProfilerSampleTypes::Type& TypeA = A->GetStatType();
			const EProfilerSampleTypes::Type& TypeB = B->GetStatType();

			if( TypeA == TypeB )
			{
				// Sort by stat name.
				return A->GetName() < B->GetName();
			}
			else
			{
				return TypeA > TypeB;
			}		
		}
	};
};


/** Configurable window with advanced options for filtering and creating presets. */
class SFiltersAndPresets
	: public SCompoundWidget
{
public:
	/** Default constructor. */
	SFiltersAndPresets();

	/** Virtual destructor. */
	virtual ~SFiltersAndPresets();

	SLATE_BEGIN_ARGS(SFiltersAndPresets){}
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 * @param	InArgs	- The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

protected:
	/** Called when the filter and presets widget should be updated with the latest data. */
	void ProfilerManager_OnRequestFilterAndPresetsUpdate();

	void UpdateGroupAndStatTree( const TSharedPtr<FProfilerSession> InProfilerSession );
	void CreateGroups();
	void SortStats();

	/** Populates the group and stat tree with items based on the current data. */
	void ApplyFiltering();

	/**
	 * Populates OutSearchStrings with the strings that should be used in searching.
	 *
	 * @param GroupOrStatNodePtr	- the group and stat node to get a text description from.
	 * @param out_SearchStrings		- an array of strings to use in searching.
	 *
	 */
	void HandleItemToStringArray( const FGroupOrStatNodePtr& GroupOrStatNodePtr, TArray< FString >& out_SearchStrings ) const;

	void CreateGroupByOptionsSources();
	void RecreateSortByOptionsSources();

	TSharedRef<SWidget> GetToggleButtonForStatType( const EProfilerSampleTypes::Type StatType );
	void FilterByStatType_OnCheckStateChanged( ECheckBoxState NewRadioState, const EProfilerSampleTypes::Type InStatType );
	ECheckBoxState FilterByStatType_IsChecked( const EProfilerSampleTypes::Type InStatType ) const;

	/*-----------------------------------------------------------------------------
		GroupAndStatTree
	-----------------------------------------------------------------------------*/

	/** Called by STreeView to generate a table row for the specified item. */
	TSharedRef< ITableRow > GroupAndStatTree_OnGenerateRow( FGroupOrStatNodePtr TreeNode, const TSharedRef< STableViewBase >& OwnerTable );

	/**
	 * Called by STreeView to retrieves the children for the specified parent item.
	 * @param InParent		- The parent node to retrieve the children from.
	 * @param out_Children	- List of children for the parent node.
	 */
	void GroupAndStatTree_OnGetChildren( FGroupOrStatNodePtr InParent, TArray< FGroupOrStatNodePtr >& out_Children );

	/** Called by STreeView when a tree item is double clicked. */
	void GroupAndStatTree_OnMouseButtonDoubleClick( FGroupOrStatNodePtr TreeNode );

	/**
	 * Update the highlight string in the stats and groups tree.
	 */
	FText GroupAndStatTableRow_GetHighlightText() const;

	bool GroupAndStatTableRow_ShouldBeEnabled( const uint32 StatID ) const;

	/*-----------------------------------------------------------------------------
		SearchBox
	-----------------------------------------------------------------------------*/

	void SearchBox_OnTextChanged( const FText& InFilterText );

	bool SearchBox_IsEnabled() const;

	/*-----------------------------------------------------------------------------
		GroupBy
	-----------------------------------------------------------------------------*/

	void GroupBy_OnSelectionChanged( TSharedPtr<EStatGroupingOrSortingMode::Type> NewGroupingMode, ESelectInfo::Type SelectInfo );

	TSharedRef<SWidget> GroupBy_OnGenerateWidget( TSharedPtr<EStatGroupingOrSortingMode::Type> InGroupingMode ) const;

	FText GroupBy_GetSelectedText() const;

	FText GroupBy_GetSelectedTooltipText() const;

	/*-----------------------------------------------------------------------------
		SortBy
	-----------------------------------------------------------------------------*/

	void SortBy_OnSelectionChanged( TSharedPtr<EStatGroupingOrSortingMode::Type> NewSortingMode, ESelectInfo::Type SelectInfo );

	TSharedRef<SWidget> SortBy_OnGenerateWidget( TSharedPtr<EStatGroupingOrSortingMode::Type> InSortingMode ) const;

	FText SortBy_GetSelectedText() const;

protected:
	/** An array of group and stat nodes generated from the metadata. */
	TArray< FGroupOrStatNodePtr > GroupNodes;

	/** A filtered array of group and stat nodes to be displayed in the tree widget. */
	TArray< FGroupOrStatNodePtr > FilteredGroupNodes;

	/** All stat nodes collected during the profiling session, stored as StatName -> FGroupOrStatNodePtr. */
	TMap< FName, FGroupOrStatNodePtr > StatNodesMap;

	/** Currently expanded group nodes. */
	TSet<FGroupOrStatNodePtr> ExpandedNodes;

	TArray<TSharedPtr<EStatGroupingOrSortingMode::Type>> GroupByOptionsSource;
	TArray<TSharedPtr<EStatGroupingOrSortingMode::Type>> SortByOptionsSource;

	TSharedPtr< SComboBox< TSharedPtr<EStatGroupingOrSortingMode::Type> > > GroupByComboBox;

	TSharedPtr< SComboBox< TSharedPtr<EStatGroupingOrSortingMode::Type> > > SortByComboBox;

	/** The tree widget which holds the list of stat groups and stats corresponding with each group. */
	TSharedPtr< STreeView< FGroupOrStatNodePtr > > GroupAndStatTree;

	/** The search box widget used to filter items displayed in the stats and groups tree. */
	TSharedPtr< SSearchBox > GroupAndStatSearchBox;

	/** Group and stat text based filter. */
	TSharedPtr< FGroupAndStatTextFilter > GroupAndStatTextFilter;

	/** Group and stat filter collection. */
	TSharedPtr< FGroupAndStatFilterCollection > GroupAndStatFilters;
	
	/** A weak pointer to the profiler session used to populate this widget. */
	TSharedPtr<FProfilerSession>/*Weak*/ ProfilerSession;

	/** How we group the metadata?. */
	EStatGroupingOrSortingMode::Type GroupingMode;

	/** How we sort the metadata?. */
	EStatGroupingOrSortingMode::Type SortingMode;

	/** If true, the expanded nodes have been saved before applying a text filter. */
	bool bExpansionSaved;

	/** Holds the visibility of each stat type. */
	bool bStatTypeIsVisible[EProfilerSampleTypes::InvalidOrMax];
};
