// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Geometry.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Models/WidgetReflectorNode.h"
#include "Widgets/Layout/SGridPanel.h"

#define LOCTEXT_NAMESPACE "SReflectorToolTipWidget"

class SReflectorToolTipWidget
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SReflectorToolTipWidget)
		: _WidgetInfoToVisualize()
	{ }

		SLATE_ARGUMENT(TSharedPtr<FWidgetReflectorNodeBase>, WidgetInfoToVisualize)

	SLATE_END_ARGS()

public:

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		this->WidgetInfo = InArgs._WidgetInfoToVisualize;
		check(WidgetInfo.IsValid());

		const FGeometry Geom = FGeometry().MakeChild(WidgetInfo->GetLocalSize(), WidgetInfo->GetAccumulatedLayoutTransform(), WidgetInfo->GetAccumulatedRenderTransform(), FVector2D::ZeroVector);
		SizeInfoText = FText::FromString(Geom.ToString());

		ChildSlot
		[
			SNew(SGridPanel)
				.FillColumn(1, 1.0f)

			// Desired Size
			+ SGridPanel::Slot(0, 0)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("DesiredSize", "Desired Size"))
				]

			// Desired Size Value
			+ SGridPanel::Slot(1, 0)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(this, &SReflectorToolTipWidget::GetWidgetsDesiredSize)
				]

			// Actual Size
			+ SGridPanel::Slot(0, 1)
				[
					SNew( STextBlock )
						.Text(LOCTEXT("ActualSize", "Actual Size"))
				]

			// Actual Size Value
			+ SGridPanel::Slot(1, 1)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(this, &SReflectorToolTipWidget::GetWidgetActualSize)
				]

			// Size Info
			+ SGridPanel::Slot(0, 2)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("SizeInfo", "Size Info"))
				]

			// Size Info Value
			+ SGridPanel::Slot(1, 2)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(this, &SReflectorToolTipWidget::GetSizeInfo)
				]

			// Enabled
			+ SGridPanel::Slot(0, 3)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("Enabled", "Enabled"))
				]

			// Enabled Value
			+ SGridPanel::Slot(1, 3)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(this, &SReflectorToolTipWidget::GetEnabled)
				]
		];
	}

private:

	FText GetWidgetsDesiredSize() const
	{
		return FText::FromString(WidgetInfo->GetWidgetDesiredSize().ToString());
	}

	FText GetWidgetActualSize() const
	{
		return FText::FromString(WidgetInfo->GetLocalSize().ToString());
	}

	FText GetSizeInfo() const
	{
		return SizeInfoText;
	}

	FText GetEnabled() const
	{
		static const FText TrueText = LOCTEXT("True", "True");
		static const FText FalseText = LOCTEXT("False", "False");
		return (WidgetInfo->GetWidgetEnabled()) ? TrueText : FalseText;
	}

private:

	/** The info about the widget that we are visualizing. */
	TSharedPtr<FWidgetReflectorNodeBase> WidgetInfo;

	/** The size info text */
	FText SizeInfoText;
};


#undef LOCTEXT_NAMESPACE
