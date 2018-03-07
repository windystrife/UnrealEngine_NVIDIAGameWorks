// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Colors/SColorThemes.h"
#include "Misc/ConfigCacheIni.h"
#include "Layout/ArrangedChildren.h"
#include "Application/SlateWindowHelper.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Colors/SColorBlock.h"


void FColorDragDrop::OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent )
{
	HideTrash.ExecuteIfBound();

	if (OriginBar.Pin().IsValid() && !bSetForDeletion)
	{
		OriginBar.Pin()->AddNewColorBlock(Color, OriginBarPosition);
	}
	
	FDragDropOperation::OnDrop( bDropWasHandled, MouseEvent );
}

void FColorDragDrop::OnDragged( const class FDragDropEvent& DragDropEvent )
{
	if (CursorDecoratorWindow.IsValid())
	{
		CursorDecoratorWindow->MoveWindowTo( DragDropEvent.GetScreenSpacePosition() - BlockSize * 0.5f );
	}
}

TSharedRef<FColorDragDrop> FColorDragDrop::New(FLinearColor InColor, bool bSRGB, bool bUseAlpha,
	FSimpleDelegate TrashShowCallback, FSimpleDelegate TrashHideCallback,
	TSharedPtr<SThemeColorBlocksBar> Origin, int32 OriginPosition)
{
	TSharedRef<FColorDragDrop> Operation = MakeShareable(new FColorDragDrop);

	Operation->Color = InColor;
	Operation->bUseSRGB = bSRGB;
	Operation->bUseAlpha = bUseAlpha;
	Operation->OriginBar = Origin;
	Operation->OriginBarPosition = OriginPosition;
	Operation->ShowTrash = TrashShowCallback;
	Operation->HideTrash = TrashHideCallback;
	Operation->bSetForDeletion = false;
	Operation->BlockSize = FVector2D(32, 32);

	Operation->ShowTrash.ExecuteIfBound();

	Operation->Construct();

	return Operation;
}

TSharedPtr<SWidget> FColorDragDrop::GetDefaultDecorator() const
{
	const bool bIgnoreAlpha = !bUseAlpha;
	const bool bShowBackgroundForAlpha = bUseAlpha;

	return SNew(SBorder) .Cursor(EMouseCursor::GrabHandClosed)
		[
			SNew(SColorBlock) 
				.Color(Color)
				.ColorIsHSV(true) 
				.IgnoreAlpha(bIgnoreAlpha) 
				.ShowBackgroundForAlpha(bShowBackgroundForAlpha) 
				.UseSRGB(bUseSRGB)
		];
}


FColorTheme::FColorTheme( const FString& InName, const TArray< TSharedPtr<FLinearColor> >& InColors )
	: Name(InName)
	, Colors(InColors)
	, RefreshEvent()
{ }


void FColorTheme::InsertNewColor( TSharedPtr<FLinearColor> InColor, int32 InsertPosition )
{
	Colors.Insert(InColor, InsertPosition);
	RefreshEvent.Broadcast();
}


int32 FColorTheme::FindApproxColor( const FLinearColor& InColor, float Tolerance ) const
{
	for (int32 ColorIndex = 0; ColorIndex < Colors.Num(); ++ColorIndex)
	{
		if (Colors[ColorIndex]->Equals(InColor, Tolerance))
		{
			return ColorIndex;
		}
	}

	return INDEX_NONE;
}


void FColorTheme::RemoveAll()
{
	Colors.Empty();
	RefreshEvent.Broadcast();
}


void FColorTheme::RemoveColor( int32 ColorIndex )
{
	Colors.RemoveAt(ColorIndex);
	RefreshEvent.Broadcast();
}


int32 FColorTheme::RemoveColor( const TSharedPtr<FLinearColor> InColor )
{
	const int32 Position = Colors.Find(InColor);
	if (Position != INDEX_NONE)
	{
		RemoveColor(Position);
	}

	return Position;
}


void SColorTrash::Construct( const FArguments& InArgs )
{
	bBorderActivated = false;

	this->ChildSlot
	[
		SNew(SBorder)
			.ToolTipText(NSLOCTEXT("ColorTrashWidget", "MouseOverToolTip", "Delete Color"))
			.BorderImage(this, &SColorTrash::GetBorderStyle)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot() .HAlign(HAlign_Center) .FillWidth(1)
			[
				SNew(SImage) .Image(InArgs._UsesSmallIcon.Get() ?
					FCoreStyle::Get().GetBrush("TrashCan_Small") :
					FCoreStyle::Get().GetBrush("TrashCan"))
			]
		]
	];
}
	
/**
* Called during drag and drop when the drag enters a widget.
*
* @param MyGeometry      The geometry of the widget receiving the event.
* @param DragDropEvent   The drag and drop event.
*
* @return A reply that indicated whether this event was handled.
*/
void SColorTrash::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FColorDragDrop>().IsValid() )
	{
		bBorderActivated = true;
	}
}

/**
* Called during drag and drop when the drag leaves a widget.
*
* @param DragDropEvent   The drag and drop event.
*
* @return A reply that indicated whether this event was handled.
*/
void SColorTrash::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FColorDragDrop>().IsValid() )
	{
		bBorderActivated = false;
	}
}

