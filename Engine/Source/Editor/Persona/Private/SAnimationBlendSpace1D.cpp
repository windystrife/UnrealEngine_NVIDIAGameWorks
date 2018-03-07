// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAnimationBlendSpace1D.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "BlendSpace1DEditor"

void SBlendSpaceEditor1D::Construct(const FArguments& InArgs, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo)
{
	SBlendSpaceEditorBase::Construct(SBlendSpaceEditorBase::FArguments()
		.BlendSpace(InArgs._BlendSpace1D),
		InPreviewScene,
		OnPostUndo );
}

void SBlendSpaceEditor1D::ResampleData()
{
	const FBlendParameter& BlendParameter = BlendSpace->GetBlendParameter(0);
	ElementGenerator.Init(BlendParameter);

	const TArray<FBlendSample>& BlendSamples = BlendSpace->GetBlendSamples();
	if (BlendSamples.Num())
	{
		for (const FBlendSample& Sample : BlendSamples)
		{
			// Add X value from sample (this is the only valid value to be set for 1D blend spaces / aim offsets
			if (Sample.bIsValid)
			{
				ElementGenerator.SamplePointList.Add(Sample.SampleValue.X);
			}
		}
		ElementGenerator.CalculateEditorElements();

		// Create point to sample index list
		TArray<int32> PointListToSampleIndices;
		PointListToSampleIndices.Init(INDEX_NONE, ElementGenerator.SamplePointList.Num());
		for (int32 PointIndex = 0; PointIndex < ElementGenerator.SamplePointList.Num(); ++PointIndex)
		{
			const float Point = ElementGenerator.SamplePointList[PointIndex];
			for (int32 SampleIndex = 0; SampleIndex < BlendSamples.Num(); ++SampleIndex)
			{
				if (BlendSamples[SampleIndex].SampleValue.X == Point)
				{
					PointListToSampleIndices[PointIndex] = SampleIndex;
					break;
				}
			}
		}

		BlendSpace->FillupGridElements(PointListToSampleIndices, ElementGenerator.EditorElements);
	}
}

#undef LOCTEXT_NAMESPACE
