// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn_IO_AsyncReallocate.cpp: Default path for streaming in texture 2D mips.
=============================================================================*/

#include "Streaming/Texture2DStreamIn_IO_AsyncReallocate.h"
#include "RenderUtils.h"

FTexture2DStreamIn_IO_AsyncReallocate::FTexture2DStreamIn_IO_AsyncReallocate(UTexture2D* InTexture, int32 InRequestedMips, bool InPrioritizedIORequest) 
	: FTexture2DStreamIn_IO(InTexture, InRequestedMips, InPrioritizedIORequest)
{
	PushTask(FContext(InTexture, TT_None), TT_Render, TEXTURE2D_UPDATE_CALLBACK(AsyncReallocate), TT_None, nullptr);
}

// ****************************
// ******* Update Steps *******
// ****************************

void FTexture2DStreamIn_IO_AsyncReallocate::AsyncReallocate(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_AsyncReallocate::AsyncReallocate"), STAT_Texture2DStreamInIOAsyncReallocate_AsyncReallocate, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	SetIOFilename(Context);
	DoAsyncReallocate(Context);

	PushTask(Context, TT_Render, TEXTURE2D_UPDATE_CALLBACK(LockMips), TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_IO_AsyncReallocate::LockMips(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_AsyncReallocate::LockMips"), STAT_Texture2DStreamInIOAsyncReallocate_LockMips, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	RHIFinalizeAsyncReallocateTexture2D(IntermediateTextureRHI, true);
	DoLockNewMips(Context);

	PushTask(Context, TT_Async, TEXTURE2D_UPDATE_CALLBACK(LoadMips), TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_IO_AsyncReallocate::LoadMips(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_AsyncReallocate::LoadMips"), STAT_Texture2DStreamInIOAsyncReallocate_LoadMips, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	SetIORequests(Context);

	PushTask(Context, TT_Render, TEXTURE2D_UPDATE_CALLBACK(Finalize), TT_Async, TEXTURE2D_UPDATE_CALLBACK(CancelIO));
}

void FTexture2DStreamIn_IO_AsyncReallocate::Finalize(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_AsyncReallocate::Finalize"), STAT_Texture2DStreamInIOAsyncReallocate_Finalize, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	ClearIORequests(Context);
	DoUnlockNewMips(Context);
	DoFinishUpdate(Context);
}

// ****************************
// ******* Cancel Steps *******
// ****************************

void FTexture2DStreamIn_IO_AsyncReallocate::CancelIO(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_AsyncReallocate::CancelIO"), STAT_Texture2DStreamInIOAsyncReallocate_CancelIO, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	ClearIORequests(Context);

	PushTask(Context, TT_None, nullptr, TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_IO_AsyncReallocate::Cancel(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_IO_AsyncReallocate::Cancel"), STAT_Texture2DStreamInIOAsyncReallocate_Cancel, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	DoUnlockNewMips(Context);
	DoFinishUpdate(Context);
}