/**
* Called when the user is dropping something onto a widget; terminates drag and drop.
*
* @param MyGeometry      The geometry of the widget receiving the event.
* @param DragDropEvent   The drag and drop event.
*
* @return A reply that indicated whether this event was handled.
*/
FReply SColorTrash::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FColorDragDrop> DragDropContent = DragDropEvent.GetOperationAs<FColorDragDrop>();
	if ( DragDropContent.IsValid() )
	{
		DragDropContent->bSetForDeletion = true;

		bBorderActivated = false;

		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

const FSlateBrush* SColorTrash::GetBorderStyle() const
{
	return (bBorderActivated) ?
		FCoreStyle::Get().GetBrush("FocusRectangle") :
		FStyleDefaults::GetNoBrush();
}



/**
* Construct the widget
*
* @param InArgs   Declaration from which to construct the widget.
*/
void SThemeColorBlock::Construct(const FArguments& InArgs )
{
	ColorPtr = InArgs._Color.Get();
	OnSelectColor = InArgs._OnSelectColor;
	ParentPtr = InArgs._Parent.Get();
	ShowTrashCallback = InArgs._ShowTrashCallback;
	HideTrashCallback = InArgs._HideTrashCallback;
	bUseSRGB = InArgs._UseSRGB;
	bUseAlpha = InArgs._UseAlpha;

	DistanceDragged = 0;

	const FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 9 );

	TSharedPtr<SToolTip> ColorTooltip =
		SNew(SToolTip)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight() .Padding(2)
				[
					SNew(SBox)
					.WidthOverride(110)
					.HeightOverride(110)
					[
						SNew(SColorBlock)
							.Color(this, &SThemeColorBlock::GetColor)
							.ColorIsHSV(true)
							.IgnoreAlpha(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SThemeColorBlock::OnReadIgnoreAlpha)))
							.ShowBackgroundForAlpha(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SThemeColorBlock::OnReadShowBackgroundForAlpha)))
							.UseSRGB(bUseSRGB)
					]
				]
				+SVerticalBox::Slot().AutoHeight() .Padding(2)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot() .FillWidth(1)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight() .Padding(3)
						[
							SNew(STextBlock) .Font(SmallLayoutFont) .Text(this, &SThemeColorBlock::GetRedText)
						]
						+SVerticalBox::Slot().AutoHeight() .Padding(3)
						[
							SNew(STextBlock) .Font(SmallLayoutFont) .Text(this, &SThemeColorBlock::GetGreenText)
						]
						+SVerticalBox::Slot().AutoHeight() .Padding(3)
						[
							SNew(STextBlock) .Font(SmallLayoutFont) .Text(this, &SThemeColorBlock::GetBlueText)
						]
					]
					+SHorizontalBox::Slot() .FillWidth(1)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight() .Padding(3)
						[
							SNew(STextBlock) .Font(SmallLayoutFont) .Text(this, &SThemeColorBlock::GetHueText)
						]
						+SVerticalBox::Slot().AutoHeight() .Padding(3)
						[
							SNew(STextBlock) .Font(SmallLayoutFont) .Text(this, &SThemeColorBlock::GetSaturationText)
						]
						+SVerticalBox::Slot().AutoHeight() .Padding(3)
						[
							SNew(STextBlock) .Font(SmallLayoutFont) .Text(this, &SThemeColorBlock::GetValueText)
						]
					]
				]
				+SVerticalBox::Slot().AutoHeight() .Padding(2)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock) .Font(SmallLayoutFont) .Text(this, &SThemeColorBlock::GetAlphaText) .Visibility(SharedThis(this), &SThemeColorBlock::OnGetAlphaVisibility)
				]
			]
		];

	this->ChildSlot
	[
		SNew(SBorder)
			.BorderImage(this, &SThemeColorBlock::HandleBorderImage)
			.BorderBackgroundColor(this, &SThemeColorBlock::HandleBorderColor)
			.Padding(FMargin(1.0f))
			.ToolTip(ColorTooltip)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SColorBlock)
					.Color(this, &SThemeColorBlock::GetColor)
					.ColorIsHSV(true)
					.IgnoreAlpha(true)
					.ShowBackgroundForAlpha(false)
					.UseSRGB(bUseSRGB)
				]
				+ SHorizontalBox::Slot()
				[
					SNew(SColorBlock)
					.Color(this, &SThemeColorBlock::GetColor)
					.ColorIsHSV(true)
					.IgnoreAlpha(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SThemeColorBlock::OnReadIgnoreAlpha)))
					.ShowBackgroundForAlpha(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SThemeColorBlock::OnReadShowBackgroundForAlpha)))
					.UseSRGB(bUseSRGB)
				]
			]
	];
}

