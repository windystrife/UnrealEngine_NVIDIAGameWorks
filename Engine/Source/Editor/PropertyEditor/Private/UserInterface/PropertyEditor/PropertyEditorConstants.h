// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

struct FSlateBrush;

class PropertyEditorConstants
{
public:

	static const FName PropertyFontStyle;
	static const FName CategoryFontStyle;

	static const FName MD_Bitmask;
	static const FName MD_BitmaskEnum;
	static const FName MD_UseEnumValuesAsMaskValuesInEditor;

	static const FSlateBrush* GetOverlayBrush( const TSharedRef< class FPropertyEditor > PropertyEditor );
};
