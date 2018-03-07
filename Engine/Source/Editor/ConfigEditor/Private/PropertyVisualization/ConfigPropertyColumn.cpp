// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PropertyVisualization/ConfigPropertyColumn.h"
#include "Widgets/Text/STextBlock.h"
#include "IPropertyTable.h"
#include "IPropertyTableCell.h"
#include "PropertyVisualization/ConfigPropertyCellPresenter.h"



#define LOCTEXT_NAMESPACE "ConfigEditor"


bool FConfigPropertyCustomColumn::Supports(const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities) const
{
	bool IsSupported = false;

	if (Column->GetDataSource()->IsValid())
	{
		TSharedPtr< FPropertyPath > PropertyPath = Column->GetDataSource()->AsPropertyPath();
		if (PropertyPath.IsValid() && PropertyPath->GetNumProperties() > 0)
		{
			const FPropertyInfo& PropertyInfo = PropertyPath->GetRootProperty();
			UProperty* Property = PropertyInfo.Property.Get();
			IsSupported = Property->GetFName() == TEXT("ExternalProperty");
		}
	}

	return IsSupported;
}

TSharedPtr< SWidget > FConfigPropertyCustomColumn::CreateColumnLabel(const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style) const
{
	if (Column->GetDataSource()->IsValid())
	{
		TSharedPtr< FPropertyPath > PropertyPath = Column->GetDataSource()->AsPropertyPath();
		if (PropertyPath.IsValid() && PropertyPath->GetNumProperties() > 0)
		{
			return SNew(STextBlock)
				.Text(EditProperty->GetDisplayNameText());
		}
	}
	return SNullWidget::NullWidget;
}


TSharedPtr< IPropertyTableCellPresenter > FConfigPropertyCustomColumn::CreateCellPresenter(const TSharedRef< IPropertyTableCell >& Cell, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style) const
{
	TSharedPtr< IPropertyHandle > PropertyHandle = Cell->GetPropertyHandle();
	if (PropertyHandle.IsValid())
	{
		return MakeShareable(new FConfigPropertyCellPresenter(PropertyHandle));
	}

	return nullptr;
}


#undef LOCTEXT_NAMESPACE
