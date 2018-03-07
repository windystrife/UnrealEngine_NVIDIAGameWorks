// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RawIndexBuffer.h: Raw index buffer definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "Containers/DynamicRHIResourceArray.h"

#define DISALLOW_32BIT_INDICES 0

class FRawIndexBuffer : public FIndexBuffer
{
public:

	TArray<uint16> Indices;

	/**
	 * Orders a triangle list for better vertex cache coherency.
	 */
	void CacheOptimize();

	// FRenderResource interface.
	virtual void InitRHI() override;

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,FRawIndexBuffer& I);
};

#if DISALLOW_32BIT_INDICES

// if 32 bit indices are disallowed, then use 16 bits in the FRawIndexBuffer16or32
class FRawIndexBuffer16or32 : public FRawIndexBuffer
{
};

#else

class FRawIndexBuffer16or32 : public FIndexBuffer
{
public:
	FRawIndexBuffer16or32()
		: b32Bit(true)
	{
	}

	TArray<uint32> Indices;

	/**
	 * Orders a triangle list for better vertex cache coherency.
	 */
	void CacheOptimize();

	/**
	 * Computes whether index buffer should be 32 bit
	 */
	void ComputeIndexWidth();

	// FRenderResource interface.
	virtual void InitRHI() override;

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,FRawIndexBuffer16or32& I);

private:
	bool b32Bit;
};
#endif

/**
 * Desired stride when creating a static index buffer.
 */
namespace EIndexBufferStride
{
	enum Type
	{
		/** Forces all indices to be 16-bit. */
		Force16Bit = 1,
		/** Forces all indices to be 32-bit. */
		Force32Bit = 2,
		/** Use 16 bits unless an index exceeds MAX_uint16. */
		AutoDetect = 3
	};
}

/**
 * An array view in to a static index buffer. Allows access to the underlying
 * indices regardless of their type without a copy.
 */
class FIndexArrayView
{
public:
	/** Default constructor. */
	FIndexArrayView()
		: UntypedIndexData(NULL)
		, NumIndices(0)
		, b32Bit(false)
	{
	}

	/**
	 * Initialization constructor.
	 * @param InIndexData	A pointer to untyped index data.
	 * @param InNumIndices	The number of indices stored in the untyped index data.
	 * @param bIn32Bit		True if the data is stored as an array of uint32, false for uint16.
	 */
	FIndexArrayView(const void* InIndexData, int32 InNumIndices, bool bIn32Bit)
		: UntypedIndexData(InIndexData)
		, NumIndices(InNumIndices)
		, b32Bit(bIn32Bit)
	{
	}

	/** Common array access semantics. */
	uint32 operator[](int32 i) { return (uint32)(b32Bit ? ((const uint32*)UntypedIndexData)[i] : ((const uint16*)UntypedIndexData)[i]); }
	uint32 operator[](int32 i) const { return (uint32)(b32Bit ? ((const uint32*)UntypedIndexData)[i] : ((const uint16*)UntypedIndexData)[i]); }
	FORCEINLINE int32 Num() const { return NumIndices; }

private:
	/** Pointer to the untyped index data. */
	const void* UntypedIndexData;
	/** The number of indices stored in the untyped index data. */
	int32 NumIndices;
	/** True if the data is stored as an array of uint32, false for uint16. */
	bool b32Bit;
};

class FRawStaticIndexBuffer : public FIndexBuffer
{
public:	
	/**
	 * Initialization constructor.
	 * @param InNeedsCPUAccess	True if resource array data should be accessible by the CPU.
	 */
	FRawStaticIndexBuffer(bool InNeedsCPUAccess=false);

	/**
	 * Set the indices stored within this buffer.
	 * @param InIndices		The new indices to copy in to the buffer.
	 * @param DesiredStride	The desired stride (16 or 32 bits).
	 */
	ENGINE_API void SetIndices(const TArray<uint32>& InIndices, EIndexBufferStride::Type DesiredStride);

	/**
	 * Retrieve a copy of the indices in this buffer. Only valid if created with
	 * NeedsCPUAccess set to true or the resource has not yet been initialized.
	 * @param OutIndices	Array in which to store the copy of the indices.
	 */
	ENGINE_API void GetCopy(TArray<uint32>& OutIndices) const;

	/**
	 * Retrieves an array view in to the index buffer. The array view allows code
	 * to retrieve indices as 32-bit regardless of how they are stored internally
	 * without a copy. The array view is valid only if:
	 *		The buffer was created with NeedsCPUAccess = true
	 *		  OR the resource has not yet been initialized
	 *		  AND SetIndices has not been called since.
	 */
	ENGINE_API FIndexArrayView GetArrayView() const;

	/**
	 * Computes the number of indices stored in this buffer.
	 */
	FORCEINLINE int32 GetNumIndices() const
	{
		return b32Bit ? (IndexStorage.Num()/4) : (IndexStorage.Num()/2);
	}

	/**
	 * Computes the amount of memory allocated to store the indices.
	 */
	FORCEINLINE uint32 GetAllocatedSize() const
	{
		return IndexStorage.GetAllocatedSize();
	}

	/**
	 * Serialization.
	 * @param	Ar				Archive to serialize with
	 * @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
	 */
	void Serialize(FArchive& Ar, bool bNeedsCPUAccess);

	// FRenderResource interface.
	virtual void InitRHI() override;

