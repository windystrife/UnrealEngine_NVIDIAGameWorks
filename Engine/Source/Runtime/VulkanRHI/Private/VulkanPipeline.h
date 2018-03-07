// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPipeline.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanResources.h"
#include "VulkanShaderResources.h"
#include "VulkanDescriptorSets.h"

class FVulkanDevice;

// Common pipeline class
class FVulkanPipeline
{
public:
	FVulkanPipeline(FVulkanDevice* InDevice);
	virtual ~FVulkanPipeline();

	inline VkPipeline GetHandle() const
	{
		return Pipeline;
	}

	inline const FVulkanLayout& GetLayout() const
	{
		check(Layout);
		return *Layout;
	}

protected:
	FVulkanDevice* Device;
	VkPipeline Pipeline;
	FVulkanLayout* Layout; /*owned by FVulkanPipelineStateCache, do not delete yourself !*/

	friend class FVulkanPipelineStateCache;
	friend class FVulkanGraphicsPipelineState;
};

class FVulkanComputePipeline : public FVulkanPipeline, public FRHIComputePipelineState
{
public:
	FVulkanComputePipeline(FVulkanDevice* InDevice);
	virtual ~FVulkanComputePipeline();

	inline const FVulkanCodeHeader& GetShaderCodeHeader() const
	{
		return ComputeShader->GetCodeHeader();
	}

	inline const FVulkanComputeShader* GetShader() const
	{
		return ComputeShader;
	}

	inline void Bind(VkCommandBuffer CmdBuffer)
	{
		VulkanRHI::vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
	}

protected:
	FVulkanComputeShader* ComputeShader;

	friend class FVulkanPipelineStateCache;
};

class FVulkanGfxPipeline : public FVulkanPipeline, public FRHIResource
{
public:
	FVulkanGfxPipeline(FVulkanDevice* InDevice);
	
	inline void Bind(VkCommandBuffer CmdBuffer)
	{
		VulkanRHI::vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);
	}

	inline bool IsRuntimeInitialized() const
	{
		return bRuntimeObjectsValid;
	}

	inline const FVulkanVertexInputStateInfo& GetVertexInputState() const
	{
		check(bRuntimeObjectsValid);
		return VertexInputState;
	}

	void CreateRuntimeObjects(const FGraphicsPipelineStateInitializer& InPSOInitializer);

private:
	bool bRuntimeObjectsValid;
	FVulkanVertexInputStateInfo VertexInputState;
};

class FVulkanGraphicsPipelineState : public FRHIGraphicsPipelineState
{
public:
	FVulkanGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer, FVulkanGfxPipeline* InPipeline)
		: PipelineStateInitializer(Initializer), Pipeline(InPipeline)
	{}

	~FVulkanGraphicsPipelineState();

	FGraphicsPipelineStateInitializer PipelineStateInitializer;
	TRefCountPtr<FVulkanGfxPipeline> Pipeline;
};

