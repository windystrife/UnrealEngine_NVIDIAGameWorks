// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SDetailsViewBase.h"
#include "GameFramework/Actor.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "ObjectPropertyNode.h"
#include "Modules/ModuleManager.h"
#include "Settings/EditorExperimentalSettings.h"
#include "IDetailCustomization.h"
#include "SDetailsView.h"
#include "DetailLayoutBuilderImpl.h"
#include "DetailCategoryBuilderImpl.h"
#include "CategoryPropertyNode.h"
#include "ScopedTransaction.h"
#include "SDetailNameArea.h"
#include "UserInterface/PropertyEditor/SPropertyEditorEditInline.h"
#include "ObjectEditorUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Colors/SColorPicker.h"
#include "DetailPropertyRow.h"
#include "Widgets/Input/SSearchBox.h"
#include "EditorStyleSettings.h"
#include "DetailLayoutHelpers.h"

SDetailsViewBase::~SDetailsViewBase()
{
	if (ThumbnailPool.IsValid())
	{
		ThumbnailPool->ReleaseResources();
	}
}


void SDetailsViewBase::OnGetChildrenForDetailTree(TSharedRef<FDetailTreeNode> InTreeNode, TArray< TSharedRef<FDetailTreeNode> >& OutChildren)
{
	InTreeNode->GetChildren(OutChildren);
}

TSharedRef<ITableRow> SDetailsViewBase::OnGenerateRowForDetailTree(TSharedRef<FDetailTreeNode> InTreeNode, const TSharedRef<STableViewBase>& OwnerTable)
{
	return InTreeNode->GenerateWidgetForTableView(OwnerTable, ColumnSizeData, DetailsViewArgs.bAllowFavoriteSystem);
}

void SDetailsViewBase::SetRootExpansionStates(const bool bExpand, const bool bRecurse)
{
	if(ContainsMultipleTopLevelObjects())
	{
		FDetailNodeList Children;
		for(auto Iter = RootTreeNodes.CreateIterator(); Iter; ++Iter)
		{
			Children.Reset();
			(*Iter)->GetChildren(Children);

			for(TSharedRef<class FDetailTreeNode>& Child : Children)
			{
				SetNodeExpansionState(Child, bExpand, bRecurse);
			}
		}
	}
	else
	{
		for(auto Iter = RootTreeNodes.CreateIterator(); Iter; ++Iter)
		{
			SetNodeExpansionState(*Iter, bExpand, bRecurse);
		}
	}
}

void SDetailsViewBase::SetNodeExpansionState(TSharedRef<FDetailTreeNode> InTreeNode, bool bIsItemExpanded, bool bRecursive)
{
	TArray< TSharedRef<FDetailTreeNode> > Children;
	InTreeNode->GetChildren(Children);

	if (Children.Num())
	{
		RequestItemExpanded(InTreeNode, bIsItemExpanded);
		const bool bShouldSaveState = true;
		InTreeNode->OnItemExpansionChanged(bIsItemExpanded, bShouldSaveState);

		if (bRecursive)
		{
			for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
			{
				TSharedRef<FDetailTreeNode> Child = Children[ChildIndex];

				SetNodeExpansionState(Child, bIsItemExpanded, bRecursive);
			}
		}
	}
}

void SDetailsViewBase::SetNodeExpansionStateRecursive(TSharedRef<FDetailTreeNode> InTreeNode, bool bIsItemExpanded)
{
	SetNodeExpansionState(InTreeNode, bIsItemExpanded, true);
}

void SDetailsViewBase::OnItemExpansionChanged(TSharedRef<FDetailTreeNode> InTreeNode, bool bIsItemExpanded)
{
	SetNodeExpansionState(InTreeNode, bIsItemExpanded, false);
}

FReply SDetailsViewBase::OnLockButtonClicked()
{
	bIsLocked = !bIsLocked;
	return FReply::Handled();
}

void SDetailsViewBase::HideFilterArea(bool bHide)
{
	DetailsViewArgs.bAllowSearch = !bHide;
}

static void GetPropertiesInOrderDisplayedRecursive(const TArray< TSharedRef<FDetailTreeNode> >& TreeNodes, TArray< FPropertyPath > &OutLeaves)
{
	for (auto& TreeNode : TreeNodes)
	{
		if (TreeNode->IsLeaf())
		{
			FPropertyPath Path = TreeNode->GetPropertyPath();
			// Some leaf nodes are not associated with properties, specifically the collision presets.
			// @todo doc: investigate what we can do about this, result is that for these fields
			// we can't highlight hte property in the diff tool.
			if( Path.GetNumProperties() != 0 )
			{
				OutLeaves.Push(Path);
			}
		}
		else
		{
			TArray< TSharedRef<FDetailTreeNode> > Children;
			TreeNode->GetChildren(Children);
			GetPropertiesInOrderDisplayedRecursive(Children, OutLeaves);
		}
	}
}
TArray< FPropertyPath > SDetailsViewBase::GetPropertiesInOrderDisplayed() const
{
	TArray< FPropertyPath > Ret;
	GetPropertiesInOrderDisplayedRecursive(RootTreeNodes, Ret);
	return Ret;
}

