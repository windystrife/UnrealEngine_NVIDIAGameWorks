/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_API_HANDLE_H
#define NV_CO_API_HANDLE_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! \brief API types that can be used with ApiHandle abstraction */
class ApiType { ApiType(); public: enum Enum
{
	UNKNOWN,			///< Unknown 

	DX11,				///< Dx11 
	DX12,				///< Dx12
	VULCAN,				///< Vulcan
	METAL,				///< Metal
	OPEN_GL,			///< Generic OpenGl

	HAIR_WORKS_DX12 = 8,	///< HairWorks Dx12 specific  

	COUNT_OF,
}; };
typedef ApiType::Enum EApiType;

/*!
\brief API agnostic types, can be use to identify more cleanly the actual type with a TypedApiHandle<...> */
class ApiSubType { ApiSubType(); public: enum Enum
{
	UNKNOWN,

	DEVICE,
	CONTEXT,
	TEXTURE,
	BUFFER,

	COUNT_OF,
}; };
typedef ApiSubType::Enum EApiSubType;

/*! 
\brief A very simple structure to provide type safety passing around types for different APIs - specifically rendering APIs currently.
The idea is to make handles homogeneous, or in 'global' categories, and to be able to check the explicit native type is correct on the receiving side.
The system also allows using TypedApiHandle to do have runtime type checking for 'generic' API sub types (ie are int ApiSubType enum). 
Some type safety is lost at compile time, depending on how you use this - but you have 100% safety at execution time. So you can 
think of this as a kind of API Variant of sorts. 
It is implicit in the type the interpretation of m_handle. For some types it will actually hold the handle. */
class ApiHandle
{
	NV_CO_DECLARE_CLASS_BASE(ApiHandle);

		/// Default Ctor
	NV_FORCE_INLINE ApiHandle():m_type(0), m_handle(NV_NULL) {}
		/// Ctor with type/handle
	NV_FORCE_INLINE ApiHandle(Int type, Void* handle): m_type(type), m_handle(handle) {}

		/// True if set to something 
	NV_FORCE_INLINE Bool isSet() const { return m_handle != NV_NULL; }
		/// It doesn't contain anything
	NV_FORCE_INLINE Bool isNull() const { return m_handle == NV_NULL; }

		/// Return true if is the specified type
	NV_FORCE_INLINE Bool isType(EApiType apiType, Int subType) const { return m_type == getType(apiType, subType); }

		/// Given an api and a type specific for an api return an type
	NV_FORCE_INLINE static Int getType(EApiType apiType, Int subType) { return (Int(apiType) << 8) | subType; }
		/// Get the api type from a type
	NV_FORCE_INLINE static EApiType getApiType(Int type) { return EApiType((type >> 8) & 0xff); }
		/// Get the subtype
	NV_FORCE_INLINE static Int getSubType(Int type) { return type & 0xff;}
	
		/// Null means NV_NULL for any type
	NV_FORCE_INLINE static const ApiHandle& getNull() 
	{
		static const ApiHandle s_null(0, NV_NULL);
		return s_null; 
	}

		/// Get the api type as text
	static const Char* getApiText(EApiType apiType);
	
		/// True if it's a generic failure (wrong api, different apis etc)
	static Bool isGenericCastFailure(Int fromType, Int toType, EApiType apiType);
		/// Log that there is a cast failure
	static Void logCastFailure(Int fromType, Int toType, EApiType apiType);
		/// Log that a subtype cast is not possible
	static Void logSubTypeCastFailure(const Char* fromSubText, const Char* toSubText, EApiType apiType);
	
	Int m_type;			///< The type of this handle
	Void* m_handle;		///< NOTE! Depending on the type this could be the 'thing' or pointer to the thing (if can't fit in pointer). Should be NV_NULL if null. 
};

/*! 
\brief Used for making an API handle more type safe, as it can be restricted by the generic ApiSubTypes. If
that doesn't describe the type suitably, it it can accept a ApiHandle, which will take anything. */
template <EApiSubType SUB_TYPE>
class TypedApiHandle : public ApiHandle
{
	public:
	typedef ApiHandle Parent;
		/// Default ctor
	NV_FORCE_INLINE TypedApiHandle(): Parent() {}
		/// Construct with type
	NV_FORCE_INLINE TypedApiHandle(Int type, Void* handle) : Parent(type, handle) {}
		/// Get null for a specific type
	NV_FORCE_INLINE static const TypedApiHandle& getNull() { return static_cast<const TypedApiHandle&>(ApiHandle::getNull()); }
};

