// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RootSignature.h: D3D12 Root Signatures
=============================================================================*/

// Root parameter keys grouped by visibility.
enum ERootParameterKeys
{
	PS_SRVs,
	PS_CBVs,
	PS_RootCBVs,
	PS_Samplers,
	VS_SRVs,
	VS_CBVs,
	VS_RootCBVs,
	VS_Samplers,
	GS_SRVs,
	GS_CBVs,
	GS_RootCBVs,
	GS_Samplers,
	HS_SRVs,
	HS_CBVs,
	HS_RootCBVs,
	HS_Samplers,
	DS_SRVs,
	DS_CBVs,
	DS_RootCBVs,
	DS_Samplers,
	ALL_SRVs,
	ALL_CBVs,
	ALL_RootCBVs,
	ALL_Samplers,
	ALL_UAVs,
	RPK_RootParameterKeyCount,
};

class FD3D12RootSignatureDesc
{
public:
	explicit FD3D12RootSignatureDesc(const FD3D12QuantizedBoundShaderState& QBSS, const D3D12_RESOURCE_BINDING_TIER ResourceBindingTier);

	inline const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& GetDesc() const { return RootDesc; }

	static D3D12_VERSIONED_ROOT_SIGNATURE_DESC& GetStaticGraphicsRootSignatureDesc();
	static D3D12_VERSIONED_ROOT_SIGNATURE_DESC& GetStaticComputeRootSignatureDesc();

private:
	static const uint32 MaxRootParameters = 32;	// Arbitrary max, increase as needed.
	uint32 RootParametersSize;	// The size of all root parameters in the root signature. Size in DWORDs, the limit is 64.
	CD3DX12_ROOT_PARAMETER1 TableSlots[MaxRootParameters];
	CD3DX12_DESCRIPTOR_RANGE1 DescriptorRanges[MaxRootParameters];
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootDesc;
};

class FD3D12RootSignature : public FD3D12AdapterChild
{
private:
	// Struct for all the useful info we want per shader stage.
	struct ShaderStage
	{
		ShaderStage()
			: MaxCBVCount(0u)
			, MaxSRVCount(0u)
			, MaxSamplerCount(0u)
			, MaxUAVCount(0u)
			, CBVRegisterMask(0u)
			, bVisible(false)
		{
		}

		// TODO: Make these arrays and index into them by type instead of individual variables.
		uint8 MaxCBVCount;
		uint8 MaxSRVCount;
		uint8 MaxSamplerCount;
		uint8 MaxUAVCount;
		CBVSlotMask CBVRegisterMask;
		bool bVisible;
	};

public:
	explicit FD3D12RootSignature(FD3D12Adapter* InParent)
		: FD3D12AdapterChild(InParent)
	{}
	explicit FD3D12RootSignature(FD3D12Adapter* InParent, const FD3D12QuantizedBoundShaderState& InQBSS)
		: FD3D12AdapterChild(InParent)
	{
		Init(InQBSS);
	}
	explicit FD3D12RootSignature(FD3D12Adapter* InParent, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& InDesc)
		: FD3D12AdapterChild(InParent)
	{
		Init(InDesc);
	}
	explicit FD3D12RootSignature(FD3D12Adapter* InParent, ID3DBlob* const InBlob)
		: FD3D12AdapterChild(InParent)
	{
		Init(InBlob);
	}

	void Init(const FD3D12QuantizedBoundShaderState& InQBSS);
	void Init(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& InDesc);
	void Init(ID3DBlob* const InBlob);

	ID3D12RootSignature* GetRootSignature() const { return RootSignature.GetReference(); }
	ID3DBlob* GetRootSignatureBlob() const { return RootSignatureBlob.GetReference(); }

	inline uint32 SamplerRDTBindSlot(EShaderFrequency ShaderStage) const
	{
		switch (ShaderStage)
		{
		case SF_Vertex: return BindSlotMap[VS_Samplers];
		case SF_Pixel: return BindSlotMap[PS_Samplers];
		case SF_Geometry: return BindSlotMap[GS_Samplers];
		case SF_Hull: return BindSlotMap[HS_Samplers];
		case SF_Domain: return BindSlotMap[DS_Samplers];
		case SF_Compute: return BindSlotMap[ALL_Samplers];

		default: check(false);
			return UINT_MAX;
		}
	}

