// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLVertexDeclaration.cpp: OpenGL vertex declaration RHI implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "ShaderCache.h"
#include "OpenGLDrv.h"

static FORCEINLINE void SetupGLElement(FOpenGLVertexElement& GLElement, GLenum Type, GLuint Size, bool bNormalized, bool bShouldConvertToFloat)
{
	GLElement.Type = Type;
	GLElement.Size = Size;
	GLElement.bNormalized = bNormalized;
	GLElement.bShouldConvertToFloat = bShouldConvertToFloat;
}

/**
 * Key used to look up vertex declarations in the cache.
 */
struct FOpenGLVertexDeclarationKey
{
	/** Vertex elements in the declaration. */
	FOpenGLVertexElements VertexElements;
	/** Hash of the vertex elements. */
	uint32 Hash;

	uint16 StreamStrides[MaxVertexElementCount];

	/** Initialization constructor. */
	explicit FOpenGLVertexDeclarationKey(const FVertexDeclarationElementList& InElements)
	{
		uint16 UsedStreamsMask = 0;
		FMemory::Memzero(StreamStrides);

		for(int32 ElementIndex = 0;ElementIndex < InElements.Num();ElementIndex++)
		{
			const FVertexElement& Element = InElements[ElementIndex];
			FOpenGLVertexElement GLElement;
			GLElement.StreamIndex = Element.StreamIndex;
			GLElement.Offset = Element.Offset;
			GLElement.Divisor = Element.bUseInstanceIndex ? 1 : 0;
			GLElement.AttributeIndex = Element.AttributeIndex;
			GLElement.Padding = 0;
			switch(Element.Type)
			{
				case VET_Float1:		SetupGLElement(GLElement, GL_FLOAT,			1,			false,	true); break;
				case VET_Float2:		SetupGLElement(GLElement, GL_FLOAT,			2,			false,	true); break;
				case VET_Float3:		SetupGLElement(GLElement, GL_FLOAT,			3,			false,	true); break;
				case VET_Float4:		SetupGLElement(GLElement, GL_FLOAT,			4,			false,	true); break;
				case VET_PackedNormal:	SetupGLElement(GLElement, GL_UNSIGNED_BYTE,	4,			true,	true); break;
				case VET_UByte4:		SetupGLElement(GLElement, GL_UNSIGNED_BYTE,	4,			false,	false); break;
				case VET_UByte4N:		SetupGLElement(GLElement, GL_UNSIGNED_BYTE,	4,			true,	true); break;
				case VET_Color:	
					if (FOpenGL::SupportsVertexArrayBGRA())
					{
						SetupGLElement(GLElement, GL_UNSIGNED_BYTE,	GL_BGRA,	true,	true);
					}
					else
					{
						SetupGLElement(GLElement, GL_UNSIGNED_BYTE,	4,	true,	true);
					}
					break;
				case VET_Short2:		SetupGLElement(GLElement, GL_SHORT,			2,			false,	false); break;
				case VET_Short4:		SetupGLElement(GLElement, GL_SHORT,			4,			false,	false); break;
				case VET_Short2N:		SetupGLElement(GLElement, GL_SHORT,			2,			true,	true); break;
				case VET_Half2:
					if (FOpenGL::SupportsVertexHalfFloat())
					{
						SetupGLElement(GLElement, FOpenGL::GetVertexHalfFloatFormat(), 2, false, true);
					}
					else
					{
						// @todo-mobile: Use shorts?
						SetupGLElement(GLElement, GL_SHORT, 2, false, true);
					}
					break;
				case VET_Half4:
					if (FOpenGL::SupportsVertexHalfFloat())
					{
						SetupGLElement(GLElement, FOpenGL::GetVertexHalfFloatFormat(), 4, false, true);
					}
					else
					{
						// @todo-mobile: Use shorts?
						SetupGLElement(GLElement, GL_SHORT, 4, false, true);
					}
					break;
				case VET_Short4N:		SetupGLElement(GLElement, GL_SHORT,			4,			true,	true); break;
				case VET_UShort2:		SetupGLElement(GLElement, GL_UNSIGNED_SHORT, 2, false, false); break;
				case VET_UShort4:		SetupGLElement(GLElement, GL_UNSIGNED_SHORT, 4, false, false); break;
				case VET_UShort2N:		SetupGLElement(GLElement, GL_UNSIGNED_SHORT, 2, true, true); break;
				case VET_UShort4N:		SetupGLElement(GLElement, GL_UNSIGNED_SHORT, 4, true, true); break;
				case VET_URGB10A2N:		SetupGLElement(GLElement, GL_UNSIGNED_INT_2_10_10_10_REV, 4, true, true); break;
				default: UE_LOG(LogRHI, Fatal,TEXT("Unknown RHI vertex element type %u"),(uint8)InElements[ElementIndex].Type);
			};

			if ((UsedStreamsMask & 1 << Element.StreamIndex) != 0)
			{
				ensure(StreamStrides[Element.StreamIndex] == Element.Stride);
			}
			else
			{
				UsedStreamsMask = UsedStreamsMask | (1 << Element.StreamIndex);
				StreamStrides[Element.StreamIndex] = Element.Stride;
			}

			VertexElements.Add(GLElement);
		}

		struct FCompareFOpenGLVertexElement
		{
			FORCEINLINE bool operator()( const FOpenGLVertexElement& A, const FOpenGLVertexElement& B ) const
			{
				return ((int32)A.Offset + A.StreamIndex * MAX_uint16) < ((int32)B.Offset + B.StreamIndex * MAX_uint16);
			}
		};
		// Sort the FOpenGLVertexElements by stream then offset.
		Sort( VertexElements.GetData(), VertexElements.Num(), FCompareFOpenGLVertexElement() );

		Hash = FCrc::MemCrc_DEPRECATED(VertexElements.GetData(),VertexElements.Num()*sizeof(FOpenGLVertexElement));
		Hash = FCrc::MemCrc_DEPRECATED(StreamStrides, sizeof(StreamStrides), Hash);
	}
};

