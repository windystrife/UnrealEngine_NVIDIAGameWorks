// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyGlobalUniformBuffer.h: Empty Global uniform definitions.
=============================================================================*/

#pragma once 

/** Size of the default constant buffer (copied from D3D11) */
#define MAX_GLOBAL_CONSTANT_BUFFER_SIZE		4096

/**
 * An Empty uniform buffer that has backing memory to store global uniforms
 */
class FEmptyGlobalUniformBuffer : public FRenderResource, public FRefCountedObject
{
public:

	FEmptyGlobalUniformBuffer(uint32 InSize = 0);
	~FEmptyGlobalUniformBuffer();

	// FRenderResource interface.
	virtual void	InitDynamicRHI() override;
	virtual void	ReleaseDynamicRHI() override;

	void			UpdateConstant(const uint8* Data, uint16 Offset, uint16 Size);

private:
	uint32	MaxSize;
	bool	bIsDirty;
	uint8*	ShadowData;

	/** Size of all constants that has been updated since the last call to Commit. */
	uint32	CurrentUpdateSize;

	/**
	 * Size of all constants that has been updated since the last Discard.
	 * Includes "shared" constants that don't necessarily gets updated between every Commit.
	 */
	uint32	TotalUpdateSize;
};
