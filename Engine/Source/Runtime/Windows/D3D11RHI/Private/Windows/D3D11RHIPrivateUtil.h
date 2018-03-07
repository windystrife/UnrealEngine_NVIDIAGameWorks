// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11RHIPrivateUtil.h: Private D3D RHI Utility definitions for Windows.
=============================================================================*/

#pragma once

#include "WindowsD3D11ConstantBuffer.h"

struct FD3DRHIUtil
{
	template <EShaderFrequency ShaderFrequencyT>
	static FORCEINLINE void CommitConstants(FD3D11ConstantBuffer* InConstantBuffer, FD3D11StateCache& StateCache, uint32 Index, bool bDiscardSharedConstants)
	{
		auto* ConstantBuffer = ((FWinD3D11ConstantBuffer*)InConstantBuffer);
		// Array may contain NULL entries to pad out to proper 
		if (ConstantBuffer && ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants))
		{
			ID3D11Buffer* DeviceBuffer = ConstantBuffer->GetConstantBuffer();
			StateCache.SetConstantBuffer<ShaderFrequencyT>(DeviceBuffer, Index);
		}
	}
};
