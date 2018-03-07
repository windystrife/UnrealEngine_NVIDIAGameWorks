// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Styling/SlateTypes.h"
#include "Brushes/SlateNoResource.h"
#include "Styling/StyleDefaults.h"


namespace SlateTypeDefs
{
	// Colors
	const static FLinearColor DefaultForeground = FLinearColor(0.72f, 0.72f, 0.72f, 1.f);
	const static FLinearColor InvertedForeground = FLinearColor(0,0,0);
}


FCheckBoxStyle::FCheckBoxStyle()
: CheckBoxType(ESlateCheckBoxType::CheckBox)
, UncheckedImage()
, UncheckedHoveredImage()
, UncheckedPressedImage()
, CheckedImage()
, CheckedHoveredImage()
, CheckedPressedImage()
, UndeterminedImage()
, UndeterminedHoveredImage()
, UndeterminedPressedImage()
, Padding(FMargin(2,0,0,0))
, ForegroundColor(FSlateColor::UseForeground())
, BorderBackgroundColor(FLinearColor::White)
{
}

const FName FCheckBoxStyle::TypeName( TEXT("FCheckBoxStyle") );

const FCheckBoxStyle& FCheckBoxStyle::GetDefault()
{
	static FCheckBoxStyle Default;
	return Default;
}

void FCheckBoxStyle::GetResources( TArray< const FSlateBrush* > & OutBrushes ) const
{
	OutBrushes.Add( &UncheckedImage );
	OutBrushes.Add( &UncheckedHoveredImage );
	OutBrushes.Add( &UncheckedPressedImage );
	OutBrushes.Add( &CheckedImage );
	OutBrushes.Add( &CheckedHoveredImage );
	OutBrushes.Add( &CheckedPressedImage );
	OutBrushes.Add( &UndeterminedImage );
	OutBrushes.Add( &UndeterminedHoveredImage );
	OutBrushes.Add( &UndeterminedPressedImage );
}

#if WITH_EDITORONLY_DATA
void FCheckBoxStyle::PostSerialize(const FArchive& Ar)
{
	if(Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FSLATESOUND_CONVERSION)
	{
		// eg, SoundCue'/Game/QA_Assets/Sound/TEST_Music_Ambient.TEST_Music_Ambient'
		CheckedSlateSound = FSlateSound::FromName_DEPRECATED(CheckedSound_DEPRECATED);
		UncheckedSlateSound = FSlateSound::FromName_DEPRECATED(UncheckedSound_DEPRECATED);
		HoveredSlateSound = FSlateSound::FromName_DEPRECATED(HoveredSound_DEPRECATED);
	}
}
#endif

FTextBlockStyle::FTextBlockStyle()
: Font()
, ColorAndOpacity()
, ShadowOffset(FVector2D::ZeroVector)
, ShadowColorAndOpacity(FLinearColor::Black)
, SelectedBackgroundColor(FSlateColor::UseForeground())
{
}

const FName FTextBlockStyle::TypeName( TEXT("FTextBlockStyle") );

void FTextBlockStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	OutBrushes.Add(&HighlightShape);
	OutBrushes.Add(&UnderlineBrush);
}

const FTextBlockStyle& FTextBlockStyle::GetDefault()
{
	static TSharedPtr< FTextBlockStyle > Default;
	if ( !Default.IsValid() )
	{
		Default = MakeShareable( new FTextBlockStyle() );
		Default->Font = FStyleDefaults::GetFontInfo();
	}

	return *Default.Get();
}

FButtonStyle::FButtonStyle()
: Normal()
, Hovered()
, Pressed()
, NormalPadding()
, PressedPadding()
{
	Disabled = FSlateNoResource();
}

const FName FButtonStyle::TypeName( TEXT("FButtonStyle") );

void FButtonStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &Normal );
	OutBrushes.Add( &Hovered );
	OutBrushes.Add( &Pressed );
	OutBrushes.Add( &Disabled );
}

const FButtonStyle& FButtonStyle::GetDefault()
{
	static FButtonStyle Default;
	return Default;
}