FReply SThemeColorBlock::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		DistanceDragged = 0;
		return FReply::Handled().DetectDrag( SharedThis(this), EKeys::LeftMouseButton ).CaptureMouse(SharedThis(this));
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SThemeColorBlock::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()) )
	{
		check(ColorPtr.IsValid());
		OnSelectColor.ExecuteIfBound(*ColorPtr.Pin());
		
		return FReply::Handled().ReleaseMouseCapture();
			
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SThemeColorBlock::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) && ParentPtr.IsValid() )
	{
		FLinearColor ColorToGrab = *ColorPtr.Pin();

		int32 Position = ParentPtr.Pin()->RemoveColorBlock(ColorPtr.Pin());
			
		ParentPtr.Pin()->SetPlaceholderGrabOffset(MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ));
		
		TSharedRef<FColorDragDrop> Operation = FColorDragDrop::New(ColorToGrab, bUseSRGB.Get(), bUseAlpha.Get(), 
			ShowTrashCallback, HideTrashCallback, ParentPtr.Pin(), Position);
		return FReply::Handled().BeginDragDrop(Operation);
	}
	
	return FReply::Unhandled();
}

FLinearColor SThemeColorBlock::GetColor() const
{
	return ColorPtr.IsValid() ? *ColorPtr.Pin() : FLinearColor(ForceInit);
}

FSlateColor SThemeColorBlock::HandleBorderColor() const
{
	if (IsHovered())
	{
		return FSlateColor(FLinearColor::White);
	}
	
	return FSlateColor(GetColor().HSVToLinearRGB().ToFColor(bUseSRGB.Get()));
}

const FSlateBrush* SThemeColorBlock::HandleBorderImage() const
{
	return IsHovered() ?
		FCoreStyle::Get().GetBrush("FocusRectangle") :
		FCoreStyle::Get().GetBrush("GenericWhiteBox");
}

#define LOCTEXT_NAMESPACE "ThemeColorBlock"
FText SThemeColorBlock::GetRedText() const { return ColorPtr.IsValid() ? FormatToolTipText(LOCTEXT("Red", "R"), ColorPtr.Pin()->HSVToLinearRGB().R) : FText::GetEmpty(); }
FText SThemeColorBlock::GetGreenText() const { return ColorPtr.IsValid() ? FormatToolTipText(LOCTEXT("Green", "G"), ColorPtr.Pin()->HSVToLinearRGB().G) : FText::GetEmpty(); }
FText SThemeColorBlock::GetBlueText() const { return ColorPtr.IsValid() ? FormatToolTipText(LOCTEXT("Blue", "B"), ColorPtr.Pin()->HSVToLinearRGB().B) : FText::GetEmpty(); }
FText SThemeColorBlock::GetAlphaText() const { return ColorPtr.IsValid() ? FormatToolTipText(LOCTEXT("Alpha", "A"), ColorPtr.Pin()->HSVToLinearRGB().A) : FText::GetEmpty(); }
FText SThemeColorBlock::GetHueText() const { return ColorPtr.IsValid() ? FormatToolTipText(LOCTEXT("Hue", "H"), FMath::RoundToFloat(ColorPtr.Pin()->R)) : FText::GetEmpty(); }	// Rounded to let the value match the value in the Hue spinbox in the color picker
FText SThemeColorBlock::GetSaturationText() const { return ColorPtr.IsValid() ? FormatToolTipText(LOCTEXT("Saturation", "S"), ColorPtr.Pin()->G) : FText::GetEmpty(); }
FText SThemeColorBlock::GetValueText() const { return ColorPtr.IsValid() ? FormatToolTipText(LOCTEXT("Value", "V"), ColorPtr.Pin()->B) : FText::GetEmpty(); }

FText SThemeColorBlock::FormatToolTipText(const FText& ColorIdentifier, float Value) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Identifier"), ColorIdentifier);

	if (Value >= 0.f)
	{
		static const float LogToLog10 = 1.f / FMath::Loge(10.f);
		int32 PreRadixDigits = FMath::Max(0, int32(FMath::Loge(Value + KINDA_SMALL_NUMBER) * LogToLog10));

		int32 Precision = FMath::Max(0, 2 - PreRadixDigits);

		FNumberFormattingOptions FormatRules;
		FormatRules.MinimumFractionalDigits = Precision;

		Args.Add(TEXT("Value"), FText::AsNumber(Value, &FormatRules));
	}
	else
	{
		Args.Add(TEXT("Value"), FText::GetEmpty());
	}

	return FText::Format(LOCTEXT("ToolTipFormat", "{Identifier}: {Value}"), Args);
}
#undef LOCTEXT_NAMESPACE

bool SThemeColorBlock::OnReadIgnoreAlpha() const
{
	return !bUseAlpha.Get();
}

bool SThemeColorBlock::OnReadShowBackgroundForAlpha() const
{
	return bUseAlpha.Get();
}

EVisibility SThemeColorBlock::OnGetAlphaVisibility() const
{
	return bUseAlpha.Get() ? EVisibility::Visible : EVisibility::Collapsed;
}

SThemeColorBlocksBar::SThemeColorBlocksBar()
: ColorBlocks()
{
}


