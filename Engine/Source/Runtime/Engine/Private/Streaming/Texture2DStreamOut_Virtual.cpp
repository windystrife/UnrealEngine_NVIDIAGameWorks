// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamOut_Virtual.cpp: Definitions of classes used for texture.
=============================================================================*/

#include "Streaming/Texture2DStreamOut_Virtual.h"
#include "RenderUtils.h"

FTexture2DStreamOut_Virtual::FTexture2DStreamOut_Virtual(UTexture2D* InTexture, int32 InRequestedMips)
	: FTexture2DUpdate(InTexture, InRequestedMips) 
{
	ensure(InRequestedMips < InTexture->GetNumResidentMips());
	
	PushTask(FContext(InTexture, TT_None), TT_Render, TEXTURE2D_UPDATE_CALLBACK(Finalize), TT_None, nullptr);
}

// ****************************
// ******* Update Steps *******
// ****************************

void FTexture2DStreamOut_Virtual::Finalize(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamOut_Virtual::Finalize"), STAT_Texture2DStreamOutVirtual_Finalize, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	static auto CVarVirtualTextureReducedMemoryEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextureReducedMemory"));
	check(CVarVirtualTextureReducedMemoryEnabled);

	if (CVarVirtualTextureReducedMemoryEnabled->GetValueOnRenderThread() == 0 || RequestedMips > UTexture2D::GetMinTextureResidentMipCount())
	{
		IntermediateTextureRHI = Context.Resource->GetTexture2DRHI();
		RHIVirtualTextureSetFirstMipVisible(IntermediateTextureRHI, PendingFirstMip);
		RHIVirtualTextureSetFirstMipInMemory(IntermediateTextureRHI, PendingFirstMip);
	}
	else
	{
		 DoConvertToNonVirtual(Context);
	}
	DoFinishUpdate(Context);
}

// ****************************
// ******* Cancel Steps *******
// ****************************

void FTexture2DStreamOut_Virtual::Cancel(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamOut_Virtual::Cancel"), STAT_Texture2DStreamOutVirtual_Cancel, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	DoFinishUpdate(Context);
}
