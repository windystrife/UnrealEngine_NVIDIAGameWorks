/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_STDC_FILE_READ_STREAM_H
#define NV_CO_STDC_FILE_READ_STREAM_H

#include <stdio.h>

#include <Nv/Common/NvCoStream.h>
#include <Nv/Common/NvCoString.h>

/** \addtogroup common 
@{ 
*/

namespace nvidia {
namespace Common {

class StdCFileReadStream: public ReadStream
{
	NV_CO_DECLARE_STACK_POLYMORPHIC_CLASS(StdCFileReadStream, ReadStream)

	NV_INLINE virtual Int64 read(Void* buffer, Int64 numBytes) NV_OVERRIDE;
	NV_INLINE virtual Int64 seek(ESeekOrigin origin, Int64 changeInBytes) NV_OVERRIDE;
	NV_INLINE virtual Int64 tell() NV_OVERRIDE;
	NV_INLINE virtual Void close() NV_OVERRIDE;
	NV_INLINE virtual Bool isClosed() NV_OVERRIDE;

	/// Ctor with filename. Check if open was successful with isClosed() != false
	NV_INLINE StdCFileReadStream(const char* filename);
	NV_INLINE StdCFileReadStream(const SubString& filename);
	StdCFileReadStream(FILE* file):m_file(file) {}
	~StdCFileReadStream() { close(); }

	FILE* m_file;		///< if it's null means it has been closed. 
};

StdCFileReadStream::StdCFileReadStream(const SubString& filename)
{
	String buf(filename);
	m_file = ::fopen(buf.getCstr(), "rb");
}

StdCFileReadStream::StdCFileReadStream(const char* filename)
{
	m_file = ::fopen(filename, "rb");
}

Int64 StdCFileReadStream::read(Void* buffer, Int64 numBytes)
{
	if (m_file)
	{
		return ::fread(buffer, 1, size_t(numBytes), m_file); 
	}	
	return 0;
}

Int64 StdCFileReadStream::seek(ESeekOrigin origin, Int64 changeInBytes)
{
	if (m_file)
	{
		int orig;
		switch (origin)
		{
			default:
			case SeekOrigin::CURRENT:	orig = SEEK_CUR; break;
			case SeekOrigin::START:		orig = SEEK_SET; break;
			case SeekOrigin::END:		orig = SEEK_END; break;
		}
		::fseek(m_file, long int(changeInBytes), orig);
		return Int64(::ftell(m_file));
	}
	return 0;
}

Int64 StdCFileReadStream::tell()
{
	if (m_file)
	{
		return Int64(::ftell(m_file));
	}
	return 0;
}

Void StdCFileReadStream::close()
{
	if (m_file)
	{
		::fclose(m_file);
		m_file = NV_NULL;
	}
}

Bool StdCFileReadStream::isClosed()
{
	return m_file == NV_NULL;
}

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_STDC_FILE_READ_STREAM_H