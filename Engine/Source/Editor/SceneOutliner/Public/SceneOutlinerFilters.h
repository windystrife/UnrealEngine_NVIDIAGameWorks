// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/IFilter.h"
#include "Misc/FilterCollection.h"

#include "FolderTreeItem.h"
#include "ActorTreeItem.h"
#include "WorldTreeItem.h"

namespace SceneOutliner
{

	class FOutlinerFilterInfo
	{ 
	public:
		FOutlinerFilterInfo(const FText& InFilterTitle, const FText& InFilterTooltip, bool bInActive, const FCreateSceneOutlinerFilter& InFactory = FCreateSceneOutlinerFilter())
			: FilterTitle(InFilterTitle)
			, FilterTooltip(InFilterTooltip)
			, bActive(bInActive)
			, Factory(InFactory)
		{}

		/** Initialize and apply a new filter */
		void InitFilter(TSharedPtr<FOutlinerFilters> InFilters);

		/** Add menu for this filter */
		void AddMenu(FMenuBuilder& InMenuBuilder);

	private:
		void ApplyFilter(bool bActive);
		void ToggleFilterActive();
		bool IsFilterActive() const;

		TWeakPtr<FOutlinerFilters> Filters;

		TSharedPtr<FOutlinerFilter> Filter;

		FText FilterTitle;
		FText FilterTooltip;
		bool bActive;

		FCreateSceneOutlinerFilter Factory;
	};

	/** Enum to specify how items that are not explicitly handled by this filter should be managed */
	enum class EDefaultFilterBehaviour : uint8 { Pass, Fail };

	/** Enum that defines how a tree item should be dealt with in the case where it appears in the tree, but doesn't match the filter (eg if it has a matching child) */
	enum class EFailedFilterState : uint8 { Interactive, NonInteractive };

	/** A filter that can be applied to any type in the tree */
	class FOutlinerFilter : public ITreeItemVisitor, public IFilter<const ITreeItem&>
	{
	public:

		/** Event that is fired if this filter changes */
		DECLARE_DERIVED_EVENT(FOutlinerFilter, IFilter<const ITreeItem&>::FChangedEvent, FChangedEvent);
		virtual FChangedEvent& OnChanged() override { return ChangedEvent; }
		
		/** Enum that defines how a tree item should be dealt with in the case where it appears in the tree, but doesn't match the filter (eg if it has a matching child) */
		EFailedFilterState FailedItemState;

	protected:

		/**	The event that broadcasts whenever a change occurs to the filter */
		FChangedEvent ChangedEvent;

		/** Default result of the filter when not overridden in derived classes */
		const EDefaultFilterBehaviour DefaultBehaviour;

		/** Constructor to specify the default result of a filter */
		FOutlinerFilter(EDefaultFilterBehaviour InDefaultBehaviour, EFailedFilterState InFailedFilterState = EFailedFilterState::NonInteractive)
			: FailedItemState(InFailedFilterState), DefaultBehaviour(InDefaultBehaviour)
		{}

		/** Overridden in derived types to filter actors */
		virtual bool PassesFilter(const AActor* Actor) const { return DefaultBehaviour == EDefaultFilterBehaviour::Pass; }
		
		/** Overridden in derived types to filter worlds */
		virtual bool PassesFilter(const UWorld* World) const { return DefaultBehaviour == EDefaultFilterBehaviour::Pass; }

		/** Overridden in derived types to filter folders */
		virtual bool PassesFilter(FName Folder) const { return DefaultBehaviour == EDefaultFilterBehaviour::Pass; }

	private:

		/** Transient result from the filter operation. Only valid until the next invocation of the filter. */
		mutable bool bTransientFilterResult;

		virtual void Visit(const FActorTreeItem& ActorItem) const override
		{
			if (const AActor* Actor = ActorItem.Actor.Get())
			{
				bTransientFilterResult = PassesFilter(Actor);
			}
			else
			{
				bTransientFilterResult = false;
			}
		}

		virtual void Visit(const FWorldTreeItem& WorldItem) const override
		{
			if (const UWorld* World = WorldItem.World.Get())
			{
				bTransientFilterResult = PassesFilter(World);
			}
			else
			{
				bTransientFilterResult = false;
			}
		}

		virtual void Visit(const FFolderTreeItem& FolderItem) const override
		{
			bTransientFilterResult = PassesFilter(FolderItem.Path);
		}

