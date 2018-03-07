// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "WidgetBlueprintEditor.h"
#include "Misc/TextFilter.h"
#include "Widgets/Views/STreeView.h"
#include "TreeFilterHandler.h"

class FWidgetTemplate;
class UWidgetBlueprint;

/** View model for the items in the widget template list */
class FWidgetViewModel : public TSharedFromThis<FWidgetViewModel>
{
public:
	virtual ~FWidgetViewModel() { }

	virtual FText GetName() const = 0;

	virtual bool IsTemplate() const = 0;

	/** Get the string which should be used for filtering the item. */
	virtual FString GetFilterString() const = 0;

	virtual TSharedRef<ITableRow> BuildRow(const TSharedRef<STableViewBase>& OwnerTable) = 0;

	virtual void GetChildren(TArray< TSharedPtr<FWidgetViewModel> >& OutChildren)
	{
	}
};

/**  */
class SPaletteView : public SCompoundWidget
{
public:
	typedef TTextFilter<TSharedPtr<FWidgetViewModel>> WidgetViewModelTextFilter;

public:
	SLATE_BEGIN_ARGS( SPaletteView ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor);
	virtual ~SPaletteView();
	
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	/** Gets the text currently displayed in the search box. */
	FText GetSearchText() const;

	/** On Selected template widget changed in palette view */
	void WidgetPalette_OnSelectionChanged(TSharedPtr<FWidgetViewModel> SelectedItem, ESelectInfo::Type SelectInfo);

	/** Gets the selected template widget in palette */
	TSharedPtr<FWidgetTemplate> GetSelectedTemplateWidget() const;

private:
	UWidgetBlueprint* GetBlueprint() const;

	void BuildWidgetList();
	void BuildClassWidgetList();
	void BuildSpecialWidgetList();

	void OnGetChildren(TSharedPtr<FWidgetViewModel> Item, TArray< TSharedPtr<FWidgetViewModel> >& Children);
	TSharedRef<ITableRow> OnGenerateWidgetTemplateItem(TSharedPtr<FWidgetViewModel> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Called when the filter text is changed. */
	void OnSearchChanged(const FText& InFilterText);

private:
	void LoadItemExpansion();
	void SaveItemExpansion();

	/** Called when a Blueprint is recompiled and live objects are swapped out for replacements */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

	void AddWidgetTemplate(TSharedPtr<FWidgetTemplate> Template);

	/** Transforms the widget view model into a searchable string. */
	void TransformWidgetViewModelToString(TSharedPtr<FWidgetViewModel> WidgetViewModel, OUT TArray< FString >& Array);

	/** Requests a rebuild of the widget list if a widget blueprint was compiled */
	void OnBlueprintReinstanced();

	/** Requests a rebuild of the widget list */
	void HandleOnHotReload(bool bWasTriggeredAutomatically);

	/** Requests a rebuild of the widget list if a widget blueprint was deleted */
	void HandleOnAssetsDeleted(const TArray<UClass*>& DeletedAssetClasses);

	TWeakPtr<class FWidgetBlueprintEditor> BlueprintEditor;

	/** Handles filtering the palette based on an IFilter. */
	typedef TreeFilterHandler<TSharedPtr<FWidgetViewModel>> PaletteFilterHandler;
	TSharedPtr<PaletteFilterHandler> FilterHandler;

	typedef TArray< TSharedPtr<FWidgetTemplate> > WidgetTemplateArray;
	TMap< FString, WidgetTemplateArray > WidgetTemplateCategories;

	typedef TArray< TSharedPtr<FWidgetViewModel> > ViewModelsArray;
	
	/** The source root view models for the tree. */
	ViewModelsArray WidgetViewModels;

	/** The root view models which are actually displayed by the TreeView which will be managed by the TreeFilterHandler. */
	ViewModelsArray TreeWidgetViewModels;

	TSharedPtr< STreeView< TSharedPtr<FWidgetViewModel> > > WidgetTemplatesView;

	/** The search box used to update the filter text */
	TSharedPtr<class SSearchBox> SearchBoxPtr;

	/** The filter instance which is used by the TreeFilterHandler to filter the TreeView. */
	TSharedPtr<WidgetViewModelTextFilter> WidgetFilter;

	bool bRefreshRequested;
	FText SearchText;

	/** Controls rebuilding the list of spawnable widgets */
	bool bRebuildRequested;
};
