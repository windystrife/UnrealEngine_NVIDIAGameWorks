// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Palette/SPaletteView.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"
#include "Editor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBorder.h"

#if WITH_EDITOR
	#include "EditorStyleSet.h"
#endif // WITH_EDITOR



#include "DragDrop/WidgetTemplateDragDropOp.h"

#include "Templates/WidgetTemplateClass.h"
#include "Templates/WidgetTemplateBlueprintClass.h"

#include "Developer/HotReload/Public/IHotReload.h"

#include "AssetRegistryModule.h"
#include "Widgets/Input/SSearchBox.h"

#include "Settings/ContentBrowserSettings.h"
#include "WidgetBlueprintEditorUtils.h"



#include "UMGEditorProjectSettings.h"

#define LOCTEXT_NAMESPACE "UMG"

class SPaletteViewItem : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SPaletteViewItem) {}

		/** The current text to highlight */
		SLATE_ATTRIBUTE(FText, HighlightText)

	SLATE_END_ARGS()

	/**
	* Constructs this widget
	*
	* @param InArgs    Declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, TSharedPtr<FWidgetTemplate> InTemplate)
	{
		Template = InTemplate;

		ChildSlot
		[
			SNew(SHorizontalBox)
			.Visibility(EVisibility::Visible)
			.ToolTip(Template->GetToolTip())

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.ColorAndOpacity(FLinearColor(1, 1, 1, 0.5))
				.Image(Template->GetIcon())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Template->Name)
				.HighlightText(InArgs._HighlightText)
			]
		];
	}

	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override
	{
		return Template->OnDoubleClicked();
	}

private:
	TSharedPtr<FWidgetTemplate> Template;
};

class FWidgetTemplateViewModel : public FWidgetViewModel
{
public:
	virtual ~FWidgetTemplateViewModel()
	{
	}

	virtual FText GetName() const override
	{
		return Template->Name;
	}

	virtual bool IsTemplate() const override
	{
		return true;
	}

	virtual FString GetFilterString() const override
	{
		return Template->Name.ToString();
	}

	virtual TSharedRef<ITableRow> BuildRow(const TSharedRef<STableViewBase>& OwnerTable) override
	{
		return SNew(STableRow< TSharedPtr<FWidgetViewModel> >, OwnerTable)
			.Padding(2.0f)
			.Style(FEditorStyle::Get(), "UMGEditor.PaletteItem")
			.OnDragDetected(this, &FWidgetTemplateViewModel::OnDraggingWidgetTemplateItem)
			[
				SNew(SPaletteViewItem, Template)
				.HighlightText(OwnerView, &SPaletteView::GetSearchText)
			];
	}

	FReply OnDraggingWidgetTemplateItem(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		return FReply::Handled().BeginDragDrop(FWidgetTemplateDragDropOp::New(Template));
	}

	SPaletteView* OwnerView;
	TSharedPtr<FWidgetTemplate> Template;
};

class FWidgetHeaderViewModel : public FWidgetViewModel
{
public:
	virtual ~FWidgetHeaderViewModel()
	{
	}

	virtual FText GetName() const override
	{
		return GroupName;
	}

	virtual bool IsTemplate() const override
	{
		return false;
	}

	virtual FString GetFilterString() const override
	{
		// Headers should never be included in filtering to avoid showing a header with all of
		// it's widgets filtered out, so return an empty filter string.
		return TEXT("");
	}
	
	virtual TSharedRef<ITableRow> BuildRow(const TSharedRef<STableViewBase>& OwnerTable) override
	{
		return SNew(STableRow< TSharedPtr<FWidgetViewModel> >, OwnerTable)
			.Style( FEditorStyle::Get(), "UMGEditor.PaletteHeader" )
			.Padding(2.0f)
			.ShowSelection(false)
			[
				SNew(STextBlock)
				.Text(GroupName)
				.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			];
	}

	virtual void GetChildren(TArray< TSharedPtr<FWidgetViewModel> >& OutChildren) override
	{
		for ( TSharedPtr<FWidgetViewModel>& Child : Children )
		{
			OutChildren.Add(Child);
		}
	}

	FText GroupName;
	TArray< TSharedPtr<FWidgetViewModel> > Children;
};

void SPaletteView::Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
{
	// Register for events that can trigger a palette rebuild
	GEditor->OnBlueprintReinstanced().AddRaw(this, &SPaletteView::OnBlueprintReinstanced);
	FEditorDelegates::OnAssetsDeleted.AddSP(this, &SPaletteView::HandleOnAssetsDeleted);
	IHotReloadModule::Get().OnHotReload().AddSP(this, &SPaletteView::HandleOnHotReload);
	
	// register for any objects replaced
	GEditor->OnObjectsReplaced().AddRaw(this, &SPaletteView::OnObjectsReplaced);

	BlueprintEditor = InBlueprintEditor;

	UBlueprint* BP = InBlueprintEditor->GetBlueprintObj();

	WidgetFilter = MakeShareable(new WidgetViewModelTextFilter(
		WidgetViewModelTextFilter::FItemToStringArray::CreateSP(this, &SPaletteView::TransformWidgetViewModelToString)));

	FilterHandler = MakeShareable(new PaletteFilterHandler());
	FilterHandler->SetFilter(WidgetFilter.Get());
	FilterHandler->SetRootItems(&WidgetViewModels, &TreeWidgetViewModels);
	FilterHandler->SetGetChildrenDelegate(PaletteFilterHandler::FOnGetChildren::CreateRaw(this, &SPaletteView::OnGetChildren));

	SAssignNew(WidgetTemplatesView, STreeView< TSharedPtr<FWidgetViewModel> >)
		.ItemHeight(1.0f)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SPaletteView::OnGenerateWidgetTemplateItem)
		.OnGetChildren(FilterHandler.ToSharedRef(), &PaletteFilterHandler::OnGetFilteredChildren)
		.OnSelectionChanged(this, &SPaletteView::WidgetPalette_OnSelectionChanged)
		.TreeItemsSource(&TreeWidgetViewModels);
		

	FilterHandler->SetTreeView(WidgetTemplatesView.Get());

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(4)
		.AutoHeight()
		[
			SAssignNew(SearchBoxPtr, SSearchBox)
			.HintText(LOCTEXT("SearchTemplates", "Search Palette"))
			.OnTextChanged(this, &SPaletteView::OnSearchChanged)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBorder, WidgetTemplatesView.ToSharedRef())
			[
				WidgetTemplatesView.ToSharedRef()
			]
		]
	];

	bRefreshRequested = true;

	BuildWidgetList();
	LoadItemExpansion();

	bRebuildRequested = false;
}

SPaletteView::~SPaletteView()
{
	// If the filter is enabled, disable it before saving the expanded items since
	// filtering expands all items by default.
	if (FilterHandler->GetIsEnabled())
	{
		FilterHandler->SetIsEnabled(false);
		FilterHandler->RefreshAndFilterTree();
	}

	GEditor->OnBlueprintReinstanced().RemoveAll(this);
	FEditorDelegates::OnAssetsDeleted.RemoveAll(this);
	IHotReloadModule::Get().OnHotReload().RemoveAll(this);
	GEditor->OnObjectsReplaced().RemoveAll( this );
	

	SaveItemExpansion();
}

void SPaletteView::OnSearchChanged(const FText& InFilterText)
{
	bRefreshRequested = true;
	FilterHandler->SetIsEnabled(!InFilterText.IsEmpty());
	WidgetFilter->SetRawFilterText(InFilterText);
	SearchBoxPtr->SetError(WidgetFilter->GetFilterErrorText());
	SearchText = InFilterText;
}

FText SPaletteView::GetSearchText() const
{
	return SearchText;
}

void SPaletteView::WidgetPalette_OnSelectionChanged(TSharedPtr<FWidgetViewModel> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (!SelectedItem.IsValid()) 
	{
		return;
	}

	// Reset the selected
	BlueprintEditor.Pin()->SetSelectedTemplate(nullptr);
	BlueprintEditor.Pin()->SetSelectedUserWidget(FAssetData());

	// If it's not a template, return
	if (!SelectedItem->IsTemplate())
	{
		return;
	}

	TSharedPtr<FWidgetTemplateViewModel> SelectedTemplate = StaticCastSharedPtr<FWidgetTemplateViewModel>(SelectedItem);
	if (SelectedTemplate.IsValid())
	{
		TSharedPtr<FWidgetTemplateClass> TemplateClass = StaticCastSharedPtr<FWidgetTemplateClass>(SelectedTemplate->Template);
		if (TemplateClass.IsValid())
		{
			if (TemplateClass->GetWidgetClass().IsValid())
			{
				BlueprintEditor.Pin()->SetSelectedTemplate(TemplateClass->GetWidgetClass());
			}
			else
			{
				TSharedPtr<FWidgetTemplateBlueprintClass> UserCreatedTemplate = StaticCastSharedPtr<FWidgetTemplateBlueprintClass>(TemplateClass);
				if (UserCreatedTemplate.IsValid())
				{
					// Then pass in the asset data of selected widget
					FAssetData UserCreatedWidget = UserCreatedTemplate->GetWidgetAssetData();
					BlueprintEditor.Pin()->SetSelectedUserWidget(UserCreatedWidget);
				}
			}
		}
	}
}

TSharedPtr<FWidgetTemplate> SPaletteView::GetSelectedTemplateWidget() const
{
	TArray<TSharedPtr<FWidgetViewModel>> SelectedTemplates = WidgetTemplatesView.Get()->GetSelectedItems();
	if (SelectedTemplates.Num() == 1)
	{
		TSharedPtr<FWidgetTemplateViewModel> TemplateViewModel = StaticCastSharedPtr<FWidgetTemplateViewModel>(SelectedTemplates[0]);
		if (TemplateViewModel.IsValid())
		{
			return TemplateViewModel->Template;
		}
	}
	return nullptr;
}

void SPaletteView::LoadItemExpansion()
{
	// Restore the expansion state of the widget groups.
	for ( TSharedPtr<FWidgetViewModel>& ViewModel : WidgetViewModels )
	{
		bool IsExpanded;
		if ( GConfig->GetBool(TEXT("WidgetTemplatesExpanded"), *ViewModel->GetName().ToString(), IsExpanded, GEditorPerProjectIni) && IsExpanded )
		{
			WidgetTemplatesView->SetItemExpansion(ViewModel, true);
		}
	}
}

void SPaletteView::SaveItemExpansion()
{
	// Restore the expansion state of the widget groups.
	for ( TSharedPtr<FWidgetViewModel>& ViewModel : WidgetViewModels )
	{
		const bool IsExpanded = WidgetTemplatesView->IsItemExpanded(ViewModel);
		GConfig->SetBool(TEXT("WidgetTemplatesExpanded"), *ViewModel->GetName().ToString(), IsExpanded, GEditorPerProjectIni);
	}
}

UWidgetBlueprint* SPaletteView::GetBlueprint() const
{
	if ( BlueprintEditor.IsValid() )
	{
		UBlueprint* BP = BlueprintEditor.Pin()->GetBlueprintObj();
		return Cast<UWidgetBlueprint>(BP);
	}

	return NULL;
}

void SPaletteView::BuildWidgetList()
{
	// Clear the current list of view models and categories
	WidgetViewModels.Reset();
	WidgetTemplateCategories.Reset();

	// Generate a list of templates
	BuildClassWidgetList();
	BuildSpecialWidgetList();

	// For each entry in the category create a view model for the widget template
	for ( auto& Entry : WidgetTemplateCategories )
	{
		TSharedPtr<FWidgetHeaderViewModel> Header = MakeShareable(new FWidgetHeaderViewModel());
		Header->GroupName = FText::FromString(Entry.Key);

		for ( auto& Template : Entry.Value )
		{
			TSharedPtr<FWidgetTemplateViewModel> TemplateViewModel = MakeShareable(new FWidgetTemplateViewModel());
			TemplateViewModel->Template = Template;
			TemplateViewModel->OwnerView = this;
			Header->Children.Add(TemplateViewModel);
		}

		Header->Children.Sort([] (TSharedPtr<FWidgetViewModel> L, TSharedPtr<FWidgetViewModel> R) { return R->GetName().CompareTo(L->GetName()) > 0; });

		WidgetViewModels.Add(Header);
	}

	// Sort the view models by name
	WidgetViewModels.Sort([] (TSharedPtr<FWidgetViewModel> L, TSharedPtr<FWidgetViewModel> R) { return R->GetName().CompareTo(L->GetName()) > 0; });
}

void SPaletteView::BuildClassWidgetList()
{
	static const FName DevelopmentStatusKey(TEXT("DevelopmentStatus"));

	TMap<FName, TSubclassOf<UUserWidget>> LoadedWidgetBlueprintClassesByName;

	auto ActiveWidgetBlueprintClass = GetBlueprint()->GeneratedClass;
	FName ActiveWidgetBlueprintClassName = ActiveWidgetBlueprintClass->GetFName();

	TArray<FSoftClassPath> WidgetClassesToHide = GetDefault<UUMGEditorProjectSettings>()->WidgetClassesToHide;

	// Locate all UWidget classes from code and loaded widget BPs
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* WidgetClass = *ClassIt;

		if (!FWidgetBlueprintEditorUtils::IsUsableWidgetClass(WidgetClass))
		{
			continue;
		}

		// Initialize AssetData for checking PackagePath
		FAssetData WidgetAssetData = FAssetData(WidgetClass);

		// Excludes engine content if user sets it to false
		if (!GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder() || !GetDefault<UUMGEditorProjectSettings>()->bShowWidgetsFromEngineContent)
		{
			if (WidgetAssetData.PackagePath.ToString().Find(TEXT("/Engine")) == 0)
			{
				continue;
			}
		}

		// Excludes developer content if user sets it to false
		if (!GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder() || !GetDefault<UUMGEditorProjectSettings>()->bShowWidgetsFromDeveloperContent)
		{
			if (WidgetAssetData.PackagePath.ToString().Find(TEXT("/Game/Developers")) == 0)
			{
				continue;
			}
		}

		// Excludes this widget if it is on the hide list
		bool bIsOnList = false;
		for (FSoftClassPath Widget : WidgetClassesToHide)
		{
			if (WidgetAssetData.ObjectPath.ToString().Find(Widget.ToString()) == 0)
			{
				bIsOnList = true;
				break;
			}
		}
		if (bIsOnList)
		{
			continue;
		}

		const bool bIsSameClass = WidgetClass->GetFName() == ActiveWidgetBlueprintClassName;

		// Check that the asset that generated this class is valid (necessary b/c of a larger issue wherein force delete does not wipe the generated class object)
		if ( bIsSameClass )
		{
			continue;
		}

		if (WidgetClass->IsChildOf(UUserWidget::StaticClass()))
		{
			if ( WidgetClass->ClassGeneratedBy )
			{
				// Track the widget blueprint classes that are already loaded
				LoadedWidgetBlueprintClassesByName.Add(WidgetClass->ClassGeneratedBy->GetFName()) = WidgetClass;
			}
		}
		else
		{
			TSharedPtr<FWidgetTemplateClass> Template = MakeShareable(new FWidgetTemplateClass(WidgetClass));

			AddWidgetTemplate(Template);
		}

		//TODO UMG does not prevent deep nested circular references
	}

	// Locate all widget BP assets (include unloaded)
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AllWidgetBPsAssetData;
	AssetRegistryModule.Get().GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetFName(), AllWidgetBPsAssetData, true);

	FName ActiveWidgetBlueprintName = ActiveWidgetBlueprintClass->ClassGeneratedBy->GetFName();
	for (FAssetData& WidgetBPAssetData : AllWidgetBPsAssetData)
	{
		// Excludes the blueprint you're currently in
		if (WidgetBPAssetData.AssetName == ActiveWidgetBlueprintName)
		{
			continue;
		}

		// Excludes engine content if user sets it to false
		if (!GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder() || !GetDefault<UUMGEditorProjectSettings>()->bShowWidgetsFromEngineContent)
		{
			if (WidgetBPAssetData.PackagePath.ToString().Find(TEXT("/Engine")) == 0)
			{
				continue;
			}
		}

		// Excludes developer content if user sets it to false
		if (!GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder() || !GetDefault<UUMGEditorProjectSettings>()->bShowWidgetsFromDeveloperContent)
		{
			if (WidgetBPAssetData.PackagePath.ToString().Find(TEXT("/Game/Developers")) == 0)
			{
				continue;
			}
		}

		// Excludes this widget if it is on the hide list
		bool bIsOnList = false;
		for (FSoftClassPath Widget : WidgetClassesToHide)
		{
			if (Widget.ToString().Find(WidgetBPAssetData.ObjectPath.ToString()) == 0)
			{
				bIsOnList = true;
				break;
			}
		}
		if (bIsOnList)
		{
			continue;
		}

		// If the blueprint generated class was found earlier, pass it to the template
		TSubclassOf<UUserWidget> WidgetBPClass = nullptr;
		auto LoadedWidgetBPClass = LoadedWidgetBlueprintClassesByName.Find(WidgetBPAssetData.AssetName);
		if (LoadedWidgetBPClass)
		{
			WidgetBPClass = *LoadedWidgetBPClass;
		}

		auto Template = MakeShareable(new FWidgetTemplateBlueprintClass(WidgetBPAssetData, WidgetBPClass));

		AddWidgetTemplate(Template);
	}
}

void SPaletteView::BuildSpecialWidgetList()
{
	//AddWidgetTemplate(MakeShareable(new FWidgetTemplateButton()));
	//AddWidgetTemplate(MakeShareable(new FWidgetTemplateCheckBox()));

	//TODO UMG Make this pluggable.
}

void SPaletteView::AddWidgetTemplate(TSharedPtr<FWidgetTemplate> Template)
{
	FString Category = Template->GetCategory().ToString();

	// Hide user specific categories
	TArray<FString> CategoriesToHide = GetDefault<UUMGEditorProjectSettings>()->CategoriesToHide;
	for (FString CategoryName : CategoriesToHide)
	{
		if (Category == CategoryName)
		{
			return;
		}
	}
	WidgetTemplateArray& Group = WidgetTemplateCategories.FindOrAdd(Category);
	Group.Add(Template);
}

void SPaletteView::OnGetChildren(TSharedPtr<FWidgetViewModel> Item, TArray< TSharedPtr<FWidgetViewModel> >& Children)
{
	return Item->GetChildren(Children);
}

TSharedRef<ITableRow> SPaletteView::OnGenerateWidgetTemplateItem(TSharedPtr<FWidgetViewModel> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return Item->BuildRow(OwnerTable);
}

void SPaletteView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if ( bRebuildRequested )
	{
		bRebuildRequested = false;

		// Save the old expanded items temporarily
		TSet<TSharedPtr<FWidgetViewModel>> ExpandedItems;
		WidgetTemplatesView->GetExpandedItems(ExpandedItems);

		BuildWidgetList();

		// Restore the expansion state
		for ( TSharedPtr<FWidgetViewModel>& ExpandedItem : ExpandedItems )
		{
			for ( TSharedPtr<FWidgetViewModel>& ViewModel : WidgetViewModels )
			{
				if ( ViewModel->GetName().EqualTo(ExpandedItem->GetName()) )
				{
					WidgetTemplatesView->SetItemExpansion(ViewModel, true);
				}
			}
		}
	}

	if (bRefreshRequested)
	{
		bRefreshRequested = false;
		FilterHandler->RefreshAndFilterTree();
	}
}

void SPaletteView::TransformWidgetViewModelToString(TSharedPtr<FWidgetViewModel> WidgetViewModel, OUT TArray< FString >& Array)
{
	Array.Add(WidgetViewModel->GetFilterString());
}

void SPaletteView::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	//bRefreshRequested = true;
	//bRebuildRequested = true;
}

void SPaletteView::OnBlueprintReinstanced()
{
	bRebuildRequested = true;
	bRefreshRequested = true;
}

void SPaletteView::HandleOnHotReload(bool bWasTriggeredAutomatically)
{
	bRebuildRequested = true;
	bRefreshRequested = true;
}

void SPaletteView::HandleOnAssetsDeleted(const TArray<UClass*>& DeletedAssetClasses)
{
	for (auto DeletedAssetClass : DeletedAssetClasses)
	{
		if (DeletedAssetClass->IsChildOf(UWidgetBlueprint::StaticClass()))
		{
			bRebuildRequested = true;
			bRefreshRequested = true;
		}
	}
	
}

#undef LOCTEXT_NAMESPACE
