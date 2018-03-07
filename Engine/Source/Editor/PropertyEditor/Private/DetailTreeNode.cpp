// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DetailTreeNode.h"
#include "DetailWidgetRow.h"

FNodeWidgets FDetailTreeNode::CreateNodeWidgets() const
{
	FDetailWidgetRow Row;
	GenerateStandaloneWidget(Row);

	FNodeWidgets Widgets;

	if(Row.HasAnyContent())
	{
		if (Row.HasColumns())
		{
			Widgets.NameWidget = Row.NameWidget.Widget;
			Widgets.ValueWidget = Row.ValueWidget.Widget;
		}
		else
		{
			Widgets.WholeRowWidget = Row.WholeRowWidget.Widget;
		}
	}

	return Widgets;
}

void FDetailTreeNode::GetChildren(TArray<TSharedRef<IDetailTreeNode>>& OutChildren)
{
	FDetailNodeList Children;
	GetChildren(Children);

	OutChildren.Reset(Children.Num());

	OutChildren.Append(Children);
}
