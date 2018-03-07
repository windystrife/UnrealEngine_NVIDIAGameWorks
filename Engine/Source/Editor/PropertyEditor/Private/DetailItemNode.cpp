// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DetailItemNode.h"
#include "DetailCategoryGroupNode.h"
#include "DetailGroup.h"
#include "DetailPropertyRow.h"
#include "SDetailSingleItemRow.h"



FDetailItemNode::FDetailItemNode(const FDetailLayoutCustomization& InCustomization, TSharedRef<FDetailCategoryImpl> InParentCategory, TAttribute<bool> InIsParentEnabled, TSharedPtr<IDetailGroup> InParentGroup)
	: Customization( InCustomization )
	, ParentCategory( InParentCategory )
	, ParentGroup(InParentGroup)
	, IsParentEnabled( InIsParentEnabled )
	, CachedItemVisibility( EVisibility::Visible )
	, bShouldBeVisibleDueToFiltering( false )
	, bShouldBeVisibleDueToChildFiltering( false )
	, bTickable( false )
	, bIsExpanded( InCustomization.HasCustomBuilder() ? !InCustomization.CustomBuilderRow->IsInitiallyCollapsed() : false )
	, bIsHighlighted( false )
{
	
}

void FDetailItemNode::Initialize()
{
	if( ( Customization.HasCustomWidget() && Customization.WidgetDecl->VisibilityAttr.IsBound() )
		|| ( Customization.HasCustomBuilder() && Customization.CustomBuilderRow->RequiresTick() )
		|| ( Customization.HasPropertyNode() && Customization.PropertyRow->RequiresTick() )
		|| ( Customization.HasGroup() && Customization.DetailGroup->RequiresTick() ) )
	{
		// The node needs to be ticked because it has widgets that can dynamically come and go
		bTickable = true;
		ParentCategory.Pin()->AddTickableNode( *this );
	}

	if( Customization.HasPropertyNode() )
	{
		InitPropertyEditor();
	}
	else if( Customization.HasCustomBuilder() )
	{
		InitCustomBuilder();
	}
	else if( Customization.HasGroup() )
	{
		InitGroup();
	}

	if (Customization.PropertyRow.IsValid() && Customization.PropertyRow->GetForceAutoExpansion())
	{
		const bool bShouldExpand = true;
		const bool bSaveState = false;
		SetExpansionState(bShouldExpand, bSaveState);
	}

	// Cache the visibility of customizations that can set it
	if( Customization.HasCustomWidget() )
	{	
		CachedItemVisibility = Customization.WidgetDecl->VisibilityAttr.Get();
	}
	else if( Customization.HasPropertyNode() )
	{
		CachedItemVisibility = Customization.PropertyRow->GetPropertyVisibility();
	}
	else if( Customization.HasGroup() )
	{
		CachedItemVisibility = Customization.DetailGroup->GetGroupVisibility();
	}

	const bool bUpdateFilteredNodes = false;
	GenerateChildren( bUpdateFilteredNodes );
}

FDetailItemNode::~FDetailItemNode()
{
	if( bTickable && ParentCategory.IsValid() )
	{
		ParentCategory.Pin()->RemoveTickableNode( *this );
	}
}

EDetailNodeType FDetailItemNode::GetNodeType() const
{
	if (Customization.HasPropertyNode() && Customization.GetPropertyNode()->AsCategoryNode())
	{
		return EDetailNodeType::Category;
	}
	else
	{
		return EDetailNodeType::Item;
	}
}

TSharedPtr<IPropertyHandle> FDetailItemNode::CreatePropertyHandle() const
{
	if (Customization.HasPropertyNode())
	{
		return ParentCategory.Pin()->GetParentLayoutImpl().GetPropertyHandle(Customization.GetPropertyNode());
	}
	else
	{
		return nullptr;
	}
}

void FDetailItemNode::InitPropertyEditor()
{
	UProperty* NodeProperty = Customization.GetPropertyNode()->GetProperty();

	if( NodeProperty && (NodeProperty->IsA<UArrayProperty>() || NodeProperty->IsA<USetProperty>() || NodeProperty->IsA<UMapProperty>() ))
	{
		const bool bUpdateFilteredNodes = false;
		FSimpleDelegate OnRegenerateChildren = FSimpleDelegate::CreateSP( this, &FDetailItemNode::GenerateChildren, bUpdateFilteredNodes );

		Customization.GetPropertyNode()->SetOnRebuildChildren( OnRegenerateChildren );
	}

	Customization.PropertyRow->OnItemNodeInitialized( ParentCategory.Pin().ToSharedRef(), IsParentEnabled, ParentGroup.IsValid() ? ParentGroup.Pin() : nullptr );

	if (Customization.HasExternalPropertyRow())
	{
		const bool bSaveState = false;
		SetExpansionState(ParentCategory.Pin()->GetSavedExpansionState(*this), bSaveState);
	}
}

void FDetailItemNode::InitCustomBuilder()
{
	Customization.CustomBuilderRow->OnItemNodeInitialized( AsShared(), ParentCategory.Pin().ToSharedRef(), IsParentEnabled );

	// Restore saved expansion state
	FName BuilderName = Customization.CustomBuilderRow->GetCustomBuilderName();
	if( BuilderName != NAME_None )
	{
		const bool bSaveState = false;
		SetExpansionState(ParentCategory.Pin()->GetSavedExpansionState(*this), bSaveState);
	}

}

void FDetailItemNode::InitGroup()
{
	Customization.DetailGroup->OnItemNodeInitialized( AsShared(), ParentCategory.Pin().ToSharedRef(), IsParentEnabled );

	if (Customization.DetailGroup->ShouldStartExpanded())
	{
		bIsExpanded = true;
	}
	else
	{
		// Restore saved expansion state
		FName GroupName = Customization.DetailGroup->GetGroupName();
		if (GroupName != NAME_None)
		{
			const bool bSaveState = false;
			SetExpansionState(ParentCategory.Pin()->GetSavedExpansionState(*this), bSaveState);
		}
	}
}

bool FDetailItemNode::HasMultiColumnWidget() const
{
	return ( Customization.HasCustomWidget() && Customization.WidgetDecl->HasColumns() )
		|| ( Customization.HasCustomBuilder() && Customization.CustomBuilderRow->HasColumns() )
		|| ( Customization.HasGroup() && Customization.DetailGroup->HasColumns() )
		|| ( Customization.HasPropertyNode() && Customization.PropertyRow->HasColumns());
}

void FDetailItemNode::ToggleExpansion()
{
	const bool bSaveState = true;
	SetExpansionState( !bIsExpanded, bSaveState );
}

void FDetailItemNode::SetExpansionState(bool bWantsExpanded, bool bSaveState)
{
	bIsExpanded = bWantsExpanded;

	// Expand the child after filtering if it wants to be expanded
	ParentCategory.Pin()->RequestItemExpanded(AsShared(), bIsExpanded);

	OnItemExpansionChanged(bIsExpanded, bSaveState);
}

TSharedRef< ITableRow > FDetailItemNode::GenerateWidgetForTableView( const TSharedRef<STableViewBase>& OwnerTable, const FDetailColumnSizeData& ColumnSizeData, bool bAllowFavoriteSystem)
{
	FTagMetaData TagMeta(TEXT("DetailRowItem"));
	if (ParentCategory.IsValid())
	{
		if (Customization.IsValidCustomization() && Customization.GetPropertyNode().IsValid())
		{
			TagMeta.Tag = *FString::Printf(TEXT("DetailRowItem.%s"), *Customization.GetPropertyNode()->GetDisplayName().ToString());
		}
		else if (Customization.HasCustomWidget() )
		{
			TagMeta.Tag = Customization.GetWidgetRow().RowTagName;
		}
	}
	if( Customization.HasPropertyNode() && Customization.GetPropertyNode()->AsCategoryNode() )
	{
		return
			SNew(SDetailCategoryTableRow, AsShared(), OwnerTable)
			.DisplayName(Customization.GetPropertyNode()->GetDisplayName())
			.AddMetaData<FTagMetaData>(TagMeta)
			.InnerCategory( true );
	}
	else
	{
		return
			SNew(SDetailSingleItemRow, &Customization, HasMultiColumnWidget(), AsShared(), OwnerTable )
			.AddMetaData<FTagMetaData>(TagMeta)
			.ColumnSizeData(ColumnSizeData)
			.AllowFavoriteSystem(bAllowFavoriteSystem);
	}
}

