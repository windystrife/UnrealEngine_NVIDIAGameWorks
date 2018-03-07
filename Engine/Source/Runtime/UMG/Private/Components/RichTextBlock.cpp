// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/RichTextBlock.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Components/RichTextBlockDecorator.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// URichTextBlock

URichTextBlock::URichTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		Font = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Regular"));
	}
	Color = FLinearColor::White;
}

void URichTextBlock::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyRichTextBlock.Reset();
}

TSharedRef<SWidget> URichTextBlock::RebuildWidget()
{
	//+ OnHyperlinkClicked = FSlateHyperlinkRun::FOnClick::CreateStatic(&RichTextHelper::OnBrowserLinkClicked, AsShared());
	//+ FHyperlinkDecorator::Create(TEXT("browser"), OnHyperlinkClicked))
	//+MakeShareable(new FDefaultRichTextDecorator(Font, Color));

	DefaultStyle.SetFont(Font);
	DefaultStyle.SetColorAndOpacity(Color);

	TArray< TSharedRef< class ITextDecorator > > CreatedDecorators;

	for ( URichTextBlockDecorator* Decorator : Decorators )
	{
		if ( Decorator )
		{
			CreatedDecorators.Add(Decorator->CreateDecorator(Font, Color));
		}
	}

	MyRichTextBlock =
		SNew(SRichTextBlock)
		.TextStyle(&DefaultStyle)
		.Decorators(CreatedDecorators);
	
	return MyRichTextBlock.ToSharedRef();
}

void URichTextBlock::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FText> TextBinding = PROPERTY_BINDING(FText, Text);

	MyRichTextBlock->SetText(TextBinding);

	Super::SynchronizeTextLayoutProperties( *MyRichTextBlock );
}

#if WITH_EDITOR

const FText URichTextBlock::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

void URichTextBlock::OnCreationFromPalette()
{
	Decorators.Add(NewObject<URichTextBlockDecorator>(this, NAME_None, RF_Transactional));
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
