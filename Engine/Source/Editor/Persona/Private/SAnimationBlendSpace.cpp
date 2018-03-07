// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAnimationBlendSpace.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "BlendSpaceEditor"

void SBlendSpaceEditor::Construct(const FArguments& InArgs, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo)
{
	SBlendSpaceEditorBase::Construct(SBlendSpaceEditorBase::FArguments()
									.BlendSpace(InArgs._BlendSpace),
									InPreviewScene,
									OnPostUndo );
}

void SBlendSpaceEditor::ResampleData()
{
	// clear first
	BlendSpaceGrid.Reset();
	Generator.Reset();

	// you don't like to overwrite the link here (between visible points vs sample points, 
	// so allow this if no triangle is generated
	const FBlendParameter& BlendParamX = BlendSpace->GetBlendParameter(0);
	const FBlendParameter& BlendParamY = BlendSpace->GetBlendParameter(1);
	BlendSpaceGrid.SetGridInfo(BlendParamX, BlendParamY);
	Generator.SetGridBox(BlendParamX, BlendParamY);

	BlendSpace->EmptyGridElements();

	if (BlendSpace->GetNumberOfBlendSamples())
	{
		bool bAllSamplesValid = true;
		for (int32 SampleIndex = 0; SampleIndex < BlendSpace->GetNumberOfBlendSamples(); ++SampleIndex)
		{
			const FBlendSample& Sample = BlendSpace->GetBlendSample(SampleIndex);

			// Do not add invalid sample points (user will need to correct them to be incorporated into the blendspace)
			if (Sample.bIsValid)
			{				
				Generator.AddSamplePoint(Sample.SampleValue, SampleIndex);
			}
		}		
		
		// triangulate
		Generator.Triangulate();

		// once triangulated, generate grid
		const TArray<FPoint>& Points = Generator.GetSamplePointList();
		const TArray<FTriangle*>& Triangles = Generator.GetTriangleList();
		BlendSpaceGrid.GenerateGridElements(Points, Triangles);

		// now fill up grid elements in BlendSpace using this Element information
		if (Triangles.Num() > 0)
		{
			const TArray<FEditorElement>& GridElements = BlendSpaceGrid.GetElements();
			BlendSpace->FillupGridElements(Generator.GetIndiceMapping(), GridElements);
		}
	}
}

#undef LOCTEXT_NAMESPACE
