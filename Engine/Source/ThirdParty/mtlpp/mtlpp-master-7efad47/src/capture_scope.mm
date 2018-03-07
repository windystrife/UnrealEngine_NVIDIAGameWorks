// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "capture_scope.hpp"
#include "device.hpp"
#include "command_queue.hpp"
#include <Metal/MTLCaptureScope.h>
#include <Metal/MTLDevice.h>
#include <Metal/MTLCommandQueue.h>
#include <Foundation/NSString.h>

namespace mtlpp
{
	void CaptureScope::BeginScope()
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLCaptureScope>) m_ptr beginScope];
#endif
	}
	
	void CaptureScope::EndScope()
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLCaptureScope>) m_ptr endScope];
#endif
	}
	
	ns::String   CaptureScope::GetLabel() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ [(id<MTLCaptureScope>) m_ptr label] };
#else
		return ns::String();
#endif
	}
	
	void CaptureScope::SetLabel(const ns::String& label)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLCaptureScope>) m_ptr setLabel:(NSString*)label.GetPtr()];
#endif
	}
	
	Device CaptureScope::GetDevice() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ [(id<MTLCaptureScope>) m_ptr device] };
#else
		return Device();
#endif
	}
	
	CommandQueue CaptureScope::GetCommandQueue() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ [(id<MTLCaptureScope>) m_ptr commandQueue] };
#else
		return CommandQueue();
#endif
	}
}
