// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IPropertyTableCustomColumn;

class ColumnWidgetFactory : public TSharedFromThis< ColumnWidgetFactory >	
{
public:

	bool Supports( const TSharedRef < class IPropertyTableColumn >& Column );

	TSharedRef< class SColumnHeader > CreateColumnHeaderWidget( const TSharedRef < class IPropertyTableColumn >& Column, const TSharedRef< class IPropertyTableUtilities >& Utilities, const TSharedPtr< IPropertyTableCustomColumn >& Customization, const FName& Style );

};
