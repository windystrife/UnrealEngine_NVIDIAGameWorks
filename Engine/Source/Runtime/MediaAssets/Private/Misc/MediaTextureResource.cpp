// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/MediaTextureResource.h"
#include "MediaAssetsPrivate.h"

#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "ExternalTexture.h"
#include "IMediaPlayer.h"
#include "IMediaSamples.h"
#include "IMediaTextureSample.h"
#include "MediaPlayerFacade.h"
#include "MediaSampleSource.h"
#include "MediaShaders.h"
#include "PipelineStateCache.h"
#include "SceneUtils.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "RenderUtils.h"
#include "RHIStaticStates.h"
#include "ExternalTexture.h"

#include "MediaTexture.h"


#define MEDIATEXTURERESOURCE_TRACE_RENDER 0


/* Local helpers
 *****************************************************************************/

namespace MediaTextureResource
{
	/**
	 * Get the pixel format for a given sample.
	 *
	 * @param Sample The sample.
	 * @return The sample's pixel format.
	 */
	EPixelFormat GetPixelFormat(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample)
	{
		switch (Sample->GetFormat())
		{
		case EMediaTextureSampleFormat::CharAYUV:
		case EMediaTextureSampleFormat::CharBGRA:
		case EMediaTextureSampleFormat::CharBMP:
		case EMediaTextureSampleFormat::CharUYVY:
		case EMediaTextureSampleFormat::CharYUY2:
		case EMediaTextureSampleFormat::CharYVYU:
			return PF_B8G8R8A8;

		case EMediaTextureSampleFormat::CharNV12:
		case EMediaTextureSampleFormat::CharNV21:
			return PF_G8;

		case EMediaTextureSampleFormat::FloatRGB:
			return PF_FloatRGB;

		case EMediaTextureSampleFormat::FloatRGBA:
			return PF_FloatRGBA;

		default:
			return PF_Unknown;
		}
	}


	/**
	 * Check whether the given sample requires a conversion shader.
	 *
	 * @param Sample The sample to check.
	 * @param SrgbOutput Whether the output is expected in sRGB color space.
	 * @return true if conversion is required, false otherwise.
	 * @see RequiresOutputResource
	 */
	bool RequiresConversion(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample, bool SrgbOutput)
	{
		// If the output color space is expected to be sRGB, but the
		// sample is not, a color space conversion on the GPU is required.

		if (Sample->IsOutputSrgb() != SrgbOutput)
		{
			return true;
		}

		// If the output dimensions are not the same as the sample's
		// dimensions, a resizing conversion on the GPU is required.

		if (Sample->GetDim() != Sample->GetOutputDim())
		{
			return true;
		}

		// Only the following pixel formats are supported natively.
		// All other formats require a conversion on the GPU.

		const EMediaTextureSampleFormat Format = Sample->GetFormat();

		return ((Format != EMediaTextureSampleFormat::CharBGRA) &&
				(Format != EMediaTextureSampleFormat::FloatRGB) &&
				(Format != EMediaTextureSampleFormat::FloatRGBA));
	}


	/**
	 * Check whether the given sample requires an sRGB texture.
	 *
	 * @param Sample The sample to check.
	 * @return true if an sRGB texture is required, false otherwise.
	 */
	bool RequiresSrgbTexture(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample)
	{
		if (!Sample->IsOutputSrgb())
		{
			return false;
		}

		const EMediaTextureSampleFormat Format = Sample->GetFormat();

		return ((Format == EMediaTextureSampleFormat::CharBGRA) ||
				(Format == EMediaTextureSampleFormat::CharBMP) ||
				(Format == EMediaTextureSampleFormat::FloatRGB) ||
				(Format == EMediaTextureSampleFormat::FloatRGBA));
	}
}


/* FMediaTextureResource structors
 *****************************************************************************/

FMediaTextureResource::FMediaTextureResource(UMediaTexture& InOwner, FIntPoint& InOwnerDim, SIZE_T& InOwnerSize)
	: Cleared(false)
	, CurrentClearColor(FLinearColor::Transparent)
	, Owner(InOwner)
	, OwnerDim(InOwnerDim)
	, OwnerSize(InOwnerSize)
{ }


/* FMediaTextureResource interface
 *****************************************************************************/

