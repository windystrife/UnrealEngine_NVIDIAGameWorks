// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "capture_manager.hpp"
#include "capture_scope.hpp"
#include "device.hpp"
#include "command_queue.hpp"
#include <Metal/MTLCaptureManager.h>
#include <Metal/MTLCaptureScope.h>
#include <Metal/MTLDevice.h>
#include <Metal/MTLCommandQueue.h>

namespace mtlpp
{
	CaptureManager& CaptureManager::SharedCaptureManager()
	{
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		static CaptureManager CaptureMan(ns::Handle{ [MTLCaptureManager sharedCaptureManager] });
#else
		static CaptureManager;
#endif
		return CaptureMan;
	}
	
	CaptureScope CaptureManager::NewCaptureScopeWithDevice(Device Device)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ [(MTLCaptureManager*) m_ptr newCaptureScopeWithDevice:(id<MTLDevice>)Device.GetPtr()] };
#else
		return CaptureScope();
#endif
	}
	
	CaptureScope CaptureManager::NewCaptureScopeWithCommandQueue(CommandQueue Queue)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ [(MTLCaptureManager*) m_ptr newCaptureScopeWithCommandQueue:(id<MTLCommandQueue>)Queue.GetPtr()] };
#else
		return CaptureScope();
#endif
	}
	
	void CaptureManager::StartCaptureWithDevice(Device device)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(MTLCaptureManager*) m_ptr startCaptureWithDevice:(id<MTLDevice>)device.GetPtr()];
#endif
	}
	
	void CaptureManager::StartCaptureWithCommandQueue(CommandQueue queue)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(MTLCaptureManager*) m_ptr startCaptureWithCommandQueue:(id<MTLCommandQueue>)queue.GetPtr()];
#endif
	}
	
	void CaptureManager::StartCaptureWithScope(CaptureScope scope)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(MTLCaptureManager*) m_ptr startCaptureWithScope:(id<MTLCaptureScope>)scope.GetPtr()];
#endif
	}
	
	void CaptureManager::StopCapture()
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(MTLCaptureManager*) m_ptr stopCapture];
#endif
	}
	
	CaptureScope CaptureManager::GetDefaultCaptureScope() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ [(MTLCaptureManager*) m_ptr defaultCaptureScope] };
#else
		return CaptureScope();
#endif
	}
	
	void CaptureManager::SetDefaultCaptureScope(CaptureScope scope)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(MTLCaptureManager*) m_ptr setDefaultCaptureScope: (id<MTLCaptureScope>)scope.GetPtr()];
#endif
	}
	
	bool CaptureManager::IsCapturing() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(MTLCaptureManager*) m_ptr isCapturing];
#else
		return false;
#endif
	}
}

