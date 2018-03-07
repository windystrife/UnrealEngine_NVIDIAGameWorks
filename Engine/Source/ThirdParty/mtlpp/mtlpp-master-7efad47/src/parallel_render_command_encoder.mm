/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#include "parallel_render_command_encoder.hpp"
#include "render_command_encoder.hpp"
#include <Metal/MTLParallelRenderCommandEncoder.h>

namespace mtlpp
{
    RenderCommandEncoder ParallelRenderCommandEncoder::GetRenderCommandEncoder()
    {
        Validate();
        return ns::Handle { (__bridge void*)[(__bridge id<MTLParallelRenderCommandEncoder>)m_ptr renderCommandEncoder] };
    }

    void ParallelRenderCommandEncoder::SetColorStoreAction(StoreAction storeAction, uint32_t colorAttachmentIndex)
    {
        Validate();
#if MTLPP_IS_AVAILABLE(10_12, 10_0)
        [(__bridge id<MTLParallelRenderCommandEncoder>)m_ptr setColorStoreAction:MTLStoreAction(storeAction) atIndex:colorAttachmentIndex];
#endif
    }

    void ParallelRenderCommandEncoder::SetDepthStoreAction(StoreAction storeAction)
    {
        Validate();
#if MTLPP_IS_AVAILABLE(10_12, 10_0)
        [(__bridge id<MTLParallelRenderCommandEncoder>)m_ptr setDepthStoreAction:MTLStoreAction(storeAction)];
#endif
    }

    void ParallelRenderCommandEncoder::SetStencilStoreAction(StoreAction storeAction)
    {
        Validate();
#if MTLPP_IS_AVAILABLE(10_12, 10_0)
        [(__bridge id<MTLParallelRenderCommandEncoder>)m_ptr setStencilStoreAction:MTLStoreAction(storeAction)];
#endif
    }
	
	void ParallelRenderCommandEncoder::SetColorStoreActionOptions(StoreActionOptions options, uint32_t colorAttachmentIndex)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(__bridge id<MTLParallelRenderCommandEncoder>)m_ptr setColorStoreActionOptions:(MTLStoreActionOptions)options atIndex:colorAttachmentIndex];
#endif
	}
	
	void ParallelRenderCommandEncoder::SetDepthStoreActionOptions(StoreActionOptions options)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(__bridge id<MTLParallelRenderCommandEncoder>)m_ptr setDepthStoreActionOptions:(MTLStoreActionOptions)options];
#endif
	}
	
	void ParallelRenderCommandEncoder::SetStencilStoreActionOptions(StoreActionOptions options)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(__bridge id<MTLParallelRenderCommandEncoder>)m_ptr setStencilStoreActionOptions:(MTLStoreActionOptions)options];
#endif
	}
}
