// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DragDrop/WidgetTemplateDragDropOp.h"
#include "WidgetTemplate.h"

#define LOCTEXT_NAMESPACE "UMG"

TSharedRef<FWidgetTemplateDragDropOp> FWidgetTemplateDragDropOp::New(const TSharedPtr<FWidgetTemplate>& InTemplate)
{
	TSharedRef<FWidgetTemplateDragDropOp> Operation = MakeShareable(new FWidgetTemplateDragDropOp());
	Operation->Template = InTemplate;
	Operation->DefaultHoverText = InTemplate->Name;
	Operation->CurrentHoverText = InTemplate->Name;
	Operation->Construct();

	return Operation;
}

#undef LOCTEXT_NAMESPACE
