/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#include "argument.hpp"
#include <Metal/MTLArgument.h>

namespace mtlpp
{
	Type::Type() :
	ns::Object(ns::Handle{[[MTLType alloc] init]}, false)
	{
		
	}
	
	DataType Type::GetDataType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return DataType([(__bridge MTLType*)m_ptr dataType]);
#else
		return 0;
#endif
	}
	
	TextureReferenceType::TextureReferenceType() :
	ns::Object(ns::Handle{[[MTLTextureReferenceType alloc] init]}, false)
	{
		
	}
	
	DataType   TextureReferenceType::GetTextureDataType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return DataType([(__bridge MTLTextureReferenceType*)m_ptr textureDataType]);
#else
		return 0;
#endif
	}
	
	TextureType   TextureReferenceType::GetTextureType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return TextureType([(__bridge MTLTextureReferenceType*)m_ptr textureType]);
#else
		return 0;
#endif
	}
	
	ArgumentAccess TextureReferenceType::GetAccess() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ArgumentAccess([(__bridge MTLTextureReferenceType*)m_ptr access]);
#else
		return 0;
#endif
	}
	
	bool TextureReferenceType::IsDepthTexture() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ([(__bridge MTLTextureReferenceType*)m_ptr isDepthTexture]);
#else
		return false;
#endif
	}
	
	PointerType::PointerType() :
	ns::Object(ns::Handle{[[MTLPointerType alloc] init]}, false)
	{
	}
	
	DataType   PointerType::GetElementType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return DataType([(__bridge MTLPointerType*)m_ptr elementType]);
#else
		return 0;
#endif
	}
	
	ArgumentAccess PointerType::GetAccess() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ArgumentAccess([(__bridge MTLPointerType*)m_ptr access]);
#else
		return 0;
#endif
	}
	
	uint32_t PointerType::GetAlignment() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ([(__bridge MTLPointerType*)m_ptr alignment]);
#else
		return 0;
#endif
	}
	
	uint32_t PointerType::GetDataSize() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ([(__bridge MTLPointerType*)m_ptr dataSize]);
#else
		return 0;
#endif
	}
	
	bool PointerType::GetElementIsArgumentBuffer() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ([(__bridge MTLPointerType*)m_ptr elementIsArgumentBuffer]);
#else
		return false;
#endif
	}
	
	StructType PointerType::GetElementStructType()
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return StructType(ns::Handle{[(__bridge MTLPointerType*)m_ptr elementStructType]});
#else
		return StructType();
#endif
	}
	
	ArrayType PointerType::GetElementArrayType()
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ArrayType(ns::Handle{[(__bridge MTLTextureReferenceType*)m_ptr elementArrayType]});
#else
		return ArrayType();
#endif
	}
	
    StructMember::StructMember() :
        ns::Object(ns::Handle{ (__bridge void*)[[MTLStructMember alloc] init] }, false)
    {
    }

    ns::String StructMember::GetName() const
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge MTLStructMember*)m_ptr name] };
    }

    uint32_t StructMember::GetOffset() const
    {
        Validate();
        return uint32_t([(__bridge MTLStructMember*)m_ptr offset]);
    }

    DataType StructMember::GetDataType() const
    {
        Validate();
        return DataType([(__bridge MTLStructMember*)m_ptr dataType]);
    }

    StructType StructMember::GetStructType() const
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge MTLStructMember*)m_ptr structType] };
    }

    ArrayType StructMember::GetArrayType() const
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge MTLStructMember*)m_ptr arrayType] };
    }

	TextureReferenceType StructMember::GetTextureReferenceType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ (__bridge void*)[(__bridge MTLStructMember*)m_ptr textureReferenceType] };
#else
		return TextureReferenceType();
#endif
	}
	
	PointerType StructMember::GetPointerType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ (__bridge void*)[(__bridge MTLStructMember*)m_ptr pointerType] };
#else
		return PointerType();
#endif
	}
	
	uint64_t StructMember::GetArgumentIndex() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(__bridge MTLStructMember*)m_ptr argumentIndex];
#else
		return 0;
