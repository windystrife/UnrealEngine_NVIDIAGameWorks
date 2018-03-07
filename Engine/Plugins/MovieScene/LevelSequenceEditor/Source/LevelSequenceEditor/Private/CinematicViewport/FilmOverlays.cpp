// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CinematicViewport/FilmOverlays.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SHeader.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "Styles/LevelSequenceEditorStyle.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Colors/SColorPicker.h"

#define LOCTEXT_NAMESPACE "LevelSequenceEditorFilmOverlays"

DECLARE_DELEGATE_OneParam(FOnColorPicked, const FLinearColor&);

namespace WidgetHelpers
{
	TSharedRef<SWidget> CreateColorWidget(FLinearColor* Color, FOnColorPicked OnColorPicked = FOnColorPicked())
	{
		auto GetValue = [=]{
			return *Color;
		};

		auto SetValue = [=](FLinearColor NewColor){
			*Color = NewColor;
			OnColorPicked.ExecuteIfBound(NewColor);
		};

		auto OnGetMenuContent = [=]() -> TSharedRef<SWidget> {
			// Open a color picker
			return SNew(SColorPicker)
			.TargetColorAttribute_Lambda(GetValue)
			.UseAlpha(true)
			.DisplayInlineVersion(true)
			.OnColorCommitted_Lambda(SetValue);
		};

		return SNew(SComboButton)
			.ContentPadding(0)
			.HasDownArrow(false)
			.ButtonStyle(FEditorStyle::Get(), "Sequencer.AnimationOutliner.ColorStrip")
			.OnGetMenuContent_Lambda(OnGetMenuContent)
			.CollapseMenuOnParentFocus(true)
			.ButtonContent()
			[
				SNew(SColorBlock)
				.Color_Lambda(GetValue)
				.ShowBackgroundForAlpha(true)
				.IgnoreAlpha(false)
				.Size(FVector2D(10.0f, 10.0f))
			];
	}

	template<typename T>
	TSharedRef<SWidget> CreateSpinBox(T* Value, T Min, T Max, TSharedPtr<INumericTypeInterface<T>> TypeInterface = nullptr)
	{
		auto GetValue = [=]{ return *Value; };
		auto SetValue = [=](T NewValue){ *Value = NewValue; };
		auto SetValueCommitted = [=](T NewValue, ETextCommit::Type){ *Value = NewValue; };

		return SNew(SSpinBox<T>)
			.MinValue(Min)
			.MaxValue(Max)
			.Value_Lambda(GetValue)
			.OnValueChanged_Lambda(SetValue)
			.OnValueCommitted_Lambda(SetValueCommitted)
			.TypeInterface(TypeInterface);
	}
}

struct FFilmOverlay_None : IFilmOverlay
{
	FText GetDisplayName() const { return LOCTEXT("OverlayDisabled", "Disabled"); }
	
	const FSlateBrush* GetThumbnail() const { return FLevelSequenceEditorStyle::Get()->GetBrush("FilmOverlay.Disabled"); }

	void Paint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
	{
	}
};

struct FFilmOverlay_Grid : IFilmOverlay
{
	FFilmOverlay_Grid(int32 InNumDivsH, int32 InNumDivsV)
		: NumDivsH(InNumDivsH)
		, NumDivsV(InNumDivsV)
	{
		BrushName = *FString::Printf(TEXT("FilmOverlay.%dx%dGrid"), NumDivsH, NumDivsV);
	}

	FText GetDisplayName() const
	{
		return FText::Format(
			LOCTEXT("GridNameFormat", "Grid ({0}x{1})"),
			FText::AsNumber(NumDivsH),
			FText::AsNumber(NumDivsV)
		);
	}
	
	const FSlateBrush* GetThumbnail() const
	{
		return FLevelSequenceEditorStyle::Get()->GetBrush(BrushName);
	}

	void Paint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
	{
		const int32 IntervalH = AllottedGeometry.GetLocalSize().X / NumDivsH;
		const int32 IntervalV = AllottedGeometry.GetLocalSize().Y / NumDivsV;

		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(2);

		for (int32 OffsetH = 1; OffsetH < NumDivsH; ++OffsetH)
		{
			LinePoints[0] = FVector2D(IntervalH*OffsetH, 0.f);
			LinePoints[1] = FVector2D(IntervalH*OffsetH, AllottedGeometry.Size.Y);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				Tint,
				false
				);
		}

