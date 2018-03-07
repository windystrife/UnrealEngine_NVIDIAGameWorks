// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceFile.h"
#include "Modules/ModuleManager.h"
#include "OpenGLDrv.h"

/** This define decreases file size, but substantially increases frame dump time. Also, if you turn it off, make sure your image viewer correctly displays BMP files with alpha. */
#define USE_COMPRESSED_PNG_INSTEAD_OF_BMP_FOR_CONTENT_OUTPUT 1

#if USE_COMPRESSED_PNG_INSTEAD_OF_BMP_FOR_CONTENT_OUTPUT
	// For PNG compression
	#include "IImageWrapper.h"
	#include "IImageWrapperModule.h"
	const GLenum TextureOutputFormat = GL_RGBA;
#else
	const GLenum TextureOutputFormat = GL_BGRA;
#endif

#define DEBUG_GL_ERRORS_CAUSED_BY_THIS_CODE 1

#if DEBUG_GL_ERRORS_CAUSED_BY_THIS_CODE
	#define ASSERT_NO_GL_ERROR()	check( glGetError() == GL_NO_ERROR )
#else
	#define ASSERT_NO_GL_ERROR()
#endif

extern bool GDisableOpenGLDebugOutput;


#if USE_COMPRESSED_PNG_INSTEAD_OF_BMP_FOR_CONTENT_OUTPUT

void appCreatePNGWithAlpha( const TCHAR* File, int32 Width, int32 Height, FColor* Data, IFileManager* FileManager = &IFileManager::Get() )
{
	// We assume all resources are png for now.
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
	if ( ImageWrapper.IsValid() && ImageWrapper->SetRaw( Data, 4 * Width * Height, Width, Height, ERGBFormat::RGBA, 8 ) )
	{
		FArchive* Ar = FileManager->CreateFileWriter( File );
		if( !Ar )
			return;

		const TArray<uint8>& CompressedData = ImageWrapper->GetCompressed();
		int32 CompressedSize = CompressedData.Num();
		Ar->Serialize( (void*)CompressedData.GetData(), CompressedSize );
		delete Ar;
	}
}

#else

void appCreateBitmapWithAlpha( const TCHAR* File, int32 Width, int32 Height, FColor* Data, IFileManager* FileManager = &IFileManager::Get() )
{
	FArchive* Ar = FileManager->CreateFileWriter( File );
	if( !Ar )
		return;

	// Types.
	#if PLATFORM_SUPPORTS_PRAGMA_PACK
		#pragma pack (push,1)
	#endif
	struct BITMAPFILEHEADER
	{
		uint16   bfType GCC_PACK(1);
		uint32   bfSize GCC_PACK(1);
		uint16   bfReserved1 GCC_PACK(1); 
		uint16   bfReserved2 GCC_PACK(1);
		uint32   bfOffBits GCC_PACK(1);
	} FH; 
	struct BITMAPINFOHEADER
	{
		uint32	biSize GCC_PACK(1);
		int32	biWidth GCC_PACK(1);
		int32	biHeight GCC_PACK(1);
		uint16	biPlanes GCC_PACK(1);
		uint16	biBitCount GCC_PACK(1);
		uint32	biCompression GCC_PACK(1);
		uint32	biSizeImage GCC_PACK(1);
		int32	biXPelsPerMeter GCC_PACK(1);
		int32	biYPelsPerMeter GCC_PACK(1);
		uint32	biClrUsed GCC_PACK(1);
		uint32	biClrImportant GCC_PACK(1);
	} IH;
	#if PLATFORM_SUPPORTS_PRAGMA_PACK
		#pragma pack (pop)
	#endif

	uint32	BytesPerLine = Width * 4;

	// File header.
	FH.bfType       		= INTEL_ORDER16((uint16) ('B' + 256*'M'));
	FH.bfSize       		= INTEL_ORDER32((uint32) (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + BytesPerLine * Height));
	FH.bfReserved1  		= INTEL_ORDER16((uint16) 0);
	FH.bfReserved2  		= INTEL_ORDER16((uint16) 0);
	FH.bfOffBits    		= INTEL_ORDER32((uint32) (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)));
	Ar->Serialize( &FH, sizeof(FH) );

	// Info header.
	IH.biSize               = INTEL_ORDER32((uint32) sizeof(BITMAPINFOHEADER));
	IH.biWidth              = INTEL_ORDER32((uint32) Width);
	IH.biHeight             = INTEL_ORDER32((uint32) Height);
	IH.biPlanes             = INTEL_ORDER16((uint16) 1);
	IH.biBitCount           = INTEL_ORDER16((uint16) 32);
	IH.biCompression        = INTEL_ORDER32((uint32) 0); //BI_RGBA
	IH.biSizeImage          = INTEL_ORDER32((uint32) BytesPerLine * Height);
	IH.biXPelsPerMeter      = INTEL_ORDER32((uint32) 0);
	IH.biYPelsPerMeter      = INTEL_ORDER32((uint32) 0);
	IH.biClrUsed            = INTEL_ORDER32((uint32) 0);
	IH.biClrImportant       = INTEL_ORDER32((uint32) 0);
	Ar->Serialize( &IH, sizeof(IH) );

	// Colors.
	for( int32 i=Height-1; i>=0; i-- )
	{
		for( int32 j=0; j<Width; j++ )
		{
			Ar->Serialize( &Data[i*Width+j].B, 1 );
			Ar->Serialize( &Data[i*Width+j].G, 1 );
			Ar->Serialize( &Data[i*Width+j].R, 1 );
			Ar->Serialize( &Data[i*Width+j].A, 1 );
		}
	}

	// Success.
	delete Ar;
}
#endif

// A simple code to save a single surface into separate DDS file.