/**
* Panels arrange their children in a space described by the AllottedGeometry parameter. The results of the arrangement
* should be returned by appending a FArrangedWidget pair for every child widget.
*
* @param AllottedGeometry    The geometry allotted for this widget by its parent.
* @param ArrangedChildren    The array to which to add the WidgetGeometries that represent the arranged children.
*/
void SThemeColorBlocksBar::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	bool bPlaceHolderExists = NewColorPlaceholder.IsValid();

	const int32 NumColorBlocks = bPlaceHolderExists ? ColorBlocks.Num() + 1 : ColorBlocks.Num();

	if (NumColorBlocks > 0)
	{
		const float ChildPadding = 3.0f;
		const FVector2D ChildSize = FVector2D(AllottedGeometry.GetLocalSize().X / NumColorBlocks - ChildPadding, AllottedGeometry.GetLocalSize().Y);

		const float CurrentDragCenter = PlaceholderBlockOffset + ChildSize.X * 0.5f;

		float XOffset = 0;
		for (int32 i = 0; i < NumColorBlocks; ++i)
		{
			if (bPlaceHolderExists && XOffset <= CurrentDragCenter && CurrentDragCenter < (XOffset + ChildSize.X))
			{
				XOffset += ChildSize.X;
				XOffset += ChildPadding;
			}

			if (i < ColorBlocks.Num())
			{
				ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ColorBlocks[i], FVector2D(XOffset, 0), ChildSize));
				XOffset += ChildSize.X;
				XOffset += ChildPadding;
			}
		}

		if (bPlaceHolderExists && NewColorBlockPlaceholder.IsValid())
		{
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(NewColorBlockPlaceholder.ToSharedRef(), FVector2D(PlaceholderBlockOffset, 0), ChildSize));
		}
	}
	else if (EmptyHintTextBlock.IsValid())
	{
		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(EmptyHintTextBlock.ToSharedRef(), FVector2D::ZeroVector, AllottedGeometry.Size));
	}
}

/**
* A Panel's desired size in the space required to arrange of its children on the screen while respecting all of
* the children's desired sizes and any layout-related options specified by the user. See StackPanel for an example.
*
* @return The desired size.
*/
FVector2D SThemeColorBlocksBar::ComputeDesiredSize( float ) const
{
	return FVector2D(64, 16);
}

/**
* All widgets must provide a way to access their children in a layout-agnostic way.
* Panels store their children in Slots, which creates a dilemma. Most panels
* can store their children in a TPanelChildren<Slot>, where the Slot class
* provides layout information about the child it stores. In that case
* GetChildren should simply return the TPanelChildren<Slot>.
*/
FChildren* SThemeColorBlocksBar::GetChildren()
{
	return &ColorBlocks;
}

void SThemeColorBlocksBar::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FColorDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FColorDragDrop>();
	if ( DragDropOperation.IsValid() )
	{
		NewColorPlaceholder = MakeShareable(new FLinearColor(DragDropOperation->Color));
		NewColorBlockPlaceholder =
			SNew(SThemeColorBlock) .Color(NewColorPlaceholder) .UseSRGB(bUseSRGB) .UseAlpha(bUseAlpha);
	}
}

void SThemeColorBlocksBar::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FColorDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FColorDragDrop>();
	if ( DragDropOperation.IsValid() )
	{
		DragDropOperation->SetDecoratorVisibility(true);

		DestroyPlaceholders();
	}
}

/**
* Called during drag and drop when the the mouse is being dragged over a widget.
*
* @param MyGeometry      The geometry of the widget receiving the event.
* @param DragDropEvent   The drag and drop event.
*
* @return A reply that indicated whether this event was handled.
*/
FReply SThemeColorBlocksBar::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FColorDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FColorDragDrop>();
	if ( DragDropOperation.IsValid() )
	{
		const float ChildSizeX = MyGeometry.Size.X / (ColorBlocks.Num() + 1);
		const float GrabOffsetX = PlaceholderInitialGrabOffset.X;
		PlaceholderBlockOffset = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition()).X -
			((GrabOffsetX == 0.f) ? ChildSizeX * 0.5f : GrabOffsetX);

		DragDropOperation->SetDecoratorVisibility(false);
			
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

