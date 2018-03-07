// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkyLightComponentDetails.h"
#include "Components/SceneComponent.h"
#include "Engine/SkyLight.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Components/LightComponentBase.h"
#include "Engine/World.h"
#include "Components/SkyLightComponent.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "SkyLightComponentDetails"



TSharedRef<IDetailCustomization> FSkyLightComponentDetails::MakeInstance()
{
	return MakeShareable( new FSkyLightComponentDetails );
}

void FSkyLightComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	// Mobility property is on the scene component base class not the light component and that is why we have to use USceneComponent::StaticClass
	TSharedRef<IPropertyHandle> MobilityHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USkyLightComponent, Mobility), USceneComponent::StaticClass());
	// Set a mobility tooltip specific to lights
	MobilityHandle->SetToolTipText(LOCTEXT("SkyLightMobilityTooltip", "Mobility for sky light components determines what rendering methods will be used.  A Stationary sky light has its shadowing baked into Bent Normal AO by Lightmass, but its lighting can be changed in game."));

	TSharedPtr<IPropertyHandle> LightIntensityProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USkyLightComponent, Intensity), ULightComponentBase::StaticClass());

	if( LightIntensityProperty->IsValidHandle() )
	{
		// Point lights need to override the ui min and max for units of lumens, so we have to undo that
		LightIntensityProperty->SetInstanceMetaData("UIMin", TEXT("0.0f"));
		LightIntensityProperty->SetInstanceMetaData("UIMax", TEXT("20.0f"));
	}

	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetSelectedObjects();

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if ( CurrentObject.IsValid() )
		{
			ASkyLight* CurrentCaptureActor = Cast<ASkyLight>(CurrentObject.Get());
			if (CurrentCaptureActor != NULL)
			{
				SkyLight = CurrentCaptureActor;
				break;
			}
		}
	}

	DetailLayout.EditCategory("Light", FText::GetEmpty(), ECategoryPriority::TypeSpecific);

	// The bVisible checkbox in the rendering category is frequently used on lights
	// Editing the rendering category and giving it TypeSpecific priority will place it just under the Light category
	DetailLayout.EditCategory("Rendering", FText::GetEmpty(), ECategoryPriority::TypeSpecific);

	DetailLayout.EditCategory( "SkyLight" )
	.AddCustomRow( NSLOCTEXT("SkyLightDetails", "UpdateSkyLight", "Recapture Scene") )
		.NameContent()
		[
			SNew( STextBlock )
			.Font( IDetailLayoutBuilder::GetDetailFont() )
			.Text( NSLOCTEXT("SkyLightDetails", "UpdateSkyLight", "Recapture Scene") )
		]
		.ValueContent()
		.MaxDesiredWidth(125.f)
		.MinDesiredWidth(125.f)
		[
			SNew(SButton)
			.ContentPadding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked( this, &FSkyLightComponentDetails::OnUpdateSkyCapture )
			[
				SNew( STextBlock )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
				.Text( NSLOCTEXT("SkyLightDetails", "UpdateSkyCapture", "Recapture") )
			]
		];
}

FReply FSkyLightComponentDetails::OnUpdateSkyCapture()
{
	if (SkyLight.IsValid())
	{
		if (UWorld* SkyLightWorld = SkyLight->GetWorld())
		{
			SkyLightWorld->UpdateAllSkyCaptures();
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
