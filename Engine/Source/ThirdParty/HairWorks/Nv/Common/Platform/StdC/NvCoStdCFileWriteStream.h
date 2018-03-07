/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_STDC_FILE_WRITE_STREAM_H
#define NV_CO_STDC_FILE_WRITE_STREAM_H

#include <stdio.h>

#include <Nv/Common/NvCoStream.h>
#include <Nv/Common/NvCoString.h>

/** \addtogroup common 
@{ 
*/

namespace nvidia {
namespace Common {

/*! An implementation of WriteStream that works with StdC file types. NOTE! It is designed 
to only be allocated on the stack. This is so there is no dependency on a memory allocator, 
if you want to to allocate it on the heap, derive from it, and set a suitable operator new/operator delete
*/
class StdCFileWriteStream : public WriteStream
{
	NV_CO_DECLARE_STACK_POLYMORPHIC_CLASS(StdCFileWriteStream, WriteStream)

	NV_INLINE virtual Int64 write(const Void* data, Int64 numBytes) NV_OVERRIDE;
	NV_INLINE virtual Void flush() NV_OVERRIDE;
	NV_INLINE virtual Void close() NV_OVERRIDE;
	NV_INLINE virtual Bool isClosed() NV_OVERRIDE;

	NV_INLINE StdCFileWriteStream(const char* filename);
	NV_INLINE StdCFileWriteStream(const SubString& filename);

	StdCFileWriteStream(FILE* file) :m_file(file) {}
	~StdCFileWriteStream() { close(); }

	FILE* m_file;		///< if it's null means it has been closed. 
};

StdCFileWriteStream::StdCFileWriteStream(const SubString& filename)
{
	String buf(filename);
	m_file = ::fopen(buf.getCstr(), "wb");
}

StdCFileWriteStream::StdCFileWriteStream(const char* filename)
{
	m_file = ::fopen(filename, "wb");
}

Int64 StdCFileWriteStream::write(const Void* buffer, Int64 numBytes)
{
	if (m_file)
	{
		return ::fwrite(buffer, 1, size_t(numBytes), m_file);
	}
	return 0;
}

Void StdCFileWriteStream::flush()
{
	if (m_file)
	{
		::fflush(m_file);
	}
}

Void StdCFileWriteStream::close()
{
	if (m_file)
	{
		::fclose(m_file);
		m_file = NV_NULL;
	}
}

Bool StdCFileWriteStream::isClosed()
{
	return m_file == NV_NULL;
}

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_STDC_FILE_WRITE_STREAM_H