/**
* Called when the user is dropping something onto a widget; terminates drag and drop.
*
* @param MyGeometry      The geometry of the widget receiving the event.
* @param DragDropEvent   The drag and drop event.
*
* @return A reply that indicated whether this event was handled.
*/
FReply SThemeColorBlocksBar::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FColorDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FColorDragDrop>();
	if ( DragDropOperation.IsValid() )
	{
		const float ChildSizeX = MyGeometry.Size.X / (ColorBlocks.Num() + 1);
		const float CurrentDragCenter = PlaceholderBlockOffset + ChildSizeX * 0.5f;
		const int32 LocID = FMath::Clamp(int32(CurrentDragCenter / ChildSizeX), 0, ColorBlocks.Num());

		AddNewColorBlock(DragDropOperation->Color, LocID);

		DragDropOperation->bSetForDeletion = true;

		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

void SThemeColorBlocksBar::AddNewColorBlock(FLinearColor Color, int32 InsertPosition)
{
	// Force the alpha component to 1 when we don't care about the alpha, it'll allow the color to work with alpha-important widgets later
	if( !bUseAlpha.Get() )
	{
		Color.A = 1.0f;
	}
	ColorTheme.Get()->InsertNewColor(MakeShareable(new FLinearColor(Color)), InsertPosition);

	DestroyPlaceholders();

	SColorThemesViewer::SaveColorThemesToIni();
}

int32 SThemeColorBlocksBar::RemoveColorBlock(TSharedPtr<FLinearColor> ColorToRemove)
{
	int32 Position = ColorTheme.Get()->RemoveColor(ColorToRemove);
	
	SColorThemesViewer::SaveColorThemesToIni();

	return Position;
}

void SThemeColorBlocksBar::DestroyPlaceholders()
{
	NewColorBlockPlaceholder.Reset();
	NewColorPlaceholder.Reset();
	PlaceholderBlockOffset = 0.f;
	PlaceholderInitialGrabOffset = FVector2D(ForceInit);
}

void SThemeColorBlocksBar::RemoveRefreshCallback()
{
	ColorTheme.Get()->OnRefresh().Remove(RefreshCallbackHandle);
}

void SThemeColorBlocksBar::AddRefreshCallback()
{
	RefreshCallbackHandle = ColorTheme.Get()->OnRefresh().Add(RefreshCallback);
}

void SThemeColorBlocksBar::Refresh()
{
	DestroyPlaceholders();
	ColorBlocks.Empty();

	check(ColorTheme.Get().IsValid());

	const TArray< TSharedPtr<FLinearColor> >& Theme = ColorTheme.Get()->GetColors();
	for (int32 i = 0; i < Theme.Num(); ++i)
	{
		ColorBlocks.Add(
			SNew(SThemeColorBlock)
				.Color(Theme[i])
				.OnSelectColor(OnSelectColor)
				.Parent(SharedThis(this))
				.ShowTrashCallback(ShowTrashCallback)
				.HideTrashCallback(HideTrashCallback)
				.UseSRGB(bUseSRGB)
				.UseAlpha(bUseAlpha)
			);
	}
}

void SThemeColorBlocksBar::SetPlaceholderGrabOffset(FVector2D GrabOffset)
{
	PlaceholderInitialGrabOffset = GrabOffset;
}

/**
* Most panels do not create widgets as part of their implementation, so
* they do not need to implement a Construct()
*/
void SThemeColorBlocksBar::Construct(const FArguments& InArgs)
{
	ColorTheme = InArgs._ColorTheme;
	OnSelectColor = InArgs._OnSelectColor;
	ShowTrashCallback = InArgs._ShowTrashCallback;
	HideTrashCallback = InArgs._HideTrashCallback;
	bUseSRGB = InArgs._UseSRGB;
	bUseAlpha = InArgs._UseAlpha;
	
	RefreshCallback = FSimpleDelegate::CreateSP(this, &SThemeColorBlocksBar::Refresh);
	AddRefreshCallback();

	DestroyPlaceholders();

	if (!InArgs._EmptyText.IsEmpty())
	{
		EmptyHintTextBlock = SNew(SBorder)
			.Padding(1.0f)
			.Content()
			[
				SNew(STextBlock)
					.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8))
					.Text(InArgs._EmptyText)
			];
	}

	Refresh();
}




void SColorThemeBar::Construct(const FArguments& InArgs)
{
	ColorTheme = InArgs._ColorTheme.Get();
	OnCurrentThemeChanged = InArgs._OnCurrentThemeChanged;
	ShowTrashCallback = InArgs._ShowTrashCallback;
	HideTrashCallback = InArgs._HideTrashCallback;
	bUseSRGB = InArgs._UseSRGB;
	bUseAlpha = InArgs._UseAlpha;

	this->ChildSlot
	[
		SNew(SBox)
			.Padding(5.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(ThemeNameText, STextBlock)
							.Text(this, &SColorThemeBar::GetThemeName)
							.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10))
					]

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SThemeColorBlocksBar)
							.ColorTheme(InArgs._ColorTheme)
							.ShowTrashCallback(ShowTrashCallback)
							.HideTrashCallback(HideTrashCallback)
							.EmptyText(NSLOCTEXT("ColorThemesViewer", "NoColorsText", "No Colors Added Yet"))
							.UseSRGB(bUseSRGB)
							.UseAlpha(bUseAlpha)
					]
			]
	];
}

FText SColorThemeBar::GetThemeName() const
{
	return FText::FromString(ColorTheme.Pin()->Name);
}

FReply SColorThemeBar::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnCurrentThemeChanged.ExecuteIfBound(ColorTheme.Pin());

		return FReply::Handled();
	}

	return FReply::Unhandled();
}


