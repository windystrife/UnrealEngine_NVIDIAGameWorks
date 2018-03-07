// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/TextBlock.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SInvalidationPanel.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UTextBlock

UTextBlock::UTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;
	bWrapWithInvalidationPanel = false;
	ShadowOffset = FVector2D(1.0f, 1.0f);
	ColorAndOpacity = FLinearColor::White;
	ShadowColorAndOpacity = FLinearColor::Transparent;

	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		Font = FSlateFontInfo(RobotoFontObj.Object, 24, FName("Bold"));
	}
}

void UTextBlock::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyTextBlock.Reset();
}

void UTextBlock::SetColorAndOpacity(FSlateColor InColorAndOpacity)
{
	ColorAndOpacity = InColorAndOpacity;
	if( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetColorAndOpacity( InColorAndOpacity );
	}
}

void UTextBlock::SetOpacity(float InOpacity)
{
	FLinearColor CurrentColor = ColorAndOpacity.GetSpecifiedColor();
	CurrentColor.A = InOpacity;
	
	SetColorAndOpacity(FSlateColor(CurrentColor));
}

void UTextBlock::SetShadowColorAndOpacity(FLinearColor InShadowColorAndOpacity)
{
	ShadowColorAndOpacity = InShadowColorAndOpacity;
	if( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetShadowColorAndOpacity(InShadowColorAndOpacity);
	}
}

void UTextBlock::SetShadowOffset(FVector2D InShadowOffset)
{
	ShadowOffset = InShadowOffset;
	if( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetShadowOffset(ShadowOffset);
	}
}

void UTextBlock::SetFont(FSlateFontInfo InFontInfo)
{
	Font = InFontInfo;
	if (MyTextBlock.IsValid())
	{
		MyTextBlock->SetFont(Font);
	}
}

void UTextBlock::SetJustification( ETextJustify::Type InJustification )
{
	Justification = InJustification;
	if ( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetJustification( Justification );
	}
}

void UTextBlock::SetMinDesiredWidth(float InMinDesiredWidth)
{
	MinDesiredWidth = InMinDesiredWidth;
	if (MyTextBlock.IsValid())
	{
		MyTextBlock->SetMinDesiredWidth(MinDesiredWidth);
	}
}

TSharedRef<SWidget> UTextBlock::RebuildWidget()
{
 	if (bWrapWithInvalidationPanel && !IsDesignTime())
 	{
 		TSharedPtr<SWidget> RetWidget = SNew(SInvalidationPanel)
 		[
 			SAssignNew(MyTextBlock, STextBlock)
 		];
 		return RetWidget.ToSharedRef();
 	}
 	else
	{
		MyTextBlock = SNew(STextBlock);
		return MyTextBlock.ToSharedRef();
	}
}

void UTextBlock::OnBindingChanged(const FName& Property)
{
	Super::OnBindingChanged(Property);

	if ( MyTextBlock.IsValid() )
	{
		static const FName TextProperty(TEXT("TextDelegate"));
		static const FName ColorAndOpacityProperty(TEXT("ColorAndOpacityDelegate"));
		static const FName ShadowColorAndOpacityProperty(TEXT("ShadowColorAndOpacityDelegate"));

		if ( Property == TextProperty )
		{
			TAttribute<FText> TextBinding = GetDisplayText();
			MyTextBlock->SetText(TextBinding);
		}
		else if ( Property == ColorAndOpacityProperty )
		{
			TAttribute<FSlateColor> ColorAndOpacityBinding = PROPERTY_BINDING(FSlateColor, ColorAndOpacity);
			MyTextBlock->SetColorAndOpacity(ColorAndOpacityBinding);
		}
		else if ( Property == ShadowColorAndOpacityProperty )
		{
			TAttribute<FLinearColor> ShadowColorAndOpacityBinding = PROPERTY_BINDING(FLinearColor, ShadowColorAndOpacity);
			MyTextBlock->SetShadowColorAndOpacity(ShadowColorAndOpacityBinding);
		}
	}
}

void UTextBlock::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FText> TextBinding = GetDisplayText();
	TAttribute<FSlateColor> ColorAndOpacityBinding = PROPERTY_BINDING(FSlateColor, ColorAndOpacity);
	TAttribute<FLinearColor> ShadowColorAndOpacityBinding = PROPERTY_BINDING(FLinearColor, ShadowColorAndOpacity);

	if ( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetText( TextBinding );
		MyTextBlock->SetFont( Font );
		MyTextBlock->SetColorAndOpacity( ColorAndOpacityBinding );
		MyTextBlock->SetShadowOffset( ShadowOffset );
		MyTextBlock->SetShadowColorAndOpacity( ShadowColorAndOpacityBinding );
		MyTextBlock->SetMinDesiredWidth( MinDesiredWidth );

		Super::SynchronizeTextLayoutProperties( *MyTextBlock );
	}
}

/// @cond DOXYGEN_WARNINGS

FText UTextBlock::GetText() const
{
	if (MyTextBlock.IsValid())
	{
		return MyTextBlock->GetText();
	}

	return Text;
}

/// @endcond

void UTextBlock::SetText(FText InText)
{
	Text = InText;
	TextDelegate.Unbind();
	if ( MyTextBlock.IsValid() )
	{
		TAttribute<FText> TextBinding = GetDisplayText();
		MyTextBlock->SetText(TextBinding);
	}
}

TAttribute<FText> UTextBlock::GetDisplayText()
{
	return PROPERTY_BINDING(FText, Text);
}

#if WITH_EDITOR

FString UTextBlock::GetLabelMetadata() const
{
	const int32 MaxSampleLength = 15;

	FString TextStr = Text.ToString();
	TextStr = TextStr.Len() <= MaxSampleLength ? TextStr : TextStr.Left(MaxSampleLength - 2) + TEXT("..");
	return TEXT(" \"") + TextStr + TEXT("\"");
}

void UTextBlock::HandleTextCommitted(const FText& InText, ETextCommit::Type CommitteType)
{
	//TODO UMG How will this migrate to the template?  Seems to me we need the previews to have access to their templates!
	//TODO UMG How will the user click the editable area?  There is an overlay blocking input so that other widgets don't get them.
	//     Need a way to recognize one particular widget and forward things to them!
}

const FText UTextBlock::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

void UTextBlock::OnCreationFromPalette()
{
	Text = LOCTEXT("TextBlockDefaultValue", "Text Block");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
