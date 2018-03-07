// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditorStyleSet.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"

TSharedPtr< ISlateStyle > FEditorStyle::Instance = NULL;

void FEditorStyle::ResetToDefault()
{
	SetStyle( FCoreStyle::Create("EditorStyle") );
}

void FEditorStyle::SetStyle( const TSharedRef< ISlateStyle >& NewStyle )
{
	if( Instance != NewStyle )
	{
		if ( Instance.IsValid() )
		{
			FSlateStyleRegistry::UnRegisterSlateStyle( *Instance.Get() );
		}

		Instance = NewStyle;

		if ( Instance.IsValid() )
		{
			FSlateStyleRegistry::RegisterSlateStyle( *Instance.Get() );
		}
		else
		{
			ResetToDefault();
		}
	}
}
