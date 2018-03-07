/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_QUEUE_UTIL_H
#define NV_CO_QUEUE_UTIL_H

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*!
\brief Provides an implementation that can be used across multiple instantiations of the Queue container 
template. */
struct QueueUtil
{
		/// Changes the capacity of a queue. queueIn must be an instantiation of Queue container template 
	static Void increaseCapacity(Void* queueIn, IndexT extraCapacity, SizeT extraCapacityInBytes);
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_QUEUE_UTIL_H
