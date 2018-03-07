// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLResources.h: OpenGL resource RHI definitions.
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"
#include "Logging/LogMacros.h"
#include "Containers/BitArray.h"
#include "Math/IntPoint.h"
#include "Misc/CommandLine.h"
#include "Templates/RefCounting.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "BoundShaderStateCache.h"
#include "RenderResource.h"
#include "OpenGLShaderResources.h"
#include "ShaderCache.h"

class FOpenGLDynamicRHI;
class FOpenGLLinkedProgram;

extern void OnVertexBufferDeletion( GLuint VertexBufferResource );
extern void OnIndexBufferDeletion( GLuint IndexBufferResource );
extern void OnPixelBufferDeletion( GLuint PixelBufferResource );
extern void OnUniformBufferDeletion( GLuint UniformBufferResource, uint32 AllocatedSize, bool bStreamDraw, uint32 Offset, uint8* Pointer );
extern void OnProgramDeletion( GLint ProgramResource );

extern void CachedBindArrayBuffer( GLuint Buffer );
extern void CachedBindElementArrayBuffer( GLuint Buffer );
extern void CachedBindPixelUnpackBuffer( GLuint Buffer );
extern void CachedBindUniformBuffer( GLuint Buffer );
extern bool IsUniformBufferBound( GLuint Buffer );

namespace OpenGLConsoleVariables
{
	extern int32 bUseMapBuffer;
	extern int32 bUseVAB;
	extern int32 MaxSubDataSize;
	extern int32 bUseStagingBuffer;
	extern int32 bBindlessTexture;
	extern int32 bUseBufferDiscard;
};

#if PLATFORM_WINDOWS || PLATFORM_ANDROIDESDEFERRED
#define RESTRICT_SUBDATA_SIZE 1
#else
#define RESTRICT_SUBDATA_SIZE 0
#endif

void IncrementBufferMemory(GLenum Type, bool bStructuredBuffer, uint32 NumBytes);
void DecrementBufferMemory(GLenum Type, bool bStructuredBuffer, uint32 NumBytes);

// Extra stats for finer-grained timing
// They shouldn't always be on, as they may impact overall performance
#define OPENGLRHI_DETAILED_STATS 0
#if OPENGLRHI_DETAILED_STATS
	DECLARE_CYCLE_STAT_EXTERN(TEXT("MapBuffer time"),STAT_OpenGLMapBufferTime,STATGROUP_OpenGLRHI, );
	DECLARE_CYCLE_STAT_EXTERN(TEXT("UnmapBuffer time"),STAT_OpenGLUnmapBufferTime,STATGROUP_OpenGLRHI, );
	#define SCOPE_CYCLE_COUNTER_DETAILED(Stat)	SCOPE_CYCLE_COUNTER(Stat)
#else
	#define SCOPE_CYCLE_COUNTER_DETAILED(Stat)
#endif

typedef void (*BufferBindFunction)( GLuint Buffer );

template <typename BaseType, GLenum Type, BufferBindFunction BufBind>
class TOpenGLBuffer : public BaseType
{
	void LoadData( uint32 InOffset, uint32 InSize, const void* InData)
	{
		const uint8* Data = (const uint8*)InData;
		const uint32 BlockSize = OpenGLConsoleVariables::MaxSubDataSize;

		if (BlockSize > 0)
		{
			while ( InSize > 0)
			{
				const uint32 BufferSize = FMath::Min<uint32>( BlockSize, InSize);

				FOpenGL::BufferSubData( Type, InOffset, BufferSize, Data);

				InOffset += BufferSize;
				InSize -= BufferSize;
				Data += BufferSize;
			}
		}
		else
		{
			FOpenGL::BufferSubData( Type, InOffset, InSize, InData);
		}
	}

