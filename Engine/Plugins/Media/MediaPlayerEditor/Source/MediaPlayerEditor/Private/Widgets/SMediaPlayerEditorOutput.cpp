// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SMediaPlayerEditorOutput.h"

#include "AudioDevice.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "IMediaEventSink.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"


/* SMediaPlayerEditorOutput structors
 *****************************************************************************/

SMediaPlayerEditorOutput::SMediaPlayerEditorOutput()
	: Material(nullptr)
	, MediaPlayer(nullptr)
	, MediaTexture(nullptr)
	, SoundComponent(nullptr)
{ }


SMediaPlayerEditorOutput::~SMediaPlayerEditorOutput()
{
	if (MediaPlayer.IsValid())
	{
		MediaPlayer->OnMediaEvent().RemoveAll(this);
		MediaPlayer.Reset();
	}

	if (Material != nullptr)
	{
		Material->RemoveFromRoot();
		Material = nullptr;
	}

	if (MediaTexture != nullptr)
	{
		MediaTexture->RemoveFromRoot();
		MediaTexture = nullptr;
	}

	if (SoundComponent != nullptr)
	{
		SoundComponent->Stop();
		SoundComponent->RemoveFromRoot();
		SoundComponent = nullptr;
	}
}


/* SMediaPlayerEditorOutput interface
 *****************************************************************************/

void SMediaPlayerEditorOutput::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer)
{
	MediaPlayer = &InMediaPlayer;

	// create media sound component
	if ((GEngine != nullptr) && GEngine->UseSound())
	{
		SoundComponent = NewObject<UMediaSoundComponent>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

		if (SoundComponent != nullptr)
		{
			SoundComponent->MediaPlayer = &InMediaPlayer;
			SoundComponent->bIsUISound = true;
			SoundComponent->Initialize();
			SoundComponent->AddToRoot();
		}
	}

	// create media texture
	MediaTexture = NewObject<UMediaTexture>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

	if (MediaTexture != nullptr)
	{
		MediaTexture->MediaPlayer = &InMediaPlayer;
		MediaTexture->UpdateResource();
		MediaTexture->AddToRoot();
	}

	// create wrapper material
	Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

	if (Material != nullptr)
	{
		TextureSampler = NewObject<UMaterialExpressionTextureSample>(Material);
		{
			TextureSampler->Texture = MediaTexture;
			TextureSampler->AutoSetSampleType();
		}

		FExpressionOutput& Output = TextureSampler->GetOutputs()[0];
		FExpressionInput& Input = Material->EmissiveColor;
		{
			Input.Expression = TextureSampler;
			Input.Mask = Output.Mask;
			Input.MaskR = Output.MaskR;
			Input.MaskG = Output.MaskG;
			Input.MaskB = Output.MaskB;
			Input.MaskA = Output.MaskA;
		}

		Material->Expressions.Add(TextureSampler);
		Material->MaterialDomain = EMaterialDomain::MD_UI;
		Material->PostEditChange();
		Material->AddToRoot();
	}

	// create Slate brush
	MaterialBrush = MakeShareable(new FSlateBrush());
	{
		MaterialBrush->SetResourceObject(Material);
	}

	ChildSlot
	[
		SNew(SImage)
			.Image(MaterialBrush.Get())
	];

	MediaPlayer->OnMediaEvent().AddRaw(this, &SMediaPlayerEditorOutput::HandleMediaPlayerMediaEvent);
}


/* SWidget interface
 *****************************************************************************/

void SMediaPlayerEditorOutput::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (MediaTexture != nullptr)
	{
		MaterialBrush->ImageSize.X = MediaTexture->GetSurfaceWidth();
		MaterialBrush->ImageSize.Y = MediaTexture->GetSurfaceHeight();
	}
	else
	{
		MaterialBrush->ImageSize = FVector2D::ZeroVector;
	}

	if (SoundComponent != nullptr)
	{
		SoundComponent->UpdatePlayer();
	}
}


/* SMediaPlayerEditorOutput callbacks
 *****************************************************************************/

void SMediaPlayerEditorOutput::HandleMediaPlayerMediaEvent(EMediaEvent Event)
{
	if (SoundComponent == nullptr)
	{
		return;
	}

	if (Event == EMediaEvent::PlaybackSuspended)
	{
		SoundComponent->Stop();
	}
	else if (Event == EMediaEvent::PlaybackResumed)
	{
		if (GEditor->PlayWorld == nullptr)
		{
			SoundComponent->Start();
		}
	}
}
