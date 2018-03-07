// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneBuiltInEasingFunctionCustomization.h"
#include "Generators/MovieSceneEasingCurves.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"

#include "STextBlock.h"
#include "SGridPanel.h"
#include "SButton.h"
#include "SBox.h"
#include "SlateApplication.h"
#include "ScopedTransaction.h"

#include "EditorStyleSet.h"

struct FGroupedEasing
{
	FString GroupingName;
	TArray<EMovieSceneBuiltInEasing, TInlineAllocator<3>> Values;
};

class SBuiltInFunctionVisualizer : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SBuiltInFunctionVisualizer){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, EMovieSceneBuiltInEasing InValue)
	{
		InterpValue = FVector2D::ZeroVector;

		UMovieSceneBuiltInEasingFunction* DefaultObject = GetMutableDefault<UMovieSceneBuiltInEasingFunction>();
		EMovieSceneBuiltInEasing DefaultType = DefaultObject->Type;

		DefaultObject->Type = InValue;

		float Interp = 0.f;
		while (Interp <= 1.f)
		{
			Samples.Add(FVector2D(Interp, DefaultObject->Evaluate(Interp)));
			Interp += 0.05f;
		}

		DefaultObject->Type = DefaultType;
		EasingType = InValue;

		ChildSlot
		[
			SNew(SOverlay)
		];
	}

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (!TimerHandle.IsValid())
		{
			MouseOverTime = FSlateApplication::Get().GetCurrentTime();
			TimerHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SBuiltInFunctionVisualizer::TickInterp));
		}
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		if (TimerHandle.IsValid())
		{
			InterpValue = FVector2D::ZeroVector;
			UnRegisterActiveTimer(TimerHandle.ToSharedRef());
			TimerHandle = nullptr;
		}
	}

	EActiveTimerReturnType TickInterp(const double InCurrentTime, const float InDeltaTime)
	{
		static float InterpInPad = .25f, InterpOutPad = .5f;
		static float InterpDuration = .5f;

		float TotalInterpTime = InterpInPad + InterpDuration + InterpOutPad;
		InterpValue.X = FMath::Clamp((FMath::Fmod(float(InCurrentTime - MouseOverTime), TotalInterpTime) - InterpInPad) / InterpDuration, 0.f, 1.f);

		UMovieSceneBuiltInEasingFunction* DefaultObject = GetMutableDefault<UMovieSceneBuiltInEasingFunction>();

		EMovieSceneBuiltInEasing DefaultType = DefaultObject->Type;
		DefaultObject->Type = EasingType;

		InterpValue.Y = DefaultObject->Evaluate(InterpValue.X);
		DefaultObject->Type = DefaultType;

		return EActiveTimerReturnType::Continue;
	}

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
	{
		float VerticalPad = 0.2f;
		FVector2D InverseVerticalSize(AllottedGeometry.Size.X, -AllottedGeometry.Size.Y);

		const float VerticalBottom = AllottedGeometry.Size.Y-AllottedGeometry.Size.Y*VerticalPad*.5f;
		const float CurveHeight = AllottedGeometry.Size.Y * (1.f-VerticalPad);
		const float CurveWidth = AllottedGeometry.Size.X - 5.f;

		TArray<FVector2D> Points;
		for (FVector2D Sample : Samples)
		{
			FVector2D Offset(5.f, VerticalBottom);
			Points.Add(Offset + FVector2D(
				CurveWidth*Sample.X,
				-CurveHeight * Sample.Y
			));
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None);

		if (TimerHandle.IsValid())
		{
			FVector2D PointOffset(0.f, VerticalBottom - CurveHeight*InterpValue.Y - 4.f);

			static const FSlateBrush* InterpPointBrush = FEditorStyle::GetBrush("Sequencer.InterpLine");
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.MakeChild(
					FVector2D(AllottedGeometry.Size.X, 7.f),
					FSlateLayoutTransform(PointOffset)
				).ToPaintGeometry(),
				InterpPointBrush,
				ESlateDrawEffect::None,
				FLinearColor::Green
			);
		}

		return LayerId+1;
	}

private:

	TSharedPtr<FActiveTimerHandle> TimerHandle;
	double MouseOverTime;
	EMovieSceneBuiltInEasing EasingType;

	FVector2D InterpValue;
	TArray<FVector2D> Samples;
};

void FMovieSceneBuiltInEasingFunctionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TypeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieSceneBuiltInEasingFunction, Type));

	const UEnum* EasingEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EMovieSceneBuiltInEasing"), false);
	check(EasingEnum)

	TArray<FGroupedEasing> Groups;
	auto FindGroup = [&](const FString& GroupName) -> FGroupedEasing&
	{
		for (int32 Index = Groups.Num() - 1; Index >= 0; --Index)
		{
			if (Groups[Index].GroupingName == GroupName)
			{
				return Groups[Index];
			}
		}

		Groups.Emplace();
		Groups.Last().GroupingName = GroupName;
		return Groups.Last();
	};

	for (int32 NameIndex = 0; NameIndex < EasingEnum->NumEnums() - 1; ++NameIndex)
	{
		const FString& Grouping = EasingEnum->GetMetaData(TEXT("Grouping"), NameIndex);
		EMovieSceneBuiltInEasing Value = (EMovieSceneBuiltInEasing)EasingEnum->GetValueByIndex(NameIndex);

		FindGroup(Grouping).Values.Add(Value);
	}


	TSharedRef<SGridPanel> Grid = SNew(SGridPanel);

	int32 RowIndex = 0;
	for (const FGroupedEasing& Group : Groups)
	{
		for (int32 ColumnIndex = 0; ColumnIndex < Group.Values.Num(); ++ColumnIndex)
		{
			EMovieSceneBuiltInEasing Value = Group.Values[ColumnIndex];

			Grid->AddSlot(ColumnIndex, RowIndex)
			[
				SNew(SBox)
				.WidthOverride(100.f)
				.HeightOverride(50.f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.OnClicked(this, &FMovieSceneBuiltInEasingFunctionCustomization::SetType, Value)
					[
						SNew(SBuiltInFunctionVisualizer, Value)
					]
				]
			];

			Grid->AddSlot(ColumnIndex, RowIndex+1)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(EasingEnum->GetDisplayNameTextByValue((int64)Value))
			];
		}

		RowIndex += 2;
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Easing");
	DetailBuilder.HideProperty(TypeProperty);
	FDetailWidgetRow& Row = Category.AddCustomRow(FText());
	Row.WholeRowContent()
	[
		Grid
	];
}

FReply FMovieSceneBuiltInEasingFunctionCustomization::SetType(EMovieSceneBuiltInEasing NewType)
{
	FScopedTransaction Transaction(NSLOCTEXT("EasingFunctionCustomization", "SetEasingType", "Set Easing Type"));	

	TypeProperty->NotifyPreChange();

	TArray<void*> RawData;
	TypeProperty->AccessRawData(RawData);

	for (void* Ptr : RawData)
	{
		*((EMovieSceneBuiltInEasing*)Ptr) = NewType;
	}

	TypeProperty->NotifyPostChange();
	TypeProperty->NotifyFinishedChangingProperties();

	return FReply::Unhandled();
}