// I know there's a TextureFormatDXT module, but it's too intertwined with the rest of the engine,
// and I prefer this small function to be kept independent of everything, as it's only for one simple debug job, and
// to not introduce a lot of UE4 types like FImage that aren't really necessary.

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)\
	((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |\
	((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))
#endif

void appCreateDDSWithSingleSurface( const TCHAR* File, int32 Width, int32 Height, GLint InternalFormat, const void* Data, uint32 DataSize, IFileManager* FileManager = &IFileManager::Get() )
{
	FArchive* Ar = FileManager->CreateFileWriter( File );
	if( !Ar )
		return;

	// Types.
	#if PLATFORM_SUPPORTS_PRAGMA_PACK
		#pragma pack (push,1)
	#endif
	struct DDSPixelFormat
	{
		uint32 Size GCC_PACK(1);
		uint32 Flags GCC_PACK(1);
		uint32 FourCC GCC_PACK(1);
		uint32 RGBBitCount GCC_PACK(1);
		uint32 RBitMask GCC_PACK(1);
		uint32 GBitMask GCC_PACK(1);
		uint32 BBitMask GCC_PACK(1);
		uint32 ABitMask GCC_PACK(1);
	};

	struct DDSHeader
	{
		uint32 Size GCC_PACK(1);
		uint32 Flags GCC_PACK(1);
		uint32 Height GCC_PACK(1);
		uint32 Width GCC_PACK(1);
		uint32 PitchOrLinearSize GCC_PACK(1);
		uint32 Depth GCC_PACK(1);
		uint32 MipMapCount GCC_PACK(1);
		uint32 Reserved[11] GCC_PACK(1);
		struct DDSPixelFormat PixelFormat;
		uint32 Caps1 GCC_PACK(1);
		uint32 Caps2 GCC_PACK(1);
		uint32 Reserved2[3] GCC_PACK(1);
	} Header;
	#if PLATFORM_SUPPORTS_PRAGMA_PACK
		#pragma pack (pop)
	#endif

	uint8 FileType[] = "DDS ";
	Ar->Serialize( FileType, sizeof(uint8)*4 );

	check( sizeof(Header) == 124 );

	memset( &Header, 0, sizeof(Header) );

	Header.Size					= INTEL_ORDER32((uint32) 124);
	Header.Flags				= INTEL_ORDER32((uint32) 0x81007);
	Header.Width				= INTEL_ORDER32((uint32) Width);
	Header.Height				= INTEL_ORDER32((uint32) Height);
	Header.PitchOrLinearSize	= INTEL_ORDER32((uint32) DataSize);
	Header.Depth				= INTEL_ORDER32((uint32) 1);
	Header.MipMapCount			= INTEL_ORDER32((uint32) 1);
	Header.Caps1				= INTEL_ORDER32((uint32) 0x1000);
	Header.PixelFormat.Size		= INTEL_ORDER32((uint32) 32);
	Header.PixelFormat.Flags	= INTEL_ORDER32((uint32) 4);

	switch( InternalFormat )
	{
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
		case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
			Header.PixelFormat.FourCC = INTEL_ORDER32((uint32) MAKEFOURCC('D','X','T','1') );
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
			Header.PixelFormat.FourCC = INTEL_ORDER32((uint32) MAKEFOURCC('D','X','T','3') );
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
			Header.PixelFormat.FourCC = INTEL_ORDER32((uint32) MAKEFOURCC('D','X','T','5') );
			break;
		case GL_COMPRESSED_RED_RGTC1:
			Header.PixelFormat.FourCC = INTEL_ORDER32((uint32) MAKEFOURCC('B','C','4','U') );	// BC4 UNORM
			break;
		case GL_COMPRESSED_SIGNED_RED_RGTC1:
			Header.PixelFormat.FourCC = INTEL_ORDER32((uint32) MAKEFOURCC('B','C','4','S') );	// BC4 SNORM
			break;
		case GL_COMPRESSED_RG_RGTC2:
			Header.PixelFormat.FourCC = INTEL_ORDER32((uint32) MAKEFOURCC('A','T','I','2') );	// BC5 UNORM
			break;
		case GL_COMPRESSED_SIGNED_RG_RGTC2:
			Header.PixelFormat.FourCC = INTEL_ORDER32((uint32) MAKEFOURCC('B','C','5','S') );	// BC5 SNORM
			break;
		default:
			UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Unknown internal format ( 0x%x ) while saving DDS file '%s'. Resulting DDS may be unreadable."), InternalFormat, *File );
			break;
	}

	Ar->Serialize( &Header, sizeof(Header) );
	Ar->Serialize( (void*)Data, DataSize );
	delete Ar;
}


//-----------------------------------------------------------------------------

#if ENABLE_OPENGL_FRAMEDUMP || ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
const TCHAR* GetGLUniformTypeString( GLint UniformType )
{
	switch( UniformType )
	{
	case GL_FLOAT:										return TEXT("GL_FLOAT");
	case GL_FLOAT_VEC2:									return TEXT("GL_FLOAT_VEC2");
	case GL_FLOAT_VEC3:									return TEXT("GL_FLOAT_VEC3");
	case GL_FLOAT_VEC4:									return TEXT("GL_FLOAT_VEC4");
	case GL_INT:										return TEXT("GL_INT");
	case GL_INT_VEC2:									return TEXT("GL_INT_VEC2");
	case GL_INT_VEC3:									return TEXT("GL_INT_VEC3");
	case GL_INT_VEC4:									return TEXT("GL_INT_VEC4");
	case GL_UNSIGNED_INT:								return TEXT("GL_UNSIGNED_INT");
	case GL_UNSIGNED_INT_VEC2:							return TEXT("GL_UNSIGNED_INT_VEC2");
	case GL_UNSIGNED_INT_VEC3:							return TEXT("GL_UNSIGNED_INT_VEC3");
	case GL_UNSIGNED_INT_VEC4:							return TEXT("GL_UNSIGNED_INT_VEC4");
	case GL_BOOL:										return TEXT("GL_BOOL");
	case GL_BOOL_VEC2:									return TEXT("GL_BOOL_VEC2");
	case GL_BOOL_VEC3:									return TEXT("GL_BOOL_VEC3");
	case GL_BOOL_VEC4:									return TEXT("GL_BOOL_VEC4");
	case GL_FLOAT_MAT2:									return TEXT("GL_FLOAT_MAT2");
	case GL_FLOAT_MAT3:									return TEXT("GL_FLOAT_MAT3");
	case GL_FLOAT_MAT4:									return TEXT("GL_FLOAT_MAT4");
	case GL_FLOAT_MAT2x3:								return TEXT("GL_FLOAT_MAT2x3");
	case GL_FLOAT_MAT2x4:								return TEXT("GL_FLOAT_MAT2x4");
	case GL_FLOAT_MAT3x2:								return TEXT("GL_FLOAT_MAT3x2");
	case GL_FLOAT_MAT3x4:								return TEXT("GL_FLOAT_MAT3x4");
	case GL_FLOAT_MAT4x2:								return TEXT("GL_FLOAT_MAT4x2");
	case GL_FLOAT_MAT4x3:								return TEXT("GL_FLOAT_MAT4x3");
	case GL_SAMPLER_1D:									return TEXT("GL_SAMPLER_1D");
	case GL_SAMPLER_2D:									return TEXT("GL_SAMPLER_2D");
	case GL_SAMPLER_3D:									return TEXT("GL_SAMPLER_3D");
	case GL_SAMPLER_CUBE:								return TEXT("GL_SAMPLER_CUBE");
	case GL_SAMPLER_1D_SHADOW:							return TEXT("GL_SAMPLER_1D_SHADOW");
	case GL_SAMPLER_2D_SHADOW:							return TEXT("GL_SAMPLER_2D_SHADOW");
	case GL_SAMPLER_1D_ARRAY:							return TEXT("GL_SAMPLER_1D_ARRAY");
	case GL_SAMPLER_2D_ARRAY:							return TEXT("GL_SAMPLER_2D_ARRAY");
	case GL_SAMPLER_1D_ARRAY_SHADOW:					return TEXT("GL_SAMPLER_1D_ARRAY_SHADOW");
	case GL_SAMPLER_2D_ARRAY_SHADOW:					return TEXT("GL_SAMPLER_2D_ARRAY_SHADOW");
	case GL_SAMPLER_2D_MULTISAMPLE:						return TEXT("GL_SAMPLER_2D_MULTISAMPLE");
	case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:				return TEXT("GL_SAMPLER_2D_MULTISAMPLE_ARRAY");
	case GL_SAMPLER_CUBE_SHADOW:						return TEXT("GL_SAMPLER_CUBE_SHADOW");
	case GL_SAMPLER_BUFFER:								return TEXT("GL_SAMPLER_BUFFER");
	case GL_SAMPLER_2D_RECT:							return TEXT("GL_SAMPLER_2D_RECT");
	case GL_SAMPLER_2D_RECT_SHADOW:						return TEXT("GL_SAMPLER_2D_RECT_SHADOW");
	case GL_INT_SAMPLER_1D:								return TEXT("GL_INT_SAMPLER_1D");
	case GL_INT_SAMPLER_2D:								return TEXT("GL_INT_SAMPLER_2D");
	case GL_INT_SAMPLER_3D:								return TEXT("GL_INT_SAMPLER_3D");
	case GL_INT_SAMPLER_CUBE:							return TEXT("GL_INT_SAMPLER_CUBE");
	case GL_INT_SAMPLER_1D_ARRAY:						return TEXT("GL_INT_SAMPLER_1D_ARRAY");
	case GL_INT_SAMPLER_2D_ARRAY:						return TEXT("GL_INT_SAMPLER_2D_ARRAY");
	case GL_INT_SAMPLER_2D_MULTISAMPLE:					return TEXT("GL_INT_SAMPLER_2D_MULTISAMPLE");
	case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:			return TEXT("GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY");
	case GL_INT_SAMPLER_BUFFER:							return TEXT("GL_INT_SAMPLER_BUFFER");
	case GL_INT_SAMPLER_2D_RECT:						return TEXT("GL_INT_SAMPLER_2D_RECT");
	case GL_UNSIGNED_INT_SAMPLER_1D:					return TEXT("GL_UNSIGNED_INT_SAMPLER_1D");
	case GL_UNSIGNED_INT_SAMPLER_2D:					return TEXT("GL_UNSIGNED_INT_SAMPLER_2D");
	case GL_UNSIGNED_INT_SAMPLER_3D:					return TEXT("GL_UNSIGNED_INT_SAMPLER_3D");
	case GL_UNSIGNED_INT_SAMPLER_CUBE:					return TEXT("GL_UNSIGNED_INT_SAMPLER_CUBE");
	case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:				return TEXT("GL_UNSIGNED_INT_SAMPLER_1D_ARRAY");
	case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:				return TEXT("GL_UNSIGNED_INT_SAMPLER_2D_ARRAY");
	case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:		return TEXT("GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE");
	case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:	return TEXT("GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY");
	case GL_UNSIGNED_INT_SAMPLER_BUFFER:				return TEXT("GL_UNSIGNED_INT_SAMPLER_BUFFER");
	case GL_UNSIGNED_INT_SAMPLER_2D_RECT:				return TEXT("GL_UNSIGNED_INT_SAMPLER_2D_RECT");
	default:											return TEXT("!!!unknown!!!");
	};
}

#endif

#if ENABLE_OPENGL_FRAMEDUMP
static const TCHAR* GetAttachedBufferName( bool bIsScreenBuffer, GLint DrawBufferIndex )
{
	if( bIsScreenBuffer )
	{
		switch( DrawBufferIndex )
		{
		case GL_NONE:			return TEXT("GL_NONE");
		case GL_FRONT_LEFT:		return TEXT("GL_FRONT_LEFT");
		case GL_FRONT_RIGHT:	return TEXT("GL_FRONT_RIGHT");
		case GL_BACK_LEFT:		return TEXT("GL_BACK_LEFT");
		case GL_BACK_RIGHT:		return TEXT("GL_BACK_RIGHT");
		case GL_FRONT:			return TEXT("GL_FRONT");
		case GL_BACK:			return TEXT("GL_BACK");
		case GL_LEFT:			return TEXT("GL_LEFT");
		case GL_RIGHT:			return TEXT("GL_RIGHT");
		case GL_FRONT_AND_BACK:	return TEXT("GL_FRONT_AND_BACK");
		case GL_DEPTH:			return TEXT("GL_DEPTH");
		case GL_STENCIL:		return TEXT("GL_STENCIL");
		default:				return TEXT("unknown");
		}
	}
	else
	{
		switch( DrawBufferIndex )
		{
		case GL_COLOR_ATTACHMENT0:	return TEXT("GL_COLOR_ATTACHMENT0");
		case GL_COLOR_ATTACHMENT1:	return TEXT("GL_COLOR_ATTACHMENT1");
		case GL_COLOR_ATTACHMENT2:	return TEXT("GL_COLOR_ATTACHMENT2");
		case GL_COLOR_ATTACHMENT3:	return TEXT("GL_COLOR_ATTACHMENT3");
		case GL_COLOR_ATTACHMENT4:	return TEXT("GL_COLOR_ATTACHMENT4");
		case GL_COLOR_ATTACHMENT5:	return TEXT("GL_COLOR_ATTACHMENT5");
		case GL_COLOR_ATTACHMENT6:	return TEXT("GL_COLOR_ATTACHMENT6");
		case GL_COLOR_ATTACHMENT7:	return TEXT("GL_COLOR_ATTACHMENT7");
		case GL_COLOR_ATTACHMENT8:	return TEXT("GL_COLOR_ATTACHMENT8");
		case GL_COLOR_ATTACHMENT9:	return TEXT("GL_COLOR_ATTACHMENT9");
		case GL_COLOR_ATTACHMENT10:	return TEXT("GL_COLOR_ATTACHMENT10");
		case GL_COLOR_ATTACHMENT11:	return TEXT("GL_COLOR_ATTACHMENT11");
		case GL_COLOR_ATTACHMENT12:	return TEXT("GL_COLOR_ATTACHMENT12");
		case GL_COLOR_ATTACHMENT13:	return TEXT("GL_COLOR_ATTACHMENT13");
		case GL_COLOR_ATTACHMENT14:	return TEXT("GL_COLOR_ATTACHMENT14");
		case GL_COLOR_ATTACHMENT15:	return TEXT("GL_COLOR_ATTACHMENT15");
		case GL_DEPTH_ATTACHMENT:	return TEXT("GL_DEPTH_ATTACHMENT");
		case GL_STENCIL_ATTACHMENT:	return TEXT("GL_STENCIL_ATTACHMENT");
		default:					return TEXT("unknown");
		}
	}
	return NULL;	// to shut up the compiler; it can't reach here, ever
}

static const TCHAR* GetGLCompareString( GLint CompareFunction )
{
	switch( CompareFunction )
	{
	case GL_NEVER:		return TEXT("GL_NEVER");
	case GL_LESS:		return TEXT("GL_LESS");
	case GL_EQUAL:		return TEXT("GL_EQUAL");
	case GL_LEQUAL:		return TEXT("GL_LEQUAL");
	case GL_GREATER:	return TEXT("GL_GREATER");
	case GL_NOTEQUAL:	return TEXT("GL_NOTEQUAL");
	case GL_GEQUAL:		return TEXT("GL_GEQUAL");
	case GL_ALWAYS:		return TEXT("GL_ALWAYS");
	default:			return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetGLStencilOpString( GLint StencilOp )
{
	switch( StencilOp )
	{
	case GL_ZERO:		return TEXT("GL_ZERO");
	case GL_KEEP:		return TEXT("GL_KEEP");
	case GL_REPLACE:	return TEXT("GL_REPLACE");
	case GL_INCR:		return TEXT("GL_INCR");
	case GL_DECR:		return TEXT("GL_DECR");
	case GL_INCR_WRAP:	return TEXT("GL_INCR_WRAP");
	case GL_DECR_WRAP:	return TEXT("GL_DECR_WRAP");
	case GL_INVERT:		return TEXT("GL_INVERT");
	default:			return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetGLDataTypeString( GLint DataType )
{
//	static FString Unknown;
	switch( DataType )
	{
	case GL_BYTE:			return TEXT("GL_BYTE");
	case GL_UNSIGNED_BYTE:	return TEXT("GL_UNSIGNED_BYTE");
	case GL_SHORT:			return TEXT("GL_SHORT");
	case GL_UNSIGNED_SHORT:	return TEXT("GL_UNSIGNED_SHORT");
	case GL_INT:			return TEXT("GL_INT");
	case GL_UNSIGNED_INT:	return TEXT("GL_UNSIGNED_INT");
	case GL_FLOAT:			return TEXT("GL_FLOAT");
	case GL_DOUBLE:			return TEXT("GL_DOUBLE");
	case GL_HALF_FLOAT:		return TEXT("GL_HALF_FLOAT");
	default:				return TEXT("!!!unknown!!!");
//	default:
//		Unknown = FString::Printf( TEXT("!!!unknown!!!(%d)"), DataType );
//		return *Unknown;
	}
}

static const TCHAR* GetGLBlendingFactorString( GLint BlendingFactor )
{
	switch( BlendingFactor )
	{
	case GL_ZERO:						return TEXT("GL_ZERO");
	case GL_ONE:						return TEXT("GL_ONE");
	case GL_SRC_COLOR:					return TEXT("GL_SRC_COLOR");
	case GL_ONE_MINUS_SRC_COLOR:		return TEXT("GL_ONE_MINUS_SRC_COLOR");
	case GL_SRC_ALPHA:					return TEXT("GL_SRC_ALPHA");
	case GL_ONE_MINUS_SRC_ALPHA:		return TEXT("GL_ONE_MINUS_SRC_ALPHA");
	case GL_DST_ALPHA:					return TEXT("GL_DST_ALPHA");
	case GL_ONE_MINUS_DST_ALPHA:		return TEXT("GL_ONE_MINUS_DST_ALPHA");
	case GL_DST_COLOR:					return TEXT("GL_DST_COLOR");
	case GL_ONE_MINUS_DST_COLOR:		return TEXT("GL_ONE_MINUS_DST_COLOR");
	case GL_SRC_ALPHA_SATURATE:			return TEXT("GL_SRC_ALPHA_SATURATE");
	case GL_CONSTANT_COLOR:				return TEXT("GL_CONSTANT_COLOR");
	case GL_ONE_MINUS_CONSTANT_COLOR:	return TEXT("GL_ONE_MINUS_CONSTANT_COLOR");
	case GL_CONSTANT_ALPHA:				return TEXT("GL_CONSTANT_ALPHA");
	case GL_ONE_MINUS_CONSTANT_ALPHA:	return TEXT("GL_ONE_MINUS_CONSTANT_ALPHA");
	case GL_BLEND_COLOR:				return TEXT("GL_BLEND_COLOR");
	default:							return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetGLBlendFuncString( GLint BlendFunction )
{
	switch( BlendFunction )
	{
	case GL_FUNC_ADD:				return TEXT("GL_FUNC_ADD");
	case GL_MIN:					return TEXT("GL_MIN");
	case GL_MAX:					return TEXT("GL_MAX");
	case GL_FUNC_SUBTRACT:			return TEXT("GL_FUNC_SUBTRACT");
	case GL_FUNC_REVERSE_SUBTRACT:	return TEXT("GL_FUNC_REVERSE_SUBTRACT");
	default:						return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetHintName( GLint Hint )
{
	switch( Hint )
	{
	case GL_DONT_CARE:	return TEXT("GL_DONT_CARE");
	case GL_FASTEST:	return TEXT("GL_FASTEST");
	case GL_NICEST:		return TEXT("GL_NICEST");
	default:			return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetCompressedTextureFormatName( GLint CompressedTextureFormat )
{
	switch( CompressedTextureFormat )
	{
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:			return TEXT("GL_COMPRESSED_RGB_S3TC_DXT1_EXT");
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:			return TEXT("GL_COMPRESSED_RGBA_S3TC_DXT1_EXT");
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:			return TEXT("GL_COMPRESSED_RGBA_S3TC_DXT3_EXT");
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:			return TEXT("GL_COMPRESSED_RGBA_S3TC_DXT5_EXT");
	case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:			return TEXT("GL_COMPRESSED_SRGB_S3TC_DXT1_EXT");
	case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:	return TEXT("GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT");
	case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:	return TEXT("GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT");
	case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:	return TEXT("GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT");
	default:										return TEXT("(other)");
	};
}

static const TCHAR* GetGLLogicOpString( GLint LogicOp )
{
	switch( LogicOp )
	{
	case GL_CLEAR:			return TEXT("GL_CLEAR");
	case GL_AND:			return TEXT("GL_AND");
	case GL_AND_REVERSE:	return TEXT("GL_AND_REVERSE");
	case GL_COPY:			return TEXT("GL_COPY");
	case GL_AND_INVERTED:	return TEXT("GL_AND_INVERTED");
	case GL_NOOP:			return TEXT("GL_NOOP");
	case GL_XOR:			return TEXT("GL_XOR");
	case GL_OR:				return TEXT("GL_OR");
	case GL_NOR:			return TEXT("GL_NOR");
	case GL_EQUIV:			return TEXT("GL_EQUIV");
	case GL_INVERT:			return TEXT("GL_INVERT");
	case GL_OR_REVERSE:		return TEXT("GL_OR_REVERSE");
	case GL_COPY_INVERTED:	return TEXT("GL_COPY_INVERTED");
	case GL_OR_INVERTED:	return TEXT("GL_OR_INVERTED");
	case GL_NAND:			return TEXT("GL_NAND");
	case GL_SET:			return TEXT("GL_SET");
	default:				return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetCullFaceModeName( GLint CullFaceMode )
{
	switch( CullFaceMode )
	{
	case GL_FRONT:			return TEXT("GL_FRONT");
	case GL_BACK:			return TEXT("GL_BACK");
	case GL_FRONT_AND_BACK:	return TEXT("GL_FRONT_AND_BACK");
	default:				return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetFrontFaceName( GLint FrontFace )
{
	switch( FrontFace )
	{
	case GL_CCW:	return TEXT("GL_CCW");
	case GL_CW:		return TEXT("GL_CW");
	default:		return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetAttachmentSlotName( GLenum AttachmentSlot )
{
	switch( AttachmentSlot )
	{
	case GL_FRONT_LEFT:					return TEXT("GL_FRONT_LEFT");
	case GL_FRONT_RIGHT:				return TEXT("GL_FRONT_RIGHT");
	case GL_BACK_LEFT:					return TEXT("GL_BACK_LEFT");
	case GL_BACK_RIGHT:					return TEXT("GL_BACK_RIGHT");
	case GL_DEPTH:						return TEXT("GL_DEPTH");
	case GL_STENCIL:					return TEXT("GL_STENCIL");
	case GL_COLOR_ATTACHMENT0:			return TEXT("GL_COLOR_ATTACHMENT0");
	case GL_COLOR_ATTACHMENT1:			return TEXT("GL_COLOR_ATTACHMENT1");
	case GL_COLOR_ATTACHMENT2:			return TEXT("GL_COLOR_ATTACHMENT2");
	case GL_COLOR_ATTACHMENT3:			return TEXT("GL_COLOR_ATTACHMENT3");
	case GL_COLOR_ATTACHMENT4:			return TEXT("GL_COLOR_ATTACHMENT4");
	case GL_COLOR_ATTACHMENT5:			return TEXT("GL_COLOR_ATTACHMENT5");
	case GL_COLOR_ATTACHMENT6:			return TEXT("GL_COLOR_ATTACHMENT6");
	case GL_COLOR_ATTACHMENT7:			return TEXT("GL_COLOR_ATTACHMENT7");
	case GL_COLOR_ATTACHMENT8:			return TEXT("GL_COLOR_ATTACHMENT8");
	case GL_COLOR_ATTACHMENT9:			return TEXT("GL_COLOR_ATTACHMENT9");
	case GL_COLOR_ATTACHMENT10:			return TEXT("GL_COLOR_ATTACHMENT10");
	case GL_COLOR_ATTACHMENT11:			return TEXT("GL_COLOR_ATTACHMENT11");
	case GL_COLOR_ATTACHMENT12:			return TEXT("GL_COLOR_ATTACHMENT12");
	case GL_COLOR_ATTACHMENT13:			return TEXT("GL_COLOR_ATTACHMENT13");
	case GL_COLOR_ATTACHMENT14:			return TEXT("GL_COLOR_ATTACHMENT14");
	case GL_COLOR_ATTACHMENT15:			return TEXT("GL_COLOR_ATTACHMENT15");
	case GL_DEPTH_ATTACHMENT:			return TEXT("GL_DEPTH_ATTACHMENT");
	case GL_STENCIL_ATTACHMENT:			return TEXT("GL_STENCIL_ATTACHMENT");
	case GL_DEPTH_STENCIL_ATTACHMENT:	return TEXT("GL_DEPTH_STENCIL_ATTACHMENT");
	default:							return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetGLInternalFormatString( GLint InternalFormat )
{
	switch( InternalFormat )
	{
	// Compressed formats
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:	return TEXT("GL_COMPRESSED_RGB_S3TC_DXT1_EXT");
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:	return TEXT("GL_COMPRESSED_RGBA_S3TC_DXT1_EXT");
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:	return TEXT("GL_COMPRESSED_RGBA_S3TC_DXT3_EXT");
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:	return TEXT("GL_COMPRESSED_RGBA_S3TC_DXT5_EXT");

	// Depth/stencil formats
	case GL_DEPTH_COMPONENT16:				return TEXT("GL_DEPTH_COMPONENT16");
	case GL_DEPTH_COMPONENT24:				return TEXT("GL_DEPTH_COMPONENT24");
	case GL_DEPTH_COMPONENT32F:				return TEXT("GL_DEPTH_COMPONENT32F");
	case GL_DEPTH24_STENCIL8:				return TEXT("GL_DEPTH24_STENCIL8");
	case GL_DEPTH32F_STENCIL8:				return TEXT("GL_DEPTH32F_STENCIL8");

	// RGBA
	case GL_RGBA8:							return TEXT("GL_RGBA8");
	case GL_RGBA12:							return TEXT("GL_RGBA12");
	case GL_RGBA16:							return TEXT("GL_RGBA16");
	case GL_RGBA16F:						return TEXT("GL_RGBA16F");
	case GL_RGBA32F:						return TEXT("GL_RGBA32");
	case GL_RGBA16I:						return TEXT("GL_RGBA16I");
	case GL_RGBA16UI:						return TEXT("GL_RGBA16UI");
	case GL_RGBA32I:						return TEXT("GL_RGBA32I");
	case GL_RGBA32UI:						return TEXT("GL_RGBA32UI");
	case GL_RGB10_A2:						return TEXT("GL_RGB10_A2");
	case GL_RGBA4:							return TEXT("GL_RGBA4");
	case GL_RGB5_A1:						return TEXT("GL_RGB5_A1");
	case GL_SRGB8_ALPHA8:					return TEXT("GL_SRGB8_ALPHA8");

	// RG
	case GL_RG8:							return TEXT("GL_RG8");
	case GL_RG16:							return TEXT("GL_RG16");
	case GL_RG16F:							return TEXT("GL_RG16F");
	case GL_RG32F:							return TEXT("GL_RG32F");
	case GL_RG8I:							return TEXT("GL_RG8I");
	case GL_RG8UI:							return TEXT("GL_RG8UI");
	case GL_RG16I:							return TEXT("GL_RG16I");
	case GL_RG16UI:							return TEXT("GL_RG16UI");
	case GL_RG32I:							return TEXT("GL_RG32I");
	case GL_RG32UI:							return TEXT("GL_RG32UI");

	// R
	case GL_R8:								return TEXT("GL_R8");
	case GL_R16:							return TEXT("GL_R16");
	case GL_R16F:							return TEXT("GL_R16F");
	case GL_R32F:							return TEXT("GL_R32F");
	case GL_R8I:							return TEXT("GL_R8I");
	case GL_R8UI:							return TEXT("GL_R8UI");
	case GL_R16I:							return TEXT("GL_R16I");
	case GL_R16UI:							return TEXT("GL_R16UI");
	case GL_R32I:							return TEXT("GL_R32I");
	case GL_R32UI:							return TEXT("GL_R32UI");

	// RGB (at the end, as it's not expected to be used)
	case GL_RGB8:							return TEXT("GL_RGB8");
	case GL_RGB5:							return TEXT("GL_RGB5");
	case GL_R3_G3_B2:						return TEXT("GL_R3_G3_B2");
	case GL_RGB4:							return TEXT("GL_RGB4");
	case GL_SRGB8:							return TEXT("GL_SRGB8");
	case GL_R11F_G11F_B10F:					return TEXT("GL_R11F_G11F_B10F");

	case GL_RGB9_E5:						return TEXT("GL_RGB9_E5");

	default:								return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetComponentType( GLint ComponentType )
{
	switch( ComponentType )
	{
	case GL_FLOAT:					return TEXT("GL_FLOAT");
	case GL_INT:					return TEXT("GL_INT");
	case GL_UNSIGNED_INT:			return TEXT("GL_UNSIGNED_INT");
	case GL_SIGNED_NORMALIZED:		return TEXT("GL_SIGNED_NORMALIZED");
	case GL_UNSIGNED_NORMALIZED:	return TEXT("GL_UNSIGNED_NORMALIZED");
	default:						return TEXT("!!!unknown!!!");
	}
}

static const TCHAR* GetColorEncoding( GLint ColorEncoding )
{
	switch( ColorEncoding )
	{
	case GL_LINEAR:					return TEXT("GL_LINEAR");
	case GL_SRGB:					return TEXT("GL_SRGB");
	default:						return TEXT("!!!unknown!!!");
	}
}

static const TCHAR* GetCubeMapFaceName( GLint CubeMapFace )
{
	switch( CubeMapFace )
	{
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:	return TEXT("GL_TEXTURE_CUBE_MAP_POSITIVE_X");
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:	return TEXT("GL_TEXTURE_CUBE_MAP_NEGATIVE_X");
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:	return TEXT("GL_TEXTURE_CUBE_MAP_POSITIVE_Y");
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:	return TEXT("GL_TEXTURE_CUBE_MAP_NEGATIVE_Y");
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:	return TEXT("GL_TEXTURE_CUBE_MAP_POSITIVE_Z");
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:	return TEXT("GL_TEXTURE_CUBE_MAP_NEGATIVE_Z");
	default:								return TEXT("!!!unknown!!!");
	}
}


static const TCHAR* GetShaderType( GLint Type )
{
	switch( Type )
	{
	case GL_VERTEX_SHADER:		return TEXT("GL_VERTEX_SHADER");
	case GL_GEOMETRY_SHADER:	return TEXT("GL_GEOMETRY_SHADER");
	case GL_FRAGMENT_SHADER:	return TEXT("GL_FRAGMENT_SHADER");
	default:					return TEXT("!!!unknown!!!");
	}
}

static const TCHAR* GetGLTextureFilterString( GLint TextureFilter )
{
	switch( TextureFilter )
	{
	case GL_NEAREST:				return TEXT("GL_NEAREST");
	case GL_LINEAR:					return TEXT("GL_LINEAR");
	case GL_NEAREST_MIPMAP_NEAREST:	return TEXT("GL_NEAREST_MIPMAP_NEAREST");
	case GL_LINEAR_MIPMAP_NEAREST:	return TEXT("GL_LINEAR_MIPMAP_NEAREST");
	case GL_NEAREST_MIPMAP_LINEAR:	return TEXT("GL_NEAREST_MIPMAP_LINEAR");
	case GL_LINEAR_MIPMAP_LINEAR:	return TEXT("GL_LINEAR_MIPMAP_LINEAR");
	default:						return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* GetGLTextureWrapString( GLint TextureWrap )
{
	switch( TextureWrap )
	{
	case GL_REPEAT:				return TEXT("GL_REPEAT");
	case GL_MIRRORED_REPEAT:	return TEXT("GL_MIRRORED_REPEAT");
	case GL_CLAMP_TO_EDGE:		return TEXT("GL_CLAMP_TO_EDGE");
	case GL_CLAMP_TO_BORDER:	return TEXT("GL_CLAMP_TO_BORDER");
	case GL_MIRROR_CLAMP_EXT:	return TEXT("GL_MIRROR_CLAMP_EXT");
	default:					return TEXT("!!!unknown!!!");
	};
}

static const TCHAR* NameOfType( GLint Type )
{
	switch( Type )
	{
	case GL_DOUBLE:			return TEXT("GL_DOUBLE");
	case GL_FLOAT:			return TEXT("GL_FLOAT");
	case GL_HALF_FLOAT:		return TEXT("GL_HALF_FLOAT");
	case GL_UNSIGNED_SHORT:	return TEXT("GL_UNSIGNED_SHORT");
	case GL_SHORT:			return TEXT("GL_SHORT");
	case GL_UNSIGNED_BYTE:	return TEXT("GL_UNSIGNED_BYTE");
	default:				return TEXT("!!!unknown!!!");
	}
}

static int32 SizeOfType( GLint Type )
{
	switch( Type )
	{
	case GL_DOUBLE:			return 8;
	case GL_FLOAT:			return 4;
	case GL_HALF_FLOAT:
	case GL_UNSIGNED_SHORT:
	case GL_SHORT:			return 2;
	case GL_UNSIGNED_BYTE:	return 1;
	default:
		check( 0 );
		return 0;
	}
}

class FOpenGLDebugFrameDumper
{
public:

	/** Event call, called from the engine immediately after a draw command */
	void SignalDrawEvent( const TCHAR* FolderPart, const TCHAR* DrawCommandDescription, GLint ElementArrayType, GLint StartVertex, GLint VertexCount, GLint InstanceCount );

	/** Event call, called from the engine immediately after a clear command */
	void SignalClearEvent( int8 ClearType, int8 NumColors, const float* Colors, float Depth, uint32 Stencil );

	/** Event call, called from the engine immediately after a framebuffer blit command */
	void SignalFramebufferBlitEvent( GLbitfield Mask );

	/** Event call, called from the engine immediately after buffer swap/end of frame */
	void SignalEndFrameEvent( void );

	/** Command to dump information about all events from now until next end frame event (ie. to dump this frame, or partial frame, if frame rendering already started) */
	void TriggerFrameDump( void );

	static inline FOpenGLDebugFrameDumper* Instance( void )
	{
		if( !Singleton )
			Singleton = new FOpenGLDebugFrameDumper;
		return Singleton;
	}

protected:

	static FOpenGLDebugFrameDumper* Singleton;

	struct TextureLevelInfo
	{
		GLint		Width;
		GLint		Height;
		GLint		Depth;
		GLint		Samples;
		GLboolean	bHasFixedSampleLocations;
		GLint		InternalFormat;
		GLint		RedBits;
		GLint		GreenBits;
		GLint		BlueBits;
		GLint		AlphaBits;
		GLint		DepthBits;
		GLint		StencilBits;
		GLint		SharedSize;
		GLint		RedType;
		GLint		GreenType;
		GLint		BlueType;
		GLint		AlphaType;
		GLint		DepthType;
		GLboolean	bIsCompressed;
		GLint		CompressedSize;
		GLint		DataStoreBinding;
	};

	struct EFramebufferAttachmentSlotType
	{
		enum Type
		{
			Color,
			Depth,
			Stencil,
		};
	};

	struct VertexAttribInfo
	{
		GLint	Index;
		GLuint	Stride;
		GLint	Type;
		GLint	Size;
		GLint	SizeRead;
		GLuint	Offset;
		GLuint	OffsetWithinVertex;
		bool	bInteger;
		bool	bNormalized;
		bool	bSkip;
		bool	bDivisor;
	};

	static int32 CDECL qsort_compare_VertexAttribInfo( const void* A, const void* B )
	{
		const VertexAttribInfo* Element1 = (const VertexAttribInfo*)A;
		const VertexAttribInfo* Element2 = (const VertexAttribInfo*)B;
		return Element1->Offset - Element2->Offset;
	}

	/** Event counter. Describes how to name the subfolder we dump the next event to. */
	uint32 EventCounter;

	/** Frame counter. Describes how to name the subfolder we dump the next frame to. */
	uint32 FrameCounter;

	FString*	CachedRootFolder;
	FString*	CachedFrameFolder;
	FString*	CachedEventFolder;

	bool bDumpingFrame;

	FOpenGLDebugFrameDumper( void )
	:	EventCounter( 0 )
	,	FrameCounter( 0 )
	,	CachedRootFolder( NULL )
	,	CachedFrameFolder( NULL )
	,	CachedEventFolder( NULL )
	,	bDumpingFrame( false )
	{
	}

	void DumpRenderTargetsState( FOutputDeviceFile& LogFile );
	void DumpDepthState( FOutputDeviceFile& LogFile );
	void DumpStencilState( FOutputDeviceFile& LogFile );
	void DumpBufferMasks( FOutputDeviceFile& LogFile );
	void DumpClearValues( FOutputDeviceFile& LogFile );
	void DumpMultisamplingSettings( FOutputDeviceFile& LogFile );
	void DumpScissorAndViewport( FOutputDeviceFile& LogFile );
	void DumpVertexAttribArraysState( FOutputDeviceFile& LogFile );
	void DumpBlendingState( FOutputDeviceFile& LogFile );
	void DumpBufferBindings( FOutputDeviceFile& LogFile );
	void DumpHintSettings( FOutputDeviceFile& LogFile );
	void DumpOpenGLLimits( FOutputDeviceFile& LogFile );
	void DumpPointsSettings( FOutputDeviceFile& LogFile );
	void DumpLinesSettings( FOutputDeviceFile& LogFile );
	void DumpPolygonsSettings( FOutputDeviceFile& LogFile );
	void DumpLogicOpsSettings( FOutputDeviceFile& LogFile );
	void DumpPixelModeSettings( FOutputDeviceFile& LogFile );
	void DumpTextureLimitsAndBindings( FOutputDeviceFile& LogFile );
	void DumpProgramSettings( FOutputDeviceFile& LogFile );

	void DumpRenderbufferSettings( FOutputDeviceFile& LogFile, GLint RenderbufferID );
	void DumpFramebufferAttachmentSettings( FOutputDeviceFile& LogFile, GLenum AttachmentSlot );
	void DumpFramebufferSettings( FOutputDeviceFile& LogFile, GLint FramebufferID );
	void DumpTextureUnitSettings( FOutputDeviceFile& LogFile, GLint TextureUnitIndex );
	void DumpBoundTextureSettings( FOutputDeviceFile& LogFile, GLenum UnitTarget );
	void DumpBoundTextureSurfaceSettings( FOutputDeviceFile& LogFile, GLenum SurfaceType, GLint BaseLevel, GLint MaxLevel );
	void GetBoundTextureSurfaceLevelSettings( GLenum SurfaceType, GLint Level, TextureLevelInfo& OutInfo );
	void DumpBoundSamplerSettings( FOutputDeviceFile& LogFile, GLint SamplerID );

	void DumpProgramContents( FOutputDeviceFile& LogFile, GLint ProgramID );
	void DumpShaderContents( FOutputDeviceFile& LogFile, GLint ShaderID );

	void DumpFramebufferContent( GLint FramebufferID, GLint AttachmentSlot, const TCHAR* TargetFilename, EFramebufferAttachmentSlotType::Type SlotType, bool bShouldFlipImageVertically );
	void DumpTextureContentForImageUnit( int32 UnitIndex );
	void DumpTextureSurfaceContent( const TCHAR* TargetFilename, GLenum SurfaceType, GLint Level );

	void DumpGeneralOpenGLState( const TCHAR* DrawCommandDescription, bool bIsDrawEvent, bool bIsFramebufferBlitEvent );
	void DumpFramebufferState( bool bReadFramebuffer );
	void DumpProgramAndShaderState( void );
	void DumpBoundTextureState( void );

	void DumpFramebufferContents( bool bReadFramebuffer );
	void DumpBoundTexturesContents( void );

	void DumpElementArrayBufferContents( GLenum ElementArrayType );
	void DumpRelevantVertexArrayBufferContents( GLint StartVertex, GLint VertexCount, GLint InstanceCount );
	void DumpBoundVertexArrayBufferContents( GLint VertexBufferID, GLint StartVertex, GLint VertexCount, GLint InstanceCount );

	void InterpretUniform( GLint UniformType, void* DataBuffer, FString& OutAppendString );

	void SetNewEventFolder( const FString& EventString );
};

FOpenGLDebugFrameDumper* FOpenGLDebugFrameDumper::Singleton = NULL;

void FOpenGLDebugFrameDumper::DumpRenderTargetsState( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Render Targets") LINE_TERMINATOR );

	if ( FOpenGL::SupportsMultipleRenderTargets() )
	{
		GLint DrawFramebuffer;
		glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &DrawFramebuffer );
		ASSERT_NO_GL_ERROR();

		GLint MaxDrawBuffers = 0;
		glGetIntegerv( GL_MAX_DRAW_BUFFERS, &MaxDrawBuffers );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\tGL_MAX_DRAW_BUFFERS: %d") LINE_TERMINATOR, MaxDrawBuffers );

		GLint AttachedBufferIndex;
		for( GLint DrawBufferIndex = 0; DrawBufferIndex < MaxDrawBuffers; ++DrawBufferIndex )
		{
			glGetIntegerv( GL_DRAW_BUFFER0+DrawBufferIndex, &AttachedBufferIndex );
			ASSERT_NO_GL_ERROR();
			if( AttachedBufferIndex )
			{
				const TCHAR* AttachedBufferName = GetAttachedBufferName( ( DrawFramebuffer == 0 ), AttachedBufferIndex );
				if( AttachedBufferName )
				{
					LogFile.Logf( TEXT("\t\tGL_DRAW_BUFFER%d: %s") LINE_TERMINATOR, DrawBufferIndex, AttachedBufferName );
				}
				else
				{
					LogFile.Logf( TEXT("\t\tGL_DRAW_BUFFER%d: 0x%x") LINE_TERMINATOR, DrawBufferIndex, AttachedBufferIndex );
				}
			}
		}

		GLint ReadFramebuffer;
		glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &ReadFramebuffer );
		ASSERT_NO_GL_ERROR();
		glGetIntegerv( GL_READ_BUFFER, &AttachedBufferIndex );
		ASSERT_NO_GL_ERROR();
		const TCHAR* AttachedBufferName = GetAttachedBufferName( ( ReadFramebuffer == 0 ), AttachedBufferIndex );
		if( AttachedBufferName )
		{
			LogFile.Logf( TEXT("\tGL_READ_BUFFER: %s") LINE_TERMINATOR, AttachedBufferName );
		}
		else
		{
			LogFile.Logf( TEXT("\tGL_READ_BUFFER: 0x%x") LINE_TERMINATOR, AttachedBufferIndex );
		}
	}
	else
	{
		//@todo-mobile: More debug info
		GLint CurrentFBO;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &CurrentFBO);
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\GL_FRAMEBUFFER_BINDING: %d") LINE_TERMINATOR, CurrentFBO );
	}
}

void FOpenGLDebugFrameDumper::DumpDepthState( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Depth") LINE_TERMINATOR );

	GLboolean DepthTestEnabled;
	glGetBooleanv( GL_DEPTH_TEST, &DepthTestEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_DEPTH_TEST: %s") LINE_TERMINATOR, DepthTestEnabled ? TEXT("Enabled") : TEXT("Disabled") );

	GLfloat DepthClearValue;
	glGetFloatv( GL_DEPTH_CLEAR_VALUE, &DepthClearValue );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_DEPTH_CLEAR_VALUE: %f") LINE_TERMINATOR, DepthClearValue );

	GLint DepthFunction;
	glGetIntegerv( GL_DEPTH_FUNC, &DepthFunction );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_DEPTH_FUNC: %s") LINE_TERMINATOR, GetGLCompareString( DepthFunction ) );

	GLfloat DepthRange[2];
	glGetFloatv( GL_DEPTH_RANGE, DepthRange );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_DEPTH_RANGE: { %f, %f }") LINE_TERMINATOR, DepthRange[0], DepthRange[1] );

	GLboolean WriteMask;
	glGetBooleanv( GL_DEPTH_WRITEMASK, &WriteMask );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_DEPTH_WRITEMASK: %s") LINE_TERMINATOR, WriteMask ? TEXT("TRUE") : TEXT("FALSE") );
}

void FOpenGLDebugFrameDumper::DumpStencilState( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Stencil") LINE_TERMINATOR );

	GLboolean StencilTestEnabled;
	glGetBooleanv( GL_STENCIL_TEST, &StencilTestEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_TEST: %s") LINE_TERMINATOR, StencilTestEnabled ? TEXT("Enabled") : TEXT("Disabled") );

	GLint ClearValue;
	glGetIntegerv( GL_STENCIL_CLEAR_VALUE, &ClearValue );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_CLEAR_VALUE: 0x%x") LINE_TERMINATOR, ClearValue );

	GLint TestFailResult;
	glGetIntegerv( GL_STENCIL_FAIL, &TestFailResult );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_FAIL: %s") LINE_TERMINATOR, GetGLStencilOpString( TestFailResult ) );

	GLint TestPassDepthFailResult;
	glGetIntegerv( GL_STENCIL_PASS_DEPTH_FAIL, &TestPassDepthFailResult );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_PASS_DEPTH_FAIL: %s") LINE_TERMINATOR, GetGLStencilOpString( TestPassDepthFailResult ) );

	GLint TestPassDepthPassResult;
	glGetIntegerv( GL_STENCIL_PASS_DEPTH_PASS, &TestPassDepthPassResult );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_PASS_DEPTH_PASS: %s") LINE_TERMINATOR, GetGLStencilOpString( TestPassDepthPassResult ) );

	GLint CompareFunction;
	glGetIntegerv( GL_STENCIL_FUNC, &CompareFunction );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_FUNC: %s") LINE_TERMINATOR, GetGLCompareString( CompareFunction ) );

	GLint CompareReference;
	glGetIntegerv( GL_STENCIL_REF, &CompareReference );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_REF: 0x%x") LINE_TERMINATOR, CompareReference );

	GLint ValueMask;
	glGetIntegerv( GL_STENCIL_VALUE_MASK, &ValueMask );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_VALUE_MASK: 0x%x") LINE_TERMINATOR, ValueMask );

	GLint WriteMask;
	glGetIntegerv( GL_STENCIL_WRITEMASK, &WriteMask );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_WRITEMASK: 0x%x") LINE_TERMINATOR, WriteMask );
}

void FOpenGLDebugFrameDumper::DumpBufferMasks( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Buffer Masks") LINE_TERMINATOR );

	GLint MaxDrawBuffers = 0;
	glGetIntegerv( GL_MAX_DRAW_BUFFERS, &MaxDrawBuffers );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_DRAW_BUFFERS: %d") LINE_TERMINATOR, MaxDrawBuffers );

	GLboolean ColorWriteMask[4];
	for( GLint DrawBufferIndex = 0; DrawBufferIndex < MaxDrawBuffers; ++DrawBufferIndex )
	{
		glGetBooleani_v( GL_COLOR_WRITEMASK, DrawBufferIndex, ColorWriteMask );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\t\tGL_COLOR_WRITEMASK for buffer %d: ( %s, %s, %s, %s )") LINE_TERMINATOR,
			DrawBufferIndex,
			ColorWriteMask[0] ? TEXT("TRUE") : TEXT("FALSE"),
			ColorWriteMask[1] ? TEXT("TRUE") : TEXT("FALSE"),
			ColorWriteMask[2] ? TEXT("TRUE") : TEXT("FALSE"),
			ColorWriteMask[3] ? TEXT("TRUE") : TEXT("FALSE") );
	}

	GLboolean DepthWriteMask;
	glGetBooleanv( GL_DEPTH_WRITEMASK, &DepthWriteMask );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_DEPTH_WRITEMASK: %s") LINE_TERMINATOR, DepthWriteMask ? TEXT("TRUE") : TEXT("FALSE") );

	GLint StencilValueMask;
	glGetIntegerv( GL_STENCIL_VALUE_MASK, &StencilValueMask );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_VALUE_MASK: 0x%x") LINE_TERMINATOR, StencilValueMask );

	GLint StencilWriteMask;
	glGetIntegerv( GL_STENCIL_WRITEMASK, &StencilWriteMask );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_WRITEMASK: 0x%x") LINE_TERMINATOR, StencilWriteMask );

//	GLint StencilBackWriteMask;
//	glGetIntegerv( GL_STENCIL_BACK_WRITEMASK, &StencilBackWriteMask );
//	ASSERT_NO_GL_ERROR();
//	LogFile.Logf( TEXT("\tGL_STENCIL_BACK_WRITEMASK: 0x%x") LINE_TERMINATOR, StencilBackWriteMask );
}

void FOpenGLDebugFrameDumper::DumpClearValues( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Clear Values") LINE_TERMINATOR );

	GLfloat ColorClearValue[4];
	glGetFloatv( GL_COLOR_CLEAR_VALUE, ColorClearValue );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_COLOR_CLEAR_VALUE: ( %f, %f, %f, %f )") LINE_TERMINATOR, ColorClearValue[0], ColorClearValue[1], ColorClearValue[2], ColorClearValue[3] );

	GLfloat DepthClearValue;
	glGetFloatv( GL_DEPTH_CLEAR_VALUE, &DepthClearValue );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_DEPTH_CLEAR_VALUE: %f") LINE_TERMINATOR, DepthClearValue );

	GLint StencilClearValue;
	glGetIntegerv( GL_STENCIL_CLEAR_VALUE, &StencilClearValue );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_STENCIL_CLEAR_VALUE: %d") LINE_TERMINATOR, StencilClearValue );
}

void FOpenGLDebugFrameDumper::DumpMultisamplingSettings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Multisampling Settings") LINE_TERMINATOR );

	GLboolean MultisamplingEnabled;
	glGetBooleanv( GL_MULTISAMPLE, &MultisamplingEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MULTISAMPLE: %s") LINE_TERMINATOR, MultisamplingEnabled ? TEXT("Enabled") : TEXT("Disabled") );

	GLboolean SampleAlphaToCoverage;
	glGetBooleanv( GL_SAMPLE_ALPHA_TO_COVERAGE, &SampleAlphaToCoverage );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SAMPLE_ALPHA_TO_COVERAGE: %s") LINE_TERMINATOR, SampleAlphaToCoverage ? TEXT("Enabled") : TEXT("Disabled") );

	GLboolean SampleAlphaToOne;
	glGetBooleanv( GL_SAMPLE_ALPHA_TO_ONE, &SampleAlphaToOne );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SAMPLE_ALPHA_TO_ONE: %s") LINE_TERMINATOR, SampleAlphaToOne ? TEXT("Enabled") : TEXT("Disabled") );

	GLboolean SampleCoverage;
	glGetBooleanv( GL_SAMPLE_COVERAGE, &SampleCoverage );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SAMPLE_COVERAGE: %s") LINE_TERMINATOR, SampleCoverage ? TEXT("Enabled") : TEXT("Disabled") );

	GLboolean SampleCoverageInvert;
	glGetBooleanv( GL_SAMPLE_COVERAGE_INVERT, &SampleCoverageInvert );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SAMPLE_COVERAGE_INVERT: %s") LINE_TERMINATOR, SampleCoverageInvert ? TEXT("Enabled") : TEXT("Disabled") );

	GLfloat SampleCoverageValue;
	glGetFloatv( GL_SAMPLE_COVERAGE_VALUE, &SampleCoverageValue );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SAMPLE_COVERAGE_VALUE: %f") LINE_TERMINATOR, SampleCoverageValue );

	GLint SampleBuffers;
	glGetIntegerv( GL_SAMPLE_BUFFERS, &SampleBuffers );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SAMPLE_BUFFERS: %d") LINE_TERMINATOR, SampleBuffers );

	GLint Samples;
	glGetIntegerv( GL_SAMPLES, &Samples );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SAMPLES: %d") LINE_TERMINATOR, Samples );
}

void FOpenGLDebugFrameDumper::DumpScissorAndViewport( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Scissor & Viewport") LINE_TERMINATOR );

	GLboolean ScissorTestEnabled;
	glGetBooleanv( GL_SCISSOR_TEST, &ScissorTestEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SCISSOR_TEST: %s") LINE_TERMINATOR, ScissorTestEnabled ? TEXT("Enabled") : TEXT("Disabled") );

	GLint ScissorBox[4];
	glGetIntegerv( GL_SCISSOR_BOX, ScissorBox );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SCISSOR_BOX: { %d, %d, %d, %d }") LINE_TERMINATOR, ScissorBox[0], ScissorBox[1], ScissorBox[2], ScissorBox[3] );

	GLint Viewport[4];
	glGetIntegerv( GL_VIEWPORT, Viewport );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_VIEWPORT: { %d, %d, %d, %d }") LINE_TERMINATOR, Viewport[0], Viewport[1], Viewport[2], Viewport[3] );

	GLint MaxClipPlanes;
	glGetIntegerv( GL_MAX_CLIP_DISTANCES, &MaxClipPlanes );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_CLIP_DISTANCES: %d") LINE_TERMINATOR, MaxClipPlanes );

	for( int32 ClipPlaneIndex = 0; ClipPlaneIndex < MaxClipPlanes; ++ClipPlaneIndex )
	{
		GLboolean Enabled = glIsEnabled( GL_CLIP_DISTANCE0+ClipPlaneIndex );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\t\tGL_CLIP_DISTANCE%d: %s") LINE_TERMINATOR, ClipPlaneIndex, Enabled ? TEXT("Enabled") : TEXT("Disabled") );
	}
}

void FOpenGLDebugFrameDumper::DumpVertexAttribArraysState( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Vertex Attrib Arrays") LINE_TERMINATOR );

	// Get generic vertex array count
	GLint MaxVertexAttribs = 0;
	glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &MaxVertexAttribs );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_VERTEX_ATTRIBS: %d") LINE_TERMINATOR, MaxVertexAttribs );

	// For each generic vertex array, get info
	for( GLint VertexAttribArrayIndex = 0; VertexAttribArrayIndex < MaxVertexAttribs; ++VertexAttribArrayIndex )
	{
		GLint VertexAttribArrayEnabled;
		glGetVertexAttribiv( VertexAttribArrayIndex, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &VertexAttribArrayEnabled );
		ASSERT_NO_GL_ERROR();

		LogFile.Logf( TEXT("\tVertex Attrib Array %d") LINE_TERMINATOR, VertexAttribArrayIndex );

		if( VertexAttribArrayEnabled )
		{
			LogFile.Log( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_ENABLED: TRUE") LINE_TERMINATOR );

			GLint VertexAttribArraySize;
			glGetVertexAttribiv( VertexAttribArrayIndex, GL_VERTEX_ATTRIB_ARRAY_SIZE, &VertexAttribArraySize );
			ASSERT_NO_GL_ERROR();
			if( VertexAttribArraySize == GL_BGRA )
			{
				LogFile.Log( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_SIZE: GL_BGRA(4)") LINE_TERMINATOR );
			}
			else
			{
				LogFile.Logf( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_SIZE: %d") LINE_TERMINATOR, VertexAttribArraySize );
			}

			GLint VertexAttribArrayStride;
			glGetVertexAttribiv( VertexAttribArrayIndex, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &VertexAttribArrayStride );
			ASSERT_NO_GL_ERROR();
			LogFile.Logf( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_STRIDE: %d") LINE_TERMINATOR, VertexAttribArrayStride );

			GLint VertexAttribArrayType;
			glGetVertexAttribiv( VertexAttribArrayIndex, GL_VERTEX_ATTRIB_ARRAY_TYPE, &VertexAttribArrayType );
			ASSERT_NO_GL_ERROR();
			LogFile.Logf( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_TYPE: %s") LINE_TERMINATOR, GetGLDataTypeString( VertexAttribArrayType ) );

			GLint VertexAttribArrayNormalized;
			glGetVertexAttribiv( VertexAttribArrayIndex, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &VertexAttribArrayNormalized );
			ASSERT_NO_GL_ERROR();
			LogFile.Logf( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_NORMALIZED: %s") LINE_TERMINATOR, VertexAttribArrayNormalized ? TEXT("TRUE") : TEXT("FALSE") );

			GLvoid* VertexAttribArrayPointer;
			glGetVertexAttribPointerv( VertexAttribArrayIndex, GL_VERTEX_ATTRIB_ARRAY_POINTER, &VertexAttribArrayPointer );
			ASSERT_NO_GL_ERROR();
			LogFile.Logf( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_POINTER: 0x%x") LINE_TERMINATOR, VertexAttribArrayPointer );

			GLint VertexAttribArrayBufferBinding;
			glGetVertexAttribiv( VertexAttribArrayIndex, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VertexAttribArrayBufferBinding );
			ASSERT_NO_GL_ERROR();
			LogFile.Logf( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: %d") LINE_TERMINATOR, VertexAttribArrayBufferBinding );

			GLint VertexAttribArrayIsInteger;
			glGetVertexAttribiv( VertexAttribArrayIndex, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &VertexAttribArrayIsInteger );
			ASSERT_NO_GL_ERROR();
			LogFile.Logf( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_INTEGER: %s") LINE_TERMINATOR, VertexAttribArrayIsInteger ? TEXT("TRUE") : TEXT("FALSE") );

			GLint VertexAttribArrayDivisor;
			glGetVertexAttribiv( VertexAttribArrayIndex, GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ARB, &VertexAttribArrayDivisor );
			ASSERT_NO_GL_ERROR();
			LogFile.Logf( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_DIVISOR: %d") LINE_TERMINATOR, VertexAttribArrayDivisor );
		}
		else
		{
			LogFile.Log( TEXT("\t\tGL_VERTEX_ATTRIB_ARRAY_ENABLED: FALSE") LINE_TERMINATOR );

			if( VertexAttribArrayIndex )
			{
				GLfloat CurrentVertexAttribFloat[4];
				glGetVertexAttribfv( VertexAttribArrayIndex, GL_CURRENT_VERTEX_ATTRIB, CurrentVertexAttribFloat );
				ASSERT_NO_GL_ERROR();
				LogFile.Logf( TEXT("\t\tGL_CURRENT_VERTEX_ATTRIB (assumming float): { %f, %f, %f, %f }") LINE_TERMINATOR, CurrentVertexAttribFloat[0], CurrentVertexAttribFloat[1], CurrentVertexAttribFloat[2], CurrentVertexAttribFloat[3] );

				GLuint CurrentVertexAttribInteger[4];
				glGetVertexAttribIuiv( VertexAttribArrayIndex, GL_CURRENT_VERTEX_ATTRIB, CurrentVertexAttribInteger );
				ASSERT_NO_GL_ERROR();
				LogFile.Logf( TEXT("\t\tGL_CURRENT_VERTEX_ATTRIB (assumming uint32): { %f, %f, %f, %f }") LINE_TERMINATOR, CurrentVertexAttribInteger[0], CurrentVertexAttribInteger[1], CurrentVertexAttribInteger[2], CurrentVertexAttribInteger[3] );
			}
			else
			{
				LogFile.Log( TEXT("\t\tVertex attrib array disabled for vertex array zero. Make sure the shader isn't trying to use gl_Position, as this won't make much sense.") LINE_TERMINATOR );
				UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Vertex attrib array is disabled for array zero. This makes sense only if the draw doesn't use vertex buffers at all, relying on vertex id and instance id instead.") );
			}
		}
	}
}

void FOpenGLDebugFrameDumper::DumpBlendingState( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Blending State") LINE_TERMINATOR );

	GLboolean bIsEnabled = glIsEnabled( GL_BLEND );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_BLEND: %s") LINE_TERMINATOR, bIsEnabled ? TEXT("Enabled") : TEXT("Disabled") );

	GLint BlendSourceRGB;
	glGetIntegerv( GL_BLEND_SRC_RGB, &BlendSourceRGB );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\t\tGL_BLEND_SRC_RGB: %s") LINE_TERMINATOR, GetGLBlendingFactorString( BlendSourceRGB ) );

	GLint BlendSourceAlpha;
	glGetIntegerv( GL_BLEND_SRC_ALPHA, &BlendSourceAlpha );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\t\tGL_BLEND_SRC_ALPHA: %s") LINE_TERMINATOR, GetGLBlendingFactorString( BlendSourceAlpha ) );

	GLint BlendDestinationRGB;
	glGetIntegerv( GL_BLEND_DST_RGB, &BlendDestinationRGB );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\t\tGL_BLEND_DST_RGB: %s") LINE_TERMINATOR, GetGLBlendingFactorString( BlendDestinationRGB ) );

	GLint BlendDestinationAlpha;
	glGetIntegerv( GL_BLEND_DST_ALPHA, &BlendDestinationAlpha );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\t\tGL_BLEND_DST_ALPHA: %s") LINE_TERMINATOR, GetGLBlendingFactorString( BlendDestinationAlpha ) );

	GLfloat BlendColor[4];
	glGetFloatv( GL_BLEND_COLOR, BlendColor );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\t\tGL_BLEND_COLOR: ( %f, %f, %f, %f )") LINE_TERMINATOR, BlendColor[0], BlendColor[1], BlendColor[2], BlendColor[3] );

	GLint BlendEquationRGB;
	glGetIntegerv( GL_BLEND_EQUATION_RGB, &BlendEquationRGB );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\t\tGL_BLEND_EQUATION_RGB: %s") LINE_TERMINATOR, GetGLBlendFuncString( BlendEquationRGB ) );

	GLint BlendEquationAlpha;
	glGetIntegerv( GL_BLEND_EQUATION_ALPHA, &BlendEquationAlpha );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\t\tGL_BLEND_EQUATION_ALPHA: %s") LINE_TERMINATOR, GetGLBlendFuncString( BlendEquationAlpha ) );
}

void FOpenGLDebugFrameDumper::DumpBufferBindings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Buffer Object Bindings") LINE_TERMINATOR );

	GLint ArrayBufferBinding;
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &ArrayBufferBinding );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_ARRAY_BUFFER_BINDING: %d") LINE_TERMINATOR, ArrayBufferBinding );

	GLint ElementArrayBufferBinding;
	glGetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING, &ElementArrayBufferBinding );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_ELEMENT_ARRAY_BUFFER_BINDING: %d") LINE_TERMINATOR, ElementArrayBufferBinding );

	GLint UniformBufferBinding;
	glGetIntegerv( GL_UNIFORM_BUFFER_BINDING, &UniformBufferBinding );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_UNIFORM_BUFFER_BINDING: %d") LINE_TERMINATOR, UniformBufferBinding );

	GLint MaxUniformBuffers = 0;
	glGetIntegerv( GL_MAX_UNIFORM_BUFFER_BINDINGS, &MaxUniformBuffers );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_UNIFORM_BUFFER_BINDINGS: %d") LINE_TERMINATOR, MaxUniformBuffers );

	for( GLint UniformBufferIndex = 0; UniformBufferIndex < MaxUniformBuffers; ++UniformBufferIndex )
	{
		GLint UniformBufferBound;
		glGetIntegeri_v( GL_UNIFORM_BUFFER_BINDING, UniformBufferIndex, &UniformBufferBound );
		ASSERT_NO_GL_ERROR();
		if( UniformBufferBound )
		{
			GLint UniformBufferStart;
			glGetIntegeri_v( GL_UNIFORM_BUFFER_START, UniformBufferIndex, &UniformBufferStart );
			ASSERT_NO_GL_ERROR();

			GLint UniformBufferSize;
			glGetIntegeri_v( GL_UNIFORM_BUFFER_SIZE, UniformBufferIndex, &UniformBufferSize );
			ASSERT_NO_GL_ERROR();

			LogFile.Logf( TEXT("\t\tIndexed GL_UNIFORM_BUFFER_BINDING for index %d: %d ( start: %d, size: %d )") LINE_TERMINATOR, UniformBufferIndex, UniformBufferBound, UniformBufferStart, UniformBufferSize );
		}
	}

	GLint PixelPackBufferBinding;
	glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, &PixelPackBufferBinding );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PIXEL_PACK_BUFFER_BINDING: %d") LINE_TERMINATOR, PixelPackBufferBinding );

	GLint PixelUnpackBufferBinding;
	glGetIntegerv( GL_PIXEL_UNPACK_BUFFER_BINDING, &PixelUnpackBufferBinding );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PIXEL_UNPACK_BUFFER_BINDING: %d") LINE_TERMINATOR, PixelUnpackBufferBinding );

	GLint DrawFramebufferBinding;
	glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &DrawFramebufferBinding );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_DRAW_FRAMEBUFFER_BINDING: %d") LINE_TERMINATOR, DrawFramebufferBinding );

	GLint ReadFramebufferBinding;
	glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &ReadFramebufferBinding );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_READ_FRAMEBUFFER_BINDING: %d") LINE_TERMINATOR, ReadFramebufferBinding );

	GLint RenderbufferBinding;
	glGetIntegerv( GL_RENDERBUFFER_BINDING, &RenderbufferBinding );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_RENDERBUFFER_BINDING: %d") LINE_TERMINATOR, RenderbufferBinding );
}

void FOpenGLDebugFrameDumper::DumpHintSettings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Hints") LINE_TERMINATOR );

	{
		GLint LineSmoothHint;
		glGetIntegerv( GL_LINE_SMOOTH_HINT, &LineSmoothHint );
		ASSERT_NO_GL_ERROR();
		const TCHAR* HintName = GetHintName( LineSmoothHint );
		if( HintName )
		{
			LogFile.Logf( TEXT("\tGL_LINE_SMOOTH_HINT: %s") LINE_TERMINATOR, HintName );
		}
		else
		{
			LogFile.Logf( TEXT("\tGL_LINE_SMOOTH_HINT: 0x%x") LINE_TERMINATOR, LineSmoothHint );
		}
	}

	{
		GLint PolygonSmoothHint;
		glGetIntegerv( GL_POLYGON_SMOOTH_HINT, &PolygonSmoothHint );
		ASSERT_NO_GL_ERROR();
		const TCHAR* HintName = GetHintName( PolygonSmoothHint );
		if( HintName )
		{
			LogFile.Logf( TEXT("\tGL_POLYGON_SMOOTH_HINT: %s") LINE_TERMINATOR, HintName );
		}
		else
		{
			LogFile.Logf( TEXT("\tGL_POLYGON_SMOOTH_HINT: 0x%x") LINE_TERMINATOR, PolygonSmoothHint );
		}
	}

	{
		GLint TextureCompressionHint;
		glGetIntegerv( GL_TEXTURE_COMPRESSION_HINT, &TextureCompressionHint );
		ASSERT_NO_GL_ERROR();
		const TCHAR* HintName = GetHintName( TextureCompressionHint );
		if( HintName )
		{
			LogFile.Logf( TEXT("\tGL_TEXTURE_COMPRESSION_HINT: %s") LINE_TERMINATOR, HintName );
		}
		else
		{
			LogFile.Logf( TEXT("\tGL_TEXTURE_COMPRESSION_HINT: 0x%x") LINE_TERMINATOR, TextureCompressionHint );
		}
	}

	{
		GLint FragmentShaderDerivativeHint;
		glGetIntegerv( GL_FRAGMENT_SHADER_DERIVATIVE_HINT, &FragmentShaderDerivativeHint );
		ASSERT_NO_GL_ERROR();
		const TCHAR* HintName = GetHintName( FragmentShaderDerivativeHint );
		if( HintName )
		{
			LogFile.Logf( TEXT("\tGL_FRAGMENT_SHADER_DERIVATIVE_HINT: %s") LINE_TERMINATOR, HintName );
		}
		else
		{
			LogFile.Logf( TEXT("\tGL_FRAGMENT_SHADER_DERIVATIVE_HINT: 0x%x") LINE_TERMINATOR, FragmentShaderDerivativeHint );
		}
	}
}

void FOpenGLDebugFrameDumper::DumpOpenGLLimits( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Limits") LINE_TERMINATOR );

	GLint SubpixelBits;
	glGetIntegerv( GL_SUBPIXEL_BITS, &SubpixelBits );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_SUBPIXEL_BIT: %d") LINE_TERMINATOR, SubpixelBits );

	GLint Max3DTextureSize;
	glGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &Max3DTextureSize );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_3D_TEXTURE_SIZE: %d") LINE_TERMINATOR, Max3DTextureSize );

	GLint MaxCombinedTextureImageUnits;
	glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &MaxCombinedTextureImageUnits );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: %d") LINE_TERMINATOR, MaxCombinedTextureImageUnits );

	GLint MaxCubeMapTextureSize;
	glGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE, &MaxCubeMapTextureSize );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_CUBE_MAP_TEXTURE_SIZE: %d") LINE_TERMINATOR, MaxCubeMapTextureSize );

	GLint MaxElementsIndices;
	glGetIntegerv( GL_MAX_ELEMENTS_INDICES, &MaxElementsIndices );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_ELEMENTS_INDICES: %d") LINE_TERMINATOR, MaxElementsIndices );

	GLint MaxElementsVertices;
	glGetIntegerv( GL_MAX_ELEMENTS_VERTICES, &MaxElementsVertices );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_ELEMENTS_VERTICES: %d") LINE_TERMINATOR, MaxElementsVertices );

	GLint MaxFragmentUniformComponents;
	glGetIntegerv( GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &MaxFragmentUniformComponents );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_FRAGMENT_UNIFORM_COMPONENTS: %d") LINE_TERMINATOR, MaxFragmentUniformComponents );

	GLint MaxTextureImageUnits;
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &MaxTextureImageUnits );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_TEXTURE_IMAGE_UNITS: %d") LINE_TERMINATOR, MaxTextureImageUnits );

	GLint MaxTextureLODBias;
	glGetIntegerv( GL_MAX_TEXTURE_LOD_BIAS, &MaxTextureLODBias );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_TEXTURE_LOD_BIAS: %d") LINE_TERMINATOR, MaxTextureLODBias );

	if( FOpenGL::SupportsTextureFilterAnisotropic())
	{
		GLint MaxTextureMaxAnisotropy;
		glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &MaxTextureMaxAnisotropy );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\tGL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: %d") LINE_TERMINATOR, MaxTextureMaxAnisotropy );
	}

	GLint MaxTextureSize;
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &MaxTextureSize );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_TEXTURE_SIZE: %d") LINE_TERMINATOR, MaxTextureSize );

	GLint MaxVertexAttribs;
	glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &MaxVertexAttribs );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_VERTEX_ATTRIBS: %d") LINE_TERMINATOR, MaxVertexAttribs );
	
	GLint MaxVertexTextureImageUnits;
	glGetIntegerv( GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &MaxVertexTextureImageUnits );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: %d") LINE_TERMINATOR, MaxVertexTextureImageUnits );

	GLint MaxVertexUniformComponents;
	glGetIntegerv( GL_MAX_VERTEX_UNIFORM_COMPONENTS, &MaxVertexUniformComponents );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_VERTEX_UNIFORM_COMPONENTS: %d") LINE_TERMINATOR, MaxVertexUniformComponents );

	GLint MaxViewportDimensions[2];
	glGetIntegerv( GL_MAX_VIEWPORT_DIMS, MaxViewportDimensions );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_VIEWPORT_DIMS: { %d, %d }") LINE_TERMINATOR, MaxViewportDimensions[0], MaxViewportDimensions[1] );

	GLint NumCompressedTextureFormats;
	glGetIntegerv( GL_NUM_COMPRESSED_TEXTURE_FORMATS, &NumCompressedTextureFormats );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_NUM_COMPRESSED_TEXTURE_FORMATS: %d") LINE_TERMINATOR, NumCompressedTextureFormats );

	if( NumCompressedTextureFormats )
	{
		LogFile.Logf( TEXT("\t{") LINE_TERMINATOR );
		GLint* CompressedTextureFormatsTable = (GLint*)FMemory::Malloc( NumCompressedTextureFormats * sizeof( GLint ) );
		glGetIntegerv( GL_COMPRESSED_TEXTURE_FORMATS, CompressedTextureFormatsTable );
		ASSERT_NO_GL_ERROR();
		for( GLint CompressedTextureFormatIndex = 0; CompressedTextureFormatIndex < NumCompressedTextureFormats; ++CompressedTextureFormatIndex )
		{
			const TCHAR* FormatName = GetCompressedTextureFormatName( CompressedTextureFormatsTable[CompressedTextureFormatIndex] );
			if( FormatName )
				LogFile.Logf( TEXT("\t\t%s") LINE_TERMINATOR, FormatName );
			else
				LogFile.Logf( TEXT("\t\t0x%x") LINE_TERMINATOR, CompressedTextureFormatsTable[CompressedTextureFormatIndex] );
		}
		FMemory::Free( CompressedTextureFormatsTable );
		LogFile.Logf( TEXT("\t}") LINE_TERMINATOR );
	}
}

void FOpenGLDebugFrameDumper::DumpLinesSettings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Lines") LINE_TERMINATOR );

	GLboolean bLineSmoothEnabled;
	glGetBooleanv( GL_LINE_SMOOTH, &bLineSmoothEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_LINE_SMOOTH: %s") LINE_TERMINATOR, bLineSmoothEnabled ? TEXT("Enabled") : TEXT("Disabled") );

	GLfloat LineWidth;
	glGetFloatv( GL_LINE_WIDTH, &LineWidth );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_LINE_WIDTH: %f") LINE_TERMINATOR, LineWidth );

	GLfloat LineWidthGranularity;
	glGetFloatv( GL_LINE_WIDTH_GRANULARITY, &LineWidthGranularity );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_LINE_WIDTH_GRANULARITY: %f") LINE_TERMINATOR, LineWidthGranularity );

	GLfloat LineWidthRange[2];
	glGetFloatv( GL_LINE_WIDTH_RANGE, LineWidthRange );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_LINE_WIDTH_RANGE: { %f, %f }") LINE_TERMINATOR, LineWidthRange[0], LineWidthRange[1] );

	GLfloat AliasedLineWidthRange[2];
	glGetFloatv( GL_ALIASED_LINE_WIDTH_RANGE, AliasedLineWidthRange );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_ALIASED_LINE_WIDTH_RANGE: { %f, %f }") LINE_TERMINATOR, AliasedLineWidthRange[0], AliasedLineWidthRange[1] );
}

void FOpenGLDebugFrameDumper::DumpLogicOpsSettings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Logic Ops") LINE_TERMINATOR );

	GLboolean ColorLogicOp;
	glGetBooleanv( GL_COLOR_LOGIC_OP, &ColorLogicOp );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_COLOR_LOGIC_OP: %s") LINE_TERMINATOR, ColorLogicOp ? TEXT("Enabled") : TEXT("Disabled") );

	GLint LogicOpMode;
	glGetIntegerv( GL_LOGIC_OP_MODE, &LogicOpMode );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_LOGIC_OP_MODE: %s") LINE_TERMINATOR, GetGLLogicOpString( LogicOpMode ) );
}

void FOpenGLDebugFrameDumper::DumpPixelModeSettings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Pixel Mode") LINE_TERMINATOR );

	GLint PackAlignment;
	glGetIntegerv( GL_PACK_ALIGNMENT, &PackAlignment );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PACK_ALIGNMENT: %d") LINE_TERMINATOR, PackAlignment );

	GLint PackImageHeight;
	glGetIntegerv( GL_PACK_IMAGE_HEIGHT, &PackImageHeight );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PACK_IMAGE_HEIGHT: %d") LINE_TERMINATOR, PackImageHeight );

	GLboolean bPackLSBFirst;
	glGetBooleanv( GL_PACK_LSB_FIRST, &bPackLSBFirst );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PACK_LSB_FIRST: %s") LINE_TERMINATOR, bPackLSBFirst ? TEXT("TRUE") : TEXT("FALSE") );

	GLint PackRowLength;
	glGetIntegerv( GL_PACK_ROW_LENGTH, &PackRowLength );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PACK_ROW_LENGTH: %d") LINE_TERMINATOR, PackRowLength );

	GLint PackSkipImages;
	glGetIntegerv( GL_PACK_SKIP_IMAGES, &PackSkipImages );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PACK_SKIP_IMAGES: %d") LINE_TERMINATOR, PackSkipImages );

	GLint PackSkipPixels;
	glGetIntegerv( GL_PACK_SKIP_PIXELS, &PackSkipPixels );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PACK_SKIP_PIXELS: %d") LINE_TERMINATOR, PackSkipPixels );

	GLint PackSkipRows;
	glGetIntegerv( GL_PACK_SKIP_ROWS, &PackSkipRows );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PACK_SKIP_ROWS: %d") LINE_TERMINATOR, PackSkipRows );

	GLboolean bPackSwapBytes;
	glGetBooleanv( GL_PACK_SWAP_BYTES, &bPackSwapBytes );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_PACK_SWAP_BYTES: %s") LINE_TERMINATOR, bPackSwapBytes ? TEXT("TRUE") : TEXT("FALSE") );

	GLint UnpackAlignment;
	glGetIntegerv( GL_UNPACK_ALIGNMENT, &UnpackAlignment );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_UNPACK_ALIGNMENTS: %d") LINE_TERMINATOR, UnpackAlignment );

	GLint UnpackImageHeight;
	glGetIntegerv( GL_UNPACK_IMAGE_HEIGHT, &UnpackImageHeight );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_UNPACK_IMAGE_HEIGHT: %d") LINE_TERMINATOR, UnpackImageHeight );

	GLboolean bUnpackLSBFirst;
	glGetBooleanv( GL_UNPACK_LSB_FIRST, &bUnpackLSBFirst );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_UNPACK_LSB_FIRST: %s") LINE_TERMINATOR, bUnpackLSBFirst ? TEXT("TRUE") : TEXT("FALSE") );

	GLint UnpackRowLength;
	glGetIntegerv( GL_UNPACK_ROW_LENGTH, &UnpackRowLength );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_UNPACK_ROW_LENGTH: %d") LINE_TERMINATOR, UnpackRowLength );

	GLint UnpackSkipImages;
	glGetIntegerv( GL_UNPACK_SKIP_IMAGES, &UnpackSkipImages );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_UNPACK_SKIP_IMAGES: %d") LINE_TERMINATOR, UnpackSkipImages );

	GLint UnpackSkipRows;
	glGetIntegerv( GL_UNPACK_SKIP_ROWS, &UnpackSkipRows );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_UNPACK_SKIP_ROWS: %d") LINE_TERMINATOR, UnpackSkipRows );

	GLboolean bUnpackSwapBytes;
	glGetBooleanv( GL_UNPACK_SWAP_BYTES, &bUnpackSwapBytes );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_UNPACK_SWAP_BYTES: %s") LINE_TERMINATOR, bUnpackSwapBytes ? TEXT("TRUE") : TEXT("FALSE") );
}

void FOpenGLDebugFrameDumper::DumpPointsSettings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Points") LINE_TERMINATOR );

	GLfloat PointSize;
	glGetFloatv( GL_POINT_SIZE, &PointSize );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_POINT_SIZE: %f") LINE_TERMINATOR, PointSize );

	GLfloat PointSizeRange[2];
	glGetFloatv( GL_POINT_SIZE_RANGE, PointSizeRange );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_POINT_SIZE_RANGE: { %f, %f }") LINE_TERMINATOR, PointSizeRange[0], PointSizeRange[1] );
}

void FOpenGLDebugFrameDumper::DumpPolygonsSettings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Polygons") LINE_TERMINATOR );

	GLfloat PolygonOffsetFactor;
	glGetFloatv( GL_POLYGON_OFFSET_FACTOR, &PolygonOffsetFactor );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_POLYGON_OFFSET_FACTOR: %f") LINE_TERMINATOR, PolygonOffsetFactor );

	GLboolean bCullFaceEnabled;
	glGetBooleanv( GL_CULL_FACE, &bCullFaceEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_CULL_FACE: %s") LINE_TERMINATOR, bCullFaceEnabled ? TEXT("TRUE") : TEXT("FALSE") );

	GLint CullFaceMode;
	glGetIntegerv( GL_CULL_FACE_MODE, &CullFaceMode );
	ASSERT_NO_GL_ERROR();
	const TCHAR* CullFaceModeName = GetCullFaceModeName( CullFaceMode );
	if( CullFaceModeName )
		LogFile.Logf( TEXT("\tGL_CULL_FACE_MODE: %s") LINE_TERMINATOR, CullFaceModeName );
	else
		LogFile.Logf( TEXT("\tGL_CULL_FACE_MODE: 0x%x") LINE_TERMINATOR, CullFaceMode );

	GLint FrontFace;
	glGetIntegerv( GL_FRONT_FACE, &FrontFace );
	ASSERT_NO_GL_ERROR();
	const TCHAR* FrontFaceName = GetFrontFaceName( FrontFace );
	if( FrontFaceName )
		LogFile.Logf( TEXT("\tGL_FRONT_FACE: %s") LINE_TERMINATOR, FrontFaceName );
	else
		LogFile.Logf( TEXT("\tGL_FRONT_FACE: 0x%x") LINE_TERMINATOR, FrontFace );

	GLboolean bPolygonOffsetFillEnabled;
	glGetBooleanv( GL_POLYGON_OFFSET_FILL, &bPolygonOffsetFillEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_POLYGON_OFFSET_FILL: %s") LINE_TERMINATOR, bPolygonOffsetFillEnabled ? TEXT("Enabled") : TEXT("Disabled") );

	GLboolean bPolygonOffsetLineEnabled;
	glGetBooleanv( GL_POLYGON_OFFSET_LINE, &bPolygonOffsetLineEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_POLYGON_OFFSET_LINE: %s") LINE_TERMINATOR, bPolygonOffsetLineEnabled ? TEXT("Enabled") : TEXT("Disabled") );

	GLboolean bPolygonOffsetPointEnabled;
	glGetBooleanv( GL_POLYGON_OFFSET_POINT, &bPolygonOffsetPointEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_POLYGON_OFFSET_POINT: %s") LINE_TERMINATOR, bPolygonOffsetPointEnabled ? TEXT("Enabled") : TEXT("Disabled") );

	GLfloat PolygonOffsetUnits;
	glGetFloatv( GL_POLYGON_OFFSET_UNITS, &PolygonOffsetUnits );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_POLYGON_OFFSET_UNITS: %f") LINE_TERMINATOR, PolygonOffsetUnits );

	GLboolean bPolygonSmoothEnabled;
	glGetBooleanv( GL_POLYGON_SMOOTH, &bPolygonSmoothEnabled );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_POLYGON_SMOOTH: %s") LINE_TERMINATOR, bPolygonSmoothEnabled ? TEXT("Enabled") : TEXT("Disabled") );
}

void FOpenGLDebugFrameDumper::DumpTextureLimitsAndBindings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Texture Limits And Bindings") LINE_TERMINATOR );

	GLint ActiveTextureUnit;
	glGetIntegerv( GL_ACTIVE_TEXTURE, &ActiveTextureUnit );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_ACTIVE_TEXTURE: %d") LINE_TERMINATOR, ActiveTextureUnit-GL_TEXTURE0 );

	GLint MaxTextureImageUnits;
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &MaxTextureImageUnits );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_TEXTURE_IMAGE_UNITS: %d") LINE_TERMINATOR, MaxTextureImageUnits );

	GLint MaxVertexTextureImageUnits;
	glGetIntegerv( GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &MaxVertexTextureImageUnits );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: %d") LINE_TERMINATOR, MaxVertexTextureImageUnits );

	for( GLint TextureImageUnitIndex = 0; TextureImageUnitIndex < MaxTextureImageUnits; ++TextureImageUnitIndex )
	{
		glActiveTexture( GL_TEXTURE0+TextureImageUnitIndex );
		ASSERT_NO_GL_ERROR();

		GLint TextureBinding1D;
		glGetIntegerv( GL_TEXTURE_BINDING_1D, &TextureBinding1D );
		ASSERT_NO_GL_ERROR();
		if( TextureBinding1D )
		{
			LogFile.Logf( TEXT("\t\tUnit %2d : GL_TEXTURE_BINDING_1D: %d") LINE_TERMINATOR, TextureImageUnitIndex, TextureBinding1D );
		}

		GLint TextureBinding2D;
		glGetIntegerv( GL_TEXTURE_BINDING_2D, &TextureBinding2D );
		ASSERT_NO_GL_ERROR();
		if( TextureBinding2D )
		{
			LogFile.Logf( TEXT("\t\tUnit %2d : GL_TEXTURE_BINDING_2D: %d") LINE_TERMINATOR, TextureImageUnitIndex, TextureBinding2D );
		}

		GLint TextureBinding3D;
		glGetIntegerv( GL_TEXTURE_BINDING_3D, &TextureBinding3D );
		ASSERT_NO_GL_ERROR();
		if( TextureBinding3D )
		{
			LogFile.Logf( TEXT("\t\tUnit %2d : GL_TEXTURE_BINDING_3D: %d") LINE_TERMINATOR, TextureImageUnitIndex, TextureBinding3D );
		}

		GLint TextureBindingCubeMap;
		glGetIntegerv( GL_TEXTURE_BINDING_CUBE_MAP, &TextureBindingCubeMap );
		ASSERT_NO_GL_ERROR();
		if( TextureBindingCubeMap )
		{
			LogFile.Logf( TEXT("\t\tUnit %2d : GL_TEXTURE_BINDING_CUBE_MAP: %d") LINE_TERMINATOR, TextureImageUnitIndex, TextureBindingCubeMap );
		}
	}
	glActiveTexture( ActiveTextureUnit );
	ASSERT_NO_GL_ERROR();
}

void FOpenGLDebugFrameDumper::DumpProgramSettings( FOutputDeviceFile& LogFile )
{
	LogFile.Log( TEXT("Program") LINE_TERMINATOR );

	GLint CurrentProgram;
	glGetIntegerv( GL_CURRENT_PROGRAM, &CurrentProgram );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_CURRENT_PROGRAM: %d") LINE_TERMINATOR, CurrentProgram );
}

void FOpenGLDebugFrameDumper::DumpRenderbufferSettings( FOutputDeviceFile& LogFile, GLint RenderbufferID )
{
	GLint CurrentlyBoundRenderbuffer;
	glGetIntegerv( GL_RENDERBUFFER_BINDING, &CurrentlyBoundRenderbuffer );
	ASSERT_NO_GL_ERROR();

	GLboolean bIsRenderbuffer = glIsRenderbuffer( RenderbufferID );
	ASSERT_NO_GL_ERROR();
	if( !bIsRenderbuffer )
	{
		LogFile.Logf( TEXT("\t\t\tRenderbuffer ID %d is not a valid renderbuffer ID!") LINE_TERMINATOR, RenderbufferID );
		return;
	}

	if( RenderbufferID != CurrentlyBoundRenderbuffer )
	{
		glBindRenderbuffer( GL_RENDERBUFFER, RenderbufferID );
		ASSERT_NO_GL_ERROR();
	}

	LogFile.Logf( TEXT("\t\t\tRenderbuffer object %d info") LINE_TERMINATOR, RenderbufferID );

	GLint Width;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &Width );
	ASSERT_NO_GL_ERROR();

	GLint Height;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &Height );
	ASSERT_NO_GL_ERROR();

	GLint Format;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &Format );
	ASSERT_NO_GL_ERROR();

	GLint RedSize;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE, &RedSize );
	ASSERT_NO_GL_ERROR();

	GLint GreenSize;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_GREEN_SIZE, &GreenSize );
	ASSERT_NO_GL_ERROR();

	GLint BlueSize;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_BLUE_SIZE, &BlueSize );
	ASSERT_NO_GL_ERROR();

	GLint AlphaSize;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE, &AlphaSize );
	ASSERT_NO_GL_ERROR();

	GLint DepthSize;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &DepthSize );
	ASSERT_NO_GL_ERROR();

	GLint StencilSize;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_STENCIL_SIZE, &StencilSize );
	ASSERT_NO_GL_ERROR();

	GLint Samples;
	glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &Samples );
	ASSERT_NO_GL_ERROR();

	FString RenderbufferInfo = FString::Printf( TEXT("\t\t%d x %d, format: %s, samples: %d"), Width, Height, GetGLInternalFormatString( Format ), Samples );

	if( RedSize )
	{
		RenderbufferInfo += FString::Printf( TEXT(", red: %d"), RedSize );
	}

	if( GreenSize )
	{
		RenderbufferInfo += FString::Printf( TEXT(", green: %d"), GreenSize );
	}

	if( BlueSize )
	{
		RenderbufferInfo += FString::Printf( TEXT(", blue: %d"), BlueSize );
	}

	if( AlphaSize )
	{
		RenderbufferInfo += FString::Printf( TEXT(", alpha: %d"), AlphaSize );
	}

	if( DepthSize )
	{
		RenderbufferInfo += FString::Printf( TEXT(", depth: %d"), DepthSize );
	}

	if( StencilSize )
	{
		RenderbufferInfo += FString::Printf( TEXT(", stencil: %d"), StencilSize );
	}

	RenderbufferInfo += LINE_TERMINATOR;

	LogFile.Log( RenderbufferInfo );

	// Restore previous state
	if( RenderbufferID != CurrentlyBoundRenderbuffer )
	{
		glBindRenderbuffer( GL_RENDERBUFFER, CurrentlyBoundRenderbuffer );
	}
}

void FOpenGLDebugFrameDumper::DumpFramebufferAttachmentSettings( FOutputDeviceFile& LogFile, GLenum AttachmentSlot )
{
	GLint AttachmentType;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &AttachmentType );
	ASSERT_NO_GL_ERROR();
	if( AttachmentType == GL_NONE )
		return;	// nothing attached; no comment

	GLint AttachmentName;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &AttachmentName );
	ASSERT_NO_GL_ERROR();

	if( AttachmentType == GL_FRAMEBUFFER_DEFAULT )
	{
		LogFile.Logf( TEXT("\t\tattachment %s is default framebuffer attachment ( name is %d )") LINE_TERMINATOR, GetAttachmentSlotName( AttachmentSlot ), AttachmentName );
	}
	else if( AttachmentType == GL_TEXTURE )
	{
		GLint TextureLevel;
		glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &TextureLevel );
		ASSERT_NO_GL_ERROR();

		GLint CubeMapFace;
		glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, &CubeMapFace );
		ASSERT_NO_GL_ERROR();

		GLint TextureLayer;
		glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &TextureLayer );
		ASSERT_NO_GL_ERROR();

		GLint IsLayered;
		glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_LAYERED, &IsLayered );
		ASSERT_NO_GL_ERROR();

		GLenum TextureTypeAsk;
		GLenum TextureTypeSet;
		GLenum TextureTypeFace;
		const TCHAR* TextureType;

		bool bIsCube = ( CubeMapFace != 0 );
		if( bIsCube )
		{
			TextureTypeAsk = GL_TEXTURE_BINDING_CUBE_MAP;
			TextureTypeSet = GL_TEXTURE_CUBE_MAP;
			TextureTypeFace = CubeMapFace;
			TextureType = TEXT("cube map");
		}
		else
		{
			TextureTypeAsk = GL_TEXTURE_BINDING_2D;
			TextureTypeSet = GL_TEXTURE_2D;
			TextureTypeFace = GL_TEXTURE_2D;
			TextureType = TEXT("2D");
		}

		// Remember what's bound to the texture stage we need to use, to restore it later
		GLint BoundTextureID;
		glGetIntegerv( TextureTypeAsk, &BoundTextureID );

		// Bind our texture. Was it GL_TEXTURE_2D?
		ASSERT_NO_GL_ERROR();
		GDisableOpenGLDebugOutput = true;
		glBindTexture( TextureTypeSet, AttachmentName );
		glFinish();
		GDisableOpenGLDebugOutput = false;
		if( glGetError() )
		{
			// It could be GL_TEXTURE_2D_MULTISAMPLE then?
			check(TextureTypeSet == GL_TEXTURE_2D);
			check(TextureLevel == 0);
			TextureTypeAsk = GL_TEXTURE_BINDING_2D_MULTISAMPLE;
			TextureTypeSet = GL_TEXTURE_2D_MULTISAMPLE;
			TextureTypeFace = GL_TEXTURE_2D_MULTISAMPLE;
			TextureType = TEXT("2D multisample");
			glGetIntegerv( TextureTypeAsk, &BoundTextureID );
			ASSERT_NO_GL_ERROR();
			GDisableOpenGLDebugOutput = true;
			glBindTexture( TextureTypeSet, AttachmentName );
			glFinish();
			GDisableOpenGLDebugOutput = false;
			if( glGetError() )
			{
				// Ok, then GL_TEXTURE_3D?
				TextureTypeAsk = GL_TEXTURE_BINDING_3D;
				TextureTypeSet = GL_TEXTURE_3D;
				TextureTypeFace = GL_TEXTURE_3D;
				TextureType = TEXT("3D");
				glGetIntegerv( TextureTypeAsk, &BoundTextureID );
				ASSERT_NO_GL_ERROR();
				glBindTexture( TextureTypeSet, AttachmentName );
				ASSERT_NO_GL_ERROR();
			}
		}

		// Get the information we need
		GLint Width;
		glGetTexLevelParameteriv( TextureTypeFace, TextureLevel, GL_TEXTURE_WIDTH, &Width );
		ASSERT_NO_GL_ERROR();

		GLint Height;
		glGetTexLevelParameteriv( TextureTypeFace, TextureLevel, GL_TEXTURE_HEIGHT, &Height );
		ASSERT_NO_GL_ERROR();

		GLint Depth;
		glGetTexLevelParameteriv( TextureTypeFace, TextureLevel, GL_TEXTURE_DEPTH, &Depth );
		ASSERT_NO_GL_ERROR();

		GLint InternalFormat;
		glGetTexLevelParameteriv( TextureTypeFace, TextureLevel, GL_TEXTURE_INTERNAL_FORMAT, &InternalFormat );
		ASSERT_NO_GL_ERROR();

		// Restore previous binding to this texture stage
		if( BoundTextureID != AttachmentName )
		{
			glBindTexture( TextureTypeSet, BoundTextureID );
			ASSERT_NO_GL_ERROR();
		}

		LogFile.Logf( TEXT("\t\tattachment %s is a %s texture ( ID %d, level %d, %d x %d x %d, %s%s )") LINE_TERMINATOR,
			GetAttachmentSlotName( AttachmentSlot ), TextureType, AttachmentName, TextureLevel, Width, Height, Depth,
			GetGLInternalFormatString( InternalFormat ), IsLayered ? TEXT(", layered") : TEXT("") );

		if( CubeMapFace != 0 && CubeMapFace != GL_TEXTURE_CUBE_MAP )
		{
			LogFile.Logf( TEXT("\t\t\tcube map face: %s") LINE_TERMINATOR, GetCubeMapFaceName( CubeMapFace ) );
		}
	}
	else if( AttachmentType == GL_RENDERBUFFER )
	{
		LogFile.Logf( TEXT("\t\tattachment %s is a renderbuffer ( ID %d )") LINE_TERMINATOR, GetAttachmentSlotName( AttachmentSlot ), AttachmentName );
		DumpRenderbufferSettings( LogFile, AttachmentName );
	}

	GLint RedSize;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &RedSize );
	ASSERT_NO_GL_ERROR();

	GLint GreenSize;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &GreenSize );
	ASSERT_NO_GL_ERROR();

	GLint BlueSize;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &BlueSize );
	ASSERT_NO_GL_ERROR();

	GLint AlphaSize;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &AlphaSize );
	ASSERT_NO_GL_ERROR();

	GLint DepthSize;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &DepthSize );
	ASSERT_NO_GL_ERROR();

	GLint StencilSize;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &StencilSize );
	ASSERT_NO_GL_ERROR();

	GLint ComponentType;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &ComponentType );
	ASSERT_NO_GL_ERROR();

	GLint ColorEncoding;
	glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &ColorEncoding );
	ASSERT_NO_GL_ERROR();

	{
		FString ComponentInfo = FString::Printf( TEXT("\t\t\tComponent type: %s, color encoding: %s"), GetComponentType( ComponentType ), GetColorEncoding( ColorEncoding ) );

		if( RedSize )
		{
			ComponentInfo += FString::Printf( TEXT(", red: %d"), RedSize );
		}

		if( GreenSize )
		{
			ComponentInfo += FString::Printf( TEXT(", green: %d"), GreenSize );
		}

		if( BlueSize )
		{
			ComponentInfo += FString::Printf( TEXT(", blue: %d"), BlueSize );
		}

		if( AlphaSize )
		{
			ComponentInfo += FString::Printf( TEXT(", alpha: %d"), AlphaSize );
		}

		if( DepthSize )
		{
			ComponentInfo += FString::Printf( TEXT(", depth: %d"), DepthSize );
		}

		if( StencilSize )
		{
			ComponentInfo += FString::Printf( TEXT(", stencil: %d"), StencilSize );
		}

		ComponentInfo += LINE_TERMINATOR;

		LogFile.Log( ComponentInfo );
	}
}

