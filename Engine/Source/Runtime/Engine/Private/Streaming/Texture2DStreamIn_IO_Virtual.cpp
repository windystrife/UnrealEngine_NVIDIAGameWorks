// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn_IO_AsyncCreate.cpp: Async create path for streaming in texture 2D mips.
=============================================================================*/

#include "Streaming/Texture2DStreamIn_IO_Virtual.h"
#include "RenderUtils.h"

FTexture2DStreamIn_IO_Virtual::FTexture2DStreamIn_IO_Virtual(UTexture2D* InTexture, int32 InRequestedMips, bool InPrioritizedIORequest) 
	: FTexture2DStreamIn_IO(InTexture, InRequestedMips, InPrioritizedIORequest)
{
	PushTask(FContext(InTexture, TT_None), TT_Render, TEXTURE2D_UPDATE_CALLBACK(LockMips), TT_None, nullptr);
}

void FTexture2DStreamIn_IO_Virtual::LockMips(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_Virtual::LockMips"), STAT_Texture2DStreamInIOVirtual_LockMips, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	SetIOFilename(Context);
	DoConvertToVirtualWithNewMips(Context);
	DoLockNewMips(Context);

	PushTask(Context, TT_Async, TEXTURE2D_UPDATE_CALLBACK(LoadMips), TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_IO_Virtual::LoadMips(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_Virtual::LoadMips"), STAT_Texture2DStreamInIOVirtual_LoadMips, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	SetIORequests(Context);

	PushTask(Context, TT_Render, TEXTURE2D_UPDATE_CALLBACK(Finalize), TT_Async, TEXTURE2D_UPDATE_CALLBACK(CancelIO));
}

void FTexture2DStreamIn_IO_Virtual::Finalize(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_Virtual::Finalize"), STAT_Texture2DStreamInIOVirtual_Finalize, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	ClearIORequests(Context);
	DoUnlockNewMips(Context);
	RHIVirtualTextureSetFirstMipVisible(IntermediateTextureRHI, PendingFirstMip);
	DoFinishUpdate(Context);
}

// ****************************
// ******* Cancel Steps *******
// ****************************

void FTexture2DStreamIn_IO_Virtual::CancelIO(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_Virtual::CancelIO"), STAT_Texture2DStreamInIOVirtual_CancelIO, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	ClearIORequests(Context);

	PushTask(Context, TT_None, nullptr, TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_IO_Virtual::Cancel(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_Virtual::Cancel"), STAT_Texture2DStreamInIOVirtual_Cancel, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	DoUnlockNewMips(Context);
	RHIVirtualTextureSetFirstMipInMemory(IntermediateTextureRHI, Context.Resource->GetCurrentFirstMip());
	DoFinishUpdate(Context);
}