		for (int32 OffsetV = 1; OffsetV < NumDivsV; ++OffsetV)
		{
			LinePoints[0] = FVector2D(0.f, IntervalV*OffsetV);
			LinePoints[1] = FVector2D(AllottedGeometry.GetLocalSize().X, IntervalV*OffsetV);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				Tint,
				false
				);
		}

	}
private:
	FName BrushName;
	int32 NumDivsH;
	int32 NumDivsV;
};

struct FFilmOverlay_Rabatment : IFilmOverlay
{
	FText GetDisplayName() const { return LOCTEXT("RabatmentName", "Rabatment"); }
	
	const FSlateBrush* GetThumbnail() const { return FLevelSequenceEditorStyle::Get()->GetBrush("FilmOverlay.Rabatment"); }

	void Paint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
	{
		FVector2D Position(AllottedGeometry.Size.X / 2, AllottedGeometry.Size.Y / 2);
		float Size = FMath::Min(AllottedGeometry.Size.X, AllottedGeometry.Size.Y) * .1f;

		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(2);

		LinePoints[0] = FVector2D(AllottedGeometry.GetLocalSize().Y, 0.f);
		LinePoints[1] = FVector2D(AllottedGeometry.GetLocalSize().Y, AllottedGeometry.Size.Y);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			Tint,
			false
			);

		LinePoints[0] = FVector2D(AllottedGeometry.GetLocalSize().X - AllottedGeometry.GetLocalSize().Y, 0.f);
		LinePoints[1] = FVector2D(AllottedGeometry.GetLocalSize().X - AllottedGeometry.GetLocalSize().Y, AllottedGeometry.GetLocalSize().Y);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			Tint,
			false
			);

	}
};

struct FFilmOverlay_Crosshair : IFilmOverlay
{
	FText GetDisplayName() const { return LOCTEXT("CrosshairName", "Crosshair"); }
	const FSlateBrush* GetThumbnail() const { return FLevelSequenceEditorStyle::Get()->GetBrush("FilmOverlay.Crosshair"); }
	void Paint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
	{
		FVector2D Position(AllottedGeometry.Size.X / 2, AllottedGeometry.Size.Y / 2);
		float Size = FMath::Min(AllottedGeometry.Size.X, AllottedGeometry.Size.Y) * .1f;

		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(2);

		LinePoints[0] = FVector2D(Position.X, Position.Y - Size);
		LinePoints[1] = FVector2D(Position.X, Position.Y - Size * .25f);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			Tint
			);
		LinePoints[0] = FVector2D(Position.X, Position.Y + Size);
		LinePoints[1] = FVector2D(Position.X, Position.Y + Size * .25f);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			Tint,
			false
			);

		LinePoints[0] = FVector2D(Position.X - Size, Position.Y);
		LinePoints[1] = FVector2D(Position.X - Size * .25f, Position.Y);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			Tint
			);
		LinePoints[0] = FVector2D(Position.X + Size, Position.Y);
		LinePoints[1] = FVector2D(Position.X + Size * .25f, Position.Y);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			Tint,
			false
			);
	}
};

struct FFilmOverlay_SafeFrame : IFilmOverlay
{
	FFilmOverlay_SafeFrame(FText InDisplayName, int32 InSizePercentage, const FLinearColor& InColor)
	{
		DisplayName = InDisplayName;
		SizePercentage = InSizePercentage;

		Tint = InColor;
	}

	FText GetDisplayName() const { return DisplayName; }

	const FSlateBrush* GetThumbnail() const { return nullptr; }

