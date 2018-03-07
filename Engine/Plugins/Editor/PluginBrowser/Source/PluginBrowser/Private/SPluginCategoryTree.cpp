// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SPluginCategoryTree.h"
#include "Interfaces/IPluginManager.h"
#include "SPluginCategory.h"
#include "SPluginBrowser.h"

#define LOCTEXT_NAMESPACE "PluginCategories"



void SPluginCategoryTree::Construct( const FArguments& Args, const TSharedRef< SPluginBrowser > Owner )
{
	OwnerWeak = Owner;

	// Create the root categories
	BuiltInCategory = MakeShareable(new FPluginCategory(NULL, TEXT("Built-In"), LOCTEXT("BuiltInCategoryName", "Built-In")));
	InstalledCategory = MakeShareable(new FPluginCategory(NULL, TEXT("Installed"), LOCTEXT("InstalledCategoryName", "Installed")));
	ProjectCategory = MakeShareable(new FPluginCategory(NULL, TEXT("Project"), LOCTEXT("ProjectCategoryName", "Project")));
	ModCategory = MakeShareable(new FPluginCategory(NULL, TEXT("Mods"), LOCTEXT("ModsCategoryName", "Mods")));

	// Create the tree view control
	TreeView =
		SNew( STreeView<TSharedPtr<FPluginCategory>> )

		// For now we only support selecting a single folder in the tree
		.SelectionMode( ESelectionMode::Single )
		.ClearSelectionOnClick( false )		// Don't allow user to select nothing.  We always expect a category to be selected!

		.TreeItemsSource( &RootCategories )
		.OnGenerateRow( this, &SPluginCategoryTree::PluginCategoryTreeView_OnGenerateRow ) 
		.OnGetChildren( this, &SPluginCategoryTree::PluginCategoryTreeView_OnGetChildren )

		.OnSelectionChanged( this, &SPluginCategoryTree::PluginCategoryTreeView_OnSelectionChanged )
		;

	RebuildAndFilterCategoryTree();

	ChildSlot.AttachWidget( TreeView.ToSharedRef() );
}


SPluginCategoryTree::~SPluginCategoryTree()
{
}


/** @return Gets the owner of this list */
SPluginBrowser& SPluginCategoryTree::GetOwner()
{
	return *OwnerWeak.Pin();
}


static void ResetCategories(TArray<TSharedPtr<FPluginCategory>>& Categories)
{
	for (const TSharedPtr<FPluginCategory>& Category : Categories)
	{
		ResetCategories(Category->SubCategories);
		Category->Plugins.Reset();
		Category->SubCategories.Reset();
	}
}

