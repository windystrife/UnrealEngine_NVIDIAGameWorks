// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "DetailWidgetRow.h"
#include "SDetailsViewBase.h"

class FCustomChildrenBuilder;
class FDetailCategoryImpl;
class FDetailItemNode;
class IDetailCustomNodeBuilder;

class FDetailCustomBuilderRow : public TSharedFromThis<FDetailCustomBuilderRow>
{
public:
	FDetailCustomBuilderRow( TSharedRef<IDetailCustomNodeBuilder> CustomBuilder );

	void Tick( float DeltaTime );
	bool RequiresTick() const;
	bool HasColumns() const;
	bool ShowOnlyChildren() const;
	void OnItemNodeInitialized( TSharedRef<FDetailItemNode> InTreeNode, TSharedRef<FDetailCategoryImpl> InParentCategory, const TAttribute<bool>& InIsParentEnabled );
	FName GetCustomBuilderName() const;
	void OnGenerateChildren( FDetailNodeList& OutChildren );
	bool IsInitiallyCollapsed() const;
	FDetailWidgetRow GetWidgetRow();
private:	
	/** Whether or not our parent is enabled */
	TAttribute<bool> IsParentEnabled;
	TSharedPtr<FDetailWidgetRow> HeaderRow;
	TSharedRef<class IDetailCustomNodeBuilder> CustomNodeBuilder;
	TSharedPtr<class FCustomChildrenBuilder> ChildrenBuilder;
	TWeakPtr<FDetailCategoryImpl> ParentCategory;
};

