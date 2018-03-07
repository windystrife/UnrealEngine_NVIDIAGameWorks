// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "argument_encoder.hpp"
#include "device.hpp"
#include "buffer.hpp"
#include "texture.hpp"
#include "sampler.hpp"
#include <Metal/MTLArgumentEncoder.h>

namespace mtlpp
{
	Device     ArgumentEncoder::GetDevice() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{[(id<MTLArgumentEncoder>) m_ptr device]};
#else
		return Device();
#endif
	}
	
	ns::String ArgumentEncoder::GetLabel() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{[(id<MTLArgumentEncoder>) m_ptr label]};
#else
		return ns::String();
#endif
	}
	
	uint32_t ArgumentEncoder::GetEncodedLength() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(id<MTLArgumentEncoder>) m_ptr encodedLength];
#else
		return 0;
#endif
	}

	uint32_t ArgumentEncoder::GetAlignment() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(id<MTLArgumentEncoder>) m_ptr alignment];
#else
		return 0;
#endif
	}
	
	void* ArgumentEncoder::GetConstantDataAtIndex(uint32_t index) const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(id<MTLArgumentEncoder>) m_ptr constantDataAtIndex:index];
#else
		return nullptr;
#endif
	}
	
	void ArgumentEncoder::SetLabel(const ns::String& label)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLArgumentEncoder>) m_ptr setLabel:(NSString*)label.GetPtr()];
#endif
	}
	
	void ArgumentEncoder::SetArgumentBuffer(const Buffer& buffer, uint32_t offset)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLArgumentEncoder>) m_ptr setArgumentBuffer:(id<MTLBuffer>)buffer.GetPtr() offset:offset];
#endif
	}
	
	void ArgumentEncoder::SetArgumentBuffer(const Buffer& buffer, uint32_t offset, uint32_t index)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLArgumentEncoder>) m_ptr setArgumentBuffer:(id<MTLBuffer>)buffer.GetPtr() startOffset:offset arrayElement:index];
#endif
	}
	
	void ArgumentEncoder::SetBuffer(const Buffer& buffer, uint32_t offset, uint32_t index)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLArgumentEncoder>) m_ptr setBuffer:(id<MTLBuffer>)buffer.GetPtr() offset:offset atIndex:index];
#endif
	}
	
	void ArgumentEncoder::SetBuffers(const Buffer* buffers, const unsigned long* offsets, const ns::Range& range)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		id<MTLBuffer>* array = (id<MTLBuffer>*)alloca(range.Length * sizeof(id<MTLBuffer>));
		for (uint32_t i = 0; i < range.Length; i++)
			array[i] = (id<MTLBuffer>)buffers[i].GetPtr();
		
		[(id<MTLArgumentEncoder>) m_ptr setBuffers:array offsets:offsets withRange:NSMakeRange(range.Location, range.Length)];
#endif
	}
	
	void ArgumentEncoder::SetTexture(const Texture& texture, uint32_t index)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLArgumentEncoder>) m_ptr setTexture:(id<MTLTexture>)texture.GetPtr() atIndex:index];
#endif
	}
	
	void ArgumentEncoder::SetTextures(const Texture* textures, const ns::Range& range)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		id<MTLTexture>* array = (id<MTLTexture>*)alloca(range.Length * sizeof(id<MTLTexture>));
		for (uint32_t i = 0; i < range.Length; i++)
			array[i] = (id<MTLTexture>)textures[i].GetPtr();
		
		[(id<MTLArgumentEncoder>) m_ptr setTextures:array withRange:NSMakeRange(range.Location, range.Length)];
#endif
	}
	
	void ArgumentEncoder::SetSamplerState(const SamplerState& sampler, uint32_t index)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		[(id<MTLArgumentEncoder>) m_ptr setSamplerState:(id<MTLSamplerState>)sampler.GetPtr() atIndex:index];
#endif
	}
	
	void ArgumentEncoder::SetSamplerStates(const SamplerState* samplers, const ns::Range& range)
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		id<MTLSamplerState>* array = (id<MTLSamplerState>*)alloca(range.Length * sizeof(id<MTLSamplerState>));
		for (uint32_t i = 0; i < range.Length; i++)
			array[i] = (id<MTLSamplerState>)samplers[i].GetPtr();

		[(id<MTLArgumentEncoder>) m_ptr setSamplerStates:array withRange:NSMakeRange(range.Location, range.Length)];
#endif
	}
	
	ArgumentEncoder ArgumentEncoder::NewArgumentEncoderForBufferAtIndex(uint32_t index)
	{
		Validate();
#if MTLPP_IS_AVAILABLE_MAC(10_13)
		return ns::Handle{[(id<MTLArgumentEncoder>) m_ptr newArgumentEncoderForBufferAtIndex:index]};
#else
		return ArgumentEncoder();
#endif
	}
}
