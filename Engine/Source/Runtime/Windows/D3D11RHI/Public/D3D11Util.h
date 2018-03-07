// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Util.h: D3D RHI utility definitions.
=============================================================================*/

#pragma once

#if WINVER == 0x0502
// Windows XP uses Win7 sdk, and in that one winerror.h doesn't include them

#define DXGI_ERROR_INVALID_CALL                 MAKE_DXGI_HRESULT(1)
#define DXGI_ERROR_NOT_FOUND                    MAKE_DXGI_HRESULT(2)
#define DXGI_ERROR_MORE_DATA                    MAKE_DXGI_HRESULT(3)
#define DXGI_ERROR_UNSUPPORTED                  MAKE_DXGI_HRESULT(4)
#define DXGI_ERROR_DEVICE_REMOVED               MAKE_DXGI_HRESULT(5)
#define DXGI_ERROR_DEVICE_HUNG                  MAKE_DXGI_HRESULT(6)
#define DXGI_ERROR_DEVICE_RESET                 MAKE_DXGI_HRESULT(7)
#define DXGI_ERROR_WAS_STILL_DRAWING            MAKE_DXGI_HRESULT(10)
#define DXGI_ERROR_FRAME_STATISTICS_DISJOINT    MAKE_DXGI_HRESULT(11)
#define DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE MAKE_DXGI_HRESULT(12)
#define DXGI_ERROR_DRIVER_INTERNAL_ERROR        MAKE_DXGI_HRESULT(32)
#define DXGI_ERROR_NONEXCLUSIVE                 MAKE_DXGI_HRESULT(33)
#define DXGI_ERROR_NOT_CURRENTLY_AVAILABLE      MAKE_DXGI_HRESULT(34)
#define DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED   MAKE_DXGI_HRESULT(35)
#define DXGI_ERROR_REMOTE_OUTOFMEMORY           MAKE_DXGI_HRESULT(36)

#endif

/**
 * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
 * @param	Result - The result code to check.
 * @param	Code - The code which yielded the result.
 * @param	Filename - The filename of the source file containing Code.
 * @param	Line - The line number of Code within Filename.
 */
extern D3D11RHI_API void VerifyD3D11Result(HRESULT Result,const ANSICHAR* Code,const ANSICHAR* Filename,uint32 Line, ID3D11Device* Device);

/**
 * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
 * @param	Shader - The shader we are trying to create.
 * @param	Result - The result code to check.
 * @param	Code - The code which yielded the result.
 * @param	Filename - The filename of the source file containing Code.
 * @param	Line - The line number of Code within Filename.
 * @param	Device - The D3D device used to create the shadr.
 */
extern D3D11RHI_API void VerifyD3D11ShaderResult(class FRHIShader* Shader, HRESULT Result, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line, ID3D11Device* Device);

/**
* Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
* @param	Result - The result code to check
* @param	Code - The code which yielded the result.
* @param	Filename - The filename of the source file containing Code.
* @param	Line - The line number of Code within Filename.	
*/
extern D3D11RHI_API void VerifyD3D11CreateTextureResult(HRESULT D3DResult,const ANSICHAR* Code,const ANSICHAR* Filename,uint32 Line,
										 uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 D3DFormat,uint32 NumMips,uint32 Flags, ID3D11Device* Device);


extern D3D11RHI_API void VerifyD3D11ResizeViewportResult(HRESULT D3DResult, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line,
	uint32 SizeX, uint32 SizeY, uint8 D3DFormat, ID3D11Device* Device);

/**
 * A macro for using VERIFYD3D11RESULT that automatically passes in the code and filename/line.
 */
#define VERIFYD3D11RESULT_EX(x, Device)	{HRESULT hr = x; if (FAILED(hr)) { VerifyD3D11Result(hr,#x,__FILE__,__LINE__, Device); }}
#define VERIFYD3D11RESULT(x)			{HRESULT hr = x; if (FAILED(hr)) { VerifyD3D11Result(hr,#x,__FILE__,__LINE__, 0); }}
#define VERIFYD3D11SHADERRESULT(Result, Shader, Device) {HRESULT hr = (Result); if (FAILED(hr)) { VerifyD3D11ShaderResult(Shader, hr, #Result,__FILE__,__LINE__, Device); }}
#define VERIFYD3D11CREATETEXTURERESULT(x,SizeX,SizeY,SizeZ,Format,NumMips,Flags, Device) {HRESULT hr = x; if (FAILED(hr)) { VerifyD3D11CreateTextureResult(hr,#x,__FILE__,__LINE__,SizeX,SizeY,SizeZ,Format,NumMips,Flags, Device); }}
#define VERIFYD3D11RESIZEVIEWPORTRESULT(x,SizeX,SizeY,Format, Device) {HRESULT hr = x; if (FAILED(hr)) { VerifyD3D11ResizeViewportResult(hr,#x,__FILE__,__LINE__,SizeX,SizeY,Format, Device); }}
/**
 * Checks that a COM object has the expected number of references.
 */
extern D3D11RHI_API void VerifyComRefCount(IUnknown* Object,int32 ExpectedRefs,const TCHAR* Code,const TCHAR* Filename,int32 Line);
#define checkComRefCount(Obj,ExpectedRefs) VerifyComRefCount(Obj,ExpectedRefs,TEXT(#Obj),TEXT(__FILE__),__LINE__)

/** Returns a string for the provided DXGI format. */
const TCHAR* GetD3D11TextureFormatString(DXGI_FORMAT TextureFormat);

