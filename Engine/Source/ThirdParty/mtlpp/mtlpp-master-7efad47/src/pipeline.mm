// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "pipeline.hpp"
#include <Metal/MTLPipeline.h>

namespace mtlpp
{
	PipelineBufferDescriptor::PipelineBufferDescriptor()
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
	: ns::Object(ns::Handle{ [MTLPipelineBufferDescriptor new] })
#endif
	{
	}
	
	void PipelineBufferDescriptor::SetMutability(Mutability m)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(MTLPipelineBufferDescriptor*)m_ptr setMutability:(MTLMutability)m];
#endif
	}
	
	Mutability PipelineBufferDescriptor::GetMutability() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return (Mutability)[(MTLPipelineBufferDescriptor*)m_ptr mutability];
#else
		return 0;
#endif
	}
	
}
