/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_DX12_HANDLE_H
#define NV_CO_DX12_HANDLE_H

#include <Nv/Common/NvCoApiHandle.h>

// Dx12 types
#include <d3d12.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common { 

/*!
\brief Description of how the intended rendering target is arranged. 
*/
struct Dx12TargetInfo
{
	Void init()
	{
		m_numRenderTargets = 1;
		m_renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		m_numSamples = 1;
		m_sampleQuality = 0;
		m_sampleMask = ~UInt32(0);
	}

	Int m_numRenderTargets;
	DXGI_FORMAT m_renderTargetFormats[8];			///< Format used for render target view access (the actual resource could be different!)
	DXGI_FORMAT m_depthStencilFormat;				///< Format to use for depth stencil view access (the actual resource could be different!)
	Int m_numSamples;								///< The number of multi sample samples
	Int m_sampleQuality;							///< The multi sample quality
	UInt32 m_sampleMask;							///< The sample mask 
};

/*!
\brief Specifies the kinds of types that can be wrapped in ApiHandle/ApiPtr/ConstPtr types */
class Dx12SubType { Dx12SubType(); public:
/*! SubTypes for Dx12 */
enum Enum
{
	UNKNOWN,	///< Unknown
	CONTEXT,	
	DEVICE,
	BUFFER,
	FLOAT32,
	CPU_DESCRIPTOR_HANDLE,
	COMMAND_QUEUE,
	TARGET_INFO,
	COUNT_OF,
};
}; 

// Handle types 
#define NV_DX12_HANDLE_TYPES(x)	\
	x(ID3D12Device, DEVICE, DEVICE) \
	x(ID3D12GraphicsCommandList, CONTEXT, CONTEXT) \
	x(ID3D12Resource, BUFFER, BUFFER) \
	x(ID3D12CommandQueue, COMMAND_QUEUE, UNKNOWN )

// The 'value' types - ie ones that can be passed as pointers, in addition to the handle types
#define NV_DX12_VALUE_TYPES(x) \
	x(Float32, FLOAT32, UNKNOWN) \
	x(D3D12_CPU_DESCRIPTOR_HANDLE, CPU_DESCRIPTOR_HANDLE, CPU_DESCRIPTOR_HANDLE) \
	x(Dx12TargetInfo, TARGET_INFO, UNKNOWN)

/*!
\brief A helper class to wrap Dx12 related types to ApiHandle, ApiPtr types, or extract via a cast the Dx12 types back out again. 

Some examples of how to wrap and cast handles and pointers

\code{.cpp}
ID3D12Device* device = ...; 

\\ To wrap as a handle
ApiHandle handle = Dx12Type::wrap(device); 

\\ To wrap an array or pointer you can use

Dx12TargetInfo target;
ApiPtr ptr = Dx12Type::wrapPtr(&target);

\\ If you want to convert a wrapped handle back you can use
ID3D12Device* device = Dx12Type::cast<ID3D12Device>(handle);

\\ Similarly to get a pointer
Dx12TargetInfo* target = Dx12Type::castPtr<Dx12TargetInfo>(ptr);
\endcode

*/
struct Dx12Type
{
	// Used by the macros. NOTE! Should be wrapping type (ie not the actual type - typically with E prefix)
	typedef Dx12SubType ScopeSubType;

		/// Get the full type for the subtype
	NV_FORCE_INLINE static Int getType(Dx12SubType::Enum subType) { return (Int(ApiType::DX12) << 8) | Int(subType); }
		/// Get the type via template, needed for arrays
	template <typename T>
	NV_FORCE_INLINE static Int getType() { return getType((T*)NV_NULL); }

		/// Implement getType	
	NV_DX12_HANDLE_TYPES(NV_CO_API_GET_TYPE)
		/// Implement getHandle, which will return a TypedApiHandle 
	NV_DX12_HANDLE_TYPES(NV_CO_API_WRAP)
		/// Implement getType for 'value types' (ie structs and others that shouldn't be in a handle)
	NV_DX12_VALUE_TYPES(NV_CO_API_GET_VALUE_TYPE)

		/// A template to work around warnings from dereferencing NV_NULL
	template <typename T>
	NV_FORCE_INLINE static Int getPtrType() { Void* data = NV_NULL; return getType(*(const T*)&data); }

		/// Get a pointer
	template <typename T>
	NV_FORCE_INLINE static ConstApiPtr wrapPtr(const T* in) { return ConstApiPtr(getPtrType<T>(), in); }
	template <typename T>
	NV_FORCE_INLINE static ApiPtr wrapPtr(T* in) { return ApiPtr(getPtrType<T>(), in); }

		/// Get from a handle
	template <typename T>
	NV_FORCE_INLINE static T* cast(const ApiHandle& in) { const Int type = getType((T*)NV_NULL); return reinterpret_cast<T*>((type == in.m_type) ? in.m_handle : NV_NULL); }
		/// Get from 
	template <typename T>
	NV_FORCE_INLINE static const T* cast(const ConstApiPtr& ptr) { const Int type = getPtrType<T>(); return reinterpret_cast<const T*>((ptr.m_type == type) ? ptr.getData() : NV_NULL); }
		// Get from 
	template <typename T>
	NV_FORCE_INLINE static T* cast(const ApiPtr& ptr) { const Int type = getPtrType<T>(); return reinterpret_cast<T*>((ptr.m_type == type) ? ptr.getData() : NV_NULL); }

		/// Get the sub type as text
	static const Char* getSubTypeText(Dx12SubType::Enum subType);

		/// If the match fails - implement casting. Writes impossible casts to Logger and returns NV_NULL.
	static Void* handlePtrCast(Int fromType, Int toType);
	static Void* handleCast(Int fromType, Int toType);
		/// Called when a cast isn't possible. Will output warnings to log and return NV_NULL
	static Void castFailure(Int fromType, Int toType);
		/// Log failure
	static Void logCastFailure(Int fromType, Int toType);
};

/* For generic handles you can use Dx11Handle. If you want the typed handle type use Dx11Type::wrap(texture) */
typedef WrapApiHandle<Dx12Type> Dx12Handle;

} // namespace Common
} // namespace nvidia

/** @} */

#endif // NV_CO_DX12_HANDLE_H