// @return populates OutNodes with the leaf node corresponding to property as the first entry in the list (e.g. [leaf, parent, grandparent]):
static void FindTreeNodeFromPropertyRecursive( const TArray< TSharedRef<FDetailTreeNode> >& Nodes, const FPropertyPath& Property, TArray< TSharedPtr< FDetailTreeNode > >& OutNodes )
{
	for (auto& TreeNode : Nodes)
	{
		if (TreeNode->IsLeaf())
		{
			FPropertyPath tmp = TreeNode->GetPropertyPath();
			if( Property == tmp )
			{
				OutNodes.Push(TreeNode);
				return;
			}
		}

		// Need to check children even if we're a leaf, because all DetailItemNodes are leaves, even if they may have sub-children
		TArray< TSharedRef<FDetailTreeNode> > Children;
		TreeNode->GetChildren(Children);
		FindTreeNodeFromPropertyRecursive(Children, Property, OutNodes);
		if (OutNodes.Num() > 0)
		{
			OutNodes.Push(TreeNode);
			return;
		}
	}
}

void SDetailsViewBase::HighlightProperty(const FPropertyPath& Property)
{
	auto PrevHighlightedNodePtr = CurrentlyHighlightedNode.Pin();
	if (PrevHighlightedNodePtr.IsValid())
	{
		PrevHighlightedNodePtr->SetIsHighlighted(false);
	}

	TSharedPtr< FDetailTreeNode > FinalNodePtr = nullptr;
	TArray< TSharedPtr< FDetailTreeNode > > TreeNodeChain;
	FindTreeNodeFromPropertyRecursive(RootTreeNodes, Property, TreeNodeChain);
	if (TreeNodeChain.Num() > 0)
	{
		FinalNodePtr = TreeNodeChain[0];
		check(FinalNodePtr.IsValid());
		FinalNodePtr->SetIsHighlighted(true);

		for (int ParentIndex = 1; ParentIndex < TreeNodeChain.Num(); ++ParentIndex)
		{
			TSharedPtr< FDetailTreeNode > CurrentParent = TreeNodeChain[ParentIndex];
			check(CurrentParent.IsValid());
			DetailTree->SetItemExpansion(CurrentParent.ToSharedRef(), true);
		}

		DetailTree->RequestScrollIntoView(FinalNodePtr.ToSharedRef());
	}
	CurrentlyHighlightedNode = FinalNodePtr;
}

void SDetailsViewBase::ShowAllAdvancedProperties()
{
	CurrentFilter.bShowAllAdvanced = true;
}

void SDetailsViewBase::SetOnDisplayedPropertiesChanged(FOnDisplayedPropertiesChanged InOnDisplayedPropertiesChangedDelegate)
{
	OnDisplayedPropertiesChangedDelegate = InOnDisplayedPropertiesChangedDelegate;
}

void SDetailsViewBase::RerunCurrentFilter()
{
	UpdateFilteredDetails();
}

EVisibility SDetailsViewBase::GetTreeVisibility() const
{
	for(const FDetailLayoutData& Data : DetailLayouts)
	{
		if(Data.DetailLayout.IsValid() && Data.DetailLayout->HasDetails())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

/** Returns the image used for the icon on the filter button */
const FSlateBrush* SDetailsViewBase::OnGetFilterButtonImageResource() const
{
	if (bHasActiveFilter)
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.FilterCancel"));
	}
	else
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.FilterSearch"));
	}
}

void SDetailsViewBase::EnqueueDeferredAction(FSimpleDelegate& DeferredAction)
{
	DeferredActions.Add(DeferredAction);
}

/**
* Creates the color picker window for this property view.
*
* @param Node				The slate property node to edit.
* @param bUseAlpha			Whether or not alpha is supported
*/
void SDetailsViewBase::CreateColorPickerWindow(const TSharedRef< FPropertyEditor >& PropertyEditor, bool bUseAlpha)
{
	const TSharedRef< FPropertyNode > PinnedColorPropertyNode = PropertyEditor->GetPropertyNode();
	ColorPropertyNode = PinnedColorPropertyNode;

	UProperty* Property = PinnedColorPropertyNode->GetProperty();
	check(Property);

	FReadAddressList ReadAddresses;
	PinnedColorPropertyNode->GetReadAddress(false, ReadAddresses, false);

	TArray<FLinearColor*> LinearColor;
	TArray<FColor*> DWORDColor;
	for (int32 ColorIndex = 0; ColorIndex < ReadAddresses.Num(); ++ColorIndex)
	{
		const uint8* Addr = ReadAddresses.GetAddress(ColorIndex);
		if (Addr)
		{
			if (Cast<UStructProperty>(Property)->Struct->GetFName() == NAME_Color)
			{
				DWORDColor.Add((FColor*)Addr);
			}
			else
			{
				check(Cast<UStructProperty>(Property)->Struct->GetFName() == NAME_LinearColor);
				LinearColor.Add((FLinearColor*)Addr);
			}
		}
	}

	bHasOpenColorPicker = true;

	FColorPickerArgs PickerArgs;
	PickerArgs.ParentWidget = AsShared();
	PickerArgs.bUseAlpha = bUseAlpha;
	PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
	PickerArgs.ColorArray = &DWORDColor;
	PickerArgs.LinearColorArray = &LinearColor;
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SDetailsViewBase::SetColorPropertyFromColorPicker);
	PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP(this, &SDetailsViewBase::OnColorPickerWindowClosed);

	OpenColorPicker(PickerArgs);
}

