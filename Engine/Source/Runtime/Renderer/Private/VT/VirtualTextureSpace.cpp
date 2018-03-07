// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureSpace.h"

#include "VirtualTextureSystem.h"
#include "SpriteIndexBuffer.h"
#include "SceneFilterRendering.h"
#include "RenderTargetPool.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "HAL/IConsoleManager.h"
#include "SceneUtils.h"

static TAutoConsoleVariable<int32> CVarVTRefreshEntirePageTable(
	TEXT("r.VT.RefreshEntirePageTable"),
	0,
	TEXT("Refreshes the entire page table texture every frame"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarVTMaskedPageTableUpdates(
	TEXT("r.VT.MaskedPageTableUpdates"),
	1,
	TEXT("Masks the page table update quads to reduce pixel fill costs"),
	ECVF_RenderThreadSafe
	);


FVirtualTextureSpace::FVirtualTextureSpace( uint32 InSize, uint8 InDimensions, EPixelFormat InFormat, FTexturePagePool* InPool )
	: ID( 0xff )
	, Dimensions( InDimensions )
	, Pool( InPool )
	, Allocator( InSize, InDimensions )
{
	PageTableSize = InSize;
	PageTableLevels = FMath::FloorLog2( PageTableSize ) + 1;
	PageTableFormat = InFormat;

	GVirtualTextureSystem.RegisterSpace( this );
}

FVirtualTextureSpace::~FVirtualTextureSpace()
{
	GVirtualTextureSystem.UnregisterSpace( this );
}

void FVirtualTextureSpace::InitDynamicRHI()
{
	{
		// Page Table
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( FIntPoint( PageTableSize, PageTableSize ), PageTableFormat, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_ShaderResource, false, PageTableLevels ) );
		GRenderTargetPool.FindFreeElement( RHICmdList, Desc, PageTable, TEXT("PageTable") );
	}

	{
		// Worst case scenario of max mip page update when all other pages are perfectly sparse at mip 0.
		const uint32 MaxSparseRegions = Pool->GetSize();
		const uint32 SparseRegionSize = PageTableSize / (uint32)FMath::Pow( (float)MaxSparseRegions, 1.0f / Dimensions );
		const uint32 PerRegionMaxExpansion = ( (1 << Dimensions) - 1 ) * FMath::FloorLog2( SparseRegionSize );
		const uint32 MaxExpansionMip0 = PerRegionMaxExpansion * MaxSparseRegions;
		const uint32 Mip0Updates = 64;
		const uint32 MaxUpdates = MaxExpansionMip0 + (PageTableLevels - 1) + Mip0Updates;

		// Update Buffer
		FRHIResourceCreateInfo CreateInfo;
		UpdateBuffer = RHICreateStructuredBuffer( sizeof( FPageTableUpdate ), MaxUpdates * sizeof( FPageTableUpdate ), BUF_ShaderResource | BUF_Volatile, CreateInfo );
		UpdateBufferSRV = RHICreateShaderResourceView( UpdateBuffer );
	}
}

void FVirtualTextureSpace::ReleaseDynamicRHI()
{
	GRenderTargetPool.FreeUnusedResource( PageTable );

	UpdateBuffer.SafeRelease();
	UpdateBufferSRV.SafeRelease();
}

void FVirtualTextureSpace::QueueUpdate( uint8 vLogSize, uint64 vAddress, uint8 vLevel, uint16 pAddress )
{
	FPageUpdate Update;
	Update.vAddress	= vAddress;
	Update.pAddress	= pAddress;
	Update.vLevel	= vLevel;
	Update.vLogSize	= vLogSize;
	Update.Check( Dimensions );

	PageTableUpdates.Add( Update );
}


TGlobalResource< FSpriteIndexBuffer<8> > GQuadIndexBuffer;

class FPageTableUpdateVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPageTableUpdateVS,Global);

	FPageTableUpdateVS() {}
	