TArray< TSharedPtr<FColorTheme> > SColorThemesViewer::ColorThemes;
TWeakPtr<FColorTheme> SColorThemesViewer::CurrentlySelectedThemePtr;
bool SColorThemesViewer::bSRGBEnabled = true;


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SColorThemesViewer::Construct(const FArguments& InArgs)
{
	#define LOCTEXT_NAMESPACE "ColorThemesViewer"

	bUseAlpha = InArgs._UseAlpha;

	LoadColorThemesFromIni();

	const FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );

	// different menus that could be visible for the color themes menu
	// standard menu with "new", "rename" and "delete"
	MenuStandard =
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot() .FillWidth(1) .Padding(3)
		[
			SNew(SButton) .Text(LOCTEXT("NewButton", "New")) .HAlign(HAlign_Center)
				.OnClicked(this, &SColorThemesViewer::NewColorTheme)
		]
		+SHorizontalBox::Slot() .FillWidth(1) .Padding(3)
		[
			SNew(SButton) .Text(LOCTEXT("DuplicateButton", "Duplicate")) .HAlign(HAlign_Center)
				.OnClicked(this, &SColorThemesViewer::DuplicateColorTheme)
		]
		+SHorizontalBox::Slot() .FillWidth(1) .Padding(3)
		[
			SNew(SButton) .Text(LOCTEXT("RenameButton", "Rename")) .HAlign(HAlign_Center)
				.OnClicked(this, &SColorThemesViewer::MenuToRename)
		]
		+SHorizontalBox::Slot() .FillWidth(1) .Padding(3)
		[
			SNew(SButton) .Text(LOCTEXT("DeleteButton", "Delete")) .HAlign(HAlign_Center)
				.OnClicked(this, &SColorThemesViewer::MenuToDelete)
		];

	// menu for renaming the currently selected color theme
	MenuRename =
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot() .FillWidth(1) .Padding(3)
		[
			SAssignNew(RenameTextBox, SEditableTextBox) .Font(SmallLayoutFont)
				.OnTextChanged(this, &SColorThemesViewer::ChangeThemeName)
				.OnTextCommitted(this, &SColorThemesViewer::CommitThemeName)
		]
		+SHorizontalBox::Slot().AutoWidth() .Padding(3)
		[
			SNew(SButton) .Text(LOCTEXT("AcceptRenameButton", "Accept")) .HAlign(HAlign_Right)
				.OnClicked(this, &SColorThemesViewer::AcceptThemeName)
				.IsEnabled(this, &SColorThemesViewer::CanAcceptThemeName)
		]
		+SHorizontalBox::Slot().AutoWidth() .Padding(3)
		[
			SNew(SButton) .Text(LOCTEXT("CancelRenameButton", "Cancel")) .HAlign(HAlign_Right)
				.OnClicked(this, &SColorThemesViewer::MenuToStandard)
		];

	// menu for confirming whether you wish to delete the currently selected color theme
	MenuConfirmDelete =
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot() .FillWidth(1) .HAlign(HAlign_Left) .Padding(3)
		[
			SNew(SButton) .Text(LOCTEXT("ConfirmDeleteButton", "Delete")) .HAlign(HAlign_Center)
				.OnClicked(this, &SColorThemesViewer::DeleteColorTheme)
		]
		+SHorizontalBox::Slot() .FillWidth(1) .HAlign(HAlign_Center) .VAlign(VAlign_Center) .Padding(3)
		[
			SNew(STextBlock) .Text(LOCTEXT("ConfirmDeletePrompt", "Confirm Delete")) .Font(SmallLayoutFont)
		]
		+SHorizontalBox::Slot() .FillWidth(1) .HAlign(HAlign_Right) .Padding(3)
		[
			SNew(SButton) .Text(LOCTEXT("CancelDeleteButton", "Cancel")) .HAlign(HAlign_Center)
				.OnClicked(this, &SColorThemesViewer::MenuToStandard)
		];

	// menu for dropping colors into the trash
	MenuTrashColor =
		SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SColorTrash)
		];

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.WidthOverride(320)
			[
				SAssignNew(ColorThemeList, SListView< TSharedPtr<FColorTheme> >)
					.ItemHeight(32)
					.ListItemsSource(&ColorThemes)
					.OnGenerateRow(this, &SColorThemesViewer::OnGenerateColorThemeBars)
			]
		]
		+SVerticalBox::Slot().AutoHeight() .Padding(0,15,0,0)
		[
			SAssignNew(Menu, SBorder)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(ErrorText, SErrorText)
				.Visibility(this, &SColorThemesViewer::OnGetErrorTextVisibility)
		]
	];

	MenuToStandardNoReturn();

	if (!CurrentlySelectedThemePtr.IsValid())
	{
		CurrentlySelectedThemePtr = ColorThemes[0];
	}
	ColorThemeList->ClearSelection();
	ColorThemeList->SetItemSelection(CurrentlySelectedThemePtr.Pin(), true);

	#undef LOCTEXT_NAMESPACE
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SColorThemesViewer::MenuToStandardNoReturn()
{
	ErrorText->SetError(FString());
	Menu->SetContent(MenuStandard.ToSharedRef());
}

FReply SColorThemesViewer::MenuToStandard()
{
	MenuToStandardNoReturn();
	return FReply::Handled();
}

FReply SColorThemesViewer::MenuToRename()
{
	RenameTextBox->SetText( FText::FromString( GetCurrentColorTheme()->Name ) );
	Menu->SetContent(MenuRename.ToSharedRef());
	return FReply::Handled();
}

FReply SColorThemesViewer::MenuToDelete()
{
	Menu->SetContent(MenuConfirmDelete.ToSharedRef());
	return FReply::Handled();
}