void SDetailsViewBase::SetColorPropertyFromColorPicker(FLinearColor NewColor)
{
	const TSharedPtr< FPropertyNode > PinnedColorPropertyNode = ColorPropertyNode.Pin();
	if (ensure(PinnedColorPropertyNode.IsValid()))
	{
		UProperty* Property = PinnedColorPropertyNode->GetProperty();
		check(Property);

		FObjectPropertyNode* ObjectNode = PinnedColorPropertyNode->FindObjectItemParent();

		if (ObjectNode && ObjectNode->GetNumObjects())
		{
			FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "SetColorProperty", "Set Color Property"));

			PinnedColorPropertyNode->NotifyPreChange(Property, GetNotifyHook());

			FPropertyChangedEvent ChangeEvent(Property, EPropertyChangeType::ValueSet);
			PinnedColorPropertyNode->NotifyPostChange(ChangeEvent, GetNotifyHook());
		}
	}
}


void SDetailsViewBase::UpdatePropertyMaps()
{
	RootTreeNodes.Empty();

	for(FDetailLayoutData& LayoutData : DetailLayouts)
	{
		// Check uniqueness.  It is critical that detail layouts can be destroyed
		// We need to be able to create a new detail layout and properly clean up the old one in the process
		check(!LayoutData.DetailLayout.IsValid() || LayoutData.DetailLayout.IsUnique());

		// All the current customization instances need to be deleted when it is safe
		CustomizationClassInstancesPendingDelete.Append(LayoutData.CustomizationClassInstances);
	}

	FRootPropertyNodeList& RootPropertyNodes = GetRootNodes();
	
	DetailLayouts.Empty(RootPropertyNodes.Num());

	// There should be one detail layout for each root node
	DetailLayouts.AddDefaulted(RootPropertyNodes.Num());

	for(int32 RootNodeIndex = 0; RootNodeIndex < RootPropertyNodes.Num(); ++RootNodeIndex)
	{
		FDetailLayoutData& LayoutData = DetailLayouts[RootNodeIndex];
		UpdateSinglePropertyMap(RootPropertyNodes[RootNodeIndex], LayoutData);
	}
}

void SDetailsViewBase::UpdateSinglePropertyMap(TSharedPtr<FComplexPropertyNode> InRootPropertyNode, FDetailLayoutData& LayoutData)
{
	// Reset everything
	LayoutData.ClassToPropertyMap.Empty();

	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayout = MakeShareable(new FDetailLayoutBuilderImpl(InRootPropertyNode, LayoutData.ClassToPropertyMap, PropertyUtilities.ToSharedRef(), SharedThis(this)));
	LayoutData.DetailLayout = DetailLayout;

	TSharedPtr<FComplexPropertyNode> RootPropertyNode = InRootPropertyNode;
	check(RootPropertyNode.IsValid());

	const bool bEnableFavoriteSystem = GIsRequestingExit ? false : (GetDefault<UEditorExperimentalSettings>()->bEnableFavoriteSystem && DetailsViewArgs.bAllowFavoriteSystem);

	DetailLayoutHelpers::FUpdatePropertyMapArgs Args;

	Args.LayoutData = &LayoutData;
	Args.InstancedPropertyTypeToDetailLayoutMap = &InstancedTypeToLayoutMap;
	Args.IsPropertyReadOnly = [this](const FPropertyAndParent& PropertyAndParent) { return IsPropertyReadOnly(PropertyAndParent); };
	Args.IsPropertyVisible = [this](const FPropertyAndParent& PropertyAndParent) { return IsPropertyVisible(PropertyAndParent); };
	Args.bEnableFavoriteSystem = bEnableFavoriteSystem;
	Args.bUpdateFavoriteSystemOnly = false;
	DetailLayoutHelpers::UpdateSinglePropertyMapRecursive(*RootPropertyNode, NAME_None, RootPropertyNode.Get(), Args);

	CustomUpdatePropertyMap(LayoutData.DetailLayout);

	// Ask for custom detail layouts, unless disabled. One reason for disabling custom layouts is that the custom layouts
	// inhibit our ability to find a single property's tree node. This is problematic for the diff and merge tools, that need
	// to display and highlight each changed property for the user. We could whitelist 'good' customizations here if 
	// we can make them work with the diff/merge tools.
	if(!bDisableCustomDetailLayouts)
	{
		DetailLayoutHelpers::QueryCustomDetailLayout(LayoutData, InstancedClassToDetailLayoutMap, GenericLayoutDelegate);
	}

	LayoutData.DetailLayout->GenerateDetailLayout();
}


void SDetailsViewBase::OnColorPickerWindowClosed(const TSharedRef<SWindow>& Window)
{
	const TSharedPtr< FPropertyNode > PinnedColorPropertyNode = ColorPropertyNode.Pin();
	if (ensure(PinnedColorPropertyNode.IsValid()))
	{
		UProperty* Property = PinnedColorPropertyNode->GetProperty();
		if (Property && PropertyUtilities.IsValid())
		{
			FPropertyChangedEvent ChangeEvent(Property, EPropertyChangeType::ArrayAdd);
			PinnedColorPropertyNode->FixPropertiesInEvent(ChangeEvent);
			PropertyUtilities->NotifyFinishedChangingProperties(ChangeEvent);
		}
	}

	// A color picker window is no longer open
	bHasOpenColorPicker = false;
	ColorPropertyNode.Reset();
}


void SDetailsViewBase::SetIsPropertyVisibleDelegate(FIsPropertyVisible InIsPropertyVisible)
{
	IsPropertyVisibleDelegate = InIsPropertyVisible;
}

void SDetailsViewBase::SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly InIsPropertyReadOnly)
{
	IsPropertyReadOnlyDelegate = InIsPropertyReadOnly;
}

void SDetailsViewBase::SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled IsPropertyEditingEnabled)
{
	IsPropertyEditingEnabledDelegate = IsPropertyEditingEnabled;
}

bool SDetailsViewBase::IsPropertyEditingEnabled() const
{
	// If the delegate is not bound assume property editing is enabled, otherwise ask the delegate
	return !IsPropertyEditingEnabledDelegate.IsBound() || IsPropertyEditingEnabledDelegate.Execute();
}

void SDetailsViewBase::SetKeyframeHandler( TSharedPtr<class IDetailKeyframeHandler> InKeyframeHandler )
{
	KeyframeHandler = InKeyframeHandler;
}

TSharedPtr<IDetailKeyframeHandler> SDetailsViewBase::GetKeyframeHandler() 
{
	return KeyframeHandler;
}

void SDetailsViewBase::SetExtensionHandler(TSharedPtr<class IDetailPropertyExtensionHandler> InExtensionHandler)
{
	ExtensionHandler = InExtensionHandler;
}

TSharedPtr<IDetailPropertyExtensionHandler> SDetailsViewBase::GetExtensionHandler()
{
	return ExtensionHandler;
}

void SDetailsViewBase::SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance OnGetGenericDetails)
{
	GenericLayoutDelegate = OnGetGenericDetails;
}

void SDetailsViewBase::RefreshRootObjectVisibility()
{
	RerunCurrentFilter();
}

TSharedPtr<FAssetThumbnailPool> SDetailsViewBase::GetThumbnailPool() const
{
	if (!ThumbnailPool.IsValid())
	{
		// Create a thumbnail pool for the view if it doesnt exist.  This does not use resources of no thumbnails are used
		ThumbnailPool = MakeShareable(new FAssetThumbnailPool(50, TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SDetailsView::IsHovered))));
	}

	return ThumbnailPool;
}

void SDetailsViewBase::NotifyFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	OnFinishedChangingPropertiesDelegate.Broadcast(PropertyChangedEvent);
}

void SDetailsViewBase::RequestItemExpanded(TSharedRef<FDetailTreeNode> TreeNode, bool bExpand)
{
	// Don't change expansion state if its already in that state
	if (DetailTree->IsItemExpanded(TreeNode) != bExpand)
	{
		FilteredNodesRequestingExpansionState.Add(TreeNode, bExpand);
	}
}

void SDetailsViewBase::RefreshTree()
{
	if (OnDisplayedPropertiesChangedDelegate.IsBound())
	{
		OnDisplayedPropertiesChangedDelegate.Execute();
	}

	DetailTree->RequestTreeRefresh();
}

void SDetailsViewBase::SaveCustomExpansionState(const FString& NodePath, bool bIsExpanded)
{
	if (bIsExpanded)
	{
		ExpandedDetailNodes.Add(NodePath);
	}
	else
	{
		ExpandedDetailNodes.Remove(NodePath);
	}
}

bool SDetailsViewBase::GetCustomSavedExpansionState(const FString& NodePath) const
{
	return ExpandedDetailNodes.Contains(NodePath);
}

bool SDetailsViewBase::IsPropertyVisible( const FPropertyAndParent& PropertyAndParent ) const
{
	return IsPropertyVisibleDelegate.IsBound() ? IsPropertyVisibleDelegate.Execute(PropertyAndParent) : true;
}

bool SDetailsViewBase::IsPropertyReadOnly( const FPropertyAndParent& PropertyAndParent ) const
{
	return IsPropertyReadOnlyDelegate.IsBound() ? IsPropertyReadOnlyDelegate.Execute(PropertyAndParent) : false;
}

TSharedPtr<IPropertyUtilities> SDetailsViewBase::GetPropertyUtilities()
{
	return PropertyUtilities;
}

void SDetailsViewBase::OnShowOnlyModifiedClicked()
{
	CurrentFilter.bShowOnlyModifiedProperties = !CurrentFilter.bShowOnlyModifiedProperties;

	UpdateFilteredDetails();
}