	GLenum GetAccess()
	{
		// Previously there was special-case logic to always use GL_STATIC_DRAW for vertex buffers allocated from staging buffer.
		// However it seems to be incorrect as NVidia drivers complain (via debug output callback) about VIDEO->HOST copying for buffers with such hints
		return bStreamDraw ? GL_STREAM_DRAW : (IsDynamic() ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	}
public:

	GLuint Resource;

	/** Needed on OS X to force a rebind of the texture buffer to the texture name to workaround radr://18379338 */
	uint64 ModificationCount;

	TOpenGLBuffer(uint32 InStride,uint32 InSize,uint32 InUsage,
		const void *InData = NULL, bool bStreamedDraw = false, GLuint ResourceToUse = 0, uint32 ResourceSize = 0)
	: BaseType(InStride,InSize,InUsage)
	, Resource(0)
	, ModificationCount(0)
	, bIsLocked(false)
	, bIsLockReadOnly(false)
	, bStreamDraw(bStreamedDraw)
	, bLockBufferWasAllocated(false)
	, LockSize(0)
	, LockOffset(0)
	, LockBuffer(NULL)
	, RealSize(InSize)
	{
		if( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || !( InUsage & BUF_ZeroStride ) )
		{
			VERIFY_GL_SCOPE();
			RealSize = ResourceSize ? ResourceSize : InSize;
			if( ResourceToUse )
			{
				Resource = ResourceToUse;
				check( Type != GL_UNIFORM_BUFFER || !IsUniformBufferBound(Resource) );
				Bind();
				FOpenGL::BufferSubData(Type, 0, InSize, InData);
			}
			else
			{
				if (BaseType::GLSupportsType())
				{
					FOpenGL::GenBuffers(1, &Resource);
					check( Type != GL_UNIFORM_BUFFER || !IsUniformBufferBound(Resource) );
					Bind();
#if !RESTRICT_SUBDATA_SIZE
					if( InData == NULL || RealSize <= InSize )
					{
						glBufferData(Type, RealSize, InData, GetAccess());
					}
					else
					{
						glBufferData(Type, RealSize, NULL, GetAccess());
						FOpenGL::BufferSubData(Type, 0, InSize, InData);
					}
#else
					glBufferData(Type, RealSize, NULL, GetAccess());
					if ( InData != NULL )
					{
						LoadData( 0, FMath::Min<uint32>(InSize,RealSize), InData);
					}
#endif
					IncrementBufferMemory(Type, BaseType::IsStructuredBuffer(), RealSize);
				}
				else
				{
					BaseType::CreateType(Resource, InData, InSize);
				}
			}
		}
	}

	virtual ~TOpenGLBuffer()
	{
		VERIFY_GL_SCOPE();
		if (Resource != 0 && BaseType::OnDelete(Resource,RealSize,bStreamDraw,0))
		{
			FOpenGL::DeleteBuffers(1, &Resource);
			DecrementBufferMemory(Type, BaseType::IsStructuredBuffer(), RealSize);
		}
		if (LockBuffer != NULL)
		{
			if (bLockBufferWasAllocated)
			{
				FMemory::Free(LockBuffer);
			}
			else
			{
				UE_LOG(LogRHI,Warning,TEXT("Destroying TOpenGLBuffer without returning memory to the driver; possibly called RHIMapStagingSurface() but didn't call RHIUnmapStagingSurface()? Resource %u"), Resource);
			}
			LockBuffer = NULL;
		}
	}

	void Bind()
	{
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		BufBind(Resource);
	}

	uint8 *Lock(uint32 InOffset, uint32 InSize, bool bReadOnly, bool bDiscard)
	{
		//SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLMapBufferTime);
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		check(InOffset + InSize <= this->GetSize());
		//check( LockBuffer == NULL );	// Only one outstanding lock is allowed at a time!
		check( !bIsLocked );	// Only one outstanding lock is allowed at a time!
		VERIFY_GL_SCOPE();

		Bind();

		bIsLocked = true;
		bIsLockReadOnly = bReadOnly;
		uint8 *Data = NULL;

		// Discard if the input size is the same as the backing store size, regardless of the input argument, as orphaning the backing store will typically be faster.
		bDiscard = bDiscard || (!bReadOnly && InSize == RealSize);

#if PLATFORM_HTML5
		// In browsers calling glBufferData() to discard-reupload is slower than calling glBufferSubData(),
		// because changing glBufferData() with a different size from before incurs security related validation.
		// Therefore never use the glBufferData() discard trick on HTML5 builds.
		bDiscard = false;
#endif

		// Map buffer is faster in some circumstances and slower in others, decide when to use it carefully.
		bool const bCanUseMapBuffer = FOpenGL::SupportsMapBuffer() && BaseType::GLSupportsType();
		bool const bUseMapBuffer = bCanUseMapBuffer && (bReadOnly || OpenGLConsoleVariables::bUseMapBuffer);

		// If we're able to discard the current data, do so right away
		// If we can then we should orphan the buffer name & reallocate the backing store only once as calls to glBufferData may do so even when the size is the same.
		uint32 DiscardSize = (bDiscard && !bUseMapBuffer && InSize == RealSize && !RESTRICT_SUBDATA_SIZE) ? 0 : RealSize;

		// Don't call BufferData if Bindless is on, as bindless texture buffers make buffers immutable
		if ( bDiscard && !OpenGLConsoleVariables::bBindlessTexture && OpenGLConsoleVariables::bUseBufferDiscard)
		{
			if (BaseType::GLSupportsType())
			{
				glBufferData( Type, DiscardSize, NULL, GetAccess());
			}
		}

		if ( bUseMapBuffer)
		{
			FOpenGL::EResourceLockMode LockMode = bReadOnly ? FOpenGL::RLM_ReadOnly : FOpenGL::RLM_WriteOnly;
			Data = static_cast<uint8*>( FOpenGL::MapBufferRange( Type, InOffset, InSize, LockMode ) );
//			checkf(Data != NULL, TEXT("FOpenGL::MapBufferRange Failed, glError %d (0x%x)"), glGetError(), glGetError());

			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = Data;
			bLockBufferWasAllocated = false;
		}
		else
		{
			// Allocate a temp buffer to write into
			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = FMemory::Malloc( InSize );
			Data = static_cast<uint8*>( LockBuffer );
			bLockBufferWasAllocated = true;
		}

		check(Data != NULL);
		return Data;
	}

	uint8 *LockWriteOnlyUnsynchronized(uint32 InOffset, uint32 InSize, bool bDiscard)
	{
		//SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLMapBufferTime);
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		check(InOffset + InSize <= this->GetSize());
		//check( LockBuffer == NULL );	// Only one outstanding lock is allowed at a time!
		check( !bIsLocked );	// Only one outstanding lock is allowed at a time!
		VERIFY_GL_SCOPE();

		Bind();

		bIsLocked = true;
		bIsLockReadOnly = false;
		uint8 *Data = NULL;

		// Discard if the input size is the same as the backing store size, regardless of the input argument, as orphaning the backing store will typically be faster.
		bDiscard = bDiscard || InSize == RealSize;

#if PLATFORM_HTML5
		// In browsers calling glBufferData() to discard-reupload is slower than calling glBufferSubData(),
		// because changing glBufferData() with a different size from before incurs security related validation.
		// Therefore never use the glBufferData() discard trick on HTML5 builds.
		bDiscard = false;
#endif

		// Map buffer is faster in some circumstances and slower in others, decide when to use it carefully.
		bool const bCanUseMapBuffer = FOpenGL::SupportsMapBuffer() && BaseType::GLSupportsType();
		bool const bUseMapBuffer = bCanUseMapBuffer && OpenGLConsoleVariables::bUseMapBuffer;

		// If we're able to discard the current data, do so right away
		// If we can then we should orphan the buffer name & reallocate the backing store only once as calls to glBufferData may do so even when the size is the same.
		uint32 DiscardSize = (bDiscard && !bUseMapBuffer && InSize == RealSize && !RESTRICT_SUBDATA_SIZE) ? 0 : RealSize;

		// Don't call BufferData if Bindless is on, as bindless texture buffers make buffers immutable
		if ( bDiscard && !OpenGLConsoleVariables::bBindlessTexture && OpenGLConsoleVariables::bUseBufferDiscard)
		{
			if (BaseType::GLSupportsType())
			{
				glBufferData( Type, DiscardSize, NULL, GetAccess());
			}
		}

		if ( bUseMapBuffer)
		{
			FOpenGL::EResourceLockMode LockMode = bDiscard ? FOpenGL::RLM_WriteOnly : FOpenGL::RLM_WriteOnlyUnsynchronized;
			Data = static_cast<uint8*>( FOpenGL::MapBufferRange( Type, InOffset, InSize, LockMode ) );
			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = Data;
			bLockBufferWasAllocated = false;
		}
		else
		{
			// Allocate a temp buffer to write into
			LockOffset = InOffset;
			LockSize = InSize;
			LockBuffer = FMemory::Malloc( InSize );
			Data = static_cast<uint8*>( LockBuffer );
			bLockBufferWasAllocated = true;
		}

		check(Data != NULL);
		return Data;
	}

	void Unlock()
	{
		//SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLUnmapBufferTime);
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		VERIFY_GL_SCOPE();
		if (bIsLocked)
		{
			Bind();

			if ( FOpenGL::SupportsMapBuffer() && BaseType::GLSupportsType() && (OpenGLConsoleVariables::bUseMapBuffer || bIsLockReadOnly))
			{
				check(!bLockBufferWasAllocated);
				if (Type == GL_ARRAY_BUFFER || Type == GL_ELEMENT_ARRAY_BUFFER)
				{
					FOpenGL::UnmapBufferRange(Type, LockOffset, LockSize);
				}
				else
				{
					FOpenGL::UnmapBuffer(Type);
				}
				LockBuffer = NULL;
			}
			else
			{
				if (BaseType::GLSupportsType())
				{
#if !RESTRICT_SUBDATA_SIZE
					// Check for the typical, optimized case
					if( LockSize == RealSize )
					{
#if PLATFORM_HTML5
						// In browsers using glBufferData() to upload data is slower
						// than using glBufferSubData(), because glBufferData()
						// can resize the buffer storage, and so incurs extra validation.
						FOpenGL::BufferSubData(Type, 0, LockSize, LockBuffer);
#else
						glBufferData(Type, RealSize, LockBuffer, GetAccess());
#endif
						check( LockBuffer != NULL );
					}
					else
					{
						// Only updating a subset of the data
						FOpenGL::BufferSubData(Type, LockOffset, LockSize, LockBuffer);
						check( LockBuffer != NULL );
					}
#else
					LoadData( LockOffset, LockSize, LockBuffer);
					check( LockBuffer != NULL);
#endif
				}
				check(bLockBufferWasAllocated);
				FMemory::Free(LockBuffer);
				LockBuffer = NULL;
				bLockBufferWasAllocated = false;
			}
			ModificationCount += (bIsLockReadOnly ? 0 : 1);
			bIsLocked = false;
		}
	}

	void Update(void *InData, uint32 InOffset, uint32 InSize, bool bDiscard)
	{
		check( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || ( this->GetUsage() & BUF_ZeroStride ) == 0 );
		check(InOffset + InSize <= this->GetSize());
		VERIFY_GL_SCOPE();
		Bind();
#if !RESTRICT_SUBDATA_SIZE
		FOpenGL::BufferSubData(Type, InOffset, InSize, InData);
#else
		LoadData( InOffset, InSize, InData);
#endif
		ModificationCount++;
	}

	bool IsDynamic() const { return (this->GetUsage() & BUF_AnyDynamic) != 0; }
	bool IsLocked() const { return bIsLocked; }
	bool IsLockReadOnly() const { return bIsLockReadOnly; }
	void* GetLockedBuffer() const { return LockBuffer; }

private:

	uint32 bIsLocked : 1;
	uint32 bIsLockReadOnly : 1;
	uint32 bStreamDraw : 1;
	uint32 bLockBufferWasAllocated : 1;

	GLuint LockSize;
	GLuint LockOffset;
	void* LockBuffer;

	uint32 RealSize;	// sometimes (for example, for uniform buffer pool) we allocate more in OpenGL than is requested of us.
};

class FOpenGLBasePixelBuffer : public FRefCountedObject
{
public:
	FOpenGLBasePixelBuffer(uint32 InStride,uint32 InSize,uint32 InUsage)
	: Size(InSize)
	, Usage(InUsage)
	{}
	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnPixelBufferDeletion(Resource);
		return true;
	}
	uint32 GetSize() const { return Size; }
	uint32 GetUsage() const { return Usage; }