void FMediaTextureResource::Render(const FRenderParams& Params)
{
	check(IsInRenderingThread());

	TSharedPtr<FMediaTextureSampleSource, ESPMode::ThreadSafe> SampleSource = Params.SampleSource.Pin();

	if (SampleSource.IsValid())
	{
		// get the most current sample to be rendered
		TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;
		bool SampleValid = false;
		
		while (SampleSource->Peek(Sample) && Sample.IsValid())
		{
			const FTimespan StartTime = Sample->GetTime();
			const FTimespan EndTime = StartTime + Sample->GetDuration();

			if (((Params.Rate > 0.0f) && (StartTime >= Params.Time)) ||
				((Params.Rate < 0.0f) && (EndTime <= Params.Time)))
			{
				break; // future sample
			}

			SampleValid = SampleSource->Dequeue(Sample);
		}

		if (!SampleValid)
		{
			return; // no sample to render
		}

		check(Sample.IsValid());

		// render the sample
		if (Sample->GetOutputDim().GetMin() <= 0)
		{
			#if MEDIATEXTURERESOURCE_TRACE_RENDER
				UE_LOG(LogMediaAssets, VeryVerbose, TEXT("TextureResource %p: Corrupt sample with time %s at time %s"), this, *Sample->GetTime().ToString(), *Params.Time.ToString());
			#endif

			ClearTexture(FLinearColor::Red, Params.SrgbOutput); // mark corrupt sample
		}
		else if (MediaTextureResource::RequiresConversion(Sample, Params.SrgbOutput))
		{
			#if MEDIATEXTURERESOURCE_TRACE_RENDER
				UE_LOG(LogMediaAssets, VeryVerbose, TEXT("TextureResource %p: Converting sample with time %s at time %s"), this, *Sample->GetTime().ToString(), *Params.Time.ToString());
			#endif

			ConvertSample(Sample, Params.ClearColor, Params.SrgbOutput);
		}
		else
		{
			#if MEDIATEXTURERESOURCE_TRACE_RENDER
				UE_LOG(LogMediaAssets, VeryVerbose, TEXT("TextureResource %p: Copying sample with time %s at time %s"), this, *Sample->GetTime().ToString(), *Params.Time.ToString());
			#endif

			CopySample(Sample, Params.ClearColor, Params.SrgbOutput);
		}

		if (!GSupportsImageExternal && Params.PlayerGuid.IsValid())
		{
			FTextureRHIRef VideoTexture = (FTextureRHIRef)Owner.TextureReference.TextureReferenceRHI;
			FExternalTextureRegistry::Get().RegisterExternalTexture(Params.PlayerGuid, VideoTexture, SamplerStateRHI, Sample->GetScaleRotation(), Sample->GetOffset());
		}
	}
	else if (!Cleared)
	{
		#if MEDIATEXTURERESOURCE_TRACE_RENDER
			UE_LOG(LogMediaAssets, VeryVerbose, TEXT("TextureResource %p: Clearing texture at time %s"), this, *Params.Time.ToString());
		#endif

		ClearTexture(Params.ClearColor, Params.SrgbOutput);

		if (!GSupportsImageExternal && Params.PlayerGuid.IsValid())
		{
			FTextureRHIRef VideoTexture = (FTextureRHIRef)Owner.TextureReference.TextureReferenceRHI;
			FExternalTextureRegistry::Get().RegisterExternalTexture(Params.PlayerGuid, VideoTexture, SamplerStateRHI, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}
}


/* FRenderTarget interface
 *****************************************************************************/

FIntPoint FMediaTextureResource::GetSizeXY() const
{
	return FIntPoint(Owner.GetWidth(), Owner.GetHeight());
}


/* FTextureResource interface
 *****************************************************************************/

FString FMediaTextureResource::GetFriendlyName() const
{
	return Owner.GetPathName();
}


uint32 FMediaTextureResource::GetSizeX() const
{
	return Owner.GetWidth();
}


uint32 FMediaTextureResource::GetSizeY() const
{
	return Owner.GetHeight();
}


void FMediaTextureResource::InitDynamicRHI()
{
	// create the sampler state
	FSamplerStateInitializerRHI SamplerStateInitializer(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(&Owner),
		(Owner.AddressX == TA_Wrap) ? AM_Wrap : ((Owner.AddressX == TA_Clamp) ? AM_Clamp : AM_Mirror),
		(Owner.AddressY == TA_Wrap) ? AM_Wrap : ((Owner.AddressY == TA_Clamp) ? AM_Clamp : AM_Mirror),
		AM_Wrap
	);

	SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

	// Note: set up default texture, or we can get sampler bind errors on render
	// we can't leave here without having a valid bindable resource for some RHIs.

	ClearTexture(CurrentClearColor, Owner.SRGB);

	check(TextureRHI.IsValid());
	check(RenderTargetTextureRHI.IsValid());
	check(OutputTarget.IsValid());
}


void FMediaTextureResource::ReleaseDynamicRHI()
{
	Cleared = false;

	InputTarget.SafeRelease();
	OutputTarget.SafeRelease();
	RenderTargetTextureRHI.SafeRelease();
	TextureRHI.SafeRelease();

	UpdateTextureReference(nullptr);
}


/* FMediaTextureResource implementation
 *****************************************************************************/

void FMediaTextureResource::ClearTexture(const FLinearColor& ClearColor, bool SrgbOutput)
{
	// create output render target if we don't have one yet
	const uint32 OutputCreateFlags = TexCreate_Dynamic | (SrgbOutput ? TexCreate_SRGB : 0);

	if ((ClearColor != CurrentClearColor) || !OutputTarget.IsValid() || ((OutputTarget->GetFlags() & OutputCreateFlags) != OutputCreateFlags))
	{
		FRHIResourceCreateInfo CreateInfo = {
			FClearValueBinding(ClearColor)
		};

		TRefCountPtr<FRHITexture2D> DummyTexture2DRHI;

		RHICreateTargetableShaderResource2D(
			2,
			2,
			PF_B8G8R8A8,
			1,
			OutputCreateFlags,
			TexCreate_RenderTargetable,
			false,
			CreateInfo,
			OutputTarget,
			DummyTexture2DRHI
		);

		CurrentClearColor = ClearColor;
		UpdateResourceSize();
	}

	if (RenderTargetTextureRHI != OutputTarget)
	{
		UpdateTextureReference(OutputTarget);
	}

	// draw the clear color
	FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(RenderTargetTextureRHI, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		CommandList.SetRenderTargetsAndClear(Info);
		CommandList.SetViewport(0, 0, 0.0f, RenderTargetTextureRHI->GetSizeX(), RenderTargetTextureRHI->GetSizeY(), 1.0f);
		CommandList.TransitionResource(EResourceTransitionAccess::EReadable, RenderTargetTextureRHI);
	}

	Cleared = true;
}


void FMediaTextureResource::ConvertSample(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample, const FLinearColor& ClearColor, bool SrgbOutput)
{
	const EPixelFormat PixelFormat = MediaTextureResource::GetPixelFormat(Sample);

	// get input texture
	FRHITexture2D* InputTexture = nullptr;
	{
		// If the sample already provides a texture resource, we simply use that
		// as the input texture. If the sample only provides raw data, then we
		// create our own input render target and copy the data into it.

		FRHITexture* SampleTexture = Sample->GetTexture();
		FRHITexture2D* SampleTexture2D = (SampleTexture != nullptr) ? SampleTexture->GetTexture2D() : nullptr;

		if (SampleTexture2D != nullptr)
		{
			InputTexture = SampleTexture2D;

			InputTarget.SafeRelease();
			UpdateResourceSize();
		}
		else
		{
			const bool SrgbTexture = MediaTextureResource::RequiresSrgbTexture(Sample);
			const uint32 InputCreateFlags = TexCreate_Dynamic | (SrgbTexture ? TexCreate_SRGB : 0);
			const FIntPoint SampleDim = Sample->GetDim();

			// create a new input render target if necessary
			if (!InputTarget.IsValid() || (InputTarget->GetSizeXY() != SampleDim) || (InputTarget->GetFormat() != PixelFormat) || ((InputTarget->GetFlags() & InputCreateFlags) != InputCreateFlags))
			{
				TRefCountPtr<FRHITexture2D> DummyTexture2DRHI;
				FRHIResourceCreateInfo CreateInfo;

				RHICreateTargetableShaderResource2D(
					SampleDim.X,
					SampleDim.Y,
					PixelFormat,
					1,
					InputCreateFlags,
					TexCreate_RenderTargetable,
					false,
					CreateInfo,
					InputTarget,
					DummyTexture2DRHI
				);

				UpdateResourceSize();
			}

			// copy sample data to input render target
			FUpdateTextureRegion2D Region(0, 0, 0, 0, SampleDim.X, SampleDim.Y);
			RHIUpdateTexture2D(InputTarget, 0, Region, Sample->GetStride(), (uint8*)Sample->GetBuffer());

			InputTexture = InputTarget;
		}
	}

	// create output render target if necessary
	const uint32 OutputCreateFlags = TexCreate_Dynamic | (SrgbOutput ? TexCreate_SRGB : 0);
	const FIntPoint OutputDim = Sample->GetOutputDim();

	if ((ClearColor != CurrentClearColor) || !OutputTarget.IsValid() || (OutputTarget->GetSizeXY() != OutputDim) || (OutputTarget->GetFormat() != PF_B8G8R8A8) || ((OutputTarget->GetFlags() & OutputCreateFlags) != OutputCreateFlags))
	{
		TRefCountPtr<FRHITexture2D> DummyTexture2DRHI;
		
		FRHIResourceCreateInfo CreateInfo = {
			FClearValueBinding(ClearColor)
		};

		RHICreateTargetableShaderResource2D(
			OutputDim.X,
			OutputDim.Y,
			PF_B8G8R8A8,
			1,
			OutputCreateFlags,
			TexCreate_RenderTargetable,
			false,
			CreateInfo,
			OutputTarget,
			DummyTexture2DRHI
		);

		CurrentClearColor = ClearColor;
		UpdateResourceSize();
	}

	if (RenderTargetTextureRHI != OutputTarget)
	{
		UpdateTextureReference(OutputTarget);
	}

	// perform the conversion
	FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
	{
		SCOPED_DRAW_EVENT(CommandList, MediaTextureConvertResource);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		FRHITexture* RenderTarget = RenderTargetTextureRHI.GetReference();
		SetRenderTargets(CommandList, 1, &RenderTarget, nullptr, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop);

		CommandList.ApplyCachedRenderTargets(GraphicsPSOInit);
		CommandList.SetViewport(0, 0, 0.0f, OutputDim.X, OutputDim.Y, 1.0f);

		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

		// configure media shaders
		auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMediaVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);

		switch (Sample->GetFormat())
		{
		case EMediaTextureSampleFormat::CharAYUV:
		{
			TShaderMapRef<FAYUVConvertPS> ConvertShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
			SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
			ConvertShader->SetParameters(CommandList, InputTexture, MediaShaders::YuvToSrgbDefault, Sample->IsOutputSrgb());
		}
		break;

		case EMediaTextureSampleFormat::CharBMP:
		{
			TShaderMapRef<FBMPConvertPS> ConvertShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
			SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
			ConvertShader->SetParameters(CommandList, InputTexture, OutputDim, Sample->IsOutputSrgb() && !SrgbOutput);
		}
		break;

		case EMediaTextureSampleFormat::CharNV12:
		{
			TShaderMapRef<FNV12ConvertPS> ConvertShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
			SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
			ConvertShader->SetParameters(CommandList, InputTexture, OutputDim, MediaShaders::YuvToSrgbDefault, Sample->IsOutputSrgb());
		}
		break;

		case EMediaTextureSampleFormat::CharNV21:
		{
			TShaderMapRef<FNV21ConvertPS> ConvertShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
			SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
			ConvertShader->SetParameters(CommandList, InputTexture, OutputDim, MediaShaders::YuvToSrgbDefault, Sample->IsOutputSrgb());
		}
		break;

		case EMediaTextureSampleFormat::CharUYVY:
		{
			TShaderMapRef<FUYVYConvertPS> ConvertShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
			SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
			ConvertShader->SetParameters(CommandList, InputTexture, MediaShaders::YuvToSrgbDefault, Sample->IsOutputSrgb());
		}
		break;

		case EMediaTextureSampleFormat::CharYUY2:
		{
			TShaderMapRef<FYUY2ConvertPS> ConvertShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
			SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
			ConvertShader->SetParameters(CommandList, InputTexture, OutputDim, MediaShaders::YuvToSrgbDefault, Sample->IsOutputSrgb());
		}
		break;

		case EMediaTextureSampleFormat::CharYVYU:
		{
			TShaderMapRef<FYVYUConvertPS> ConvertShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
			SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
			ConvertShader->SetParameters(CommandList, InputTexture, MediaShaders::YuvToSrgbDefault, Sample->IsOutputSrgb());
		}
		break;

		case EMediaTextureSampleFormat::CharBGRA:
		case EMediaTextureSampleFormat::FloatRGB:
		case EMediaTextureSampleFormat::FloatRGBA:
		{
			TShaderMapRef<FRGBConvertPS> ConvertShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*ConvertShader);
			SetGraphicsPipelineState(CommandList, GraphicsPSOInit);
			ConvertShader->SetParameters(CommandList, InputTexture, OutputDim);
		}
		break;

		default:
			return; // unsupported format
		}

		// draw full size quad into render target
		FMediaElementVertex Vertices[4];
		{
			Vertices[0].Position.Set(-1.0f, 1.0f, 1.0f, 1.0f);
			Vertices[0].TextureCoordinate.Set(0.0f, 0.0f);
			Vertices[1].Position.Set(1.0f, 1.0f, 1.0f, 1.0f);
			Vertices[1].TextureCoordinate.Set(1.0f, 0.0f);
			Vertices[2].Position.Set(-1.0f, -1.0f, 1.0f, 1.0f);
			Vertices[2].TextureCoordinate.Set(0.0f, 1.0f);
			Vertices[3].Position.Set(1.0f, -1.0f, 1.0f, 1.0f);
			Vertices[3].TextureCoordinate.Set(1.0f, 1.0f);
		}

		CommandList.SetViewport(0, 0, 0.0f, OutputDim.X, OutputDim.Y, 1.0f);
		DrawPrimitiveUP(CommandList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));
		CommandList.TransitionResource(EResourceTransitionAccess::EReadable, RenderTargetTextureRHI);
	}

	Cleared = false;
}