void SDetailsViewBase::OnShowAllAdvancedClicked()
{
	CurrentFilter.bShowAllAdvanced = !CurrentFilter.bShowAllAdvanced;
	GetMutableDefault<UEditorStyleSettings>()->bShowAllAdvancedDetails = CurrentFilter.bShowAllAdvanced;
	GConfig->SetBool(TEXT("/Script/EditorStyle.EditorStyleSettings"), TEXT("bShowAllAdvancedDetails"), GetMutableDefault<UEditorStyleSettings>()->bShowAllAdvancedDetails, GEditorPerProjectIni);

	UpdateFilteredDetails();
}

void SDetailsViewBase::OnShowOnlyDifferingClicked()
{
	CurrentFilter.bShowOnlyDiffering = !CurrentFilter.bShowOnlyDiffering;

	UpdateFilteredDetails();
}

void SDetailsViewBase::OnShowAllChildrenIfCategoryMatchesClicked()
{
	CurrentFilter.bShowAllChildrenIfCategoryMatches = !CurrentFilter.bShowAllChildrenIfCategoryMatches;

	UpdateFilteredDetails();
}

/** Called when the filter text changes.  This filters specific property nodes out of view */
void SDetailsViewBase::OnFilterTextChanged(const FText& InFilterText)
{
	FString InFilterString = InFilterText.ToString();
	InFilterString.TrimStartAndEndInline();

	// Was the filter just cleared
	bool bFilterCleared = InFilterString.Len() == 0 && CurrentFilter.FilterStrings.Num() > 0;

	FilterView(InFilterString);

}

TSharedPtr<SWidget> SDetailsViewBase::GetNameAreaWidget()
{
	return DetailsViewArgs.bCustomNameAreaLocation ? NameArea : nullptr;
}

TSharedPtr<SWidget> SDetailsViewBase::GetFilterAreaWidget()
{
	return DetailsViewArgs.bCustomFilterAreaLocation ? FilterRow : nullptr;
}

TSharedPtr<class FUICommandList> SDetailsViewBase::GetHostCommandList() const
{
	return DetailsViewArgs.HostCommandList;
}

TSharedPtr<FTabManager> SDetailsViewBase::GetHostTabManager() const
{
	return DetailsViewArgs.HostTabManager;
}

void SDetailsViewBase::SetHostTabManager(TSharedPtr<FTabManager> InTabManager)
{
	DetailsViewArgs.HostTabManager = InTabManager;
}


/** 
 * Hides or shows properties based on the passed in filter text
 * 
 * @param InFilterText	The filter text
 */
void SDetailsViewBase::FilterView(const FString& InFilterText)
{
	TArray<FString> CurrentFilterStrings;

	FString ParseString = InFilterText;
	// Remove whitespace from the front and back of the string
	ParseString.TrimStartAndEndInline();
	ParseString.ParseIntoArray(CurrentFilterStrings, TEXT(" "), true);

	bHasActiveFilter = CurrentFilterStrings.Num() > 0;

	CurrentFilter.FilterStrings = CurrentFilterStrings;

	UpdateFilteredDetails();
}


EVisibility SDetailsViewBase::GetFilterBoxVisibility() const
{
	// Visible if we allow search and we have anything to search otherwise collapsed so it doesn't take up room
	return (DetailsViewArgs.bAllowSearch && IsConnected() && RootTreeNodes.Num() > 0) || HasActiveSearch() || CurrentFilter.bShowOnlyModifiedProperties || CurrentFilter.bShowOnlyDiffering ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SDetailsViewBase::SupportsKeyboardFocus() const
{
	return DetailsViewArgs.bSearchInitialKeyFocus && SearchBox->SupportsKeyboardFocus() && GetFilterBoxVisibility() == EVisibility::Visible;
}

FReply SDetailsViewBase::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	FReply Reply = FReply::Handled();

	if (InFocusEvent.GetCause() != EFocusCause::Cleared)
	{
		Reply.SetUserFocus(SearchBox.ToSharedRef(), InFocusEvent.GetCause());
	}

	return Reply;
}