/**
* Convert from ECubeFace to D3DCUBEMAP_FACES type
* @param Face - ECubeFace type to convert
* @return D3D cube face enum value
*/
FORCEINLINE uint32 GetD3D11CubeFace(ECubeFace Face)
{
	switch(Face)
	{
	case CubeFace_PosX:
	default:
		return 0;//D3DCUBEMAP_FACE_POSITIVE_X;
	case CubeFace_NegX:
		return 1;//D3DCUBEMAP_FACE_NEGATIVE_X;
	case CubeFace_PosY:
		return 2;//D3DCUBEMAP_FACE_POSITIVE_Y;
	case CubeFace_NegY:
		return 3;//D3DCUBEMAP_FACE_NEGATIVE_Y;
	case CubeFace_PosZ:
		return 4;//D3DCUBEMAP_FACE_POSITIVE_Z;
	case CubeFace_NegZ:
		return 5;//D3DCUBEMAP_FACE_NEGATIVE_Z;
	};
}

/**
 * Keeps track of Locks for D3D11 objects
 */
class FD3D11LockedKey
{
public:
	void* SourceObject;
	uint32 Subresource;

public:
	FD3D11LockedKey() : SourceObject(NULL)
		, Subresource(0)
	{}
	FD3D11LockedKey(ID3D11Texture2D* source, uint32 subres=0) : SourceObject((void*)source)
		, Subresource(subres)
	{}
	FD3D11LockedKey(ID3D11Texture3D* source, uint32 subres=0) : SourceObject((void*)source)
		, Subresource(subres)
	{}
	FD3D11LockedKey(ID3D11Buffer* source, uint32 subres=0) : SourceObject((void*)source)
		, Subresource(subres)
	{}
	bool operator==( const FD3D11LockedKey& Other ) const
	{
		return SourceObject == Other.SourceObject && Subresource == Other.Subresource;
	}
	bool operator!=( const FD3D11LockedKey& Other ) const
	{
		return SourceObject != Other.SourceObject || Subresource != Other.Subresource;
	}
	FD3D11LockedKey& operator=( const FD3D11LockedKey& Other )
	{
		SourceObject = Other.SourceObject;
		Subresource = Other.Subresource;
		return *this;
	}
	uint32 GetHash() const
	{
		return PointerHash( SourceObject );
	}

	/** Hashing function. */
	friend uint32 GetTypeHash( const FD3D11LockedKey& K )
	{
		return K.GetHash();
	}
};

/** Information about a D3D resource that is currently locked. */
struct FD3D11LockedData
{
	TRefCountPtr<ID3D11Resource> StagingResource;
	uint32 Pitch;
	uint32 DepthPitch;

	// constructor
	FD3D11LockedData()
		: bAllocDataWasUsed(false)
	{
	}

	// 16 byte alignment for best performance  (can be 30x faster than unaligned)
	void AllocData(uint32 Size)
	{
		Data = (uint8*)FMemory::Malloc(Size, 16);
		bAllocDataWasUsed = true;
	}

	// Some driver might return aligned memory so we don't enforce the alignment
	void SetData(void* InData)
	{
		check(!bAllocDataWasUsed); Data = (uint8*)InData;
	}

	uint8* GetData() const
	{
		return Data;
	}

	// only call if AllocData() was used
	void FreeData()
	{
		check(bAllocDataWasUsed);
		FMemory::Free(Data);
		Data = 0;
	}

private:
	//
	uint8* Data;
	// then FreeData
	bool bAllocDataWasUsed;
};

/**
 * Class for retrieving render targets currently bound to the device context.
 */
class FD3D11BoundRenderTargets
{
public:
	/** Initialization constructor: requires the device context. */
	explicit FD3D11BoundRenderTargets(ID3D11DeviceContext* InDeviceContext);

	/** Destructor. */
	~FD3D11BoundRenderTargets();

	/** Accessors. */
	FORCEINLINE int32 GetNumActiveTargets() const { return NumActiveTargets; }
	FORCEINLINE ID3D11RenderTargetView* GetRenderTargetView(int32 TargetIndex) { return RenderTargetViews[TargetIndex]; }
	FORCEINLINE ID3D11DepthStencilView* GetDepthStencilView() { return DepthStencilView; }

private:
	/** Active render target views. */
	ID3D11RenderTargetView* RenderTargetViews[MaxSimultaneousRenderTargets];
	/** Active depth stencil view. */
	ID3D11DepthStencilView* DepthStencilView;
	/** The number of active render targets. */
	int32 NumActiveTargets;
};

/**
 * Class for managing dynamic buffers.
 */
class FD3D11DynamicBuffer : public FRenderResource, public FRefCountedObject
{
public:
	/** Initialization constructor. */
	FD3D11DynamicBuffer(class FD3D11DynamicRHI* InD3DRHI, D3D11_BIND_FLAG InBindFlags, uint32* InBufferSizes);
	/** Destructor. */
	~FD3D11DynamicBuffer();

	/** Locks the buffer returning at least Size bytes. */
	void* Lock(uint32 Size);
	/** Unlocks the buffer returning the underlying D3D11 buffer to use as a resource. */
	ID3D11Buffer* Unlock();

	//~ Begin FRenderResource Interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	// End FRenderResource interface.

private:
	/** The maximum number of sub-buffers supported. */
	enum { MAX_BUFFER_SIZES = 4 };
	/** The size of each sub-buffer. */
	TArray<uint32,TFixedAllocator<MAX_BUFFER_SIZES> > BufferSizes;
	/** The sub-buffers. */
	TArray<TRefCountPtr<ID3D11Buffer>,TFixedAllocator<MAX_BUFFER_SIZES> > Buffers;
	/** The D3D11 RHI to that owns this dynamic buffer. */
	class FD3D11DynamicRHI* D3DRHI;
	/** Bind flags to use when creating sub-buffers. */
	D3D11_BIND_FLAG BindFlags;
	/** The index of the currently locked sub-buffer. */
	int32 LockedBufferIndex;
};
