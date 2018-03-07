/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#pragma once

#include "defines.hpp"
#include "ns.hpp"
#include "device.hpp"


namespace mtlpp
{
    class Fence : public ns::Object
    {
    public:
        Fence() { }
        Fence(const ns::Handle& handle) : ns::Object(handle) { }

        Device    GetDevice() const;
        ns::String GetLabel() const;

        void SetLabel(const ns::String& label);
    }
    MTLPP_AVAILABLE(10_13, 10_0);
}