	inline uint32 SRVRDTBindSlot(EShaderFrequency ShaderStage) const
	{
		switch (ShaderStage)
		{
		case SF_Vertex: return BindSlotMap[VS_SRVs];
		case SF_Pixel: return BindSlotMap[PS_SRVs];
		case SF_Geometry: return BindSlotMap[GS_SRVs];
		case SF_Hull: return BindSlotMap[HS_SRVs];
		case SF_Domain: return BindSlotMap[DS_SRVs];
		case SF_Compute: return BindSlotMap[ALL_SRVs];

		default: check(false);
			return UINT_MAX;
		}
	}

	inline uint32 CBVRDTBindSlot(EShaderFrequency ShaderStage) const
	{
		switch (ShaderStage)
		{
		case SF_Vertex: return BindSlotMap[VS_CBVs];
		case SF_Pixel: return BindSlotMap[PS_CBVs];
		case SF_Geometry: return BindSlotMap[GS_CBVs];
		case SF_Hull: return BindSlotMap[HS_CBVs];
		case SF_Domain: return BindSlotMap[DS_CBVs];
		case SF_Compute: return BindSlotMap[ALL_CBVs];

		default: check(false);
			return UINT_MAX;
		}
	}

	inline uint32 CBVRDBaseBindSlot(EShaderFrequency ShaderStage) const
	{
		switch (ShaderStage)
		{
		case SF_Vertex: return BindSlotMap[VS_RootCBVs];
		case SF_Pixel: return BindSlotMap[PS_RootCBVs];
		case SF_Geometry: return BindSlotMap[GS_RootCBVs];
		case SF_Hull: return BindSlotMap[HS_RootCBVs];
		case SF_Domain: return BindSlotMap[DS_RootCBVs];

		case SF_NumFrequencies:
		case SF_Compute: return BindSlotMap[ALL_RootCBVs];

		default: check(false);
			return UINT_MAX;
		}
	}

	inline uint32 CBVRDBindSlot(EShaderFrequency ShaderStage, uint32 BufferIndex) const
	{
		// This code assumes that all Root CBVs for a particular stage are contiguous in the root signature (thus indexable by the buffer index).
		return CBVRDBaseBindSlot(ShaderStage) + BufferIndex;
	}

	inline uint32 UAVRDTBindSlot(EShaderFrequency ShaderStage) const
	{
		check(ShaderStage == SF_Pixel || ShaderStage == SF_Compute);
		return BindSlotMap[ALL_UAVs];
	}

