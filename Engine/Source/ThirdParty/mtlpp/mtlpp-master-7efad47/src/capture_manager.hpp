// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "defines.hpp"
#include "ns.hpp"

namespace mtlpp
{
	class Device;
	class CaptureScope;
	class CommandQueue;
	
	class CaptureManager : public ns::Object
	{
		CaptureManager() { }
		CaptureManager(const ns::Handle& handle) : ns::Object(handle) { }
	public:
		static CaptureManager& SharedCaptureManager();
		
		CaptureScope NewCaptureScopeWithDevice(Device Device);
		CaptureScope NewCaptureScopeWithCommandQueue(CommandQueue Queue);
		
		void StartCaptureWithDevice(Device device);
		void StartCaptureWithCommandQueue(CommandQueue queue);
		void StartCaptureWithScope(CaptureScope scope);
		
		void StopCapture();
		
		CaptureScope GetDefaultCaptureScope() const;
		void SetDefaultCaptureScope(CaptureScope scope);
		
		bool IsCapturing() const;
	} MTLPP_AVAILABLE(10_13, 11_0);
	
}
