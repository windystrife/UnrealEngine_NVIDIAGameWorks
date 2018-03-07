/************************************************************************************

Filename    :   VrApi_Ext.h
Content     :   VrApi extensions support
Created     :   February 3, 2016
Authors     :   Cass Everitt
Language    :   C99

Copyright   :   Copyright 2016 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrApi_Ext_h
#define OVR_VrApi_Ext_h

#include "VrApi_Types.h"
#include "VrApi_Helpers.h"
#include "string.h"				// for memset()

//-----------------------------------------------------------------
// Basic Ext Types
//-----------------------------------------------------------------

// This type is just to make parm chain traversal simple
typedef struct ovrFrameParmsExtBase
{
	ovrStructureType Type;
	OVR_VRAPI_PADDING_64_BIT( 4 );
	struct ovrFrameParmsExtBase * Next;
} ovrFrameParmsExtBase;

///--BEGIN_SDK_REMOVE

/* Enumerants that can be used in calls to vrapi_GetSystemProperty() */
#define VRAPI_EXT_BASE 0x10000000

#define VRAPI_REMAP_2D_EXT 					((ovrSystemProperty)(VRAPI_EXT_BASE + 2))
#define VRAPI_EXTENDED_FRAME_PARMS_EXT		((ovrSystemProperty)(VRAPI_EXT_BASE + 3))
#define VRAPI_REMAP_2D_HEMICYL_EXT			((ovrSystemProperty)(VRAPI_EXT_BASE + 4))
#define VRAPI_OFFCENTER_CUBE_MAP_EXT		((ovrSystemProperty)(VRAPI_EXT_BASE + 5))
#define VRAPI_LAYERLIST_EXT					((ovrSystemProperty)(VRAPI_EXT_BASE + 6))
#define VRAPI_LAYERLIST_SURFACE_TEX_EXT		((ovrSystemProperty)(VRAPI_EXT_BASE + 7))

/* ovrFrameLayerFlags allocations */

// REMAP_2D_EXT uses 4 flag bits to create an enum space for 16 possible 2D remappings
#define VRAPI_FRAME_LAYER_FLAG_REMAP_2D_SHIFT_EXT (27)
#define VRAPI_FRAME_LAYER_FLAG_REMAP_2D_MASK_EXT (0xf<<VRAPI_FRAME_LAYER_FLAG_REMAP_2D_SHIFT_EXT)

#define VRAPI_FRAME_LAYER_FLAG_REMAP_2D_IDENTITY_EXT (0<<VRAPI_FRAME_LAYER_FLAG_REMAP_2D_SHIFT_EXT)
#define VRAPI_FRAME_LAYER_FLAG_REMAP_2D_EQUIRECT_EXT (1<<VRAPI_FRAME_LAYER_FLAG_REMAP_2D_SHIFT_EXT)
#define VRAPI_FRAME_LAYER_FLAG_REMAP_2D_HEMICYL_EXT  (2<<VRAPI_FRAME_LAYER_FLAG_REMAP_2D_SHIFT_EXT)

///--END_SDK_REMOVE

/* ovrStructureType allocations */

static inline ovrFrameParms * vrapi_GetFrameParms( ovrFrameParmsExtBase * frameParmsChain )
{
	while ( frameParmsChain != NULL && frameParmsChain->Type != VRAPI_STRUCTURE_TYPE_FRAME_PARMS )
	{
		frameParmsChain = frameParmsChain->Next;
	}

	return 	(ovrFrameParms *)frameParmsChain;
}

static inline const ovrFrameParms * vrapi_GetFrameParmsConst( const ovrFrameParmsExtBase * frameParmsChain )
{
	while ( frameParmsChain != NULL && frameParmsChain->Type != VRAPI_STRUCTURE_TYPE_FRAME_PARMS )
	{
		frameParmsChain = frameParmsChain->Next;
	}

	return 	(const ovrFrameParms *)frameParmsChain;
}

///--BEGIN_SDK_REMOVE

// REMAP_2D struct

#define VRAPI_STRUCTURE_TYPE_FRAME_PARMS_REMAP_2D_EXT ((ovrStructureType)(VRAPI_EXT_BASE + 1))

typedef struct
{
	ovrVector2f Scale;
	ovrVector2f Bias;
} ovrScaleBias2DExt;

