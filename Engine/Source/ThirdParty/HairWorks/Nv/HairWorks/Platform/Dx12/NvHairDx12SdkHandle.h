/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_HAIR_DX12_SDK_HANDLE_H
#define NV_HAIR_DX12_SDK_HANDLE_H

#include <Nv/Common/Render/Dx12/NvCoDx12Handle.h>
/** \addtogroup HairWorks
@{
*/

namespace nvidia {
namespace HairWorks {

class Dx12DescriptorFlag { Dx12DescriptorFlag(); public: enum Enum {
	CBV = 0x01,
	SRV = 0x02,
	UAV = 0x04,
	SAMPLER = 0x08,
}; };
typedef Dx12DescriptorFlag::Enum EDx12DescriptorFlag;

struct Dx12InitInfo
{
	/// Initialize

	Void init() { m_targetInfo.init(); }
	NvCo::Dx12TargetInfo m_targetInfo;
};

/* Describes the pixel shader used to render with on Dx12. 
If m_hasConstantBuffer is true, then the first constant buffer register will be used for a dynamic constant buffer passed through b0. 
The constant buffer views (cbvs) are continuous and will be after the dynamic . 
The life time of the srvs, and cbvs must be managed correctly by the application */
struct Dx12PixelShaderInfo
{
	Dx12PixelShaderInfo():
		m_pixelBlob(NV_NULL),
		m_pixelBlobSize(0),
		m_numCbvs(0),
		m_numSrvs(0),
		m_hasDynamicConstantBuffer(true)
	{
		m_targetInfo.init();
	}

		/// True if it appears initialized
	NV_FORCE_INLINE Bool isInitialized() const { return m_pixelBlob != NV_NULL; }

	const UInt8* m_pixelBlob;				///< A constant buffer managed by HairWorks
	SizeT m_pixelBlobSize;					///< The size of the constant buffer

	Bool m_hasDynamicConstantBuffer;		///< If true has a HairWorks managed dynamic constant buffer
	Int m_numCbvs;							///< Number of constant buffer views  
	Int m_numSrvs;							///< Number or shader resource views 

	NvCo::Dx12TargetInfo m_targetInfo;
};

/*! Structure for specifying data needed for rendering on DirectX12. 
Note that with D3D12_CPU_DESCRIPTOR_HANDLEs passed in to m_srvs, m_cbvs that it 
is the responsibility of the caller to inform the API through 
m_descriptorContentsChangedFlags to say if the contents of any of the views has 
changed. 
The array and the descriptors can go out of scope after the call using this 
structure, because the descriptors will be copied or cached in the call. 
Finally note that if a descriptor is not going to be used, so it is not 
important what the contents is pass in 0 (ie D3D12_CPU_DESCRIPTOR_HANDLE()) and
nothing will be copied. The caveat is that the descriptor contents will be 
undefined, so should not be used in the shader.  */
struct Dx12RenderInfo
{
	Void init()
	{
		m_descriptorContentsChangedFlags = 0;
		m_constantBufferSize = 0;
		m_constantBufferData = NV_NULL;
		m_srvs = NV_NULL; 
		m_cbvs = NV_NULL; 
	}

	const Void* m_constantBufferData;
	SizeT m_constantBufferSize;

	Int m_descriptorContentsChangedFlags;			///< Some combination of Dx12DescriptorBit:: enums. Set if the CONTENTS of the descriptor has change

	const D3D12_CPU_DESCRIPTOR_HANDLE* m_srvs;		///< List of srvs (Shader Resource Views) (size is defined in PixelShaderInfo).  		
	const D3D12_CPU_DESCRIPTOR_HANDLE* m_cbvs;		///< List of cbvs (Constant buffer Views) (size is definde in m_numCbvs in PixelShaderInfo). 
};

struct Dx12MsaaInfo
{
	Void init()
	{
		m_dsvBuffer = NV_NULL;
		m_dsvCpuHandle = D3D12_CPU_DESCRIPTOR_HANDLE();
		m_rtvCpuHandle = D3D12_CPU_DESCRIPTOR_HANDLE();
	}

	ID3D12Resource *m_dsvBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE m_dsvCpuHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvCpuHandle;
};

class Dx12SdkSubType { Dx12SdkSubType(); public:
	/*! SubTypes for HairWorks Sdk Dx12 */
	enum Enum
	{
		UNKNOWN,	///< Unknown
		INIT_INFO,
		PIXEL_SHADER_INFO,
		RENDER_INFO,
		MSAA_INFO,
		COUNT_OF,
	};
};

// Handle types 

// The 'value' types - ie ones that can be passed as pointers, in addition to the handle types
#define NV_HAIR_DX12_SDK_VALUE_TYPES(x) \
	x(Dx12InitInfo, INIT_INFO, UNKNOWN) \
	x(Dx12PixelShaderInfo, PIXEL_SHADER_INFO, UNKNOWN) \
	x(Dx12RenderInfo, RENDER_INFO, UNKNOWN) \
	x(Dx12MsaaInfo, MSAA_INFO, UNKNOWN)

struct Dx12SdkType
{
	// Used by the macros. NOTE! Should be wrapping type (ie not the actual type - typically with E prefix)
	typedef Dx12SdkSubType ScopeSubType;

		/// Get the full type for the subtype
	NV_FORCE_INLINE static Int getType(ScopeSubType::Enum subType) { return (Int(NvCo::ApiType::HAIR_WORKS_DX12) << 8) | Int(subType); }
		/// Get the type via template, needed for arrays
	template <typename T>
	NV_FORCE_INLINE static Int getType() { return getType((T*)NV_NULL); }

		/// Implement getType for 'value types' (ie structs and others that shouldn't be in a handle)
	NV_HAIR_DX12_SDK_VALUE_TYPES(NV_CO_API_GET_VALUE_TYPE)

		/// A template to work around warnings from dereferencing NV_NULL
	template <typename T>
	NV_FORCE_INLINE static Int getPtrType() { Void* data = NV_NULL; return getType(*(const T*)&data); }

		/// Get a pointer
	template <typename T>
	NV_FORCE_INLINE static NvCo::ConstApiPtr wrapPtr(const T* in) { return NvCo::ConstApiPtr(getPtrType<T>(), in); }
	template <typename T>
	NV_FORCE_INLINE static NvCo::ApiPtr wrapPtr(T* in) { return NvCo::ApiPtr(getPtrType<T>(), in); }

		/// Get from 
	template <typename T>
	NV_FORCE_INLINE static const T* cast(const NvCo::ConstApiPtr& ptr) { const Int type = getPtrType<T>(); return reinterpret_cast<const T*>((ptr.m_type == type) ? ptr.getData() : NV_NULL); }
		// Get from 
	template <typename T>
	NV_FORCE_INLINE static T* cast(const NvCo::ApiPtr& ptr) { const Int type = getPtrType<T>(); return reinterpret_cast<T*>((ptr.m_type == type) ? ptr.getData() : NV_NULL); }
};

/* For generic handles you can use Dx12Handle. If you want the typed handle type use Dx12Type::wrap(texture) */
typedef NvCo::WrapApiHandle<NvCo::Dx12Type> Dx12Handle;

} // namespace HairWorks
} // namespace nvidia

/** @} */

#endif // NV_HAIR_DX12_TYPES_H