typedef TypedApiHandle<ApiSubType::TEXTURE> ApiTexture;
typedef TypedApiHandle<ApiSubType::DEVICE> ApiDevice;
typedef TypedApiHandle<ApiSubType::CONTEXT> ApiContext;
typedef TypedApiHandle<ApiSubType::BUFFER> ApiBuffer;

/*! 
\brief Similar to the ApiHandle - allows a pointer to one or several things. */
class ConstApiPtr
{
	NV_CO_DECLARE_CLASS_BASE(ConstApiPtr);

		/// It's pointing to something
	NV_FORCE_INLINE Bool isSet() const { return m_data != NV_NULL; }
		/// It's null
	NV_FORCE_INLINE Bool isNull() const { return m_data == NV_NULL; }

		/// Const get the data
	NV_FORCE_INLINE const Void* getData() const { return m_data; }

		/// Default Ctor
	NV_FORCE_INLINE ConstApiPtr() :m_type(0), m_data(NV_NULL) {}
		/// Ctor with contents
	NV_FORCE_INLINE ConstApiPtr(Int type, const Void* data) :m_type(type), m_data(const_cast<Void*>(data)) {}

		/// Given an api and a type specific for an api return an type
	NV_FORCE_INLINE static Int getType(EApiType api, Int subType) { return (Int(api) << 8) | subType; }

		/// Get null
	NV_FORCE_INLINE static const ConstApiPtr& getNull() 
	{ 
		static const ConstApiPtr s_null(0, NV_NULL);
		return s_null; 
	}

	Int m_type;		///< The type of the items pointed to by 
protected:
	Void* m_data;	///< An array of elements of the type. NOTE! This is treated as const, but is this way such DeviceHandleArray can derive from this
};

/*! 
\brief For access to an array that is read/write (ie non const) */
struct ApiPtr: public ConstApiPtr
{
	NV_CO_DECLARE_CLASS(ApiPtr, ConstApiPtr);

		/// Get even with const returns non const ptr
	NV_FORCE_INLINE Void* getData() const { return m_data; }
	
		/// Default Ctor
	NV_FORCE_INLINE ApiPtr() :Parent() {}
		/// Ctor with contents
	NV_FORCE_INLINE ApiPtr(Int type, Void* data) : Parent(type, data)  {}

		/// Get null
	NV_FORCE_INLINE static const ApiPtr& getNull() { return static_cast<const ApiPtr&>(ConstApiPtr::getNull()); }
};

/*! \brief Templates for generating 'wrapped' types so you can use Dx11Handle(blah) etc instead of using Dx11Type::wrap(blah). */
template <typename REFLECT_TYPE>
class WrapApiHandle : public ApiHandle
{
	NV_CO_DECLARE_CLASS(WrapApiHandle, ApiHandle);
		/// Ctor
	template <typename T>
	WrapApiHandle(const T& in) :Parent(REFLECT_TYPE::getHandle(in)) {}
};

/* Macros to help generate the a specific implementation. Needs SubType to be defined in the implementing type, and the 
static method 'getType' that generates the type from the SubType */

#define NV_CO_API_GET_TYPE(nativeType, nativeSubType, apiSubType) \
	NV_FORCE_INLINE static Int getType(const nativeType*) { return getType(ScopeSubType::nativeSubType); }

#define NV_CO_API_WRAP(nativeType, nativeSubType, apiSubType) \
	NV_FORCE_INLINE static TypedApiHandle<ApiSubType::apiSubType> wrap(const nativeType* device) { return TypedApiHandle<ApiSubType::apiSubType>(getType(ScopeSubType::nativeSubType), const_cast<nativeType*>(device)); }

#define NV_CO_API_GET_VALUE_TYPE(nativeType, nativeSubType, apiSubType) \
	NV_FORCE_INLINE static Int getType(const nativeType&) { return getType(ScopeSubType::nativeSubType); }

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_API_HANDLE_H