/** Ticks the property view.  This function performs a data consistency check */
void SDetailsViewBase::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	for (int32 i = 0; i < CustomizationClassInstancesPendingDelete.Num(); ++i)
	{
		ensure(CustomizationClassInstancesPendingDelete[i].IsUnique());
	}

	// Release any pending kill nodes.
	for ( TSharedPtr<FComplexPropertyNode>& PendingKillNode : RootNodesPendingKill )
	{
		if ( PendingKillNode.IsValid() )
		{
			PendingKillNode->Disconnect();
			PendingKillNode.Reset();
		}
	}
	RootNodesPendingKill.Empty();

	// Empty all the customization instances that need to be deleted
	CustomizationClassInstancesPendingDelete.Empty();

	FRootPropertyNodeList& RootPropertyNodes = GetRootNodes();

	for(TSharedPtr<FComplexPropertyNode>& RootPropertyNode : RootPropertyNodes)
	{
		check(RootPropertyNode.IsValid());

		// Purge any objects that are marked pending kill from the object list
		if(auto ObjectRoot = RootPropertyNode->AsObjectNode())
		{
			ObjectRoot->PurgeKilledObjects();
		}

		if(DeferredActions.Num() > 0)
		{		
			// Any deferred actions are likely to cause the node  tree to be at least partially rebuilt
			// Save the expansion state of existing nodes so we can expand them later
			SaveExpandedItems(RootPropertyNode.ToSharedRef());
		}
	}

	if (DeferredActions.Num() > 0)
	{
		// Execute any deferred actions
		for (int32 ActionIndex = 0; ActionIndex < DeferredActions.Num(); ++ActionIndex)
		{
			DeferredActions[ActionIndex].ExecuteIfBound();
		}
		DeferredActions.Empty();
	}

	TSharedPtr<FComplexPropertyNode> LastRootPendingKill;
	if (RootNodesPendingKill.Num() > 0 )
	{
		LastRootPendingKill = RootNodesPendingKill.Last();
	}

	bool bValidateExternalNodes = true;

	int32 FoundIndex = RootPropertyNodes.Find(LastRootPendingKill);
	if (FoundIndex != INDEX_NONE)
	{ 
		// Reaquire the root property nodes.  It may have been changed by the deferred actions if something like a blueprint editor forcefully resets a details panel during a posteditchange
		ForceRefresh();

		// All objects are being reset, no need to validate external nodes
		bValidateExternalNodes = false;
	}
	else
	{
		for(TSharedPtr<FComplexPropertyNode>& RootPropertyNode : RootPropertyNodes)
		{
			if(RootPropertyNode == LastRootPendingKill)
			{
				RestoreExpandedItems(RootPropertyNode.ToSharedRef());
			}

			EPropertyDataValidationResult Result = RootPropertyNode->EnsureDataIsValid();
			if(Result == EPropertyDataValidationResult::PropertiesChanged || Result == EPropertyDataValidationResult::EditInlineNewValueChanged)
			{
				RestoreExpandedItems(RootPropertyNode.ToSharedRef());
				UpdatePropertyMaps();
				UpdateFilteredDetails();
			}
			else if(Result == EPropertyDataValidationResult::ArraySizeChanged)
			{
				RestoreExpandedItems(RootPropertyNode.ToSharedRef());
				UpdateFilteredDetails();
			}
			else if(Result == EPropertyDataValidationResult::ObjectInvalid)
			{
				ForceRefresh();
				break;

				// All objects are being reset, no need to validate external nodes
				bValidateExternalNodes = false;
			}
		}
	}

	if (bValidateExternalNodes)
	{
		for (FDetailLayoutData& LayoutData : DetailLayouts)
		{
			FRootPropertyNodeList& ExternalRootPropertyNodes = LayoutData.DetailLayout->GetExternalRootPropertyNodes();

			for (int32 NodeIndex = 0; NodeIndex < ExternalRootPropertyNodes.Num(); ++NodeIndex)
			{
				TSharedPtr<FPropertyNode> PropertyNode = ExternalRootPropertyNodes[NodeIndex];
				{
					EPropertyDataValidationResult Result = PropertyNode->EnsureDataIsValid();
					if (Result == EPropertyDataValidationResult::PropertiesChanged || Result == EPropertyDataValidationResult::EditInlineNewValueChanged)
					{
						RestoreExpandedItems(PropertyNode.ToSharedRef());

						// Note this will invalidate all the external root nodes so there is no need to continue
						ExternalRootPropertyNodes.Empty();

						UpdatePropertyMaps();
						UpdateFilteredDetails();

						break;
					}
					else if (Result == EPropertyDataValidationResult::ArraySizeChanged)
					{
						RestoreExpandedItems(PropertyNode.ToSharedRef());
						UpdateFilteredDetails();
					}
				}
			}
		}
	}

	for(FDetailLayoutData& LayoutData : DetailLayouts)
	{
		if(LayoutData.DetailLayout.IsValid())
		{
			LayoutData.DetailLayout->Tick(InDeltaTime);
		}
	}

	if (!ColorPropertyNode.IsValid() && bHasOpenColorPicker)
	{
		// Destroy the color picker window if the color property node has become invalid
		DestroyColorPicker();
		bHasOpenColorPicker = false;
	}


	if (FilteredNodesRequestingExpansionState.Num() > 0)
	{
		// change expansion state on the nodes that request it
		for (TMap<TSharedRef<FDetailTreeNode>, bool >::TConstIterator It(FilteredNodesRequestingExpansionState); It; ++It)
		{
			DetailTree->SetItemExpansion(It.Key(), It.Value());
		}

		FilteredNodesRequestingExpansionState.Empty();
	}
}

