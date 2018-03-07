// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "defines.hpp"
#include "ns.hpp"

namespace mtlpp
{
	class Device;
	class Buffer;
	class Texture;
	class SamplerState;
	
	class ArgumentEncoder : public ns::Object
	{
	public:
		ArgumentEncoder() { }
		ArgumentEncoder(const ns::Handle& handle) : ns::Object(handle) { }
		
		Device     GetDevice() const;
		ns::String GetLabel() const;
		uint32_t GetEncodedLength() const;
		uint32_t GetAlignment() const;
		void* GetConstantDataAtIndex(uint32_t index) const;
		
		void SetLabel(const ns::String& label);
		
		void SetArgumentBuffer(const Buffer& buffer, uint32_t offset);
		void SetArgumentBuffer(const Buffer& buffer, uint32_t offset, uint32_t index);
		
		void SetBuffer(const Buffer& buffer, uint32_t offset, uint32_t index);
		void SetBuffers(const Buffer* buffers, const unsigned long* offsets, const ns::Range& range);
		void SetTexture(const Texture& texture, uint32_t index);
		void SetTextures(const Texture* textures, const ns::Range& range);
		void SetSamplerState(const SamplerState& sampler, uint32_t index);
		void SetSamplerStates(const SamplerState* samplers, const ns::Range& range);

		ArgumentEncoder NewArgumentEncoderForBufferAtIndex(uint32_t index) MTLPP_AVAILABLE_MAC(10_13);
	}
	MTLPP_AVAILABLE(10_13, 11_0);
}