typedef struct
{
	ovrStructureType Type;
	OVR_VRAPI_PADDING_64_BIT( 4 );
	ovrFrameParmsExtBase * Next;
	// Extension payload
	ovrScaleBias2DExt ScaleBias[VRAPI_FRAME_LAYER_TYPE_MAX][VRAPI_FRAME_LAYER_EYE_MAX];
} ovrFrameParmsRemap2DExt;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrFrameParmsRemap2DExt, ( 16 * VRAPI_FRAME_LAYER_TYPE_MAX * VRAPI_FRAME_LAYER_EYE_MAX + 8 ) );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrFrameParmsRemap2DExt, ( 16 * VRAPI_FRAME_LAYER_TYPE_MAX * VRAPI_FRAME_LAYER_EYE_MAX + 16 ) );

static inline ovrFrameParmsRemap2DExt vrapi_DefaultFrameParmsRemap2DExt()
{
	ovrFrameParmsRemap2DExt eq;
	memset( &eq, 0, sizeof( eq ) );

	eq.Type = VRAPI_STRUCTURE_TYPE_FRAME_PARMS_REMAP_2D_EXT;
	for ( int j = 0; j < VRAPI_FRAME_LAYER_TYPE_MAX; j++ )
	{
		for ( int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++ )
		{
			eq.ScaleBias[j][i].Scale.x = 1.0f;
			eq.ScaleBias[j][i].Scale.y = 1.0f;
		}
	}
	eq.Next = NULL;

	return eq;
}

// OFFCENTER_CUBE_MAP struct

#define VRAPI_STRUCTURE_TYPE_FRAME_PARMS_OFFCENTER_CUBE_MAP_EXT ((ovrStructureType)(VRAPI_EXT_BASE + 2))

typedef struct
{
	ovrStructureType Type;
	OVR_VRAPI_PADDING_64_BIT( 4 );
	ovrFrameParmsExtBase * Next;
	// Extension payload
	ovrVector3f Displacement[VRAPI_FRAME_LAYER_TYPE_MAX][VRAPI_FRAME_LAYER_EYE_MAX];
} ovrFrameParmsOffcenterCubeMapExt;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrFrameParmsOffcenterCubeMapExt, ( 12 * VRAPI_FRAME_LAYER_TYPE_MAX * VRAPI_FRAME_LAYER_EYE_MAX + 8 ) );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrFrameParmsOffcenterCubeMapExt, ( 12 * VRAPI_FRAME_LAYER_TYPE_MAX * VRAPI_FRAME_LAYER_EYE_MAX + 16 ) );

static inline ovrFrameParmsOffcenterCubeMapExt vrapi_DefaultFrameParmsOffcenterCubeMapExt()
{
	ovrFrameParmsOffcenterCubeMapExt oc;
	memset( &oc, 0, sizeof( oc ) );

	oc.Type = VRAPI_STRUCTURE_TYPE_FRAME_PARMS_OFFCENTER_CUBE_MAP_EXT;
	oc.Next = NULL;

	return oc;
}
///--END_SDK_REMOVE

///--BEGIN_SDK_REMOVE

// This private-only interface has been subsumed by the
// vrapi_SubmitFrame2 path and will be removed in the near future.
typedef enum ovrLayerType2_temp_
{
	VRAPI_LAYER_TYPE_PROJECTION2_TEMP		= 1,
	VRAPI_LAYER_TYPE_LOADING_ICON2_TEMP		= 2,
	VRAPI_LAYER_TYPE_CUBE2_TEMP				= 3,
	VRAPI_LAYER_TYPE_EQUIRECT2_TEMP			= 4,
	VRAPI_LAYER_TYPE_CYLINDER2_TEMP			= 5,
	VRAPI_LAYER_TYPE_SURFACE_TEXTURE_PROJECTION2_TEMP = 6,
	VRAPI_LAYER_TYPE_SURFACE_TEXTURE_EQUIRECT2_TEMP	= 7,
	VRAPI_LAYER_TYPE_SURFACE_TEXTURE_CYLINDER2_TEMP	= 8,
	VRAPI_LAYER_TYPE_SURFACE_TEXTURE_FISHEYE2_TEMP	= 9,
} ovrLayerType2_temp;