bool FDetailItemNode::GenerateStandaloneWidget(FDetailWidgetRow& OutRow) const
{
	bool bResult = false;

	if (Customization.HasPropertyNode() && Customization.GetPropertyNode()->AsCategoryNode())
	{
		const bool bIsInnerCategory = true;

		OutRow.NameContent()
		[
			SNew(STextBlock)
			.Text(Customization.GetPropertyNode()->GetDisplayName())
			.Font(FEditorStyle::GetFontStyle(bIsInnerCategory ? "PropertyWindow.NormalFont" : "DetailsView.CategoryFontStyle"))
			.ShadowOffset(bIsInnerCategory ? FVector2D::ZeroVector : FVector2D(1.0f, 1.0f))
		];

		bResult = true;
	}
	else if(Customization.IsValidCustomization())
	{
		FDetailWidgetRow Row = Customization.GetWidgetRow();

		// We make some slight modifications to the row here before giving it to OutRow
		if (HasMultiColumnWidget())
		{
			TSharedPtr<SWidget> NameWidget;
			TSharedPtr<SWidget> ValueWidget;

			NameWidget = Row.NameWidget.Widget;
			if (Row.IsEnabledAttr.IsBound())
			{
				NameWidget->SetEnabled(Row.IsEnabledAttr);
			}

			ValueWidget =
				SNew(SConstrainedBox)
				.MinWidth(Row.ValueWidget.MinWidth)
				.MaxWidth(Row.ValueWidget.MaxWidth)
				[
					Row.ValueWidget.Widget
				];

			if (Row.IsEnabledAttr.IsBound())
			{
				ValueWidget->SetEnabled(Row.IsEnabledAttr);
			}

			OutRow.NameContent()
			[
				NameWidget.ToSharedRef()
			];

			OutRow.ValueContent()
			[
				ValueWidget.ToSharedRef()
			];
		}
		else
		{
			OutRow.WholeRowContent()
			[
				Row.WholeRowWidget.Widget
			];
		}

		bResult = true;
	}

	return bResult;
}

void FDetailItemNode::GetChildren(FDetailNodeList& OutChildren)
{
	for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailTreeNode>& Child = Children[ChildIndex];

		ENodeVisibility ChildVisibility = Child->GetVisibility();

		// Report the child if the child is visible or we are visible due to filtering and there were no filtered children.  
		// If we are visible due to filtering and so is a child, we only show that child.  
		// If we are visible due to filtering and no child is visible, we show all children

		if( ChildVisibility == ENodeVisibility::Visible ||
			( !bShouldBeVisibleDueToChildFiltering && bShouldBeVisibleDueToFiltering && ChildVisibility != ENodeVisibility::ForcedHidden ) )
		{
			if( Child->ShouldShowOnlyChildren() )
			{
				Child->GetChildren( OutChildren );
			}
			else
			{
				OutChildren.Add( Child );
			}
		}
	}
}

void FDetailItemNode::GenerateChildren( bool bUpdateFilteredNodes )
{
	Children.Empty();

	if (!ParentCategory.IsValid())
	{
		return;
	}

	if( Customization.HasPropertyNode() )
	{
		Customization.PropertyRow->OnGenerateChildren( Children );
	}
	else if( Customization.HasCustomBuilder() )
	{
		Customization.CustomBuilderRow->OnGenerateChildren( Children );

		// Need to refresh the tree for custom builders as we could be regenerating children at any point
		ParentCategory.Pin()->RefreshTree( bUpdateFilteredNodes );
	}
	else if( Customization.HasGroup() )
	{
		Customization.DetailGroup->OnGenerateChildren( Children );
	}
}


void FDetailItemNode::OnItemExpansionChanged( bool bInIsExpanded, bool bShouldSaveState )
{
	bIsExpanded = bInIsExpanded;
	if( Customization.HasPropertyNode() )
	{
		Customization.GetPropertyNode()->SetNodeFlags( EPropertyNodeFlags::Expanded, bInIsExpanded );
	}

	if (ParentCategory.IsValid() && bShouldSaveState &&
			(  (Customization.HasCustomBuilder() && Customization.CustomBuilderRow->GetCustomBuilderName() != NAME_None)
			|| (Customization.HasGroup() && Customization.DetailGroup->GetGroupName() != NAME_None)
			|| (Customization.HasExternalPropertyRow())))
	{
		ParentCategory.Pin()->SaveExpansionState(*this);
	}
}

