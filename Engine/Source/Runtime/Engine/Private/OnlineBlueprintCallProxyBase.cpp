// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Net/OnlineBlueprintCallProxyBase.h"

//////////////////////////////////////////////////////////////////////////
// UOnlineBlueprintCallProxyBase

UOnlineBlueprintCallProxyBase::UOnlineBlueprintCallProxyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetFlags(RF_StrongRefOnFrame);
}

void UOnlineBlueprintCallProxyBase::Activate()
{
}