void FOpenGLDebugFrameDumper::DumpFramebufferSettings( FOutputDeviceFile& LogFile, GLint FramebufferID )
{
	LogFile.Log( TEXT("Framebuffer State") LINE_TERMINATOR );

	GLint CurrentlyBoundDrawFramebuffer;
	glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &CurrentlyBoundDrawFramebuffer );
	ASSERT_NO_GL_ERROR();

	if( FramebufferID )
	{
		GLboolean bIsFramebuffer = glIsFramebuffer( FramebufferID );
		ASSERT_NO_GL_ERROR();

		if( !bIsFramebuffer )
		{
			LogFile.Logf( TEXT("\tFramebuffer ID %d is not a valid framebuffer ID! ( %d )") LINE_TERMINATOR, FramebufferID );
			return;
		}
	}

	if( FramebufferID != CurrentlyBoundDrawFramebuffer )
	{
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, FramebufferID );
		ASSERT_NO_GL_ERROR();
	}

	if( !FramebufferID )
	{
		LogFile.Log( TEXT("\tFramebuffer object 0 (screen buffer)") LINE_TERMINATOR );
		DumpFramebufferAttachmentSettings( LogFile, GL_FRONT_LEFT );
		DumpFramebufferAttachmentSettings( LogFile, GL_FRONT_RIGHT );
		DumpFramebufferAttachmentSettings( LogFile, GL_BACK_LEFT );
		DumpFramebufferAttachmentSettings( LogFile, GL_BACK_RIGHT );
		DumpFramebufferAttachmentSettings( LogFile, GL_DEPTH );
		DumpFramebufferAttachmentSettings( LogFile, GL_STENCIL );
	}
	else
	{
		LogFile.Logf( TEXT("\tFramebuffer object %d info") LINE_TERMINATOR, FramebufferID );

		GLint MaxColorAttachments;
		glGetIntegerv( GL_MAX_COLOR_ATTACHMENTS, &MaxColorAttachments );
		ASSERT_NO_GL_ERROR();
		for( GLint ColorAttachmentIndex = 0; ColorAttachmentIndex < MaxColorAttachments; ++ColorAttachmentIndex )
		{
			DumpFramebufferAttachmentSettings( LogFile, GL_COLOR_ATTACHMENT0 + ColorAttachmentIndex );
		}
		DumpFramebufferAttachmentSettings( LogFile, GL_DEPTH_ATTACHMENT );
		DumpFramebufferAttachmentSettings( LogFile, GL_STENCIL_ATTACHMENT );
	}

	// Restore previous state
	if( FramebufferID != CurrentlyBoundDrawFramebuffer )
	{
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, CurrentlyBoundDrawFramebuffer );
		ASSERT_NO_GL_ERROR();
	}
}