	static FORCEINLINE bool GLSupportsType()
	{
		return FOpenGL::SupportsPixelBuffers();
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

	static bool IsStructuredBuffer() { return false; }

private:
	uint32 Size;
	uint32 Usage;
};

class FOpenGLBaseVertexBuffer : public FRHIVertexBuffer
{
public:
	FOpenGLBaseVertexBuffer(uint32 InStride,uint32 InSize,uint32 InUsage): FRHIVertexBuffer(InSize,InUsage), ZeroStrideVertexBuffer(0)
	{
		if(!(FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) && InUsage & BUF_ZeroStride )
		{
			ZeroStrideVertexBuffer = FMemory::Malloc( InSize );
		}
	}

	~FOpenGLBaseVertexBuffer( void )
	{
		if( ZeroStrideVertexBuffer )
		{
			FMemory::Free( ZeroStrideVertexBuffer );
		}
	}

	void* GetZeroStrideBuffer( void )
	{
		check( ZeroStrideVertexBuffer );
		return ZeroStrideVertexBuffer;
	}

	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnVertexBufferDeletion(Resource);
		return true;
	}

	static FORCEINLINE bool GLSupportsType()
	{
		return true;
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

	static bool IsStructuredBuffer() { return false; }

private:
	void*	ZeroStrideVertexBuffer;
};

struct FOpenGLEUniformBufferData : public FRefCountedObject
{
	FOpenGLEUniformBufferData(uint32 SizeInBytes)
	{
		uint32 SizeInUint32s = (SizeInBytes + 3) / 4;
		Data.Empty(SizeInUint32s);
		Data.AddUninitialized(SizeInUint32s);
		IncrementBufferMemory(GL_UNIFORM_BUFFER,false,Data.GetAllocatedSize());
	}

	~FOpenGLEUniformBufferData()
	{
		DecrementBufferMemory(GL_UNIFORM_BUFFER,false,Data.GetAllocatedSize());
	}

	TArray<uint32> Data;
};
typedef TRefCountPtr<FOpenGLEUniformBufferData> FOpenGLEUniformBufferDataRef;

class FOpenGLUniformBuffer : public FRHIUniformBuffer
{
public:
	/** The GL resource for this uniform buffer. */
	GLuint Resource;

	/** The offset of the uniform buffer's contents in the resource. */
	uint32 Offset;

	/** When using a persistently mapped buffer this is a pointer to the CPU accessible data. */
	uint8* PersistentlyMappedBuffer;

	/** Unique ID for state shadowing purposes. */
	const uint32 UniqueID;

	/** Resource table containing RHI references. */
	TArray<TRefCountPtr<FRHIResource> > ResourceTable;

	/** Emulated uniform data for ES2. */
	FOpenGLEUniformBufferDataRef EmulatedBufferData;

	/** The size of the buffer allocated to hold the uniform buffer contents. May be larger than necessary. */
	uint32 AllocatedSize;

	/** True if the uniform buffer is not used across frames. */
	bool bStreamDraw;

	/** Initialization constructor. */
	FOpenGLUniformBuffer(const FRHIUniformBufferLayout& InLayout, GLuint InResource, uint32 InOffset, uint8* InPersistentlyMappedBuffer, uint32 InAllocatedSize, FOpenGLEUniformBufferDataRef& InEmulatedBuffer, bool bInStreamDraw);

	/** Destructor. */
	~FOpenGLUniformBuffer();
};


class FOpenGLBaseIndexBuffer : public FRHIIndexBuffer
{
public:
	FOpenGLBaseIndexBuffer(uint32 InStride,uint32 InSize,uint32 InUsage): FRHIIndexBuffer(InStride,InSize,InUsage) {}
	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnIndexBufferDeletion(Resource);
		return true;
	}