class FVulkanPipelineStateCache
{
public:
	inline FVulkanGraphicsPipelineState* FindInRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32& OutHash)
	{
		OutHash = FCrc::MemCrc32(&Initializer, sizeof(Initializer));
		
		{
			FScopeLock Lock(&InitializerToPipelineMapCS);
			FVulkanGraphicsPipelineState** Found = InitializerToPipelineMap.Find(OutHash);
			if (Found)
			{
				return *Found;
			}
		}

		return nullptr;
	}

	void DestroyPipeline(FVulkanGfxPipeline* Pipeline);

	// Array of potential cache locations; first entries have highest priority. Only one cache file is loaded. If unsuccessful, tries next entry in the array.
	void InitAndLoad(const TArray<FString>& CacheFilenames);
	void Save(FString& CacheFilename);

	FVulkanPipelineStateCache(FVulkanDevice* InParent);
	~FVulkanPipelineStateCache();

	void RebuildCache();

	enum
	{
		// Bump every time serialization changes
		VERSION = 16,
	};

	struct FDescriptorSetLayoutBinding
	{
		uint32 Binding;
		//uint16 DescriptorCount;
		uint8 DescriptorType;
		uint8 StageFlags;

		void ReadFrom(const VkDescriptorSetLayoutBinding& InState);
		void WriteInto(VkDescriptorSetLayoutBinding& OutState) const;

		bool operator==(const FDescriptorSetLayoutBinding& In) const
		{
			return  Binding == In.Binding &&
				//DescriptorCount == In.DescriptorCount &&
				DescriptorType == In.DescriptorType &&
				StageFlags == In.StageFlags;
		}
	};

	// Shader microcode is shared between pipeline entries so keep a cache around to prevent duplicated storage
	struct FShaderUCodeCache
	{
		using TCodeHandle = TArray<uint8>;
		using TDataMap = TMap<FSHAHash, TCodeHandle>;
		TDataMap Data;

		TCodeHandle* Add(const FSHAHash& Hash, const FVulkanShader* Shader)
		{
			check(Shader->CodeSize != 0);

			TCodeHandle& Code = Data.Add(Hash);
			Code.AddUninitialized(Shader->CodeSize);
			FMemory::Memcpy(Code.GetData(), Shader->Code, Shader->CodeSize);

			return &Data[Hash];
		}

		TCodeHandle* Get(const FSHAHash& Hash)
		{
			return Data.Find(Hash);
		}
	};

	// Actual information required to recreate a pipeline when saving/loading from disk
	struct FGfxPipelineEntry
	{
		uint32 GetEntryHash(uint32 Crc = 0);
		uint32 VertexInputKey;
		bool bLoaded;

		uint32 RasterizationSamples;
		uint32 Topology;
		struct FBlendAttachment
		{
			bool bBlend;
			uint8 ColorBlendOp;
			uint8 SrcColorBlendFactor;
			uint8 DstColorBlendFactor;
			uint8 AlphaBlendOp;
			uint8 SrcAlphaBlendFactor;
			uint8 DstAlphaBlendFactor;
			uint8 ColorWriteMask;

			void ReadFrom(const VkPipelineColorBlendAttachmentState& InState);
			void WriteInto(VkPipelineColorBlendAttachmentState& OutState) const;

			bool operator==(const FBlendAttachment& In) const
			{
				return bBlend == In.bBlend &&
					ColorBlendOp == In.ColorBlendOp &&
					SrcColorBlendFactor == In.SrcColorBlendFactor &&
					DstColorBlendFactor == In.DstColorBlendFactor &&
					AlphaBlendOp == In.AlphaBlendOp &&
					SrcAlphaBlendFactor == In.SrcAlphaBlendFactor &&
					DstAlphaBlendFactor == In.DstAlphaBlendFactor &&
					ColorWriteMask == In.ColorWriteMask;
			}
		};
		TArray<FBlendAttachment> ColorAttachmentStates;

		TArray<TArray<FDescriptorSetLayoutBinding>> DescriptorSetLayoutBindings;

		struct FVertexBinding
		{
			uint32 Stride;
			uint16 Binding;
			uint16 InputRate;

			void ReadFrom(const VkVertexInputBindingDescription& InState);
			void WriteInto(VkVertexInputBindingDescription& OutState) const;

			bool operator==(const FVertexBinding& In) const
			{
				return Stride == In.Stride &&
					Binding == In.Binding &&
					InputRate == In.InputRate;
			}
		};
		TArray<FVertexBinding> VertexBindings;
		struct FVertexAttribute
		{
			uint32 Location;
			uint32 Binding;
			uint32 Format;
			uint32 Offset;

			void ReadFrom(const VkVertexInputAttributeDescription& InState);
			void WriteInto(VkVertexInputAttributeDescription& OutState) const;

			bool operator==(const FVertexAttribute& In) const
			{
				return Location == In.Location &&
					Binding == In.Binding &&
					Format == In.Format &&
					Offset == In.Offset;
			}
		};
		TArray<FVertexAttribute> VertexAttributes;

		struct FRasterizer
		{
			uint8 PolygonMode;
			uint8 CullMode;
			float DepthBiasSlopeScale;
			float DepthBiasConstantFactor;

			void ReadFrom(const VkPipelineRasterizationStateCreateInfo& InState);
			void WriteInto(VkPipelineRasterizationStateCreateInfo& OutState) const;

			bool operator==(const FRasterizer& In) const
			{
				return PolygonMode == In.PolygonMode &&
					CullMode == In.CullMode &&
					DepthBiasSlopeScale == In.DepthBiasSlopeScale &&
					DepthBiasConstantFactor == In.DepthBiasConstantFactor;
			}
		};
		FRasterizer Rasterizer;

		struct FDepthStencil
		{
			uint8 DepthCompareOp;
			bool bDepthTestEnable;
			bool bDepthWriteEnable;
			bool bStencilTestEnable;
			uint8 FrontFailOp;
			uint8 FrontPassOp;
			uint8 FrontDepthFailOp;
			uint8 FrontCompareOp;
			uint32 FrontCompareMask;
			uint32 FrontWriteMask;
			uint32 FrontReference;
			uint8 BackFailOp;
			uint8 BackPassOp;
			uint8 BackDepthFailOp;
			uint8 BackCompareOp;
			uint32 BackCompareMask;
			uint32 BackWriteMask;
			uint32 BackReference;

			void ReadFrom(const VkPipelineDepthStencilStateCreateInfo& InState);
			void WriteInto(VkPipelineDepthStencilStateCreateInfo& OutState) const;

			bool operator==(const FDepthStencil& In) const
			{
				return DepthCompareOp == In.DepthCompareOp &&
					bDepthTestEnable == In.bDepthTestEnable &&
					bDepthWriteEnable == In.bDepthWriteEnable &&
					bStencilTestEnable == In.bStencilTestEnable &&
					FrontFailOp == In.FrontFailOp &&
					FrontPassOp == In.FrontPassOp &&
					FrontDepthFailOp == In.FrontDepthFailOp &&
					FrontCompareOp == In.FrontCompareOp &&
					FrontCompareMask == In.FrontCompareMask &&
					FrontWriteMask == In.FrontWriteMask &&
					FrontReference == In.FrontReference &&
					BackFailOp == In.BackFailOp &&
					BackPassOp == In.BackPassOp &&
					BackDepthFailOp == In.BackDepthFailOp &&
					BackCompareOp == In.BackCompareOp &&
					BackCompareMask == In.BackCompareMask &&
					BackWriteMask == In.BackWriteMask &&
					BackReference == In.BackReference;
			}
		};
		FDepthStencil DepthStencil;

		FShaderUCodeCache::TCodeHandle* ShaderMicrocodes[SF_Compute];
		FSHAHash ShaderHashes[SF_Compute];

		struct FRenderTargets
		{
			struct FAttachmentRef
			{
				uint32 Attachment;
				uint64 Layout;

				void ReadFrom(const VkAttachmentReference& InState);
				void WriteInto(VkAttachmentReference& OutState) const;
				bool operator == (const FAttachmentRef& In) const
				{
					return Attachment == In.Attachment && Layout == In.Layout;
				}
			};
			TArray<FAttachmentRef> ColorAttachments;
			TArray<FAttachmentRef> ResolveAttachments;
			FAttachmentRef DepthStencil;

			struct FAttachmentDesc
			{
				uint32 Format;
				uint8 Flags;
				uint8 Samples;
				uint8 LoadOp;
				uint8 StoreOp;
				uint8 StencilLoadOp;
				uint8 StencilStoreOp;
				uint64 InitialLayout;
				uint64 FinalLayout;

				bool operator==(const FAttachmentDesc& In) const
				{
					return Format == In.Format &&
						Flags == In.Flags &&
						Samples == In.Samples &&
						LoadOp == In.LoadOp &&
						StoreOp == In.StoreOp &&
						StencilLoadOp == In.StencilLoadOp &&
						StencilStoreOp == In.StencilStoreOp &&
						InitialLayout == In.InitialLayout &&
						FinalLayout == In.FinalLayout;
				}

				void ReadFrom(const VkAttachmentDescription &InState);
				void WriteInto(VkAttachmentDescription& OutState) const;
			};
			TArray<FAttachmentDesc> Descriptions;

			uint8 NumAttachments;
			uint8 NumColorAttachments;
			uint8 bHasDepthStencil;
			uint8 bHasResolveAttachments;
			uint8 NumUsedClearValues;
			uint32 OldHash;
			uint32 RenderPassHash;
			FVector Extent3D;

			void ReadFrom(const FVulkanRenderTargetLayout &InState);
			void WriteInto(FVulkanRenderTargetLayout& OutState) const;

			bool operator==(const FRenderTargets& In) const
			{
				return ColorAttachments == In.ColorAttachments &&
					ResolveAttachments == In.ResolveAttachments &&
					DepthStencil == In.DepthStencil &&
					Descriptions == In.Descriptions &&
					NumAttachments == In.NumAttachments &&
					NumColorAttachments == In.NumColorAttachments &&
					bHasDepthStencil == In.bHasDepthStencil &&
					bHasResolveAttachments == In.bHasResolveAttachments &&
					NumUsedClearValues == In.NumUsedClearValues &&
					OldHash == In.OldHash &&
					RenderPassHash == In.RenderPassHash &&
					Extent3D == In.Extent3D;
			}
		};
		FRenderTargets RenderTargets;

		FGfxPipelineEntry()
			: VertexInputKey(0)
			, bLoaded(false)
			, RasterizationSamples(0)
			, Topology(0)
			, RenderPass(nullptr)
			, Layout(nullptr)
		{
			FMemory::Memzero(Rasterizer);
			FMemory::Memzero(DepthStencil);
			FMemory::Memzero(ShaderModules);
			FMemory::Memzero(ShaderMicrocodes);
		}

		~FGfxPipelineEntry();

		// Vulkan Runtime Data/Objects
		VkShaderModule ShaderModules[SF_Compute];
		const FVulkanRenderPass* RenderPass;
		FVulkanLayout* Layout;

		bool operator==(const FGfxPipelineEntry& In) const
		{
			if (VertexInputKey != In.VertexInputKey)
			{
				return false;
			}

			if( bLoaded != In.bLoaded)
			{
				return false;
			}

			if (RasterizationSamples != In.RasterizationSamples)
			{
				return false;
			}

			if (Topology != In.Topology)
			{
				return false;
			}

			if (ColorAttachmentStates != In.ColorAttachmentStates)
			{
				return false;
			}

			if (DescriptorSetLayoutBindings != In.DescriptorSetLayoutBindings)
			{
				return false;
			}

			if (!(Rasterizer == In.Rasterizer))
			{
				return false;
			}

			if (!(DepthStencil == In.DepthStencil))
			{
				return false;
			}

			for (int32 Index = 0; Index < ARRAY_COUNT(In.ShaderHashes); ++Index)
			{
				if (ShaderHashes[Index] != In.ShaderHashes[Index])
				{
					return false;
				}
			}

			for (int32 Index = 0; Index < ARRAY_COUNT(In.ShaderMicrocodes); ++Index)
			{
				if (ShaderMicrocodes[Index] != In.ShaderMicrocodes[Index])
				{
					return false;
				}
			}

			if (!(RenderTargets == In.RenderTargets))
			{
				return false;
			}

			if (VertexBindings != In.VertexBindings)
			{
				return false;
			}

			if (VertexAttributes != In.VertexAttributes)
			{
				return false;
			}

			return true;
		}
	};

	struct FComputePipelineEntry
	{
		uint32 EntryHash;
		void CalculateEntryHash();

		bool bLoaded;

		FShaderUCodeCache::TCodeHandle* ShaderMicrocode;
		FSHAHash ShaderHash;
		TArray<TArray<FDescriptorSetLayoutBinding>> DescriptorSetLayoutBindings;

		// Runtime objects
		VkShaderModule ShaderModule;
		FVulkanLayout* Layout;

		FComputePipelineEntry()
			: EntryHash(0)
			, bLoaded(false)
			, ShaderMicrocode(nullptr)
			, ShaderModule(VK_NULL_HANDLE)
			, Layout(nullptr)
		{}

		~FComputePipelineEntry();
	};
	FVulkanComputePipeline* GetOrCreateComputePipeline(FVulkanComputeShader* ComputeShader);

private:
	FVulkanDevice* Device;

	// Key is a hash of the PSO, which is based off shader pointers
	TMap<uint32, FVulkanGraphicsPipelineState*> InitializerToPipelineMap;
	FCriticalSection InitializerToPipelineMapCS;

	TMap<FVulkanComputeShader*, FVulkanComputePipeline*> ComputeShaderToPipelineMap;
	TMap<uint32, FVulkanComputePipeline*> ComputeEntryHashToPipelineMap;

	TMap<uint32, FGfxPipelineEntry*> GfxPipelineEntries;
	TMap<uint32, FComputePipelineEntry*> ComputePipelineEntries;

	VkPipelineCache PipelineCache;

	FShaderUCodeCache ShaderCache;

	FVulkanGraphicsPipelineState* CreateAndAdd(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOInitializerHash, FGfxPipelineEntry* GfxEntry);
	void CreateGfxPipelineFromEntry(const FGfxPipelineEntry* GfxEntry, FVulkanGfxPipeline* Pipeline);
	FGfxPipelineEntry* CreateGfxEntry(const FGraphicsPipelineStateInitializer& PSOInitializer);
	void CreatGfxEntryRuntimeObjects(FGfxPipelineEntry* GfxEntry);
	void Load(const TArray<FString>& CacheFilenames);
	void DestroyCache();

	struct FShaderHashes
	{
		uint32 Hash;
		FSHAHash Stages[SF_Compute];

		FShaderHashes();
		FShaderHashes(const FGraphicsPipelineStateInitializer& PSOInitializer);

		friend inline uint32 GetTypeHash(const FShaderHashes& Hashes)
		{
			return Hashes.Hash;
		}

		inline void Finalize()
		{
			Hash = FCrc::MemCrc32(Stages, sizeof(Stages));
		}


		friend inline bool operator == (const FShaderHashes& A, const FShaderHashes& B)
		{
			for (int32 Index = 0; Index < SF_Compute; ++Index)
			{
				if (A.Stages[Index] != B.Stages[Index])
				{
					return false;
				}
			}

			return true;
		}
	};

	typedef TMap<uint32, FVulkanGfxPipeline*> FHashToGfxPipelinesMap;
	TMap<FShaderHashes, FHashToGfxPipelinesMap> ShaderHashToGfxPipelineMap;
	FCriticalSection ShaderHashToGfxEntriesMapCS;

	TMap<FVulkanDescriptorSetsLayoutInfo, FVulkanLayout*> LayoutMap;
	FCriticalSection LayoutMapCS;

	FVulkanGraphicsPipelineState* FindInLoadedLibrary(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOInitializerHash, const FShaderHashes& ShaderHashes, FGfxPipelineEntry*& outGfxEntry);
	
	friend class FVulkanDynamicRHI;

	FVulkanLayout* FindOrAddLayout(const FVulkanDescriptorSetsLayoutInfo& DescriptorSetLayoutInfo);

	FComputePipelineEntry* CreateComputeEntry(const FVulkanComputeShader* ComputeShader);
	FVulkanComputePipeline* CreateComputePipelineFromEntry(const FComputePipelineEntry* ComputeEntry);
	void CreateComputeEntryRuntimeObjects(FComputePipelineEntry* GfxEntry);


	struct FVulkanPipelineStateCacheFile
	{
		struct FFileHeader
		{
			int32 Version = -1;
			int32 SizeOfGfxEntry = -1;
			int32 SizeOfComputeEntry = -1;
			int32 UncompressedSize = 0; // 0 == file is uncompressed
		} Header;

		TArray<uint8> DeviceCache;
		FShaderUCodeCache::TDataMap* ShaderCache = nullptr;

		TArray<FGfxPipelineEntry*> GfxPipelineEntries;		

		TArray<FComputePipelineEntry*> ComputePipelineEntries;

		void Save(FArchive& Ar);
		bool Load(FArchive& Ar, const TCHAR* Filename);
		bool BinaryCacheMatches(FVulkanDevice* InDevice);

		static const ECompressionFlags CompressionFlags = (ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasSpeed);
	};
};

template<>
struct TVulkanResourceTraits<class FRHIComputePipelineState>
{
	typedef class FVulkanComputePipeline TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIGraphicsPipelineState>
{
	typedef FVulkanGraphicsPipelineState TConcreteType;
};