// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MediaTexture.h"
#include "MediaAssetsPrivate.h"

#include "ExternalTexture.h"
#include "IMediaClock.h"
#include "IMediaClockSink.h"
#include "IMediaModule.h"
#include "MediaPlayerFacade.h"
#include "Modules/ModuleManager.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "MediaPlayer.h"
#include "Misc/MediaTextureResource.h"


/* Local helpers
 *****************************************************************************/

/**
 * Media clock sink for media textures.
 */
class FMediaTextureClockSink
	: public IMediaClockSink
{
public:

	FMediaTextureClockSink(UMediaTexture& InOwner)
		: Owner(&InOwner)
	{ }

	virtual ~FMediaTextureClockSink() { }

public:

	virtual void TickRender(FTimespan DeltaTime, FTimespan Timecode) override
	{
		if (Owner.IsValid())
		{
			Owner->TickResource(Timecode);
		}
	}

private:

	TWeakObjectPtr<UMediaTexture> Owner;
};


/* UMediaTexture structors
 *****************************************************************************/

UMediaTexture::UMediaTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AddressX(TA_Clamp)
	, AddressY(TA_Clamp)
	, AutoClear(true)
	, ClearColor(FLinearColor::Black)
	, MediaPlayer(nullptr)
	, Dimensions(FIntPoint::ZeroValue)
	, Size(0)
{
	NeverStream = true;
}


/* UMediaTexture interface
 *****************************************************************************/

float UMediaTexture::GetAspectRatio() const
{
	if (Dimensions.Y == 0)
	{
		return 0.0f;
	}

	return (float)(Dimensions.X) / Dimensions.Y;
}


int32 UMediaTexture::GetHeight() const
{
	return Dimensions.Y;
}


int32 UMediaTexture::GetWidth() const
{
	return Dimensions.X;
}


/* UTexture interface
 *****************************************************************************/

FTextureResource* UMediaTexture::CreateResource()
{
	if (!ClockSink.IsValid())
	{
		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			ClockSink = MakeShared<FMediaTextureClockSink, ESPMode::ThreadSafe>(*this);
			MediaModule->GetClock().AddSink(ClockSink.ToSharedRef());
		}
	}

	return new FMediaTextureResource(*this, Dimensions, Size);
}


EMaterialValueType UMediaTexture::GetMaterialType() const
{
	return MCT_TextureExternal;
}


float UMediaTexture::GetSurfaceWidth() const
{
	return Dimensions.X;
}


float UMediaTexture::GetSurfaceHeight() const
{
	return Dimensions.Y;
}


FGuid UMediaTexture::GetExternalTextureGuid() const
{
	return MediaPlayer ? MediaPlayer->GetGuid() : FGuid();
}


/* UObject interface
 *****************************************************************************/

void UMediaTexture::BeginDestroy()
{
	if (ClockSink.IsValid())
	{
		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			MediaModule->GetClock().RemoveSink(ClockSink.ToSharedRef());
		}

		ClockSink.Reset();
	}

	UnregisterPlayerGuid();

	Super::BeginDestroy();
}


FString UMediaTexture::GetDesc()
{
	return FString::Printf(TEXT("%ix%i [%s]"), Dimensions.X,  Dimensions.Y, GPixelFormats[PF_B8G8R8A8].Name);
}


void UMediaTexture::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	CumulativeResourceSize.AddUnknownMemoryBytes(Size);
}


#if WITH_EDITOR

void UMediaTexture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName AddressXName = GET_MEMBER_NAME_CHECKED(UMediaTexture, AddressX);
	static const FName AddressYName = GET_MEMBER_NAME_CHECKED(UMediaTexture, AddressY);
	static const FName AutoClearName = GET_MEMBER_NAME_CHECKED(UMediaTexture, AutoClear);
	static const FName ClearColorName = GET_MEMBER_NAME_CHECKED(UMediaTexture, ClearColor);
	static const FName MediaPlayerName = GET_MEMBER_NAME_CHECKED(UMediaTexture, MediaPlayer);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	
	if (PropertyThatChanged == nullptr)
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);

		return;
	}

	const FName PropertyName = PropertyThatChanged->GetFName();

	// don't update resource for these properties
	if ((PropertyName == AutoClearName) ||
		(PropertyName == ClearColorName) ||
		(PropertyName == MediaPlayerName))
	{
		UObject::PostEditChangeProperty(PropertyChangedEvent);

		return;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	// notify materials for these properties
	if ((PropertyName == AddressXName) ||
		(PropertyName == AddressYName))
	{
		NotifyMaterials();
	}
}

#endif // WITH_EDITOR


/* UMediaTexture implementation
 *****************************************************************************/

void UMediaTexture::TickResource(FTimespan Timecode)
{
	if ((MediaPlayer == nullptr) || (Resource == nullptr))
	{
		CurrentPlayerFacade.Reset();
		SampleQueue.Reset();

		return;
	}

	// create a new sample queue if the player changed
	TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe> PlayerFacade = MediaPlayer->GetPlayerFacade();

	if (PlayerFacade != CurrentPlayerFacade)
	{
		SampleQueue = MakeShared<FMediaTextureSampleQueue, ESPMode::ThreadSafe>();
		PlayerFacade->AddVideoSampleSink(SampleQueue.ToSharedRef());
		CurrentPlayerFacade = PlayerFacade;
	}

	check(SampleQueue.IsValid());

	// unregister previous external texture GUID if needed
	const FGuid PlayerGuid = MediaPlayer->GetGuid();

	if (PlayerGuid != LastPlayerGuid)
	{
		UnregisterPlayerGuid();
		LastPlayerGuid = PlayerGuid;
	}

	// retain the last rendered frame if player is inactive
	const bool PlayerActive = MediaPlayer->IsPaused() || MediaPlayer->IsPlaying() || MediaPlayer->IsPreparing();

	if (!PlayerActive && !AutoClear)
	{
		return;
	}

	// issue a render command to render the current sample
	FMediaTextureResource::FRenderParams RenderParams;
	{
		RenderParams.ClearColor = ClearColor;
		RenderParams.PlayerGuid = PlayerGuid;
		RenderParams.Rate = MediaPlayer->GetRate();
		RenderParams.SrgbOutput = SRGB;
		RenderParams.Time = MediaPlayer->GetTime();

		if (PlayerActive)
		{
			RenderParams.SampleSource = SampleQueue;
		}
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(MediaTextureResourceRender,
		FMediaTextureResource*, ResourceParam, (FMediaTextureResource*)Resource,
		FMediaTextureResource::FRenderParams, RenderParamsParam, RenderParams,
		{
			ResourceParam->Render(RenderParamsParam);
		});
}


void UMediaTexture::UnregisterPlayerGuid()
{
	if (!LastPlayerGuid.IsValid())
	{
		return;
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(MediaTextureUnregisterPlayerGuid,
		FGuid, PlayerGuid, LastPlayerGuid,
		{
			FExternalTextureRegistry::Get().UnregisterExternalTexture(PlayerGuid);
		});
}