bool FDetailItemNode::ShouldBeExpanded() const
{
	bool bShouldBeExpanded = bIsExpanded || bShouldBeVisibleDueToChildFiltering;
	if( Customization.HasPropertyNode() )
	{
		FPropertyNode& PropertyNode = *Customization.GetPropertyNode();
		bShouldBeExpanded = PropertyNode.HasNodeFlags( EPropertyNodeFlags::Expanded ) != 0;
		bShouldBeExpanded |= PropertyNode.HasNodeFlags( EPropertyNodeFlags::IsSeenDueToChildFiltering ) != 0;
	}
	return bShouldBeExpanded;
}

ENodeVisibility FDetailItemNode::GetVisibility() const
{
	const bool bHasAnythingToShow = Customization.IsValidCustomization();

	const bool bIsForcedHidden = 
		!bHasAnythingToShow 
		|| (Customization.HasCustomWidget() && Customization.WidgetDecl->VisibilityAttr.Get() != EVisibility::Visible )
		|| (Customization.HasPropertyNode() && Customization.PropertyRow->GetPropertyVisibility() != EVisibility::Visible );

	ENodeVisibility Visibility;
	if( bIsForcedHidden )
	{
		Visibility = ENodeVisibility::ForcedHidden;
	}
	else
	{
		Visibility = (bShouldBeVisibleDueToFiltering || bShouldBeVisibleDueToChildFiltering) ? ENodeVisibility::Visible : ENodeVisibility::HiddenDueToFiltering;
	}

	return Visibility;
}

static bool PassesAllFilters( const FDetailLayoutCustomization& InCustomization, const FDetailFilter& InFilter, const FString& InCategoryName )
{	
	struct Local
	{
		static bool StringPassesFilter(const FDetailFilter& InDetailFilter, const FString& InString)
		{
			// Make sure the passed string matches all filter strings
			if( InString.Len() > 0 )
			{
				for (int32 TestNameIndex = 0; TestNameIndex < InDetailFilter.FilterStrings.Num(); ++TestNameIndex)
				{
					const FString& TestName = InDetailFilter.FilterStrings[TestNameIndex];
					if ( !InString.Contains(TestName) ) 
					{
						return false;
					}
				}
				return true;
			}
			return false;
		}
	};

	bool bPassesAllFilters = true;

	if( InFilter.FilterStrings.Num() > 0 || InFilter.bShowOnlyModifiedProperties == true || InFilter.bShowOnlyDiffering == true )
	{
		const bool bSearchFilterIsEmpty = InFilter.FilterStrings.Num() == 0;

		TSharedPtr<FPropertyNode> PropertyNodePin = InCustomization.GetPropertyNode();

		const bool bPassesCategoryFilter = !bSearchFilterIsEmpty && InFilter.bShowAllChildrenIfCategoryMatches ? Local::StringPassesFilter(InFilter, InCategoryName) : false;

		bPassesAllFilters = false;
		if( PropertyNodePin.IsValid() && !PropertyNodePin->AsCategoryNode() )
		{
			
			const bool bIsNotBeingFiltered = PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::IsBeingFiltered) == 0;
			const bool bIsSeenDueToFiltering = PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering) != 0;
			const bool bIsParentSeenDueToFiltering = PropertyNodePin->HasNodeFlags(EPropertyNodeFlags::IsParentSeenDueToFiltering) != 0;

			const bool bPassesSearchFilter = bSearchFilterIsEmpty || ( bIsNotBeingFiltered || bIsSeenDueToFiltering || bIsParentSeenDueToFiltering );
			const bool bPassesModifiedFilter = bPassesSearchFilter && ( InFilter.bShowOnlyModifiedProperties == false || PropertyNodePin->GetDiffersFromDefault() == true );
			const bool bPassesDifferingFilter = InFilter.bShowOnlyDiffering ? InFilter.WhitelistedProperties.Find(*FPropertyNode::CreatePropertyPath(PropertyNodePin.ToSharedRef())) != nullptr : true;

			// The property node is visible (note categories are never visible unless they have a child that is visible )
			bPassesAllFilters = (bPassesSearchFilter && bPassesModifiedFilter && bPassesDifferingFilter) || bPassesCategoryFilter;
		}
		else if (InCustomization.HasCustomWidget())
		{
			const bool bPassesTextFilter = Local::StringPassesFilter(InFilter, InCustomization.WidgetDecl->FilterTextString.ToString());
			const bool bPassesModifiedFilter = (InFilter.bShowOnlyModifiedProperties == false || InCustomization.WidgetDecl->DiffersFromDefaultAttr.Get() == true);

			bPassesAllFilters = (bPassesTextFilter && bPassesModifiedFilter) || bPassesCategoryFilter;
		}
	}

	return bPassesAllFilters;
}

