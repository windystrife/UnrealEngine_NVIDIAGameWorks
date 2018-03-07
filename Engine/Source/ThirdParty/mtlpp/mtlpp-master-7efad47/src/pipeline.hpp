// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "defines.hpp"
#include "device.hpp"

namespace mtlpp
{
	enum class Mutability
	{
		Default   = 0,
		Mutable   = 1,
		Immutable = 2,
	}
	MTLPP_AVAILABLE(10_13, 11_0);
	
	class PipelineBufferDescriptor : public ns::Object
	{
	public:
		PipelineBufferDescriptor();
		PipelineBufferDescriptor(ns::Handle const& h) : ns::Object(h) {}
		
		void SetMutability(Mutability m);
		Mutability GetMutability() const;
	}
	MTLPP_AVAILABLE(10_13, 11_0);
}

