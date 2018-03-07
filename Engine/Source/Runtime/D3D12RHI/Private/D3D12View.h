// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12View.h: D3D12 Resource Views
=============================================================================*/

enum ViewSubresourceSubsetFlags
{
	ViewSubresourceSubsetFlags_None = 0x0,
	ViewSubresourceSubsetFlags_DepthOnlyDsv = 0x1,
	ViewSubresourceSubsetFlags_StencilOnlyDsv = 0x2,
	ViewSubresourceSubsetFlags_DepthAndStencilDsv = (ViewSubresourceSubsetFlags_DepthOnlyDsv | ViewSubresourceSubsetFlags_StencilOnlyDsv),
};

/** Class to track subresources in a view */
struct CBufferView {};
class CSubresourceSubset
{
public:
	CSubresourceSubset() {}
	inline explicit CSubresourceSubset(const CBufferView&) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_EndMip(1),
		m_BeginPlane(0),
		m_EndPlane(1)
	{
	}
	inline explicit CSubresourceSubset(const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc, DXGI_FORMAT ResourceFormat) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_EndMip(1),
		m_BeginPlane(0),
		m_EndPlane(1)
	{
		switch (Desc.ViewDimension)
		{
		default: ASSUME(0 && "Corrupt Resource Type on Shader Resource View"); break;

		case (D3D12_SRV_DIMENSION_BUFFER) :
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE1D) :
			m_BeginMip = uint8(Desc.Texture1D.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture1D.MipLevels);
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE1DARRAY) :
			m_BeginArray = uint16(Desc.Texture1DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture1DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture1DArray.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture1DArray.MipLevels);
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE2D) :
			m_BeginMip = uint8(Desc.Texture2D.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture2D.MipLevels);
			m_BeginPlane = uint8(Desc.Texture2D.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2D.PlaneSlice + 1);
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE2DARRAY) :
			m_BeginArray = uint16(Desc.Texture2DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture2DArray.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture2DArray.MipLevels);
			m_BeginPlane = uint8(Desc.Texture2DArray.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2DArray.PlaneSlice + 1);
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE2DMS) :
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY) :
			m_BeginArray = uint16(Desc.Texture2DMSArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DMSArray.ArraySize);
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE3D) :
			m_EndArray = uint16(-1); //all slices
			m_BeginMip = uint8(Desc.Texture3D.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture3D.MipLevels);
			break;

		case (D3D12_SRV_DIMENSION_TEXTURECUBE) :
			m_BeginMip = uint8(Desc.TextureCube.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.TextureCube.MipLevels);
			m_BeginArray = 0;
			m_EndArray = 6;
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURECUBEARRAY) :
			m_BeginArray = uint16(Desc.TextureCubeArray.First2DArrayFace);
			m_EndArray = uint16(m_BeginArray + Desc.TextureCubeArray.NumCubes * 6);
			m_BeginMip = uint8(Desc.TextureCubeArray.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.TextureCubeArray.MipLevels);
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;
		}
	}
	inline explicit CSubresourceSubset(const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_BeginPlane(0),
		m_EndPlane(1)
	{
		switch (Desc.ViewDimension)
		{
		default: ASSUME(0 && "Corrupt Resource Type on Unordered Access View"); break;

		case (D3D12_UAV_DIMENSION_BUFFER) : break;

		case (D3D12_UAV_DIMENSION_TEXTURE1D) :
			m_BeginMip = uint8(Desc.Texture1D.MipSlice);
			break;

		case (D3D12_UAV_DIMENSION_TEXTURE1DARRAY) :
			m_BeginArray = uint16(Desc.Texture1DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture1DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture1DArray.MipSlice);
			break;

		case (D3D12_UAV_DIMENSION_TEXTURE2D) :
			m_BeginMip = uint8(Desc.Texture2D.MipSlice);
			m_BeginPlane = uint8(Desc.Texture2D.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2D.PlaneSlice + 1);
			break;

		case (D3D12_UAV_DIMENSION_TEXTURE2DARRAY) :
			m_BeginArray = uint16(Desc.Texture2DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture2DArray.MipSlice);
			m_BeginPlane = uint8(Desc.Texture2DArray.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2DArray.PlaneSlice + 1);
			break;

		case (D3D12_UAV_DIMENSION_TEXTURE3D) :
			m_BeginArray = uint16(Desc.Texture3D.FirstWSlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture3D.WSize);
			m_BeginMip = uint8(Desc.Texture3D.MipSlice);
			break;
		}

		m_EndMip = m_BeginMip + 1;
	}
	inline explicit CSubresourceSubset(const D3D12_RENDER_TARGET_VIEW_DESC& Desc) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_BeginPlane(0),
		m_EndPlane(1)
	{
		switch (Desc.ViewDimension)
		{
		default: ASSUME(0 && "Corrupt Resource Type on Render Target View"); break;

		case (D3D12_RTV_DIMENSION_BUFFER) : break;

		case (D3D12_RTV_DIMENSION_TEXTURE1D) :
			m_BeginMip = uint8(Desc.Texture1D.MipSlice);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE1DARRAY) :
			m_BeginArray = uint16(Desc.Texture1DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture1DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture1DArray.MipSlice);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE2D) :
			m_BeginMip = uint8(Desc.Texture2D.MipSlice);
			m_BeginPlane = uint8(Desc.Texture2D.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2D.PlaneSlice + 1);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE2DMS) : break;

		case (D3D12_RTV_DIMENSION_TEXTURE2DARRAY) :
			m_BeginArray = uint16(Desc.Texture2DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture2DArray.MipSlice);
			m_BeginPlane = uint8(Desc.Texture2DArray.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2DArray.PlaneSlice + 1);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY) :
			m_BeginArray = uint16(Desc.Texture2DMSArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DMSArray.ArraySize);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE3D) :
			m_BeginArray = uint16(Desc.Texture3D.FirstWSlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture3D.WSize);
			m_BeginMip = uint8(Desc.Texture3D.MipSlice);
			break;
		}

		m_EndMip = m_BeginMip + 1;
	}

	inline explicit CSubresourceSubset(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags Flags) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_BeginPlane(0),
		m_EndPlane(GetPlaneCount(ResourceFormat))
	{
		switch (Desc.ViewDimension)
		{
		default: ASSUME(0 && "Corrupt Resource Type on Depth Stencil View"); break;

		case (D3D12_DSV_DIMENSION_TEXTURE1D) :
			m_BeginMip = uint8(Desc.Texture1D.MipSlice);
			break;

		case (D3D12_DSV_DIMENSION_TEXTURE1DARRAY) :
			m_BeginArray = uint16(Desc.Texture1DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture1DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture1DArray.MipSlice);
			break;

		case (D3D12_DSV_DIMENSION_TEXTURE2D) :
			m_BeginMip = uint8(Desc.Texture2D.MipSlice);
			break;

		case (D3D12_DSV_DIMENSION_TEXTURE2DMS) : break;

		case (D3D12_DSV_DIMENSION_TEXTURE2DARRAY) :
			m_BeginArray = uint16(Desc.Texture2DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture2DArray.MipSlice);
			break;

		case (D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY) :
			m_BeginArray = uint16(Desc.Texture2DMSArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DMSArray.ArraySize);
			break;
		}

		m_EndMip = m_BeginMip + 1;

		if (m_EndPlane == 2)
		{
			if ((Flags & ViewSubresourceSubsetFlags_DepthAndStencilDsv) != ViewSubresourceSubsetFlags_DepthAndStencilDsv)
			{
				if (Flags & ViewSubresourceSubsetFlags_DepthOnlyDsv)
				{
					m_BeginPlane = 0;
					m_EndPlane = 1;
				}
				else if (Flags & ViewSubresourceSubsetFlags_StencilOnlyDsv)
				{
					m_BeginPlane = 1;
					m_EndPlane = 2;
				}
			}
		}
	}

	__forceinline bool DoesNotOverlap(const CSubresourceSubset& other) const
	{
		if (m_EndArray <= other.m_BeginArray)
		{
			return true;
		}

		if (other.m_EndArray <= m_BeginArray)
		{
			return true;
		}

		if (m_EndMip <= other.m_BeginMip)
		{
			return true;
		}

		if (other.m_EndMip <= m_BeginMip)
		{
			return true;
		}

		if (m_EndPlane <= other.m_BeginPlane)
		{
			return true;
		}

		if (other.m_EndPlane <= m_BeginPlane)
		{
			return true;
		}

		return false;
	}

