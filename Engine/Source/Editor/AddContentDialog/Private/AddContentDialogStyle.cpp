// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AddContentDialogStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"


TSharedPtr< FSlateStyleSet > FAddContentDialogStyle::AddContentDialogStyleInstance = NULL;

void FAddContentDialogStyle::Initialize()
{
	if ( !AddContentDialogStyleInstance.IsValid() )
	{
		AddContentDialogStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle( *AddContentDialogStyleInstance );
	}
}

void FAddContentDialogStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle( *AddContentDialogStyleInstance );
	ensure( AddContentDialogStyleInstance.IsUnique() );
	AddContentDialogStyleInstance.Reset();
}

FName FAddContentDialogStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AddContentDialogStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )
#define TTF_CORE_FONT( RelativePath, ... ) FSlateFontInfo( FPaths::EngineContentDir()  / "Slate"/ RelativePath + TEXT(".ttf"), __VA_ARGS__ )

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef< FSlateStyleSet > FAddContentDialogStyle::Create()
{
	const FTextBlockStyle NormalText = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");

	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("AddContentDialogStyle"));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/AddContentDialog"));
	
	Style->Set("AddContentDialog.TabBackground", new BOX_BRUSH("TabBackground", 4 / 16.0f));
	Style->Set("AddContentDialog.Splitter", new IMAGE_BRUSH("Splitter", FVector2D(8, 8)));

	Style->Set("AddContentDialog.CategoryTab", FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
		.SetUncheckedImage(BOX_BRUSH("TabInactive", 4 / 16.0f))
		.SetUncheckedPressedImage(BOX_BRUSH("TabHovered", 4 / 16.0f))
		.SetUncheckedHoveredImage(BOX_BRUSH("TabHovered", 4 / 16.0f))
		.SetCheckedHoveredImage(BOX_BRUSH("TabActive", 4 / 16.0f))
		.SetCheckedPressedImage(BOX_BRUSH("TabActive", 4 / 16.0f))
		.SetCheckedImage(BOX_BRUSH("TabActive", 4 / 16.0f))		);

	Style->Set("AddContentDialog.BlankButton", FButtonStyle()
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateNoResource())
		.SetPressed(FSlateNoResource())		);

	Style->Set("AddContentDialog.LeftArrow", new IMAGE_BRUSH("ArrowLeft", FVector2D(25.0f, 42.0f)));
	Style->Set("AddContentDialog.RightArrow", new IMAGE_BRUSH("ArrowRight", FVector2D(25.0f, 42.0f)));

	Style->Set("AddContentDialog.HeadingText", FTextBlockStyle(NormalText)
		.SetFont(TTF_CORE_FONT("Fonts/Roboto-Regular", 14))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

	Style->Set("AddContentDialog.HeadingTextSmall", FTextBlockStyle(NormalText)
		.SetFont(TTF_CORE_FONT("Fonts/Roboto-Regular", 12))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

	Style->Set("AddContentDialog.RemoveButton", FButtonStyle()
		.SetNormal(IMAGE_BRUSH("Remove", FVector2D(16.0f, 16.0f)))
		.SetHovered(IMAGE_BRUSH("RemoveHovered", FVector2D(16.0f, 16.0f)))
		.SetPressed(IMAGE_BRUSH("RemoveHovered", FVector2D(16.0f, 16.0f)))		);

	Style->Set("AddContentDialog.BlueprintFeatureCategory", new IMAGE_BRUSH("BlueprintFeature", FVector2D(32.0f, 32.0f)));
	Style->Set("AddContentDialog.CodeFeatureCategory", new IMAGE_BRUSH("CodeFeature", FVector2D(32.0f, 32.0f)));
	Style->Set("AddContentDialog.ContentPackCategory", new IMAGE_BRUSH( "ContentPack", FVector2D(32.0f, 32.0f)));
	Style->Set("AddContentDialog.UnknownCategory", new FSlateNoResource());

	Style->Set("AddContentDialog.AddButton.TextStyle", FTextBlockStyle(NormalText)
		.SetFont(TTF_CORE_FONT("Fonts/Roboto-Bold", 11))
		.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f))
		.SetHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f))
		.SetShadowOffset(FVector2D(1, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f)));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
#undef TTF_CORE_FONT

void FAddContentDialogStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FAddContentDialogStyle::Get()
{
	return *AddContentDialogStyleInstance;
}
