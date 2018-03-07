// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5PlatformTLS.cpp: HTML5 implementations of TLS functions
=============================================================================*/

#include "HTML5/HTML5PlatformTLS.h"
#include "Containers/Array.h"

static TArray<void*>& GetTLSArray()
{
	static TArray<void*> TLS;
	return TLS;
}
uint32 FHTML5TLS::AllocTlsSlot(void)
{
	return GetTLSArray().Add(0);
}

void FHTML5TLS::SetTlsValue(uint32 SlotIndex,void* Value)
{
	GetTLSArray()[SlotIndex] = Value;
}

void* FHTML5TLS::GetTlsValue(uint32 SlotIndex)
{
	return GetTLSArray()[SlotIndex];
}

