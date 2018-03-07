// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn_DDC_AsyncCreate.h: Load texture 2D mips from the DDC using async create.
=============================================================================*/

#include "Streaming/Texture2DStreamIn_DDC_AsyncCreate.h"
#include "RenderUtils.h"

#if WITH_EDITORONLY_DATA

FTexture2DStreamIn_DDC_AsyncCreate::FTexture2DStreamIn_DDC_AsyncCreate(UTexture2D* InTexture, int32 InRequestedMips)
	: FTexture2DStreamIn_DDC(InTexture, InRequestedMips) 
{
	PushTask(FContext(InTexture, TT_None), TT_Async, TEXTURE2D_UPDATE_CALLBACK(AllocateAndLoadMips), TT_None, nullptr);
}

// ****************************
// ******* Update Steps *******
// ****************************

void FTexture2DStreamIn_DDC_AsyncCreate::AllocateAndLoadMips(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_DDC_AsyncCreate::AllocateAndLoadMips"), STAT_Texture2DStreamInDDCAsyncCreate_AllocateAndLoadMips, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	DoAllocateNewMips(Context);
	DoLoadNewMipsFromDDC(Context);

	PushTask(Context, TT_Async, TEXTURE2D_UPDATE_CALLBACK(AsyncCreate), TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_DDC_AsyncCreate::AsyncCreate(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_DDC_AsyncCreate::AsyncCreate"), STAT_Texture2DStreamInDDCAsyncCreate_AsyncCreate, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	DoAsyncCreateWithNewMips(Context);
	DoFreeNewMips(Context);

	PushTask(Context, TT_Render, TEXTURE2D_UPDATE_CALLBACK(Finalize), TT_Render, TEXTURE2D_UPDATE_CALLBACK(Cancel));
}

void FTexture2DStreamIn_DDC_AsyncCreate::Finalize(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_DDC_AsyncCreate::Finalize"), STAT_Texture2DStreamInDDCAsyncCreate_Finalize, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	DoCopySharedMips(Context);
	DoFinishUpdate(Context);
}

// ****************************
// ******* Cancel Steps *******
// ****************************


void FTexture2DStreamIn_DDC_AsyncCreate::Cancel(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FTexture2DStreamIn_DDC_AsyncCreate::Cancel"), STAT_Texture2DStreamInDDCAsyncCreate_Cancel, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	DoFreeNewMips(Context);
	DoFinishUpdate(Context);
}

#endif // WITH_EDITORONLY_DATA