void SPluginCategoryTree::RebuildAndFilterCategoryTree()
{
	// Get a plugin from the currently selected category, so we can track it if it's removed
	TSharedPtr<IPlugin> TrackPlugin = nullptr;
	for(TSharedPtr<FPluginCategory> SelectedItem: TreeView->GetSelectedItems())
	{
		if(SelectedItem->Plugins.Num() > 0)
		{
			TrackPlugin = SelectedItem->Plugins[0];
			break;
		}
	}

	// Clear the list of plugins in each current category
	ResetCategories(RootCategories);

	// Add all the known plugins into categories
	TSharedPtr<FPluginCategory> SelectCategory;
	for(TSharedRef<IPlugin> Plugin: IPluginManager::Get().GetDiscoveredPlugins())
	{
		if (Plugin->IsHidden())
		{
			continue;
		}

		// Figure out which base category this plugin belongs in
		TSharedPtr<FPluginCategory> RootCategory;
		if (Plugin->GetType() == EPluginType::Mod)
		{
			RootCategory = ModCategory;
		}
		else if(Plugin->GetDescriptor().bInstalled)
		{
			RootCategory = InstalledCategory;
		}
		else if(Plugin->GetLoadedFrom() == EPluginLoadedFrom::Engine)
		{
			RootCategory = BuiltInCategory;
		}
		else
		{
			RootCategory = ProjectCategory;
		}

		// Get the subcategory for this plugin
		FString CategoryName = Plugin->GetDescriptor().Category;
		if(CategoryName.IsEmpty())
		{
			CategoryName = TEXT("Other");
		}

		// Locate this category at the level we're at in the hierarchy
		TSharedPtr<FPluginCategory> FoundCategory = NULL;
		for(TSharedPtr<FPluginCategory> TestCategory: RootCategory->SubCategories)
		{
			if(TestCategory->Name == CategoryName)
			{
				FoundCategory = TestCategory;
				break;
			}
		}

		if( !FoundCategory.IsValid() )
		{
			//@todo Allow for properly localized category names [3/7/2014 justin.sargent]
			FoundCategory = MakeShareable(new FPluginCategory(RootCategory, CategoryName, FText::FromString(CategoryName)));
			RootCategory->SubCategories.Add( FoundCategory );
		}
			
		// Associate the plugin with the category
		FoundCategory->Plugins.Add(Plugin);

		TSharedPtr<FPluginCategory> ParentCategory = FoundCategory->ParentCategory.Pin();
		while (ParentCategory.IsValid())
		{
			ParentCategory->Plugins.Add(Plugin);
			ParentCategory = ParentCategory->ParentCategory.Pin();
		}

		// Update the selection if this is the plugin we're tracking
		if(TrackPlugin == Plugin)
		{
			SelectCategory = FoundCategory;
		}
	}

	// Remove any empty categories, keeping track of which items are still selected
	for(TSharedPtr<FPluginCategory> RootCategory: RootCategories)
	{
		for(int32 Idx = 0; Idx < RootCategory->SubCategories.Num(); Idx++)
		{
			if(RootCategory->SubCategories[Idx]->Plugins.Num() == 0)
			{
				RootCategory->SubCategories.RemoveAt(Idx);
			}
		}
	}

	// Build the new list of root plugin categories
	RootCategories.Reset();
	if(ModCategory->SubCategories.Num() > 0 || ModCategory->Plugins.Num() > 0)
	{
		RootCategories.Add(ModCategory);
	}
	if(InstalledCategory->SubCategories.Num() > 0 || InstalledCategory->Plugins.Num() > 0)
	{
		RootCategories.Add(InstalledCategory);
	}
	if(BuiltInCategory->SubCategories.Num() > 0 || BuiltInCategory->Plugins.Num() > 0)
	{
		RootCategories.Add(BuiltInCategory);
	}
	if(ProjectCategory->SubCategories.Num() > 0 || ProjectCategory->Plugins.Num() > 0)
	{
		RootCategories.Add(ProjectCategory);
	}

	// Sort every single category alphabetically
	for(TSharedPtr<FPluginCategory> RootCategory: RootCategories)
	{
		RootCategory->SubCategories.Sort([](const TSharedPtr<FPluginCategory>& A, const TSharedPtr<FPluginCategory>& B) -> bool { return A->DisplayName.CompareTo(B->DisplayName) < 0; });
	}

	// Expand all the root categories by default
	for(TSharedPtr<FPluginCategory> RootCategory: RootCategories)
	{
		TreeView->SetItemExpansion(RootCategory, true);
	}

	// Refresh the view
	TreeView->RequestTreeRefresh();

	// Make sure we have something selected
	if(RootCategories.Num() > 0)
	{
		if(SelectCategory.IsValid())
		{
			TreeView->SetSelection(SelectCategory);
		}
		else if(RootCategories.Contains(ModCategory))
		{
			TreeView->SetSelection(ModCategory);
		}
		else if(RootCategories.Contains(InstalledCategory))
		{
			TreeView->SetSelection(InstalledCategory);
		}
		else if(RootCategories.Num() > 0)
		{
			TreeView->SetSelection(RootCategories[0]);
		}
	}
}

TSharedRef<ITableRow> SPluginCategoryTree::PluginCategoryTreeView_OnGenerateRow( TSharedPtr<FPluginCategory> Item, const TSharedRef<STableViewBase>& OwnerTable )
{
	return
		SNew( STableRow<TSharedPtr<FPluginCategory>>, OwnerTable )
		[
			SNew( SPluginCategory, Item.ToSharedRef() )
		];
}


void SPluginCategoryTree::PluginCategoryTreeView_OnGetChildren(TSharedPtr<FPluginCategory> Item, TArray<TSharedPtr<FPluginCategory>>& OutChildren )
{
	OutChildren.Append(Item->SubCategories);
}


void SPluginCategoryTree::PluginCategoryTreeView_OnSelectionChanged(TSharedPtr<FPluginCategory> Item, ESelectInfo::Type SelectInfo )
{
	// Selection changed, which may affect which plugins are displayed in the list.  We need to invalidate the list.
	OwnerWeak.Pin()->OnCategorySelectionChanged();
}


TSharedPtr<FPluginCategory> SPluginCategoryTree::GetSelectedCategory() const
{
	if( TreeView.IsValid() )
	{
		auto SelectedItems = TreeView->GetSelectedItems();
		if( SelectedItems.Num() > 0 )
		{
			const auto& SelectedCategoryItem = SelectedItems[ 0 ];
			return SelectedCategoryItem;
		}
	}

	return NULL;
}

void SPluginCategoryTree::SelectCategory( const TSharedPtr<FPluginCategory>& CategoryToSelect )
{
	if( ensure( TreeView.IsValid() ) )
	{
		TreeView->SetSelection( CategoryToSelect );
	}
}

bool SPluginCategoryTree::IsItemExpanded( const TSharedPtr<FPluginCategory> Item ) const
{
	return TreeView->IsItemExpanded( Item );
}

void SPluginCategoryTree::SetNeedsRefresh()
{
	RegisterActiveTimer (0.f, FWidgetActiveTimerDelegate::CreateSP (this, &SPluginCategoryTree::TriggerCategoriesRefresh));
}

EActiveTimerReturnType SPluginCategoryTree::TriggerCategoriesRefresh(double InCurrentTime, float InDeltaTime)
{
	RebuildAndFilterCategoryTree();
	return EActiveTimerReturnType::Stop;
}

#undef LOCTEXT_NAMESPACE
