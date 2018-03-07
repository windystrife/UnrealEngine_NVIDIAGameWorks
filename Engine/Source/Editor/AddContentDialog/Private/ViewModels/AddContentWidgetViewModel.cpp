// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/AddContentWidgetViewModel.h"
#include "Modules/ModuleManager.h"
#include "IAddContentDialogModule.h"
#include "IContentSource.h"
#include "ContentSourceProviderManager.h"
#include "ViewModels/ContentSourceViewModel.h"


FAddContentWidgetViewModel::FAddContentWidgetViewModel()
{
}

TSharedRef<FAddContentWidgetViewModel> FAddContentWidgetViewModel::CreateShared()
{
	TSharedPtr<FAddContentWidgetViewModel> Shared = MakeShareable(new FAddContentWidgetViewModel());
	Shared->Initialize();
	return Shared.ToSharedRef();
}

void FAddContentWidgetViewModel::Initialize()
{
	IAddContentDialogModule& AddContentDialogModule = FModuleManager::LoadModuleChecked<IAddContentDialogModule>("AddContentDialog");

	for (auto ContentSourceProvider : *AddContentDialogModule.GetContentSourceProviderManager()->GetContentSourceProviders())
	{
		ContentSourceProviders.Add(ContentSourceProvider);
		ContentSourceProvider->SetContentSourcesChanged(FOnContentSourcesChanged::CreateSP(this, &FAddContentWidgetViewModel::ContentSourcesChanged));
	}

	ContentSourceFilter = TSharedPtr<ContentSourceTextFilter>(new ContentSourceTextFilter(
		ContentSourceTextFilter::FItemToStringArray::CreateSP(this, &FAddContentWidgetViewModel::TransformContentSourceToStrings)));

	BuildContentSourceViewModels();
}

const TArray<FCategoryViewModel>* FAddContentWidgetViewModel::GetCategories()
{
	return &Categories;
}

void FAddContentWidgetViewModel::SetOnCategoriesChanged(FOnCategoriesChanged OnCategoriesChangedIn)
{
	OnCategoriesChanged = OnCategoriesChangedIn;
}

FCategoryViewModel FAddContentWidgetViewModel::GetSelectedCategory()
{
	return SelectedCategory;
}

void FAddContentWidgetViewModel::SetSelectedCategory(FCategoryViewModel SelectedCategoryIn)
{
	SelectedCategory = SelectedCategoryIn;
	UpdateFilteredContentSourcesAndSelection(true);
	OnSelectedContentSourceChanged.ExecuteIfBound();
}

void FAddContentWidgetViewModel::SetSearchText(FText SearchTextIn)
{
	SearchText = SearchTextIn;
	ContentSourceFilter->SetRawFilterText(SearchTextIn);
	UpdateFilteredContentSourcesAndSelection(true);
}

FText FAddContentWidgetViewModel::GetSearchErrorText() const
{
	return ContentSourceFilter->GetFilterErrorText();
}

const TArray<TSharedPtr<FContentSourceViewModel>>* FAddContentWidgetViewModel::GetContentSources()
{
	return &FilteredContentSourceViewModels;
}

void FAddContentWidgetViewModel::SetOnContentSourcesChanged(FOnContentSourcesChanged OnContentSourcesChangedIn)
{
	OnContentSourcesChanged = OnContentSourcesChangedIn;
}

TSharedPtr<FContentSourceViewModel> FAddContentWidgetViewModel::GetSelectedContentSource()
{
	TSharedPtr<FContentSourceViewModel>* SelectedContentSource = CategoryToSelectedContentSourceMap.Find(SelectedCategory);
	if (SelectedContentSource != nullptr)
	{
		return *SelectedContentSource;
	}
	return TSharedPtr<FContentSourceViewModel>();
}

void FAddContentWidgetViewModel::SetSelectedContentSource(TSharedPtr<FContentSourceViewModel> SelectedContentSourceIn)
{
	// Ignore selecting the currently selected item.
	TSharedPtr<FContentSourceViewModel> SelectedContentSource = GetSelectedContentSource();
	if (SelectedContentSource != SelectedContentSourceIn)
	{
		CategoryToSelectedContentSourceMap.Add(SelectedCategory) = SelectedContentSourceIn;
		OnSelectedContentSourceChanged.ExecuteIfBound();
	}
}

void FAddContentWidgetViewModel::SetOnSelectedContentSourceChanged(FOnSelectedContentSourceChanged OnSelectedContentSourceChangedIn)
{
	OnSelectedContentSourceChanged = OnSelectedContentSourceChangedIn;
}

void FAddContentWidgetViewModel::BuildContentSourceViewModels()
{
	Categories.Empty();
	ContentSourceViewModels.Empty();
	FilteredContentSourceViewModels.Empty();
	CategoryToSelectedContentSourceMap.Empty();
	
	// List of categorys we dont want to see
	TArray<EContentSourceCategory> FilteredCategories;
	FilteredCategories.Add(EContentSourceCategory::SharedPack);
	FilteredCategories.Add(EContentSourceCategory::Unknown);

	for (auto ContentSourceProvider : ContentSourceProviders)
	{
		for (auto ContentSource : ContentSourceProvider->GetContentSources())
		{
			// Check if we want to see this category
			if( FilteredCategories.Contains( ContentSource->GetCategory()) == false )
			{
				TSharedPtr<FContentSourceViewModel> ContentSourceViewModel = MakeShareable(new FContentSourceViewModel(ContentSource));
				if (Categories.Contains(ContentSourceViewModel->GetCategory()) == false)
				{
					Categories.Add(ContentSourceViewModel->GetCategory());
				}
				ContentSourceViewModels.Add(ContentSourceViewModel);
			}
		}
	}

	if (Categories.Num() > 0)
	{
		Categories.Sort();
		// Update the current selection for all categories.  Do this in reverse order so that the first category
		// remains selected when finished.
		for (int i = Categories.Num() - 1; i >= 0; i--)
		{
			SelectedCategory = Categories[i];
			UpdateFilteredContentSourcesAndSelection(false);
		}
	}
	OnCategoriesChanged.ExecuteIfBound();
}

void FAddContentWidgetViewModel::UpdateFilteredContentSourcesAndSelection(bool bAllowEmptySelection)
{
	FilteredContentSourceViewModels.Empty();
	for (auto ContentSource : ContentSourceViewModels)
	{
		if ((ContentSource->GetCategory() == SelectedCategory) && (ContentSourceFilter.IsValid() == false || ContentSourceFilter->PassesFilter(ContentSource)))
		{
			FilteredContentSourceViewModels.Add(ContentSource);
		}
	}
	OnContentSourcesChanged.ExecuteIfBound();

	if (FilteredContentSourceViewModels.Contains(GetSelectedContentSource()) == false)
	{
		TSharedPtr<FContentSourceViewModel> NewSelectedContentSource;
		if (bAllowEmptySelection == false && FilteredContentSourceViewModels.Num() > 0)
		{
			NewSelectedContentSource =  FilteredContentSourceViewModels[0];
		}
		SetSelectedContentSource(NewSelectedContentSource);
	}
}

void FAddContentWidgetViewModel::TransformContentSourceToStrings(TSharedPtr<FContentSourceViewModel> Item, OUT TArray<FString>& Array)
{
	Array.Add(Item->GetName().ToString());
}

void FAddContentWidgetViewModel::ContentSourcesChanged()
{
	BuildContentSourceViewModels();
}