#endif
	}
	
    StructType::StructType() :
        ns::Object(ns::Handle{ (__bridge void*)[[MTLStructType alloc] init] }, false)
    {
    }

    const ns::Array<StructMember> StructType::GetMembers() const
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge MTLStructType*)m_ptr members] };
    }

    StructMember StructType::GetMember(const ns::String& name) const
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge MTLStructType*)m_ptr memberByName:(__bridge NSString*)name.GetPtr()] };
    }

    ArrayType::ArrayType() :
        ns::Object(ns::Handle{ (__bridge void*)[[MTLArrayType alloc] init] }, false)
    {
    }

    uint32_t ArrayType::GetArrayLength() const
    {
        Validate();
        return uint32_t([(__bridge MTLArrayType*)m_ptr arrayLength]);
    }

    DataType ArrayType::GetElementType() const
    {
        Validate();
        return DataType([(__bridge MTLArrayType*)m_ptr elementType]);
    }

    uint32_t ArrayType::GetStride() const
    {
        Validate();
        return uint32_t([(__bridge MTLArrayType*)m_ptr stride]);
    }

    StructType ArrayType::GetElementStructType() const
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge MTLArrayType*)m_ptr elementStructType] };
    }

    ArrayType ArrayType::GetElementArrayType() const
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge MTLArrayType*)m_ptr elementArrayType] };
    }
	
	uint64_t ArrayType::GetArgumentIndexStride() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return [(__bridge MTLArrayType*)m_ptr argumentIndexStride];
#else
		return 0;
#endif
	}
	
	TextureReferenceType ArrayType::GetElementTextureReferenceType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ (__bridge void*)[(__bridge MTLArrayType*)m_ptr elementTextureReferenceType] };
#else
		return TextureReferenceType();
#endif
	}
	
	PointerType ArrayType::GetElementPointerType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle{ (__bridge void*)[(__bridge MTLArrayType*)m_ptr elementPointerType] };
#else
		return PointerType();
#endif
	}

    Argument::Argument() :
        ns::Object(ns::Handle{ (__bridge void*)[[MTLArgument alloc] init] }, false)
    {
    }

    ns::String Argument::GetName() const
    {
        Validate();
        return ns::Handle{ (__bridge void*)[(__bridge MTLArgument*)m_ptr name] };
    }

    ArgumentType Argument::GetType() const
    {
        Validate();
        return ArgumentType([(__bridge MTLArgument*)m_ptr type]);
    }

    ArgumentAccess Argument::GetAccess() const
    {
        Validate();
        return ArgumentAccess([(__bridge MTLArgument*)m_ptr access]);
    }

    uint32_t Argument::GetIndex() const
    {
        Validate();
        return uint32_t([(__bridge MTLArgument*)m_ptr index]);
    }

    bool Argument::IsActive() const
    {
        Validate();
        return [(__bridge MTLArgument*)m_ptr isActive];
    }

    uint32_t Argument::GetBufferAlignment() const
    {
        Validate();
        return uint32_t([(__bridge MTLArgument*)m_ptr bufferAlignment]);
    }

    uint32_t Argument::GetBufferDataSize() const
    {
        Validate();
        return uint32_t([(__bridge MTLArgument*)m_ptr bufferDataSize]);
    }

    DataType Argument::GetBufferDataType() const
    {
        Validate();
        return DataType([(__bridge MTLArgument*)m_ptr bufferDataType]);
    }

    StructType Argument::GetBufferStructType() const
    {
        Validate();
        return StructType(ns::Handle { (__bridge void*)[(__bridge MTLArgument*)m_ptr bufferStructType] });
    }
	
	PointerType	   Argument::GetBufferPointerType() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return ns::Handle { (__bridge void*)[(__bridge MTLArgument*)m_ptr bufferPointerType] };
#else
		return PointerType();
#endif
	}

    uint32_t Argument::GetThreadgroupMemoryAlignment() const
    {
        Validate();
        return uint32_t([(__bridge MTLArgument*)m_ptr threadgroupMemoryAlignment]);
    }

    uint32_t Argument::GetThreadgroupMemoryDataSize() const
    {
        Validate();
        return uint32_t([(__bridge MTLArgument*)m_ptr threadgroupMemoryDataSize]);
    }

    TextureType Argument::GetTextureType() const
    {
        Validate();
        return TextureType([(__bridge MTLArgument*)m_ptr textureType]);
    }

    DataType Argument::GetTextureDataType() const
    {
        Validate();
        return DataType([(__bridge MTLArgument*)m_ptr textureDataType]);
    }

    bool Argument::IsDepthTexture() const
    {
        Validate();
#if MTLPP_IS_AVAILABLE(10_12, 10_0)
        return [(__bridge MTLArgument*)m_ptr isDepthTexture];
#else
        return false;
#endif
    }
	
	uint32_t       Argument::GetArrayLength() const
	{
		Validate();
#if MTLPP_IS_AVAILABLE(10_13, 11_0)
		return uint32_t([(__bridge MTLArgument*)m_ptr arrayLength]);
#else
		return 0;
#endif
	}
}