void FOpenGLDebugFrameDumper::InterpretUniform( GLint UniformType, void* DataBuffer, FString& OutAppendString )
{
	switch( UniformType )
	{
	case GL_FLOAT:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "%f"), Fields[0] );
		}
		break;

	case GL_FLOAT_VEC2:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %f, %f }"), Fields[0], Fields[1] );
		}
		break;

	case GL_FLOAT_VEC3:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %f, %f, %f }"), Fields[0], Fields[1], Fields[2] );
		}
		break;

	case GL_FLOAT_VEC4:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %f, %f, %f, %f }"), Fields[0], Fields[1], Fields[2], Fields[3] );
		}
		break;

	case GL_SAMPLER_1D:
	case GL_SAMPLER_2D:
	case GL_SAMPLER_3D:
	case GL_SAMPLER_CUBE:
	case GL_SAMPLER_1D_SHADOW:
	case GL_SAMPLER_2D_SHADOW:
	case GL_SAMPLER_1D_ARRAY:
	case GL_SAMPLER_2D_ARRAY:
	case GL_SAMPLER_1D_ARRAY_SHADOW:
	case GL_SAMPLER_2D_ARRAY_SHADOW:
	case GL_SAMPLER_2D_MULTISAMPLE:
	case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
	case GL_SAMPLER_CUBE_SHADOW:
	case GL_SAMPLER_BUFFER:
	case GL_SAMPLER_2D_RECT:
	case GL_SAMPLER_2D_RECT_SHADOW:
	case GL_INT_SAMPLER_1D:
	case GL_INT_SAMPLER_2D:
	case GL_INT_SAMPLER_3D:
	case GL_INT_SAMPLER_CUBE:
	case GL_INT_SAMPLER_1D_ARRAY:
	case GL_INT_SAMPLER_2D_ARRAY:
	case GL_INT_SAMPLER_2D_MULTISAMPLE:
	case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
	case GL_INT_SAMPLER_BUFFER:
	case GL_INT_SAMPLER_2D_RECT:
	case GL_UNSIGNED_INT_SAMPLER_1D:
	case GL_UNSIGNED_INT_SAMPLER_2D:
	case GL_UNSIGNED_INT_SAMPLER_3D:
	case GL_UNSIGNED_INT_SAMPLER_CUBE:
	case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
	case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
	case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
	case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
	case GL_UNSIGNED_INT_SAMPLER_BUFFER:
	case GL_UNSIGNED_INT_SAMPLER_2D_RECT:

	case GL_INT:
		{
			int32* Fields = (int32*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "%d"), Fields[0] );
		}
		break;

	case GL_INT_VEC2:
		{
			int32* Fields = (int32*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %d, %d }"), Fields[0], Fields[1] );
		}
		break;

	case GL_INT_VEC3:
		{
			int32* Fields = (int32*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %d, %d, %d }"), Fields[0], Fields[1], Fields[2] );
		}
		break;

	case GL_INT_VEC4:
		{
			int32* Fields = (int32*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %d, %d, %d, %d }"), Fields[0], Fields[1], Fields[2], Fields[3] );
		}
		break;

	case GL_UNSIGNED_INT:
		{
			uint32* Fields = (uint32*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "%u"), Fields[0] );
		}
		break;

	case GL_UNSIGNED_INT_VEC2:
		{
			uint32* Fields = (uint32*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %u, %u }"), Fields[0], Fields[1] );
		}
		break;

	case GL_UNSIGNED_INT_VEC3:
		{
			uint32* Fields = (uint32*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %u, %u, %u }"), Fields[0], Fields[1], Fields[2] );
		}
		break;

	case GL_UNSIGNED_INT_VEC4:
		{
			uint32* Fields = (uint32*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %u, %u, %u, %u }"), Fields[0], Fields[1], Fields[2], Fields[3] );
		}
		break;

	case GL_BOOL:
		{
			GLboolean* Fields = (GLboolean*)DataBuffer;
			OutAppendString += Fields[0] ? TEXT("TRUE") : TEXT("FALSE");
		}
		break;

	case GL_BOOL_VEC2:
		{
			GLboolean* Fields = (GLboolean*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %s, %s }"),
				Fields[0] ? TEXT("TRUE") : TEXT("FALSE"),
				Fields[1] ? TEXT("TRUE") : TEXT("FALSE")
			);
		}
		break;

	case GL_BOOL_VEC3:
		{
			GLboolean* Fields = (GLboolean*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %s, %s, %s }"),
				Fields[0] ? TEXT("TRUE") : TEXT("FALSE"),
				Fields[1] ? TEXT("TRUE") : TEXT("FALSE"),
				Fields[2] ? TEXT("TRUE") : TEXT("FALSE")
			);
		}
		break;

	case GL_BOOL_VEC4:
		{
			GLboolean* Fields = (GLboolean*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ %s, %s, %s, %s }"),
				Fields[0] ? TEXT("TRUE") : TEXT("FALSE"),
				Fields[1] ? TEXT("TRUE") : TEXT("FALSE"),
				Fields[2] ? TEXT("TRUE") : TEXT("FALSE"),
				Fields[3] ? TEXT("TRUE") : TEXT("FALSE")
			);
		}
		break;

	case GL_FLOAT_MAT2:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ { %f, %f }, { %f, %f } }"), Fields[0], Fields[1], Fields[2], Fields[3] );
		}
		break;

	case GL_FLOAT_MAT3:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ { %f, %f, %f }, { %f, %f, %f }, { %f, %f, %f } }"),
				Fields[0], Fields[1], Fields[2], Fields[3], Fields[4], Fields[5], Fields[6], Fields[7], Fields[8] );
		}
		break;

	case GL_FLOAT_MAT4:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ { %f, %f, %f, %f }, { %f, %f, %f, %f }, { %f, %f, %f, %f }, { %f, %f, %f, %f } }"),
				Fields[0], Fields[1], Fields[2], Fields[3], Fields[4], Fields[5], Fields[6], Fields[7],
				Fields[8], Fields[9], Fields[10], Fields[11], Fields[12], Fields[13], Fields[14], Fields[15]
			);
		}
		break;

	case GL_FLOAT_MAT2x3:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ { %f, %f }, { %f, %f }, { %f, %f } }"),
				Fields[0], Fields[1], Fields[2], Fields[3], Fields[4], Fields[5] );
		}
		break;

	case GL_FLOAT_MAT2x4:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ { %f, %f }, { %f, %f }, { %f, %f }, { %f, %f } }"),
				Fields[0], Fields[1], Fields[2], Fields[3], Fields[4], Fields[5], Fields[6], Fields[7] );
		}
		break;

	case GL_FLOAT_MAT3x2:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ { %f, %f, %f }, { %f, %f, %f } }"),
				Fields[0], Fields[1], Fields[2], Fields[3], Fields[4], Fields[5] );
		}
		break;

	case GL_FLOAT_MAT3x4:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ { %f, %f, %f }, { %f, %f, %f }, { %f, %f, %f }, { %f, %f, %f } }"),
				Fields[0], Fields[1], Fields[2], Fields[3], Fields[4], Fields[5], Fields[6], Fields[7],
				Fields[8], Fields[9], Fields[10], Fields[11]
			);
		}
		break;

	case GL_FLOAT_MAT4x2:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ { %f, %f, %f, %f }, { %f, %f, %f, %f } }"),
				Fields[0], Fields[1], Fields[2], Fields[3], Fields[4], Fields[5], Fields[6], Fields[7] );
		}
		break;

	case GL_FLOAT_MAT4x3:
		{
			float* Fields = (float*)DataBuffer;
			OutAppendString += FString::Printf( TEXT( "{ { %f, %f, %f, %f }, { %f, %f, %f, %f }, { %f, %f, %f, %f } }"),
				Fields[0], Fields[1], Fields[2], Fields[3], Fields[4], Fields[5], Fields[6], Fields[7],
				Fields[8], Fields[9], Fields[10], Fields[11]
			);
		}
		break;

	default:
		OutAppendString += TEXT("!!!unknown!!!");
		break;
	};
}

void FOpenGLDebugFrameDumper::DumpProgramContents( FOutputDeviceFile& LogFile, GLint ProgramID )
{
	GLboolean bIsProgram = glIsProgram( ProgramID );
	ASSERT_NO_GL_ERROR();

	if( !bIsProgram )
	{
		LogFile.Logf( TEXT("Program ID %d is not a valid program ID!") LINE_TERMINATOR, ProgramID );
		return;
	}

	LogFile.Logf( TEXT("Program %d info") LINE_TERMINATOR, ProgramID );

	{
		GLint Status;
		glGetProgramiv( ProgramID, GL_DELETE_STATUS, &Status );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\tGL_DELETE_STATUS: %s") LINE_TERMINATOR, Status ? TEXT("TRUE") : TEXT("FALSE") );
	}

	{
		GLint Status;
		glGetProgramiv( ProgramID, GL_LINK_STATUS, &Status );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\tGL_LINK_STATUS: %s") LINE_TERMINATOR, Status ? TEXT("TRUE") : TEXT("FALSE") );
	}

	{
		GLint Status;
		glGetProgramiv( ProgramID, GL_VALIDATE_STATUS, &Status );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\tGL_VALIDATE_STATUS: %s") LINE_TERMINATOR, Status ? TEXT("TRUE") : TEXT("FALSE") );
	}

	GLint AttachedShaderCount;
	glGetProgramiv( ProgramID, GL_ATTACHED_SHADERS, &AttachedShaderCount );
	ASSERT_NO_GL_ERROR();
	if( AttachedShaderCount )
	{
		GLsizei CountReceived = 0;
		GLuint* AttachedShadersTable = (GLuint*)FMemory::Malloc( sizeof(GLuint)*AttachedShaderCount );
		glGetAttachedShaders( ProgramID, AttachedShaderCount, &CountReceived, AttachedShadersTable );
		ASSERT_NO_GL_ERROR();

		FString ShaderNumbers = TEXT("");
		for( GLint ShaderIndex = 0; ShaderIndex < CountReceived; ++ShaderIndex )
		{
			ShaderNumbers += FString::Printf( TEXT("%s%u"), ShaderIndex ? TEXT(", ") : TEXT(""), AttachedShadersTable[ShaderIndex] );
		}

		LogFile.Logf( TEXT("\tAttached shaders: %d ( %s )") LINE_TERMINATOR, AttachedShaderCount, *ShaderNumbers );
		FMemory::Free( AttachedShadersTable );
	}

	// Attributes
	GLint ActiveAttributesCount;
	glGetProgramiv( ProgramID, GL_ACTIVE_ATTRIBUTES, &ActiveAttributesCount );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tActive attributes: %d") LINE_TERMINATOR, ActiveAttributesCount );

	if( ActiveAttributesCount )
	{
		GLint MaxActiveAttributeNameLength;
		glGetProgramiv( ProgramID, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &MaxActiveAttributeNameLength );
		ASSERT_NO_GL_ERROR();

		if( MaxActiveAttributeNameLength )
		{
			ANSICHAR* ActiveAttributeName = (ANSICHAR*)FMemory::Malloc( sizeof(ANSICHAR)*(MaxActiveAttributeNameLength+1) );
			ActiveAttributeName[MaxActiveAttributeNameLength] = 0;

			for( GLint AttributeIndex = 0; AttributeIndex < ActiveAttributesCount; ++AttributeIndex )
			{
				GLsizei NameLength = 0;
				GLint Size = 0;
				GLenum Type = 0;
				glGetActiveAttrib( ProgramID, AttributeIndex, MaxActiveAttributeNameLength+1, &NameLength, &Size, &Type, ActiveAttributeName );
				ASSERT_NO_GL_ERROR();

				GLint AttributeLocation = glGetAttribLocation( ProgramID, ActiveAttributeName );
				ASSERT_NO_GL_ERROR();

				FString ActiveAttributeNameString( ActiveAttributeName );

				LogFile.Logf( TEXT("\t%04d: %s ( type %s, location %d, size %d )") LINE_TERMINATOR, AttributeIndex, *ActiveAttributeNameString, GetGLUniformTypeString( Type ), AttributeLocation, Size );
			}

			FMemory::Free( ActiveAttributeName );
		}
	}

	// Uniforms
	GLint ActiveUniformCount;
	glGetProgramiv( ProgramID, GL_ACTIVE_UNIFORMS, &ActiveUniformCount );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tActive uniforms: %d") LINE_TERMINATOR, ActiveUniformCount );

	if( ActiveUniformCount )
	{
		GLint MaxActiveUniformNameLength;
		glGetProgramiv( ProgramID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &MaxActiveUniformNameLength );
		ASSERT_NO_GL_ERROR();
		if( MaxActiveUniformNameLength )
		{
			ANSICHAR* ActiveUniformName = (ANSICHAR*)FMemory::Malloc( sizeof(ANSICHAR)*(MaxActiveUniformNameLength+1) );
			ActiveUniformName[MaxActiveUniformNameLength] = 0;

			for( GLint ActiveUniformIndex = 0; ActiveUniformIndex < ActiveUniformCount; ++ActiveUniformIndex )
			{
				GLsizei NameLengthReceived;
				glGetActiveUniformName( ProgramID, ActiveUniformIndex, MaxActiveUniformNameLength+1, &NameLengthReceived, ActiveUniformName );
				ASSERT_NO_GL_ERROR();

				GLuint TempUniformIndex = ActiveUniformIndex;

				GLint UniformType;
				glGetActiveUniformsiv( ProgramID, 1, &TempUniformIndex, GL_UNIFORM_TYPE, &UniformType );
				ASSERT_NO_GL_ERROR();

				GLint UniformSize;
				glGetActiveUniformsiv( ProgramID, 1, &TempUniformIndex, GL_UNIFORM_SIZE, &UniformSize );
				ASSERT_NO_GL_ERROR();

				GLint UniformIsRowMajor;
				glGetActiveUniformsiv( ProgramID, 1, &TempUniformIndex, GL_UNIFORM_IS_ROW_MAJOR, &UniformIsRowMajor );
				ASSERT_NO_GL_ERROR();

				FString ActiveUniformNameString( ActiveUniformName );

				LogFile.Logf( TEXT("\t%04d: %s ( type %s )") LINE_TERMINATOR, ActiveUniformIndex, *ActiveUniformNameString, GetGLUniformTypeString( UniformType ) );

				GLint UniformBlockIndex;
				glGetActiveUniformsiv( ProgramID, 1, &TempUniformIndex, GL_UNIFORM_BLOCK_INDEX, &UniformBlockIndex );
				ASSERT_NO_GL_ERROR();

				if( UniformBlockIndex == -1 )	// default uniform
				{
					GLint UniformOffset = glGetUniformLocation( ProgramID, ActiveUniformName );
					ASSERT_NO_GL_ERROR();
					LogFile.Logf( TEXT("\t      ( size %d, DEFAULT uniform block  : location %d, is row major %d )") LINE_TERMINATOR,
						UniformSize, UniformOffset, UniformIsRowMajor );

					for( int32 UniformArrayMemberIndex = 0; UniformArrayMemberIndex < UniformSize; ++UniformArrayMemberIndex )
					{
						float DataBuffer[16];	// so it can fit all types
						switch( UniformType )
						{
						case GL_FLOAT:
						case GL_FLOAT_VEC2:
						case GL_FLOAT_VEC3:
						case GL_FLOAT_VEC4:
						case GL_FLOAT_MAT2:
						case GL_FLOAT_MAT3:
						case GL_FLOAT_MAT4:
						case GL_FLOAT_MAT2x3:
						case GL_FLOAT_MAT2x4:
						case GL_FLOAT_MAT3x2:
						case GL_FLOAT_MAT3x4:
						case GL_FLOAT_MAT4x2:
						case GL_FLOAT_MAT4x3:
							glGetUniformfv( ProgramID, UniformOffset, DataBuffer );
							ASSERT_NO_GL_ERROR();
							break;

						case GL_UNSIGNED_INT:
						case GL_UNSIGNED_INT_VEC2:
						case GL_UNSIGNED_INT_VEC3:
						case GL_UNSIGNED_INT_VEC4:
							glGetUniformuiv( ProgramID, UniformOffset, (GLuint*)DataBuffer );
							ASSERT_NO_GL_ERROR();
							break;

						default:
							glGetUniformiv( ProgramID, UniformOffset, (GLint*)DataBuffer );
							ASSERT_NO_GL_ERROR();
							break;
						}

						FString Line = TEXT("\t    ");
						InterpretUniform( UniformType, DataBuffer, Line );
						Line += LINE_TERMINATOR;
						LogFile.Log( Line );

						++UniformOffset;
					}
				}
				else
				{
					GLint UniformOffset;
					glGetActiveUniformsiv( ProgramID, 1, &TempUniformIndex, GL_UNIFORM_OFFSET, &UniformOffset );
					ASSERT_NO_GL_ERROR();

					GLint UniformArrayStride;
					glGetActiveUniformsiv( ProgramID, 1, &TempUniformIndex, GL_UNIFORM_ARRAY_STRIDE, &UniformArrayStride );
					ASSERT_NO_GL_ERROR();

					GLint UniformMatrixStride;
					glGetActiveUniformsiv( ProgramID, 1, &TempUniformIndex, GL_UNIFORM_MATRIX_STRIDE, &UniformMatrixStride );
					ASSERT_NO_GL_ERROR();

					LogFile.Logf( TEXT("\t      ( size %d, uniform block %d : offset %d array stride %d, matrix stride %d, is row major %d )") LINE_TERMINATOR,
						UniformSize, UniformBlockIndex, UniformOffset, UniformArrayStride, UniformMatrixStride, UniformIsRowMajor );

					GLint UniformBlockBinding;
					glGetActiveUniformBlockiv( ProgramID, UniformBlockIndex, GL_UNIFORM_BLOCK_BINDING, &UniformBlockBinding );
					ASSERT_NO_GL_ERROR();

					GLint UniformBufferID;
					glGetIntegeri_v( GL_UNIFORM_BUFFER_BINDING, UniformBlockBinding, &UniformBufferID );
					ASSERT_NO_GL_ERROR();

					if( UniformBufferID )
					{
						GLint CurrentUniformBufferBinding;
						glGetIntegerv( GL_UNIFORM_BUFFER_BINDING, &CurrentUniformBufferBinding );
						ASSERT_NO_GL_ERROR();

						glBindBuffer( GL_UNIFORM_BUFFER, UniformBufferID );
						ASSERT_NO_GL_ERROR();

						GLint64 TotalBufferSize = 0LL;	// getters apparently like to overwrite just some bytes of this on Mac, if they return small number, so we need to put 0 in both dwords initially
						glGetBufferParameteri64v( GL_UNIFORM_BUFFER, GL_BUFFER_SIZE, &TotalBufferSize );
						ASSERT_NO_GL_ERROR();

						GLuint BufferSize = (GLuint)TotalBufferSize;	// yes, I know it's conversion to shorter variable, I don't expect buffers > 4GB in size.

						int32 SizeToMap = UniformArrayStride ? UniformSize*UniformArrayStride : 16*sizeof(float);
						int32 MaxSizeToMap = BufferSize-UniformOffset;
						if( MaxSizeToMap < SizeToMap )
						{
							SizeToMap = MaxSizeToMap;
						}

						if( SizeToMap <= 0 )
						{
							UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Active uniform is beyond the end of the uniform buffer!") );
						}
						else
						{
							void* BufferPtr = glMapBufferRange( GL_UNIFORM_BUFFER, UniformOffset, SizeToMap, GL_MAP_READ_BIT );
							ASSERT_NO_GL_ERROR();
							if( BufferPtr )
							{
								for( int32 UniformArrayMemberIndex = 0; UniformArrayMemberIndex < UniformSize; ++UniformArrayMemberIndex )
								{
									uint8* DataBuffer = (uint8*)BufferPtr + UniformArrayMemberIndex * UniformArrayStride;

									FString Line = TEXT("\t    ");
									InterpretUniform( UniformType, DataBuffer, Line );
									Line += LINE_TERMINATOR;
									LogFile.Log( Line );
								}
								glUnmapBuffer( GL_UNIFORM_BUFFER );
								ASSERT_NO_GL_ERROR();
							}
							else
							{
								UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Failed to map uniform buffer %d!"), UniformBlockBinding );
							}
						}

						glBindBuffer( GL_UNIFORM_BUFFER, CurrentUniformBufferBinding );
						ASSERT_NO_GL_ERROR();
					}
					else
					{
						UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Nothing bound to active uniform block right after draw!") );
					}
				}
			}

			FMemory::Free( ActiveUniformName );
		}
	}

	// Uniform blocks
	GLint ActiveUniformBlockCount;
	glGetProgramiv( ProgramID, GL_ACTIVE_UNIFORM_BLOCKS, &ActiveUniformBlockCount );
	ASSERT_NO_GL_ERROR();

	LogFile.Logf( TEXT("\tActive uniform blocks: %d") LINE_TERMINATOR, ActiveUniformBlockCount );

	if( ActiveUniformBlockCount )
	{
		GLint MaxActiveUniformBlockNameLength;
		glGetProgramiv( ProgramID, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &MaxActiveUniformBlockNameLength );
		ASSERT_NO_GL_ERROR();

		if( MaxActiveUniformBlockNameLength )
		{
			ANSICHAR* ActiveUniformBlockName = (ANSICHAR*)FMemory::Malloc( sizeof(ANSICHAR)*(MaxActiveUniformBlockNameLength+1) );
			ActiveUniformBlockName[MaxActiveUniformBlockNameLength] = 0;

			for( GLint ActiveUniformBlockIndex = 0; ActiveUniformBlockIndex < ActiveUniformBlockCount; ++ActiveUniformBlockIndex )
			{
				GLsizei NameLengthReceived;
				glGetActiveUniformBlockName( ProgramID, ActiveUniformBlockIndex, MaxActiveUniformBlockNameLength+1, &NameLengthReceived, ActiveUniformBlockName );
				ASSERT_NO_GL_ERROR();

				GLint UniformBlockBinding;
				glGetActiveUniformBlockiv( ProgramID, ActiveUniformBlockIndex, GL_UNIFORM_BLOCK_BINDING, &UniformBlockBinding );
				ASSERT_NO_GL_ERROR();

				GLint UniformBlockDataSize;
				glGetActiveUniformBlockiv( ProgramID, ActiveUniformBlockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &UniformBlockDataSize );
				ASSERT_NO_GL_ERROR();

				GLint ActiveUniformsInBlockCount;
				glGetActiveUniformBlockiv( ProgramID, ActiveUniformBlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &ActiveUniformsInBlockCount );
				ASSERT_NO_GL_ERROR();

				GLint IsReferencedByVertexShader;
				glGetActiveUniformBlockiv( ProgramID, ActiveUniformBlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, &IsReferencedByVertexShader );
				ASSERT_NO_GL_ERROR();

				GLint IsReferencedByGeometryShader;
				glGetActiveUniformBlockiv( ProgramID, ActiveUniformBlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER, &IsReferencedByGeometryShader );
				ASSERT_NO_GL_ERROR();

				GLint IsReferencedByFragmentShader;
				glGetActiveUniformBlockiv( ProgramID, ActiveUniformBlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, &IsReferencedByFragmentShader );
				ASSERT_NO_GL_ERROR();

				// TO DO someday, if ever needed - get a list of active uniform indices for the block
//				gl3GetActiveUniformBlockiv( ProgramID, ActiveUniformBlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, &TableOfIndices );

				FString ActiveUniformBlockNameString( ActiveUniformBlockName );

				LogFile.Logf( TEXT("\t%02d: %s ( binding %d, size %d, active uniforms %d, referenced by: %s%s%s )") LINE_TERMINATOR,
					ActiveUniformBlockIndex,
					*ActiveUniformBlockNameString,
					UniformBlockBinding,
					UniformBlockDataSize,
					ActiveUniformsInBlockCount,
					IsReferencedByVertexShader ? TEXT("V") : TEXT("_"),
					IsReferencedByGeometryShader ? TEXT("G") : TEXT("_"),
					IsReferencedByFragmentShader ? TEXT("F") : TEXT("_")
				);
			}

			FMemory::Free( ActiveUniformBlockName );
		}
	}

	// glValidateProgram
	glValidateProgram( ProgramID );

	GLint ValidationStatus;
	glGetProgramiv( ProgramID, GL_VALIDATE_STATUS, &ValidationStatus );
	LogFile.Logf( TEXT("\tProgram validation status: %s") LINE_TERMINATOR, ( ValidationStatus == GL_FALSE ) ? TEXT("FALSE") : TEXT("TRUE") );

	// At the end, info log
	GLint InfoLogLength;
	glGetProgramiv( ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength );
	ASSERT_NO_GL_ERROR();
	if( InfoLogLength )
	{
		ANSICHAR* ANSIBuffer = (ANSICHAR*)FMemory::Malloc( sizeof(ANSICHAR)*InfoLogLength );	// OpenGL gives back char*
		glGetProgramInfoLog( ProgramID, InfoLogLength, NULL, ANSIBuffer );
		FString Buffer( ANSIBuffer );
		LogFile.Logf( TEXT("\tProgram info log:") LINE_TERMINATOR
			TEXT("==============================================") LINE_TERMINATOR
			TEXT("%s==============================================") LINE_TERMINATOR,
			*Buffer );
		FMemory::Free( ANSIBuffer );
	}
	else
	{
		LogFile.Log( TEXT("\tNo program info log") LINE_TERMINATOR );
	}

	// TO DO someday - add geometry shader and transform feedback information here. Skipping it for now.
}

void FOpenGLDebugFrameDumper::DumpShaderContents( FOutputDeviceFile& LogFile, GLint ShaderID )
{
	GLboolean bIsShader = glIsShader( ShaderID );
	ASSERT_NO_GL_ERROR();

	if( !bIsShader )
	{
		LogFile.Logf( TEXT("Shader ID %d is not a valid shader ID!") LINE_TERMINATOR, ShaderID );
		return;
	}

	LogFile.Logf( TEXT("Shader %d info") LINE_TERMINATOR, ShaderID );

	GLint ShaderType;
	glGetShaderiv( ShaderID, GL_SHADER_TYPE, &ShaderType );
	ASSERT_NO_GL_ERROR();

	const TCHAR* ShaderTypeString = GetShaderType( ShaderType );
	LogFile.Logf( TEXT("\tGL_SHADER_TYPE: %s") LINE_TERMINATOR, ShaderTypeString );

	{
		GLint Status;
		glGetShaderiv( ShaderID, GL_DELETE_STATUS, &Status );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\tGL_DELETE_STATUS: %s") LINE_TERMINATOR, Status ? TEXT("TRUE") : TEXT("FALSE") );
	}

	{
		GLint Status;
		glGetShaderiv( ShaderID, GL_COMPILE_STATUS, &Status );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\tGL_COMPILE_STATUS: %s") LINE_TERMINATOR, Status ? TEXT("TRUE") : TEXT("FALSE") );
	}

	GLint ShaderSourceLength = 0;
	glGetShaderiv( ShaderID, GL_SHADER_SOURCE_LENGTH, &ShaderSourceLength );
	ASSERT_NO_GL_ERROR();

	if( ShaderSourceLength )
	{
		ANSICHAR* SourceCode = (ANSICHAR*)FMemory::Malloc( ShaderSourceLength );
		glGetShaderSource( ShaderID, ShaderSourceLength, 0, SourceCode );
		ASSERT_NO_GL_ERROR();

		uint32 CRC = FCrc::MemCrc_DEPRECATED( SourceCode, ShaderSourceLength );

		LogFile.Logf( TEXT("\tShader source code (length %u characters, CRC: 0x%x):") LINE_TERMINATOR
			TEXT("==============================================") LINE_TERMINATOR
			TEXT("%s==============================================") LINE_TERMINATOR,
			ShaderSourceLength-1, CRC, *FString( SourceCode ) );
		FMemory::Free( SourceCode );
	}
	else
	{
		LogFile.Logf( TEXT("\tNo shader source code") LINE_TERMINATOR );
	}

	GLint ShaderInfoLogLength = 0;
	glGetShaderiv( ShaderID, GL_INFO_LOG_LENGTH, &ShaderInfoLogLength );
	ASSERT_NO_GL_ERROR();

	if( ShaderInfoLogLength )
	{
		ANSICHAR* InfoLog = (ANSICHAR*)FMemory::Malloc( ShaderInfoLogLength+1 );
		glGetShaderInfoLog( ShaderID, ShaderInfoLogLength+1, 0, InfoLog );
		ASSERT_NO_GL_ERROR();

		LogFile.Logf( TEXT("\tShader info log:") LINE_TERMINATOR
			TEXT("==============================================") LINE_TERMINATOR
			TEXT("%s==============================================") LINE_TERMINATOR,
			*FString( InfoLog ) );
		FMemory::Free( InfoLog );
	}
	else
	{
		LogFile.Logf( TEXT("\tNo shader info log") LINE_TERMINATOR );
	}
}

void FOpenGLDebugFrameDumper::GetBoundTextureSurfaceLevelSettings( GLenum SurfaceType, GLint Level, TextureLevelInfo& OutInfo )
{
	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_WIDTH, &OutInfo.Width );
	ASSERT_NO_GL_ERROR();

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_HEIGHT, &OutInfo.Height );
	ASSERT_NO_GL_ERROR();

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_DEPTH, &OutInfo.Depth );
	ASSERT_NO_GL_ERROR();

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_SAMPLES, &OutInfo.Samples );
	ASSERT_NO_GL_ERROR();

	GLint FixedSampleLocations;
	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_FIXED_SAMPLE_LOCATIONS, &FixedSampleLocations );
	ASSERT_NO_GL_ERROR();
	OutInfo.bHasFixedSampleLocations = ( FixedSampleLocations != 0 );

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_INTERNAL_FORMAT, &OutInfo.InternalFormat );
	ASSERT_NO_GL_ERROR();

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_RED_SIZE, &OutInfo.RedBits );
	ASSERT_NO_GL_ERROR();

	if( OutInfo.RedBits )
	{
		glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_RED_TYPE, &OutInfo.RedType );
		ASSERT_NO_GL_ERROR();
	}
	else
	{
		OutInfo.RedType = 0;
	}

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_GREEN_SIZE, &OutInfo.GreenBits );
	ASSERT_NO_GL_ERROR();

	if( OutInfo.GreenBits )
	{
		glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_GREEN_TYPE, &OutInfo.GreenType );
		ASSERT_NO_GL_ERROR();
	}
	else
	{
		OutInfo.GreenType = 0;
	}

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_BLUE_SIZE, &OutInfo.BlueBits );
	ASSERT_NO_GL_ERROR();

	if( OutInfo.BlueBits )
	{
		glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_BLUE_TYPE, &OutInfo.BlueType );
		ASSERT_NO_GL_ERROR();
	}
	else
	{
		OutInfo.BlueType = 0;
	}

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_ALPHA_SIZE, &OutInfo.AlphaBits );
	ASSERT_NO_GL_ERROR();

	if( OutInfo.AlphaBits )
	{
		glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_ALPHA_TYPE, &OutInfo.AlphaType );
		ASSERT_NO_GL_ERROR();
	}
	else
	{
		OutInfo.AlphaType = 0;
	}

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_DEPTH_SIZE, &OutInfo.DepthBits );
	ASSERT_NO_GL_ERROR();

	if( OutInfo.DepthBits )
	{
		glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_DEPTH_TYPE, &OutInfo.DepthType );
		ASSERT_NO_GL_ERROR();
	}
	else
	{
		OutInfo.DepthType = 0;
	}

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_STENCIL_SIZE, &OutInfo.StencilBits );
	ASSERT_NO_GL_ERROR();

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_SHARED_SIZE, &OutInfo.SharedSize );
	ASSERT_NO_GL_ERROR();

	GLint TextureCompressed;
	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_COMPRESSED, &TextureCompressed );
	ASSERT_NO_GL_ERROR();
	OutInfo.bIsCompressed = ( TextureCompressed != 0 );

	if( OutInfo.bIsCompressed )
	{
		glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &OutInfo.CompressedSize );
		ASSERT_NO_GL_ERROR();
	}
	else
	{
		OutInfo.CompressedSize = 0;
	}

	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &OutInfo.DataStoreBinding );
	ASSERT_NO_GL_ERROR();
}

void FOpenGLDebugFrameDumper::DumpBoundTextureSurfaceSettings( FOutputDeviceFile& LogFile, GLenum SurfaceType, GLint BaseLevel, GLint MaxLevel )
{
	TextureLevelInfo BaseInfo;
	GetBoundTextureSurfaceLevelSettings( SurfaceType, BaseLevel, BaseInfo );

	// Log base level
	LogFile.Logf( TEXT("\tBase level ( %d ) info") LINE_TERMINATOR, BaseLevel );
	LogFile.Logf( TEXT("\t\t%d x %d x %d ( %d samples, shared size %d )") LINE_TERMINATOR, BaseInfo.Width, BaseInfo.Height, BaseInfo.Depth, BaseInfo.Samples, BaseInfo.SharedSize );
	if( BaseInfo.bHasFixedSampleLocations )
	{
		LogFile.Log( TEXT("\t\tfixed sample locations") LINE_TERMINATOR );
	}
	LogFile.Logf( TEXT("\t\tInternal format: %s") LINE_TERMINATOR, GetGLInternalFormatString( BaseInfo.InternalFormat ) );
	if( BaseInfo.RedBits )
	{
		LogFile.Logf( TEXT("\t\tR bits: %d, component type: %s") LINE_TERMINATOR, BaseInfo.RedBits, GetComponentType( BaseInfo.RedType ) );
	}
	if( BaseInfo.GreenBits )
	{
		LogFile.Logf( TEXT("\t\tG bits: %d, component type: %s") LINE_TERMINATOR, BaseInfo.GreenBits, GetComponentType( BaseInfo.GreenType ) );
	}
	if( BaseInfo.BlueBits )
	{
		LogFile.Logf( TEXT("\t\tB bits: %d, component type: %s") LINE_TERMINATOR, BaseInfo.BlueBits, GetComponentType( BaseInfo.BlueType ) );
	}
	if( BaseInfo.AlphaBits )
	{
		LogFile.Logf( TEXT("\t\tA bits: %d, component type: %s") LINE_TERMINATOR, BaseInfo.AlphaBits, GetComponentType( BaseInfo.AlphaType ) );
	}
	if( BaseInfo.DepthBits )
	{
		LogFile.Logf( TEXT("\t\tDepth bits: %d, component type: %s") LINE_TERMINATOR, BaseInfo.DepthBits, GetComponentType( BaseInfo.DepthType ) );
	}
	if( BaseInfo.StencilBits )
	{
		LogFile.Logf( TEXT("\t\tStencil bits: %d") LINE_TERMINATOR, BaseInfo.StencilBits );
	}
	if( BaseInfo.bIsCompressed )
	{
		LogFile.Logf( TEXT("\t\tTexture compressed, size: %d") LINE_TERMINATOR, BaseInfo.CompressedSize );
	}
	if( BaseInfo.DataStoreBinding )
	{
		LogFile.Logf( TEXT("\t\tData store binding: %d") LINE_TERMINATOR, BaseInfo.DataStoreBinding );
	}

	if( MaxLevel > BaseLevel )
		LogFile.Log( TEXT("\t") LINE_TERMINATOR );

	TextureLevelInfo PreviousLevelInfo = BaseInfo;

	for( GLint Level = BaseLevel+1; Level <= MaxLevel; ++Level )
	{
		TextureLevelInfo NewInfo;
		GetBoundTextureSurfaceLevelSettings( SurfaceType, Level, NewInfo );

		LogFile.Logf( TEXT("\tLevel %d: %d x %d x %d ( %d samples, shared size: %d )") LINE_TERMINATOR, Level, NewInfo.Width, NewInfo.Height, NewInfo.Depth, NewInfo.Samples, NewInfo.SharedSize );
		if( NewInfo.bHasFixedSampleLocations != PreviousLevelInfo.bHasFixedSampleLocations )
		{
			LogFile.Logf( TEXT("\t\tfixed sample locations: %s") LINE_TERMINATOR, NewInfo.bHasFixedSampleLocations ? TEXT("TRUE") : TEXT("FALSE") );
		}

		if( NewInfo.InternalFormat != PreviousLevelInfo.InternalFormat )
		{
			LogFile.Logf( TEXT("\t\tInternal format: %s") LINE_TERMINATOR, GetGLInternalFormatString( NewInfo.InternalFormat ) );
		}

		if( NewInfo.RedBits != PreviousLevelInfo.RedBits )
		{
			if( NewInfo.RedBits )
			{
				LogFile.Logf( TEXT("\t\tR bits: %d, component type: %s") LINE_TERMINATOR, NewInfo.RedBits, GetComponentType( NewInfo.RedType ) );
			}
			else
			{
				LogFile.Log( TEXT("\t\tR bits gone!") );
			}
		}
		if( NewInfo.GreenBits != PreviousLevelInfo.GreenBits )
		{
			if( NewInfo.GreenBits )
			{
				LogFile.Logf( TEXT("\t\tG bits: %d, component type: %s") LINE_TERMINATOR, NewInfo.GreenBits, GetComponentType( NewInfo.GreenType ) );
			}
			else
			{
				LogFile.Log( TEXT("\t\tG bits gone!") );
			}
		}
		if( NewInfo.BlueBits != PreviousLevelInfo.BlueBits )
		{
			if( NewInfo.BlueBits )
			{
				LogFile.Logf( TEXT("\t\tB bits: %d, component type: %s") LINE_TERMINATOR, NewInfo.BlueBits, GetComponentType( NewInfo.BlueType ) );
			}
			else
			{
				LogFile.Log( TEXT("\t\tB bits gone!") );
			}
		}
		if( NewInfo.AlphaBits != PreviousLevelInfo.AlphaBits )
		{
			if( NewInfo.AlphaBits )
			{
				LogFile.Logf( TEXT("\t\tA bits: %d, component type: %s") LINE_TERMINATOR, NewInfo.AlphaBits, GetComponentType( NewInfo.AlphaType ) );
			}
			else
			{
				LogFile.Log( TEXT("\t\tA bits gone!") );
			}
		}
		if( NewInfo.DepthBits != PreviousLevelInfo.DepthBits )
		{
			if( NewInfo.DepthBits )
			{
				LogFile.Logf( TEXT("\t\tDepth bits: %d, component type: %s") LINE_TERMINATOR, NewInfo.DepthBits, GetComponentType( NewInfo.DepthType ) );
			}
			else
			{
				LogFile.Log( TEXT("\t\tDepth bits gone!") );
			}
		}
		if( NewInfo.StencilBits != PreviousLevelInfo.StencilBits )
		{
			if( NewInfo.StencilBits )
			{
				LogFile.Logf( TEXT("\t\tStencil bits: %d") LINE_TERMINATOR, NewInfo.StencilBits );
			}
			else
			{
				LogFile.Log( TEXT("\t\tStencil bits gone!") );
			}
		}

		if( NewInfo.bIsCompressed != PreviousLevelInfo.bIsCompressed )
		{
			if( NewInfo.bIsCompressed )
			{
				LogFile.Logf( TEXT("\t\tTexture compressed, size: %d") LINE_TERMINATOR, NewInfo.CompressedSize );
			}
			else
			{
				LogFile.Log( TEXT("\t\tTexture not compressed now!") LINE_TERMINATOR );
			}
		}

		if( NewInfo.DataStoreBinding )
		{
			LogFile.Logf( TEXT("\t\tData store binding: %d") LINE_TERMINATOR, NewInfo.DataStoreBinding );
		}

		PreviousLevelInfo = NewInfo;
	}

	LogFile.Log( TEXT("\t") LINE_TERMINATOR );
}

void FOpenGLDebugFrameDumper::DumpBoundTextureSettings( FOutputDeviceFile& LogFile, GLenum UnitTarget )
{
	GLfloat BorderColor[4];
	glGetTexParameterfv( UnitTarget, GL_TEXTURE_BORDER_COLOR, BorderColor );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_BORDER_COLOR: ( %f, %f, %f, %f )") LINE_TERMINATOR, BorderColor[0], BorderColor[1], BorderColor[2], BorderColor[3] );

	GLint TextureMinFilter;
	glGetTexParameteriv( UnitTarget, GL_TEXTURE_MIN_FILTER, &TextureMinFilter );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_MIN_FILTER: %s") LINE_TERMINATOR, GetGLTextureFilterString( TextureMinFilter ) );

	GLint TextureMagFilter;
	glGetTexParameteriv( UnitTarget, GL_TEXTURE_MAG_FILTER, &TextureMagFilter );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_MAG_FILTER: %s") LINE_TERMINATOR, GetGLTextureFilterString( TextureMagFilter ) );

	GLint TextureWrapS;
	glGetTexParameteriv( UnitTarget, GL_TEXTURE_WRAP_S, &TextureWrapS );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_WRAP_S: %s") LINE_TERMINATOR, GetGLTextureWrapString( TextureWrapS ) );

	GLint TextureWrapT;
	glGetTexParameteriv( UnitTarget, GL_TEXTURE_WRAP_T, &TextureWrapT );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_WRAP_T: %s") LINE_TERMINATOR, GetGLTextureWrapString( TextureWrapT ) );

	GLint TextureWrapR;
	glGetTexParameteriv( UnitTarget, GL_TEXTURE_WRAP_R, &TextureWrapR );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_WRAP_R: %s") LINE_TERMINATOR, GetGLTextureWrapString( TextureWrapR ) );

	GLfloat TextureMinLOD;
	glGetTexParameterfv( UnitTarget, GL_TEXTURE_MIN_LOD, &TextureMinLOD );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_MIN_LOD: %f") LINE_TERMINATOR, TextureMinLOD );

	GLfloat TextureMaxLOD;
	glGetTexParameterfv( UnitTarget, GL_TEXTURE_MAX_LOD, &TextureMaxLOD );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_MAX_LOD: %f") LINE_TERMINATOR, TextureMaxLOD );

	GLint TextureBaseLevel;
	glGetTexParameteriv( UnitTarget, GL_TEXTURE_BASE_LEVEL, &TextureBaseLevel );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_BASE_LEVEL: %d") LINE_TERMINATOR, TextureBaseLevel );

	GLint TextureMaxLevel;
	glGetTexParameteriv( UnitTarget, GL_TEXTURE_MAX_LEVEL, &TextureMaxLevel );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_MAX_LEVEL: %d") LINE_TERMINATOR, TextureMaxLevel );

	GLfloat TextureLODBias;
	glGetTexParameterfv( UnitTarget, GL_TEXTURE_LOD_BIAS, &TextureLODBias );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_LOD_BIAS: %f") LINE_TERMINATOR, TextureLODBias );

	GLint TextureCompareMode;
	glGetTexParameteriv( UnitTarget, GL_TEXTURE_COMPARE_MODE, &TextureCompareMode );
	ASSERT_NO_GL_ERROR();
	if( TextureCompareMode != GL_NONE )
	{
		LogFile.Logf( TEXT("\tGL_TEXTURE_COMPARE_MODE: unknown value ( 0x%x )") LINE_TERMINATOR, TextureCompareMode );

		GLint TextureCompareFunc;
		glGetTexParameteriv( UnitTarget, GL_TEXTURE_COMPARE_FUNC, &TextureCompareFunc );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\t\tGL_TEXTURE_COMPARE_FUNC: %s") LINE_TERMINATOR, GetGLCompareString( TextureCompareFunc ) );
	}
	else
	{
		LogFile.Log( TEXT("\tGL_TEXTURE_COMPARE_MODE: GL_NONE") LINE_TERMINATOR );
	}

	LogFile.Log( TEXT("\t") LINE_TERMINATOR );

	if( TextureBaseLevel > TextureMaxLevel )
	{
		LogFile.Logf( TEXT("\tBase texture level > max level, data makes no sense!") LINE_TERMINATOR TEXT("\t") LINE_TERMINATOR );
	}
	else if( UnitTarget == GL_TEXTURE_CUBE_MAP )
	{
		for( GLint TextureFace = GL_TEXTURE_CUBE_MAP_POSITIVE_X; TextureFace <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; ++TextureFace )
		{
			LogFile.Logf( TEXT("\tTexture face: %s") LINE_TERMINATOR TEXT("\t") LINE_TERMINATOR, GetCubeMapFaceName( TextureFace ) );
			DumpBoundTextureSurfaceSettings( LogFile, TextureFace, TextureBaseLevel, TextureMaxLevel );
		}
	}
	else
	{
		DumpBoundTextureSurfaceSettings( LogFile, UnitTarget, TextureBaseLevel, TextureMaxLevel );
	}
}

void FOpenGLDebugFrameDumper::DumpBoundSamplerSettings( FOutputDeviceFile& LogFile, GLint SamplerID )
{
	GLfloat BorderColor[4];
	glGetSamplerParameterfv( SamplerID, GL_TEXTURE_BORDER_COLOR, BorderColor );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_BORDER_COLOR: ( %f, %f, %f, %f )") LINE_TERMINATOR, BorderColor[0], BorderColor[1], BorderColor[2], BorderColor[3] );

	GLint MinFilter;
	glGetSamplerParameteriv( SamplerID, GL_TEXTURE_MIN_FILTER, &MinFilter );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_MIN_FILTER: %s") LINE_TERMINATOR, GetGLTextureFilterString( MinFilter ) );

	GLint MagFilter;
	glGetSamplerParameteriv( SamplerID, GL_TEXTURE_MAG_FILTER, &MagFilter );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_MAG_FILTER: %s") LINE_TERMINATOR, GetGLTextureFilterString( MagFilter ) );

	GLint WrapS;
	glGetSamplerParameteriv( SamplerID, GL_TEXTURE_WRAP_S, &WrapS );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_WRAP_S: %s") LINE_TERMINATOR, GetGLTextureWrapString( WrapS ) );

	GLint WrapT;
	glGetSamplerParameteriv( SamplerID, GL_TEXTURE_WRAP_T, &WrapT );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_WRAP_T: %s") LINE_TERMINATOR, GetGLTextureWrapString( WrapT ) );

	GLint WrapR;
	glGetSamplerParameteriv( SamplerID, GL_TEXTURE_WRAP_R, &WrapR );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_WRAP_R: %s") LINE_TERMINATOR, GetGLTextureWrapString( WrapR ) );

	GLfloat MinLOD;
	glGetSamplerParameterfv( SamplerID, GL_TEXTURE_MIN_LOD, &MinLOD );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_MIN_LOD: %f") LINE_TERMINATOR, MinLOD );

	GLfloat MaxLOD;
	glGetSamplerParameterfv( SamplerID, GL_TEXTURE_MAX_LOD, &MaxLOD );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_MAX_LOD: %f") LINE_TERMINATOR, MaxLOD );

	GLfloat LODBias;
	glGetSamplerParameterfv( SamplerID, GL_TEXTURE_LOD_BIAS, &LODBias );
	ASSERT_NO_GL_ERROR();
	LogFile.Logf( TEXT("\tGL_TEXTURE_LOD_BIAS: %f") LINE_TERMINATOR, LODBias );

	GLint CompareMode;
	glGetSamplerParameteriv( SamplerID, GL_TEXTURE_COMPARE_MODE, &CompareMode );
	ASSERT_NO_GL_ERROR();
	if( CompareMode != GL_NONE )
	{
		LogFile.Logf( TEXT("\tGL_TEXTURE_COMPARE_MODE: unknown value ( 0x%x )") LINE_TERMINATOR, CompareMode );

		GLint CompareFunc;
		glGetSamplerParameteriv( SamplerID, GL_TEXTURE_COMPARE_FUNC, &CompareFunc );
		ASSERT_NO_GL_ERROR();
		LogFile.Logf( TEXT("\t\tGL_TEXTURE_COMPARE_FUNC: %s") LINE_TERMINATOR, GetGLCompareString( CompareFunc ) );
	}
	else
	{
		LogFile.Log( TEXT("\tGL_TEXTURE_COMPARE_MODE: GL_NONE") LINE_TERMINATOR );
	}

	LogFile.Log( TEXT("\t") LINE_TERMINATOR );
}

