/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_DX11_HANDLE_H
#define NV_CO_DX11_HANDLE_H

#include <Nv/Common/NvCoApiHandle.h>

// Dx11 types
#include <dxgi.h>
#include <d3d11.h>

/** \addtogroup common 
@{ 
*/

namespace nvidia {
namespace Common {

/*!
\brief Specifies the kinds of types that can be wrapped in ApiHandle/ApiPtr/ConstPtr types */
class Dx11SubType { Dx11SubType(); public: enum Enum
{
	UNKNOWN,	///< Unknown
	CONTEXT,	
	DEVICE,
	BUFFER,
	FLOAT32,
	SHADER_RESOURCE_VIEW,
	DEPTH_STENCIL_VIEW,
	COUNT_OF,
}; }; 
typedef Dx11SubType::Enum EDx11SubType;

// Handle types 
#define NV_DX11_HANDLE_TYPES(x)	\
	x(ID3D11Device, DEVICE, DEVICE) \
	x(ID3D11DeviceContext, CONTEXT, CONTEXT) \
	x(ID3D11Buffer, BUFFER, BUFFER) \
	x(ID3D11ShaderResourceView, SHADER_RESOURCE_VIEW, UNKNOWN) \
	x(ID3D11DepthStencilView, DEPTH_STENCIL_VIEW, UNKNOWN)

// The 'value' types - ie ones that can be passed as pointers, in addition to the handle types
#define NV_DX11_VALUE_TYPES(x) \
	x(Float32, FLOAT32, UNKNOWN) 


/*!
\brief A helper class to wrap Dx11 related types to ApiHandle, ApiPtr types, or extract via a cast the Dx11 types back out again.

Some examples of how to wrap and cast handles and pointers

\code{.cpp}
ID3D11Device* device = ...;

\\ To wrap as a handle
ApiHandle handle = Dx11Type::wrap(device);

\\ To wrap an array or pointer you can use

Float32 values[10];
ApiPtr ptr = Dx12Type::wrapPtr(values);

\\ If you want to convert a wrapped handle back you can use
ID3D11Device* device = Dx11Type::cast<ID3D11Device>(handle);

\\ Similarly to get a pointer
Float32* target = Dx11Type::castPtr<Float32>(ptr);
\endcode
*/
struct Dx11Type
{
	// Used by the macros. NOTE! Should be wrapping type (ie not the actual type - typically with E prefix)
	typedef Dx11SubType ScopeSubType;

		/// Get the full type for the subtype
	NV_FORCE_INLINE static Int getType(EDx11SubType subType) { return (Int(ApiType::DX11) << 8) | Int(subType); }
		/// Get the type via template, needed for arrays
	template <typename T>
	NV_FORCE_INLINE static Int getType() { return getType((T*)NV_NULL); }

		/// Implement getType	
	NV_DX11_HANDLE_TYPES(NV_CO_API_GET_TYPE)
		/// Implement getHandle, which will return a TypedApiHandle 
	NV_DX11_HANDLE_TYPES(NV_CO_API_WRAP)
		/// Implement getType for 'value types' (ie structs and others that shouldn't be in a handle)
	NV_DX11_VALUE_TYPES(NV_CO_API_GET_VALUE_TYPE)

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
	NV_FORCE_INLINE static T* cast(const ApiHandle& in) { const Int type = getType((T*)NV_NULL); return reinterpret_cast<T*>((type == in.m_type) ? in.m_handle : handleCast(in.m_type, type)); }

		/// Get from 
	template <typename T>
	NV_FORCE_INLINE static const T* cast(const ConstApiPtr& ptr) { const Int type = getPtrType<T>(); return reinterpret_cast<const T*>((ptr.m_type == type) ? ptr.getData() : handlePtrCast(ptr.m_type, type)); }
		// Get from 
	template <typename T>
	NV_FORCE_INLINE static T* cast(const ApiPtr& ptr) { const Int type = getPtrType<T>(); return reinterpret_cast<T*>((ptr.m_type == type) ? ptr.getData() : handlePtrCast(ptr.m_type, type)); }

		/// Get the sub type as text
	static const Char* getSubTypeText(EDx11SubType subType);

		/// If the match fails - implement casting. Writes impossible casts to Logger and returns NV_NULL.
	static Void* handlePtrCast(Int fromType, Int toType);
	static Void* handleCast(Int fromType, Int toType);
		/// Called when a cast isn't possible. Will output warnings to log and return NV_NULL
	static Void castFailure(Int fromType, Int toType);
		/// Log failure
	static Void logCastFailure(Int fromType, Int toType);
};

/* For generic handles you can use Dx11Handle. If you want the typed handle type use Dx11Type::wrap(texture) */
typedef WrapApiHandle<Dx11Type> Dx11Handle;

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_DX11_HANDLE_H