/**
* Recursively gets expanded items for a node
*
* @param InPropertyNode			The node to get expanded items from
* @param OutExpandedItems	List of expanded items that were found
*/
void GetExpandedItems(TSharedPtr<FPropertyNode> InPropertyNode, TArray<FString>& OutExpandedItems)
{
	if (InPropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded))
	{
		const bool bWithArrayIndex = true;
		FString Path;
		Path.Empty(128);
		InPropertyNode->GetQualifiedName(Path, bWithArrayIndex);

		OutExpandedItems.Add(Path);
	}

	for (int32 ChildIndex = 0; ChildIndex < InPropertyNode->GetNumChildNodes(); ++ChildIndex)
	{
		GetExpandedItems(InPropertyNode->GetChildNode(ChildIndex), OutExpandedItems);
	}

}

/**
* Recursively sets expanded items for a node
*
* @param InNode			The node to set expanded items on
* @param OutExpandedItems	List of expanded items to set
*/
void SetExpandedItems(TSharedPtr<FPropertyNode> InPropertyNode, const TSet<FString>& InExpandedItems)
{
	if (InExpandedItems.Num() > 0)
	{
		const bool bWithArrayIndex = true;
		FString Path;
		Path.Empty(128);
		InPropertyNode->GetQualifiedName(Path, bWithArrayIndex);

		if (InExpandedItems.Contains(Path))
		{
			InPropertyNode->SetNodeFlags(EPropertyNodeFlags::Expanded, true);
		}

		for (int32 NodeIndex = 0; NodeIndex < InPropertyNode->GetNumChildNodes(); ++NodeIndex)
		{
			SetExpandedItems(InPropertyNode->GetChildNode(NodeIndex), InExpandedItems);
		}
	}
}

void SDetailsViewBase::SaveExpandedItems( TSharedRef<FPropertyNode> StartNode )
{
	UStruct* BestBaseStruct = StartNode->FindComplexParent()->GetBaseStructure();

	TArray<FString> ExpandedPropertyItems;
	GetExpandedItems(StartNode, ExpandedPropertyItems);

	// Handle spaces in expanded node names by wrapping them in quotes
	for( FString& String : ExpandedPropertyItems )
	{
		String.InsertAt(0, '"');
		String.AppendChar('"');
	}

	TArray<FString> ExpandedCustomItems = ExpandedDetailNodes.Array();

	// Expanded custom items may have spaces but SetSingleLineArray doesnt support spaces (treats it as another element in the array)
	// Append a '|' after each element instead
	FString ExpandedCustomItemsString;
	for (auto It = ExpandedDetailNodes.CreateConstIterator(); It; ++It)
	{
		ExpandedCustomItemsString += *It;
		ExpandedCustomItemsString += TEXT(",");
	}

	//while a valid class, and we're either the same as the base class (for multiple actors being selected and base class is AActor) OR we're not down to AActor yet)
	for (UStruct* Struct = BestBaseStruct; Struct && ((BestBaseStruct == Struct) || (Struct != AActor::StaticClass())); Struct = Struct->GetSuperStruct())
	{
		if (StartNode->GetNumChildNodes() > 0)
		{
			bool bShouldSave = ExpandedPropertyItems.Num() > 0;
			if (!bShouldSave)
			{
				TArray<FString> DummyExpandedPropertyItems;
				GConfig->GetSingleLineArray(TEXT("DetailPropertyExpansion"), *Struct->GetName(), DummyExpandedPropertyItems, GEditorPerProjectIni);
				bShouldSave = DummyExpandedPropertyItems.Num() > 0;
			}

			if (bShouldSave)
			{
				GConfig->SetSingleLineArray(TEXT("DetailPropertyExpansion"), *Struct->GetName(), ExpandedPropertyItems, GEditorPerProjectIni);
			}
		}
	}

	if (DetailLayouts.Num() > 0 && BestBaseStruct)
	{
		bool bShouldSave = !ExpandedCustomItemsString.IsEmpty();
		if (!bShouldSave)
		{
			FString DummyExpandedCustomItemsString;
			GConfig->GetString(TEXT("DetailCustomWidgetExpansion"), *BestBaseStruct->GetName(), DummyExpandedCustomItemsString, GEditorPerProjectIni);
			bShouldSave = !DummyExpandedCustomItemsString.IsEmpty();
		}
		if (bShouldSave)
		{
			GConfig->SetString(TEXT("DetailCustomWidgetExpansion"), *BestBaseStruct->GetName(), *ExpandedCustomItemsString, GEditorPerProjectIni);
		}
	}
}

void SDetailsViewBase::RestoreExpandedItems(TSharedRef<FPropertyNode> InitialStartNode)
{
	TSharedPtr<FPropertyNode> StartNode = InitialStartNode;

	ExpandedDetailNodes.Empty();

	FString ExpandedCustomItems;

	UStruct* BestBaseStruct = StartNode->FindComplexParent()->GetBaseStructure();

	//while a valid class, and we're either the same as the base class (for multiple actors being selected and base class is AActor) OR we're not down to AActor yet)
	TArray<FString> DetailPropertyExpansionStrings;
	for (UStruct* Struct = BestBaseStruct; Struct && ((BestBaseStruct == Struct) || (Struct != AActor::StaticClass())); Struct = Struct->GetSuperStruct())
	{
		GConfig->GetSingleLineArray(TEXT("DetailPropertyExpansion"), *Struct->GetName(), DetailPropertyExpansionStrings, GEditorPerProjectIni);
	}

	TSet<FString> ExpandedPropertyItems;
	ExpandedPropertyItems.Append(DetailPropertyExpansionStrings);
	SetExpandedItems(StartNode, ExpandedPropertyItems);

	if (BestBaseStruct)
	{
		GConfig->GetString(TEXT("DetailCustomWidgetExpansion"), *BestBaseStruct->GetName(), ExpandedCustomItems, GEditorPerProjectIni);
		TArray<FString> ExpandedCustomItemsArray;
		ExpandedCustomItems.ParseIntoArray(ExpandedCustomItemsArray, TEXT(","), true);

		ExpandedDetailNodes.Append(ExpandedCustomItemsArray);
	}
}