protected:
	uint16 m_BeginArray; // Also used to store Tex3D slices.
	uint16 m_EndArray; // End - Begin == Array Slices
	uint8 m_BeginMip;
	uint8 m_EndMip; // End - Begin == Mip Levels
	uint8 m_BeginPlane;
	uint8 m_EndPlane;
};

template <typename TDesc>
class FD3D12View;

class CViewSubresourceSubset : public CSubresourceSubset
{
	friend class FD3D12View < D3D12_SHADER_RESOURCE_VIEW_DESC >;
	friend class FD3D12View < D3D12_RENDER_TARGET_VIEW_DESC >;
	friend class FD3D12View < D3D12_DEPTH_STENCIL_VIEW_DESC >;
	friend class FD3D12View < D3D12_UNORDERED_ACCESS_VIEW_DESC >;

public:
	CViewSubresourceSubset() {}
	inline explicit CViewSubresourceSubset(const CBufferView&)
		: CSubresourceSubset(CBufferView())
		, m_MipLevels(1)
		, m_ArraySlices(1)
		, m_MostDetailedMip(0)
		, m_ViewArraySize(1)
	{
	}

	inline CViewSubresourceSubset(uint32 Subresource, uint8 MipLevels, uint16 ArraySize, uint8 PlaneCount)
		: m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(PlaneCount)
	{
		if (Subresource < uint32(MipLevels) * uint32(ArraySize))
		{
			m_BeginArray = Subresource / MipLevels;
			m_EndArray = m_BeginArray + 1;
			m_BeginMip = Subresource % MipLevels;
			m_EndMip = m_EndArray + 1;
		}
		else
		{
			m_BeginArray = 0;
			m_BeginMip = 0;
			if (Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
			{
				m_EndArray = ArraySize;
				m_EndMip = MipLevels;
			}
			else
			{
				m_EndArray = 0;
				m_EndMip = 0;
			}
		}
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
	}

	inline CViewSubresourceSubset(const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc, uint8 MipLevels, uint16 ArraySize, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags /*Flags*/)
		: CSubresourceSubset(Desc, ResourceFormat)
		, m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(GetPlaneCount(ResourceFormat))
	{
		if (Desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE3D)
		{
			check(m_BeginArray == 0);
			m_EndArray = 1;
		}
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
		Reduce();
	}

	inline CViewSubresourceSubset(const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc, uint8 MipLevels, uint16 ArraySize, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags /*Flags*/)
		: CSubresourceSubset(Desc)
		, m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(GetPlaneCount(ResourceFormat))
	{
		if (Desc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE3D)
		{
			m_BeginArray = 0;
			m_EndArray = 1;
		}
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
		Reduce();
	}

	inline CViewSubresourceSubset(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc, uint8 MipLevels, uint16 ArraySize, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags Flags)
		: CSubresourceSubset(Desc, ResourceFormat, Flags)
		, m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(GetPlaneCount(ResourceFormat))
	{
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
		Reduce();
	}

	inline CViewSubresourceSubset(const D3D12_RENDER_TARGET_VIEW_DESC& Desc, uint8 MipLevels, uint16 ArraySize, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags /*Flags*/)
		: CSubresourceSubset(Desc)
		, m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(GetPlaneCount(ResourceFormat))
	{
		if (Desc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE3D)
		{
			m_BeginArray = 0;
			m_EndArray = 1;
		}
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
		Reduce();
	}

	template<typename T>
	static CViewSubresourceSubset FromView(const T* pView)
	{
		return CViewSubresourceSubset(
			pView->Desc(),
			static_cast<uint8>(pView->GetResource()->GetMipLevels()),
			static_cast<uint16>(pView->GetResource()->GetArraySize()),
			static_cast<uint8>(pView->GetResource()->GetPlaneCount())
			);
	}

public:
	class CViewSubresourceIterator;

public:
	CViewSubresourceIterator begin() const;
	CViewSubresourceIterator end() const;
	bool IsWholeResource() const;
	uint32 ArraySize() const;

	uint8 MostDetailedMip() const;
	uint16 ViewArraySize() const;

	uint32 MinSubresource() const;
	uint32 MaxSubresource() const;

private:
	// Strictly for performance, allows coalescing contiguous subresource ranges into a single range
	inline void Reduce()
	{
		if (m_BeginMip == 0
			&& m_EndMip == m_MipLevels
			&& m_BeginArray == 0
			&& m_EndArray == m_ArraySlices
			&& m_BeginPlane == 0
			&& m_EndPlane == m_PlaneCount)
		{
			uint32 startSubresource = D3D12CalcSubresource(0, 0, m_BeginPlane, m_MipLevels, m_ArraySlices);
			uint32 endSubresource = D3D12CalcSubresource(0, 0, m_EndPlane, m_MipLevels, m_ArraySlices);

			// Only coalesce if the full-resolution UINTs fit in the UINT8s used for storage here
			if (endSubresource < static_cast<uint8>(-1))
			{
				m_BeginArray = 0;
				m_EndArray = 1;
				m_BeginPlane = 0;
				m_EndPlane = 1;
				m_BeginMip = static_cast<uint8>(startSubresource);
				m_EndMip = static_cast<uint8>(endSubresource);
			}
		}
	}

protected:
	uint8 m_MipLevels;
	uint16 m_ArraySlices;
	uint8 m_PlaneCount;
	uint8 m_MostDetailedMip;
	uint16 m_ViewArraySize;
};

// This iterator iterates over contiguous ranges of subresources within a subresource subset. eg:
//
// // For each contiguous subresource range.
// for( CViewSubresourceIterator it = ViewSubset.begin(); it != ViewSubset.end(); ++it )
// {
//      // StartSubresource and EndSubresource members of the iterator describe the contiguous range.
//      for( uint32 SubresourceIndex = it.StartSubresource(); SubresourceIndex < it.EndSubresource(); SubresourceIndex++ )
//      {
//          // Action for each subresource within the current range.
//      }
//  }
//
class CViewSubresourceSubset::CViewSubresourceIterator
{
public:
	inline CViewSubresourceIterator(CViewSubresourceSubset const& SubresourceSet, uint16 ArraySlice, uint8 PlaneSlice)
		: m_Subresources(SubresourceSet)
		, m_CurrentArraySlice(ArraySlice)
		, m_CurrentPlaneSlice(PlaneSlice)
	{
	}

	inline CViewSubresourceSubset::CViewSubresourceIterator& operator++()
	{
		check(m_CurrentArraySlice < m_Subresources.m_EndArray);

		if (++m_CurrentArraySlice >= m_Subresources.m_EndArray)
		{
			check(m_CurrentPlaneSlice < m_Subresources.m_EndPlane);
			m_CurrentArraySlice = m_Subresources.m_BeginArray;
			++m_CurrentPlaneSlice;
		}

		return *this;
	}

	inline CViewSubresourceSubset::CViewSubresourceIterator& operator--()
	{
		if (m_CurrentArraySlice <= m_Subresources.m_BeginArray)
		{
			m_CurrentArraySlice = m_Subresources.m_EndArray;

			check(m_CurrentPlaneSlice > m_Subresources.m_BeginPlane);
			--m_CurrentPlaneSlice;
		}

		--m_CurrentArraySlice;

		return *this;
	}

	inline bool operator==(CViewSubresourceIterator const& other) const
	{
		return &other.m_Subresources == &m_Subresources
			&& other.m_CurrentArraySlice == m_CurrentArraySlice
			&& other.m_CurrentPlaneSlice == m_CurrentPlaneSlice;
	}

	inline bool operator!=(CViewSubresourceIterator const& other) const
	{
		return !(other == *this);
	}

	inline uint32 StartSubresource() const
	{
		return D3D12CalcSubresource(m_Subresources.m_BeginMip, m_CurrentArraySlice, m_CurrentPlaneSlice, m_Subresources.m_MipLevels, m_Subresources.m_ArraySlices);
	}

	inline uint32 EndSubresource() const
	{
		return D3D12CalcSubresource(m_Subresources.m_EndMip, m_CurrentArraySlice, m_CurrentPlaneSlice, m_Subresources.m_MipLevels, m_Subresources.m_ArraySlices);
	}

	inline TPair<uint32, uint32> operator*() const
	{
		TPair<uint32, uint32> NewPair;
		NewPair.Key = StartSubresource();
		NewPair.Value = EndSubresource();
		return NewPair;
	}

private:
	CViewSubresourceSubset const& m_Subresources;
	uint16 m_CurrentArraySlice;
	uint8 m_CurrentPlaneSlice;
};

inline CViewSubresourceSubset::CViewSubresourceIterator CViewSubresourceSubset::begin() const
{
	return CViewSubresourceIterator(*this, m_BeginArray, m_BeginPlane);
}

inline CViewSubresourceSubset::CViewSubresourceIterator CViewSubresourceSubset::end() const
{
	return CViewSubresourceIterator(*this, m_BeginArray, m_EndPlane);
}

inline bool CViewSubresourceSubset::IsWholeResource() const
{
	return m_BeginMip == 0 && m_BeginArray == 0 && m_BeginPlane == 0 && (m_EndMip * m_EndArray * m_EndPlane == m_MipLevels * m_ArraySlices * m_PlaneCount);
}

inline uint32 CViewSubresourceSubset::ArraySize() const
{
	return m_ArraySlices;
}

inline uint8 CViewSubresourceSubset::MostDetailedMip() const
{
	return m_MostDetailedMip;
}

inline uint16 CViewSubresourceSubset::ViewArraySize() const
{
	return m_ViewArraySize;
}

inline uint32 CViewSubresourceSubset::MinSubresource() const
{
	return (*begin()).Key;
}

inline uint32 CViewSubresourceSubset::MaxSubresource() const
{
	return (*(--end())).Value;
}

// The view is either based on a resource location or a resource; not both
class FD3D12ViewGeneric
{
protected:
	CD3DX12_CPU_DESCRIPTOR_HANDLE Descriptor;
	uint32 DescriptorHeapIndex;
	FD3D12ResourceLocation* ResourceLocation;
	FD3D12ResidencyHandle* ResidencyHandle;

	explicit FD3D12ViewGeneric(FD3D12ResourceLocation* InResourceLocation)
		: ResourceLocation(InResourceLocation)
		, ResidencyHandle((InResourceLocation) ? InResourceLocation->GetResource()->GetResidencyHandle() : nullptr)
	{
		Descriptor.ptr = 0;
		DescriptorHeapIndex = 0;
	}

	void ResetFromResourceLocation(FD3D12ResourceLocation* InResourceLocation)
	{
		ResourceLocation = InResourceLocation;
		Descriptor.ptr = 0;
		DescriptorHeapIndex = 0;

		if (InResourceLocation)
		{
			ResidencyHandle = InResourceLocation->GetResource()->GetResidencyHandle();
		}
	}

public:
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetView() const { check(Descriptor.ptr != 0); return Descriptor; }
	uint32 GetDescriptorHeapIndex() const { return DescriptorHeapIndex; }
	FD3D12ResourceLocation* GetResourceLocation() const { return ResourceLocation; }
	FD3D12Resource* GetResource() const
	{
		if (ResourceLocation)
		{
			return ResourceLocation->GetResource();
		}

		return nullptr;
	}

	FD3D12ResidencyHandle* GetResidencyHandle() { return ResidencyHandle; }

	friend class FD3D12VertexBuffer;
};

template <typename TDesc> struct TCreateViewMap;
template<> struct TCreateViewMap < D3D12_SHADER_RESOURCE_VIEW_DESC > { static decltype(&ID3D12Device::CreateShaderResourceView) GetCreate() { return &ID3D12Device::CreateShaderResourceView; } };
template<> struct TCreateViewMap < D3D12_RENDER_TARGET_VIEW_DESC > { static decltype(&ID3D12Device::CreateRenderTargetView) GetCreate() { return &ID3D12Device::CreateRenderTargetView; } };
template<> struct TCreateViewMap < D3D12_DEPTH_STENCIL_VIEW_DESC > { static decltype(&ID3D12Device::CreateDepthStencilView) GetCreate() { return &ID3D12Device::CreateDepthStencilView; } };
template<> struct TCreateViewMap < D3D12_UNORDERED_ACCESS_VIEW_DESC > { static decltype(&ID3D12Device::CreateUnorderedAccessView) GetCreate() { return &ID3D12Device::CreateUnorderedAccessView; } };

template <typename TDesc>
class FD3D12View : public FD3D12ViewGeneric, public FD3D12DeviceChild
{
protected:
	CViewSubresourceSubset ViewSubresourceSubset;
	TDesc Desc;

protected:
	explicit FD3D12View()
		: FD3D12ViewGeneric(nullptr)
		, FD3D12DeviceChild(nullptr)
	{
	}

	explicit FD3D12View(FD3D12Device* InParent, const TDesc* InDesc, FD3D12ResourceLocation* InResourceLocation, ViewSubresourceSubsetFlags InFlags)
		: FD3D12ViewGeneric(InResourceLocation)
		, ViewSubresourceSubset(*InDesc,
			GetResource() ? GetResource()->GetMipLevels() : 0,
			GetResource() ? GetResource()->GetArraySize() : 0,
			GetResource() ? GetResource()->GetDesc().Format : DXGI_FORMAT_UNKNOWN,
			InFlags)
		, FD3D12DeviceChild(InParent)
	{
		Init(InDesc);
	}

	virtual ~FD3D12View()
	{
		FreeHeapSlot();
	}

	void Init(const TDesc* InDesc)
	{
		if (InDesc)
		{
			Desc = *InDesc;
		}
		else
		{
			FMemory::Memzero(&Desc, sizeof(Desc));
		}
		AllocateHeapSlot();
	}

	void AllocateHeapSlot()
	{
		FD3D12OfflineDescriptorManager& DescriptorAllocator = GetParentDevice()->template GetViewDescriptorAllocator<TDesc>();
		Descriptor = DescriptorAllocator.AllocateHeapSlot(DescriptorHeapIndex);
		check(Descriptor.ptr != 0);
	}

	void FreeHeapSlot()
	{
		if (Descriptor.ptr)
		{
			FD3D12OfflineDescriptorManager& DescriptorAllocator = GetParentDevice()->template GetViewDescriptorAllocator<TDesc>();
			DescriptorAllocator.FreeHeapSlot(Descriptor, DescriptorHeapIndex);
			Descriptor.ptr = 0;
		}
	}

	void UpdateViewSubresourceSubset(const FD3D12Resource* const InResource)
	{
		if (InResource == GetResource())
		{
			// Nothing to update
			return;
		}

		if (InResource)
		{
			const D3D12_RESOURCE_DESC &ResDesc = InResource->GetDesc();
			ViewSubresourceSubset.m_MipLevels = ResDesc.MipLevels;
			ViewSubresourceSubset.m_ArraySlices = ResDesc.DepthOrArraySize;
			ViewSubresourceSubset.m_PlaneCount = GetPlaneCount(ResDesc.Format);
		}
		else
		{
			// Null resource
			ViewSubresourceSubset.m_MipLevels = 0;
			ViewSubresourceSubset.m_ArraySlices = 0;
			ViewSubresourceSubset.m_PlaneCount = 0;
		}
	}


public:
	void CreateView(FD3D12Resource* InResource = nullptr, FD3D12Resource* InCounterResource = nullptr)
	{
		if (!InResource)
		{
			InResource = GetResource();
		}
		else
		{
			// Only need to update the view's subresource subset if a new resource is used
			UpdateViewSubresourceSubset(InResource);
		}

		check(Descriptor.ptr != 0);
		(GetParentDevice()->GetDevice()->*TCreateViewMap<TDesc>::GetCreate()) (
			InResource ? InResource->GetResource() : nullptr, &Desc, Descriptor);
	}

	void CreateViewWithCounter(FD3D12Resource* InResource, FD3D12Resource* InCounterResource)
	{
		if (!InResource)
		{
			InResource = GetResource();
		}
		else
		{
			// Only need to update the view's subresource subset if a new resource is used
			UpdateViewSubresourceSubset(InResource);
		}

		check(Descriptor.ptr != 0);
		(GetParentDevice()->GetDevice()->*TCreateViewMap<TDesc>::GetCreate()) (
			InResource ? InResource->GetResource() : nullptr,
			InCounterResource ? InCounterResource->GetResource() : nullptr, &Desc, Descriptor);
	}

	const TDesc& GetDesc() const { return Desc; }
	const CViewSubresourceSubset& GetViewSubresourceSubset() const { return ViewSubresourceSubset; }