void FDetailItemNode::Tick( float DeltaTime )
{
	if( ensure( bTickable ) )
	{
		if( Customization.HasCustomBuilder() && Customization.CustomBuilderRow->RequiresTick() ) 
		{
			Customization.CustomBuilderRow->Tick( DeltaTime );
		}

		// Recache visibility
		EVisibility NewVisibility;
		if( Customization.HasCustomWidget() )
		{	
			NewVisibility = Customization.WidgetDecl->VisibilityAttr.Get();
		}
		else if( Customization.HasPropertyNode() )
		{
			NewVisibility = Customization.PropertyRow->GetPropertyVisibility();
		}
		else if( Customization.HasGroup() )
		{
			NewVisibility = Customization.DetailGroup->GetGroupVisibility();
		}
	
		if( CachedItemVisibility != NewVisibility )
		{
			// The visibility of a node in the tree has changed.  We must refresh the tree to remove the widget
			CachedItemVisibility = NewVisibility;
			const bool bRefilterCategory = true;
			ParentCategory.Pin()->RefreshTree( bRefilterCategory );
		}
	}
}

bool FDetailItemNode::ShouldShowOnlyChildren() const
{
	// Show only children of this node if there is no content for custom details or the property node requests that only children be shown
	return ( Customization.HasCustomBuilder() && Customization.CustomBuilderRow->ShowOnlyChildren() )
		 || (Customization.HasPropertyNode() && Customization.PropertyRow->ShowOnlyChildren() );
}

FName FDetailItemNode::GetNodeName() const
{
	if( Customization.HasCustomBuilder() )
	{
		return Customization.CustomBuilderRow->GetCustomBuilderName();
	}
	else if( Customization.HasGroup() )
	{
		return Customization.DetailGroup->GetGroupName();
	}
	else if (Customization.HasExternalPropertyRow())
	{
		FName CustomName = Customization.PropertyRow->GetCustomExpansionId();

		return CustomName;
	}
	return NAME_None;
}

FPropertyPath FDetailItemNode::GetPropertyPath() const
{
	FPropertyPath Ret;
	auto PropertyNode = Customization.GetPropertyNode();
	if( PropertyNode.IsValid() )
	{
		Ret = *FPropertyNode::CreatePropertyPath( PropertyNode.ToSharedRef() );
	}
	return Ret;
}

void FDetailItemNode::FilterNode( const FDetailFilter& InFilter )
{
	bShouldBeVisibleDueToFiltering = PassesAllFilters( Customization, InFilter, ParentCategory.Pin()->GetDisplayName().ToString() );

	bShouldBeVisibleDueToChildFiltering = false;

	// Filter each child
	for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailTreeNode>& Child = Children[ChildIndex];

		// If the parent is visible, we pass an empty filter to all children so that they resume their
		// default expansion.  This is a lot safer method, otherwise customized details panels tend to be
		// filtered incorrectly because they have no means of discovering if their parents were filtered.
		if ( bShouldBeVisibleDueToFiltering )
		{
			Child->FilterNode(FDetailFilter());

			// The child should be visible, but maybe something else has it hidden, check if it's
			// visible just for safety reasons.
			if ( Child->GetVisibility() == ENodeVisibility::Visible )
			{
				// Expand the child after filtering if it wants to be expanded
				ParentCategory.Pin()->RequestItemExpanded(Child, Child->ShouldBeExpanded());
			}
		}
		else
		{
			Child->FilterNode(InFilter);

			if ( Child->GetVisibility() == ENodeVisibility::Visible )
			{
				if ( !InFilter.IsEmptyFilter() )
				{
					// The child is visible due to filtering so we must also be visible
					bShouldBeVisibleDueToChildFiltering = true;
				}

				// Expand the child after filtering if it wants to be expanded
				ParentCategory.Pin()->RequestItemExpanded(Child, Child->ShouldBeExpanded());
			}
		}
	}
}