#if WITH_EDITORONLY_DATA
void FButtonStyle::PostSerialize(const FArchive& Ar)
{
	if(Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FSLATESOUND_CONVERSION)
	{
		// eg, SoundCue'/Game/QA_Assets/Sound/TEST_Music_Ambient.TEST_Music_Ambient'
		PressedSlateSound = FSlateSound::FromName_DEPRECATED(PressedSound_DEPRECATED);
		HoveredSlateSound = FSlateSound::FromName_DEPRECATED(HoveredSound_DEPRECATED);
	}
}
#endif

FComboButtonStyle::FComboButtonStyle()
	: MenuBorderPadding(FMargin(0.0f))
{
}

const FName FComboButtonStyle::TypeName( TEXT("FComboButtonStyle") );

void FComboButtonStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add(&MenuBorderBrush);
	OutBrushes.Add(&DownArrowImage);
	ButtonStyle.GetResources(OutBrushes);
}

const FComboButtonStyle& FComboButtonStyle::GetDefault()
{
	static FComboButtonStyle Default;
	return Default;
}

FComboBoxStyle::FComboBoxStyle()
{
	ComboButtonStyle.SetMenuBorderPadding(FMargin(1.0));
}

const FName FComboBoxStyle::TypeName( TEXT("FComboBoxStyle") );

void FComboBoxStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	ComboButtonStyle.GetResources(OutBrushes);
}

const FComboBoxStyle& FComboBoxStyle::GetDefault()
{
	static FComboBoxStyle Default;
	return Default;
}

#if WITH_EDITORONLY_DATA
void FComboBoxStyle::PostSerialize(const FArchive& Ar)
{
	if(Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FSLATESOUND_CONVERSION)
	{
		// eg, SoundCue'/Game/QA_Assets/Sound/TEST_Music_Ambient.TEST_Music_Ambient'
		PressedSlateSound = FSlateSound::FromName_DEPRECATED(PressedSound_DEPRECATED);
		SelectionChangeSlateSound = FSlateSound::FromName_DEPRECATED(SelectionChangeSound_DEPRECATED);
	}
}
#endif

FHyperlinkStyle::FHyperlinkStyle()
: UnderlineStyle()
, TextStyle()
, Padding()
{
}

const FName FHyperlinkStyle::TypeName( TEXT("FHyperlinkStyle") );

void FHyperlinkStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	UnderlineStyle.GetResources(OutBrushes);
	TextStyle.GetResources(OutBrushes);
}

const FHyperlinkStyle& FHyperlinkStyle::GetDefault()
{
	static FHyperlinkStyle Default;
	return Default;
}

FEditableTextStyle::FEditableTextStyle()
	: Font(FStyleDefaults::GetFontInfo(9))
	, ColorAndOpacity(FSlateColor::UseForeground())
	, BackgroundImageSelected()
	, CaretImage()
{
}

void FEditableTextStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &BackgroundImageSelected );
	OutBrushes.Add( &CaretImage );
}

const FName FEditableTextStyle::TypeName( TEXT("FEditableTextStyle") );

const FEditableTextStyle& FEditableTextStyle::GetDefault()
{
	static FEditableTextStyle Default;
	return Default;
}

FEditableTextBoxStyle::FEditableTextBoxStyle()
	: BackgroundImageNormal()
	, BackgroundImageHovered()
	, BackgroundImageFocused()
	, BackgroundImageReadOnly()
	, Padding(FMargin(4.0f, 2.0f))
	, Font(FStyleDefaults::GetFontInfo(9))
	, ForegroundColor(SlateTypeDefs::InvertedForeground)
	, BackgroundColor(FLinearColor::White)
	, ReadOnlyForegroundColor(SlateTypeDefs::DefaultForeground)
{
}

FEditableTextBoxStyle::~FEditableTextBoxStyle()
{
}

void FEditableTextBoxStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &BackgroundImageNormal );
	OutBrushes.Add( &BackgroundImageHovered );
	OutBrushes.Add( &BackgroundImageFocused );
	OutBrushes.Add( &BackgroundImageReadOnly );

	ScrollBarStyle.GetResources(OutBrushes);
}

