// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Rendering/ColorVertexBuffer.h"
#include "CoreMinimal.h"
#include "RHI.h"
#include "Components.h"
#include "EngineUtils.h"
#include "StaticMeshVertexData.h"

/*-----------------------------------------------------------------------------
	FColorVertexBuffer
-----------------------------------------------------------------------------*/

/** The implementation of the static mesh color-only vertex data storage type. */
class FColorVertexData :
	public TStaticMeshVertexData<FColor>
{
public:
	FColorVertexData( bool InNeedsCPUAccess=false )
		: TStaticMeshVertexData<FColor>( InNeedsCPUAccess )
	{
	}

	/**
	* Assignment operator. This is currently the only method which allows for 
	* modifying an existing resource array
	*/
	TStaticMeshVertexData<FColor>& operator=(const TArray<FColor>& Other)
	{
		TStaticMeshVertexData<FColor>::operator=(Other);
		return *this;
	}
};


FColorVertexBuffer::FColorVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{
}

FColorVertexBuffer::FColorVertexBuffer( const FColorVertexBuffer &rhs ):
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{
	if (rhs.VertexData)
	{
		InitFromColorArray(TArray<FColor>(*rhs.VertexData));
	}
	else
	{
		CleanUp();
	}
}

FColorVertexBuffer::~FColorVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FColorVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

/**
 * Initializes the buffer with the given vertices, used to convert legacy layouts.
 * @param InVertices - The vertices to initialize the buffer with.
 */
void FColorVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices)
{
	// First, make sure that there is at least one non-default vertex color in the original data.
	const int32 InVertexCount = InVertices.Num();
	bool bAllColorsAreOpaqueWhite = true;
	bool bAllColorsAreEqual = true;

	if( InVertexCount > 0 )
	{
		const FColor FirstColor = InVertices[ 0 ].Color;

		for( int32 CurVertexIndex = 0; CurVertexIndex < InVertexCount; ++CurVertexIndex )
		{
			const FColor CurColor = InVertices[ CurVertexIndex ].Color;

			if( CurColor.R != 255 || CurColor.G != 255 || CurColor.B != 255 || CurColor.A != 255 )
			{
				bAllColorsAreOpaqueWhite = false;
			}

			if( CurColor.R != FirstColor.R || CurColor.G != FirstColor.G || CurColor.B != FirstColor.B || CurColor.A != FirstColor.A )
			{
				bAllColorsAreEqual = false;
			}

			if( !bAllColorsAreEqual && !bAllColorsAreOpaqueWhite )
			{
				break;
			}
		}
	}

	if( bAllColorsAreOpaqueWhite )
	{
		// Ensure no vertex data is allocated.
		CleanUp();

		// Clear the vertex count and stride.
		Stride = 0;
		NumVertices = 0;
	}
	else
	{
		NumVertices = InVertexCount;

		// Allocate the vertex data storage type.
		AllocateData();

		// Allocate the vertex data buffer.
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();

		// Copy the vertices into the buffer.
		for(int32 VertexIndex = 0;VertexIndex < InVertices.Num();VertexIndex++)
		{
			const FStaticMeshBuildVertex& SourceVertex = InVertices[VertexIndex];
			const uint32 DestVertexIndex = VertexIndex;
			VertexColor(DestVertexIndex) = SourceVertex.Color;
		}
	}
}

/**
 * Initializes this vertex buffer with the contents of the given vertex buffer.
 * @param InVertexBuffer - The vertex buffer to initialize from.
 */
void FColorVertexBuffer::Init(const FColorVertexBuffer& InVertexBuffer)
{
	NumVertices = InVertexBuffer.GetNumVertices();
	if ( NumVertices )
	{
		AllocateData();
		check( Stride == InVertexBuffer.GetStride() );
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		const uint8* InData = InVertexBuffer.Data;
		FMemory::Memcpy( Data, InData, Stride * NumVertices );
	}
}

/**
* Removes the cloned vertices used for extruding shadow volumes.
* @param NumVertices - The real number of static mesh vertices which should remain in the buffer upon return.
*/
void FColorVertexBuffer::RemoveLegacyShadowVolumeVertices(uint32 InNumVertices)
{
	if( VertexData != NULL )
	{
		VertexData->ResizeBuffer(InNumVertices);
		NumVertices = InNumVertices;

		// Make a copy of the vertex data pointer.
		Data = VertexData->GetDataPointer();
	}
}

/**
* Serializer
*
* @param	Ar				Archive to serialize with
* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
*/
void FColorVertexBuffer::Serialize( FArchive& Ar, bool bNeedsCPUAccess )
{
	FStripDataFlags StripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);

	if (Ar.IsSaving() && NumVertices > 0 && VertexData == NULL)
	{
		// ...serialize as if the vertex count were zero. Else on load serialization breaks.
		// This situation should never occur because VertexData should not be null if NumVertices
		// is greater than zero. So really this should be a checkf but I don't want to crash
		// the Editor when saving a package.
		UE_LOG(LogStaticMesh, Warning, TEXT("Color vertex buffer being saved with NumVertices=%d Stride=%d VertexData=NULL. This should never happen."),
			NumVertices, Stride );

		int32 SerializedStride = 0;
		int32 SerializedNumVertices = 0;
		Ar << SerializedStride << SerializedNumVertices;
	}
	else
	{
		Ar << Stride << NumVertices;

		if (Ar.IsLoading() && NumVertices > 0)
		{
			// Allocate the vertex data storage type.
			AllocateData(bNeedsCPUAccess);
		}

		if (!StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory())
		{
			if (VertexData != NULL)
			{
				// Serialize the vertex data.
				VertexData->Serialize(Ar);

				if (VertexData->Num() > 0)
				{
					// Make a copy of the vertex data pointer.
					Data = VertexData->GetDataPointer();
				}
			}
		}
	}
}


