/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_COM_TYPES_H
#define NV_CO_COM_TYPES_H

#include <Nv/Core/1.0/NvResult.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

struct Guid
{
	UInt32 m_data1;     ///< Low field of the timestamp
	UInt16 m_data2;     ///< Middle field of the timestamp
	UInt16 m_data3;     ///< High field of the timestamp with multiplexed version number
	UInt8  m_data4[8];  ///< 0, 1 = clock_seq_hi_and_reserved, clock_seq_low, followed by 'spatially unique node' (48 bits) 
};

/// ! Must be kept in sync with IUnknown
class IForwardUnknown
{
public:
	virtual NV_NO_THROW Result NV_MCALL QueryInterface(const Guid& iid, Void* objOut) = 0;
	virtual NV_NO_THROW UInt NV_MCALL AddRef() = 0;
	virtual NV_NO_THROW UInt NV_MCALL Release() = 0;
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_COM_TYPES_H
