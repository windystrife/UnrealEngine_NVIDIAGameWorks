// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FbxAutomationBuilderStyle.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(StyleSet->RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...) FSlateFontInfo(StyleSet->RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)

TSharedPtr< FSlateStyleSet > FFbxAutomationBuilderStyle::StyleSet = NULL;
TSharedPtr< class ISlateStyle > FFbxAutomationBuilderStyle::Get() { return StyleSet; }

FName FFbxAutomationBuilderStyle::GetStyleSetName()
{
	static FName FbxAutomationBuilderStyleName(TEXT("FbxAutomationBuilderStyle"));
	return FbxAutomationBuilderStyleName;
}

void FFbxAutomationBuilderStyle::Initialize()
{
	// Const icon sizes
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon9x19(9.0f, 19.0f);
	const FVector2D Icon10x10(10.0f, 10.0f);
	const FVector2D Icon12x12(12.0f, 12.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon22x22(22.0f, 22.0f);
	const FVector2D Icon24x24(24.0f, 24.0f);
	const FVector2D Icon27x31(27.0f, 31.0f);
	const FVector2D Icon26x26(26.0f, 26.0f);
	const FVector2D Icon32x32(32.0f, 32.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon75x82(75.0f, 82.0f);
	const FVector2D Icon360x32(360.0f, 32.0f);
	const FVector2D Icon171x39(171.0f, 39.0f);
	const FVector2D Icon170x50(170.0f, 50.0f);
	const FVector2D Icon267x140(170.0f, 50.0f);

	// Only register once
	if( StyleSet.IsValid() )
	{
		return;
	}

	StyleSet = MakeShareable( new FSlateStyleSet(FFbxAutomationBuilderStyle::GetStyleSetName()) );
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// FbxAutomationBuilder Icons
	{
		StyleSet->Set( "FbxAutomationBuilder.TabIcon", new IMAGE_BRUSH( "Icons/FbxAutomationBuilder/icon_FbxAutomationBuilder_tab_16x", Icon16x16 ) );
	}

	FSlateStyleRegistry::RegisterSlateStyle( *StyleSet.Get() );
};

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef TTF_CORE_FONT
#undef OTF_FONT
#undef OTF_CORE_FONT

void FFbxAutomationBuilderStyle::Shutdown()
{
	if( StyleSet.IsValid() )
	{
		FSlateStyleRegistry::UnRegisterSlateStyle( *StyleSet.Get() );
		ensure( StyleSet.IsUnique() );
		StyleSet.Reset();
	}
}
