// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/BaseToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "Widgets/Docking/SDockableTab.h"
#include "Widgets/Docking/SDockTabStack.h"

#define LOCTEXT_NAMESPACE "BaseToolkit"

FBaseToolkit::FBaseToolkit()
	: ToolkitMode( EToolkitMode::Standalone ),
	  ToolkitCommands( new FUICommandList() )
{
	WorkspaceMenuCategory = FWorkspaceItem::NewGroup(LOCTEXT("WorkspaceMenu_BaseToolkit", "Toolkit"));
}


FBaseToolkit::~FBaseToolkit()
{
	// Destroy any tabs that we still have hanging out.  This is just for convenience, so that the derived
	// classes don't need to bother cleaning up their tabs when a world-centric editor is closed.  However,
	// if the order that tabs are destroyed is important, it is suggested that you clean the up yourself!
	{
		for( auto CurSpotIt( ToolkitTabsInSpots.CreateConstIterator() ); CurSpotIt; ++CurSpotIt )
		{
			const auto& TabsForSpot = CurSpotIt.Value();
			for( auto CurTabIt( TabsForSpot.CreateConstIterator() ); CurTabIt; ++CurTabIt )
			{
				const auto& PinnedTab = CurTabIt->Pin();
				if( PinnedTab.IsValid() )
				{
					PinnedTab->RemoveTabFromParent();
				}
			}
		}
	}
}



bool FBaseToolkit::IsWorldCentricAssetEditor() const
{
	return ToolkitMode == EToolkitMode::WorldCentric;
}



bool FBaseToolkit::IsHosted() const
{
	return ToolkitHost.IsValid();
}


const TSharedRef< IToolkitHost > FBaseToolkit::GetToolkitHost() const
{
	return ToolkitHost.Pin().ToSharedRef();
}


const TMap< EToolkitTabSpot::Type, TArray< TWeakPtr< SDockableTab > > >& FBaseToolkit::GetToolkitTabsInSpots() const
{
	return ToolkitTabsInSpots;
}


FName FBaseToolkit::GetToolkitContextFName() const
{
	return GetToolkitFName();
}


bool FBaseToolkit::ProcessCommandBindings( const FKeyEvent& InKeyEvent ) const
{
	if( ToolkitCommands->ProcessCommandBindings( InKeyEvent ) )
	{
		return true;
	}
	
	return false;
}


void FBaseToolkit::AddToolkitTab( const TSharedRef< SDockableTab >& TabToAdd, const EToolkitTabSpot::Type TabSpot )
{
	// Figure out where to put this tab by asking the toolkit host which spot to put it in.  It will give us
	// back a dock tab stack that we can add the tab to!
	TSharedRef< SDockTabStack > FoundTabStack = GetToolkitHost()->GetTabSpot( TabSpot );

	// When a context menu is opening for this tab, ask the spawning app to fill the menu.
	ensureMsgf(false, TEXT("TabToAdd->SetOnTabStackMenuOpening( FOnTabStackMenuOpening::CreateSP( GetToolkitHost(), &IToolkitHost::PopulateLayoutMenu ) );"));
	//TabToAdd->SetOnTabStackMenuOpening( FOnTabStackMenuOpening::CreateSP( GetToolkitHost(), &IToolkitHost::PopulateLayoutMenu ) );

	// Add the tab
	FoundTabStack->AddTab( TabToAdd );

	// Keep track of tabs
	ToolkitTabsInSpots.FindOrAdd( TabSpot ).Add( TabToAdd );
}

FString FBaseToolkit::GetTabPrefix() const
{
	if( IsWorldCentricAssetEditor() )
	{
		return GetWorldCentricTabPrefix();
	}
	else
	{
		return TEXT( "" );
	}
}


FLinearColor FBaseToolkit::GetTabColorScale() const
{
	return IsWorldCentricAssetEditor() ? GetWorldCentricTabColorScale() : FLinearColor( 0, 0, 0, 0 );
}


void FBaseToolkit::BringToolkitToFront()
{
	if( ensure( ToolkitHost.IsValid() ) )
	{
		// Bring the host window to front
		ToolkitHost.Pin()->BringToFront();

		// First, figure out what the foreground tab is in each tab stack we have tabs docked inside of
		TSet< SDockTabStack* > TabStacksWithOurTabsForegrounded;
		{
			for( auto CurSpotIt( ToolkitTabsInSpots.CreateConstIterator() ); CurSpotIt; ++CurSpotIt )
			{
				const auto& TabsForSpot = CurSpotIt.Value();
				for( auto CurTabIt( TabsForSpot.CreateConstIterator() ); CurTabIt; ++CurTabIt )
				{
					const auto& PinnedTab = CurTabIt->Pin();
					if( PinnedTab.IsValid() )
					{
						if( PinnedTab->IsForeground() )
						{
							const auto& TabStack = PinnedTab->GetParentDockTabStack();
							if( TabStack.IsValid() )
							{
								TabStacksWithOurTabsForegrounded.Add( TabStack.Get() );
							}
						}
					}
				}
			}
		}

		// @todo toolkit major: Also draw user's attention when clicked?

		// @todo toolkit major: If any of the tabs are in their own floating windows, these should be brought to front

		// Now, make sure that our tabs are foregrounded in their respective stacks!
		// NOTE: We don't want to push tabs to the front that are in a stack where one of our other tabs is already front-most
		for( auto CurSpotIt( ToolkitTabsInSpots.CreateConstIterator() ); CurSpotIt; ++CurSpotIt )
		{
			const auto& TabsForSpot = CurSpotIt.Value();
			for( auto CurTabIt( TabsForSpot.CreateConstIterator() ); CurTabIt; ++CurTabIt )
			{
				const auto& PinnedTab = CurTabIt->Pin();

				if( PinnedTab.IsValid() )
				{
					const auto& TabStack = PinnedTab->GetParentDockTabStack();
					if( TabStack.IsValid() )
					{
						// Only foreground if we don't already have a tab foregrounded in this tab's stack
						if( !TabStacksWithOurTabsForegrounded.Contains( TabStack.Get() ) )
						{
							PinnedTab->BringToFrontInParent();

							// Take note that we've foregrounded a tab in this stack, no need to do that again
							TabStacksWithOurTabsForegrounded.Add( TabStack.Get() );
						}
					}
					else
					{
						// Just do what we can to foreground ourselves
						PinnedTab->BringToFrontInParent();
					}
				}
			}
		}
		// Tell the toolkit its been brought to the fore - give it a chance to update anything it needs to
		ToolkitBroughtToFront();
	}
}

TSharedPtr<class SWidget> FBaseToolkit::GetInlineContent() const
{
	return TSharedPtr<class SWidget>();
}


bool FBaseToolkit::IsBlueprintEditor() const
{
	return false;
}

#undef LOCTEXT_NAMESPACE


void FModeToolkit::Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost)
{
	check( InitToolkitHost.IsValid() );
	
	ToolkitMode = EToolkitMode::WorldCentric;
	ToolkitHost = InitToolkitHost;

	FToolkitManager::Get().RegisterNewToolkit( SharedThis( this ) );
}

FString FModeToolkit::GetWorldCentricTabPrefix() const
{
	return FString();
}

bool FModeToolkit::IsAssetEditor() const
{
	return false;
}

const TArray< UObject* >* FModeToolkit::GetObjectsCurrentlyBeingEdited() const
{
	return NULL;
}

FLinearColor FModeToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor();
}