/** Export the data to a string, used for editor Copy&Paste. */
void FColorVertexBuffer::ExportText(FString &ValueStr) const
{
	// the following code only works if there is data and this method should only be called if there is data
	check(NumVertices);

	ValueStr += FString::Printf(TEXT("ColorVertexData(%i)=("), NumVertices);

	// 9 characters per color (ARGB in hex plus comma)
	ValueStr.Reserve(ValueStr.Len() + NumVertices * 9);

	for(uint32 i = 0; i < NumVertices; ++i)
	{
		uint32 Raw = VertexColor(i).DWColor();
		// does not handle endianess	
		// order: ARGB
		TCHAR ColorString[10];

		// does not use FString::Printf for performance reasons
		FCString::Sprintf(ColorString, TEXT("%.8x,"), Raw);
		ValueStr += ColorString;
	}

	// replace , by )
	ValueStr[ValueStr.Len() - 1] = ')';
}



/** Export the data from a string, used for editor Copy&Paste. */
void FColorVertexBuffer::ImportText(const TCHAR* SourceText)
{
	check(SourceText);
	check(!VertexData);

	uint32 VertexCount;

	if (FParse::Value(SourceText, TEXT("ColorVertexData("), VertexCount))
	{
		while(*SourceText && *SourceText != TEXT(')'))
		{
			++SourceText;
		}

		while(*SourceText && *SourceText != TEXT('('))
		{
			++SourceText;
		}

		check(*SourceText == TEXT('('));
		++SourceText;

		NumVertices = VertexCount;
		AllocateData();
		VertexData->ResizeBuffer(NumVertices);
		uint8 *Dst = (uint8 *)VertexData->GetDataPointer();

		// 9 characters per color (ARGB in hex plus comma)
		for( uint32 i = 0; i < NumVertices; ++i)
		{
			// does not handle endianess or malformed input
			*Dst++ = FParse::HexDigit(SourceText[6]) * 16 + FParse::HexDigit(SourceText[7]);
			*Dst++ = FParse::HexDigit(SourceText[4]) * 16 + FParse::HexDigit(SourceText[5]);
			*Dst++ = FParse::HexDigit(SourceText[2]) * 16 + FParse::HexDigit(SourceText[3]);
			*Dst++ = FParse::HexDigit(SourceText[0]) * 16 + FParse::HexDigit(SourceText[1]);
			SourceText += 9;
		}
		check(*(SourceText - 1) == TCHAR(')'));

		// Make a copy of the vertex data pointer.
		Data = VertexData->GetDataPointer();

		BeginInitResource(this);
	}
}

/**
* Specialized assignment operator, only used when importing LOD's.  
*/
void FColorVertexBuffer::operator=(const FColorVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	VertexData = NULL;
}

void FColorVertexBuffer::GetVertexColors( TArray<FColor>& OutColors ) const
{
	if( VertexData != NULL && NumVertices > 0 )
	{
		OutColors.SetNumUninitialized( NumVertices );

		FMemory::Memcpy( OutColors.GetData(), VertexData->GetDataPointer(), NumVertices * VertexData->GetStride() ) ;
	}
}

/** Load from raw color array */
void FColorVertexBuffer::InitFromColorArray( const FColor *InColors, const uint32 Count, const uint32 InStride )
{
	check( Count > 0 );

	NumVertices = Count;

	// Allocate the vertex data storage type.
	AllocateData();

	// Copy the colors
	{
		VertexData->AddUninitialized(Count);

		const uint8 *Src = (const uint8 *)InColors;
		FColor *Dst = (FColor *)VertexData->GetDataPointer();

		for( uint32 i = 0; i < Count; ++i)
		{
			*Dst++ = *(const FColor*)Src;

			Src += InStride;
		}
	}

	// Make a copy of the vertex data pointer.
	Data = VertexData->GetDataPointer();
}

uint32 FColorVertexBuffer::GetAllocatedSize() const
{
	if(VertexData)
	{
		return VertexData->GetAllocatedSize();
	}
	else
	{
		return 0;
	}
}

void FColorVertexBuffer::InitRHI()
{
	if( VertexData != NULL )
	{
		FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
		if(ResourceArray->GetResourceDataSize())
		{
			// Create the vertex buffer.
			FRHIResourceCreateInfo CreateInfo(ResourceArray);
			VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(),BUF_Static,CreateInfo);
		}
	}
}

void FColorVertexBuffer::AllocateData( bool bNeedsCPUAccess /*= true*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FColorVertexData(bNeedsCPUAccess);
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}
