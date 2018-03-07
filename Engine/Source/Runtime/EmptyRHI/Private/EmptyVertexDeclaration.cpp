// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyVertexDeclaration.cpp: Empty vertex declaration RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"

FEmptyVertexDeclaration::FEmptyVertexDeclaration(const FVertexDeclarationElementList& InElements)
{
	Elements = InElements;
}

FVertexDeclarationRHIRef FEmptyDynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	return new FEmptyVertexDeclaration(Elements);
}
