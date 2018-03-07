// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Images/SThrobber.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"


void SThrobber::Construct(const FArguments& InArgs)
{
	PieceImage = InArgs._PieceImage;
	NumPieces = InArgs._NumPieces;
	Animate = InArgs._Animate;

	HBox = SNew(SHorizontalBox);

	this->ChildSlot
	[
		HBox.ToSharedRef()
	];

	ConstructPieces();
}

void SThrobber::ConstructPieces()
{
	ThrobberCurve.Empty();
	AnimCurves = FCurveSequence();
	for (int32 PieceIndex = 0; PieceIndex < NumPieces; ++PieceIndex)
	{
		ThrobberCurve.Add(AnimCurves.AddCurve(PieceIndex*0.05f, 1.5f));
	}
	AnimCurves.Play(this->AsShared(), true);

	HBox->ClearChildren();
	for (int32 PieceIndex = 0; PieceIndex < NumPieces; ++PieceIndex)
	{
		HBox->AddSlot()
		.AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.ContentScale(this, &SThrobber::GetPieceScale, PieceIndex)
			.ColorAndOpacity(this, &SThrobber::GetPieceColor, PieceIndex)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(this, &SThrobber::GetPieceBrush)
			]
		];
	}
}

const FSlateBrush* SThrobber::GetPieceBrush() const
{
	return PieceImage;
}

void SThrobber::SetPieceImage(const FSlateBrush* InPieceImage)
{
	PieceImage = InPieceImage;
}

void SThrobber::SetNumPieces(int InNumPieces)
{
	NumPieces = InNumPieces;
	ConstructPieces();
}

void SThrobber::SetAnimate(EAnimation InAnimate)
{
	Animate = InAnimate;
}

FVector2D SThrobber::GetPieceScale(int32 PieceIndex) const
{
	const float Value = FMath::Sin( 2 * PI * ThrobberCurve[PieceIndex].GetLerp());
	
	const bool bAnimateHorizontally = ((0 != (Animate & Horizontal)));
	const bool bAnimateVertically = (0 != (Animate & Vertical));
	
	return FVector2D(
		bAnimateHorizontally ? Value : 1.0f,
		bAnimateVertically ? Value : 1.0f
	);
}

FLinearColor SThrobber::GetPieceColor(int32 PieceIndex) const
{
	const bool bAnimateOpacity = (0 != (Animate & Opacity));
	if (bAnimateOpacity)
	{
		const float Value = FMath::Sin( 2 * PI * ThrobberCurve[PieceIndex].GetLerp());
		return FLinearColor(1.0f,1.0f,1.0f, Value);
	}
	else
	{
		return FLinearColor::White;
	}
}

// SCircularThrobber
const float SCircularThrobber::MinimumPeriodValue = SMALL_NUMBER;

void SCircularThrobber::Construct(const FArguments& InArgs)
{
	PieceImage = InArgs._PieceImage;
	NumPieces = InArgs._NumPieces;
	Period = InArgs._Period;
	Radius = InArgs._Radius;

	ConstructSequence();
}

void SCircularThrobber::SetPieceImage(const FSlateBrush* InPieceImage)
{
	PieceImage = InPieceImage;
}

void SCircularThrobber::SetNumPieces(const int32 InNumPieces)
{
	NumPieces = InNumPieces;
}

void SCircularThrobber::SetPeriod(const float InPeriod)
{
	Period = InPeriod;
	ConstructSequence();
}

void SCircularThrobber::SetRadius(const float InRadius)
{
	Radius = InRadius;
}

void SCircularThrobber::ConstructSequence()
{
	Sequence = FCurveSequence();
	Curve = Sequence.AddCurve(0.f, FMath::Max(Period, MinimumPeriodValue));
	Sequence.Play(this->AsShared(), true);
}

int32 SCircularThrobber::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FLinearColor FinalColorAndOpacity( InWidgetStyle.GetColorAndOpacityTint() * PieceImage->GetTint( InWidgetStyle ) );
	const FVector2D LocalOffset = (AllottedGeometry.GetLocalSize() - PieceImage->ImageSize) * 0.5f;
	const float DeltaAngle = NumPieces > 0 ? 2 * PI / NumPieces : 0;
	const float Phase = Curve.GetLerp() * 2 * PI;

	for (int32 PieceIdx = 0; PieceIdx < NumPieces; ++PieceIdx)
	{
		const float Angle = DeltaAngle * PieceIdx + Phase;
		// scale each piece linearly until the last piece is full size
		FSlateLayoutTransform PieceLocalTransform(
			(PieceIdx + 1) / (float)NumPieces,
			LocalOffset + LocalOffset * FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)));
		FPaintGeometry PaintGeom = AllottedGeometry.ToPaintGeometry(PieceImage->ImageSize, PieceLocalTransform);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, PaintGeom, PieceImage, ESlateDrawEffect::None, FinalColorAndOpacity);
	}
	
	return LayerId;
}

FVector2D SCircularThrobber::ComputeDesiredSize( float ) const
{
	return FVector2D(Radius, Radius) * 2;
}
