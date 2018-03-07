// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsD3D11ConstantBuffer.h: D3D Constant Buffer functions
=============================================================================*/

#pragma once

#include "D3D11ConstantBuffer.h"

struct ID3D11Buffer;

class FWinD3D11ConstantBuffer : public FD3D11ConstantBuffer
{
public:
	FWinD3D11ConstantBuffer(FD3D11DynamicRHI* InD3DRHI, uint32 InSize = 0, uint32 SubBuffers = 1) :
		FD3D11ConstantBuffer(InD3DRHI, InSize, SubBuffers),
		Buffers(nullptr),
		CurrentSubBuffer(0),
		NumSubBuffers(SubBuffers)
	{
	}

	// FRenderResource interface.
	virtual void	InitDynamicRHI() override;
	virtual void	ReleaseDynamicRHI() override;

	/**
	* Get the current pool buffer
	*/
	ID3D11Buffer* GetConstantBuffer() const
	{
		return Buffers[CurrentSubBuffer];
	}

	/**
	* Unlocks the constant buffer so the data can be transmitted to the device
	*/
	bool CommitConstantsToDevice(bool bDiscardSharedConstants);

private:
	TRefCountPtr<ID3D11Buffer>* Buffers;
	uint32	CurrentSubBuffer;
	uint32	NumSubBuffers;
};
