// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Util.h: D3D RHI utility implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "EngineModule.h"
#include "RendererInterface.h"

#define D3DERR(x) case x: ErrorCodeText = TEXT(#x); break;
#define LOCTEXT_NAMESPACE "Developer.MessageLog"

#ifndef _FACD3D 
#define _FACD3D  0x876
#endif	//_FACD3D 
#ifndef MAKE_D3DHRESULT
#define _FACD3D  0x876
#define MAKE_D3DHRESULT( code )  MAKE_HRESULT( 1, _FACD3D, code )
#endif	//MAKE_D3DHRESULT

#if WITH_D3DX_LIBS
#ifndef D3DERR_INVALIDCALL
#define D3DERR_INVALIDCALL MAKE_D3DHRESULT(2156)
#endif//D3DERR_INVALIDCALL
#ifndef D3DERR_WASSTILLDRAWING
#define D3DERR_WASSTILLDRAWING MAKE_D3DHRESULT(540)
#endif//D3DERR_WASSTILLDRAWING
#endif

extern bool D3D12RHI_ShouldCreateWithD3DDebug();

void SetName(ID3D12Object* const Object, const TCHAR* const Name)
{
#if NAME_OBJECTS
	if (Object)
	{
		VERIFYD3D12RESULT(Object->SetName(Name));
	}
#else
	UNREFERENCED_PARAMETER(Object);
	UNREFERENCED_PARAMETER(Name);
#endif
}

void SetName(FD3D12Resource* const Resource, const TCHAR* const Name)
{
#if NAME_OBJECTS
	// Special case for FD3D12Resources because we also store the name as a member in FD3D12Resource
	if (Resource)
	{
		Resource->SetName(Name);
	}
#else
	UNREFERENCED_PARAMETER(Resource);
	UNREFERENCED_PARAMETER(Name);
#endif
}

static FString GetD3D12DeviceHungErrorString(HRESULT ErrorCode)
{
	FString ErrorCodeText;

	switch (ErrorCode)
	{
		D3DERR(DXGI_ERROR_DEVICE_HUNG)
			D3DERR(DXGI_ERROR_DEVICE_REMOVED)
			D3DERR(DXGI_ERROR_DEVICE_RESET)
			D3DERR(DXGI_ERROR_DRIVER_INTERNAL_ERROR)
			D3DERR(DXGI_ERROR_INVALID_CALL)
	default: ErrorCodeText = FString::Printf(TEXT("%08X"), (int32)ErrorCode);
	}

	return ErrorCodeText;
}

static FString GetD3D12ErrorString(HRESULT ErrorCode, ID3D12Device* Device)
{
	FString ErrorCodeText;

	switch (ErrorCode)
	{
		D3DERR(S_OK);
		D3DERR(D3D11_ERROR_FILE_NOT_FOUND)
			D3DERR(D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS)
#if WITH_D3DX_LIBS
			D3DERR(D3DERR_INVALIDCALL)
			D3DERR(D3DERR_WASSTILLDRAWING)
#endif	//WITH_D3DX_LIBS
			D3DERR(E_FAIL)
			D3DERR(E_INVALIDARG)
			D3DERR(E_OUTOFMEMORY)
			D3DERR(DXGI_ERROR_INVALID_CALL)
			D3DERR(E_NOINTERFACE)
			D3DERR(DXGI_ERROR_DEVICE_REMOVED)
	default: ErrorCodeText = FString::Printf(TEXT("%08X"), (int32)ErrorCode);
	}

	if (ErrorCode == DXGI_ERROR_DEVICE_REMOVED && Device)
	{
		HRESULT hResDeviceRemoved = Device->GetDeviceRemovedReason();
		ErrorCodeText += FString(TEXT(" ")) + GetD3D12DeviceHungErrorString(hResDeviceRemoved);
	}

	return ErrorCodeText;
}

#undef D3DERR