public:
	FShaderParameter			PageTableSize;
	FShaderParameter			FirstUpdate;
	FShaderParameter			NumUpdates;
	FShaderResourceParameter	UpdateBuffer;

	FPageTableUpdateVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PageTableSize.Bind( Initializer.ParameterMap, TEXT("PageTableSize") );
		FirstUpdate.Bind( Initializer.ParameterMap, TEXT("FirstUpdate") );
		NumUpdates.Bind( Initializer.ParameterMap, TEXT("NumUpdates") );
		UpdateBuffer.Bind( Initializer.ParameterMap, TEXT("UpdateBuffer") );
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsHlslccShaderPlatform(Platform);
	}

	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PageTableSize;
		Ar << FirstUpdate;
		Ar << NumUpdates;
		Ar << UpdateBuffer;
		return bShaderHasOutdatedParameters;
	}
};

class FPageTableUpdatePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPageTableUpdatePS, Global);

	FPageTableUpdatePS() {}

public:
	FPageTableUpdatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
	
	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported( Platform, ERHIFeatureLevel::SM5 ) && !IsHlslccShaderPlatform(Platform);
	}
};

template< uint32 Format >
class TPageTableUpdateVS : public FPageTableUpdateVS
{
	DECLARE_SHADER_TYPE(TPageTableUpdateVS,Global);

	TPageTableUpdateVS() {}

public:
	TPageTableUpdateVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPageTableUpdateVS(Initializer)
	{}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("PAGE_TABLE_FORMAT"), Format );
	}
};

template< uint32 Format >
class TPageTableUpdatePS : public FPageTableUpdatePS
{
	DECLARE_SHADER_TYPE(TPageTableUpdatePS, Global);

	TPageTableUpdatePS() {}

public:
	TPageTableUpdatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPageTableUpdatePS(Initializer)
	{}
	
	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("PAGE_TABLE_FORMAT"), Format );
		OutEnvironment.SetRenderTargetOutputFormat( 0, Format == 0 ? PF_R16_UINT : PF_R8G8B8A8 );
	}
};