	static FORCEINLINE bool GLSupportsType()
	{
		return true;
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

	static bool IsStructuredBuffer() { return false; }
};

class FOpenGLBaseStructuredBuffer : public FRHIStructuredBuffer
{
public:
	FOpenGLBaseStructuredBuffer(uint32 InStride,uint32 InSize,uint32 InUsage): FRHIStructuredBuffer(InStride,InSize,InUsage) {}
	static bool OnDelete(GLuint Resource,uint32 Size,bool bStreamDraw,uint32 Offset)
	{
		OnVertexBufferDeletion(Resource);
		return true;
	}

	static FORCEINLINE bool GLSupportsType()
	{
		return FOpenGL::SupportsStructuredBuffers();
	}

	static void CreateType(GLuint& Resource, const void* InData, uint32 InSize)
	{
		// @todo-mobile
	}

	static bool IsStructuredBuffer() { return true; }
};

typedef TOpenGLBuffer<FOpenGLBasePixelBuffer, GL_PIXEL_UNPACK_BUFFER, CachedBindPixelUnpackBuffer> FOpenGLPixelBuffer;
typedef TOpenGLBuffer<FOpenGLBaseVertexBuffer, GL_ARRAY_BUFFER, CachedBindArrayBuffer> FOpenGLVertexBuffer;
typedef TOpenGLBuffer<FOpenGLBaseIndexBuffer,GL_ELEMENT_ARRAY_BUFFER,CachedBindElementArrayBuffer> FOpenGLIndexBuffer;
typedef TOpenGLBuffer<FOpenGLBaseStructuredBuffer,GL_ARRAY_BUFFER,CachedBindArrayBuffer> FOpenGLStructuredBuffer;

#define MAX_STREAMED_BUFFERS_IN_ARRAY 2	// must be > 1!
#define MIN_DRAWS_IN_SINGLE_BUFFER 16

template <typename BaseType, uint32 Stride>
class TOpenGLStreamedBufferArray
{
public:

	TOpenGLStreamedBufferArray( void ) {}
	virtual ~TOpenGLStreamedBufferArray( void ) {}

	void Init( uint32 InitialBufferSize )
	{
		for( int32 BufferIndex = 0; BufferIndex < MAX_STREAMED_BUFFERS_IN_ARRAY; ++BufferIndex )
		{
			Buffer[BufferIndex] = new BaseType(Stride, InitialBufferSize, BUF_Volatile, NULL, true);
		}
		CurrentBufferIndex = 0;
		CurrentOffset = 0;
		LastOffset = 0;
		MinNeededBufferSize = InitialBufferSize / MIN_DRAWS_IN_SINGLE_BUFFER;
	}

	void Cleanup( void )
	{
		for( int32 BufferIndex = 0; BufferIndex < MAX_STREAMED_BUFFERS_IN_ARRAY; ++BufferIndex )
		{
			Buffer[BufferIndex].SafeRelease();
		}
	}

	uint8* Lock( uint32 DataSize )
	{
		check(!Buffer[CurrentBufferIndex]->IsLocked());
		DataSize = Align(DataSize, (1<<8));	// to keep the speed up, let's start data for each next draw at 256-byte aligned offset

		// Keep our dynamic buffers at least MIN_DRAWS_IN_SINGLE_BUFFER times bigger than max single request size
		uint32 NeededBufSize = Align( MIN_DRAWS_IN_SINGLE_BUFFER * DataSize, (1 << 20) );	// 1MB increments
		if (NeededBufSize > MinNeededBufferSize)
		{
			MinNeededBufferSize = NeededBufSize;
		}

		// Check if we need to switch buffer, as the current draw data won't fit in current one
		bool bDiscard = false;
		if (Buffer[CurrentBufferIndex]->GetSize() < CurrentOffset + DataSize)
		{
			// We do.
			++CurrentBufferIndex;
			if (CurrentBufferIndex == MAX_STREAMED_BUFFERS_IN_ARRAY)
			{
				CurrentBufferIndex = 0;
			}
			CurrentOffset = 0;

			// Check if we should extend the next buffer, as max request size has changed
			if (MinNeededBufferSize > Buffer[CurrentBufferIndex]->GetSize())
			{
				Buffer[CurrentBufferIndex].SafeRelease();
				Buffer[CurrentBufferIndex] = new BaseType(Stride, MinNeededBufferSize, BUF_Volatile);
			}

			bDiscard = true;
		}

		LastOffset = CurrentOffset;
		CurrentOffset += DataSize;

		return Buffer[CurrentBufferIndex]->LockWriteOnlyUnsynchronized(LastOffset, DataSize, bDiscard);
	}

	void Unlock( void )
	{
		check(Buffer[CurrentBufferIndex]->IsLocked());
		Buffer[CurrentBufferIndex]->Unlock();
	}

	BaseType* GetPendingBuffer( void ) { return Buffer[CurrentBufferIndex]; }
	uint32 GetPendingOffset( void ) { return LastOffset; }

private:
	TRefCountPtr<BaseType> Buffer[MAX_STREAMED_BUFFERS_IN_ARRAY];
	uint32 CurrentBufferIndex;
	uint32 CurrentOffset;
	uint32 LastOffset;
	uint32 MinNeededBufferSize;
};

typedef TOpenGLStreamedBufferArray<FOpenGLVertexBuffer,0> FOpenGLStreamedVertexBufferArray;
typedef TOpenGLStreamedBufferArray<FOpenGLIndexBuffer,sizeof(uint16)> FOpenGLStreamedIndexBufferArray;

struct FOpenGLVertexElement
{
	GLenum Type;
	GLuint StreamIndex;
	GLuint Offset;
	GLuint Size;
	GLuint Divisor;
	uint8 bNormalized;
	uint8 AttributeIndex;
	uint8 bShouldConvertToFloat;
	uint8 Padding;