/** Hashes the array of OpenGL vertex element descriptions. */
uint32 GetTypeHash(const FOpenGLVertexDeclarationKey& Key)
{
	return Key.Hash;
}

/** Compare two vertex element descriptions. */
bool operator==(const FOpenGLVertexElement& A, const FOpenGLVertexElement& B)
{
	return A.Type == B.Type && A.StreamIndex == B.StreamIndex && A.Offset == B.Offset && A.Size == B.Size
		&& A.Divisor == B.Divisor && A.bNormalized == B.bNormalized && A.AttributeIndex == B.AttributeIndex
		&& A.bShouldConvertToFloat == B.bShouldConvertToFloat;
}

/** Compare two vertex declaration keys. */
bool operator==(const FOpenGLVertexDeclarationKey& A, const FOpenGLVertexDeclarationKey& B)
{
	return A.VertexElements == B.VertexElements;
}

/** Global cache of vertex declarations. */
TMap<FOpenGLVertexDeclarationKey,FVertexDeclarationRHIRef> GOpenGLVertexDeclarationCache;

FVertexDeclarationRHIRef FOpenGLDynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	// Construct a key from the elements.
	FOpenGLVertexDeclarationKey Key(Elements);

	// Check for a cached vertex declaration.
	FVertexDeclarationRHIRef* VertexDeclarationRefPtr = GOpenGLVertexDeclarationCache.Find(Key);
	if (VertexDeclarationRefPtr == NULL)
	{
		// Create and add to the cache if it doesn't exist.
		VertexDeclarationRefPtr = &GOpenGLVertexDeclarationCache.Add(Key,new FOpenGLVertexDeclaration(Key.VertexElements, Key.StreamStrides));
		
		check(VertexDeclarationRefPtr);
		check(IsValidRef(*VertexDeclarationRefPtr));
		FShaderCache::LogVertexDeclaration(FShaderCache::GetDefaultCacheState(), Elements, *VertexDeclarationRefPtr);
	}

	// The cached declaration must match the input declaration!
	check(VertexDeclarationRefPtr);
	check(IsValidRef(*VertexDeclarationRefPtr));
	FOpenGLVertexDeclaration* OpenGLVertexDeclaration = (FOpenGLVertexDeclaration*)VertexDeclarationRefPtr->GetReference();
	checkSlow(OpenGLVertexDeclaration->VertexElements == Key.VertexElements);

	return *VertexDeclarationRefPtr;
}