void FOpenGLDebugFrameDumper::DumpTextureUnitSettings( FOutputDeviceFile& LogFile, GLint TextureUnitIndex )
{
	glActiveTexture( GL_TEXTURE0+TextureUnitIndex );
	ASSERT_NO_GL_ERROR();

	bool bIsTextureBound = false;

	GLint Binding;
	glGetIntegerv( GL_TEXTURE_BINDING_1D, &Binding );
	ASSERT_NO_GL_ERROR();
	if( Binding != 0 )
	{
		LogFile.Logf( TEXT("Unit %2d : GL_TEXTURE_BINDING_1D: %d") LINE_TERMINATOR, TextureUnitIndex, Binding );
		DumpBoundTextureSettings( LogFile, GL_TEXTURE_1D );
		bIsTextureBound = true;
	}

	glGetIntegerv( GL_TEXTURE_BINDING_2D, &Binding );
	ASSERT_NO_GL_ERROR();
	if( Binding != 0 )
	{
		LogFile.Logf( TEXT("Unit %2d : GL_TEXTURE_BINDING_2D: %d") LINE_TERMINATOR, TextureUnitIndex, Binding );
		DumpBoundTextureSettings( LogFile, GL_TEXTURE_2D );
		bIsTextureBound = true;
	}

	glGetIntegerv( GL_TEXTURE_BINDING_3D, &Binding );
	ASSERT_NO_GL_ERROR();
	if( Binding != 0 )
	{
		LogFile.Logf( TEXT("Unit %2d : GL_TEXTURE_BINDING_3D: %d") LINE_TERMINATOR, TextureUnitIndex, Binding );
		DumpBoundTextureSettings( LogFile, GL_TEXTURE_3D );
		bIsTextureBound = true;
	}

	glGetIntegerv( GL_TEXTURE_BINDING_CUBE_MAP, &Binding );
	ASSERT_NO_GL_ERROR();
	if( Binding != 0 )
	{
		LogFile.Logf( TEXT("Unit %2d : GL_TEXTURE_BINDING_CUBE_MAP: %d") LINE_TERMINATOR, TextureUnitIndex, Binding );
		DumpBoundTextureSettings( LogFile, GL_TEXTURE_CUBE_MAP );
		bIsTextureBound = true;
	}

	glGetIntegerv( GL_TEXTURE_BINDING_2D_MULTISAMPLE, &Binding );
	ASSERT_NO_GL_ERROR();
	if( Binding != 0 )
	{
		LogFile.Logf( TEXT("Unit %2d : GL_TEXTURE_BINDING_2D_MULTISAMPLE: %d") LINE_TERMINATOR, TextureUnitIndex, Binding );
		DumpBoundTextureSurfaceSettings( LogFile, GL_TEXTURE_2D_MULTISAMPLE, 0, 0 );
		bIsTextureBound = true;
	}

	glGetIntegerv( GL_TEXTURE_BINDING_BUFFER, &Binding );
	ASSERT_NO_GL_ERROR();
	if( Binding != 0 )
	{
		GLint DataStoreBinding;
		glGetTexLevelParameteriv( GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &DataStoreBinding );
		LogFile.Logf( TEXT("Unit %2d : GL_TEXTURE_BINDING_BUFFER: %d (bound buffer: %d)") LINE_TERMINATOR, TextureUnitIndex, Binding, DataStoreBinding );
	}

	if( bIsTextureBound )
	{
		GLint SamplerBinding;
		glGetIntegerv( GL_SAMPLER_BINDING, &SamplerBinding );
		ASSERT_NO_GL_ERROR();

		if( SamplerBinding )
		{
			LogFile.Logf( TEXT("Unit %2d : GL_SAMPLER_BINDING: %d") LINE_TERMINATOR, TextureUnitIndex, SamplerBinding );
			DumpBoundSamplerSettings( LogFile, SamplerBinding );
		}
	}
}

void FOpenGLDebugFrameDumper::DumpGeneralOpenGLState( const TCHAR* DrawCommandDescription, bool bIsDrawEvent, bool bIsFramebufferBlitEvent )
{
	FString LogFileName = *CachedEventFolder / TEXT("state.log");
	FOutputDeviceFile LogFile( *LogFileName );
	LogFile.SetAutoEmitLineTerminator( false );
	LogFile.Log( LINE_TERMINATOR );	// to end "log start" line

	// Extract and log current event info
	LogFile.Logf( TEXT( "Event: %s" ) LINE_TERMINATOR, DrawCommandDescription );

	// Extract and log current OpenGL error
	GLenum OpenGLError = glGetError();
	const TCHAR* OpenGLErrorString = 0;
	switch( OpenGLError )
	{
	case GL_NO_ERROR:			OpenGLErrorString = TEXT("GL_NO_ERROR" );			break;
	case GL_INVALID_ENUM:		OpenGLErrorString = TEXT("GL_INVALID_ENUM" );		break;
	case GL_INVALID_VALUE:		OpenGLErrorString = TEXT("GL_INVALID_VALUE" );		break;
	case GL_INVALID_OPERATION:	OpenGLErrorString = TEXT("GL_INVALID_OPERATION" );	break;
	case GL_OUT_OF_MEMORY:		OpenGLErrorString = TEXT("GL_OUT_OF_MEMORY" );		break;
//	case GL_STACK_OVERFLOW:		OpenGLErrorString = TEXT("GL_STACK_OVERFLOW" );		break;
//	case GL_STACK_UNDERFLOW:	OpenGLErrorString = TEXT("GL_STACK_UNDERFLOW" );	break;
//	case GL_TABLE_TOO_LARGE:	OpenGLErrorString = TEXT("GL_TABLE_TOO_LARGE" );	break;
	default:					OpenGLErrorString = TEXT("unknown" );				break;
	}
	LogFile.Logf( TEXT( "OpenGL Error: %s ( 0x%x )" ) LINE_TERMINATOR, OpenGLErrorString, OpenGLError );

	DumpRenderTargetsState( LogFile );
	DumpDepthState( LogFile );
	DumpStencilState( LogFile );
	DumpBufferMasks( LogFile );
	DumpClearValues( LogFile );
	DumpMultisamplingSettings( LogFile );
	DumpScissorAndViewport( LogFile );

	if( bIsFramebufferBlitEvent || bIsDrawEvent )
	{
		DumpBufferBindings( LogFile );
	}

	if( bIsDrawEvent )
	{
		DumpVertexAttribArraysState( LogFile );
		DumpBlendingState( LogFile );
		DumpHintSettings( LogFile );
		DumpOpenGLLimits( LogFile );
		DumpPointsSettings( LogFile );
		DumpLinesSettings( LogFile );
		DumpPolygonsSettings( LogFile );
		DumpTextureLimitsAndBindings( LogFile );
		DumpProgramSettings( LogFile );
		DumpLogicOpsSettings( LogFile );
		DumpPixelModeSettings( LogFile );
	}

	LogFile.TearDown();
}

void FOpenGLDebugFrameDumper::DumpFramebufferState( bool bReadFramebuffer )
{
	FString LogFileName;
	GLenum FramebufferBindingEnum;
	if( bReadFramebuffer )
	{
		LogFileName = TEXT("framebufferRead.log");
		FramebufferBindingEnum = GL_READ_FRAMEBUFFER_BINDING;
	}
	else
	{
		LogFileName = TEXT("framebufferDraw.log");
		FramebufferBindingEnum = GL_DRAW_FRAMEBUFFER_BINDING;
	}

	FString LogFilePath = *CachedEventFolder / LogFileName;
	FOutputDeviceFile LogFile( *LogFilePath );
	LogFile.SetAutoEmitLineTerminator( false );
	LogFile.Log( LINE_TERMINATOR );	// to end "log start" line

	GLint CurrentlyBoundFramebuffer;
	glGetIntegerv( FramebufferBindingEnum, &CurrentlyBoundFramebuffer );
	ASSERT_NO_GL_ERROR();

	DumpFramebufferSettings( LogFile, CurrentlyBoundFramebuffer );

	LogFile.TearDown();
}

void FOpenGLDebugFrameDumper::DumpProgramAndShaderState( void )
{
	// Dump current program and its shaders state
	GLint ProgramID;
	glGetIntegerv( GL_CURRENT_PROGRAM, &ProgramID );
	ASSERT_NO_GL_ERROR();

	FString ProgramLogFileName = *CachedEventFolder / FString::Printf( TEXT("program%d.log"), ProgramID );
	FOutputDeviceFile ProgramLogFile( *ProgramLogFileName );
	ProgramLogFile.SetAutoEmitLineTerminator( false );
	ProgramLogFile.Log( LINE_TERMINATOR );	// to end "log start" line

	DumpProgramContents( ProgramLogFile, ProgramID );

	ProgramLogFile.TearDown();

	GLint AttachedShaderCount;
	glGetProgramiv( ProgramID, GL_ATTACHED_SHADERS, &AttachedShaderCount );
	ASSERT_NO_GL_ERROR();
	if( !AttachedShaderCount )
	{
		// we're done here
		return;
	}

	// Log attached shaders, into their own files
	GLsizei CountReceived = 0;
	GLuint* AttachedShadersTable = (GLuint*)FMemory::Malloc( sizeof(GLuint)*AttachedShaderCount );
	glGetAttachedShaders( ProgramID, AttachedShaderCount, &CountReceived, AttachedShadersTable );
	ASSERT_NO_GL_ERROR();

	for( GLint AttachedShaderIndex = 0; AttachedShaderIndex < CountReceived; ++AttachedShaderIndex )
	{
		FString ShaderLogFileName = *CachedEventFolder / FString::Printf( TEXT("shader%d.log"), AttachedShadersTable[AttachedShaderIndex] );
		FOutputDeviceFile ShaderLogFile( *ShaderLogFileName );
		ShaderLogFile.SetAutoEmitLineTerminator( false );
		ShaderLogFile.Log( LINE_TERMINATOR );	// to end "log start" line

		DumpShaderContents( ShaderLogFile, AttachedShadersTable[AttachedShaderIndex] );

		ShaderLogFile.TearDown();
	}

	FMemory::Free( AttachedShadersTable );
}

void FOpenGLDebugFrameDumper::DumpBoundTextureState( void )
{
	FString LogFileName = *CachedEventFolder / TEXT("textureUnits.log");
	FOutputDeviceFile LogFile( *LogFileName );
	LogFile.SetAutoEmitLineTerminator( false );
	LogFile.Log( LINE_TERMINATOR );	// to end "log start" line

	// Remember this, as DumpTextureUnitSettings() modifies it
	GLint ActiveTextureUnit;
	glGetIntegerv( GL_ACTIVE_TEXTURE, &ActiveTextureUnit );
	ASSERT_NO_GL_ERROR();

	GLint MaxTextureImageUnits;
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &MaxTextureImageUnits );
	ASSERT_NO_GL_ERROR();

	for( GLint TextureUnitIndex = 0; TextureUnitIndex < MaxTextureImageUnits; ++TextureUnitIndex )
	{
		DumpTextureUnitSettings( LogFile, TextureUnitIndex );
	}

	// And restore previous OpenGL state
	glActiveTexture( ActiveTextureUnit );
	ASSERT_NO_GL_ERROR();

	LogFile.TearDown();
}

void FOpenGLDebugFrameDumper::DumpFramebufferContent( GLint FramebufferID, GLint AttachmentSlot, const TCHAR* TargetFilename, EFramebufferAttachmentSlotType::Type SlotType, bool bShouldFlipImageVertically )
{
	// Remember those, as we'll need to restore them later
	GLint CurrentlyBoundReadFramebuffer;
	glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &CurrentlyBoundReadFramebuffer );
	ASSERT_NO_GL_ERROR();

	GLint CurrentReadBuffer;
	glGetIntegerv( GL_READ_BUFFER, &CurrentReadBuffer );
	ASSERT_NO_GL_ERROR();

	if( FramebufferID != CurrentlyBoundReadFramebuffer )
	{
		glBindFramebuffer( GL_READ_FRAMEBUFFER, FramebufferID );
		ASSERT_NO_GL_ERROR();
	}

	if( ( SlotType == EFramebufferAttachmentSlotType::Color ) && ( AttachmentSlot != CurrentReadBuffer ) )
	{
		FOpenGL::ReadBuffer( AttachmentSlot );	// this only needs to be selected for reading from one of color buffers
		ASSERT_NO_GL_ERROR();
	}

	GLint AttachmentType = GL_FRAMEBUFFER_DEFAULT;
	if( FramebufferID )
	{
		glGetFramebufferAttachmentParameteriv( GL_READ_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &AttachmentType );
		ASSERT_NO_GL_ERROR();
	}

	GLint Width = 0;
	GLint Height = 0;
	GLint Depth = 0;
	GLint InternalFormat = 0;

	bool bIsAllOk = true;

	if( AttachmentType == GL_FRAMEBUFFER_DEFAULT )
	{
		uint32 BackbufferWidth;
		uint32 BackbufferHeight;
		PlatformGetBackbufferDimensions( BackbufferWidth, BackbufferHeight );
		Width = BackbufferWidth;
		Height = BackbufferHeight;

		check( Width > 0 && Height > 0 );

		// no matter what the real format is, this is only to select target format later.
		if( SlotType == EFramebufferAttachmentSlotType::Depth )
		{
			InternalFormat = GL_DEPTH_COMPONENT32F;
		}
		else if( SlotType == EFramebufferAttachmentSlotType::Stencil )
		{
			InternalFormat = GL_DEPTH24_STENCIL8;
		}
		else
		{
			InternalFormat = GL_RGBA8;
		}
	}
	else if( AttachmentType == GL_TEXTURE )
	{
		// We need to get texture information now

		// Get texture ID for binding
		GLint TextureID = 0;
		glGetFramebufferAttachmentParameteriv( GL_READ_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &TextureID );
		ASSERT_NO_GL_ERROR();

		// Determine the level we need to ask about
		GLint TextureLevel;
		glGetFramebufferAttachmentParameteriv( GL_READ_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &TextureLevel );
		ASSERT_NO_GL_ERROR();

		// Determine if the texture we have attached is 2D or Cube ( I assume 3D and 1D make no sense )
		GLint TextureCubeMapFace;
		glGetFramebufferAttachmentParameteriv( GL_READ_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, &TextureCubeMapFace );
		ASSERT_NO_GL_ERROR();

		bool bIsCube = ( TextureCubeMapFace != 0 );

		// Setup various types of function call parameters, depending on whether it is cube or not
		GLenum TextureTypeAsk;
		GLenum TextureTypeSet;
		GLenum TextureTypeFace;
		if( bIsCube )
		{
			TextureTypeAsk = GL_TEXTURE_BINDING_CUBE_MAP;
			TextureTypeSet = GL_TEXTURE_CUBE_MAP;
			TextureTypeFace = TextureCubeMapFace;
		}
		else
		{
			TextureTypeAsk = GL_TEXTURE_BINDING_2D;
			TextureTypeSet = GL_TEXTURE_2D;
			TextureTypeFace = GL_TEXTURE_2D;
		}

		// Remember what's bound to the stage we'll bind our texture to
		GLint BoundTextureID;
		glGetIntegerv( TextureTypeAsk, &BoundTextureID );
		ASSERT_NO_GL_ERROR();

		// Bind our texture. Was it GL_TEXTURE_2D?
		ASSERT_NO_GL_ERROR();
		GDisableOpenGLDebugOutput = true;
		glBindTexture( TextureTypeSet, TextureID );
		glFinish();
		GDisableOpenGLDebugOutput = false;
		if( glGetError() )
		{
			bIsAllOk = false;

			// It could be GL_TEXTURE_2D_MULTISAMPLE then?
			check(TextureTypeSet == GL_TEXTURE_2D);
			check(TextureLevel == 0);
			TextureTypeAsk = GL_TEXTURE_BINDING_2D_MULTISAMPLE;
			TextureTypeSet = GL_TEXTURE_2D_MULTISAMPLE;
			TextureTypeFace = GL_TEXTURE_2D_MULTISAMPLE;
			glGetIntegerv( TextureTypeAsk, &BoundTextureID );
			ASSERT_NO_GL_ERROR();
			GDisableOpenGLDebugOutput = true;
			glBindTexture( TextureTypeSet, TextureID );
			glFinish();
			GDisableOpenGLDebugOutput = false;
			if( glGetError() )
			{
				// Ok, then GL_TEXTURE_3D?
				TextureTypeAsk = GL_TEXTURE_BINDING_3D;
				TextureTypeSet = GL_TEXTURE_3D;
				TextureTypeFace = GL_TEXTURE_3D;
				glGetIntegerv( TextureTypeAsk, &BoundTextureID );
				ASSERT_NO_GL_ERROR();
				glBindTexture( TextureTypeSet, TextureID );
				ASSERT_NO_GL_ERROR();

				UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Texture %d is 3D - dumping data from such is unhandled atm. Add code?"), TextureID );
			}
			else
			{
				UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Texture %d is multisampled - dumping data from such is unhandled atm. Add code?"), TextureID );
			}
		}

		// Get the information we need
		glGetTexLevelParameteriv( TextureTypeFace, TextureLevel, GL_TEXTURE_WIDTH, &Width );
		ASSERT_NO_GL_ERROR();

		glGetTexLevelParameteriv( TextureTypeFace, TextureLevel, GL_TEXTURE_HEIGHT, &Height );
		ASSERT_NO_GL_ERROR();

		glGetTexLevelParameteriv( TextureTypeFace, TextureLevel, GL_TEXTURE_DEPTH, &Depth );
		ASSERT_NO_GL_ERROR();

		glGetTexLevelParameteriv( TextureTypeFace, TextureLevel, GL_TEXTURE_INTERNAL_FORMAT, &InternalFormat );
		ASSERT_NO_GL_ERROR();

		// Restore previous binding to this stage
		if( BoundTextureID != TextureID )
		{
			glBindTexture( TextureTypeSet, BoundTextureID );
			ASSERT_NO_GL_ERROR();
		}
	}
	else if( AttachmentType == GL_RENDERBUFFER )
	{
		// We need to get renderbuffer information now

		// Get renderbuffer ID for binding
		GLint RenderbufferID = 0;
		glGetFramebufferAttachmentParameteriv( GL_READ_FRAMEBUFFER, AttachmentSlot, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &RenderbufferID );
		ASSERT_NO_GL_ERROR();

		// Remember what's bound as renderbuffer now, to restore it later
		GLint CurrentlyBoundRenderbuffer;
		glGetIntegerv( GL_RENDERBUFFER_BINDING, &CurrentlyBoundRenderbuffer );
		ASSERT_NO_GL_ERROR();

		// Bind our renderbuffer
		if( RenderbufferID != CurrentlyBoundRenderbuffer )
		{
			glBindRenderbuffer( GL_RENDERBUFFER, RenderbufferID );
			ASSERT_NO_GL_ERROR();
		}

		GLint Samples;
		glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &Samples );
		ASSERT_NO_GL_ERROR();
		if( Samples != 0 )
		{
			UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Renderbuffer %d is multisampled - dumping data from such is unhandled atm. Add code?"), RenderbufferID );
			bIsAllOk = false;
		}
		else
		{
			glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &Width );
			ASSERT_NO_GL_ERROR();

			glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &Height );
			ASSERT_NO_GL_ERROR();

			glGetRenderbufferParameteriv( GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &InternalFormat );
			ASSERT_NO_GL_ERROR();
		}

		// Restore previous state
		if( RenderbufferID != CurrentlyBoundRenderbuffer )
		{
			glBindRenderbuffer( GL_RENDERBUFFER, CurrentlyBoundRenderbuffer );
			ASSERT_NO_GL_ERROR();
		}
	}
	else if( AttachmentType == GL_NONE )
	{
		bIsAllOk = false;
	}
	else
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Unrecognized framebuffer attachment type: %d! Debug this to add handling for it?."), AttachmentType );
		bIsAllOk = false;
	}

	if( bIsAllOk )
	{
		FString FilenameString( TargetFilename );

		int32 RGBADataSize = 4 * Width * Height;
		uint8* RGBAData = (uint8*)FMemory::Malloc( RGBADataSize );

		bool bIgnoreAlpha = false;

		switch( InternalFormat )
		{
			// These formats can reasonably be expected never to contain values from outside 0-1 range,
			// and can be taken into buffer directly. All missing R,G,B components will be filled with 0s, missing alpha with 1s.
		case GL_RG8:
		case GL_RG16:
		case GL_R8:
		case GL_R16:
		case GL_RGB8:
		case GL_RGB5:
		case GL_R3_G3_B2:
		case GL_RGB4:

			bIgnoreAlpha = true;
			// pass-through

		case GL_RGBA8:
		case GL_RGBA12:
		case GL_RGBA16:
		case GL_RGB10_A2:
		case GL_RGBA4:
		case GL_RGB5_A1:
		case GL_SRGB8_ALPHA8:

			if( SlotType != EFramebufferAttachmentSlotType::Color )
			{
				UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Trying to receive depth or stencil information from color attachment! Internal format: %s"), GetGLInternalFormatString( InternalFormat ) );
				bIsAllOk = false;
			}
			else
			{
				glReadPixels( 0, 0, Width, Height, TextureOutputFormat, GL_UNSIGNED_INT_8_8_8_8_REV, RGBAData );
				ASSERT_NO_GL_ERROR();
			}
			break;

		case GL_RG16F:
		case GL_RG32F:
		case GL_R16F:
		case GL_R32F:
		case GL_R11F_G11F_B10F:

			bIgnoreAlpha = true;
			// pass-through

		case GL_RGBA32F:
		case GL_RGBA16F:
			if( SlotType != EFramebufferAttachmentSlotType::Color )
			{
				UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Trying to receive depth or stencil information from color attachment! Internal format: %s"), GetGLInternalFormatString( InternalFormat ) );
				bIsAllOk = false;
			}
			else
			{
				// These formats are stored internally as floats, and might conceivably contain values from outside 0-1 range.
				// Let's take the data from them into float buffer, check up on it, and if we detect values from outside 0-1 range,
				// report it and scale all components down by the same amount, so they fit in 0-1 range.

				int32 FloatRGBADataSize = sizeof(float) * RGBADataSize;
				float* FloatRGBAData = (float*)FMemory::Malloc( FloatRGBADataSize );
				glReadPixels( 0, 0, Width, Height, TextureOutputFormat, GL_FLOAT, FloatRGBAData );
				ASSERT_NO_GL_ERROR();

				int32 PixelComponentCount = RGBADataSize;

				// Determine minimal and maximal float value present in received data. Treat alpha separately.
				float MinValue[2] = { FLT_MAX };
				float MaxValue[2] = { FLT_MIN };
				float* DataPtr = FloatRGBAData;
				for( int32 PixelComponentIndex = 0; PixelComponentIndex < PixelComponentCount; ++PixelComponentIndex, ++DataPtr )
				{
					int32 AlphaValue = ( PixelComponentIndex % 4 == 3 ) ? 1 : 0;

					if( *DataPtr < MinValue[AlphaValue] )
					{
						MinValue[AlphaValue] = *DataPtr;
					}

					if( *DataPtr > MaxValue[AlphaValue] )
					{
						MaxValue[AlphaValue] = *DataPtr;
					}
				}

				// If necessary, rescale the data, announcing this fact.
				if( ( MinValue[0] < 0.f ) || ( MaxValue[0] > 1.f ) )
				{
					DataPtr = FloatRGBAData;
					float RescaleFactor = MaxValue[0] - MinValue[0];
					for( int32 PixelComponentIndex = 0; PixelComponentIndex < PixelComponentCount; ++PixelComponentIndex, ++DataPtr )
					{
						if( PixelComponentIndex % 4 != 3 )
						{
							*DataPtr = ( *DataPtr - MinValue[0] ) / RescaleFactor;
						}
					}

					// Add '_min%f_max%f' to end of file name, to let people know it's been rescaled.
					FilenameString += FString::Printf( TEXT("_min%f_max%f"), MinValue[0], MaxValue[0] );
				}

				if( !bIgnoreAlpha && ( ( MinValue[1] < 0.f ) || ( MaxValue[1] > 1.f ) ) )
				{
					DataPtr = FloatRGBAData;
					float RescaleFactor = MaxValue[1] - MinValue[1];
					for( int32 PixelComponentIndex = 0; PixelComponentIndex < PixelComponentCount; ++PixelComponentIndex, ++DataPtr )
					{
						if( PixelComponentIndex % 4 == 3 )
						{
							*DataPtr = ( *DataPtr - MinValue[1] ) / RescaleFactor;
						}
					}

					// Add '_amin%f_amax%f' to end of file name, to let people know it's been rescaled.
					FilenameString += FString::Printf( TEXT("_amin%f_amax%f"), MinValue[1], MaxValue[1] );
				}

				// Convert the data into RGBA8 buffer
				DataPtr = FloatRGBAData;
				uint8* TargetPtr = RGBAData;
				for( int32 PixelComponentIndex = 0; PixelComponentIndex < PixelComponentCount; ++PixelComponentIndex )
				{
					*TargetPtr++ = (uint8)( *DataPtr++ * 255.0f );
				}

				FMemory::Free( FloatRGBAData );
			}
			break;

		case GL_DEPTH_COMPONENT16:
		case GL_DEPTH_COMPONENT24:
		case GL_DEPTH_COMPONENT32F:

			if( SlotType != EFramebufferAttachmentSlotType::Depth )
			{
				UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Trying to receive color or stencil information from depth attachment! Internal format: %s"), GetGLInternalFormatString( InternalFormat ) );
				bIsAllOk = false;
			}
			else
			{
				// However these formats are stored internally, they can be received as floats.

				int32 DepthValueCount = RGBADataSize/4;
				int32 FloatDepthDataSize = sizeof(float) * DepthValueCount;
				float* FloatDepthData = (float*)FMemory::Malloc( FloatDepthDataSize );
				glReadPixels( 0, 0, Width, Height, GL_DEPTH_COMPONENT, GL_FLOAT, FloatDepthData );
				ASSERT_NO_GL_ERROR();

				// Determine minimal and maximal float value present in received data
				float MinValue = FLT_MAX;
				float MaxValue = FLT_MIN;
				float* DataPtr = FloatDepthData;
				for( int32 DepthValueIndex = 0; DepthValueIndex < DepthValueCount; ++DepthValueIndex, ++DataPtr )
				{
					if( *DataPtr < MinValue )
					{
						MinValue = *DataPtr;
					}

					if( *DataPtr > MaxValue )
					{
						MaxValue = *DataPtr;
					}
				}

				// If necessary, rescale the data.
				if( ( MinValue < 0.f ) || ( MaxValue > 1.f ) )
				{
					DataPtr = FloatDepthData;
					float RescaleFactor = MaxValue - MinValue;
					for( int32 DepthValueIndex = 0; DepthValueIndex < DepthValueCount; ++DepthValueIndex, ++DataPtr )
					{
						*DataPtr = ( *DataPtr - MinValue ) / RescaleFactor;
					}

					// Add '_min%f_max%f' to end of file name, to let people know it's been rescaled.
					FilenameString += FString::Printf( TEXT("_min%f_max%f"), MinValue, MaxValue );
				}

				// Convert the data into rgba8 buffer
				DataPtr = FloatDepthData;
				uint8* TargetPtr = RGBAData;
				for( int32 DepthValueIndex = 0; DepthValueIndex < DepthValueCount; ++DepthValueIndex )
				{
					uint8 Value = (uint8)( *DataPtr++ * 255.0f );
					*TargetPtr++ = Value;
					*TargetPtr++ = Value;
					*TargetPtr++ = Value;
					*TargetPtr++ = 255;
				}

				FMemory::Free( FloatDepthData );
			}
			break;

		case GL_DEPTH24_STENCIL8:
		case GL_DEPTH32F_STENCIL8:

			if( SlotType == EFramebufferAttachmentSlotType::Color )
			{
				UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Trying to receive color information from depth stencil attachment! Internal format: %s"), GetGLInternalFormatString( InternalFormat ) );
				bIsAllOk = false;
			}
			else if( SlotType == EFramebufferAttachmentSlotType::Depth )
			{
				// However these formats are stored internally, they can be received as floats.

				int32 DepthValueCount = RGBADataSize/4;
				int32 FloatDepthDataSize = sizeof(float) * DepthValueCount;
				float* FloatDepthData = (float*)FMemory::Malloc( FloatDepthDataSize );
				glReadPixels( 0, 0, Width, Height, GL_DEPTH_COMPONENT, GL_FLOAT, FloatDepthData );
				ASSERT_NO_GL_ERROR();

				// Determine minimal and maximal float value present in received data
				float MinValue = FLT_MAX;
				float MaxValue = FLT_MIN;
				float* DataPtr = FloatDepthData;
				for( int32 DepthValueIndex = 0; DepthValueIndex < DepthValueCount; ++DepthValueIndex, ++DataPtr )
				{
					if( *DataPtr < MinValue )
					{
						MinValue = *DataPtr;
					}

					if( *DataPtr > MaxValue )
					{
						MaxValue = *DataPtr;
					}
				}

				// If necessary, rescale the data.
				if( ( MinValue < 0.f ) || ( MaxValue > 1.f ) )
				{
					DataPtr = FloatDepthData;
					float RescaleFactor = MaxValue - MinValue;
					for( int32 DepthValueIndex = 0; DepthValueIndex < DepthValueCount; ++DepthValueIndex, ++DataPtr )
					{
						*DataPtr = ( *DataPtr - MinValue ) / RescaleFactor;
					}

					// Add '_min%f_max%f' to end of file name, to let people know it's been rescaled.
					FilenameString += FString::Printf( TEXT("_min%f_max%f"), MinValue, MaxValue );
				}

				// Convert the data into rgba8 buffer
				DataPtr = FloatDepthData;
				uint8* TargetPtr = RGBAData;
				for( int32 DepthValueIndex = 0; DepthValueIndex < DepthValueCount; ++DepthValueIndex )
				{
					uint8 Value = (uint8)( *DataPtr++ * 255.0f );
					*TargetPtr++ = Value;
					*TargetPtr++ = Value;
					*TargetPtr++ = Value;
					*TargetPtr++ = 255;
				}

				FMemory::Free( FloatDepthData );
			}
			else if( SlotType == EFramebufferAttachmentSlotType::Stencil )
			{
				int32 StencilValueCount = RGBADataSize/4;
				uint8* StencilData = (uint8*)FMemory::Malloc( StencilValueCount );
				glReadPixels( 0, 0, Width, Height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, StencilData );	// or GL_STENCIL_INDEX8 ?
				ASSERT_NO_GL_ERROR();

				// Convert the data into rgba8 buffer
				uint8* DataPtr = StencilData;
				uint8* TargetPtr = RGBAData;
				for( int32 StencilValueIndex = 0; StencilValueIndex < StencilValueCount; ++StencilValueIndex )
				{
					uint8 Value = *DataPtr++;
					*TargetPtr++ = Value;
					*TargetPtr++ = Value;
					*TargetPtr++ = Value;
					*TargetPtr++ = 255;
				}

				FMemory::Free( StencilData );
			}
			break;

		default:

			UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Unhandled internal texture format: %s (0x%x)!"), GetGLInternalFormatString( InternalFormat ), InternalFormat );
			bIsAllOk = false;
			break;
		}

		if( bIsAllOk )
		{
			if( bShouldFlipImageVertically )
			{
				// Flip image vertically, in-place
				uint32 Pitch = 4 * Width;
				uint8* LineBuffer = (uint8*)FMemory::Malloc( Pitch );
				for( int32 ImageRowIndex = 0; ImageRowIndex < Height / 2; ++ImageRowIndex )
				{
					memcpy( LineBuffer, RGBAData + ImageRowIndex * Pitch, Pitch );
					memcpy( RGBAData + ImageRowIndex * Pitch, RGBAData + ( Height - 1 - ImageRowIndex ) * Pitch, Pitch );
					memcpy( RGBAData + ( Height - 1 - ImageRowIndex ) * Pitch, LineBuffer, Pitch );
				}
				FMemory::Free( LineBuffer );
			}

#if USE_COMPRESSED_PNG_INSTEAD_OF_BMP_FOR_CONTENT_OUTPUT

			FilenameString += TEXT(".png");
			FString FilePath = *CachedEventFolder / FilenameString;

			if( bIgnoreAlpha && ( SlotType == EFramebufferAttachmentSlotType::Color ) )	// Alpha is 255 in depth and stencil already
			{
				// Make image non-transparent
				uint8* DataPtr = RGBAData+3;	// first alpha offset
				for( int32 PixelComponentIndex = 0; PixelComponentIndex < RGBADataSize/4; ++PixelComponentIndex )
				{
					*DataPtr = 255;
					DataPtr += 4;
				}
			}

			appCreatePNGWithAlpha( *FilePath, Width, Height, (FColor*)RGBAData );
#else
			FilenameString += TEXT(".bmp");
			FString FilePath = *CachedEventFolder / FilenameString;

			if( bIgnoreAlpha )
			{
				FFileHelper::CreateBitmap( *FilePath, Width, Height, (FColor*)RGBAData );
			}
			else
			{
				appCreateBitmapWithAlpha( *FilePath, Width, Height, (FColor*)RGBAData );
			}
#endif
		}

		FMemory::Free( RGBAData );
	}

	// Restore previous state
	if( FramebufferID != CurrentlyBoundReadFramebuffer )
	{
		glBindFramebuffer( GL_READ_FRAMEBUFFER, CurrentlyBoundReadFramebuffer );
		ASSERT_NO_GL_ERROR();
	}

	FOpenGL::ReadBuffer( CurrentReadBuffer );
	ASSERT_NO_GL_ERROR();
}

