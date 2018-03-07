//
// Copyright 2014, 2015 Razer Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#if !PLATFORM_WINDOWS
#include "IOSVR.h"
#include "OSVRCustomPresent.h"

#include <osvr/RenderKit/RenderManagerC.h>

// @todo OpenGL implementation
class FCurrentCustomPresent : public FOSVRCustomPresent<void*>
{
public:
    FCurrentCustomPresent(OSVR_ClientContext clientContext) :
        FOSVRCustomPresent(clientContext)
    {
    }

protected:
    //virtual osvr::renderkit::GraphicsLibrary CreateGraphicsLibrary() override
    //{
    //    osvr::renderkit::GraphicsLibrary ret;
    //    // OpenGL path not implemented yet
    //    return ret;
    //}

    virtual FString GetGraphicsLibraryName() override
    {
        return FString("OpenGL");
    }

    virtual bool ShouldFlipY() override
    {
        return false;
    }

};

#endif