	FOpenGLVertexElement()
		: Padding(0)
	{
	}
};

/** Convenience typedef: preallocated array of OpenGL input element descriptions. */
typedef TArray<FOpenGLVertexElement,TFixedAllocator<MaxVertexElementCount> > FOpenGLVertexElements;

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FOpenGLVertexDeclaration : public FRHIVertexDeclaration
{
public:
	/** Elements of the vertex declaration. */
	FOpenGLVertexElements VertexElements;

	uint16 StreamStrides[MaxVertexElementCount];

	/** Initialization constructor. */
	FOpenGLVertexDeclaration(const FOpenGLVertexElements& InElements, const uint16* InStrides)
		: VertexElements(InElements)
	{
		FMemory::Memcpy(StreamStrides, InStrides, sizeof(StreamStrides));
	}
};


/**
 * Combined shader state and vertex definition for rendering geometry.
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FOpenGLBoundShaderState : public FRHIBoundShaderState
{
public:

	FCachedBoundShaderStateLink CacheLink;

	uint16 StreamStrides[MaxVertexElementCount];

	FOpenGLLinkedProgram* LinkedProgram;
	TRefCountPtr<FOpenGLVertexDeclaration> VertexDeclaration;
	TRefCountPtr<FOpenGLVertexShader> VertexShader;
	TRefCountPtr<FOpenGLPixelShader> PixelShader;
	TRefCountPtr<FOpenGLGeometryShader> GeometryShader;
	TRefCountPtr<FOpenGLHullShader> HullShader;
	TRefCountPtr<FOpenGLDomainShader> DomainShader;

	/** Initialization constructor. */
	FOpenGLBoundShaderState(
		FOpenGLLinkedProgram* InLinkedProgram,
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI
		);

	bool NeedsTextureStage(int32 TextureStageIndex);
	int32 MaxTextureStageUsed();
	bool RequiresDriverInstantiation();

	virtual ~FOpenGLBoundShaderState();
};


inline GLenum GetOpenGLTargetFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return GL_NONE;
	}
	else if(Texture->GetTexture2D())
	{
		return GL_TEXTURE_2D;
	}
	else if(Texture->GetTexture2DArray())
	{
		return GL_TEXTURE_2D_ARRAY;
	}
	else if(Texture->GetTexture3D())
	{
		return GL_TEXTURE_3D;
	}
	else if(Texture->GetTextureCube())
	{
		return GL_TEXTURE_CUBE_MAP;
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Unknown RHI texture type"));
		return GL_NONE;
	}
}


class OPENGLDRV_API FOpenGLTextureBase
{
protected:
	class FOpenGLDynamicRHI* OpenGLRHI;

public:
	// Pointer to current sampler state in this unit
	class FOpenGLSamplerState* SamplerState;

	/** The OpenGL texture resource. */
	GLuint Resource;

	/** The OpenGL texture target. */
	GLenum Target;

	/** The number of mips in the texture. */
	uint32 NumMips;

	/** The OpenGL attachment point. This should always be GL_COLOR_ATTACHMENT0 in case of color buffer, but the actual texture may be attached on other color attachments. */
	GLenum Attachment;

	/** OpenGL 3 Stencil/SRV workaround texture resource */
	GLuint SRVResource;

	/** Initialization constructor. */
	FOpenGLTextureBase(
		FOpenGLDynamicRHI* InOpenGLRHI,
		GLuint InResource,
		GLenum InTarget,
		uint32 InNumMips,
		GLenum InAttachment
		)
	: OpenGLRHI(InOpenGLRHI)
	, SamplerState(nullptr)
	, Resource(InResource)
	, Target(InTarget)
	, NumMips(InNumMips)
	, Attachment(InAttachment)
	, SRVResource( 0 )
	, MemorySize( 0 )
	, bIsPowerOfTwo(false)
	{}

	int32 GetMemorySize() const
	{
		return MemorySize;
	}

	void SetMemorySize(uint32 InMemorySize)
	{
		MemorySize = InMemorySize;
	}

	void SetIsPowerOfTwo(bool bInIsPowerOfTwo)
	{
		bIsPowerOfTwo  = bInIsPowerOfTwo ? 1 : 0;
	}

	bool IsPowerOfTwo() const
	{
		return bIsPowerOfTwo != 0;
	}

#if PLATFORM_ANDROIDESDEFERRED // Flithy hack to workaround radr://16011763
	GLuint GetOpenGLFramebuffer(uint32 ArrayIndices, uint32 MipmapLevels);
#endif

	void InvalidateTextureResourceInCache();

	void AliasResources(FOpenGLTextureBase* Texture)
	{
		Resource = Texture->Resource;
		SRVResource = Texture->SRVResource;
	}

private:
	uint32 MemorySize		: 31;
	uint32 bIsPowerOfTwo	: 1;
};

// Textures.
template<typename BaseType>
class OPENGLDRV_API TOpenGLTexture : public BaseType, public FOpenGLTextureBase
{
public:

	/** Initialization constructor. */
	TOpenGLTexture(
		class FOpenGLDynamicRHI* InOpenGLRHI,
		GLuint InResource,
		GLenum InTarget,
		GLenum InAttachment,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		uint32 InNumSamplesTileMem, /* For render targets on Android tiled GPUs, the number of samples to use internally */
		uint32 InArraySize,
		EPixelFormat InFormat,
		bool bInCubemap,
		bool bInAllocatedStorage,
		uint32 InFlags,
		uint8* InTextureRange,
		const FClearValueBinding& InClearValue
		)
	: BaseType(InSizeX,InSizeY,InSizeZ,InNumMips,InNumSamples, InNumSamplesTileMem, InArraySize, InFormat,InFlags, InClearValue)
	, FOpenGLTextureBase(
		InOpenGLRHI,
		InResource,
		InTarget,
		InNumMips,
		InAttachment
		)
	, TextureRange(InTextureRange)
	, BaseLevel(0)
	, bCubemap(bInCubemap)
	{
		PixelBuffers.AddZeroed(this->GetNumMips() * (bCubemap ? 6 : 1) * GetEffectiveSizeZ());
		bAllocatedStorage.Init(bInAllocatedStorage, this->GetNumMips() * (bCubemap ? 6 : 1));
		ClientStorageBuffers.AddZeroed(this->GetNumMips() * (bCubemap ? 6 : 1) * GetEffectiveSizeZ());

		FShaderCache* ShaderCache = FShaderCache::GetShaderCache();
		if ( ShaderCache )
		{
			FShaderTextureKey Tex;
			Tex.Format = (EPixelFormat)InFormat;
			Tex.Flags = InFlags;
			Tex.MipLevels = InNumMips;
			Tex.Samples = InNumSamples;
			Tex.X = InSizeX;
			Tex.Y = InSizeY;
			Tex.Z = InSizeZ;
			switch(InTarget)
			{
				case GL_TEXTURE_2D:
				case GL_TEXTURE_2D_MULTISAMPLE:
				{
					Tex.Type = SCTT_Texture2D;
					break;
				}
				case GL_TEXTURE_3D:
				{
					Tex.Type = SCTT_Texture3D;
					break;
				}
				case GL_TEXTURE_CUBE_MAP:
				{
					Tex.Type = SCTT_TextureCube;
					break;
				}
				case GL_TEXTURE_2D_ARRAY:
				{
					Tex.Type = SCTT_Texture2DArray;
					break;
				}
				case GL_TEXTURE_CUBE_MAP_ARRAY:
				{
					Tex.Type = SCTT_TextureCubeArray;
					Tex.Z = InArraySize;
					break;
				}
#if PLATFORM_ANDROID
				case GL_TEXTURE_EXTERNAL_OES:
				{
					Tex.Type = SCTT_TextureExternal2D;
					break;
				}
#endif
				default:
				{
					Tex.Type = SCTT_Invalid;
				}
			}

			if (Tex.Type != SCTT_Invalid)
			{
				FShaderCache::LogTexture(Tex, this);
			}
		}
	}

	virtual ~TOpenGLTexture()
	{
		if (GIsRHIInitialized)
		{
			VERIFY_GL_SCOPE();

			OpenGLTextureDeleted(this);

			if (Resource != 0)
			{
				switch (Target)
				{
					case GL_TEXTURE_2D:
					case GL_TEXTURE_2D_MULTISAMPLE:
					case GL_TEXTURE_3D:
					case GL_TEXTURE_CUBE_MAP:
					case GL_TEXTURE_2D_ARRAY:
					case GL_TEXTURE_CUBE_MAP_ARRAY:
#if PLATFORM_ANDROID
					case GL_TEXTURE_EXTERNAL_OES:
#endif
					{
						InvalidateTextureResourceInCache();
						FOpenGL::DeleteTextures(1, &Resource);
						if (SRVResource)
						{
							FOpenGL::DeleteTextures(1, &SRVResource);
						}
						break;
					}
					case GL_RENDERBUFFER:
					{
						if (!(this->GetFlags() & TexCreate_Presentable))
						{
							glDeleteRenderbuffers(1, &Resource);
						}
						break;
					}
					default:
					{
						checkNoEntry();
					}
				}
			}

			if (TextureRange)
			{
				delete[] TextureRange;
				TextureRange = nullptr;
			}

			ReleaseOpenGLFramebuffers(OpenGLRHI, this);
		}
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return static_cast<FOpenGLTextureBase*>(this);
	}

	/**
	 * Locks one of the texture's mip-maps.
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex,uint32 ArrayIndex,EResourceLockMode LockMode,uint32& DestStride);

	/** Unlocks a previously locked mip-map. */
	void Unlock(uint32 MipIndex,uint32 ArrayIndex);

	/** Updates the host accessible version of the texture */
	void UpdateHost(uint32 MipIndex,uint32 ArrayIndex);

	/** Get PBO Resource for readback */
	GLuint GetBufferResource(uint32 MipIndex,uint32 ArrayIndex);

	// Accessors.
	bool IsDynamic() const { return (this->GetFlags() & TexCreate_Dynamic) != 0; }
	bool IsCubemap() const { return bCubemap != 0; }
	bool IsStaging() const { return (this->GetFlags() & TexCreate_CPUReadback) != 0; }


	/** FRHITexture override.  See FRHITexture::GetNativeResource() */
	virtual void* GetNativeResource() const override
	{
		return const_cast<void *>(reinterpret_cast<const void*>(&Resource));
	}

	/**
	 * Accessors to mark whether or not we have allocated storage for each mip/face.
	 * For non-cubemaps FaceIndex should always be zero.
	 */
	bool GetAllocatedStorageForMip(uint32 MipIndex, uint32 FaceIndex) const
	{
		return bAllocatedStorage[MipIndex * (bCubemap ? 6 : 1) + FaceIndex];
	}
	void SetAllocatedStorageForMip(uint32 MipIndex, uint32 FaceIndex)
	{
		bAllocatedStorage[MipIndex * (bCubemap ? 6 : 1) + FaceIndex] = true;
	}

	/**
	 * Clone texture from a source using CopyImageSubData
	 */
	void CloneViaCopyImage( TOpenGLTexture* Src, uint32 InNumMips, int32 SrcOffset, int32 DstOffset);

	/**
	 * Clone texture from a source going via PBOs
	 */
	void CloneViaPBO( TOpenGLTexture* Src, uint32 InNumMips, int32 SrcOffset, int32 DstOffset);

	/**
	 * Resolved the specified face for a read Lock, for non-renderable, CPU readable surfaces this eliminates the readback inside Lock itself.
	 */
	void Resolve(uint32 MipIndex,uint32 ArrayIndex);
private:

	TArray< TRefCountPtr<FOpenGLPixelBuffer> > PixelBuffers;

	/** Backing store for all client storage buffers for platforms and textures types where this is faster than PBOs */
	uint8* TextureRange;

	/** Client storage buffers for platforms and textures types where this is faster than PBOs */
	struct FOpenGLClientStore
	{
		uint8* Data;
		uint32 Size;
		bool bReadOnly;
	};
	TArray<FOpenGLClientStore> ClientStorageBuffers;

	uint32 GetEffectiveSizeZ( void ) { return this->GetSizeZ() ? this->GetSizeZ() : 1; }

	/** Index of the largest mip-map in the texture */
	uint32 BaseLevel;

	/** Bitfields marking whether we have allocated storage for each mip */
	TBitArray<TInlineAllocator<1> > bAllocatedStorage;

	/** Whether the texture is a cube-map. */
	const uint32 bCubemap : 1;
};

class OPENGLDRV_API FOpenGLBaseTexture2D : public FRHITexture2D
{
public:
	FOpenGLBaseTexture2D(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, uint32 InArraySize, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITexture2D(InSizeX,InSizeY,InNumMips,InNumSamples,InFormat,InFlags, InClearValue)
	, SampleCount(InNumSamples)
	, SampleCountTileMem(InNumSamplesTileMem)
	{}
	uint32 GetSizeZ() const { return 0; }
	uint32 GetNumSamples() const { return SampleCount; }
	uint32 GetNumSamplesTileMem() const { return SampleCountTileMem; }
private:
	uint32 SampleCount;
	/* For render targets on Android tiled GPUs, the number of samples to use internally */
	uint32 SampleCountTileMem;
};

class FOpenGLBaseTexture2DArray : public FRHITexture2DArray
{
public:
	FOpenGLBaseTexture2DArray(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, uint32 InArraySize, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITexture2DArray(InSizeX,InSizeY,InSizeZ,InNumMips,InFormat,InFlags, InClearValue)
	{
		check(InNumSamples == 1);	// OpenGL supports multisampled texture arrays, but they're currently not implemented in OpenGLDrv.
		check(InNumSamplesTileMem == 1);
	}
};

class FOpenGLBaseTextureCube : public FRHITextureCube
{
public:
	FOpenGLBaseTextureCube(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, uint32 InArraySize, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITextureCube(InSizeX,InNumMips,InFormat,InFlags,InClearValue)
	, ArraySize(InArraySize)
	{
		check(InNumSamples == 1);	// OpenGL doesn't currently support multisampled cube textures
		check(InNumSamplesTileMem == 1);
	}
	uint32 GetSizeX() const { return GetSize(); }
	uint32 GetSizeY() const { return GetSize(); } //-V524
	uint32 GetSizeZ() const { return ArraySize > 1 ? ArraySize : 0; }

	uint32 GetArraySize() const {return ArraySize;}
private:
	uint32 ArraySize;
};

class FOpenGLBaseTexture3D : public FRHITexture3D
{
public:
	FOpenGLBaseTexture3D(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, uint32 InArraySize, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITexture3D(InSizeX,InSizeY,InSizeZ,InNumMips,InFormat,InFlags,InClearValue)
	{
		check(InNumSamples == 1);	// Can't have multisampled texture 3D. Not supported anywhere.
		check(InNumSamplesTileMem == 1);
	}
};

typedef TOpenGLTexture<FRHITexture>						FOpenGLTexture;
typedef TOpenGLTexture<FOpenGLBaseTexture2D>			FOpenGLTexture2D;
typedef TOpenGLTexture<FOpenGLBaseTexture2DArray>		FOpenGLTexture2DArray;
typedef TOpenGLTexture<FOpenGLBaseTexture3D>			FOpenGLTexture3D;
typedef TOpenGLTexture<FOpenGLBaseTextureCube>			FOpenGLTextureCube;

class FOpenGLTextureReference : public FRHITextureReference
{
	FOpenGLTextureBase* TexturePtr;

public:
	explicit FOpenGLTextureReference(FLastRenderTimeContainer* InLastRenderTime)
		: FRHITextureReference(InLastRenderTime)
		, TexturePtr(NULL)
	{}

	void SetReferencedTexture(FRHITexture* InTexture);
	FOpenGLTextureBase* GetTexturePtr() const { return TexturePtr; }

	virtual void* GetTextureBaseRHI() override final
	{
		return TexturePtr;
	}
};

/** Given a pointer to a RHI texture that was created by the OpenGL RHI, returns a pointer to the FOpenGLTextureBase it encapsulates. */
inline FOpenGLTextureBase* GetOpenGLTextureFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return NULL;
	}
	else
	{
		return static_cast<FOpenGLTextureBase*>(Texture->GetTextureBaseRHI());
	}
}

inline uint32 GetOpenGLTextureSizeXFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return 0;
	}
	else if(Texture->GetTexture2D())
	{
		return ((FOpenGLTexture2D*)Texture)->GetSizeX();
	}
	else if(Texture->GetTexture2DArray())
	{
		return ((FOpenGLTexture2DArray*)Texture)->GetSizeX();
	}
	else if(Texture->GetTexture3D())
	{
		return ((FOpenGLTexture3D*)Texture)->GetSizeX();
	}
	else if(Texture->GetTextureCube())
	{
		return ((FOpenGLTextureCube*)Texture)->GetSize();
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Unknown RHI texture type"));
		return 0;
	}
}

inline uint32 GetOpenGLTextureSizeYFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return 0;
	}
	else if(Texture->GetTexture2D())
	{
		return ((FOpenGLTexture2D*)Texture)->GetSizeY();
	}
	else if(Texture->GetTexture2DArray())
	{
		return ((FOpenGLTexture2DArray*)Texture)->GetSizeY();
	}
	else if(Texture->GetTexture3D())
	{
		return ((FOpenGLTexture3D*)Texture)->GetSizeY();
	}
	else if(Texture->GetTextureCube())
	{
		return ((FOpenGLTextureCube*)Texture)->GetSize();
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Unknown RHI texture type"));
		return 0;
	}
}

inline uint32 GetOpenGLTextureSizeZFromRHITexture(FRHITexture* Texture)
{
	if(!Texture)
	{
		return 0;
	}
	else if(Texture->GetTexture2D())
	{
		return 0;
	}
	else if(Texture->GetTexture2DArray())
	{
		return ((FOpenGLTexture2DArray*)Texture)->GetSizeZ();
	}
	else if(Texture->GetTexture3D())
	{
		return ((FOpenGLTexture3D*)Texture)->GetSizeZ();
	}
	else if(Texture->GetTextureCube())
	{
		return ((FOpenGLTextureCube*)Texture)->GetSizeZ();
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Unknown RHI texture type"));
		return 0;
	}
}

class FOpenGLRenderQuery : public FRHIRenderQuery
{
public:

	/** The query resource. */
	GLuint Resource;

	/** Identifier of the OpenGL context the query is a part of. */
	uint64 ResourceContext;

	/** The cached query result. */
	GLuint64 Result;

	/** true if the query's result is cached. */
	bool bResultIsCached : 1;

	/** true if the context the query is in was released from another thread */
	bool bInvalidResource : 1;

	// todo: memory optimize
	ERenderQueryType QueryType;

	FOpenGLRenderQuery(ERenderQueryType InQueryType);
	FOpenGLRenderQuery(FOpenGLRenderQuery const& OtherQuery);
	virtual ~FOpenGLRenderQuery();
	FOpenGLRenderQuery& operator=(FOpenGLRenderQuery const& OtherQuery);
};

class FOpenGLUnorderedAccessView : public FRHIUnorderedAccessView
{

public:
	FOpenGLUnorderedAccessView():
		Resource(0),
		BufferResource(0),
		Format(0)
	{

	}

	GLuint	Resource;
	GLuint	BufferResource;
	GLenum	Format;

	virtual uint32 GetBufferSize()
	{
		return 0;
	}
};

class FOpenGLTextureUnorderedAccessView : public FOpenGLUnorderedAccessView
{
public:

	FOpenGLTextureUnorderedAccessView(FTextureRHIParamRef InTexture);

	FTextureRHIRef TextureRHI; // to keep the texture alive
};


class FOpenGLVertexBufferUnorderedAccessView : public FOpenGLUnorderedAccessView
{
public:

	FOpenGLVertexBufferUnorderedAccessView();

	FOpenGLVertexBufferUnorderedAccessView(	FOpenGLDynamicRHI* InOpenGLRHI, FVertexBufferRHIParamRef InVertexBuffer, uint8 Format);

	virtual ~FOpenGLVertexBufferUnorderedAccessView();

	FVertexBufferRHIRef VertexBufferRHI; // to keep the vertex buffer alive

	FOpenGLDynamicRHI* OpenGLRHI;

	virtual uint32 GetBufferSize() override;
};

class FOpenGLShaderResourceView : public FRHIShaderResourceView
{
	// In OpenGL 3.2, the only view that actually works is a Buffer<type> kind of view from D3D10,
	// and it's mapped to OpenGL's buffer texture.

public:

