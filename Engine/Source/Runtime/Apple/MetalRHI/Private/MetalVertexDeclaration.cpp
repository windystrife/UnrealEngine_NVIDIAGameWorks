// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexDeclaration.cpp: Metal vertex declaration RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "ShaderCache.h"

static MTLVertexFormat TranslateElementTypeToMTLType(EVertexElementType Type)
{
	switch (Type)
	{
		case VET_Float1:		return MTLVertexFormatFloat;
		case VET_Float2:		return MTLVertexFormatFloat2;
		case VET_Float3:		return MTLVertexFormatFloat3;
		case VET_Float4:		return MTLVertexFormatFloat4;
		case VET_PackedNormal:	return MTLVertexFormatUChar4Normalized;
		case VET_UByte4:		return MTLVertexFormatUChar4;
		case VET_UByte4N:		return MTLVertexFormatUChar4Normalized;
		case VET_Color:			return MTLVertexFormatUChar4Normalized;
		case VET_Short2:		return MTLVertexFormatShort2;
		case VET_Short4:		return MTLVertexFormatShort4;
		case VET_Short2N:		return MTLVertexFormatShort2Normalized;
		case VET_Half2:			return MTLVertexFormatHalf2;
		case VET_Half4:			return MTLVertexFormatHalf4;
		case VET_Short4N:		return MTLVertexFormatShort4Normalized;
		case VET_UShort2:		return MTLVertexFormatUShort2;
		case VET_UShort4:		return MTLVertexFormatUShort4;
		case VET_UShort2N:		return MTLVertexFormatUShort2Normalized;
		case VET_UShort4N:		return MTLVertexFormatUShort4Normalized;
		case VET_URGB10A2N:		return MTLVertexFormatUInt1010102Normalized;
		default:				UE_LOG(LogMetal, Fatal, TEXT("Unknown vertex element type!")); return MTLVertexFormatFloat;
	};

}