IMPLEMENT_SHADER_TYPE( template<>, TPageTableUpdateVS<0>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdateVS"), SF_Vertex );
IMPLEMENT_SHADER_TYPE( template<>, TPageTableUpdateVS<1>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdateVS"), SF_Vertex );
IMPLEMENT_SHADER_TYPE( template<>, TPageTableUpdatePS<0>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdatePS"), SF_Pixel );
IMPLEMENT_SHADER_TYPE( template<>, TPageTableUpdatePS<1>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdatePS"), SF_Pixel );


void FVirtualTextureSpace::ApplyUpdates( FRHICommandList& RHICmdList )
{
	static TArray< FPageTableUpdate > ExpandedUpdates[16];

	if( CVarVTRefreshEntirePageTable.GetValueOnRenderThread() )
	{
		Pool->RefreshEntirePageTable( ID, ExpandedUpdates );
	}
	else
	{
		if( PageTableUpdates.Num() == 0 )
		{
			GRenderTargetPool.VisualizeTexture.SetCheckPoint( RHICmdList, PageTable );
			return;
		}

		for( auto& Update : PageTableUpdates )
		{
			if( CVarVTMaskedPageTableUpdates.GetValueOnRenderThread() )
			{
				Pool->ExpandPageTableUpdateMasked( ID, Update, ExpandedUpdates );
			}
			else
			{
				Pool->ExpandPageTableUpdatePainters( ID, Update, ExpandedUpdates );
			}
		}
	}

	PageTableUpdates.Reset();

	// TODO Expand 3D updates for slices of volume texture

	uint32 TotalNumUpdates = 0;
	for( uint32 Mip = 0; Mip < PageTableLevels; Mip++ )
	{
		TotalNumUpdates += ExpandedUpdates[ Mip ].Num();
	}

	if( TotalNumUpdates * sizeof( FPageTableUpdate ) > UpdateBuffer->GetSize() )
	{
		// Resize Update Buffer
		uint32 MaxUpdates = FMath::RoundUpToPowerOfTwo( TotalNumUpdates );

		FRHIResourceCreateInfo CreateInfo;
		UpdateBuffer = RHICreateStructuredBuffer( sizeof( FPageUpdate ), MaxUpdates * sizeof( FPageTableUpdate ), BUF_ShaderResource | BUF_Volatile, CreateInfo );
		UpdateBufferSRV = RHICreateShaderResourceView( UpdateBuffer );
	}
	
	// This flushes the RHI thread!
	uint8* Buffer = (uint8*)RHILockStructuredBuffer( UpdateBuffer, 0, TotalNumUpdates * sizeof( FPageTableUpdate ), RLM_WriteOnly );

	for( uint32 Mip = 0; Mip < PageTableLevels; Mip++ )
	{
		uint32 NumUpdates = ExpandedUpdates[ Mip ].Num();
		if( NumUpdates )
		{
			size_t UploadSize = NumUpdates * sizeof( FPageTableUpdate );
			FMemory::Memcpy( Buffer, ExpandedUpdates[ Mip ].GetData(), UploadSize );
			Buffer += UploadSize;
		}
	}
	
	RHIUnlockStructuredBuffer( UpdateBuffer );

	// Draw
	SCOPED_DRAW_EVENT( RHICmdList, PageTableUpdate );

	auto ShaderMap = GetGlobalShaderMap( GetFeatureLevel() );

	FSceneRenderTargetItem& PageTableTarget = PageTable->GetRenderTargetItem();

	uint32 FirstUpdate = 0;
	uint32 MipSize = PageTableSize;
	for( uint32 Mip = 0; Mip < PageTableLevels; Mip++ )
	{
		uint32 NumUpdates = ExpandedUpdates[ Mip ].Num();
		if( NumUpdates )
		{
			SetRenderTarget( RHICmdList, PageTableTarget.TargetableTexture, Mip, NULL );
			RHICmdList.SetViewport( 0, 0, 0.0f, MipSize, MipSize, 1.0f );

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets( GraphicsPSOInit );

			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			FPageTableUpdateVS* VertexShader = nullptr;
			FPageTableUpdatePS* PixelShader = nullptr;

			switch( PageTableFormat )
			{
			case PF_R16_UINT:
				{
					VertexShader = *TShaderMapRef< TPageTableUpdateVS<0> >( ShaderMap );
					PixelShader  = *TShaderMapRef< TPageTableUpdatePS<0> >( ShaderMap );
				}
				break;
			case PF_R8G8B8A8:
				{
					VertexShader = *TShaderMapRef< TPageTableUpdateVS<1> >( ShaderMap );
					PixelShader  = *TShaderMapRef< TPageTableUpdatePS<1> >( ShaderMap );
				}
				break;
			default:
				check(0);
			}
			checkSlow( VertexShader && PixelShader );
			
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);

			SetGraphicsPipelineState( RHICmdList, GraphicsPSOInit );

			{
				const FVertexShaderRHIParamRef ShaderRHI = VertexShader->GetVertexShader();

				SetShaderValue( RHICmdList, ShaderRHI, VertexShader->PageTableSize,	PageTableSize );
				SetShaderValue( RHICmdList, ShaderRHI, VertexShader->FirstUpdate,	FirstUpdate );
				SetShaderValue( RHICmdList, ShaderRHI, VertexShader->NumUpdates,	NumUpdates );
				SetSRVParameter( RHICmdList, ShaderRHI, VertexShader->UpdateBuffer,	UpdateBufferSRV );
			}

			// needs to be the same on shader side (faster on NVIDIA and AMD)
			uint32 QuadsPerInstance = 8;

			RHICmdList.SetStreamSource( 0, NULL, 0 );
			RHICmdList.DrawIndexedPrimitive( GQuadIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, 32, 0, 2 * QuadsPerInstance, FMath::DivideAndRoundUp( NumUpdates, QuadsPerInstance ) );

			ExpandedUpdates[ Mip ].Reset();
		}

		FirstUpdate += NumUpdates;
		MipSize >>= 1;
	}

	RHICmdList.CopyToResolveTarget( PageTableTarget.TargetableTexture, PageTableTarget.ShaderResourceTexture, false, FResolveParams() );

	GRenderTargetPool.VisualizeTexture.SetCheckPoint( RHICmdList, PageTable );
}