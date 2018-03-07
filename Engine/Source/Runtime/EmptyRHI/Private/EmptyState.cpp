// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyState.cpp: Empty state implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"

/**
 * Translate from UE3 enums to Empty enums (leaving these in Empty just for help in setting them up)
 */
static int32 TranslateMipFilterMode(ESamplerFilter Filter)
{
	switch (Filter)
	{
		case SF_Point:	return 0;
		default:		return 0;
	}
}

static int32 TranslateFilterMode(ESamplerFilter Filter)
{
	switch (Filter)
	{
		case SF_Point:				return 0;
		case SF_AnisotropicPoint:	return 0;
		case SF_AnisotropicLinear:	return 0;
		default:					return 0;
	}
}

static int32 TranslateZFilterMode(ESamplerFilter Filter)
{	switch (Filter)
	{
		case SF_Point:				return 0;
		case SF_AnisotropicPoint:	return 0;
		case SF_AnisotropicLinear:	return 0;
		default:					return 0;
	}
}

static int32 TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch (CullMode)
	{
		case CM_CCW:	return 0;
		case CM_CW:		return 0;
		default:		return 0;
	}
}

static int32 TranslateWrapMode(ESamplerAddressMode AddressMode)
{
	switch (AddressMode)
	{
		case AM_Clamp:	return 0;
		case AM_Mirror: return 0;
		case AM_Border: return 0;
		default:		return 0;
	}
}

static int32 TranslateCompareFunction(ECompareFunction CompareFunction)
{
	switch(CompareFunction)
	{
		case CF_Less:			return 0;
		case CF_LessEqual:		return 0;
		case CF_Greater:		return 0;
		case CF_GreaterEqual:	return 0;
		case CF_Equal:			return 0;
		case CF_NotEqual:		return 0;
		case CF_Never:			return 0;
		default:				return 0;
	};
}

static int32 TranslateSamplerCompareFunction(ESamplerCompareFunction SamplerComparisonFunction)
{
	switch(SamplerComparisonFunction)
	{
		case SCF_Less:		return 0;
		case SCF_Never:		return 0;
		default:			return 0;
	};
}

static int32 TranslateStencilOp(EStencilOp StencilOp)
{
	switch(StencilOp)
	{
		case SO_Zero:				return 0;
		case SO_Replace:			return 0;
		case SO_SaturatedIncrement:	return 0;
		case SO_SaturatedDecrement:	return 0;
		case SO_Invert:				return 0;
		case SO_Increment:			return 0;
		case SO_Decrement:			return 0;
		default:					return 0;
	};
}

static int32 TranslateFillMode(ERasterizerFillMode FillMode)
{
	switch(FillMode)
	{
		case FM_Wireframe:	return 0;
		case FM_Point:		return 0;
		default:			return 0;
	};
}

static int32 TranslateBlendOp(EBlendOperation BlendOp)
{
	switch(BlendOp)
	{
		case BO_Subtract:	return 0;
		case BO_Min:		return 0;
		case BO_Max:		return 0;
		default:			return 0;
	};
}


static int32 TranslateBlendFactor(EBlendFactor BlendFactor)
{
	switch(BlendFactor)
	{
		case BF_One:					return 0;
		case BF_SourceColor:			return 0;
		case BF_InverseSourceColor:		return 0;
		case BF_SourceAlpha:			return 0;
		case BF_InverseSourceAlpha:		return 0;
		case BF_DestAlpha:				return 0;
		case BF_InverseDestAlpha:		return 0;
		case BF_DestColor:				return 0;
		case BF_InverseDestColor:		return 0;
		default:						return 0;
	};
}




FEmptySamplerState::FEmptySamplerState(const FSamplerStateInitializerRHI& Initializer)
{

}

FEmptyRasterizerState::FEmptyRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{

}

FEmptyDepthStencilState::FEmptyDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{

}

FEmptyBlendState::FEmptyBlendState(const FBlendStateInitializerRHI& Initializer)
{

}



FSamplerStateRHIRef FEmptyDynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	return new FEmptySamplerState(Initializer);
}

FRasterizerStateRHIRef FEmptyDynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
    return new FEmptyRasterizerState(Initializer);
}

FDepthStencilStateRHIRef FEmptyDynamicRHI::RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	return new FEmptyDepthStencilState(Initializer);
}


FBlendStateRHIRef FEmptyDynamicRHI::RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	return new FEmptyBlendState(Initializer);
}