uint32 TranslateElementTypeToSize(EVertexElementType Type)
{
	switch (Type)
	{
		case VET_Float1:		return 4;
		case VET_Float2:		return 8;
		case VET_Float3:		return 12;
		case VET_Float4:		return 16;
		case VET_PackedNormal:	return 4;
		case VET_UByte4:		return 4;
		case VET_UByte4N:		return 4;
		case VET_Color:			return 4;
		case VET_Short2:		return 4;
		case VET_Short4:		return 8;
		case VET_UShort2:		return 4;
		case VET_UShort4:		return 8;
		case VET_Short2N:		return 4;
		case VET_UShort2N:		return 4;
		case VET_Half2:			return 4;
		case VET_Half4:			return 8;
		case VET_Short4N:		return 8;
		case VET_UShort4N:		return 8;
		case VET_URGB10A2N:		return 4;
		default:				UE_LOG(LogMetal, Fatal, TEXT("Unknown vertex element type!")); return 0;
	};
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor()
: VertexDescHash(0)
, VertexDesc(nil)
{
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor(MTLVertexDescriptor* Desc, uint32 Hash)
: VertexDescHash(Hash)
, VertexDesc(Desc)
{
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor(FMetalHashedVertexDescriptor const& Other)
: VertexDescHash(0)
, VertexDesc(nil)
{
	operator=(Other);
}

FMetalHashedVertexDescriptor::~FMetalHashedVertexDescriptor()
{
	[VertexDesc release];
}

FMetalHashedVertexDescriptor& FMetalHashedVertexDescriptor::operator=(FMetalHashedVertexDescriptor const& Other)
{
	if (this != &Other)
	{
		[Other.VertexDesc retain];
		[VertexDesc release];
		
		VertexDescHash = Other.VertexDescHash;
		VertexDesc = Other.VertexDesc;
	}
	return *this;
}

bool FMetalHashedVertexDescriptor::operator==(FMetalHashedVertexDescriptor const& Other) const
{
	bool bEqual = false;
	if (this != &Other)
	{
		if (VertexDescHash == Other.VertexDescHash)
		{
			bEqual = true;
			if (VertexDesc != Other.VertexDesc)
			{
				MTLVertexBufferLayoutDescriptorArray* Layouts = VertexDesc.layouts;
				MTLVertexAttributeDescriptorArray* Attributes = VertexDesc.attributes;
				
				MTLVertexBufferLayoutDescriptorArray* OtherLayouts = Other.VertexDesc.layouts;
				MTLVertexAttributeDescriptorArray* OtherAttributes = Other.VertexDesc.attributes;
				check(Layouts && Attributes && OtherLayouts && OtherAttributes);
				
				for (uint32 i = 0; bEqual && i < MaxVertexElementCount; i++)
				{
					MTLVertexBufferLayoutDescriptor* LayoutDesc = [Layouts objectAtIndexedSubscript:(NSUInteger)i];
					MTLVertexBufferLayoutDescriptor* OtherLayoutDesc = [OtherLayouts objectAtIndexedSubscript:(NSUInteger)i];
					
					bEqual &= ((LayoutDesc != nil) == (OtherLayoutDesc != nil));
					
					if (LayoutDesc && OtherLayoutDesc)
					{
						bEqual &= (LayoutDesc.stride == OtherLayoutDesc.stride);
						bEqual &= (LayoutDesc.stepFunction == OtherLayoutDesc.stepFunction);
						bEqual &= (LayoutDesc.stepRate == OtherLayoutDesc.stepRate);
					}
					
					MTLVertexAttributeDescriptor* AttrDesc = [Attributes objectAtIndexedSubscript:(NSUInteger)i];
					MTLVertexAttributeDescriptor* OtherAttrDesc = [OtherAttributes objectAtIndexedSubscript:(NSUInteger)i];
					
					bEqual &= ((AttrDesc != nil) == (OtherAttrDesc != nil));
					
					if (AttrDesc && OtherAttrDesc)
					{
						bEqual &= (AttrDesc.format == OtherAttrDesc.format);
						bEqual &= (AttrDesc.offset == OtherAttrDesc.offset);
						bEqual &= (AttrDesc.bufferIndex == OtherAttrDesc.bufferIndex);
					}
				}
			}
		}
	}
	else
	{
		bEqual = true;
	}
	return bEqual;
}

FMetalVertexDeclaration::FMetalVertexDeclaration(const FVertexDeclarationElementList& InElements)
	: Elements(InElements)
	, BaseHash(0)
{
	GenerateLayout(InElements);
}

FMetalVertexDeclaration::~FMetalVertexDeclaration()
{
}

FVertexDeclarationRHIRef FMetalDynamicRHI::CreateVertexDeclaration_RenderThread(class FRHICommandListImmediate& RHICmdList, const FVertexDeclarationElementList& Elements)
{
	return GDynamicRHI->RHICreateVertexDeclaration(Elements);
}

FVertexDeclarationRHIRef FMetalDynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	@autoreleasepool {
	uint32 Key = FCrc::MemCrc32(Elements.GetData(), Elements.Num() * sizeof(FVertexElement));
	// look up an existing declaration
	FVertexDeclarationRHIRef* VertexDeclarationRefPtr = VertexDeclarationCache.Find(Key);
	if (VertexDeclarationRefPtr == NULL)
	{
//		NSLog(@"VertDecl Key: %x", Key);

		// create and add to the cache if it doesn't exist.
		VertexDeclarationRefPtr = &VertexDeclarationCache.Add(Key, new FMetalVertexDeclaration(Elements));
		FShaderCache::LogVertexDeclaration(ImmediateContext.Context->GetCurrentState().GetShaderCacheStateObject(), Elements, *VertexDeclarationRefPtr);
	}

	return *VertexDeclarationRefPtr;
	}
}

void FMetalVertexDeclaration::GenerateLayout(const FVertexDeclarationElementList& InElements)
{
	MTLVertexDescriptor* NewLayout = [[MTLVertexDescriptor alloc] init];
	TRACK_OBJECT(STAT_MetalVertexDescriptorCount, NewLayout);

	BaseHash = FCrc::MemCrc_DEPRECATED(InElements.GetData(),InElements.Num()*sizeof(FVertexElement));
	uint32 StrideHash = BaseHash;

	TMap<uint32, uint32> BufferStrides;
	for (uint32 ElementIndex = 0; ElementIndex < InElements.Num(); ElementIndex++)
	{
		const FVertexElement& Element = InElements[ElementIndex];
		
		checkf(Element.Stride == 0 || Element.Offset + TranslateElementTypeToSize(Element.Type) <= Element.Stride, 
			TEXT("Stream component is bigger than stride: Offset: %d, Size: %d [Type %d], Stride: %d"), Element.Offset, TranslateElementTypeToSize(Element.Type), (uint32)Element.Type, Element.Stride);

		StrideHash = FCrc::MemCrc32(&Element.Stride, sizeof(Element.Stride), StrideHash);

		// Vertex & Constant buffers are set up in the same space, so add VB's from the top
		uint32 ShaderBufferIndex = UNREAL_TO_METAL_BUFFER_INDEX(Element.StreamIndex);

		// track the buffer stride, making sure all elements with the same buffer have the same stride
		uint32* ExistingStride = BufferStrides.Find(ShaderBufferIndex);
		if (ExistingStride == NULL)
		{
			// handle 0 stride buffers
			MTLVertexStepFunction Function = (Element.Stride == 0 ? MTLVertexStepFunctionConstant : (Element.bUseInstanceIndex ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex));
			uint32 StepRate = (Element.Stride == 0 ? 0 : 1);
			// even with MTLVertexStepFunctionConstant, it needs a non-zero stride (not sure why)
			uint32 Stride = (Element.Stride == 0 ? TranslateElementTypeToSize(Element.Type) : Element.Stride);

			// look for any unset strides coming from UE4 (this can be removed when all are fixed)
			if (Element.Stride == 0xFFFF)
			{
				NSLog(@"Setting illegal stride - break here if you want to find out why, but this won't break until we try to render with it");
				Stride = 200;
			}

			// set the stride once per buffer
			NewLayout.layouts[ShaderBufferIndex].stride = Stride;
			NewLayout.layouts[ShaderBufferIndex].stepFunction = Function;
			NewLayout.layouts[ShaderBufferIndex].stepRate = StepRate;

			// track this buffer and stride
			BufferStrides.Add(ShaderBufferIndex, Element.Stride);
		}
		else
		{
			// if the strides of elements with same buffer index have different strides, something is VERY wrong
			check(Element.Stride == *ExistingStride);
		}

		// set the format for each element
		NewLayout.attributes[Element.AttributeIndex].format = TranslateElementTypeToMTLType(Element.Type);
		NewLayout.attributes[Element.AttributeIndex].offset = Element.Offset;
		NewLayout.attributes[Element.AttributeIndex].bufferIndex = ShaderBufferIndex;
	}
	
	Layout = FMetalHashedVertexDescriptor(NewLayout, StrideHash);
}