namespace D3D12RHI
{
	const TCHAR* GetD3D12TextureFormatString(DXGI_FORMAT TextureFormat)
	{
		static const TCHAR* EmptyString = TEXT("");
		const TCHAR* TextureFormatText = EmptyString;
#define D3DFORMATCASE(x) case x: TextureFormatText = TEXT(#x); break;
		switch (TextureFormat)
		{
			D3DFORMATCASE(DXGI_FORMAT_R8G8B8A8_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_B8G8R8A8_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_B8G8R8X8_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC1_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC2_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC3_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC4_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R16G16B16A16_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_R32G32B32A32_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_UNKNOWN)
				D3DFORMATCASE(DXGI_FORMAT_R8_UNORM)
#if DEPTH_32_BIT_CONVERSION
				D3DFORMATCASE(DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
				D3DFORMATCASE(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS)
#endif
				D3DFORMATCASE(DXGI_FORMAT_R32G8X24_TYPELESS)
				D3DFORMATCASE(DXGI_FORMAT_D24_UNORM_S8_UINT)
				D3DFORMATCASE(DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
				D3DFORMATCASE(DXGI_FORMAT_R32_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_R16G16_UINT)
				D3DFORMATCASE(DXGI_FORMAT_R16G16_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R16G16_SNORM)
				D3DFORMATCASE(DXGI_FORMAT_R16G16_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_R32G32_FLOAT)
				D3DFORMATCASE(DXGI_FORMAT_R10G10B10A2_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R16G16B16A16_UINT)
				D3DFORMATCASE(DXGI_FORMAT_R8G8_SNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC5_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R1_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_R8G8B8A8_TYPELESS)
				D3DFORMATCASE(DXGI_FORMAT_B8G8R8A8_TYPELESS)
				D3DFORMATCASE(DXGI_FORMAT_BC7_UNORM)
				D3DFORMATCASE(DXGI_FORMAT_BC6H_UF16)
		default: TextureFormatText = EmptyString;
		}
#undef D3DFORMATCASE
		return TextureFormatText;
	}
}
using namespace D3D12RHI;

static FString GetD3D12TextureFlagString(uint32 TextureFlags)
{
	FString TextureFormatText = TEXT("");

	if (TextureFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		TextureFormatText += TEXT("D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ");
	}

	if (TextureFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		TextureFormatText += TEXT("D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL ");
	}

	if (TextureFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
	{
		TextureFormatText += TEXT("D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE ");
	}

	if (TextureFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		TextureFormatText += TEXT("D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS ");
	}
	return TextureFormatText;
}

extern CORE_API bool GIsGPUCrashed;
static void TerminateOnDeviceRemoved(HRESULT D3DResult)
{
	if (D3DResult == DXGI_ERROR_DEVICE_REMOVED)
	{
		GIsGPUCrashed = true;
		if (!FApp::IsUnattended())
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *LOCTEXT("DeviceRemoved", "Video driver crashed and was reset!  Make sure your video drivers are up to date.  Exiting...").ToString(), TEXT("Error"));
		}
		else
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("%s"), *LOCTEXT("DeviceRemoved", "Video driver crashed and was reset!  Make sure your video drivers are up to date.  Exiting...").ToString());
		}
		FPlatformMisc::RequestExit(true);
	}
}


static void TerminateOnOutOfMemory(HRESULT D3DResult, bool bCreatingTextures)
{
	if (D3DResult == E_OUTOFMEMORY)
	{
		if (bCreatingTextures)
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *LOCTEXT("OutOfVideoMemoryTextures", "Out of video memory trying to allocate a texture! Make sure your video card has the minimum required memory, try lowering the resolution and/or closing other applications that are running. Exiting...").ToString(), TEXT("Error"));
		}
		else
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *NSLOCTEXT("D3D12RHI", "OutOfMemory", "Out of video memory trying to allocate a rendering resource. Make sure your video card has the minimum required memory, try lowering the resolution and/or closing other applications that are running. Exiting...").ToString(), TEXT("Error"));
		}
#if STATS
		GetRendererModule().DebugLogOnCrash();