	inline bool Is32Bit() const { return b32Bit; }

private:
	/** Storage for indices. */
	TResourceArray<uint8,INDEXBUFFER_ALIGNMENT> IndexStorage;
	/** 32bit or 16bit? */
	bool b32Bit;
};

/**
 * Virtual interface for the FRawStaticIndexBuffer16or32 class
 */
class FRawStaticIndexBuffer16or32Interface : public FIndexBuffer
{
public:
	virtual void Serialize( FArchive& Ar ) = 0;

	/**
	 * The following methods are basically just accessors that allow us
	 * to hide the implementation of FRawStaticIndexBuffer16or32 by making
	 * the index array a private member
	 */
	virtual bool GetNeedsCPUAccess() const = 0;
	// number of indices (e.g. 4 triangles would result in 12 elements)
	virtual int32 Num() const = 0;
	virtual int32 AddItem(uint32 Val) = 0;
	virtual uint32 Get(uint32 Idx) const = 0;
	virtual void* GetPointerTo(uint32 Idx) = 0;
	virtual void Insert(int32 Idx, int32 Num = 1) = 0;
	virtual void Remove(int32 Idx, int32 Num = 1) = 0;
	virtual void Empty(int32 Slack = 0) = 0;
	virtual int32 GetResourceDataSize() const = 0;

	// @param guaranteed only to be valid if the vertex buffer is valid and the buffer was created with the SRV flags
	FShaderResourceViewRHIParamRef GetSRV() const
	{
		return SRVValue;
	}

protected:
	// guaranteed only to be valid if the vertex buffer is valid and the buffer was created with the SRV flags
	FShaderResourceViewRHIRef SRVValue;
};

template <typename INDEX_TYPE>
class FRawStaticIndexBuffer16or32 : public FRawStaticIndexBuffer16or32Interface
{
public:	
	/**
	* Constructor
	* @param InNeedsCPUAccess - true if resource array data should be CPU accessible
	*/
	FRawStaticIndexBuffer16or32(bool InNeedsCPUAccess=false)
	:	Indices(InNeedsCPUAccess)
	{
#if DISALLOW_32BIT_INDICES
		static_assert(sizeof(INDEX_TYPE) == sizeof(uint16), "DISALLOW_32BIT_INDICES is defined, so you should not use 32-bit indices.");
#endif
	}

	/**
	* Create the index buffer RHI resource and initialize its data
	*/
	virtual void InitRHI() override
	{
		uint32 Size = Indices.Num() * sizeof(INDEX_TYPE);
		if(Indices.Num())
		{
			// Create the index buffer.
			FRHIResourceCreateInfo CreateInfo(&Indices);
			
			extern ENGINE_API bool DoSkeletalMeshIndexBuffersNeedSRV();
			bool bSRV = DoSkeletalMeshIndexBuffersNeedSRV();

			EBufferUsageFlags Flags = BUF_Static;

			if(bSRV)
			{
				// BUF_ShaderResource is needed for SkinCache RecomputeSkinTangents
				Flags = (EBufferUsageFlags)(Flags | BUF_ShaderResource);
			}

			IndexBufferRHI = RHICreateIndexBuffer(sizeof(INDEX_TYPE), Size, Flags, CreateInfo);

			if(bSRV)
			{
				SRVValue = RHICreateShaderResourceView(IndexBufferRHI);
			}
		}    
	}
	
	virtual void ReleaseRHI() override
	{
		FRawStaticIndexBuffer16or32Interface::ReleaseRHI();

		SRVValue.SafeRelease();
	}

	/**
	* Serializer for this class
	* @param Ar - archive to serialize to
	* @param I - data to serialize
	*/
	virtual void Serialize( FArchive& Ar ) override
	{
			Indices.BulkSerialize( Ar );
	}

	/**
	* Orders a triangle list for better vertex cache coherency.
	*/
	void CacheOptimize();

	
	/**
	 * The following methods are basically just accessors that allow us
	 * to hide the implementation by making the index array a private member
	 */
	virtual bool GetNeedsCPUAccess() const override { return Indices.GetAllowCPUAccess(); }

	virtual int32 Num() const override
	{
		return Indices.Num();
	}

	virtual int32 AddItem(uint32 Val) override
	{
		return Indices.Add(Val);
	}

	virtual uint32 Get(uint32 Idx) const override
	{
		return (uint32)Indices[Idx];
	}

	virtual void* GetPointerTo(uint32 Idx) override
	{
		return (void*)(&Indices[Idx]);
	}

	virtual void Insert(int32 Idx, int32 Num) override
	{
		Indices.InsertUninitialized(Idx, Num);
	}

	virtual void Remove(int32 Idx, int32 Num) override
	{
		Indices.RemoveAt(Idx, Num);
	}

	virtual void Empty(int32 Slack) override
	{
		Indices.Empty(Slack);
	}

	virtual int32 GetResourceDataSize() const override
	{
		return Indices.GetResourceDataSize();
	}

	virtual void AssignNewBuffer(const TArray<INDEX_TYPE>& Buffer)
	{
		Indices = TArray<INDEX_TYPE,TAlignedHeapAllocator<INDEXBUFFER_ALIGNMENT> >(Buffer);
	}

private:
	TResourceArray<INDEX_TYPE,INDEXBUFFER_ALIGNMENT> Indices;
};