void FOpenGLDebugFrameDumper::DumpFramebufferContents( bool bReadFramebuffer )
{
	FString LogFileEnding;
	GLenum FramebufferBindingEnum;
	GLenum FramebufferTypeEnum;
	if( bReadFramebuffer )
	{
		LogFileEnding = TEXT("Read");
		FramebufferBindingEnum = GL_READ_FRAMEBUFFER_BINDING;
		FramebufferTypeEnum = GL_READ_FRAMEBUFFER;
	}
	else
	{
		LogFileEnding = TEXT("Draw");
		FramebufferBindingEnum = GL_DRAW_FRAMEBUFFER_BINDING;
		FramebufferTypeEnum = GL_DRAW_FRAMEBUFFER;
	}

	GLint CurrentlyBoundFramebuffer;
	glGetIntegerv( FramebufferBindingEnum, &CurrentlyBoundFramebuffer );
	ASSERT_NO_GL_ERROR();

	if( CurrentlyBoundFramebuffer == 0 )
	{
		// Screen buffer. Assumming it always has a front and back buffer.
		DumpFramebufferContent( 0, GL_FRONT_LEFT, *FString::Printf( TEXT("fbScreenFront%s"), *LogFileEnding ), EFramebufferAttachmentSlotType::Color, true );
		DumpFramebufferContent( 0, GL_BACK_LEFT, *FString::Printf( TEXT("fbScreenBack%s"), *LogFileEnding ), EFramebufferAttachmentSlotType::Color, true );

		DumpFramebufferContent( 0, GL_DEPTH, *FString::Printf( TEXT("fbScreenDepth%s"), *LogFileEnding ), EFramebufferAttachmentSlotType::Depth, true );

		// Commented out as Windows OpenGL provides no way to find out if screen buffer contains stencil, and causes OpenGL errors when it doesn't contain it.
//		DumpFramebufferContent( 0, GL_STENCIL, *FString::Printf( TEXT("fbScreenStencil%s"), *LogFileEnding ), EFramebufferAttachmentSlotType::Stencil, true );
	}
	else
	{
		GLint MaxColorAttachments;
		glGetIntegerv( GL_MAX_COLOR_ATTACHMENTS, &MaxColorAttachments );
		ASSERT_NO_GL_ERROR();

		GLint AttachmentType;

		for( int32 AttachmentIndex = 0; AttachmentIndex < MaxColorAttachments; ++AttachmentIndex )
		{
			glGetFramebufferAttachmentParameteriv( FramebufferTypeEnum, GL_COLOR_ATTACHMENT0 + AttachmentIndex, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &AttachmentType );
			ASSERT_NO_GL_ERROR();

			if( AttachmentType != GL_NONE )
			{
				DumpFramebufferContent( CurrentlyBoundFramebuffer, GL_COLOR_ATTACHMENT0 + AttachmentIndex, *FString::Printf( TEXT("fb%d%s"), AttachmentIndex, *LogFileEnding ), EFramebufferAttachmentSlotType::Color, false );
			}
		}

		glGetFramebufferAttachmentParameteriv( FramebufferTypeEnum, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &AttachmentType );
		ASSERT_NO_GL_ERROR();
		if( AttachmentType != GL_NONE )
		{
			DumpFramebufferContent( CurrentlyBoundFramebuffer, GL_DEPTH_ATTACHMENT, *FString::Printf( TEXT("fbDepth%s"), *LogFileEnding ), EFramebufferAttachmentSlotType::Depth, false );
		}

		glGetFramebufferAttachmentParameteriv( FramebufferTypeEnum, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &AttachmentType );
		ASSERT_NO_GL_ERROR();
		if( AttachmentType != GL_NONE )
		{
			DumpFramebufferContent( CurrentlyBoundFramebuffer, GL_STENCIL_ATTACHMENT, *FString::Printf( TEXT("fbStencil%s"), *LogFileEnding ), EFramebufferAttachmentSlotType::Stencil, false );
		}
	}
}

void FOpenGLDebugFrameDumper::DumpTextureSurfaceContent( const TCHAR* TargetFilename, GLenum SurfaceType, GLint Level )
{
	GLint Samples;
	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_SAMPLES, &Samples );
	ASSERT_NO_GL_ERROR();

	if( Samples != 0 )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Texture surface we try to get data for is multisampled! Add code to handle this when you need it.") );
		return;
	}

	GLint InternalFormat;
	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_INTERNAL_FORMAT, &InternalFormat );
	ASSERT_NO_GL_ERROR();

	GLint Width;
	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_WIDTH, &Width );
	ASSERT_NO_GL_ERROR();

	GLint Height;
	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_HEIGHT, &Height );
	ASSERT_NO_GL_ERROR();

	GLint Compressed;
	glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_COMPRESSED, &Compressed );
	ASSERT_NO_GL_ERROR();
	if( Compressed != 0 )
	{
		// Save specific level and target (face) of a texture in a DDS file

		GLint CompressedSize;
		glGetTexLevelParameteriv( SurfaceType, Level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &CompressedSize );
		ASSERT_NO_GL_ERROR();

		uint8* CompressedData = (uint8*)FMemory::Malloc( CompressedSize );
		glGetCompressedTexImage( SurfaceType, Level, CompressedData );
		ASSERT_NO_GL_ERROR();

		FString FilenameString( TargetFilename );
		FString FilePath = *CachedEventFolder / ( FilenameString + TEXT(".dds") );
		appCreateDDSWithSingleSurface( *FilePath, Width, Height, InternalFormat, CompressedData, CompressedSize );

		FMemory::Free( CompressedData );
	}
	else
	{
		FString FilenameString( TargetFilename );

		// Save non-compressed format

		int32 RGBADataSize = 4 * Width * Height;
		uint8* RGBAData = (uint8*)FMemory::Malloc( RGBADataSize );

		bool bIgnoreAlpha = false;
		bool bTextureTypeIsColor = false;

		switch( InternalFormat )
		{
			// These texture formats can reasonably be expected never to contain values from outside 0-1 range,
			// and can be taken into buffer directly. All missing R,G,B components will be filled with 0s, missing alpha with 1s.

		case GL_RG8:
		case GL_RG16:
		case GL_R8:
		case GL_R16:
		case GL_RGB8:
		case GL_RGB5:
		case GL_R3_G3_B2:
		case GL_RGB4:

			bIgnoreAlpha = true;
			// pass-through

		case GL_RGBA8:
		case GL_RGBA12:
		case GL_RGBA16:
		case GL_RGB10_A2:
		case GL_RGBA4:
		case GL_RGB5_A1:
		case GL_SRGB8_ALPHA8:

			glGetTexImage( SurfaceType, Level, TextureOutputFormat, GL_UNSIGNED_INT_8_8_8_8_REV, RGBAData );
			ASSERT_NO_GL_ERROR();
			bTextureTypeIsColor = true;
			break;

		case GL_RG16F:
		case GL_RG32F:
		case GL_R16F:
		case GL_R32F:
		case GL_R11F_G11F_B10F:

			bIgnoreAlpha = true;
			// pass-through

		case GL_RGBA32F:
		case GL_RGBA16F:

			// These texture formats are stored internally as floats, and might conceivably contain values from outside 0-1 range.
			// Let's take the data from them into float buffer, check up on it, and if we detect values from outside 0-1 range,
			// report it and scale all components down by the same amount, so they fit in 0-1 range.

			bTextureTypeIsColor = true;

			{
				// Determine minimal and maximal float value present in received data. Treat alpha separately.
				int32 FloatRGBADataSize = sizeof(float) * RGBADataSize;
				float* FloatRGBAData = (float*)FMemory::Malloc( FloatRGBADataSize );
				glGetTexImage( SurfaceType, Level, TextureOutputFormat, GL_FLOAT, FloatRGBAData );
				ASSERT_NO_GL_ERROR();

				int32 PixelComponentCount = RGBADataSize;

				// Determine minimal and maximal float value present in received data. Treat alpha separately.
				float MinValue[2] = { FLT_MAX };
				float MaxValue[2] = { FLT_MIN };
				float* DataPtr = FloatRGBAData;
				for( int32 PixelComponentIndex = 0; PixelComponentIndex < PixelComponentCount; ++PixelComponentIndex, ++DataPtr )
				{
					int32 AlphaValue = ( PixelComponentIndex % 4 == 3 ) ? 1 : 0;

					if( *DataPtr < MinValue[AlphaValue] )
					{
						MinValue[AlphaValue] = *DataPtr;
					}

					if( *DataPtr > MaxValue[AlphaValue] )
					{
						MaxValue[AlphaValue] = *DataPtr;
					}
				}

				// If necessary, rescale the data.
				if( ( MinValue[0] < 0.f ) || ( MaxValue[0] > 1.f ) )
				{
					DataPtr = FloatRGBAData;
					float RescaleFactor = MaxValue[0] - MinValue[0];
					for( int32 PixelComponentIndex = 0; PixelComponentIndex < PixelComponentCount; ++PixelComponentIndex, ++DataPtr )
					{
						if( PixelComponentIndex % 4 != 3 )
						{
							*DataPtr = ( *DataPtr - MinValue[0] ) / RescaleFactor;
						}
					}

					// Add '_min%f_max%f' to end of file name, to let people know it's been rescaled.
					FilenameString += FString::Printf( TEXT("_min%f_max%f"), MinValue[0], MaxValue[0] );
				}

				if( !bIgnoreAlpha && ( ( MinValue[1] < 0.f ) || ( MaxValue[1] > 1.f ) ) )
				{
					DataPtr = FloatRGBAData;
					float RescaleFactor = MaxValue[1] - MinValue[1];
					for( int32 PixelComponentIndex = 0; PixelComponentIndex < PixelComponentCount; ++PixelComponentIndex, ++DataPtr )
					{
						if( PixelComponentIndex % 4 == 3 )
						{
							*DataPtr = ( *DataPtr - MinValue[1] ) / RescaleFactor;
						}
					}

					// Add '_amin%f_amax%f' to end of file name, to let people know it's been rescaled.
					FilenameString += FString::Printf( TEXT("_amin%f_amax%f"), MinValue[1], MaxValue[1] );
				}

				// Convert the data into RGBA8 buffer
				DataPtr = FloatRGBAData;
				uint8* TargetPtr = RGBAData;
				for( int32 PixelComponentIndex = 0; PixelComponentIndex < PixelComponentCount; ++PixelComponentIndex )
				{
					*TargetPtr++ = (uint8)( *DataPtr++ * 255.0f );
				}

				FMemory::Free( FloatRGBAData );
			}
			break;

		case GL_DEPTH_COMPONENT32F:
		case GL_DEPTH32F_STENCIL8:
		case GL_DEPTH_COMPONENT16:
		case GL_DEPTH_COMPONENT24:
		case GL_DEPTH24_STENCIL8:

			// However these formats are stored internally, they can be received as floats.
			// Not trying to get a stencil component out of a texture - assumming it's always depth that's interesting.

			{
				int32 DepthValueCount = RGBADataSize/4;
				int32 FloatDepthDataSize = sizeof(float) * DepthValueCount;
				float* FloatDepthData = (float*)FMemory::Malloc( FloatDepthDataSize );
				glGetTexImage( SurfaceType, Level, GL_DEPTH_COMPONENT, GL_FLOAT, FloatDepthData );
				ASSERT_NO_GL_ERROR();

				// Determine minimal and maximal float value present in received data
				float MinValue = FLT_MAX;
				float MaxValue = FLT_MIN;
				float* DataPtr = FloatDepthData;
				for( int32 DepthValueIndex = 0; DepthValueIndex < DepthValueCount; ++DepthValueIndex, ++DataPtr )
				{
					if( *DataPtr < MinValue )
					{
						MinValue = *DataPtr;
					}

					if( *DataPtr > MaxValue )
					{
						MaxValue = *DataPtr;
					}
				}

				// If necessary, rescale the data.
				if( ( MinValue < 0.f ) || ( MaxValue > 1.f ) )
				{
					DataPtr = FloatDepthData;
					float RescaleFactor = MaxValue - MinValue;
					for( int32 DepthValueIndex = 0; DepthValueIndex < DepthValueCount; ++DepthValueIndex, ++DataPtr )
					{
						*DataPtr = ( *DataPtr - MinValue ) / RescaleFactor;
					}

					// Add '_min%f_max%f' to end of file name, to let people know it's been rescaled.
					FilenameString += FString::Printf( TEXT("_min%f_max%f"), MinValue, MaxValue );
				}

				// Convert the data into rgba8 buffer
				DataPtr = FloatDepthData;
				uint8* TargetPtr = RGBAData;
				for( int32 DepthValueIndex = 0; DepthValueIndex < DepthValueCount; ++DepthValueIndex )
				{
					uint8 Value = (uint8)( *DataPtr++ * 255.0f );
					*TargetPtr++ = Value;
					*TargetPtr++ = Value;
					*TargetPtr++ = Value;
					*TargetPtr++ = 255;
				}

				FMemory::Free( FloatDepthData );

				bIgnoreAlpha = true;
			}
			break;

		default:

			UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Unhandled internal texture format: %s!"), GetGLInternalFormatString( InternalFormat ) );
			return;
		}

		{
			// Flip image vertically (texture image data is stored in different row order than on Windows)
			uint32 Pitch = 4 * Width;
			uint8* LineBuffer = (uint8*)FMemory::Malloc( Pitch );
			for( int32 ImageRowIndex = 0; ImageRowIndex < Height / 2; ++ImageRowIndex )
			{
				memcpy( LineBuffer, RGBAData + ImageRowIndex * Pitch, Pitch );
				memcpy( RGBAData + ImageRowIndex * Pitch, RGBAData + ( Height - 1 - ImageRowIndex ) * Pitch, Pitch );
				memcpy( RGBAData + ( Height - 1 - ImageRowIndex ) * Pitch, LineBuffer, Pitch );
			}
			FMemory::Free( LineBuffer );
		}

#if USE_COMPRESSED_PNG_INSTEAD_OF_BMP_FOR_CONTENT_OUTPUT

		FilenameString += TEXT(".png");
		FString FilePath = *CachedEventFolder / FilenameString;

		if( bIgnoreAlpha && bTextureTypeIsColor )
		{
			// Make image non-transparent
			uint8* DataPtr = RGBAData+3;	// first alpha offset
			for( int32 PixelComponentIndex = 0; PixelComponentIndex < RGBADataSize/4; ++PixelComponentIndex )
			{
				*DataPtr = 255;
				DataPtr += 4;
			}
		}

		appCreatePNGWithAlpha( *FilePath, Width, Height, (FColor*)RGBAData );
#else
		FilenameString += TEXT(".bmp");
		FString FilePath = *CachedEventFolder / FilenameString;

		if( bIgnoreAlpha )
		{
			FFileHelper::CreateBitmap( *FilePath, Width, Height, (FColor*)RGBAData );
		}
		else
		{
			appCreateBitmapWithAlpha( *FilePath, Width, Height, (FColor*)RGBAData );
		}
#endif

		FMemory::Free( RGBAData );
	}
}

void FOpenGLDebugFrameDumper::DumpTextureContentForImageUnit( int32 UnitIndex )
{
	glActiveTexture( GL_TEXTURE0+UnitIndex );
	ASSERT_NO_GL_ERROR();

	GLint TextureID;

	glGetIntegerv( GL_TEXTURE_BINDING_1D, &TextureID );
	ASSERT_NO_GL_ERROR();
	if( TextureID )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Unit %d, texture binding 1D = %d, texture dump unhandled!"), UnitIndex, TextureID );
	}

	glGetIntegerv( GL_TEXTURE_BINDING_2D, &TextureID );
	ASSERT_NO_GL_ERROR();
	if( TextureID )
	{
		GLint BaseLevel;
		glGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &BaseLevel );
		ASSERT_NO_GL_ERROR();

		GLint MaxLevel;
		glGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &MaxLevel );
		ASSERT_NO_GL_ERROR();

		for( GLint Level = BaseLevel; Level <= MaxLevel; ++Level )
		{
			DumpTextureSurfaceContent( *FString::Printf( TEXT("tex%d_2D_id%d_lvl%d"), UnitIndex, TextureID, Level ), GL_TEXTURE_2D, Level );
		}
	}

	glGetIntegerv( GL_TEXTURE_BINDING_2D_MULTISAMPLE, &TextureID );
	ASSERT_NO_GL_ERROR();
	if( TextureID )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Unit %d, texture binding 2D multisample = %d, texture dump unhandled!"), UnitIndex, TextureID );
	}

	glGetIntegerv( GL_TEXTURE_BINDING_3D, &TextureID );
	ASSERT_NO_GL_ERROR();
	if( TextureID )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Unit %d, texture binding 3D = %d, texture dump unhandled!"), UnitIndex, TextureID );
	}

	glGetIntegerv( GL_TEXTURE_BINDING_BUFFER, &TextureID );
	ASSERT_NO_GL_ERROR();
	if( TextureID )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Unit %d, texture binding buffer = %d, texture dump unhandled!"), UnitIndex, TextureID );
	}

	glGetIntegerv( GL_TEXTURE_BINDING_CUBE_MAP, &TextureID );
	ASSERT_NO_GL_ERROR();
	if( TextureID )
	{
		GLint BaseLevel;
		glGetTexParameteriv( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, &BaseLevel );
		ASSERT_NO_GL_ERROR();

		GLint MaxLevel;
		glGetTexParameteriv( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, &MaxLevel );
		ASSERT_NO_GL_ERROR();

		for( GLint Level = BaseLevel; Level <= MaxLevel; ++Level )
		{
			for( GLint Face = 0; Face < 6; ++Face )
			{
				DumpTextureSurfaceContent( *FString::Printf( TEXT("tex%d_2D_id%d_lvl%d_face%d"), UnitIndex, TextureID, Level, Face ), GL_TEXTURE_CUBE_MAP_POSITIVE_X+Face, Level );
			}
		}
	}
}

void FOpenGLDebugFrameDumper::DumpBoundTexturesContents( void )
{
	// Remember this value, as we need to restore it
	GLint ActiveTextureUnit;
	glGetIntegerv( GL_ACTIVE_TEXTURE, &ActiveTextureUnit );
	ASSERT_NO_GL_ERROR();

	GLint MaxTextureImageUnits;
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &MaxTextureImageUnits );
	ASSERT_NO_GL_ERROR();

	for( int32 TextureImageUnitIndex = 0; TextureImageUnitIndex < MaxTextureImageUnits; ++TextureImageUnitIndex )
	{
		DumpTextureContentForImageUnit( TextureImageUnitIndex );
	}

	// Now restore value
	glActiveTexture( ActiveTextureUnit );
	ASSERT_NO_GL_ERROR();
}

void FOpenGLDebugFrameDumper::DumpElementArrayBufferContents( GLenum ElementArrayType )
{
	GLint ElementArrayBufferBinding;
	glGetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING, &ElementArrayBufferBinding );
	ASSERT_NO_GL_ERROR();

	if( ElementArrayBufferBinding == 0 )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: No valid OpenGL buffer bound to element array buffer binding point!") );
		return;
	}

	GLint BufferMapped;
	glGetBufferParameteriv( GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_MAPPED, &BufferMapped );
	ASSERT_NO_GL_ERROR();
	if( BufferMapped )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Can't map element array buffer %d for reading contents, as it's currently mapped!"), ElementArrayBufferBinding );
		return;
	}

	bool bIs32Bit = ( ElementArrayType == GL_UNSIGNED_INT );

	GLint64 TotalBufferSize = 0LL;	// getters apparently like to overwrite just some bytes of this on Mac, if they return small number, so we need to put 0 in both dwords initially
	glGetBufferParameteri64v( GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &TotalBufferSize );
	ASSERT_NO_GL_ERROR();

	GLuint BufferSize = (GLuint)TotalBufferSize;	// yes, I know it's conversion to shorter variable, I don't expect buffers > 4GB in size.

	void* BufferPtr = glMapBufferRange( GL_ELEMENT_ARRAY_BUFFER, 0, BufferSize, GL_MAP_READ_BIT );
	ASSERT_NO_GL_ERROR();
	if( !BufferPtr )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Failed to map element array buffer %d!"), ElementArrayBufferBinding );
		return;
	}

	FString LogFileName = *CachedEventFolder / TEXT("elementArrayBuffer.log");
	FOutputDeviceFile LogFile( *LogFileName );
	LogFile.SetAutoEmitLineTerminator( false );
	LogFile.Log( LINE_TERMINATOR );	// to end "log start" line

	uint32 ElementCount = BufferSize / ( bIs32Bit ? sizeof(uint32) : sizeof(uint16) );

	LogFile.Logf( TEXT("Index buffer %d, size %u, element count %d, %s") LINE_TERMINATOR, ElementArrayBufferBinding, BufferSize, ElementCount, bIs32Bit ? TEXT("32-bit") : TEXT("16-bit") );
	LogFile.Log( TEXT("=========================================================================") LINE_TERMINATOR );

	FString Line = TEXT("");
	int32 ValuesInLine = 0;
	uint32* Ptr32 = (uint32*)BufferPtr;
	uint16* Ptr16 = (uint16*)BufferPtr;
	uint32 LowestValue = 0xFFFFFFFF;
	uint32 HighestValue = 0L;
	for( uint32 ElementIndex = 0; ElementIndex < ElementCount; ++ElementIndex )
	{
		Line += ( ValuesInLine ? TEXT(", ") : TEXT( "\t" ) );

		uint32 Value = ( bIs32Bit ? *Ptr32++ : *Ptr16++ );

		if( LowestValue > Value )
		{
			LowestValue = Value;
		}

		if( HighestValue < Value )
		{
			HighestValue = Value;
		}

		Line += FString::Printf( TEXT("%u"), Value );

		++ValuesInLine;
		if( ValuesInLine >= 20 )
		{
			// Log line
			Line += TEXT( ",\n" );
			LogFile.Log( *Line );
			Line = TEXT("");
			ValuesInLine = 0;
		}
	}

	if( ValuesInLine )
	{
		// Move last line to file
		Line += LINE_TERMINATOR;
		LogFile.Log( *Line );
	}

	LogFile.Log( TEXT("=========================================================================") LINE_TERMINATOR );
	LogFile.Logf( TEXT("Lowest value in buffer: %u, highest: %u") LINE_TERMINATOR, LowestValue, HighestValue );

	LogFile.TearDown();

	glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER );
	ASSERT_NO_GL_ERROR();
}

inline uint32 HalfFloatToFloatInteger( uint16 HalfFloat )
{
	register uint32 Sign = ( HalfFloat >> 15 ) & 0x00000001;
	register uint32 Exponent = ( HalfFloat >> 10 ) & 0x0000001f;
	register uint32 Mantiss = HalfFloat & 0x000003ff;

	if( Exponent == 0 )
	{
		if( Mantiss == 0 ) // Plus or minus zero
		{
			return Sign << 31;
		}
		else // Denormalized number -- renormalize it
		{
			while( ( Mantiss & 0x00000400 ) == 0 )
			{
				Mantiss <<= 1;
				Exponent -= 1;
			}

			Exponent += 1;
			Mantiss &= ~0x00000400;
		}
	}
	else if( Exponent == 31 )
	{
		if( Mantiss == 0 ) // Inf
			return ( Sign << 31 ) | 0x7f800000;
		else // NaN
			return ( Sign << 31 ) | 0x7f800000 | ( Mantiss << 13 );
	}

	Exponent = Exponent + ( 127 - 15 );
	Mantiss = Mantiss << 13;

	return ( Sign << 31 ) | ( Exponent << 23 ) | Mantiss;
}

inline float HalfFloatToFloat( uint16 HalfFloat )
{
	union
	{
		float F;
		uint32 I;
	} Convert;

	Convert.I = HalfFloatToFloatInteger( HalfFloat );
	return Convert.F;
}