const FName FEditableTextBoxStyle::TypeName( TEXT("FEditableTextBoxStyle") );

const FEditableTextBoxStyle& FEditableTextBoxStyle::GetDefault()
{
	static FEditableTextBoxStyle Default;
	return Default;
}


FInlineEditableTextBlockStyle::FInlineEditableTextBlockStyle()
{
}

void FInlineEditableTextBlockStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	EditableTextBoxStyle.GetResources(OutBrushes);
	TextStyle.GetResources(OutBrushes);
}

const FName FInlineEditableTextBlockStyle::TypeName( TEXT("FInlineEditableTextBlockStyle") );

const FInlineEditableTextBlockStyle& FInlineEditableTextBlockStyle::GetDefault()
{
	static FInlineEditableTextBlockStyle Default;
	return Default;
}


FProgressBarStyle::FProgressBarStyle()
{
}

void FProgressBarStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &BackgroundImage );
	OutBrushes.Add( &FillImage );
	OutBrushes.Add( &MarqueeImage );
}

const FName FProgressBarStyle::TypeName( TEXT("FProgressBarStyle") );

const FProgressBarStyle& FProgressBarStyle::GetDefault()
{
	static FProgressBarStyle Default;
	return Default;
}


FScrollBarStyle::FScrollBarStyle()
	: HorizontalBackgroundImage(FSlateNoResource())
	, VerticalBackgroundImage(FSlateNoResource())
	, VerticalTopSlotImage(FSlateNoResource())
	, HorizontalTopSlotImage(FSlateNoResource())
	, VerticalBottomSlotImage(FSlateNoResource())
	, HorizontalBottomSlotImage(FSlateNoResource())
	, NormalThumbImage(FSlateNoResource())
	, HoveredThumbImage(FSlateNoResource())
	, DraggedThumbImage(FSlateNoResource())
{
}

void FScrollBarStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &HorizontalBackgroundImage );
	OutBrushes.Add( &VerticalBackgroundImage );
	OutBrushes.Add( &VerticalTopSlotImage);
	OutBrushes.Add( &HorizontalTopSlotImage);
	OutBrushes.Add( &VerticalBottomSlotImage);
	OutBrushes.Add( &HorizontalBottomSlotImage);
	OutBrushes.Add( &NormalThumbImage );
	OutBrushes.Add( &HoveredThumbImage );
	OutBrushes.Add( &DraggedThumbImage );
}

const FName FScrollBarStyle::TypeName( TEXT("FScrollBarStyle") );

const FScrollBarStyle& FScrollBarStyle::GetDefault()
{
	static FScrollBarStyle Default;
	return Default;
}


FExpandableAreaStyle::FExpandableAreaStyle()
: RolloutAnimationSeconds(0.1f)
{
}

void FExpandableAreaStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &CollapsedImage );
	OutBrushes.Add( &ExpandedImage );
}

const FName FExpandableAreaStyle::TypeName( TEXT("FExpandableAreaStyle") );

const FExpandableAreaStyle& FExpandableAreaStyle::GetDefault()
{
	static FExpandableAreaStyle Default;
	return Default;
}


FSearchBoxStyle::FSearchBoxStyle()
	: bLeftAlignButtons(false)
{
}

FSearchBoxStyle& FSearchBoxStyle::SetTextBoxStyle( const FEditableTextBoxStyle& InTextBoxStyle )
{ 
	TextBoxStyle = InTextBoxStyle;
	if (!ActiveFontInfo.HasValidFont())
	{
		ActiveFontInfo = TextBoxStyle.Font;
	}
	return *this;
}

void FSearchBoxStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	TextBoxStyle.GetResources(OutBrushes);
	OutBrushes.Add( &UpArrowImage );
	OutBrushes.Add( &DownArrowImage );
	OutBrushes.Add( &GlassImage );
	OutBrushes.Add( &ClearImage );
}

const FName FSearchBoxStyle::TypeName( TEXT("FSearchBoxStyle") );

const FSearchBoxStyle& FSearchBoxStyle::GetDefault()
{
	static FSearchBoxStyle Default;
	return Default;
}

