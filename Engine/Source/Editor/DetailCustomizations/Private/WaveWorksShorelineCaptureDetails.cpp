/*
* This code contains NVIDIA Confidential Information and is disclosed
* under the Mutual Non-Disclosure Agreement.
*
* Notice
* ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES
* NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
* THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
*
* NVIDIA Corporation assumes no responsibility for the consequences of use of such
* information or for any infringement of patents or other rights of third parties that may
* result from its use. No license is granted by implication or otherwise under any patent
* or patent rights of NVIDIA Corporation. No third party distribution is allowed unless
* expressly authorized by NVIDIA.  Details are subject to change without notice.
* This code supersedes and replaces all information previously supplied.
* NVIDIA Corporation products are not authorized for use as critical
* components in life support devices or systems without express written approval of
* NVIDIA Corporation.
*
* Copyright ?2008- 2017 NVIDIA Corporation. All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software and related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is
* strictly prohibited.
*/

#include "WaveWorksShorelineCaptureDetails.h"
#include "ShowFlags.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Components/SceneCaptureComponent.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SceneCaptureCube.h"
#include "Engine/WaveWorksShorelineCapture.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "Engine/PlanarReflection.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Components/PlanarReflectionComponent.h"
#include "Components/WaveWorksShorelineCaptureComponent.h"

#define LOCTEXT_NAMESPACE "WaveWorksShorelineCaptureDetails"

TSharedRef<IDetailCustomization> FWaveWorksShorelineCaptureDetails::MakeInstance()
{
	return MakeShareable( new FWaveWorksShorelineCaptureDetails);
}

void FWaveWorksShorelineCaptureDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	DetailLayout.HideCategory( TEXT("Rendering") );
	DetailLayout.HideCategory( TEXT("ColorGrading") );
	DetailLayout.HideCategory( TEXT("Tonemapper") );
	DetailLayout.HideCategory( TEXT("Lens") );
	DetailLayout.HideCategory( TEXT("RenderingFeatures") );
	//DetailLayout.HideCategory(TEXT("Projection"));
	DetailLayout.HideCategory( TEXT("PostProcessVolume") );


	//DetailLayout.HideProperty(TEXT("FOVAngle"));

	// Add all the properties that are there by default
	// (These would get added by default anyway, but we want to add them first so what we add next comes later in the list)
	TArray<TSharedRef<IPropertyHandle>> SceneCaptureCategoryDefaultProperties;
	IDetailCategoryBuilder& SceneCaptureCategoryBuilder = DetailLayout.EditCategory("SceneCapture");
	SceneCaptureCategoryBuilder.GetDefaultProperties(SceneCaptureCategoryDefaultProperties);

	for (TSharedRef<IPropertyHandle> Handle : SceneCaptureCategoryDefaultProperties)
	{
		SceneCaptureCategoryBuilder.AddProperty(Handle);
	}

	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetSelectedObjects();
	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if ( CurrentObject.IsValid() )
		{
			AWaveWorksShorelineCapture* CurrentCaptureActor = Cast<AWaveWorksShorelineCapture>(CurrentObject.Get());
			if (CurrentCaptureActor != nullptr)
			{
				WaveWorksShorelineCaptureComponent = Cast<UWaveWorksShorelineCaptureComponent>(CurrentCaptureActor->GetWaveWorksShorelineCaptureComponent());
				break;
			}
		}
	}

	// add build distance field button
	DetailLayout.EditCategory("SceneCapture")
		.AddCustomRow(NSLOCTEXT("WaveWorksShorelineCaptureDetails", "WaveWorksCapture", "Capture Scene"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("WaveWorksShorelineCaptureDetails", "WaveWorksCapture", "Capture Scene"))
		]
		.ValueContent()
		.MaxDesiredWidth(125.f)
		.MinDesiredWidth(125.f)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FWaveWorksShorelineCaptureDetails::OnWaveWorksShorelineCapture)
			.Text(LOCTEXT("WaveWorksCapture", "Capture"))
		];
}

FReply FWaveWorksShorelineCaptureDetails::OnWaveWorksShorelineCapture()
{
	WaveWorksShorelineCaptureComponent->GenerateShorelineDFTexture();

	return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE
