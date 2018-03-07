#ifndef NV_CO_DX12_RESOURCE_H
#define NV_CO_DX12_RESOURCE_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoSubString.h>

#include <Nv/Common/NvCoComPtr.h>

#include <Nv/Common/Render/Dx/NvCoDxFormatUtil.h>

//#include <dxgi.h>
#include <d3d12.h>

namespace nvidia { 
namespace Common {

// Enables more conservative barriers - restoring the state of resources after they are used.
// Should not need to be enabled in normal builds, as the barriers should correctly sync resources
// If enabling fixes an issue it implies regular barriers are not correctly used. 
#define NV_CO_ENABLE_CONSERVATIVE_RESOURCE_BARRIERS 0

struct Dx12BarrierSubmitter
{
	enum { MAX_BARRIERS = 8 };

		/// Expand one space to hold a barrier
	NV_FORCE_INLINE D3D12_RESOURCE_BARRIER& expandOne() { return (m_numBarriers < MAX_BARRIERS) ? m_barriers[m_numBarriers++] : _expandOne(); }
		/// Flush barriers to command list 
	NV_FORCE_INLINE Void flush() { if (m_numBarriers > 0) _flush(); }

		/// Ctor
	NV_FORCE_INLINE Dx12BarrierSubmitter(ID3D12GraphicsCommandList* commandList):m_numBarriers(0), m_commandList(commandList) { }
		/// Dtor
	NV_FORCE_INLINE ~Dx12BarrierSubmitter() { flush(); }

	protected:
	D3D12_RESOURCE_BARRIER& _expandOne();
	Void _flush();

	ID3D12GraphicsCommandList* m_commandList;
	IndexT m_numBarriers;
	D3D12_RESOURCE_BARRIER m_barriers[MAX_BARRIERS];
};

/** The base class for resource types allows for tracking of state. It does not allow for setting of the resource though, such that
an interface can return a Dx12ResourceBase, and a client cant manipulate it's state, but it cannot replace/change the actual resource */
struct Dx12ResourceBase
{
		/// Add a transition if necessary to the list
	Void transition(D3D12_RESOURCE_STATES nextState, Dx12BarrierSubmitter& submitter);
		/// Get the current state
	NV_FORCE_INLINE D3D12_RESOURCE_STATES getState() const { return m_state; }

		/// Get the associated resource
	NV_FORCE_INLINE ID3D12Resource* getResource() const { return m_resource; }

		/// True if a resource is set
	NV_FORCE_INLINE Bool isSet() const { return m_resource != NV_NULL; }

		/// Coercable into ID3D12Resource
	NV_FORCE_INLINE operator ID3D12Resource*() const { return m_resource; }

		/// restore previous state
#if NV_CO_ENABLE_CONSERVATIVE_RESOURCE_BARRIERS
	NV_FORCE_INLINE Void restore(Dx12BarrierSubmitter& submitter) { transition(m_prevState, submitter); } 
#else
	NV_FORCE_INLINE Void restore(Dx12BarrierSubmitter& submitter) { NV_UNUSED(submitter) }
#endif

		/// Given the usage, flags, and format will return the most suitable format. Will return DXGI_UNKNOWN if combination is not possible
	static DXGI_FORMAT calcFormat(DxFormatUtil::UsageType usage, ID3D12Resource* resource);

		/// Ctor
	NV_FORCE_INLINE Dx12ResourceBase() :
		m_state(D3D12_RESOURCE_STATE_COMMON),
		m_prevState(D3D12_RESOURCE_STATE_COMMON),
		m_resource(NV_NULL)
	{}
	
	protected:
		/// This is protected so as clients cannot slice the class, and so state tracking is lost
	~Dx12ResourceBase() {}

	ID3D12Resource* m_resource;
	D3D12_RESOURCE_STATES m_state;
	D3D12_RESOURCE_STATES m_prevState;
};

struct Dx12Resource: public Dx12ResourceBase
{

	/// Dtor
	~Dx12Resource()
	{
		if (m_resource)
		{
			m_resource->Release();
		}
	}

		/// Initialize as committed resource
	Result initCommitted(ID3D12Device* device, const D3D12_HEAP_PROPERTIES& heapProps, D3D12_HEAP_FLAGS heapFlags, const D3D12_RESOURCE_DESC& resourceDesc, D3D12_RESOURCE_STATES initState, const D3D12_CLEAR_VALUE * clearValue);

		/// Set a resource with an initial state
	void setResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState);
		/// Make the resource null
	void setResourceNull();
		/// Returns the attached resource (with any ref counts) and sets to NV_NULL on this.
	ID3D12Resource* detach();

		/// Swaps the resource contents with the contents of the smart pointer
	Void swap(ComPtr<ID3D12Resource>& resourceInOut);

		/// Sets the current state of the resource (the current state is taken to be the future state once the command list has executed)
		/// NOTE! This must be used with care, otherwise state tracking can be made incorrect.
	void setState(D3D12_RESOURCE_STATES state);

		/// Set the debug name on a resource
	static void setDebugName(ID3D12Resource* resource, const SubString& name);

		/// Set the the debug name on the resource
	Void setDebugName(const wchar_t* name);
		/// Set the debug name
	Void setDebugName(const SubString& name);
};

/// Convenient way to set blobs 
struct Dx12Blob : public D3D12_SHADER_BYTECODE
{
	template <SizeT SIZE>
	NV_FORCE_INLINE Dx12Blob(const BYTE(&in)[SIZE]) { pShaderBytecode = in; BytecodeLength = SIZE; }
	NV_FORCE_INLINE Dx12Blob(const BYTE* in, SizeT size) { pShaderBytecode = in; BytecodeLength = size; }
	NV_FORCE_INLINE Dx12Blob() { pShaderBytecode = NV_NULL; BytecodeLength = 0; }
	NV_FORCE_INLINE Dx12Blob(ID3DBlob* blob) { pShaderBytecode = blob->GetBufferPointer(); BytecodeLength = blob->GetBufferSize(); }
};

} // namespace Common 
} // namespace nvidia 

#endif // NV_CO_DX12_RESOURCE_H