	void Paint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
	{
		FVector2D TopLeft = AllottedGeometry.GetLocalSize() * ((100.f - SizePercentage) * .5f) / 100.f;
		FVector2D BottomRight = AllottedGeometry.GetLocalSize() - TopLeft;

		TArray<FVector2D> LinePoints;
		LinePoints.Add(FVector2D(TopLeft.X,			TopLeft.Y));
		LinePoints.Add(FVector2D(BottomRight.X,		TopLeft.Y));
		LinePoints.Add(FVector2D(BottomRight.X,		BottomRight.Y));
		LinePoints.Add(FVector2D(TopLeft.X,			BottomRight.Y));
		LinePoints.Add(FVector2D(TopLeft.X-1,		TopLeft.Y-1));

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			Tint,
			false
			);
	}

	virtual TSharedPtr<SWidget> ConstructSettingsWidget() override
	{
		struct FPercentageInterface : TDefaultNumericTypeInterface<int32>
		{
			virtual FString ToString(const int32& Value) const override
			{
				return TDefaultNumericTypeInterface::ToString(Value) + TEXT("%");
			}
			virtual TOptional<int32> FromString(const FString& InString, const int32& InExistingValue) override
			{
				return TDefaultNumericTypeInterface::FromString(InString.Replace(TEXT("%"), TEXT("")), InExistingValue);
			}
			virtual bool IsCharacterValid(TCHAR InChar) const override
			{
				return InChar == '%' || TDefaultNumericTypeInterface::IsCharacterValid(InChar);
			}
		};

		return SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				WidgetHelpers::CreateSpinBox<int32>(&SizePercentage, 1, 99, MakeShareable(new FPercentageInterface))
			]

			+ SVerticalBox::Slot()
			.Padding(FMargin(0.f, 1.f, 0.f, 0.f))
			.AutoHeight()
			[
				WidgetHelpers::CreateColorWidget(&Tint)
			];
	}

private:
	int32 SizePercentage;
	FText DisplayName;
};


struct FFilmOverlay_LetterBox : IFilmOverlay
{
	FFilmOverlay_LetterBox()
	{
		Ratio1 = 2.35f;
		Ratio2 = 1.f;
		Color = FLinearColor(0.f, 0.f, 0.f, .5f);
	}

	FText GetDisplayName() const { return LOCTEXT("LetterboxMask", "Letterbox Mask"); }

	const FSlateBrush* GetThumbnail() const { return nullptr; }

	void Paint(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
	{
		const FSlateBrush* Brush = FEditorStyle::GetBrush("WhiteBrush");

		const float DesiredRatio = Ratio1 / Ratio2;
		const float CurrentRatio = AllottedGeometry.GetLocalSize().X / AllottedGeometry.GetLocalSize().Y;

		if (CurrentRatio > DesiredRatio)
		{
			// vertical letterbox mask
			FVector2D LetterBoxSize((AllottedGeometry.GetLocalSize().X - AllottedGeometry.GetLocalSize().Y * DesiredRatio) * .5f, AllottedGeometry.GetLocalSize().Y);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(LetterBoxSize, FSlateLayoutTransform(FVector2D(0.f, 0.f))),
				Brush,
				ESlateDrawEffect::None,
				Color
				);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(LetterBoxSize, FSlateLayoutTransform(FVector2D(AllottedGeometry.GetLocalSize().X - LetterBoxSize.X, 0.f))),
				Brush,
				ESlateDrawEffect::None,
				Color
				);

		}
		else if (CurrentRatio < DesiredRatio)
		{
			// horizontal letterbox mask
			FVector2D LetterBoxSize(AllottedGeometry.GetLocalSize().X, (AllottedGeometry.GetLocalSize().Y - AllottedGeometry.GetLocalSize().X / DesiredRatio) * .5f);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(LetterBoxSize, FSlateLayoutTransform(FVector2D(0.f, 0.f))),
				Brush,
				ESlateDrawEffect::None,
				Color
				);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(LetterBoxSize, FSlateLayoutTransform(FVector2D(0.f, AllottedGeometry.GetLocalSize().Y - LetterBoxSize.Y))),
				Brush,
				ESlateDrawEffect::None,
				Color
				);
		}
	}

	virtual TSharedPtr<SWidget> ConstructSettingsWidget() override
	{
		return SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				[
					WidgetHelpers::CreateSpinBox<float>(&Ratio1, 0.1, 35)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(2.f, 0.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT(":")))
				]

				+ SHorizontalBox::Slot()
				[
					WidgetHelpers::CreateSpinBox<float>(&Ratio2, 0.1, 35)
				]
			]

			+ SVerticalBox::Slot()
			.Padding(FMargin(0.f, 1.f, 0.f, 0.f))
			.AutoHeight()
			[
				WidgetHelpers::CreateColorWidget(&Color)
			];
	}

private:
	float Ratio1;
	float Ratio2;
	FLinearColor Color;
};

void SFilmOverlay::Construct(const FArguments& InArgs)
{
	FilmOverlays = InArgs._FilmOverlays;
}