void FMediaTextureResource::CopySample(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample, const FLinearColor& ClearColor, bool SrgbOutput)
{
	FRHITexture* SampleTexture = Sample->GetTexture();
	FRHITexture2D* SampleTexture2D = (SampleTexture != nullptr) ? SampleTexture->GetTexture2D() : nullptr;

	// If the sample already provides a texture resource, we simply use that
	// as the output render target. If the sample only provides raw data, then
	// we create our own output render target and copy the data into it.

	if (SampleTexture2D != nullptr)
	{
		// use sample's texture as the new render target.
		if (TextureRHI != SampleTexture2D)
		{
			UpdateTextureReference(SampleTexture2D);

			OutputTarget.SafeRelease();
			UpdateResourceSize();
		}
	}
	else
	{
		// create a new output render target if necessary
		const uint32 OutputCreateFlags = TexCreate_Dynamic | (SrgbOutput ? TexCreate_SRGB : 0);
		const EPixelFormat SampleFormat = MediaTextureResource::GetPixelFormat(Sample);
		const FIntPoint SampleDim = Sample->GetDim();

		if ((ClearColor != CurrentClearColor) || !OutputTarget.IsValid() || (OutputTarget->GetSizeXY() != SampleDim) || (OutputTarget->GetFormat() != SampleFormat) || ((OutputTarget->GetFlags() & OutputCreateFlags) != OutputCreateFlags))
		{
			TRefCountPtr<FRHITexture2D> DummyTexture2DRHI;

			FRHIResourceCreateInfo CreateInfo = {
				FClearValueBinding(ClearColor)
			};

			RHICreateTargetableShaderResource2D(
				SampleDim.X,
				SampleDim.Y,
				SampleFormat,
				1,
				OutputCreateFlags,
				TexCreate_RenderTargetable,
				false,
				CreateInfo,
				OutputTarget,
				DummyTexture2DRHI
			);

			CurrentClearColor = ClearColor;
			UpdateResourceSize();
		}

		if (RenderTargetTextureRHI != OutputTarget)
		{
			UpdateTextureReference(OutputTarget);
		}

		// copy sample data to output render target
		FUpdateTextureRegion2D Region(0, 0, 0, 0, SampleDim.X, SampleDim.Y);
		RHIUpdateTexture2D(RenderTargetTextureRHI.GetReference(), 0, Region, Sample->GetStride(), (uint8*)Sample->GetBuffer());
	}

	Cleared = false;
}


void FMediaTextureResource::UpdateResourceSize()
{
	SIZE_T ResourceSize = 0;

	if (InputTarget.IsValid())
	{
		ResourceSize += CalcTextureSize(InputTarget->GetSizeX(), InputTarget->GetSizeY(), InputTarget->GetFormat(), 1);
	}

	if (OutputTarget.IsValid())
	{
		ResourceSize += CalcTextureSize(OutputTarget->GetSizeX(), OutputTarget->GetSizeY(), OutputTarget->GetFormat(), 1);
	}

	OwnerSize = ResourceSize;
}


void FMediaTextureResource::UpdateTextureReference(FRHITexture2D* NewTexture)
{
	TextureRHI = NewTexture;
	RenderTargetTextureRHI = NewTexture;

	RHIUpdateTextureReference(Owner.TextureReference.TextureReferenceRHI, NewTexture);

	if (RenderTargetTextureRHI != nullptr)
	{
		OwnerDim = FIntPoint(RenderTargetTextureRHI->GetSizeX(), RenderTargetTextureRHI->GetSizeY());
	}
	else
	{
		OwnerDim = FIntPoint::ZeroValue;
	}
}
