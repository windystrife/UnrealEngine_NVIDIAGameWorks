// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "IPropertyTableWidgetHandle.h"
#include "UserInterface/PropertyTable/SPropertyTable.h"

class FPropertyTableWidgetHandle: public IPropertyTableWidgetHandle{
public:
	FPropertyTableWidgetHandle(const TSharedRef<SPropertyTable>& InPropertyTableWidget)
		: PropertyTableWidget(InPropertyTableWidget) {}

	virtual ~FPropertyTableWidgetHandle() {}

	virtual void RequestRefresh() override
	{
		PropertyTableWidget->RequestRefresh();
	} 

	virtual TSharedRef<SWidget> GetWidget() override
	{
		return PropertyTableWidget;
	}

private:
	TSharedRef<SPropertyTable> PropertyTableWidget;
};