#endif
		FPlatformMisc::RequestExit(true);
	}
}

#ifndef MAKE_D3DHRESULT
#define _FACD3D						0x876
#define MAKE_D3DHRESULT( code)		MAKE_HRESULT( 1, _FACD3D, code )
#endif	//MAKE_D3DHRESULT

namespace D3D12RHI
{
	void VerifyD3D12Result(HRESULT D3DResult, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line, ID3D12Device* Device)
	{
		check(FAILED(D3DResult));

		const FString& ErrorString = GetD3D12ErrorString(D3DResult, Device);

		UE_LOG(LogD3D12RHI, Error, TEXT("%s failed \n at %s:%u \n with error %s"), ANSI_TO_TCHAR(Code), ANSI_TO_TCHAR(Filename), Line, *ErrorString);

		TerminateOnDeviceRemoved(D3DResult);
		TerminateOnOutOfMemory(D3DResult, false);

		UE_LOG(LogD3D12RHI, Fatal, TEXT("%s failed \n at %s:%u \n with error %s"), ANSI_TO_TCHAR(Code), ANSI_TO_TCHAR(Filename), Line, *ErrorString);
	}

	void VerifyD3D12CreateTextureResult(HRESULT D3DResult, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags)
	{
		check(FAILED(D3DResult));

		const FString ErrorString = GetD3D12ErrorString(D3DResult, 0);
		const TCHAR* D3DFormatString = GetD3D12TextureFormatString((DXGI_FORMAT)Format);

		UE_LOG(LogD3D12RHI, Error,
			TEXT("%s failed \n at %s:%u \n with error %s, \n Size=%ix%ix%i Format=%s(0x%08X), NumMips=%i, Flags=%s"),
			ANSI_TO_TCHAR(Code),
			ANSI_TO_TCHAR(Filename),
			Line,
			*ErrorString,
			SizeX,
			SizeY,
			SizeZ,
			D3DFormatString,
			Format,
			NumMips,
			*GetD3D12TextureFlagString(Flags));

		TerminateOnDeviceRemoved(D3DResult);
		TerminateOnOutOfMemory(D3DResult, true);

		// this is to track down a rarely happening crash
		if (D3DResult == E_OUTOFMEMORY)
		{
#if STATS
			GetRendererModule().DebugLogOnCrash();
#endif // STATS
		}

		UE_LOG(LogD3D12RHI, Fatal,
			TEXT("%s failed \n at %s:%u \n with error %s, \n Size=%ix%ix%i Format=%s(0x%08X), NumMips=%i, Flags=%s"),
			ANSI_TO_TCHAR(Code),
			ANSI_TO_TCHAR(Filename),
			Line,
			*ErrorString,
			SizeX,
			SizeY,
			SizeZ,
			D3DFormatString,
			Format,
			NumMips,
			*GetD3D12TextureFlagString(Flags));
	}

	void VerifyComRefCount(IUnknown* Object, int32 ExpectedRefs, const TCHAR* Code, const TCHAR* Filename, int32 Line)
	{
		int32 NumRefs;

		if (Object)
		{
			Object->AddRef();
			NumRefs = Object->Release();

			checkSlow(NumRefs != ExpectedRefs);

			if (NumRefs != ExpectedRefs)
			{
				UE_LOG(
					LogD3D12RHI,
					Error,
					TEXT("%s:(%d): %s has %d refs, expected %d"),
					Filename,
					Line,
					Code,
					NumRefs,
					ExpectedRefs
					);
			}
		}
	}
}

void FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(const D3D12_RESOURCE_BINDING_TIER& ResourceBindingTier, const FShaderCodePackedResourceCounts& Counts, FShaderRegisterCounts& Shader, bool bAllowUAVs)
{
	static const uint32 MaxSamplerCount = MAX_SAMPLERS;
	static const uint32 MaxConstantBufferCount = MAX_CBS;
	static const uint32 MaxShaderResourceCount = MAX_SRVS;
	static const uint32 MaxUnorderedAccessCount = MAX_UAVS;

	// Round up and clamp values to their max
	// Note: Rounding and setting counts based on binding tier allows us to create fewer root signatures.

	// To reduce the size of the root signature, we only allow UAVs for certain shaders. 
	// This code makes the assumption that the engine only uses UAVs at the PS or CS shader stages.
	check(bAllowUAVs || (!bAllowUAVs && Counts.NumUAVs == 0));

	if (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
	{
		Shader.SamplerCount = (Counts.NumSamplers > 0) ? FMath::Min(MaxSamplerCount, FMath::RoundUpToPowerOfTwo(Counts.NumSamplers)) : Counts.NumSamplers;
		Shader.ShaderResourceCount = (Counts.NumSRVs > 0) ? FMath::Min(MaxShaderResourceCount, FMath::RoundUpToPowerOfTwo(Counts.NumSRVs)) : Counts.NumSRVs;
	}
	else
	{
		Shader.SamplerCount = MaxSamplerCount;
		Shader.ShaderResourceCount = MaxShaderResourceCount;
	}

	if (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
	{
		Shader.ConstantBufferCount = (Counts.NumCBs > MAX_ROOT_CBVS) ? FMath::Min(MaxConstantBufferCount, FMath::RoundUpToPowerOfTwo(Counts.NumCBs)) : Counts.NumCBs;
		Shader.UnorderedAccessCount = (Counts.NumUAVs > 0 && bAllowUAVs) ? FMath::Min(MaxUnorderedAccessCount, FMath::RoundUpToPowerOfTwo(Counts.NumUAVs)) : 0;
	}
	else
	{
		Shader.ConstantBufferCount = (Counts.NumCBs > MAX_ROOT_CBVS) ? MaxConstantBufferCount : Counts.NumCBs;
		Shader.UnorderedAccessCount = (bAllowUAVs) ? MaxUnorderedAccessCount : 0;
	}
}

void QuantizeBoundShaderState(
	const D3D12_RESOURCE_BINDING_TIER& ResourceBindingTier,
	const FD3D12BoundShaderState* const BSS,
	FD3D12QuantizedBoundShaderState &QBSS
	)
{
	// BSS quantizer. There is a 1:1 mapping of quantized bound shader state objects to root signatures.
	// The objective is to allow a single root signature to represent many bound shader state objects.
	// The bigger the quantization step sizes, the fewer the root signatures.
	FMemory::Memzero(&QBSS, sizeof(QBSS));
	QBSS.bAllowIAInputLayout = BSS->InputLayout.NumElements > 0;	// Does the root signature need access to vertex buffers?

	const FD3D12VertexShader* const VertexShader = BSS->GetVertexShader();
	const FD3D12PixelShader* const PixelShader = BSS->GetPixelShader();
	const FD3D12HullShader* const HullShader = BSS->GetHullShader();
	const FD3D12DomainShader* const DomainShader = BSS->GetDomainShader();
	const FD3D12GeometryShader* const GeometryShader = BSS->GetGeometryShader();
	if (VertexShader) FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(ResourceBindingTier, VertexShader->ResourceCounts, QBSS.RegisterCounts[SV_Vertex]);
	if (PixelShader) FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(ResourceBindingTier, PixelShader->ResourceCounts, QBSS.RegisterCounts[SV_Pixel], true);
	if (HullShader) FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(ResourceBindingTier, HullShader->ResourceCounts, QBSS.RegisterCounts[SV_Hull]);
	if (DomainShader) FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(ResourceBindingTier, DomainShader->ResourceCounts, QBSS.RegisterCounts[SV_Domain]);
	if (GeometryShader) FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(ResourceBindingTier, GeometryShader->ResourceCounts, QBSS.RegisterCounts[SV_Geometry]);
}

void QuantizeBoundShaderState(
	const D3D12_RESOURCE_BINDING_TIER& ResourceBindingTier,
	const FD3D12ComputeShader* const ComputeShader,
	FD3D12QuantizedBoundShaderState &QBSS
	)
{
	// BSS quantizer. There is a 1:1 mapping of quantized bound shader state objects to root signatures.
	// The objective is to allow a single root signature to represent many bound shader state objects.
	// The bigger the quantization step sizes, the fewer the root signatures.
	FMemory::Memzero(&QBSS, sizeof(QBSS));
	check(QBSS.bAllowIAInputLayout == false);	// No access to vertex buffers needed
	check(ComputeShader);
	FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(ResourceBindingTier, ComputeShader->ResourceCounts, QBSS.RegisterCounts[SV_All], true);
}

FD3D12BoundRenderTargets::FD3D12BoundRenderTargets(FD3D12RenderTargetView** RTArray, uint32 NumActiveRTs, FD3D12DepthStencilView* DSView)
{
	FMemory::Memcpy(RenderTargetViews, RTArray, sizeof(RenderTargetViews));
	DepthStencilView = DSView;
	NumActiveTargets = NumActiveRTs;
}

FD3D12BoundRenderTargets::~FD3D12BoundRenderTargets()
{
}

void LogExecuteCommandLists(uint32 NumCommandLists, ID3D12CommandList* const* ppCommandLists)
{
	for (uint32 i = 0; i < NumCommandLists; i++)
	{
		ID3D12CommandList* const pCurrentCommandList = ppCommandLists[i];
		UE_LOG(LogD3D12RHI, Log, TEXT("*** EXECUTE (CmdList: %016llX) %u/%u ***"), pCurrentCommandList, i + 1, NumCommandLists);
	}
}

FString ConvertToResourceStateString(uint32 ResourceState)
{
	if (ResourceState == 0)
	{
		return FString(TEXT("D3D12_RESOURCE_STATE_COMMON"));
	}

	const TCHAR* ResourceStateNames[] =
	{
		TEXT("D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER"),
		TEXT("D3D12_RESOURCE_STATE_INDEX_BUFFER"),
		TEXT("D3D12_RESOURCE_STATE_RENDER_TARGET"),
		TEXT("D3D12_RESOURCE_STATE_UNORDERED_ACCESS"),
		TEXT("D3D12_RESOURCE_STATE_DEPTH_WRITE"),
		TEXT("D3D12_RESOURCE_STATE_DEPTH_READ"),
		TEXT("D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE"),
		TEXT("D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE"),
		TEXT("D3D12_RESOURCE_STATE_STREAM_OUT"),
		TEXT("D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT"),
		TEXT("D3D12_RESOURCE_STATE_COPY_DEST"),
		TEXT("D3D12_RESOURCE_STATE_COPY_SOURCE"),
		TEXT("D3D12_RESOURCE_STATE_RESOLVE_DEST"),
		TEXT("D3D12_RESOURCE_STATE_RESOLVE_SOURCE"),
	};

	FString ResourceStateString;
	uint16 NumStates = 0;
	for (uint16 i = 0; ResourceState && i < ARRAYSIZE(ResourceStateNames); i++)
	{
		if (ResourceState & 1)
		{
			if (NumStates > 0)
			{
				ResourceStateString += " | ";
			}

			ResourceStateString += ResourceStateNames[i];
			NumStates++;
		}
		ResourceState = ResourceState >> 1;
	}
	return ResourceStateString;
}

void LogResourceBarriers(uint32 NumBarriers, D3D12_RESOURCE_BARRIER* pBarriers, ID3D12CommandList* const pCommandList)
{
	// Configure what resource barriers are logged.
	const bool bLogAll = false;
	const bool bLogTransitionDepth = true;
	const bool bLogTransitionRenderTarget = true;
	const bool bLogTransitionUAV = true;

	// Create the state bit mask to indicate what barriers should be logged.
	uint32 ShouldLogMask = bLogAll ? static_cast<uint32>(-1) : 0;
	ShouldLogMask |= bLogTransitionDepth ? D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE : 0;
	ShouldLogMask |= bLogTransitionRenderTarget ? D3D12_RESOURCE_STATE_RENDER_TARGET : 0;
	ShouldLogMask |= bLogTransitionUAV ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : 0;

	for (uint32 i = 0; i < NumBarriers; i++)
	{
		D3D12_RESOURCE_BARRIER &currentBarrier = pBarriers[i];

		switch (currentBarrier.Type)
		{
		case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
		{
			const FString StateBefore = ConvertToResourceStateString(static_cast<uint32>(currentBarrier.Transition.StateBefore));
			const FString StateAfter = ConvertToResourceStateString(static_cast<uint32>(currentBarrier.Transition.StateAfter));

			bool bShouldLog = bLogAll;
			if (!bShouldLog)
			{
				// See if we should log this transition.
				for (uint32 j = 0; (j < 2) && !bShouldLog; j++)
				{
					const D3D12_RESOURCE_STATES& State = (j == 0) ? currentBarrier.Transition.StateBefore : currentBarrier.Transition.StateAfter;
					bShouldLog = (State & ShouldLogMask) > 0;
				}
			}

			if (bShouldLog)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("*** BARRIER (CmdList: %016llX) %u/%u: %016llX (Sub: %u), %s -> %s"), pCommandList, i + 1, NumBarriers,
					currentBarrier.Transition.pResource,
					currentBarrier.Transition.Subresource,
					*StateBefore,
					*StateAfter);
			}
			break;
		}

		case D3D12_RESOURCE_BARRIER_TYPE_UAV:
			UE_LOG(LogD3D12RHI, Log, TEXT("*** BARRIER (CmdList: %016llX) %u/%u: UAV Barrier"), pCommandList, i + 1, NumBarriers);
			break;

		default:
			check(false);
			break;
		}		
	}
}


