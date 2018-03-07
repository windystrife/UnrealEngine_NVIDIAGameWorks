// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DesignerExtension.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UMG"

// Designer Extension

FDesignerExtension::FDesignerExtension()
	: ScopedTransaction(NULL)
{

}

void FDesignerExtension::Initialize(IUMGDesigner* InDesigner, UWidgetBlueprint* InBlueprint)
{
	Designer = InDesigner;
	Blueprint = InBlueprint;
}

FName FDesignerExtension::GetExtensionId() const
{
	return ExtensionId;
}

void FDesignerExtension::BeginTransaction(const FText& SessionName)
{
	if ( ScopedTransaction == NULL )
	{
		ScopedTransaction = new FScopedTransaction(SessionName);
	}

	for ( FWidgetReference& Selection : SelectionCache )
	{
		if ( Selection.IsValid() )
		{
			Selection.GetPreview()->Modify();
			Selection.GetTemplate()->Modify();
		}
	}
}

void FDesignerExtension::EndTransaction()
{
	if ( ScopedTransaction != NULL )
	{
		delete ScopedTransaction;
		ScopedTransaction = NULL;
	}
}

#undef LOCTEXT_NAMESPACE
