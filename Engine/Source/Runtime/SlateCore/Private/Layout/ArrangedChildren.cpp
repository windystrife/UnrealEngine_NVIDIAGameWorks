// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Layout/ArrangedChildren.h"
#include "Widgets/SWidget.h"


/* FArrangedChildren interface
 *****************************************************************************/

void FArrangedChildren::AddWidget(const FArrangedWidget& InWidgetGeometry)
{
	AddWidget(InWidgetGeometry.Widget->GetVisibility(), InWidgetGeometry);
}