FSliderStyle::FSliderStyle()
	: BarThickness(2.0f)
{
}

void FSliderStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &NormalBarImage );
	OutBrushes.Add( &DisabledBarImage );
	OutBrushes.Add( &NormalThumbImage );
	OutBrushes.Add( &DisabledThumbImage );
}

const FName FSliderStyle::TypeName( TEXT("FSliderStyle") );

const FSliderStyle& FSliderStyle::GetDefault()
{
	static FSliderStyle Default;
	return Default;
}


FVolumeControlStyle::FVolumeControlStyle()
{
}

void FVolumeControlStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	SliderStyle.GetResources(OutBrushes);
	OutBrushes.Add( &HighVolumeImage );
	OutBrushes.Add( &MidVolumeImage );
	OutBrushes.Add( &LowVolumeImage );
	OutBrushes.Add( &NoVolumeImage );
	OutBrushes.Add( &MutedImage );
}

const FName FVolumeControlStyle::TypeName( TEXT("FVolumeControlStyle") );

const FVolumeControlStyle& FVolumeControlStyle::GetDefault()
{
	static FVolumeControlStyle Default;
	return Default;
}

FInlineTextImageStyle::FInlineTextImageStyle()
	: Image()
	, Baseline( 0 )
{
}

void FInlineTextImageStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &Image );
}

const FName FInlineTextImageStyle::TypeName( TEXT("FInlineTextImageStyle") );

const FInlineTextImageStyle& FInlineTextImageStyle::GetDefault()
{
	static FInlineTextImageStyle Default;
	return Default;
}

FSpinBoxStyle::FSpinBoxStyle()
	: ForegroundColor(FSlateColor::UseForeground())
	, TextPadding(FMargin(1.0f,2.0f))
{
}

void FSpinBoxStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &BackgroundBrush );
	OutBrushes.Add( &HoveredBackgroundBrush );
	OutBrushes.Add( &ActiveFillBrush );
	OutBrushes.Add( &InactiveFillBrush );
	OutBrushes.Add( &ArrowsImage );
}

const FName FSpinBoxStyle::TypeName( TEXT("FSpinBoxStyle") );

const FSpinBoxStyle& FSpinBoxStyle::GetDefault()
{
	static FSpinBoxStyle Default;
	return Default;
}


FSplitterStyle::FSplitterStyle()
{
}

void FSplitterStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &HandleNormalBrush );
	OutBrushes.Add( &HandleHighlightBrush );
}

const FName FSplitterStyle::TypeName( TEXT("FSplitterStyle") );

const FSplitterStyle& FSplitterStyle::GetDefault()
{
	static FSplitterStyle Default;
	return Default;
}


FTableRowStyle::FTableRowStyle()
	: TextColor(FSlateColor::UseForeground())
	, SelectedTextColor(FLinearColor::White)
{
}

void FTableRowStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &SelectorFocusedBrush );
	OutBrushes.Add( &ActiveHoveredBrush );
	OutBrushes.Add( &ActiveBrush );
	OutBrushes.Add( &InactiveHoveredBrush );
	OutBrushes.Add( &InactiveBrush );
	OutBrushes.Add( &EvenRowBackgroundHoveredBrush );
	OutBrushes.Add( &EvenRowBackgroundBrush );
	OutBrushes.Add( &OddRowBackgroundHoveredBrush );
	OutBrushes.Add( &OddRowBackgroundBrush );
	OutBrushes.Add( &DropIndicator_Above );
	OutBrushes.Add( &DropIndicator_Onto );
	OutBrushes.Add( &DropIndicator_Below );
}

const FName FTableRowStyle::TypeName( TEXT("FTableRowStyle") );

const FTableRowStyle& FTableRowStyle::GetDefault()
{
	static FTableRowStyle Default;
	return Default;
}


FTableColumnHeaderStyle::FTableColumnHeaderStyle()
{
}

void FTableColumnHeaderStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &SortPrimaryAscendingImage );
	OutBrushes.Add( &SortPrimaryDescendingImage );
	OutBrushes.Add( &SortSecondaryAscendingImage );
	OutBrushes.Add( &SortSecondaryDescendingImage );
	OutBrushes.Add( &NormalBrush );
	OutBrushes.Add( &HoveredBrush );
	OutBrushes.Add( &MenuDropdownImage );
	OutBrushes.Add( &MenuDropdownNormalBorderBrush );
	OutBrushes.Add( &MenuDropdownHoveredBorderBrush );
}

const FName FTableColumnHeaderStyle::TypeName( TEXT("FTableColumnHeaderStyle") );

const FTableColumnHeaderStyle& FTableColumnHeaderStyle::GetDefault()
{
	static FTableColumnHeaderStyle Default;
	return Default;
}


FHeaderRowStyle::FHeaderRowStyle()
{
}

void FHeaderRowStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	ColumnStyle.GetResources(OutBrushes);
	LastColumnStyle.GetResources(OutBrushes);
	ColumnSplitterStyle.GetResources(OutBrushes);
	OutBrushes.Add( &BackgroundBrush );
}

const FName FHeaderRowStyle::TypeName( TEXT("FHeaderRowStyle") );

const FHeaderRowStyle& FHeaderRowStyle::GetDefault()
{
	static FHeaderRowStyle Default;
	return Default;
}


FDockTabStyle::FDockTabStyle()
	: OverlapWidth(0.0f)
{
}

void FDockTabStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	CloseButtonStyle.GetResources(OutBrushes);
	OutBrushes.Add( &NormalBrush );
	OutBrushes.Add( &ActiveBrush );
	OutBrushes.Add( &ColorOverlayTabBrush );
	OutBrushes.Add( &ColorOverlayIconBrush );
	OutBrushes.Add( &ForegroundBrush );
	OutBrushes.Add( &HoveredBrush );
	OutBrushes.Add( &ContentAreaBrush );
	OutBrushes.Add( &TabWellBrush );
}

const FName FDockTabStyle::TypeName( TEXT("FDockTabStyle") );

const FDockTabStyle& FDockTabStyle::GetDefault()
{
	static FDockTabStyle Default;
	return Default;
}


FScrollBoxStyle::FScrollBoxStyle()
{
}

void FScrollBoxStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &TopShadowBrush );
	OutBrushes.Add( &BottomShadowBrush );
	OutBrushes.Add( &LeftShadowBrush );
	OutBrushes.Add( &RightShadowBrush );
}

const FName FScrollBoxStyle::TypeName( TEXT("FScrollBoxStyle") );

const FScrollBoxStyle& FScrollBoxStyle::GetDefault()
{
	static FScrollBoxStyle Default;
	return Default;
}


FScrollBorderStyle::FScrollBorderStyle()
{
}

void FScrollBorderStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &TopShadowBrush );
	OutBrushes.Add( &BottomShadowBrush );
}

const FName FScrollBorderStyle::TypeName( TEXT( "FScrollBorderStyle" ) );

const FScrollBorderStyle& FScrollBorderStyle::GetDefault()
{
	static FScrollBorderStyle Default;
	return Default;
}


FWindowStyle::FWindowStyle()
	: BackgroundColor( FLinearColor::White )
	, OutlineColor( FLinearColor(0.1f, 0.1f, 0.1f, 1.0f) )
{
}

void FWindowStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	MinimizeButtonStyle.GetResources(OutBrushes);
	MaximizeButtonStyle.GetResources(OutBrushes);
	RestoreButtonStyle.GetResources(OutBrushes);
	CloseButtonStyle.GetResources(OutBrushes);

	TitleTextStyle.GetResources(OutBrushes);

	OutBrushes.Add( &ActiveTitleBrush );
	OutBrushes.Add( &InactiveTitleBrush );
	OutBrushes.Add( &FlashTitleBrush );
	OutBrushes.Add( &BorderBrush );
	OutBrushes.Add( &OutlineBrush );
	OutBrushes.Add( &BackgroundBrush );
	OutBrushes.Add( &ChildBackgroundBrush );
}

const FName FWindowStyle::TypeName( TEXT("FWindowStyle") );

const FWindowStyle& FWindowStyle::GetDefault()
{
	static FWindowStyle Default;
	return Default;
}


