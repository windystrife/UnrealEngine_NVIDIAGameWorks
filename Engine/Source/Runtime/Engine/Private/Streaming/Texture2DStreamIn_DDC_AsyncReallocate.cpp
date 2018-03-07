// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn_DDC_AsyncReallocate.cpp: Load texture 2D mips from the DDC using async reallocate.
=============================================================================*/

#include "Streaming/Texture2DStreamIn_DDC_AsyncReallocate.h"
#include "RenderUtils.h"

#if WITH_EDITORONLY_DATA

FTexture2DStreamIn_DDC_AsyncReallocate::FTexture2DStreamIn_DDC_AsyncReallocate(UTexture2D* InTexture, int32 InRequestedMips)
	: FTexture2DStreamIn_DDC(InTexture, InRequestedMips)
{
	PushTask(FContext(InTexture, TT_None), TT_Render, TEXTURE2D_UPDATE_CALLBACK(AsyncReallocate), TT_None, nullptr);
}

// ****************************
// ******* Update Steps *******
// ****************************

void FTexture2DStreamIn_DDC_AsyncReallocate::AsyncReallocate(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_DDC_AsyncReallocate::AsyncReallocate"), STAT_Texture2DStreamInDDCAsyncReallocate_AsyncReallocate, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	DoAsyncReallocate(Context);

	PushTask(Context, TT_Render, TEXTURE2D_UPDATE_CALLBACK(LockMips), TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_DDC_AsyncReallocate::LockMips(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_DDC_AsyncReallocate::LockMips"), STAT_Texture2DStreamInDDCAsyncReallocate_LockMips, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	RHIFinalizeAsyncReallocateTexture2D(IntermediateTextureRHI, true);
	DoLockNewMips(Context);

	PushTask(Context, TT_Async, TEXTURE2D_UPDATE_CALLBACK(LoadMips), TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_DDC_AsyncReallocate::LoadMips(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_DDC_AsyncReallocate::LoadMips"), STAT_Texture2DStreamInDDCAsyncReallocate_LoadMips, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	DoLoadNewMipsFromDDC(Context);

	PushTask(Context, TT_Render, TEXTURE2D_UPDATE_CALLBACK(Finalize), TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_DDC_AsyncReallocate::Finalize(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_DDC_AsyncReallocate::Finalize"), STAT_Texture2DStreamInDDCAsyncReallocate_Finalize, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	DoUnlockNewMips(Context);
	DoFinishUpdate(Context);
}

// ****************************
// ******* Cancel Steps *******
// ****************************

void FTexture2DStreamIn_DDC_AsyncReallocate::Cancel(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_DDC_AsyncReallocate::Cancel"), STAT_Texture2DStreamInDDCAsyncReallocate_Cancel, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	DoUnlockNewMips(Context);
	DoFinishUpdate(Context);
}

#endif // WITH_EDITORONLY_DATA
