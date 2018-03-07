// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorWidgetsStyle.h"

#include "SlateApplication.h"
#include "EditorStyleSet.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

TSharedPtr< FSlateStyleSet > FNiagaraEditorWidgetsStyle::NiagaraEditorWidgetsStyleInstance = NULL;

void FNiagaraEditorWidgetsStyle::Initialize()
{
	if (!NiagaraEditorWidgetsStyleInstance.IsValid())
	{
		NiagaraEditorWidgetsStyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*NiagaraEditorWidgetsStyleInstance);
	}
}

void FNiagaraEditorWidgetsStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*NiagaraEditorWidgetsStyleInstance);
	ensure(NiagaraEditorWidgetsStyleInstance.IsUnique());
	NiagaraEditorWidgetsStyleInstance.Reset();
}

FName FNiagaraEditorWidgetsStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("NiagaraEditorWidgetsStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

#define IMAGE_CORE_BRUSH( RelativePath, ... ) FSlateImageBrush( FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png") , __VA_ARGS__ )
#define BOX_CORE_BRUSH( RelativePath, ... ) FSlateBoxBrush( FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png"), __VA_ARGS__ )
#define TTF_CORE_FONT( RelativePath, ... ) FSlateFontInfo( FPaths::EngineContentDir()  / "Slate" / RelativePath + TEXT(".ttf"), __VA_ARGS__ )

const FVector2D Icon8x8(8.0f, 8.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef< FSlateStyleSet > FNiagaraEditorWidgetsStyle::Create()
{
	const FTextBlockStyle NormalText = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");
	const FEditableTextBoxStyle NormalEditableTextBox = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	const FSpinBoxStyle NormalSpinBox = FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("SpinBox");

	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("NiagaraEditorWidgetsStyle"));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/Niagara"));

	// Stack
	FSlateFontInfo StackGroupFont = TTF_CORE_FONT("Fonts/Roboto-Bold", 10);
	FTextBlockStyle StackGroupText = FTextBlockStyle(NormalText)
		.SetFont(StackGroupFont)
		.SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		.SetShadowOffset(FVector2D(0, 1))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
	Style->Set("NiagaraEditor.Stack.GroupText", StackGroupText);

	FSlateFontInfo StackDefaultFont = TTF_CORE_FONT("Fonts/Roboto-Regular", 10);
	FTextBlockStyle StackDefaultText = FTextBlockStyle(NormalText)
		.SetFont(StackDefaultFont);
	Style->Set("NiagaraEditor.Stack.DefaultText", StackDefaultText);

	FSlateFontInfo ParameterFont = TTF_CORE_FONT("Fonts/Roboto-Regular", 8);
	FTextBlockStyle ParameterText = FTextBlockStyle(NormalText)
		.SetFont(ParameterFont);
	Style->Set("NiagaraEditor.Stack.ParameterText", ParameterText);

	FSlateFontInfo ParameterCollectionFont = TTF_CORE_FONT("Fonts/Roboto-Regular", 9);
	FTextBlockStyle ParameterCollectionText = FTextBlockStyle(NormalText)
		.SetFont(ParameterCollectionFont);
	Style->Set("NiagaraEditor.Stack.ParameterCollectionText", ParameterCollectionText);

	FSlateFontInfo StackItemFont = TTF_CORE_FONT("Fonts/Roboto-Regular", 11);
	FTextBlockStyle StackItemText = FTextBlockStyle(NormalText)
		.SetFont(StackItemFont);
	Style->Set("NiagaraEditor.Stack.ItemText", StackItemText);

	Style->Set("NiagaraEditor.Stack.Group.BackgroundColor", FLinearColor(FColor(96, 96, 96)));

	Style->Set("NiagaraEditor.Stack.Item.BackgroundColor", FLinearColor(FColor(35, 35, 35)));

	Style->Set("NiagaraEditor.Stack.Item.ErrorBackgroundColor", FLinearColor(FColor(35, 0, 0)));

	Style->Set("NiagaraEditor.Stack.ItemHeaderFooter.BackgroundBrush", new FSlateColorBrush(FLinearColor(FColor(20, 20, 20))));

	Style->Set("NiagaraEditor.Stack.ForegroundColor", FLinearColor(FColor(191, 191, 191)));

	Style->Set("NiagaraEditor.Stack.Splitter", FSplitterStyle()
		.SetHandleNormalBrush(IMAGE_CORE_BRUSH("Common/SplitterHandleHighlight", Icon8x8, FLinearColor(.1f, .1f, .1f, 1.0f)))
		.SetHandleHighlightBrush(IMAGE_CORE_BRUSH("Common/SplitterHandleHighlight", Icon8x8, FLinearColor::White))
	);

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
#undef TTF_CORE_FONT
#undef BOX_CORE_BRUSH

void FNiagaraEditorWidgetsStyle::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FNiagaraEditorWidgetsStyle::Get()
{
	return *NiagaraEditorWidgetsStyleInstance;
}