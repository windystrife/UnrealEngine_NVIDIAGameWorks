// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyQuery.cpp: Empty query RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


FEmptyRenderQuery::FEmptyRenderQuery(ERenderQueryType InQueryType)
{

}

FEmptyRenderQuery::~FEmptyRenderQuery()
{

}

void FEmptyRenderQuery::Begin()
{

}

void FEmptyRenderQuery::End()
{

}






FRenderQueryRHIRef FEmptyDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	return new FEmptyRenderQuery(QueryType);
}

bool FEmptyDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI,uint64& OutNumPixels,bool bWait)
{
	check(IsInRenderingThread());

	FEmptyRenderQuery* Query = ResourceCast(QueryRHI);

	return false;
}

void FEmptyDynamicRHI::RHIBeginOcclusionQueryBatch()
{
}

void FEmptyDynamicRHI::RHIEndOcclusionQueryBatch()
{
}

void FEmptyDynamicRHI::RHISubmitCommandsHint()
{
}
