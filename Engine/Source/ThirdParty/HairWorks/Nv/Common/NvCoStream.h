/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_STREAM_H
#define NV_CO_STREAM_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! The seek type options */
class SeekOrigin { SeekOrigin(); public: enum Enum
{
	CURRENT,		///< Relative to the current position
	START,			///< Relative to the start 
	END,			///< Relative to the end
}; };
typedef SeekOrigin::Enum ESeekOrigin;

/*! Read stream interface */
class ReadStream
{
	NV_CO_DECLARE_INTERFACE_BASE(ReadStream)

		/// \brief Read bytes from the stream 
		/// @param buffer Bytes read will be written here. 
		/// @param numBytes The number of bytes to read.
		/// @return Returns the amount of bytes read
	virtual Int64 read(Void* buffer, Int64 numBytes) = 0;
		/// \brief Seeks the read position relative to the origin by changeInBytes. 
		/// @param origin Defines where seeking should be relative to. 
		/// @param changeInBytes Defines how many bytes to move.
		/// @return The current absolute read position (ie same as tell).
	virtual Int64 seek(ESeekOrigin origin, Int64 changeInBytes) = 0;
		/// \brief Returns the current read position 
		/// @return The current absolute read position.
	virtual Int64 tell() = 0;
		/// Closes the stream if open 
	virtual Void close() = 0; 
		/// \brief Returns true if the stream is closed 
		/// @return True if stream is closed.
	virtual Bool isClosed() = 0;

	virtual ~ReadStream() {}
};

/*! Write stream interface */
class WriteStream
{
	NV_CO_DECLARE_INTERFACE_BASE(WriteStream)

		/// \brief Writes data to the stream. 
		/// @param data The source of the bytes to be written.
		/// @param numBytes The number of bytes to be written.
		/// @return The number of bytes actually written
	virtual Int64 write(const Void* data, Int64 numBytes) = 0;
		/// Flushes the stream. Forces any outstanding data not yet written to the destination (such as a file) to be written.
	virtual Void flush() = 0;
		/// \brief Closes the stream if is is open. If it is closed, it has no effect.
	virtual Void close() = 0;
		/// \brief Determines if the stream is closed. If the stream is closed no operations can take place on in
		/// @return True if the stream is closed
	virtual Bool isClosed() = 0;

	virtual ~WriteStream() {}
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_STREAM_H