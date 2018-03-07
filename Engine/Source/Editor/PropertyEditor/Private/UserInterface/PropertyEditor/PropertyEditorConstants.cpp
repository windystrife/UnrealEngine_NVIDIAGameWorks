// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"
#include "EditorStyleSet.h"

const FName PropertyEditorConstants::PropertyFontStyle( TEXT("PropertyWindow.NormalFont") );
const FName PropertyEditorConstants::CategoryFontStyle( TEXT("PropertyWindow.BoldFont") );

const FName PropertyEditorConstants::MD_Bitmask( TEXT("Bitmask") );
const FName PropertyEditorConstants::MD_BitmaskEnum( TEXT("BitmaskEnum") );
const FName PropertyEditorConstants::MD_UseEnumValuesAsMaskValuesInEditor( TEXT("UseEnumValuesAsMaskValuesInEditor") );

const FSlateBrush* PropertyEditorConstants::GetOverlayBrush( const TSharedRef< class FPropertyEditor > PropertyEditor )
{
	return FEditorStyle::GetBrush( TEXT("PropertyWindow.NoOverlayColor") );
}