typedef struct ovrLayerHeader2_temp_
{
	ovrLayerType2_temp	Type;
	int					Flags;				// Bitfield of ovrFrameLayerFlags

	ovrVector4f			ColorScale;
	ovrFrameLayerBlend	SrcBlend;
	ovrFrameLayerBlend	DstBlend;
} ovrLayerHeader2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE( ovrLayerHeader2_temp, 32 );

typedef struct
{
	ovrLayerHeader2_temp	Header;					// Must be VRAPI_LAYER_TYPE_PROJECTION2_TEMP.

	ovrRigidBodyPosef		HeadPose;

	struct
	{
		ovrTextureSwapChain * ColorSwapChain;
		int					SwapChainIndex;
		unsigned long long	CompletionFence;
		ovrMatrix4f			TexCoordsFromTanAngles;
		ovrRectf			TextureRect;
	} Textures[VRAPI_FRAME_LAYER_EYE_MAX];
} ovrLayerProjection2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrLayerProjection2_temp, 320 );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrLayerProjection2_temp, 336 );

typedef struct
{
	ovrLayerHeader2_temp	Header;					// Must be VRAPI_LAYER_TYPE_LOADING_ICON2_TEMP.

	float					SpinSpeed;				// radians per second
	float					SpinScale;

	// Only monoscopic texture supported for spinning layer.
	ovrTextureSwapChain *	ColorSwapChain;
	int						SwapChainIndex;
	unsigned long long		CompletionFence;
} ovrLayerLoadingIcon2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrLayerLoadingIcon2_temp, 56 );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrLayerLoadingIcon2_temp, 64 );

typedef struct
{
	ovrLayerHeader2_temp	Header;					// Must be VRAPI_LAYER_TYPE_CUBE2_TEMP.

	ovrRigidBodyPosef		HeadPose;
	ovrMatrix4f				TexCoordsFromTanAngles;

	struct
	{
		ovrTextureSwapChain * ColorSwapChain;	// Must VRAPI_TEXTURE_TYPE_CUBE.
		int					SwapChainIndex;
		unsigned long long	CompletionFence;
		ovrVector3f			Offset;
	} Textures[VRAPI_FRAME_LAYER_EYE_MAX];
} ovrLayerCube2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrLayerCube2_temp, 256 );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrLayerCube2_temp, 272 );

typedef struct
{
	ovrLayerHeader2_temp	Header;					// Must be VRAPI_LAYER_TYPE_EQUIRECT2_TEMP.

	ovrRigidBodyPosef		HeadPose;
	ovrMatrix4f				TexCoordsFromTanAngles;

	struct
	{
		ovrTextureSwapChain * ColorSwapChain;	// Must VRAPI_TEXTURE_TYPE_2D_*.
		int					SwapChainIndex;
		unsigned long long	CompletionFence;
		ovrRectf			TextureRect;
		ovrVector2f			Scale;
		ovrVector2f			Bias;
	} Textures[VRAPI_FRAME_LAYER_EYE_MAX];
} ovrLayerEquirect2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrLayerEquirect2_temp, 288 );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrLayerEquirect2_temp, 304 );

typedef struct
{
	ovrLayerHeader2_temp	Header;					// Must be VRAPI_LAYER_TYPE_CYLINDER2_TEMP.

	ovrRigidBodyPosef		HeadPose;

	struct
	{
		ovrTextureSwapChain * ColorSwapChain;
		int					SwapChainIndex;
		unsigned long long	CompletionFence;
		ovrMatrix4f			TexCoordsFromTanAngles;
		ovrRectf			TextureRect;
		ovrVector2f			Scale;
		ovrVector2f			Bias;
	} Textures[VRAPI_FRAME_LAYER_EYE_MAX];
} ovrLayerCylinder2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrLayerCylinder2_temp, 352 );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrLayerCylinder2_temp, 368 );

