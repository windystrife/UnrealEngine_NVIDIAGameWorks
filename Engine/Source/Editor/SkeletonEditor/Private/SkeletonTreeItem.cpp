// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletonTreeItem.h"
#include "SSkeletonTreeRow.h"

TSharedRef<ITableRow> FSkeletonTreeItem::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, const TAttribute<FText>& InFilterText)
{
	return SNew(SSkeletonTreeRow, InOwnerTable)
		.FilterText(InFilterText)
		.Item(SharedThis(this))
		.OnDraggingItem(this, &FSkeletonTreeItem::OnDragDetected);
}
