/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#pragma once

#include "defines.hpp"
#include "ns.hpp"
#include "render_pass.hpp"
#include "command_encoder.hpp"

namespace mtlpp
{
    class RenderCommandEncoder;

    class ParallelRenderCommandEncoder : public ns::Object
    {
    public:
        ParallelRenderCommandEncoder() { }
        ParallelRenderCommandEncoder(const ns::Handle& handle) : ns::Object(handle) { }

        RenderCommandEncoder GetRenderCommandEncoder();

        void SetColorStoreAction(StoreAction storeAction, uint32_t colorAttachmentIndex) MTLPP_AVAILABLE(10_12, 10_0);
        void SetDepthStoreAction(StoreAction storeAction) MTLPP_AVAILABLE(10_12, 10_0);
        void SetStencilStoreAction(StoreAction storeAction) MTLPP_AVAILABLE(10_12, 10_0);
		
		void SetColorStoreActionOptions(StoreActionOptions options, uint32_t colorAttachmentIndex) MTLPP_AVAILABLE(10_13, 11_0);
		void SetDepthStoreActionOptions(StoreActionOptions options) MTLPP_AVAILABLE(10_13, 11_0);
		void SetStencilStoreActionOptions(StoreActionOptions options) MTLPP_AVAILABLE(10_13, 11_0);
    }
    MTLPP_AVAILABLE(10_11, 8_0);
}