	/** OpenGL texture the buffer is bound with */
	GLuint Resource;
	GLenum Target;

	/** Needed on GL <= 4.2 to copy stencil data out of combined depth-stencil surfaces. */
	FTexture2DRHIRef Texture2D;

	int32 LimitMip;

	/** Needed on OS X to force a rebind of the texture buffer to the texture name to workaround radr://18379338 */
	FVertexBufferRHIRef VertexBuffer;
	uint64 ModificationVersion;
	uint8 Format;

	FOpenGLShaderResourceView( FOpenGLDynamicRHI* InOpenGLRHI, GLuint InResource, GLenum InTarget )
	:	Resource(InResource)
	,	Target(InTarget)
	,	LimitMip(-1)
	,	ModificationVersion(0)
	,	Format(0)
	,	OpenGLRHI(InOpenGLRHI)
	,	OwnsResource(true)
	{}

	FOpenGLShaderResourceView( FOpenGLDynamicRHI* InOpenGLRHI, GLuint InResource, GLenum InTarget, FVertexBufferRHIParamRef InVertexBuffer, uint8 InFormat )
	:	Resource(InResource)
	,	Target(InTarget)
	,	LimitMip(-1)
	,	VertexBuffer(InVertexBuffer)
	,	ModificationVersion(0)
	,	Format(InFormat)
	,	OpenGLRHI(InOpenGLRHI)
	,	OwnsResource(true)
	{
		check(IsValidRef(VertexBuffer));
		FOpenGLVertexBuffer* VB = (FOpenGLVertexBuffer*)VertexBuffer.GetReference();
		ModificationVersion = VB->ModificationCount;
	}

	FOpenGLShaderResourceView( FOpenGLDynamicRHI* InOpenGLRHI, GLuint InResource, GLenum InTarget, GLuint Mip, bool InOwnsResource )
	:	Resource(InResource)
	,	Target(InTarget)
	,	LimitMip(Mip)
	,	ModificationVersion(0)
	,	Format(0)
	,	OpenGLRHI(InOpenGLRHI)
	,	OwnsResource(InOwnsResource)
	{}

	virtual ~FOpenGLShaderResourceView( void );

protected:
	FOpenGLDynamicRHI* OpenGLRHI;
	bool OwnsResource;
};

void OPENGLDRV_API OpenGLTextureDeleted(FRHITexture* Texture);
void OPENGLDRV_API OpenGLTextureAllocated( FRHITexture* Texture , uint32 Flags);

void OPENGLDRV_API ReleaseOpenGLFramebuffers(FOpenGLDynamicRHI* Device, FTextureRHIParamRef TextureRHI);

/** A OpenGL event query resource. */
class FOpenGLEventQuery : public FRenderResource
{
public:

	/** Initialization constructor. */
	FOpenGLEventQuery(class FOpenGLDynamicRHI* InOpenGLRHI)
		: OpenGLRHI(InOpenGLRHI)
		, Sync(UGLsync())
	{
	}

	/** Issues an event for the query to poll. */
	void IssueEvent();

	/** Waits for the event query to finish. */
	void WaitForCompletion();

	// FRenderResource interface.
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

private:
	FOpenGLDynamicRHI* OpenGLRHI;
	UGLsync Sync;
};

class FOpenGLViewport : public FRHIViewport
{
public:

	FOpenGLViewport(class FOpenGLDynamicRHI* InOpenGLRHI,void* InWindowHandle,uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen,EPixelFormat PreferredPixelFormat);
	~FOpenGLViewport();

	void Resize(uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen);

	// Accessors.
	FIntPoint GetSizeXY() const { return FIntPoint(SizeX, SizeY); }
	FOpenGLTexture2D *GetBackBuffer() const { return BackBuffer; }
	bool IsFullscreen( void ) const { return bIsFullscreen; }

	void WaitForFrameEventCompletion()
	{
		FrameSyncEvent.WaitForCompletion();
	}

	void IssueFrameEvent()
	{
		FrameSyncEvent.IssueEvent();
	}

	virtual void* GetNativeWindow(void** AddParam) const override;

	struct FPlatformOpenGLContext* GetGLContext() const { return OpenGLContext; }
	FOpenGLDynamicRHI* GetOpenGLRHI() const { return OpenGLRHI; }

	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override
	{
		CustomPresent = InCustomPresent;
	}
	FRHICustomPresent* GetCustomPresent() const { return CustomPresent.GetReference(); }
private:

	friend class FOpenGLDynamicRHI;

	FOpenGLDynamicRHI* OpenGLRHI;
	struct FPlatformOpenGLContext* OpenGLContext;
	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	EPixelFormat PixelFormat;
	bool bIsValid;
	TRefCountPtr<FOpenGLTexture2D> BackBuffer;
	FOpenGLEventQuery FrameSyncEvent;
	FCustomPresentRHIRef CustomPresent;
};

template<class T>
struct TOpenGLResourceTraits
{
};
template<>
struct TOpenGLResourceTraits<FRHIVertexDeclaration>
{
	typedef FOpenGLVertexDeclaration TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIVertexShader>
{
	typedef FOpenGLVertexShader TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIGeometryShader>
{
	typedef FOpenGLGeometryShader TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIHullShader>
{
	typedef FOpenGLHullShader TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIDomainShader>
{
	typedef FOpenGLDomainShader TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIPixelShader>
{
	typedef FOpenGLPixelShader TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIComputeShader>
{
	typedef FOpenGLComputeShader TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIBoundShaderState>
{
	typedef FOpenGLBoundShaderState TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHITexture3D>
{
	typedef FOpenGLTexture3D TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHITexture>
{
	typedef FOpenGLTexture TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHITexture2D>
{
	typedef FOpenGLTexture2D TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHITexture2DArray>
{
	typedef FOpenGLTexture2DArray TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHITextureCube>
{
	typedef FOpenGLTextureCube TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIRenderQuery>
{
	typedef FOpenGLRenderQuery TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIUniformBuffer>
{
	typedef FOpenGLUniformBuffer TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIIndexBuffer>
{
	typedef FOpenGLIndexBuffer TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIStructuredBuffer>
{
	typedef FOpenGLStructuredBuffer TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIVertexBuffer>
{
	typedef FOpenGLVertexBuffer TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIShaderResourceView>
{
	typedef FOpenGLShaderResourceView TConcreteType;
};
template<>
struct TOpenGLResourceTraits<FRHIUnorderedAccessView>
{
	typedef FOpenGLUnorderedAccessView TConcreteType;
};

template<>
struct TOpenGLResourceTraits<FRHIViewport>
{
	typedef FOpenGLViewport TConcreteType;
};