typedef struct
{
	ovrLayerHeader2_temp	Header;					// Must be VRAPI_LAYER_TYPE_SURFACE_TEXTURE_PROJECTION2_TEMP.

	// jobject that will be updated before each eye for minimal
	// latency.
	// IMPORTANT: This should be a JNI weak reference to the object.
	// The system will try to convert it into a global reference before
	// calling SurfaceTexture->Update, which allows it to be safely
	// freed by the application.
	jobject				SurfaceTextureObject;

	ovrRigidBodyPosef	HeadPose;

	struct
	{
		ovrTextureSwapChain * ColorSwapChain;
		int					SwapChainIndex;
		unsigned long long	CompletionFence;
		ovrMatrix4f			TexCoordsFromTanAngles;
		ovrRectf			TextureRect;
	} Textures[VRAPI_FRAME_LAYER_EYE_MAX];
} ovrLayerSurfaceTextureProjection2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrLayerSurfaceTextureProjection2_temp, 328 );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrLayerSurfaceTextureProjection2_temp, 344 );

typedef struct
{
	ovrLayerHeader2_temp	Header;					// Must be VRAPI_LAYER_TYPE_SURFACE_TEXTURE_EQUIRECT2_TEMP.

	// jobject that will be updated before each eye for minimal
	// latency.
	// IMPORTANT: This should be a JNI weak reference to the object.
	// The system will try to convert it into a global reference before
	// calling SurfaceTexture->Update, which allows it to be safely
	// freed by the application.
	jobject				SurfaceTextureObject;

	ovrRigidBodyPosef	HeadPose;
	ovrMatrix4f			TexCoordsFromTanAngles;

	struct
	{
		ovrTextureSwapChain * ColorSwapChain;	// Must VRAPI_TEXTURE_TYPE_2D_*.
		int					SwapChainIndex;
		unsigned long long	CompletionFence;
		ovrRectf			TextureRect;
		ovrVector2f			Scale;
		ovrVector2f			Bias;
	} Textures[VRAPI_FRAME_LAYER_EYE_MAX];
} ovrLayerSurfaceTextureEquirect2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrLayerSurfaceTextureEquirect2_temp, 296 );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrLayerSurfaceTextureEquirect2_temp, 312 );

typedef struct
{
	ovrLayerHeader2_temp	Header;				// Must be VRAPI_LAYER_TYPE_SURFACE_TEXTURE_CYLINDER2_TEMP.

	// jobject that will be updated before each eye for minimal
	// latency.
	// IMPORTANT: This should be a JNI weak reference to the object.
	// The system will try to convert it into a global reference before
	// calling SurfaceTexture->Update, which allows it to be safely
	// freed by the application.
	jobject				SurfaceTextureObject;

	ovrRigidBodyPosef	HeadPose;

	struct
	{
		ovrTextureSwapChain * ColorSwapChain;
		int					SwapChainIndex;
		unsigned long long	CompletionFence;
		ovrMatrix4f			TexCoordsFromTanAngles;
		ovrRectf			TextureRect;

		// NOTE: textureMatrix is set up like the following:
		//	sx,  0, tx, 0
		//	0,  sy, ty, 0
		//	0,   0,  1, 0
		//	0,   0,  0, 1
		// since we do not need z coord for mapping to 2d texture.
		ovrMatrix4f			TextureMatrix;
	} Textures[VRAPI_FRAME_LAYER_EYE_MAX];
} ovrLayerSurfaceTextureCylinder2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrLayerSurfaceTextureCylinder2_temp, 456 );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrLayerSurfaceTextureCylinder2_temp, 472 );

// An "equiangular fisheye" or "f-theta" lens can be used to capture photos or video
// of around 180 degrees without stitching.
//
// The cameras probably aren't exactly vertical, so a transformation may need to be applied
// before performing the fisheye calculation.
// A stereo fisheye camera rig will usually have slight misalignments between the two
// cameras, so they need independent transformations.
//
// Once in lens space, the ray is transformed into an ideal fisheye projection, where the
// 180 degree hemisphere is mapped to a -1 to 1 2D space.
//
// From there it can be mapped into actual texture coordinates, possibly two to an image for stereo.
typedef struct
{
	ovrLayerHeader2_temp	Header;				// Must be VRAPI_LAYER_TYPE_SURFACE_TEXTURE_FISHEYE2_TEMP.

	// jobject that will be updated before each eye for minimal
	// latency.
	// IMPORTANT: This should be a JNI weak reference to the object.
	// The system will try to convert it into a global reference before
	// calling SurfaceTexture->Update, which allows it to be safely
	// freed by the application.
	jobject				SurfaceTextureObject;

	ovrRigidBodyPosef	HeadPose;

	struct
	{
		ovrTextureSwapChain * ColorSwapChain;
		int					SwapChainIndex;
		unsigned long long	CompletionFence;
		ovrMatrix4f			LensFromTanAngles;			// transforms a tanAngle ray into lens space
		ovrRectf			TextureRect;				// packed stereo images will need to clamp at the mid border
		ovrMatrix4f			TextureMatrix;				// transform from a -1 to 1 ideal fisheye to the texture
		ovrVector4f			Distortion;					// Not currently used.
	} Textures[VRAPI_FRAME_LAYER_EYE_MAX];
} ovrLayerSurfaceTextureFisheye2_temp;

OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( ovrLayerSurfaceTextureFisheye2_temp, 488 );
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( ovrLayerSurfaceTextureFisheye2_temp, 504 );

// Union that combines ovrLayer types in a way that allows them
// to be used in a polymorphic way.
typedef union
{
	ovrLayerHeader2_temp			Header;
	ovrLayerProjection2_temp		Projection;
	ovrLayerLoadingIcon2_temp		LoadingIcon;
	ovrLayerCube2_temp				Cube;
	ovrLayerEquirect2_temp			Equirect;
	ovrLayerCylinder2_temp			Cylinder;
	ovrLayerSurfaceTextureProjection2_temp	SurfaceTextureProjection;
	ovrLayerSurfaceTextureEquirect2_temp	SurfaceTextureEquirect;
	ovrLayerSurfaceTextureCylinder2_temp	SurfaceTextureCylinder;
	ovrLayerSurfaceTextureFisheye2_temp		SurfaceTextureFisheye;
} ovrLayer_Union2_temp;

// Default Initialization

static inline ovrLayerProjection2_temp vrapi_DefaultLayerProjection2_temp()
{
	ovrLayerProjection2_temp layer = {};

	const ovrMatrix4f projectionMatrix			= ovrMatrix4f_CreateProjectionFov( 90.0f, 90.0f, 0.0f, 0.0f, 0.1f, 0.0f );
	const ovrMatrix4f texCoordsFromTanAngles	= ovrMatrix4f_TanAngleMatrixFromProjection( &projectionMatrix );

	layer.Header.Type	= VRAPI_LAYER_TYPE_PROJECTION2_TEMP;
	layer.Header.ColorScale.x	= 1.0f;
	layer.Header.ColorScale.y	= 1.0f;
	layer.Header.ColorScale.z	= 1.0f;
	layer.Header.ColorScale.w	= 1.0f;
	layer.Header.SrcBlend		= VRAPI_FRAME_LAYER_BLEND_ONE;
	layer.Header.DstBlend		= VRAPI_FRAME_LAYER_BLEND_ZERO;

	layer.HeadPose.Pose.Orientation.w = 1.0f;

	for ( int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++ )
	{
		layer.Textures[i].TexCoordsFromTanAngles		= texCoordsFromTanAngles;
		layer.Textures[i].TextureRect.x					= 0.0f;
		layer.Textures[i].TextureRect.y					= 0.0f;
		layer.Textures[i].TextureRect.width				= 1.0f;
		layer.Textures[i].TextureRect.height			= 1.0f;
	}

	return layer;
}

static inline ovrLayerLoadingIcon2_temp vrapi_DefaultLayerLoadingIcon2_temp()
{
	ovrLayerLoadingIcon2_temp layer = {};

	layer.Header.Type	= VRAPI_LAYER_TYPE_LOADING_ICON2_TEMP;
	layer.Header.ColorScale.x	= 1.0f;
	layer.Header.ColorScale.y	= 1.0f;
	layer.Header.ColorScale.z	= 1.0f;
	layer.Header.ColorScale.w	= 1.0f;
	layer.Header.SrcBlend		= VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
	layer.Header.DstBlend		= VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

	layer.SpinSpeed			= 1.0f;
	layer.SpinScale			= 16.0f;

	layer.ColorSwapChain	= (ovrTextureSwapChain *)VRAPI_DEFAULT_TEXTURE_SWAPCHAIN_LOADING_ICON;
	layer.SwapChainIndex	= 0;

	return layer;
}

