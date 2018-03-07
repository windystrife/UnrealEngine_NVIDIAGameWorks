// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WidgetReference.h"
#include "WidgetBlueprintEditor.h"

#define LOCTEXT_NAMESPACE "UMG"

FWidgetHandle::FWidgetHandle(UWidget* InWidget)
	: Widget(InWidget)
{

}

// FWidgetReference

FWidgetReference::FWidgetReference()
	: WidgetEditor()
	, TemplateHandle()
{

}

FWidgetReference::FWidgetReference(TSharedPtr<FWidgetBlueprintEditor> InWidgetEditor, TSharedPtr< FWidgetHandle > InTemplateHandle)
	: WidgetEditor(InWidgetEditor)
	, TemplateHandle(InTemplateHandle)
{
	
}

bool FWidgetReference::IsValid() const
{
	if ( TemplateHandle.IsValid() )
	{
		return TemplateHandle->Widget.Get() != nullptr && GetPreview();
	}

	return false;
}

UWidget* FWidgetReference::GetTemplate() const
{
	if ( TemplateHandle.IsValid() )
	{
		return TemplateHandle->Widget.Get();
	}

	return nullptr;
}

UWidget* FWidgetReference::GetPreview() const
{
	if ( WidgetEditor.IsValid() && TemplateHandle.IsValid() )
	{
		UUserWidget* PreviewRoot = WidgetEditor.Pin()->GetPreview();

		if ( PreviewRoot )
		{
			if ( UWidget* TemplateWidget = TemplateHandle->Widget.Get() )
			{
				UWidget* PreviewWidget = PreviewRoot->GetWidgetFromName(TemplateWidget->GetFName());
				return PreviewWidget;
			}
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