void SColorThemesViewer::MenuToTrash()
{
	Menu->SetContent(MenuTrashColor.ToSharedRef());
}

void SColorThemesViewer::RefreshThemes()
{
	ColorThemeList->RequestListRefresh();

	SaveColorThemesToIni();

	MenuToStandardNoReturn();
}

TSharedPtr<FColorTheme> SColorThemesViewer::GetCurrentColorTheme() const
{
	return CurrentlySelectedThemePtr.IsValid() ? CurrentlySelectedThemePtr.Pin() : ColorThemes[0];
}

void SColorThemesViewer::SetUseAlpha( const TAttribute<bool>& InUseAlpha )
{ 
	bUseAlpha = InUseAlpha;
}

void SColorThemesViewer::SetCurrentColorTheme(TSharedPtr<FColorTheme> NewTheme)
{
	// Set the current theme, requires a preexisting theme to be passed in
	CurrentlySelectedThemePtr = NewTheme;
	ColorThemeList->ClearSelection();
	ColorThemeList->SetItemSelection(NewTheme, true);

	CurrentThemeChangedEvent.Broadcast();
	MenuToStandardNoReturn();
}

TSharedPtr<FColorTheme> SColorThemesViewer::IsColorTheme(const FString& ThemeName)
{
	// Find the desired theme
	for (int32 ThemeIndex = 0; ThemeIndex < ColorThemes.Num(); ++ThemeIndex)
	{
		const TSharedPtr<FColorTheme>& ColorTheme = ColorThemes[ThemeIndex];
		if ( ColorTheme->Name == ThemeName )
		{
			return ColorTheme;
		}
	}
	return TSharedPtr<FColorTheme>();
}

TSharedPtr<FColorTheme> SColorThemesViewer::GetColorTheme(const FString& ThemeName)
{
	// Create the desired theme, if not already
	TSharedPtr<FColorTheme> ColorTheme = IsColorTheme(ThemeName);
	if ( !ColorTheme.IsValid() )
	{
		ColorTheme = NewColorTheme(ThemeName);
	}
	return ColorTheme;
}

FString SColorThemesViewer::MakeUniqueThemeName(const FString& ThemeName)
{
	// Ensure the name of the color theme is unique
	int32 ThemeID = 1;
	FString NewThemeName = ThemeName;
	while ( IsColorTheme(NewThemeName).IsValid() )
	{
		NewThemeName = ThemeName + FString::Printf( TEXT( " %d" ), ThemeID );
		ThemeID++;
	}
	return NewThemeName;
}

TSharedPtr<FColorTheme> SColorThemesViewer::NewColorTheme(const FString& ThemeName, const TArray< TSharedPtr<FLinearColor> >& ThemeColors)
{
	// Create a uniquely named theme
	check( ThemeName.Len() > 0 );
	const FString NewThemeName = MakeUniqueThemeName( ThemeName );
	ColorThemes.Add(MakeShareable(new FColorTheme( NewThemeName, ThemeColors )));
	return ColorThemes.Last();
}

TSharedPtr<FColorTheme> SColorThemesViewer::GetDefaultColorTheme(bool bCreateNew)
{
	// Create a default theme (if bCreateNew is always creates a new one, even if there's already a like named theme)
	const FText Name = NSLOCTEXT("ColorThemesViewer", "NewThemeName", "New Theme");
	if ( bCreateNew )
	{
		return NewColorTheme( Name.ToString() );
	}
	return GetColorTheme( Name.ToString() );
}

FReply SColorThemesViewer::AcceptThemeName()
{
	UpdateThemeNameFromTextBox();
	return FReply::Handled();
}

void SColorThemesViewer::CommitThemeName(const FText& InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter)
	{
		UpdateThemeNameFromTextBox();
	}
}

void SColorThemesViewer :: UpdateThemeNameFromTextBox()
{
	// Update the theme name if it differs, ensuring it is still unique
	const FString Name = RenameTextBox->GetText().ToString();
	if (GetCurrentColorTheme()->Name != Name)
	{
		GetCurrentColorTheme()->Name = MakeUniqueThemeName(Name);
		RefreshThemes();
	}
}

bool SColorThemesViewer::CanAcceptThemeName() const
{
	return !ErrorText->HasError();
}

void SColorThemesViewer::ChangeThemeName(const FText& InText)
{
	ErrorText->SetError(FString());

	const FString ThemeName = InText.ToString();
	for (int32 ThemeIndex = 0; ThemeIndex < ColorThemes.Num(); ++ThemeIndex)
	{
		const TSharedPtr<FColorTheme>& ColorTheme = ColorThemes[ThemeIndex];
		if (ColorTheme != GetCurrentColorTheme() && ColorTheme->Name == ThemeName)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Name"), InText);
			ErrorText->SetError(FText::Format(NSLOCTEXT("ColorThemesViewer", "VerifyTextDup", "A theme already exists with the name '{Name}'."), Args).ToString());
			return;
		}
	}
}