static inline ovrLayerCube2_temp vrapi_DefaultLayerCube2_temp()
{
	ovrLayerCube2_temp layer = {};

	const ovrMatrix4f projectionMatrix			= ovrMatrix4f_CreateProjectionFov( 90.0f, 90.0f, 0.0f, 0.0f, 0.1f, 0.0f );
	const ovrMatrix4f texCoordsFromTanAngles	= ovrMatrix4f_TanAngleMatrixFromProjection( &projectionMatrix );

	layer.Header.Type	= VRAPI_LAYER_TYPE_CUBE2_TEMP;
	layer.Header.ColorScale.x	= 1.0f;
	layer.Header.ColorScale.y	= 1.0f;
	layer.Header.ColorScale.z	= 1.0f;
	layer.Header.ColorScale.w	= 1.0f;
	layer.Header.SrcBlend		= VRAPI_FRAME_LAYER_BLEND_ONE;
	layer.Header.DstBlend		= VRAPI_FRAME_LAYER_BLEND_ZERO;

	layer.HeadPose.Pose.Orientation.w = 1.0f;
	layer.TexCoordsFromTanAngles = texCoordsFromTanAngles;

	for ( int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++ )
	{
		layer.Textures[i].Offset.x = 0.0f;
		layer.Textures[i].Offset.y = 0.0f;
		layer.Textures[i].Offset.z = 0.0f;
	}

	return layer;
}

static inline ovrLayerEquirect2_temp vrapi_DefaultLayerEquirect2_temp()
{
	ovrLayerEquirect2_temp layer = {};

	const ovrMatrix4f projectionMatrix			= ovrMatrix4f_CreateProjectionFov( 90.0f, 90.0f, 0.0f, 0.0f, 0.1f, 0.0f );
	const ovrMatrix4f texCoordsFromTanAngles	= ovrMatrix4f_TanAngleMatrixFromProjection( &projectionMatrix );

	layer.Header.Type	= VRAPI_LAYER_TYPE_EQUIRECT2_TEMP;
	layer.Header.ColorScale.x	= 1.0f;
	layer.Header.ColorScale.y	= 1.0f;
	layer.Header.ColorScale.z	= 1.0f;
	layer.Header.ColorScale.w	= 1.0f;
	layer.Header.SrcBlend		= VRAPI_FRAME_LAYER_BLEND_ONE;
	layer.Header.DstBlend		= VRAPI_FRAME_LAYER_BLEND_ZERO;

	layer.HeadPose.Pose.Orientation.w = 1.0f;
	layer.TexCoordsFromTanAngles = texCoordsFromTanAngles;

	for ( int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++ )
	{
		layer.Textures[i].TextureRect.x			= 0.0f;
		layer.Textures[i].TextureRect.y			= 0.0f;
		layer.Textures[i].TextureRect.width		= 1.0f;
		layer.Textures[i].TextureRect.height	= 1.0f;
		layer.Textures[i].Scale.x				= 1.0f;
		layer.Textures[i].Scale.y				= 1.0f;
		layer.Textures[i].Bias.x				= 0.0f;
		layer.Textures[i].Bias.y				= 0.0f;
	}

	return layer;
}

static inline ovrLayerCylinder2_temp vrapi_DefaultLayerCylinder2_temp()
{
	ovrLayerCylinder2_temp layer = {};

	const ovrMatrix4f projectionMatrix			= ovrMatrix4f_CreateProjectionFov( 90.0f, 90.0f, 0.0f, 0.0f, 0.1f, 0.0f );
	const ovrMatrix4f texCoordsFromTanAngles	= ovrMatrix4f_TanAngleMatrixFromProjection( &projectionMatrix );

	layer.Header.Type	= VRAPI_LAYER_TYPE_CYLINDER2_TEMP;
	layer.Header.ColorScale.x	= 1.0f;
	layer.Header.ColorScale.y	= 1.0f;
	layer.Header.ColorScale.z	= 1.0f;
	layer.Header.ColorScale.w	= 1.0f;
	layer.Header.SrcBlend		= VRAPI_FRAME_LAYER_BLEND_ONE;
	layer.Header.DstBlend		= VRAPI_FRAME_LAYER_BLEND_ZERO;

	for ( int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++ )
	{
		layer.Textures[i].TexCoordsFromTanAngles	= texCoordsFromTanAngles;
		layer.Textures[i].TextureRect.x			= 0.0f;
		layer.Textures[i].TextureRect.y			= 0.0f;
		layer.Textures[i].TextureRect.width		= 1.0f;
		layer.Textures[i].TextureRect.height	= 1.0f;
		layer.Textures[i].Scale.x				= 1.0f;
		layer.Textures[i].Scale.y				= 1.0f;
		layer.Textures[i].Bias.x				= 0.0f;
		layer.Textures[i].Bias.y				= 0.0f;
	}

	return layer;
}