void SDetailsViewBase::UpdateFilteredDetails()
{
	RootTreeNodes.Reset();

	FDetailNodeList InitialRootNodeList;
	
	NumVisbleTopLevelObjectNodes = 0;
	FRootPropertyNodeList& RootPropertyNodes = GetRootNodes();

	if( GetDefault<UEditorStyleSettings>()->bShowAllAdvancedDetails )
	{
		CurrentFilter.bShowAllAdvanced = true;
	}
	
	for(int32 RootNodeIndex = 0; RootNodeIndex < RootPropertyNodes.Num(); ++RootNodeIndex)
	{
		TSharedPtr<FComplexPropertyNode>& RootPropertyNode = RootPropertyNodes[RootNodeIndex];
		if(RootPropertyNode.IsValid())
		{
			RootPropertyNode->FilterNodes(CurrentFilter.FilterStrings);
			RootPropertyNode->ProcessSeenFlags(true);

			TSharedPtr<FDetailLayoutBuilderImpl>& DetailLayout = DetailLayouts[RootNodeIndex].DetailLayout;
			if(DetailLayout.IsValid())
			{
				FRootPropertyNodeList& ExternalRootPropertyNodes = DetailLayout->GetExternalRootPropertyNodes();
				for (auto ExternalRootNode : ExternalRootPropertyNodes)
				{
					if (ExternalRootNode.IsValid())
					{
						ExternalRootNode->FilterNodes(CurrentFilter.FilterStrings);
						ExternalRootNode->ProcessSeenFlags(true);
					
						RestoreExpandedItems(ExternalRootNode.ToSharedRef());
					}
				}

				DetailLayout->FilterDetailLayout(CurrentFilter);

				FDetailNodeList& LayoutRoots = DetailLayout->GetFilteredRootTreeNodes();
				if (LayoutRoots.Num() > 0)
				{
					// A top level object nodes has a non-filtered away root so add one to the total number we have
					++NumVisbleTopLevelObjectNodes;

					InitialRootNodeList.Append(LayoutRoots);
				}
			}
		}
	}


	// for multiple top level object we need to do a secondary pass on top level object nodes after we have determined if there is any nodes visible at all.  If there are then we ask the details panel if it wants to show childen
	for(TSharedRef<class FDetailTreeNode> RootNode : InitialRootNodeList)
	{
		if(RootNode->ShouldShowOnlyChildren())
		{
			RootNode->GetChildren(RootTreeNodes);
		}
		else
		{
			RootTreeNodes.Add(RootNode);
		}
	}

	RefreshTree();
}



void SDetailsViewBase::RegisterInstancedCustomPropertyLayout(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate)
{
	check( Class );

	FDetailLayoutCallback Callback;
	Callback.DetailLayoutDelegate = DetailLayoutDelegate;
	// @todo: DetailsView: Fix me: this specifies the order in which detail layouts should be queried
	Callback.Order = InstancedClassToDetailLayoutMap.Num();

	InstancedClassToDetailLayoutMap.Add( Class, Callback );	
}

void SDetailsViewBase::RegisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier /*= nullptr*/)
{
	FPropertyTypeLayoutCallback Callback;
	Callback.PropertyTypeLayoutDelegate = PropertyTypeLayoutDelegate;
	Callback.PropertyTypeIdentifier = Identifier;

	FPropertyTypeLayoutCallbackList* LayoutCallbacks = InstancedTypeToLayoutMap.Find(PropertyTypeName);
	if (LayoutCallbacks)
	{
		LayoutCallbacks->Add(Callback);
	}
	else
	{
		FPropertyTypeLayoutCallbackList NewLayoutCallbacks;
		NewLayoutCallbacks.Add(Callback);
		InstancedTypeToLayoutMap.Add(PropertyTypeName, NewLayoutCallbacks);
	}
}

void SDetailsViewBase::UnregisterInstancedCustomPropertyLayout(UStruct* Class)
{
	check( Class );

	InstancedClassToDetailLayoutMap.Remove( Class );	
}

void SDetailsViewBase::UnregisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, TSharedPtr<IPropertyTypeIdentifier> Identifier /*= nullptr*/)
{
	FPropertyTypeLayoutCallbackList* LayoutCallbacks = InstancedTypeToLayoutMap.Find(PropertyTypeName);

	if (LayoutCallbacks)
	{
		LayoutCallbacks->Remove(Identifier);
	}
}