EVisibility SColorThemesViewer::OnGetErrorTextVisibility() const
{
	return !CanAcceptThemeName() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SColorThemesViewer::NewColorTheme()
{
	// Create a new, defaultly named theme and update the display
	GetDefaultColorTheme(true);
	RefreshThemes();
	return FReply::Handled();
}

FReply SColorThemesViewer::DuplicateColorTheme()
{
	// Create a copy of the existing current color theme
	TArray< TSharedPtr<FLinearColor> > NewColors;
	const TArray< TSharedPtr<FLinearColor> >& CurrentColors = CurrentlySelectedThemePtr.Pin()->GetColors();
	for (int32 ColorIndex = 0; ColorIndex < CurrentColors.Num(); ++ColorIndex)
	{
		NewColors.Add(MakeShareable(new FLinearColor(*CurrentColors[ColorIndex])));
	}
	const FText Name = NSLOCTEXT("ColorThemesViewer", "CopyThemeNameAppend", " Copy");
	NewColorTheme( CurrentlySelectedThemePtr.Pin()->Name + Name.ToString(), NewColors );
	RefreshThemes();
	return FReply::Handled();
}

FReply SColorThemesViewer::DeleteColorTheme()
{
	// Delete the current color theme
	ColorThemes.Remove(GetCurrentColorTheme());
	if (!ColorThemes.Num())
	{
	 	// Create the default if none exists
		GetDefaultColorTheme();
	}
	SetCurrentColorTheme(ColorThemes[0]);
	RefreshThemes();
	return FReply::Handled();
}

bool SColorThemesViewer::OnReadUseSRGB() const
{
	return bSRGBEnabled;
}

bool SColorThemesViewer::OnReadUseAlpha() const
{
	return bUseAlpha.Get();
}

TSharedRef<ITableRow> SColorThemesViewer::OnGenerateColorThemeBars(TSharedPtr<FColorTheme> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
	SNew( STableRow< TSharedPtr<FColorTheme> >, OwnerTable )
	[
		SNew(SColorThemeBar)
		. ColorTheme(InItem)
		. OnCurrentThemeChanged(this, &SColorThemesViewer::SetCurrentColorTheme)
		. ShowTrashCallback(this, &SColorThemesViewer::MenuToTrash)
		. HideTrashCallback(this, &SColorThemesViewer::MenuToStandardNoReturn)
		. UseSRGB(this, &SColorThemesViewer::OnReadUseSRGB)
		. UseAlpha(this, &SColorThemesViewer::OnReadUseAlpha)
	];
}

void SColorThemesViewer::LoadColorThemesFromIni()
{
	if (FPaths::FileExists(GEditorPerProjectIni))
	{
		bool bThemesRemaining = true;
		int32 ThemeID = 0;
		while (bThemesRemaining)
		{
			const FString ThemeName = GConfig->GetStr(TEXT("ColorThemes"), *FString::Printf(TEXT("Theme%i"), ThemeID), GEditorPerProjectIni);
			if (!ThemeName.IsEmpty())
			{
				TSharedPtr<FColorTheme> ColorTheme = GetColorTheme( ThemeName );
				check( ColorTheme.IsValid() );
				bool bColorsRemaining = true;
				int32 ColorID = 0;
				while (bColorsRemaining)
				{
					const FString ColorString = GConfig->GetStr(TEXT("ColorThemes"), *FString::Printf(TEXT("Theme%iColor%i"), ThemeID, ColorID), GEditorPerProjectIni);
					if (!ColorString.IsEmpty())
					{
						// Add the color if it hasn't already
						FLinearColor Color;
						Color.InitFromString(ColorString);
						if ( ColorTheme->FindApproxColor( Color ) == INDEX_NONE )
						{
							ColorTheme->InsertNewColor( MakeShareable(new FLinearColor(Color)), 0 );
						}
						++ColorID;
					}
					else
					{
						bColorsRemaining = false;
					}
				}
				++ThemeID;
			}
			else
			{
				bThemesRemaining = false;
			}
		}
	}
	
	if (!ColorThemes.Num())
	{
	 	// Create the default if none exists
		GetDefaultColorTheme();
	}
}

void SColorThemesViewer::SaveColorThemesToIni()
{
	if (FPaths::FileExists(GEditorPerProjectIni))
	{
		GConfig->EmptySection(TEXT("ColorThemes"), GEditorPerProjectIni);
		for (int32 ThemeIndex = 0; ThemeIndex < ColorThemes.Num(); ++ThemeIndex)
		{
			const TSharedPtr<FColorTheme>& Theme = ColorThemes[ThemeIndex];
			GConfig->SetString(TEXT("ColorThemes"), *FString::Printf(TEXT("Theme%i"), ThemeIndex), *Theme->Name, GEditorPerProjectIni);

			const TArray< TSharedPtr<FLinearColor> >& Colors = Theme->GetColors();
			for (int32 ColorIndex = 0; ColorIndex < Colors.Num(); ++ColorIndex)
			{
				const TSharedPtr<FLinearColor>& Color = Colors[ColorIndex];
				GConfig->SetString(TEXT("ColorThemes"), *FString::Printf(TEXT("Theme%iColor%i"), ThemeIndex, ColorIndex), *Color->ToString(), GEditorPerProjectIni);
			}
		}
	}
}