//==================================================================================================================================
// CResourceState
// Tracking of per-resource or per-subresource state
//==================================================================================================================================
//----------------------------------------------------------------------------------------------------------------------------------
void CResourceState::Initialize(uint32 SubresourceCount)
{
	check(0 == m_SubresourceState.Num());

	// Allocate space for per-subresource tracking structures
	check(SubresourceCount > 0);
	m_SubresourceState.SetNumUninitialized(SubresourceCount);
	check(m_SubresourceState.Num() == SubresourceCount);

	// All subresources start out in an unknown state
	SetResourceState(D3D12_RESOURCE_STATE_TBD);
}

//----------------------------------------------------------------------------------------------------------------------------------
bool CResourceState::AreAllSubresourcesSame() const
{
	return m_AllSubresourcesSame && (m_ResourceState != D3D12_RESOURCE_STATE_TBD);
}

//----------------------------------------------------------------------------------------------------------------------------------
bool CResourceState::CheckResourceState(D3D12_RESOURCE_STATES State) const
{
	if (m_AllSubresourcesSame)
	{
		return State == m_ResourceState;
	}
	else
	{
		// All subresources must be individually checked
		const uint32 numSubresourceStates = m_SubresourceState.Num();
		for (uint32 i = 0; i < numSubresourceStates; i++)
		{
			if (m_SubresourceState[i] != State)
			{
				return false;
			}
		}

		return true;
	}
}