int32 SFilmOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TArray<IFilmOverlay*> Overlays = FilmOverlays.Get();
	for (IFilmOverlay* Overlay : Overlays)
	{
		Overlay->Paint(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
		++LayerId;
	}

	return LayerId;
}

void SFilmOverlayOptions::Construct(const FArguments& InArgs)
{
	MasterColorTint = FLinearColor(1.f, 1.f, 1.f, .5f);

	MasterFilmOverlays.Add(NAME_None, TUniquePtr<IFilmOverlay>(new FFilmOverlay_None));
	MasterFilmOverlays.Add("3x3Grid", TUniquePtr<IFilmOverlay>(new FFilmOverlay_Grid(3, 3)));
	MasterFilmOverlays.Add("2x2Grid", TUniquePtr<IFilmOverlay>(new FFilmOverlay_Grid(2, 2)));
	MasterFilmOverlays.Add("Crosshair", TUniquePtr<IFilmOverlay>(new FFilmOverlay_Crosshair));
	MasterFilmOverlays.Add("Rabatment", TUniquePtr<IFilmOverlay>(new FFilmOverlay_Rabatment));

	ToggleableOverlays.Add("ActionSafeFrame",	TUniquePtr<IFilmOverlay>(new FFilmOverlay_SafeFrame(LOCTEXT("ActionSafeFrame", "Action Safe"), 95.f, FLinearColor::Red)));
	ToggleableOverlays.Add("TitleSafeFrame",	TUniquePtr<IFilmOverlay>(new FFilmOverlay_SafeFrame(LOCTEXT("TitleSafeFrame", "Title Safe"), 90.f, FLinearColor::Yellow)));
	ToggleableOverlays.Add("CustomSafeFrame",	TUniquePtr<IFilmOverlay>(new FFilmOverlay_SafeFrame(LOCTEXT("CustomSafeFrame", "Custom Safe"), 85.f, FLinearColor::Green)));
	ToggleableOverlays.Add("LetterBox",			TUniquePtr<IFilmOverlay>(new FFilmOverlay_LetterBox));

	OverlayWidget = SNew(SFilmOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		.FilmOverlays(this, &SFilmOverlayOptions::GetActiveFilmOverlays);

	ChildSlot
	[
		SNew(SComboButton)
		.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
		.ForegroundColor(FEditorStyle::GetColor("InvertedForeground"))
		.OnGetMenuContent(this, &SFilmOverlayOptions::GetMenuContent)
		.ButtonContent()
		[
			SNew(SBox)
			.WidthOverride(36)
			.HeightOverride(24)
			.ToolTipText(LOCTEXT("FilmOverlaysToolTip", "Displays a list of available film overlays to apply to this viewport."))
			[
				SNew(SImage)
				.Image(this, &SFilmOverlayOptions::GetCurrentThumbnail)
			]
		]
	];
}

FLinearColor SFilmOverlayOptions::GetMasterColorTint() const
{
	return MasterColorTint;
}

void SFilmOverlayOptions::OnMasterColorTintChanged(const FLinearColor& Tint)
{
	IFilmOverlay* Overlay = GetMasterFilmOverlay();
	if (Overlay)
	{
		Overlay->SetTint(MasterColorTint);
	}
}

TSharedRef<SWidget> SFilmOverlayOptions::GetMenuContent()
{
	return SNew(SGridPanel)

		+ SGridPanel::Slot(0, 0)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 5.f)
			[
				SNew(SHeader)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OverlaysHeader", "Composition Overlays"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ConstructMasterOverlaysMenu()
			]
		]

		+ SGridPanel::Slot(0, 1)
		.Padding(10)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(2.f, 0.f, 5.f, 0.f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OverlayTint", "Tint: "))
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				WidgetHelpers::CreateColorWidget(&MasterColorTint, FOnColorPicked::CreateRaw(this, &SFilmOverlayOptions::OnMasterColorTintChanged))
			]
		]

		+ SGridPanel::Slot(1, 0)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 5.f)
			[
				SNew(SHeader)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SafeFrameHeader", "Frames"))
				]
			]
			
			+ SVerticalBox::Slot()
			[
				ConstructToggleableOverlaysMenu()
			]
		];
}

