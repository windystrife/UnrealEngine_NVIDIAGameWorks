/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#include "blit_command_encoder.hpp"
#include <Metal/MTLBlitCommandEncoder.h>

namespace mtlpp
{
    void BlitCommandEncoder::Synchronize(const Resource& resource)
    {
        Validate();
#if MTLPP_PLATFORM_MAC
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            synchronizeResource:(__bridge id<MTLResource>)resource.GetPtr()];
#endif
    }

    void BlitCommandEncoder::Synchronize(const Texture& texture, uint32_t slice, uint32_t level)
    {
        Validate();
#if MTLPP_PLATFORM_MAC
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            synchronizeTexture:(__bridge id<MTLTexture>)texture.GetPtr()
            slice:slice
            level:level];
#endif
    }

    void BlitCommandEncoder::Copy(const Texture& sourceTexture, uint32_t sourceSlice, uint32_t sourceLevel, const Origin& sourceOrigin, const Size& sourceSize, const Texture& destinationTexture, uint32_t destinationSlice, uint32_t destinationLevel, const Origin& destinationOrigin)
    {
        Validate();
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            copyFromTexture:(__bridge id<MTLTexture>)sourceTexture.GetPtr()
            sourceSlice:sourceSlice
            sourceLevel:sourceLevel
            sourceOrigin:MTLOriginMake(sourceOrigin.X, sourceOrigin.Y, sourceOrigin.Z)
            sourceSize:MTLSizeMake(sourceSize.Width, sourceSize.Height, sourceSize.Depth)
            toTexture:(__bridge id<MTLTexture>)destinationTexture.GetPtr()
            destinationSlice:destinationSlice
            destinationLevel:destinationLevel
            destinationOrigin:MTLOriginMake(destinationOrigin.X, destinationOrigin.Y, destinationOrigin.Z)];
    }

    void BlitCommandEncoder::Copy(const Buffer& sourceBuffer, uint32_t sourceOffset, uint32_t sourceBytesPerRow, uint32_t sourceBytesPerImage, const Size& sourceSize, const Texture& destinationTexture, uint32_t destinationSlice, uint32_t destinationLevel, const Origin& destinationOrigin)
    {
        Validate();
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            copyFromBuffer:(__bridge id<MTLBuffer>)sourceBuffer.GetPtr()
            sourceOffset:sourceOffset
            sourceBytesPerRow:sourceBytesPerRow
            sourceBytesPerImage:sourceBytesPerImage
            sourceSize:MTLSizeMake(sourceSize.Width, sourceSize.Height, sourceSize.Depth)
            toTexture:(__bridge id<MTLTexture>)destinationTexture.GetPtr()
            destinationSlice:destinationSlice
            destinationLevel:destinationLevel
            destinationOrigin:MTLOriginMake(destinationOrigin.X, destinationOrigin.Y, destinationOrigin.Z)];
    }

    void BlitCommandEncoder::Copy(const Buffer& sourceBuffer, uint32_t sourceOffset, uint32_t sourceBytesPerRow, uint32_t sourceBytesPerImage, const Size& sourceSize, const Texture& destinationTexture, uint32_t destinationSlice, uint32_t destinationLevel, const Origin& destinationOrigin, BlitOption options)
    {
        Validate();
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            copyFromBuffer:(__bridge id<MTLBuffer>)sourceBuffer.GetPtr()
            sourceOffset:sourceOffset
            sourceBytesPerRow:sourceBytesPerRow
            sourceBytesPerImage:sourceBytesPerImage
            sourceSize:MTLSizeMake(sourceSize.Width, sourceSize.Height, sourceSize.Depth)
            toTexture:(__bridge id<MTLTexture>)destinationTexture.GetPtr()
            destinationSlice:destinationSlice
            destinationLevel:destinationLevel
            destinationOrigin:MTLOriginMake(destinationOrigin.X, destinationOrigin.Y, destinationOrigin.Z)
            options:MTLBlitOption(options)];
    }

    void BlitCommandEncoder::Copy(const Texture& sourceTexture, uint32_t sourceSlice, uint32_t sourceLevel, const Origin& sourceOrigin, const Size& sourceSize, const Buffer& destinationBuffer, uint32_t destinationOffset, uint32_t destinationBytesPerRow, uint32_t destinationBytesPerImage)
    {
        Validate();
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            copyFromTexture:(__bridge id<MTLTexture>)sourceTexture.GetPtr()
            sourceSlice:sourceSlice
            sourceLevel:sourceLevel
            sourceOrigin:MTLOriginMake(sourceOrigin.X, sourceOrigin.Y, sourceOrigin.Z)
            sourceSize:MTLSizeMake(sourceSize.Width, sourceSize.Height, sourceSize.Depth)
            toBuffer:(__bridge id<MTLBuffer>)destinationBuffer.GetPtr()
            destinationOffset:destinationOffset
            destinationBytesPerRow:destinationBytesPerRow
            destinationBytesPerImage:destinationBytesPerImage];
    }

    void BlitCommandEncoder::Copy(const Texture& sourceTexture, uint32_t sourceSlice, uint32_t sourceLevel, const Origin& sourceOrigin, const Size& sourceSize, const Buffer& destinationBuffer, uint32_t destinationOffset, uint32_t destinationBytesPerRow, uint32_t destinationBytesPerImage, BlitOption options)
    {
        Validate();
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            copyFromTexture:(__bridge id<MTLTexture>)sourceTexture.GetPtr()
            sourceSlice:sourceSlice
            sourceLevel:sourceLevel
            sourceOrigin:MTLOriginMake(sourceOrigin.X, sourceOrigin.Y, sourceOrigin.Z)
            sourceSize:MTLSizeMake(sourceSize.Width, sourceSize.Height, sourceSize.Depth)
            toBuffer:(__bridge id<MTLBuffer>)destinationBuffer.GetPtr()
            destinationOffset:destinationOffset
            destinationBytesPerRow:destinationBytesPerRow
            destinationBytesPerImage:destinationBytesPerImage
            options:MTLBlitOption(options)];
    }

    void BlitCommandEncoder::Copy(const Buffer& sourceBuffer, uint32_t sourceOffset, const Buffer& destinationBuffer, uint32_t destinationOffset, uint32_t size)
    {
        Validate();
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            copyFromBuffer:(__bridge id<MTLBuffer>)sourceBuffer.GetPtr()
            sourceOffset:sourceOffset
            toBuffer:(__bridge id<MTLBuffer>)destinationBuffer.GetPtr()
            destinationOffset:destinationOffset
            size:size];
    }

    void BlitCommandEncoder::GenerateMipmaps(const Texture& texture)
    {
        Validate();
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            generateMipmapsForTexture:(__bridge id<MTLTexture>)texture.GetPtr()];
    }

    void BlitCommandEncoder::Fill(const Buffer& buffer, const ns::Range& range, uint8_t value)
    {
        Validate();
        [(__bridge id<MTLBlitCommandEncoder>)m_ptr
            fillBuffer:(__bridge id<MTLBuffer>)buffer.GetPtr()
            range:NSMakeRange(range.Location, range.Length)
            value:value];
    }

    void BlitCommandEncoder::UpdateFence(const Fence& fence)
    {
        Validate();
		if(@available(macOS 10.13, iOS 10.0, *))
			[(__bridge id<MTLBlitCommandEncoder>)m_ptr updateFence:(__bridge id<MTLFence>)fence.GetPtr()];
    }

    void BlitCommandEncoder::WaitForFence(const Fence& fence)
    {
		Validate();
		if(@available(macOS 10.13, iOS 10.0, *))
			[(__bridge id<MTLBlitCommandEncoder>)m_ptr waitForFence:(__bridge id<MTLFence>)fence.GetPtr()];
    }
}