//----------------------------------------------------------------------------------------------------------------------------------
bool CResourceState::CheckResourceStateInitalized() const
{
	return m_SubresourceState.Num() > 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
D3D12_RESOURCE_STATES CResourceState::GetSubresourceState(uint32 SubresourceIndex) const
{
	if (m_AllSubresourcesSame)
	{
		return m_ResourceState;
	}
	else
	{
		check(SubresourceIndex < static_cast<uint32>(m_SubresourceState.Num()));
		return m_SubresourceState[SubresourceIndex];
	}
}

//----------------------------------------------------------------------------------------------------------------------------------
void CResourceState::SetResourceState(D3D12_RESOURCE_STATES State)
{
	m_AllSubresourcesSame = 1;

	m_ResourceState = State;

	// State is now tracked per-resource, so m_SubresourceState should not be read.
#if UE_BUILD_DEBUG
	const uint32 numSubresourceStates = m_SubresourceState.Num();
	for (uint32 i = 0; i < numSubresourceStates; i++)
	{
		m_SubresourceState[i] = D3D12_RESOURCE_STATE_CORRUPT;
	}
#endif
}

//----------------------------------------------------------------------------------------------------------------------------------
void CResourceState::SetSubresourceState(uint32 SubresourceIndex, D3D12_RESOURCE_STATES State)
{
	// If setting all subresources, or the resource only has a single subresource, set the per-resource state
	if (SubresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ||
		m_SubresourceState.Num() == 1)
	{
		SetResourceState(State);
	}
	else
	{
		check(SubresourceIndex < static_cast<uint32>(m_SubresourceState.Num()));

		// If state was previously tracked on a per-resource level, then transition to per-subresource tracking
		if (m_AllSubresourcesSame)
		{
			const uint32 numSubresourceStates = m_SubresourceState.Num();
			for (uint32 i = 0; i < numSubresourceStates; i++)
			{
				m_SubresourceState[i] = m_ResourceState;
			}

			m_AllSubresourcesSame = 0;

			// State is now tracked per-subresource, so m_ResourceState should not be read.
#if UE_BUILD_DEBUG
			m_ResourceState = D3D12_RESOURCE_STATE_CORRUPT;
#endif
		}

		m_SubresourceState[SubresourceIndex] = State;
	}
}

bool FD3D12SyncPoint::IsValid() const
{
	return Fence != nullptr;
}

bool FD3D12SyncPoint::IsComplete() const
{
	check(IsValid());
	return Fence->IsFenceComplete(Value);
}

void FD3D12SyncPoint::WaitForCompletion() const
{
	check(IsValid());
	Fence->WaitForFence(Value);
}

// Forward declarations are required for the template functions
template bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12View<D3D12_RENDER_TARGET_VIEW_DESC>* pView, const D3D12_RESOURCE_STATES& State);
template bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12View<D3D12_UNORDERED_ACCESS_VIEW_DESC>* pView, const D3D12_RESOURCE_STATES& State);
template bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12View<D3D12_SHADER_RESOURCE_VIEW_DESC>* pView, const D3D12_RESOURCE_STATES& State);