	template< class T >
	inline bool DoesNotOverlap(const FD3D12View< T >& Other) const
	{
		return ViewSubresourceSubset.DoesNotOverlap(Other.GetViewSubresourceSubset());
	}
};

/** Shader resource view class. */
class FD3D12ShaderResourceView : public FRHIShaderResourceView, public FD3D12View < D3D12_SHADER_RESOURCE_VIEW_DESC >, public FD3D12LinkedAdapterObject<FD3D12ShaderResourceView>
{
	const bool bIsBuffer;
	const bool bContainsDepthPlane;
	const bool bContainsStencilPlane;
	uint32 Stride;

public:
	FD3D12ShaderResourceView(FD3D12Device* InParent, D3D12_SHADER_RESOURCE_VIEW_DESC* InSRVDesc, FD3D12ResourceLocation* InResourceLocation, uint32 InStride = 1)
		: FD3D12View(InParent, InSRVDesc, InResourceLocation, ViewSubresourceSubsetFlags_None)
		, bIsBuffer(InSRVDesc->ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
		, bContainsDepthPlane(InResourceLocation ? InResourceLocation->GetResource()->IsDepthStencilResource() && GetPlaneSliceFromViewFormat(InResourceLocation->GetResource()->GetDesc().Format, InSRVDesc->Format) == 0 : false)
		, bContainsStencilPlane(InResourceLocation ? InResourceLocation->GetResource()->IsDepthStencilResource() && GetPlaneSliceFromViewFormat(InResourceLocation->GetResource()->GetDesc().Format, InSRVDesc->Format) == 1 : false)
		, Stride(InStride)
	{
		// Check the plane slice of the SRV matches the texture format
		// Texture2DMS does not have explicit plane index (it's implied by the format)
		if (InResourceLocation && InSRVDesc->ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
		{
			check(GetPlaneSliceFromViewFormat(InResourceLocation->GetResource()->GetDesc().Format, InSRVDesc->Format) == InSRVDesc->Texture2D.PlaneSlice);
		}
		
		if (bIsBuffer && !!InResourceLocation)
		{
			check(InResourceLocation->GetOffsetFromBaseOfResource() / Stride == InSRVDesc->Buffer.FirstElement);
		}

		CreateView();
	}

	static FD3D12ShaderResourceView* CreateShaderResourceView(FD3D12Device* Parent, FD3D12ResourceLocation* ResourceLocation, D3D12_SHADER_RESOURCE_VIEW_DESC& Desc);

	void Rename(FD3D12ResourceLocation& InResourceLocation, CD3DX12_CPU_DESCRIPTOR_HANDLE InDescriptor, uint32 InDescriptorHeapIndex)
	{
		ResourceLocation = &InResourceLocation;
		if (bIsBuffer)
		{
			Desc.Buffer.FirstElement = InResourceLocation.GetOffsetFromBaseOfResource() / Stride;
		}

		// If no descriptor and heap index were provided...
		if (InDescriptor.ptr == 0)
		{
			// Create a new view
			CreateView();
		}
		else
		{
			// Otherwise use the provided descriptor and index
			Descriptor.ptr = InDescriptor.ptr;
			DescriptorHeapIndex = InDescriptorHeapIndex;
		}

		check(ResourceLocation);
		ResidencyHandle = ResourceLocation->GetResource()->GetResidencyHandle();
	}

	void Rename(float ResourceMinLODClamp)
	{
		check(ResourceLocation);

		// Update the LODClamp
		check(Desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D);
		Desc.Texture2D.ResourceMinLODClamp = ResourceMinLODClamp;

		// Create a new view
		CreateView();
	}

	FORCEINLINE bool IsDepthStencilResource() const { return bContainsDepthPlane || bContainsStencilPlane; }
	FORCEINLINE bool IsDepthPlaneResource() const { return bContainsDepthPlane; }
	FORCEINLINE bool IsStencilPlaneResource() const { return bContainsStencilPlane; }
	FORCEINLINE const D3D12_SHADER_RESOURCE_VIEW_DESC& GetDesc()
	{
		return Desc;
	}
};

class FD3D12UnorderedAccessView : public FRHIUnorderedAccessView, public FD3D12View < D3D12_UNORDERED_ACCESS_VIEW_DESC >, public FD3D12LinkedAdapterObject<FD3D12UnorderedAccessView>
{
public:

	TRefCountPtr<FD3D12Resource> CounterResource;
	bool CounterResourceInitialized;

	FD3D12UnorderedAccessView(FD3D12Device* InParent, D3D12_UNORDERED_ACCESS_VIEW_DESC* InUAVDesc, FD3D12ResourceLocation* InResourceLocation, TRefCountPtr<FD3D12Resource> InCounterResource = nullptr)
		: FD3D12View(InParent, InUAVDesc, InResourceLocation, ViewSubresourceSubsetFlags_None)
		, CounterResource(InCounterResource)
		, CounterResourceInitialized(false)
	{
		CreateViewWithCounter(nullptr, CounterResource);
	}
};

#if USE_STATIC_ROOT_SIGNATURE
class FD3D12ConstantBufferView : /*public FD3D12ViewGeneric, */public FD3D12DeviceChild
{
			public:
//protected:
	/** The handle to the descriptor in the offline descriptor heap */
	CD3DX12_CPU_DESCRIPTOR_HANDLE OfflineDescriptorHandle;

	/** Index of the descriptor in the offline heap */
	uint32 OfflineHeapIndex;

	D3D12_CONSTANT_BUFFER_VIEW_DESC Desc;

//protected:

	explicit FD3D12ConstantBufferView(FD3D12Device* InParent, const D3D12_CONSTANT_BUFFER_VIEW_DESC* InDesc)
		: FD3D12DeviceChild(InParent)
		, OfflineHeapIndex(UINT_MAX)
	{
		OfflineDescriptorHandle.ptr = 0;
		Init(InDesc);
	}

	virtual ~FD3D12ConstantBufferView()
	{
		FreeHeapSlot();
	}

	void Init(const D3D12_CONSTANT_BUFFER_VIEW_DESC* InDesc)
	{
		if (InDesc)
		{
			Desc = *InDesc;
		}
		else
		{
			FMemory::Memzero(&Desc, sizeof(Desc));
		}
		AllocateHeapSlot();
	}

	void AllocateHeapSlot();

	void FreeHeapSlot();

	void Create(D3D12_GPU_VIRTUAL_ADDRESS GPUAddress, const uint32 AlignedSize);

public:
/*
	void CreateView(FD3D12Resource* InResource = nullptr, FD3D12Resource* InCounterResource = nullptr)
	{
		if (!InResource)
		{
			InResource = GetResource();
		}
		else
		{
			// Only need to update the view's subresource subset if a new resource is used
			UpdateViewSubresourceSubset(InResource);
		}

		check(Descriptor.ptr != 0);
		(GetParentDevice()->GetDevice()->*TCreateViewMap<TDesc>::GetCreate()) (
			InResource ? InResource->GetResource() : nullptr, &Desc, Descriptor);
	}

	void CreateViewWithCounter(FD3D12Resource* InResource, FD3D12Resource* InCounterResource)
	{
		if (!InResource)
		{
			InResource = GetResource();
		}
		else
		{
			// Only need to update the view's subresource subset if a new resource is used
			UpdateViewSubresourceSubset(InResource);
		}

		check(Descriptor.ptr != 0);
		(GetParentDevice()->GetDevice()->*TCreateViewMap<TDesc>::GetCreate()) (
			InResource ? InResource->GetResource() : nullptr,
			InCounterResource ? InCounterResource->GetResource() : nullptr, &Desc, Descriptor);
	}*/

	const D3D12_CONSTANT_BUFFER_VIEW_DESC& GetDesc() const { return Desc; }
};
#endif

class FD3D12RenderTargetView : public FD3D12View<D3D12_RENDER_TARGET_VIEW_DESC>, public FRHIResource, public FD3D12LinkedAdapterObject<FD3D12RenderTargetView>
{
public:
	FD3D12RenderTargetView(FD3D12Device* InParent, D3D12_RENDER_TARGET_VIEW_DESC* InRTVDesc, FD3D12ResourceLocation* ResourceLocation)
		: FD3D12View(InParent, InRTVDesc, ResourceLocation, ViewSubresourceSubsetFlags_None)
	{
		CreateView();
	}

	static FD3D12RenderTargetView* CreateRenderTargetView(FD3D12Device* Parent, FD3D12ResourceLocation* ResourceLocation, D3D12_RENDER_TARGET_VIEW_DESC& Desc);
};

class FD3D12DepthStencilView : public FD3D12View<D3D12_DEPTH_STENCIL_VIEW_DESC>, public FRHIResource, public FD3D12LinkedAdapterObject<FD3D12DepthStencilView>
{
	const bool bHasDepth;
	const bool bHasStencil;
	CViewSubresourceSubset DepthOnlyViewSubresourceSubset;
	CViewSubresourceSubset StencilOnlyViewSubresourceSubset;

public:
	FD3D12DepthStencilView(FD3D12Device* InParent, D3D12_DEPTH_STENCIL_VIEW_DESC* InDSVDesc, FD3D12ResourceLocation* InResourceLocation, bool InHasStencil)
		: FD3D12View(InParent, InDSVDesc, InResourceLocation, ViewSubresourceSubsetFlags_DepthAndStencilDsv)
		, bHasDepth(true)				// Assume all DSVs have depth bits in their format
		, bHasStencil(InHasStencil)		// Only some DSVs have stencil bits in their format
		, DepthOnlyViewSubresourceSubset(*InDSVDesc, 0, 0, DXGI_FORMAT_UNKNOWN, ViewSubresourceSubsetFlags_DepthOnlyDsv)
		, StencilOnlyViewSubresourceSubset(*InDSVDesc, 0, 0, DXGI_FORMAT_UNKNOWN, ViewSubresourceSubsetFlags_StencilOnlyDsv)
	{
		CreateView();

		FD3D12Resource* Resource = (InResourceLocation) ? InResourceLocation->GetResource() : nullptr;

		// Create individual subresource subsets for each plane
		if (bHasDepth)
		{
			DepthOnlyViewSubresourceSubset = CViewSubresourceSubset(*InDSVDesc,
				Resource ? Resource->GetMipLevels() : 0,
				Resource ? Resource->GetArraySize() : 0,
				Resource ? Resource->GetDesc().Format : DXGI_FORMAT_UNKNOWN,
				ViewSubresourceSubsetFlags_DepthOnlyDsv);
		}

		if (bHasStencil)
		{
			StencilOnlyViewSubresourceSubset = CViewSubresourceSubset(*InDSVDesc,
				Resource ? Resource->GetMipLevels() : 0,
				Resource ? Resource->GetArraySize() : 0,
				Resource ? Resource->GetDesc().Format : DXGI_FORMAT_UNKNOWN,
				ViewSubresourceSubsetFlags_StencilOnlyDsv);
		}
	}

	bool HasDepth() const
	{
		return bHasDepth;
	}

	bool HasStencil() const
	{
		return bHasStencil;
	}

	CViewSubresourceSubset& GetDepthOnlyViewSubresourceSubset()
	{
		check(bHasDepth);
		return DepthOnlyViewSubresourceSubset;
	}

	CViewSubresourceSubset& GetStencilOnlyViewSubresourceSubset()
	{
		check(bHasStencil);
		return StencilOnlyViewSubresourceSubset;
	}

	static FD3D12DepthStencilView* CreateDepthStencilView(FD3D12Device* Parent, FD3D12ResourceLocation* ResourceLocation, D3D12_DEPTH_STENCIL_VIEW_DESC& Desc, bool HasStencil);
};

template<>
struct TD3D12ResourceTraits<FRHIShaderResourceView>
{
	typedef FD3D12ShaderResourceView TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIUnorderedAccessView>
{
	typedef FD3D12UnorderedAccessView TConcreteType;
};