static inline ovrLayerSurfaceTextureProjection2_temp vrapi_DefaultLayerSurfaceTextureProjection2_temp()
{
	ovrLayerSurfaceTextureProjection2_temp layer = {};

	const ovrMatrix4f projectionMatrix			= ovrMatrix4f_CreateProjectionFov( 90.0f, 90.0f, 0.0f, 0.0f, 0.1f, 0.0f );
	const ovrMatrix4f texCoordsFromTanAngles	= ovrMatrix4f_TanAngleMatrixFromProjection( &projectionMatrix );

	layer.Header.Type	= VRAPI_LAYER_TYPE_SURFACE_TEXTURE_PROJECTION2_TEMP;
	layer.Header.ColorScale.x	= 1.0f;
	layer.Header.ColorScale.y	= 1.0f;
	layer.Header.ColorScale.z	= 1.0f;
	layer.Header.ColorScale.w	= 1.0f;
	layer.Header.SrcBlend		= VRAPI_FRAME_LAYER_BLEND_ONE;
	layer.Header.DstBlend		= VRAPI_FRAME_LAYER_BLEND_ZERO;

	layer.HeadPose.Pose.Orientation.w = 1.0f;

	for ( int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++ )
	{
		layer.Textures[i].TexCoordsFromTanAngles		= texCoordsFromTanAngles;
		layer.Textures[i].TextureRect.x					= 0.0f;
		layer.Textures[i].TextureRect.y					= 0.0f;
		layer.Textures[i].TextureRect.width				= 1.0f;
		layer.Textures[i].TextureRect.height			= 1.0f;
	}

	return layer;
}

static inline ovrLayerSurfaceTextureEquirect2_temp vrapi_DefaultLayerSurfaceTextureEquirect2_temp()
{
	ovrLayerSurfaceTextureEquirect2_temp layer = {};

	const ovrMatrix4f projectionMatrix			= ovrMatrix4f_CreateProjectionFov( 90.0f, 90.0f, 0.0f, 0.0f, 0.1f, 0.0f );
	const ovrMatrix4f texCoordsFromTanAngles	= ovrMatrix4f_TanAngleMatrixFromProjection( &projectionMatrix );

	layer.Header.Type	= VRAPI_LAYER_TYPE_SURFACE_TEXTURE_EQUIRECT2_TEMP;
	layer.Header.ColorScale.x	= 1.0f;
	layer.Header.ColorScale.y	= 1.0f;
	layer.Header.ColorScale.z	= 1.0f;
	layer.Header.ColorScale.w	= 1.0f;
	layer.Header.SrcBlend		= VRAPI_FRAME_LAYER_BLEND_ONE;
	layer.Header.DstBlend		= VRAPI_FRAME_LAYER_BLEND_ZERO;

	layer.HeadPose.Pose.Orientation.w = 1.0f;
	layer.TexCoordsFromTanAngles = texCoordsFromTanAngles;

	for ( int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++ )
	{
		layer.Textures[i].TextureRect.x			= 0.0f;
		layer.Textures[i].TextureRect.y			= 0.0f;
		layer.Textures[i].TextureRect.width		= 1.0f;
		layer.Textures[i].TextureRect.height	= 1.0f;
		layer.Textures[i].Scale.x				= 1.0f;
		layer.Textures[i].Scale.y				= 1.0f;
		layer.Textures[i].Bias.x				= 0.0f;
		layer.Textures[i].Bias.y				= 0.0f;
	}

	return layer;
}