template <class TView>
bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12View<TView>* pView, const D3D12_RESOURCE_STATES& State)
{
	// Check the view
	if (!pView)
	{
		// No need to check null views
		return true;
	}

	return AssertResourceState(pCommandList, pView->GetResource(), State, pView->GetViewSubresourceSubset());
}

bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12Resource* pResource, const D3D12_RESOURCE_STATES& State, uint32 Subresource)
{
	// Check the resource
	if (!pResource)
	{
		// No need to check null resources
		// Some dynamic SRVs haven't been mapped and updated yet so they actually don't have any backing resources.
		return true;
	}

	CViewSubresourceSubset SubresourceSubset(Subresource, pResource->GetMipLevels(), pResource->GetArraySize(), pResource->GetPlaneCount());
	return AssertResourceState(pCommandList, pResource, State, SubresourceSubset);
}

bool AssertResourceState(ID3D12CommandList* pCommandList, FD3D12Resource* pResource, const D3D12_RESOURCE_STATES& State, const CViewSubresourceSubset& SubresourceSubset)
{
#if PLATFORM_WINDOWS
	// Check the resource
	if (!pResource)
	{
		// No need to check null resources
		// Some dynamic SRVs haven't been mapped and updated yet so they actually don't have any backing resources.
		return true;
	}

	// Can only verify resource states if the debug layer is used
	static const bool bWithD3DDebug = D3D12RHI_ShouldCreateWithD3DDebug();
	if (!bWithD3DDebug)
	{
		UE_LOG(LogD3D12RHI, Fatal, TEXT("*** AssertResourceState requires the debug layer ***"));
		return false;
	}

	// Get the debug command queue
	TRefCountPtr<ID3D12DebugCommandList> pDebugCommandList;
	VERIFYD3D12RESULT(pCommandList->QueryInterface(pDebugCommandList.GetInitReference()));

	// Get the underlying resource
	ID3D12Resource* pD3D12Resource = pResource->GetResource();
	check(pD3D12Resource);

	// For each subresource in the view...
	for (CViewSubresourceSubset::CViewSubresourceIterator it = SubresourceSubset.begin(); it != SubresourceSubset.end(); ++it)
	{
		for (uint32 SubresourceIndex = it.StartSubresource(); SubresourceIndex < it.EndSubresource(); SubresourceIndex++)
		{
			const bool bGoodState = !!pDebugCommandList->AssertResourceState(pD3D12Resource, SubresourceIndex, State);
			if (!bGoodState)
			{
				return false;
			}
		}
	}
#endif // PLATFORM_WINDOWS

	return true;
}

