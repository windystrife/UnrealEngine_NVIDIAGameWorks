// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameMenuBuilderStyle.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "GameMenuBuilderModule.h"



TSharedPtr< FSlateStyleSet > FGameMenuBuilderStyle::SimpleStyleInstance = NULL;

void FGameMenuBuilderStyle::Initialize(const FString StyleName)
{
	if (FModuleManager::Get().IsModuleLoaded("GameMenuBuilder") == false)
	{
		FModuleManager::LoadModuleChecked<IGameMenuBuilderModule>("GameMenuBuilder");
	}
	if (!SimpleStyleInstance.IsValid())
	{
		SimpleStyleInstance = Create(StyleName);
		FSlateStyleRegistry::RegisterSlateStyle(*SimpleStyleInstance);
	}
}

void FGameMenuBuilderStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*SimpleStyleInstance);
	ensure(SimpleStyleInstance.IsUnique());
	SimpleStyleInstance.Reset();
}

FName FGameMenuBuilderStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("MenuPageStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( FPaths::ProjectContentDir() / "Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( FPaths::ProjectContentDir() / "Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( FPaths::ProjectContentDir() / "Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( FPaths::ProjectContentDir() / "Slate"/ RelativePath + TEXT(".ttf"), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( FPaths::ProjectContentDir() / "Slate"/ RelativePath + TEXT(".otf"), __VA_ARGS__ )

FString FGameMenuBuilderStyle::FontName("Fonts/Roboto-Light");
int32 FGameMenuBuilderStyle::FontSize = 42;

TSharedRef< FSlateStyleSet > FGameMenuBuilderStyle::Create(const FString StyleName)
{
	TSharedRef<FSlateGameResources> StyleRef = FSlateGameResources::New(GetStyleSetName(), *StyleName);
	
	FSlateStyleSet& Style = StyleRef.Get();

	// Fonts still need to be specified in code for now
	Style.Set("GameMenuStyle.MenuTextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT(*FontName, FontSize))
		.SetColorAndOpacity(FLinearColor::White)
		);
	Style.Set("GameMenuStyle.MenuHeaderTextStyle", FTextBlockStyle()
		.SetFont(TTF_FONT(*FontName, FontSize))
		.SetColorAndOpacity(FLinearColor::White)
		);

	return StyleRef;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FGameMenuBuilderStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FGameMenuBuilderStyle::Get()
{
	return *SimpleStyleInstance;
}
