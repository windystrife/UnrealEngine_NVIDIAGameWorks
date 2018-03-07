// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "defines.hpp"
#include "ns.hpp"

namespace mtlpp
{
	class Device;
	class CommandQueue;
	
	class CaptureScope : public ns::Object
	{
	public:
		CaptureScope() { }
		CaptureScope(const ns::Handle& handle) : ns::Object(handle) { }
		
		void BeginScope();
		void EndScope();

		ns::String   GetLabel() const;
		void SetLabel(const ns::String& label);
		
		Device GetDevice() const;
		CommandQueue GetCommandQueue() const;
	} MTLPP_AVAILABLE(10_13, 11_0);
	
}