//
// Stat declarations.
//

DEFINE_STAT(STAT_D3D12PresentTime);

DEFINE_STAT(STAT_D3D12NumCommandAllocators);
DEFINE_STAT(STAT_D3D12NumCommandLists);
DEFINE_STAT(STAT_D3D12NumPSOs);

DEFINE_STAT(STAT_D3D12TexturesAllocated);
DEFINE_STAT(STAT_D3D12TexturesReleased);
DEFINE_STAT(STAT_D3D12CreateTextureTime);
DEFINE_STAT(STAT_D3D12LockTextureTime);
DEFINE_STAT(STAT_D3D12UnlockTextureTime);
DEFINE_STAT(STAT_D3D12CreateBufferTime);
DEFINE_STAT(STAT_D3D12LockBufferTime);
DEFINE_STAT(STAT_D3D12UnlockBufferTime);
DEFINE_STAT(STAT_D3D12CommitTransientResourceTime);
DEFINE_STAT(STAT_D3D12DecommitTransientResourceTime);

DEFINE_STAT(STAT_D3D12NewBoundShaderStateTime);
DEFINE_STAT(STAT_D3D12CreateBoundShaderStateTime);
DEFINE_STAT(STAT_D3D12NumBoundShaderState);
DEFINE_STAT(STAT_D3D12SetBoundShaderState);

DEFINE_STAT(STAT_D3D12UpdateUniformBufferTime);

DEFINE_STAT(STAT_D3D12CommitResourceTables);
DEFINE_STAT(STAT_D3D12SetTextureInTableCalls);

DEFINE_STAT(STAT_D3D12ClearShaderResourceViewsTime);
DEFINE_STAT(STAT_D3D12SetShaderResourceViewTime);
DEFINE_STAT(STAT_D3D12SetUnorderedAccessViewTime);
DEFINE_STAT(STAT_D3D12CommitGraphicsConstants);
DEFINE_STAT(STAT_D3D12CommitComputeConstants);
DEFINE_STAT(STAT_D3D12SetShaderUniformBuffer);

DEFINE_STAT(STAT_D3D12ApplyStateTime);
DEFINE_STAT(STAT_D3D12ApplyStateRebuildPSOTime);
DEFINE_STAT(STAT_D3D12ApplyStateFindPSOTime);
DEFINE_STAT(STAT_D3D12ApplyStateSetSRVTime);
DEFINE_STAT(STAT_D3D12ApplyStateSetUAVTime);
DEFINE_STAT(STAT_D3D12ApplyStateSetVertexBufferTime);
DEFINE_STAT(STAT_D3D12ApplyStateSetConstantBufferTime);
DEFINE_STAT(STAT_D3D12PSOCreateTime);
DEFINE_STAT(STAT_D3D12ClearMRT);

DEFINE_STAT(STAT_D3D12ExecuteCommandListTime);
DEFINE_STAT(STAT_D3D12WaitForFenceTime);

DEFINE_STAT(STAT_D3D12UsedVideoMemory);
DEFINE_STAT(STAT_D3D12AvailableVideoMemory);
DEFINE_STAT(STAT_D3D12TotalVideoMemory);
#undef LOCTEXT_NAMESPACE
