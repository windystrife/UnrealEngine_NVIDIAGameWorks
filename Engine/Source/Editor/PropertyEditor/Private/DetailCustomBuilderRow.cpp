// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomBuilderRow.h"
#include "IDetailCustomNodeBuilder.h"
#include "DetailCategoryBuilderImpl.h"
#include "DetailItemNode.h"
#include "CustomChildBuilder.h"

FDetailCustomBuilderRow::FDetailCustomBuilderRow( TSharedRef<IDetailCustomNodeBuilder> CustomBuilder )
	: CustomNodeBuilder( CustomBuilder )
{
}

void FDetailCustomBuilderRow::Tick( float DeltaTime ) 
{
	return CustomNodeBuilder->Tick( DeltaTime );
}

bool FDetailCustomBuilderRow::RequiresTick() const
{
	return CustomNodeBuilder->RequiresTick();
}

bool FDetailCustomBuilderRow::HasColumns() const
{
	return HeaderRow->HasColumns();
}

bool FDetailCustomBuilderRow::ShowOnlyChildren() const
{
	return !HeaderRow->HasAnyContent();
}

void FDetailCustomBuilderRow::OnItemNodeInitialized( TSharedRef<FDetailItemNode> InTreeNode, TSharedRef<FDetailCategoryImpl> InParentCategory, const TAttribute<bool>& InIsParentEnabled )
{
	ParentCategory = InParentCategory;
	IsParentEnabled = InIsParentEnabled;

	const bool bUpdateFilteredNodes = true;
	// Set a delegate on the interface that it will call to rebuild this nodes children
	FSimpleDelegate OnRegenerateChildren = FSimpleDelegate::CreateSP( InTreeNode, &FDetailItemNode::GenerateChildren, bUpdateFilteredNodes );

	CustomNodeBuilder->SetOnRebuildChildren( OnRegenerateChildren );

	HeaderRow = MakeShareable( new FDetailWidgetRow );

	CustomNodeBuilder->GenerateHeaderRowContent( *HeaderRow );
}

FName FDetailCustomBuilderRow::GetCustomBuilderName() const
{
	return CustomNodeBuilder->GetName();
}

void FDetailCustomBuilderRow::OnGenerateChildren( FDetailNodeList& OutChildren )
{
	ChildrenBuilder = MakeShareable( new FCustomChildrenBuilder( ParentCategory.Pin().ToSharedRef() ) );

	CustomNodeBuilder->GenerateChildContent( *ChildrenBuilder );
		
	const TArray< FDetailLayoutCustomization >& ChildRows = ChildrenBuilder->GetChildCustomizations();

	for( int32 ChildIndex = 0; ChildIndex < ChildRows.Num(); ++ChildIndex )
	{
		TSharedRef<FDetailItemNode> ChildNodeItem = MakeShareable( new FDetailItemNode( ChildRows[ChildIndex], ParentCategory.Pin().ToSharedRef(), IsParentEnabled ) );
		ChildNodeItem->Initialize();
		OutChildren.Add( ChildNodeItem );
	}
}

bool FDetailCustomBuilderRow::IsInitiallyCollapsed() const
{
	return CustomNodeBuilder->InitiallyCollapsed();
}

FDetailWidgetRow FDetailCustomBuilderRow::GetWidgetRow()
{
	return *HeaderRow;
}