TSharedRef<SWidget> SFilmOverlayOptions::ConstructMasterOverlaysMenu()
{
	TSharedRef<SUniformGridPanel> OverlaysPanel = SNew(SUniformGridPanel).SlotPadding(10.f);

	TArray<FName> OverlayNames;
	MasterFilmOverlays.GenerateKeyArray(OverlayNames);

	const int32 NumColumns = FMath::Log2(OverlayNames.Num() - 1);

	int32 ColumnIndex = 0, RowIndex = 0;
	for (int32 OverlayIndex = 0; OverlayIndex < OverlayNames.Num(); ++OverlayIndex)
	{
		IFilmOverlay& Overlay = *MasterFilmOverlays[OverlayNames[OverlayIndex]].Get();
		OverlaysPanel->AddSlot(ColumnIndex, RowIndex)
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &SFilmOverlayOptions::SetMasterFilmOverlay, OverlayNames[OverlayIndex])
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(0.f, 4.f)
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(36)
					.HeightOverride(24)
					[
						SNew(SImage)
						.Image(Overlay.GetThumbnail())
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FEditorStyle::GetColor("DefaultForeground"))
					.Text(Overlay.GetDisplayName())
				]
			]
		];

		if (++ColumnIndex >= NumColumns)
		{
			ColumnIndex = 0;
			++RowIndex;
		}
	}

	return OverlaysPanel;
}

TSharedRef<SWidget> SFilmOverlayOptions::ConstructToggleableOverlaysMenu()
{
	TSharedRef<SGridPanel> GridPanel = SNew(SGridPanel);

	int32 Row = 0;
	for (auto& Pair : ToggleableOverlays)
	{
		TSharedPtr<SWidget> Settings = Pair.Value->ConstructSettingsWidget();
		if (!Settings.IsValid())
		{
			continue;
		}

		TUniquePtr<IFilmOverlay>* Ptr = &Pair.Value;

		auto OnCheckStateChanged = [=](ECheckBoxState InNewState){ (*Ptr)->SetEnabled(InNewState == ECheckBoxState::Checked); };
		auto IsChecked = [=]{ return (*Ptr)->IsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; };
		auto IsEnabled = [=]{ return (*Ptr)->IsEnabled(); };

		Settings->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(IsEnabled)));

		GridPanel->AddSlot(0, Row)
		.VAlign(VAlign_Center)
		.Padding(10.f, 5.f)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged_Lambda(OnCheckStateChanged)
			.IsChecked_Lambda(IsChecked)
			[
				SNew(STextBlock)
				.Text(Pair.Value->GetDisplayName())
			]
		];

		GridPanel->AddSlot(1, Row)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.Padding(10.f, 5.f)
		[
			SNew(SBox)
			.WidthOverride(100)
			[
				Settings.ToSharedRef()
			]
		];

		++Row;
	}

	return GridPanel;
}

FReply SFilmOverlayOptions::SetMasterFilmOverlay(FName InName)
{
	CurrentMasterOverlay = InName;
	IFilmOverlay* Overlay = GetMasterFilmOverlay();
	if (Overlay)
	{
		Overlay->SetTint(MasterColorTint);
	}
	return FReply::Unhandled();
}

TSharedRef<SFilmOverlay> SFilmOverlayOptions::GetFilmOverlayWidget() const
{
	return OverlayWidget.ToSharedRef();
}

TArray<IFilmOverlay*> SFilmOverlayOptions::GetActiveFilmOverlays() const
{
	TArray<IFilmOverlay*> Overlays;

	if (IFilmOverlay* Overlay = GetMasterFilmOverlay())
	{
		Overlays.Add(Overlay);
	}

	for (auto& Pair: ToggleableOverlays)
	{
		if (Pair.Value->IsEnabled())
		{
			Overlays.Add(Pair.Value.Get());
		}
	}

	return Overlays;
}

const FSlateBrush* SFilmOverlayOptions::GetCurrentThumbnail() const
{
	if (!CurrentMasterOverlay.IsNone())
	{
		return MasterFilmOverlays[CurrentMasterOverlay]->GetThumbnail();
	}

	return FLevelSequenceEditorStyle::Get()->GetBrush("FilmOverlay.DefaultThumbnail");
}

IFilmOverlay* SFilmOverlayOptions::GetMasterFilmOverlay() const
{
	if (!CurrentMasterOverlay.IsNone())
	{
		return MasterFilmOverlays[CurrentMasterOverlay].Get();
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