static inline ovrLayerSurfaceTextureCylinder2_temp vrapi_DefaultLayerSurfaceTextureCylinder2_temp()
{
	ovrLayerSurfaceTextureCylinder2_temp layer = {};

	const ovrMatrix4f projectionMatrix			= ovrMatrix4f_CreateProjectionFov( 90.0f, 90.0f, 0.0f, 0.0f, 0.1f, 0.0f );
	const ovrMatrix4f texCoordsFromTanAngles	= ovrMatrix4f_TanAngleMatrixFromProjection( &projectionMatrix );

	layer.Header.Type	= VRAPI_LAYER_TYPE_SURFACE_TEXTURE_CYLINDER2_TEMP;
	layer.Header.ColorScale.x	= 1.0f;
	layer.Header.ColorScale.y	= 1.0f;
	layer.Header.ColorScale.z	= 1.0f;
	layer.Header.ColorScale.w	= 1.0f;
	layer.Header.SrcBlend		= VRAPI_FRAME_LAYER_BLEND_ONE;
	layer.Header.DstBlend		= VRAPI_FRAME_LAYER_BLEND_ZERO;

	for ( int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++ )
	{
		layer.Textures[i].TexCoordsFromTanAngles	= texCoordsFromTanAngles;
		layer.Textures[i].TextureRect.x			= 0.0f;
		layer.Textures[i].TextureRect.y			= 0.0f;
		layer.Textures[i].TextureRect.width		= 1.0f;
		layer.Textures[i].TextureRect.height	= 1.0f;
		layer.Textures[i].TextureMatrix.M[0][0] = 1.0f;
		layer.Textures[i].TextureMatrix.M[1][1] = 1.0f;
		layer.Textures[i].TextureMatrix.M[2][2] = 1.0f;
		layer.Textures[i].TextureMatrix.M[3][3] = 1.0f;
	}

	return layer;
}

static inline ovrLayerSurfaceTextureFisheye2_temp vrapi_DefaultLayerSurfaceTextureFisheye2_temp()
{
	ovrLayerSurfaceTextureFisheye2_temp layer = {};

	const ovrMatrix4f projectionMatrix = ovrMatrix4f_CreateProjectionFov( 90.0f, 90.0f, 0.0f, 0.0f, 0.1f, 0.0f );
	const ovrMatrix4f texCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection( &projectionMatrix );

	layer.Header.Type = VRAPI_LAYER_TYPE_SURFACE_TEXTURE_FISHEYE2_TEMP;
	layer.Header.ColorScale.x = 1.0f;
	layer.Header.ColorScale.y = 1.0f;
	layer.Header.ColorScale.z = 1.0f;
	layer.Header.ColorScale.w = 1.0f;
	layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_ONE;
	layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ZERO;

	for ( int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++ )
	{
		layer.Textures[i].LensFromTanAngles = texCoordsFromTanAngles;
		layer.Textures[i].TextureRect.x = 0.0f;
		layer.Textures[i].TextureRect.y = 0.0f;
		layer.Textures[i].TextureRect.width = 1.0f;
		layer.Textures[i].TextureRect.height = 1.0f;
		layer.Textures[i].TextureMatrix.M[0][0] = 1.0f;
		layer.Textures[i].TextureMatrix.M[1][1] = 1.0f;
		layer.Textures[i].TextureMatrix.M[2][2] = 1.0f;
		layer.Textures[i].TextureMatrix.M[3][3] = 1.0f;
	}

	return layer;
}

#if defined( __cplusplus )
extern "C" {
#endif

// This temporary function will be removed in the near future. vrapi_SubmitFrame2 should be used instead.
OVR_VRAPI_EXPORT void vrapi_SubmitFrame2_temp( ovrMobile * ovr, long long frameIndex, int frameFlags,
						ovrLayerHeader2_temp const * const * layers, int layerCount,
						const ovrPerformanceParms * performanceParms, int swapInterval, int extraLatencyMode );

#if defined( __cplusplus )
}	// extern "C"
#endif

///--END_SDK_REMOVE

///--BEGIN_SDK_REMOVE
#if defined( ANDROID )
#include <android/input.h>
int32_t AInputQueue_preDispatchEvent_Hooked( AInputQueue * queue, AInputEvent * event );
#endif
///--END_SDK_REMOVE

#endif // OVR_VrApi_Ext_h