	inline bool HasUAVs() const { return bHasUAVs; }
	inline bool HasSRVs() const { return bHasSRVs; }
	inline bool HasCBVs() const { return bHasCBVs; }
	inline bool HasSamplers() const { return bHasSamplers; }
	inline bool HasVS() const { return Stage[SF_Vertex].bVisible; }
	inline bool HasHS() const { return Stage[SF_Hull].bVisible; }
	inline bool HasDS() const { return Stage[SF_Domain].bVisible; }
	inline bool HasGS() const { return Stage[SF_Geometry].bVisible; }
	inline bool HasPS() const { return Stage[SF_Pixel].bVisible; }
	inline bool HasCS() const { return Stage[SF_Compute].bVisible; }	// Root signatures can be used for Graphics and/or Compute because they exist in separate bind spaces.
	inline uint32 MaxSamplerCount(uint32 ShaderStage) const { check(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxSamplerCount; }
	inline uint32 MaxSRVCount(uint32 ShaderStage) const { check(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxSRVCount; }
	inline uint32 MaxCBVCount(uint32 ShaderStage) const { check(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxCBVCount; }
	inline uint32 MaxUAVCount(uint32 ShaderStage) const { check(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxUAVCount; }
	inline CBVSlotMask CBVRegisterMask(uint32 ShaderStage) const { check(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].CBVRegisterMask; }

private:
	void AnalyzeSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& Desc);

	template<typename RootSignatureDescType>
	void InternalAnalyzeSignature(const RootSignatureDescType& Desc);

	inline bool HasVisibility(const D3D12_SHADER_VISIBILITY& ParameterVisibility, const D3D12_SHADER_VISIBILITY& Visibility) const
	{
		return ParameterVisibility == D3D12_SHADER_VISIBILITY_ALL || ParameterVisibility == Visibility;
	}

	inline void SetSamplersRDTBindSlot(EShaderFrequency SF, uint8 RootParameterIndex)
	{
		uint8* pBindSlot = nullptr;
		switch (SF)
		{
		case SF_Vertex: pBindSlot = &BindSlotMap[VS_Samplers]; break;
		case SF_Pixel: pBindSlot = &BindSlotMap[PS_Samplers]; break;
		case SF_Geometry: pBindSlot = &BindSlotMap[GS_Samplers]; break;
		case SF_Hull: pBindSlot = &BindSlotMap[HS_Samplers]; break;
		case SF_Domain: pBindSlot = &BindSlotMap[DS_Samplers]; break;

		case SF_Compute:
		case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_Samplers]; break;

		default: check(false);
			return;
		}

		check(*pBindSlot == 0xFF);
		*pBindSlot = RootParameterIndex;

		bHasSamplers = true;
	}

	inline void SetSRVRDTBindSlot(EShaderFrequency SF, uint8 RootParameterIndex)
	{
		uint8* pBindSlot = nullptr;
		switch (SF)
		{
		case SF_Vertex: pBindSlot = &BindSlotMap[VS_SRVs]; break;
		case SF_Pixel: pBindSlot = &BindSlotMap[PS_SRVs]; break;
		case SF_Geometry: pBindSlot = &BindSlotMap[GS_SRVs]; break;
		case SF_Hull: pBindSlot = &BindSlotMap[HS_SRVs]; break;
		case SF_Domain: pBindSlot = &BindSlotMap[DS_SRVs]; break;

		case SF_Compute:
		case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_SRVs]; break;

		default: check(false);
			return;
		}

		check(*pBindSlot == 0xFF);
		*pBindSlot = RootParameterIndex;

		bHasSRVs = true;
	}

	inline void SetCBVRDTBindSlot(EShaderFrequency SF, uint8 RootParameterIndex)
	{
		uint8* pBindSlot = nullptr;
		switch (SF)
		{
		case SF_Vertex: pBindSlot = &BindSlotMap[VS_CBVs]; break;
		case SF_Pixel: pBindSlot = &BindSlotMap[PS_CBVs]; break;
		case SF_Geometry: pBindSlot = &BindSlotMap[GS_CBVs]; break;
		case SF_Hull: pBindSlot = &BindSlotMap[HS_CBVs]; break;
		case SF_Domain: pBindSlot = &BindSlotMap[DS_CBVs]; break;

		case SF_Compute:
		case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_CBVs]; break;

		default: check(false);
			return;
		}

		check(*pBindSlot == 0xFF);
		*pBindSlot = RootParameterIndex;

		bHasCBVs = true;
		bHasRDTCBVs = true;
	}

	inline void SetCBVRDBindSlot(EShaderFrequency SF, uint8 RootParameterIndex)
	{
		uint8* pBindSlot = nullptr;
		switch (SF)
		{
		case SF_Vertex: pBindSlot = &BindSlotMap[VS_RootCBVs]; break;
		case SF_Pixel: pBindSlot = &BindSlotMap[PS_RootCBVs]; break;
		case SF_Geometry: pBindSlot = &BindSlotMap[GS_RootCBVs]; break;
		case SF_Hull: pBindSlot = &BindSlotMap[HS_RootCBVs]; break;
		case SF_Domain: pBindSlot = &BindSlotMap[DS_RootCBVs]; break;

		case SF_Compute:
		case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_RootCBVs]; break;

		default: check(false);
			return;
		}

		check(*pBindSlot == 0xFF);
		*pBindSlot = RootParameterIndex;

		bHasCBVs = true;
		bHasRDCBVs = true;
	}

	inline void SetUAVRDTBindSlot(EShaderFrequency SF, uint8 RootParameterIndex)
	{
		check(SF == SF_Pixel || SF == SF_Compute || SF == SF_NumFrequencies);
		uint8* pBindSlot = &BindSlotMap[ALL_UAVs];

		check(*pBindSlot == 0xFF);
		*pBindSlot = RootParameterIndex;

		bHasUAVs = true;
	}

	inline void SetMaxSamplerCount(EShaderFrequency SF, uint8 Count)
	{
		if (SF == SF_NumFrequencies)
		{
			// Update all counts for all stages.
			for (uint32 s = SF_Vertex; s <= SF_Compute; s++)
			{
				Stage[s].MaxSamplerCount = Count;
			}
		}
		else
		{
			Stage[SF].MaxSamplerCount = Count;
		}
	}

	inline void SetMaxSRVCount(EShaderFrequency SF, uint8 Count)
	{
		if (SF == SF_NumFrequencies)
		{
			// Update all counts for all stages.
			for (uint32 s = SF_Vertex; s <= SF_Compute; s++)
			{
				Stage[s].MaxSRVCount = Count;
			}
		}
		else
		{
			Stage[SF].MaxSRVCount = Count;
		}
	}

	// Update the mask that indicates what shader registers are used in the descriptor table.
	template<typename DescriptorRangeType>
	inline void UpdateCBVRegisterMaskWithDescriptorRange(EShaderFrequency SF, const DescriptorRangeType& Range)
	{
		const uint32 StartRegister = Range.BaseShaderRegister;
		const uint32 EndRegister = StartRegister + Range.NumDescriptors;
		const uint32 StartStage = (SF == SF_NumFrequencies) ? SF_Vertex : SF;
		const uint32 EndStage = (SF == SF_NumFrequencies) ? SF_Compute : SF;
		for (uint32 CurrentStage = StartStage; CurrentStage <= EndStage; CurrentStage++)
		{
			for (uint32 Register = StartRegister; Register < EndRegister; Register++)
			{
				// The bit shouldn't already be set for the current register.
				check((Stage[CurrentStage].CBVRegisterMask & (1 << Register)) == 0); 
				Stage[CurrentStage].CBVRegisterMask |= (1 << Register);
			}
		}
	}

	// Update the mask that indicates what shader registers are used in the root descriptor.
	template<typename DescriptorType>
	inline void UpdateCBVRegisterMaskWithDescriptor(EShaderFrequency SF, const DescriptorType& Descriptor)
	{
		const uint32 StartStage = (SF == SF_NumFrequencies) ? SF_Vertex : SF;
		const uint32 EndStage = (SF == SF_NumFrequencies) ? SF_Compute : SF;
		const uint32& Register = Descriptor.ShaderRegister;
		for (uint32 CurrentStage = StartStage; CurrentStage <= EndStage; CurrentStage++)
		{
			// The bit shouldn't already be set for the current register.
			check((Stage[CurrentStage].CBVRegisterMask & (1 << Register)) == 0); 
			Stage[CurrentStage].CBVRegisterMask |= (1 << Register);
		}
	}

	inline void SetMaxCBVCount(EShaderFrequency SF, uint8 Count)
	{
		if (SF == SF_NumFrequencies)
		{
			// Update all counts for all stages.
			for (uint32 s = SF_Vertex; s <= SF_Compute; s++)
			{
				Stage[s].MaxCBVCount = Count;
			}
		}
		else
		{
			Stage[SF].MaxCBVCount = Count;
		}
	}

	inline void IncrementMaxCBVCount(EShaderFrequency SF, uint8 Count)
	{
		if (SF == SF_NumFrequencies)
		{
			// Update all counts for all stages.
			for (uint32 s = SF_Vertex; s <= SF_Compute; s++)
			{
				Stage[s].MaxCBVCount += Count;
			}
		}
		else
		{
			Stage[SF].MaxCBVCount += Count;
		}
	}

	inline void SetMaxUAVCount(EShaderFrequency SF, uint8 Count)
	{
		if (SF == SF_NumFrequencies)
		{
			// Update all counts for all stages.
			for (uint32 s = SF_Vertex; s <= SF_Compute; s++)
			{
				Stage[s].MaxUAVCount = Count;
			}
		}
		else
		{
			Stage[SF].MaxUAVCount = Count;
		}
	}

	TRefCountPtr<ID3D12RootSignature> RootSignature;
	uint8 BindSlotMap[RPK_RootParameterKeyCount];	// This map uses an enum as a key to lookup the root parameter index
	ShaderStage Stage[SF_NumFrequencies];
	bool bHasUAVs;
	bool bHasSRVs;
	bool bHasCBVs;
	bool bHasRDTCBVs;
	bool bHasRDCBVs;
	bool bHasSamplers;
	TRefCountPtr<ID3DBlob> RootSignatureBlob;
};

class FD3D12RootSignatureManager : public FD3D12AdapterChild
{
public:
	explicit FD3D12RootSignatureManager(FD3D12Adapter* InParent)
		: FD3D12AdapterChild(InParent)
	{
	}
	~FD3D12RootSignatureManager();
	FD3D12RootSignature* GetRootSignature(const FD3D12QuantizedBoundShaderState& QBSS);
	FD3D12QuantizedBoundShaderState GetQuantizedBoundShaderState(const FD3D12RootSignature* const RootSignature);

private:
	FCriticalSection CS;
	FD3D12RootSignature* CreateRootSignature(const FD3D12QuantizedBoundShaderState& QBSS);

	TMap<FD3D12QuantizedBoundShaderState, FD3D12RootSignature*> RootSignatureMap;
};