void FOpenGLDebugFrameDumper::DumpBoundVertexArrayBufferContents( GLint VertexBufferID, GLint StartVertex, GLint VertexCount, GLint InstanceCount )
{
	GLint BufferMapped;
	glGetBufferParameteriv( GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &BufferMapped );
	ASSERT_NO_GL_ERROR();

	if( BufferMapped )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Can't map vertex array buffer %d for reading contents, as it's currently mapped!"), VertexBufferID );
		return;
	}

	GLint64 TotalBufferSize = 0LL;	// getters apparently like to overwrite just some bytes of this on Mac, if they return small number, so we need to put 0 in both dwords initially
	glGetBufferParameteri64v( GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &TotalBufferSize );
	ASSERT_NO_GL_ERROR();

	GLuint BufferSize = (GLuint)TotalBufferSize;	// yes, I know it's conversion to shorter variable, I don't expect buffers > 4GB in size.

	void* BufferPtr = glMapBufferRange( GL_ARRAY_BUFFER, 0, BufferSize, GL_MAP_READ_BIT );
	ASSERT_NO_GL_ERROR();
	if( !BufferPtr )
	{
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Failed to map vertex array buffer %d!"), VertexBufferID );
		return;
	}

	// Now gather information for this buffer from vertex attrib arrays
	GLint MaxVertexAttribs = 0;
	glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &MaxVertexAttribs );
	ASSERT_NO_GL_ERROR();

	VertexAttribInfo* VertexAttribInfoTable = (VertexAttribInfo*)FMemory::Malloc( sizeof(VertexAttribInfo) * MaxVertexAttribs );
	if( !VertexAttribInfoTable )
	{
		glUnmapBuffer( GL_ARRAY_BUFFER );
		ASSERT_NO_GL_ERROR();
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Failed to allocate memory for vertex attrib info table!") );
		return;
	}

	GLint CommonStrideForAllAttributes = -1;
	GLint CommonDivisorForAllAttributes = -1;
	bool bCanUseCommonStrideAndDivisor = true;
	uint32 RelevantVertexAttribs = 0;

	for( GLint VertexAttribIndex = 0; VertexAttribIndex < MaxVertexAttribs; ++VertexAttribIndex )
	{
		GLint VertexAttribArrayEnabled;
		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &VertexAttribArrayEnabled );
		ASSERT_NO_GL_ERROR();
		if( !VertexAttribArrayEnabled )
		{
			continue;
		}

		GLint VertexAttribArrayBufferBinding;
		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VertexAttribArrayBufferBinding );
		ASSERT_NO_GL_ERROR();
		if( VertexAttribArrayBufferBinding != VertexBufferID )
		{
			continue;
		}

		bool bDifferentStrideOrDivisor = false;

		VertexAttribInfoTable[RelevantVertexAttribs].Index = VertexAttribIndex;

		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_SIZE, &VertexAttribInfoTable[RelevantVertexAttribs].SizeRead );
		ASSERT_NO_GL_ERROR();
		VertexAttribInfoTable[RelevantVertexAttribs].Size = ( VertexAttribInfoTable[RelevantVertexAttribs].SizeRead != GL_BGRA ) ? VertexAttribInfoTable[RelevantVertexAttribs].SizeRead : 4;
		check( VertexAttribInfoTable[RelevantVertexAttribs].Size <= 4 );

		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_STRIDE, (GLint*)&VertexAttribInfoTable[RelevantVertexAttribs].Stride );
		ASSERT_NO_GL_ERROR();

		if( CommonStrideForAllAttributes == -1 )
		{
			CommonStrideForAllAttributes = VertexAttribInfoTable[RelevantVertexAttribs].Stride;
		}
		else if( VertexAttribInfoTable[RelevantVertexAttribs].Stride != CommonStrideForAllAttributes )
		{
			bDifferentStrideOrDivisor = true;
		}

		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_TYPE, &VertexAttribInfoTable[RelevantVertexAttribs].Type );
		ASSERT_NO_GL_ERROR();

		GLint VertexAttribArrayNormalized;
		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &VertexAttribArrayNormalized );
		ASSERT_NO_GL_ERROR();
		VertexAttribInfoTable[RelevantVertexAttribs].bNormalized = ( VertexAttribArrayNormalized != 0 );

		GLvoid* VertexAttribArrayPointer;
		glGetVertexAttribPointerv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_POINTER, &VertexAttribArrayPointer );
		ASSERT_NO_GL_ERROR();
		VertexAttribInfoTable[RelevantVertexAttribs].Offset = (GLuint)((GLuint64)VertexAttribArrayPointer & 0xFFFFFFFF);

		GLint VertexAttribArrayInteger;
		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &VertexAttribArrayInteger );
		ASSERT_NO_GL_ERROR();
		VertexAttribInfoTable[RelevantVertexAttribs].bInteger = ( VertexAttribArrayInteger != 0 );

		GLint VertexAttribArrayDivisor;
		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ARB, &VertexAttribArrayDivisor );
		ASSERT_NO_GL_ERROR();
		if( ( VertexAttribArrayDivisor != 0 ) && ( VertexAttribArrayDivisor != 1 ) )
		{
			UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Vertex array buffer %d has non-typical divisor: %d"), VertexBufferID, VertexAttribArrayDivisor );
		}
		VertexAttribInfoTable[RelevantVertexAttribs].bDivisor = ( VertexAttribArrayDivisor != 0 );

		if( CommonDivisorForAllAttributes == -1 )
		{
			CommonDivisorForAllAttributes = VertexAttribInfoTable[RelevantVertexAttribs].bDivisor;
		}
		else if( VertexAttribInfoTable[RelevantVertexAttribs].bDivisor != ( CommonDivisorForAllAttributes != 0 ) )
		{
			bDifferentStrideOrDivisor = true;
		}

		if( bDifferentStrideOrDivisor && bCanUseCommonStrideAndDivisor )
		{
			bCanUseCommonStrideAndDivisor = false;
		}

		++RelevantVertexAttribs;
	}

	if( RelevantVertexAttribs == 0 )
	{
		glUnmapBuffer( GL_ARRAY_BUFFER );
		ASSERT_NO_GL_ERROR();
		FMemory::Free( VertexAttribInfoTable );
		UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Vertex array buffer %d isn't bound to any vertex attribs, despite it being chosen."), VertexBufferID );
		return;
	}

	// Sort vertex attribs info by increasing offset into vertex
	qsort( VertexAttribInfoTable, RelevantVertexAttribs, sizeof( VertexAttribInfo ), qsort_compare_VertexAttribInfo );

	// Adjust offset within vertex, assumming that one of those is at zero offset within vertex
	{
		GLuint BaseOffset = VertexAttribInfoTable[0].Offset;
		for( uint32 VertexAttribIndex = 0; VertexAttribIndex < RelevantVertexAttribs; ++VertexAttribIndex )
		{
			VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex = VertexAttribInfoTable[VertexAttribIndex].Offset - BaseOffset;
		}
	}

	// Determine if the buffer is indexed, or instanced
	if( InstanceCount && ( CommonDivisorForAllAttributes == 1 ) )
	{
		// Instanced buffer
		StartVertex = 0;
		VertexCount = InstanceCount;
	}

	FString LogFileName = *CachedEventFolder / FString::Printf( TEXT("vertexArrayBuffer%d.log"), VertexBufferID );
	FOutputDeviceFile LogFile( *LogFileName );
	LogFile.SetAutoEmitLineTerminator( false );
	LogFile.Log( LINE_TERMINATOR );	// to end "log start" line

	// Output header info ( bufferID, stride, size, vertexCount )
	LogFile.Logf( TEXT("Vertex buffer %d, size %u, start vertex for the draw within buffer %u, vertex count for the draw %d:") LINE_TERMINATOR, VertexBufferID, BufferSize, StartVertex, VertexCount );
	if( !bCanUseCommonStrideAndDivisor )
	{
		LogFile.Log( TEXT("(different attributes of the same buffer are placed with different stride or divisor, so it's impossible to determine unused parts of vertex)") LINE_TERMINATOR );
	}
	LogFile.Log( TEXT("============================ VERTEX BUFFER INFO SET UP IN VERTEX ATTRIBS =======================================") LINE_TERMINATOR );

	// Go through vertex attrib info and output offset, size, normalized, integer, type for each attrib.
	// If stride differs, or attrib is integer, say it won't be logged

	GLuint OffsetCovered = 0;
	for( uint32 VertexAttribIndex = 0; VertexAttribIndex < RelevantVertexAttribs; ++VertexAttribIndex )
	{
		VertexAttribInfoTable[VertexAttribIndex].bSkip = false;
		if( bCanUseCommonStrideAndDivisor )
		{
			if( OffsetCovered < VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex )
			{
				LogFile.Logf( TEXT("\tOffset: %d - %d unidentified bytes") LINE_TERMINATOR, VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex, VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex - OffsetCovered );
			}
			else if( OffsetCovered > VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex )
			{
				LogFile.Logf( TEXT("\t\t%d BYTES ARE SHARED WITH THE FOLLOWING ATTRIBUTE!") LINE_TERMINATOR, OffsetCovered - VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex );
				UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Event %d, vertex array buffer %d, vertex attrib at offset %d using exact same data as another attribute!"), EventCounter, VertexBufferID, VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex );
			}
		}
		LogFile.Logf( TEXT("\tOffset: %d (in buffer: %d ), Size: %d, type: %s, stride: %d, normalized: %s") LINE_TERMINATOR, VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex, VertexAttribInfoTable[VertexAttribIndex].Offset,
			VertexAttribInfoTable[VertexAttribIndex].Size, NameOfType( VertexAttribInfoTable[VertexAttribIndex].Type ), VertexAttribInfoTable[VertexAttribIndex].Stride, VertexAttribInfoTable[VertexAttribIndex].bNormalized ? TEXT("Yes") : TEXT("No") );

		if( VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex > VertexAttribInfoTable[VertexAttribIndex].Stride )
		{
			LogFile.Log( TEXT("\t\tTHIS ATTRIBUTE STARTS BEYOND THE END OF VERTEX DATA! IT WILL BE SKIPPED.") LINE_TERMINATOR );
			UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Event %d, ertex array buffer %d, vertex attrib at offset %d starts beyond end of vertex data!"), EventCounter, VertexBufferID, VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex );
			VertexAttribInfoTable[VertexAttribIndex].bSkip = true;
		}

		int32 SizeOfMember = VertexAttribInfoTable[VertexAttribIndex].Size * SizeOfType( VertexAttribInfoTable[VertexAttribIndex].Type );
		OffsetCovered = VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex + SizeOfMember;
		if( OffsetCovered > VertexAttribInfoTable[VertexAttribIndex].Stride )
		{
			LogFile.Log( TEXT("\t\tTHIS ATTRIBUTE ENDS BEYOND THE END OF VERTEX DATA! IT WILL BE SKIPPED.") LINE_TERMINATOR );
			UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Event %d, vertex array buffer %d, vertex attrib at offset %d ends beyond end of vertex data!"), EventCounter, VertexBufferID, VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex );
			VertexAttribInfoTable[VertexAttribIndex].bSkip = true;
		}

		if( VertexAttribInfoTable[VertexAttribIndex].Offset + StartVertex * VertexAttribInfoTable[VertexAttribIndex].Stride > BufferSize )
		{
			LogFile.Log( TEXT("\t\tVALUES FROM THIS ATTRIBUTE SUBMITTED FOR OPENGL TO DRAW START BEYOND BUFFER END! IT WILL BE SKIPPED.") LINE_TERMINATOR );
			UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Event %d, vertex array buffer %d, vertex attrib at offset %d - values from it submitted for OpenGL to draw start beyond buffer end!"), EventCounter, VertexBufferID, VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex );
			VertexAttribInfoTable[VertexAttribIndex].bSkip = true;
		}
		else if( VertexAttribInfoTable[VertexAttribIndex].Offset + ( StartVertex + VertexCount - 1 ) * VertexAttribInfoTable[VertexAttribIndex].Stride + SizeOfMember > BufferSize )
		{
			LogFile.Log( TEXT("\t\tVALUES FROM THIS ATTRIBUTE SUBMITTED FOR OPENGL TO DRAW EXTEND BEYOND BUFFER END! IT WILL BE SKIPPED.") LINE_TERMINATOR );
			UE_LOG( LogRHI, Warning, TEXT("DEBUG FRAME DUMPER: Event %d, vertex array buffer %d, vertex attrib at offset %d - values from it submitted for OpenGL to draw extend beyond buffer end!"), EventCounter, VertexBufferID, VertexAttribInfoTable[VertexAttribIndex].OffsetWithinVertex );
			VertexAttribInfoTable[VertexAttribIndex].bSkip = true;
		}
	}

	LogFile.Log( TEXT("================================= INTERPRETED VERTEX BUFFER CONTENTS ===========================================") LINE_TERMINATOR );

	// For each vertex, prepare a line with contributions from each attrib, and output it

	FString Line;
	for( int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex )
	{
		Line = TEXT("");
		for( uint32 VertexAttribIndex = 0; VertexAttribIndex < RelevantVertexAttribs; ++VertexAttribIndex )
		{
			if( VertexAttribInfoTable[VertexAttribIndex].bSkip )
			{
				continue;
			}

			if( Line.Len() )
			{
				Line += TEXT(", ");
			}
			else
			{
				Line = FString::Printf( TEXT("%08d: "), VertexIndex );
			}

			uint32 Offset = VertexAttribInfoTable[VertexAttribIndex].Offset + ( StartVertex + VertexIndex ) * VertexAttribInfoTable[VertexAttribIndex].Stride;
			int32 SizeOfMember = VertexAttribInfoTable[VertexAttribIndex].Size * SizeOfType( VertexAttribInfoTable[VertexAttribIndex].Type );
			if( Offset + SizeOfMember > BufferSize )
			{
				Line += TEXT("(beyond end of buffer)");
			}
			else
			{
				const uint8* ValuePtr = (const uint8*)BufferPtr + Offset;

				switch( VertexAttribInfoTable[VertexAttribIndex].Type )
				{
				case GL_FLOAT:
					{
						const float* FloatPtr = (const float*)ValuePtr;
						if( VertexAttribInfoTable[VertexAttribIndex].Size == 1 )
						{
							Line += FString::Printf( TEXT("%f"), FloatPtr[0] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 2 )
						{
							Line += FString::Printf( TEXT("{ %f, %f }"), FloatPtr[0], FloatPtr[1] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 3 )
						{
							Line += FString::Printf( TEXT("{ %f, %f, %f }"), FloatPtr[0], FloatPtr[1], FloatPtr[2] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 4 )
						{
							Line += FString::Printf( TEXT("{ %f, %f, %f, %f }"), FloatPtr[0], FloatPtr[1], FloatPtr[2], FloatPtr[3] );
						}
						else
						{
							Line += TEXT("(unhandled float count)");
						}
					}
					break;
						
				case GL_UNSIGNED_BYTE:
					{
						const uint8* Uint8Ptr = (const uint8*)ValuePtr;
						if( VertexAttribInfoTable[VertexAttribIndex].Size == 1 )
						{
							Line += FString::Printf( TEXT("%u"), Uint8Ptr[0] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 2 )
						{
							Line += FString::Printf( TEXT("{ %u, %u }"), Uint8Ptr[0], Uint8Ptr[1] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 3 )
						{
							Line += FString::Printf( TEXT("{ %u, %u, %u }"), Uint8Ptr[0], Uint8Ptr[1], Uint8Ptr[2] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 4 )
						{
							Line += FString::Printf( TEXT("{ %u, %u, %u, %u }"), Uint8Ptr[0], Uint8Ptr[1], Uint8Ptr[2], Uint8Ptr[3] );
						}
						else
						{
							Line += TEXT("(unhandled unsigned char count)");
						}
					}
					break;

				case GL_UNSIGNED_SHORT:
					{
						const uint16* UInt16Ptr = (const uint16*)ValuePtr;
						if( VertexAttribInfoTable[VertexAttribIndex].Size == 1 )
						{
							Line += FString::Printf( TEXT("%u"), UInt16Ptr[0] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 2 )
						{
							Line += FString::Printf( TEXT("{ %u, %u }"), UInt16Ptr[0], UInt16Ptr[1] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 3 )
						{
							Line += FString::Printf( TEXT("{ %u, %u, %u }"), UInt16Ptr[0], UInt16Ptr[1], UInt16Ptr[2] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 4 )
						{
							Line += FString::Printf( TEXT("{ %u, %u, %u, %u }"), UInt16Ptr[0], UInt16Ptr[1], UInt16Ptr[2], UInt16Ptr[3] );
						}
						else
						{
							Line += TEXT("(unhandled unsigned short count)");
						}
					}
					break;

				case GL_SHORT:
					{
						const int16* Int16Ptr = (const int16*)ValuePtr;
						if( VertexAttribInfoTable[VertexAttribIndex].Size == 1 )
						{
							Line += FString::Printf( TEXT("%d"), Int16Ptr[0] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 2 )
						{
							Line += FString::Printf( TEXT("{ %d, %d }"), Int16Ptr[0], Int16Ptr[1] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 3 )
						{
							Line += FString::Printf( TEXT("{ %d, %d, %d }"), Int16Ptr[0], Int16Ptr[1], Int16Ptr[2] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 4 )
						{
							Line += FString::Printf( TEXT("{ %d, %d, %d, %d }"), Int16Ptr[0], Int16Ptr[1], Int16Ptr[2], Int16Ptr[3] );
						}
						else
						{
							Line += TEXT("(unhandled short count)");
						}
					}
					break;

				case GL_HALF_FLOAT:
					if( VertexAttribInfoTable[VertexAttribIndex].Size > 4 )
					{
						Line += TEXT("(unhandled float count)");
					}
					else
					{
						float Floats[4];
						const uint16* UInt16Ptr = (const uint16*)ValuePtr;
						for( int32 MemberIndex = 0; MemberIndex < VertexAttribInfoTable[VertexAttribIndex].Size; ++MemberIndex )
							Floats[MemberIndex] = HalfFloatToFloat( UInt16Ptr[MemberIndex] );

						if( VertexAttribInfoTable[VertexAttribIndex].Size == 1 )
						{
							Line += FString::Printf( TEXT("%f"), Floats[0] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 2 )
						{
							Line += FString::Printf( TEXT("{ %f, %f }"), Floats[0], Floats[1] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 3 )
						{
							Line += FString::Printf( TEXT("{ %f, %f, %f }"), Floats[0], Floats[1], Floats[2] );
						}
						else if( VertexAttribInfoTable[VertexAttribIndex].Size == 4 )
						{
							Line += FString::Printf( TEXT("{ %f, %f, %f, %f }"), Floats[0], Floats[1], Floats[2], Floats[3] );
						}
						else
						{
							Line += TEXT("(unhandled float count)");
						}
					}
					break;

				default:
					Line += TEXT("(unhandled type)");
					break;
				}
			}
		}

		if( Line.Len() )
		{
			Line += LINE_TERMINATOR;
			LogFile.Log( *Line );
		}
	}

	LogFile.Log( TEXT("================================================================================================================") LINE_TERMINATOR );

	// Clean up
	LogFile.TearDown();
	glUnmapBuffer( GL_ARRAY_BUFFER );
	ASSERT_NO_GL_ERROR();
	FMemory::Free( VertexAttribInfoTable );
}

void FOpenGLDebugFrameDumper::DumpRelevantVertexArrayBufferContents( GLint StartVertex, GLint VertexCount, GLint InstanceCount )
{
	GLint MaxVertexAttribs = 0;
	glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &MaxVertexAttribs );
	ASSERT_NO_GL_ERROR();

	GLint IndicesToDump[64];
	GLint AttribsToDump[64];
	GLint DumpCount = 0;

	for( GLint VertexAttribIndex = 0; VertexAttribIndex < MaxVertexAttribs; ++VertexAttribIndex )
	{
		GLint VertexAttribArrayEnabled;
		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &VertexAttribArrayEnabled );
		ASSERT_NO_GL_ERROR();
		if( !VertexAttribArrayEnabled )
		{
			continue;
		}

		GLint VertexAttribArrayBufferBinding;
		glGetVertexAttribiv( VertexAttribIndex, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VertexAttribArrayBufferBinding );
		ASSERT_NO_GL_ERROR();

		bool bIsFound = false;
		for( GLint AlreadyThereIndex = 0; AlreadyThereIndex < DumpCount; ++AlreadyThereIndex )
		{
			if( IndicesToDump[AlreadyThereIndex] == VertexAttribArrayBufferBinding )
			{
				bIsFound = true;
				break;
			}
		}

		if( bIsFound )
		{
			continue;
		}

		IndicesToDump[DumpCount] = VertexAttribArrayBufferBinding;
		AttribsToDump[DumpCount] = VertexAttribIndex;
		++DumpCount;
		check( DumpCount < 64 );
	}

	if( DumpCount )
	{
		GLint PreviousVertexBuffer;
		glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &PreviousVertexBuffer );
		ASSERT_NO_GL_ERROR();

		GLint CurrentVertexBuffer = PreviousVertexBuffer;

		for( GLint ArrayToDumpIndex = 0; ArrayToDumpIndex < DumpCount; ++ArrayToDumpIndex )
		{
			GLint VertexBufferID = IndicesToDump[ArrayToDumpIndex];

			if( VertexBufferID != CurrentVertexBuffer )
			{
				glBindBuffer( GL_ARRAY_BUFFER, VertexBufferID );
				ASSERT_NO_GL_ERROR();
				CurrentVertexBuffer = VertexBufferID;
			}

			DumpBoundVertexArrayBufferContents( VertexBufferID, StartVertex, VertexCount, InstanceCount );
		}

		if( CurrentVertexBuffer != PreviousVertexBuffer )
		{
			glBindBuffer( GL_ARRAY_BUFFER, PreviousVertexBuffer );
			ASSERT_NO_GL_ERROR();
		}
	}
}

void FOpenGLDebugFrameDumper::TriggerFrameDump( void )
{
	if( bDumpingFrame )
		return;

	if( CachedRootFolder == NULL )
	{
		// Delete entire root frame dump folder with everything in it, if it exists.
		CachedRootFolder = new FString( FPaths::ProfilingDir() / TEXT("OpenGLDebugFrameDump") );
		IFileManager::Get().DeleteDirectory( **CachedRootFolder, false, true );

		// Create new root frame dump folder
		IFileManager::Get().MakeDirectory( **CachedRootFolder );
	}

	// Create new frame folder
	if( CachedFrameFolder )
		delete CachedFrameFolder;
	CachedFrameFolder = new FString( *CachedRootFolder / FString::Printf( TEXT( "Frame_%08u" ), FrameCounter ) );

	UE_LOG( LogRHI, Log, TEXT("DEBUG FRAME DUMPER: Frame %d dump started."), FrameCounter );

	EventCounter = 0;
	bDumpingFrame = true;
}

void FOpenGLDebugFrameDumper::SetNewEventFolder( const FString& EventString )
{
	// Create new event folder
	if( CachedEventFolder )
		delete CachedEventFolder;
	CachedEventFolder = new FString( *CachedFrameFolder / FString::Printf( TEXT( "Event_%08u-" ), EventCounter ) + EventString );
}

void FOpenGLDebugFrameDumper::SignalDrawEvent( const TCHAR* FolderPart, const TCHAR* DrawCommandDescription, GLint ElementArrayType, GLint StartVertex, GLint VertexCount, GLint InstanceCount )
{
	if( !bDumpingFrame )
		return;

	SetNewEventFolder( FolderPart );

	DumpGeneralOpenGLState( DrawCommandDescription, true, false );
	DumpFramebufferState( false );
	DumpProgramAndShaderState();
	DumpBoundTextureState();
	DumpFramebufferContents( false );
	DumpBoundTexturesContents();
	if( ElementArrayType != 0 )
	{
		DumpElementArrayBufferContents( ElementArrayType );
	}
	DumpRelevantVertexArrayBufferContents( StartVertex, VertexCount, InstanceCount );

	// TO DO - dump the rest of OpenGL state

	++EventCounter;
}

void FOpenGLDebugFrameDumper::SignalClearEvent( int8 ClearType, int8 NumColors, const float* Colors, float Depth, uint32 Stencil )
{
	if( !bDumpingFrame )
		return;

	SetNewEventFolder( TEXT("glClearBuffer(s)") );

	FString MaskString;
	{
		bool bHasText = false;
		MaskString = TEXT("");
		if( ClearType & 4 )
		{
			MaskString = FString::Printf( TEXT( "%d color buffers( " ), NumColors );
			for( int32 ColorIndex = 0; ColorIndex < NumColors; ++ColorIndex )
			{
				if( ColorIndex )
				{
					MaskString += TEXT(", ");
				}
				MaskString += *FString::Printf( TEXT("(%f,%f,%f,%f)"), Colors[4*ColorIndex], Colors[4*ColorIndex+1], Colors[4*ColorIndex+2], Colors[4*ColorIndex+3] ); 
			}
			MaskString += TEXT(" )");
			bHasText = true;
		}
		if( ClearType & 1 )
		{
			if( bHasText )
			{
				MaskString += TEXT(", ");
			}
			MaskString += *FString::Printf( TEXT( "depth(%f)" ), Depth );
			bHasText = true;
		}
		if( ClearType & 2 )
		{
			if( bHasText )
			{
				MaskString += TEXT(", ");
			}
			MaskString += *FString::Printf( TEXT( "stencil(0x%x)" ), Stencil );
		}
	}

	FString DrawCommandDescription = FString( TEXT("glClearBuffer*( ") ) + MaskString + TEXT(" )");
	DumpGeneralOpenGLState( *DrawCommandDescription, false, false );
	DumpFramebufferState( false );
	DumpFramebufferContents( false );

	++EventCounter;
}

void FOpenGLDebugFrameDumper::SignalFramebufferBlitEvent( GLbitfield Mask )
{
	if( !bDumpingFrame )
		return;

	SetNewEventFolder( TEXT("glFramebufferBlit") );

	FString MaskString;
	{
		MaskString = ( Mask & GL_COLOR_BUFFER_BIT ) ? TEXT( "GL_COLOR_BUFFER_BIT" ) : TEXT("");
		bool bHasText = ( ( Mask & GL_COLOR_BUFFER_BIT ) != 0 );
		if( Mask & GL_DEPTH_BUFFER_BIT )
		{
			if( bHasText )
				MaskString += TEXT("|");
			MaskString += TEXT( "GL_DEPTH_BUFFER_BIT" );
			bHasText = true;
		}
		if( Mask & GL_STENCIL_BUFFER_BIT )
		{
			if( bHasText )
				MaskString += TEXT("|");
			MaskString += TEXT( "GL_STENCIL_BUFFER_BIT" );
		}
	}

	FString DrawCommandDescription = FString( TEXT("glFramebufferBlit(") ) + MaskString + TEXT(")");
	DumpGeneralOpenGLState( *DrawCommandDescription, false, true );
	DumpFramebufferState( false );
	DumpFramebufferState( true );
	DumpFramebufferContents( false );
	DumpFramebufferContents( true );

	++EventCounter;
}

void FOpenGLDebugFrameDumper::SignalEndFrameEvent( void )
{
	if( !bDumpingFrame )
		return;

	SetNewEventFolder( TEXT("BufferFlush") );
	DumpGeneralOpenGLState( TEXT("(BufferFlush)"), false, false );
	DumpFramebufferContents( false );
	// TO DO - dump the rest of OpenGL state

	UE_LOG( LogRHI, Log, TEXT("DEBUG FRAME DUMPER: Frame %d dump ended."), FrameCounter );

	bDumpingFrame = false;
	EventCounter = 0;
	++FrameCounter;
}

/*=============================================================================
Implementation of C methods that serve as the only connection all external code may depend on.
=============================================================================*/

static const TCHAR* GetPrimitiveTypeString( GLint type )
{
	switch( type )
	{
	case GL_TRIANGLES:		return TEXT("GL_TRIANGLES");
	case GL_POINTS:			return TEXT("GL_POINTS");
	case GL_LINES:			return TEXT("GL_LINES");
	case GL_LINE_STRIP:		return TEXT("GL_LINE_STRIP");
	case GL_TRIANGLE_STRIP:	return TEXT("GL_TRIANGLE_STRIP");
	case GL_TRIANGLE_FAN:	return TEXT("GL_TRIANGLE_FAN");
	default:				return TEXT("!!!unknown!!!");
	}
}

extern "C" {

void SignalOpenGLDrawArraysEvent( GLenum Mode, GLint First, GLsizei Count )
{
	FOpenGLDebugFrameDumper::Instance()->SignalDrawEvent(
		TEXT("glDrawArrays"),
		*FString::Printf( TEXT( "glDrawArrays( Mode = %s, First = %d, Count = %u )" ),
			GetPrimitiveTypeString( Mode ),
			First,
			Count
		),
		0,	// no index buffer
		First,
		Count,
		0
	);
}

void SignalOpenGLDrawArraysInstancedEvent( GLenum Mode, GLint First, GLsizei Count, GLsizei PrimCount )
{
	FOpenGLDebugFrameDumper::Instance()->SignalDrawEvent(
		TEXT("glDrawArraysInstanced"),
		*FString::Printf(
			TEXT( "glDrawArraysInstanced( Mode = %s, First = %d, Count = %u, PrimCount = %u )" ),
			GetPrimitiveTypeString( Mode ),
			First,
			Count,
			PrimCount
		),
		0,	// no index buffer
		First,
		Count,
		PrimCount
	);
}

void SignalOpenGLDrawRangeElementsEvent( GLenum Mode, GLuint Start, GLuint End, GLsizei Count, GLenum Type, const GLvoid* Indices )
{
	FOpenGLDebugFrameDumper::Instance()->SignalDrawEvent(
		TEXT("glDrawRangeElements"),
		*FString::Printf(
			TEXT( "glDrawRangeElements( Mode = %s, Start = %d, End = %d, Count = %u, Type = %s, Indices = %p )" ),
			GetPrimitiveTypeString( Mode ),
			Start,
			End,
			Count,
			( ( Type == GL_UNSIGNED_INT ) ? TEXT("GL_UNSIGNED_INT") : TEXT("GL_UNSIGNED_SHORT") ),
			Indices
		),
		(GLint)Type,
		Start,
		End - Start,
		0
	);
}

void SignalOpenGLDrawRangeElementsInstancedEvent( GLenum Mode, GLsizei Count, GLenum Type, const GLvoid* Indices, GLsizei PrimCount )
{
	FOpenGLDebugFrameDumper::Instance()->SignalDrawEvent(
		TEXT("glDrawElementsInstanced"),
		*FString::Printf(
			TEXT( "glDrawElementsInstanced( Mode = %s, Count = %u, Type = %s, Indices = %p, PrimCount = %u )" ),
			GetPrimitiveTypeString( Mode ),
			Count,
			( ( Type == GL_UNSIGNED_INT ) ? TEXT("GL_UNSIGNED_INT") : TEXT("GL_UNSIGNED_SHORT") ),
			Indices,
			PrimCount
		),
		(GLint)Type,
		0,
		Count,
		PrimCount
	);
}

void SignalOpenGLClearEvent( int8 ClearType, int8 NumColors, const float* Colors, float Depth, uint32 Stencil )
{
	FOpenGLDebugFrameDumper::Instance()->SignalClearEvent( ClearType, NumColors, Colors, Depth, Stencil );
}

void SignalOpenGLFramebufferBlitEvent( GLbitfield Mask )
{
	FOpenGLDebugFrameDumper::Instance()->SignalFramebufferBlitEvent( Mask );
}

void SignalOpenGLEndFrameEvent( void )
{
	FOpenGLDebugFrameDumper::Instance()->SignalEndFrameEvent();
}

void TriggerOpenGLFrameDump( void )
{
	FOpenGLDebugFrameDumper::Instance()->TriggerFrameDump();
}

void TriggerOpenGLFrameDumpEveryXCalls( int32 X )
{
	static int32 Counter = 0;
	if( Counter >= X )
	{
		FOpenGLDebugFrameDumper::Instance()->TriggerFrameDump();
		Counter = 0;
	}
	++Counter;
}

}	// extern "C"

#endif	// ENABLE_OPENGL_FRAMEDUMP
