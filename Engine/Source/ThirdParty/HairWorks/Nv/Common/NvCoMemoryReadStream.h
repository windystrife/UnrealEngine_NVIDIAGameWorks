/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_MEMORY_READ_STREAM_H
#define NV_CO_MEMORY_READ_STREAM_H

#include <Nv/Common/NvCoStream.h>
#include <Nv/Common/NvCoMemory.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! Simple read implementation of ReadStream. 
NOTE! This implementation does not copy the memory passed to it, so for the stream to be functional 
it must be in scope for the same time as the stream */
class MemoryReadStream: public ReadStream
{
	NV_CO_DECLARE_STACK_POLYMORPHIC_CLASS(MemoryReadStream, ReadStream)

	NV_INLINE virtual Int64 read(Void* buffer, Int64 numBytes) NV_OVERRIDE;
	NV_INLINE virtual Int64 seek(ESeekOrigin origin, Int64 changeInBytes) NV_OVERRIDE;
	NV_INLINE virtual Int64 tell() NV_OVERRIDE;
	NV_INLINE virtual Void close() NV_OVERRIDE;
	NV_INLINE virtual Bool isClosed() NV_OVERRIDE;

	/// Ctor with filename. Check if open was successful with isClosed() != false
	MemoryReadStream(const Void* data, SizeT size):m_data((const UInt8*)data), m_size(size), m_position(0) {}
	~MemoryReadStream() { close(); }

	SizeT m_position;
	SizeT m_size;
	const UInt8* m_data;	
};

Int64 MemoryReadStream::read(Void* buffer, Int64 numBytesIn)
{
	NV_CORE_ASSERT(numBytesIn >= 0);
	NV_CORE_ASSERT(m_position + numBytesIn < ~SizeT(0));
	SizeT newPos = m_position + SizeT(numBytesIn);
	newPos = (newPos > m_size) ? m_size : newPos;
	SizeT size = newPos - m_position;
	if (size > 0)
	{
		Memory::copy(buffer, m_data + m_position, SizeT(size));
	}
	m_position = newPos;
	return newPos;
}

Int64 MemoryReadStream::seek(ESeekOrigin origin, Int64 changeInBytes)
{
	Int64 newPos;
	// Work out new pos relative to the origin
	switch (origin)
	{
		default:
		case SeekOrigin::START:	newPos = changeInBytes; break;
		case SeekOrigin::END:	newPos = Int64(m_size) + changeInBytes; break;
		case SeekOrigin::CURRENT: newPos = Int64(m_position) + changeInBytes; break;
	}
	// Clamp new pos
	newPos = (newPos < 0) ? 0 : newPos;
	newPos = (newPos > Int64(m_size)) ? Int64(m_size) : newPos;
	// Set the new position
	m_position = SizeT(newPos);
	// Return absolute position
	return newPos;
}

Int64 MemoryReadStream::tell()
{	
	return Int64(m_position);
}

Void MemoryReadStream::close()
{
	m_data = NV_NULL;
	m_size = 0;
	m_position = 0;
}

Bool MemoryReadStream::isClosed()
{
	return m_data == NV_NULL;
}

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_MEMORY_READ_STREAM_H