		/** Check whether the specified item passes our filter */
		virtual bool PassesFilter( const ITreeItem& InItem ) const override
		{
			InItem.Visit(*this);
			return bTransientFilterResult;
		}
	};

	DECLARE_DELEGATE_RetVal_OneParam( bool, FActorFilterPredicate, const AActor* );
	DECLARE_DELEGATE_RetVal_OneParam( bool, FWorldFilterPredicate, const UWorld* );
	DECLARE_DELEGATE_RetVal_OneParam( bool, FFolderFilterPredicate, FName );

	/** Predicate based filter for the outliner */
	struct FOutlinerPredicateFilter : public FOutlinerFilter
	{
		/** Predicate used to filter actors */
		mutable FActorFilterPredicate	ActorPred;
		/** Predicate used to filter worlds */
		mutable FWorldFilterPredicate	WorldPred;
		/** Predicate used to filter Folders */
		mutable FFolderFilterPredicate	FolderPred;

		FOutlinerPredicateFilter(FActorFilterPredicate InActorPred, EDefaultFilterBehaviour InDefaultBehaviour, EFailedFilterState InFailedFilterState = EFailedFilterState::NonInteractive)
			: FOutlinerFilter(InDefaultBehaviour, InFailedFilterState)
			, ActorPred(InActorPred)
		{}

		FOutlinerPredicateFilter(FWorldFilterPredicate InWorldPred, EDefaultFilterBehaviour InDefaultBehaviour, EFailedFilterState InFailedFilterState = EFailedFilterState::NonInteractive)
			: FOutlinerFilter(InDefaultBehaviour, InFailedFilterState)
			, WorldPred(InWorldPred)
		{}

		FOutlinerPredicateFilter(FFolderFilterPredicate InFolderPred, EDefaultFilterBehaviour InDefaultBehaviour, EFailedFilterState InFailedFilterState = EFailedFilterState::NonInteractive)
			: FOutlinerFilter(InDefaultBehaviour, InFailedFilterState)
			, FolderPred(InFolderPred)
		{}

		virtual bool PassesFilter(const AActor* Actor) const override
		{
			return ActorPred.IsBound() ? ActorPred.Execute(Actor) : DefaultBehaviour == EDefaultFilterBehaviour::Pass;
		}

		virtual bool PassesFilter(const UWorld* World) const override
		{
			return WorldPred.IsBound() ? WorldPred.Execute(World) : DefaultBehaviour == EDefaultFilterBehaviour::Pass;
		}

		virtual bool PassesFilter(FName Folder) const override
		{
			return FolderPred.IsBound() ? FolderPred.Execute(Folder) : DefaultBehaviour == EDefaultFilterBehaviour::Pass;
		}
	};

	/** Scene outliner filter class. This class abstracts the filtering of both actors and folders and allows for filtering on both types */
	struct FOutlinerFilters : public TFilterCollection<const ITreeItem&>
	{
		/** Overridden to ensure we only ever have FOutlinerFilters added */
		int32 Add(const TSharedPtr<FOutlinerFilter>& Filter)
		{
			return TFilterCollection::Add(Filter);
		}

		/** Test whether this tree item passes all filters, and set its interactive state according to the filter it failed (if applicable) */
		bool TestAndSetInteractiveState(ITreeItem& InItem) const
		{
			bool bPassed = true;

			// Default to interactive
			InItem.Flags.bInteractive = true;
			
			for (const auto& Filter : ChildFilters)
			{
				if (!Filter->PassesFilter(InItem))
				{
					bPassed = false;
					InItem.Flags.bInteractive = StaticCastSharedPtr<FOutlinerFilter>(Filter)->FailedItemState == EFailedFilterState::Interactive;
					// If this has failed, but is still interactive, we carry on to see if any others fail *and* set to non-interactive
					if (!InItem.Flags.bInteractive)
					{
						return false;
					}
				}
			}

			return bPassed;
		}

		/** Add a filter predicate to this filter collection */
		template<typename T>
		void AddFilterPredicate(T Predicate, EDefaultFilterBehaviour InDefaultBehaviour = EDefaultFilterBehaviour::Fail, EFailedFilterState InFailedFilterState = EFailedFilterState::NonInteractive)
		{
			Add(MakeShareable(new FOutlinerPredicateFilter(Predicate, InDefaultBehaviour, InFailedFilterState)));
		}
	};
}
