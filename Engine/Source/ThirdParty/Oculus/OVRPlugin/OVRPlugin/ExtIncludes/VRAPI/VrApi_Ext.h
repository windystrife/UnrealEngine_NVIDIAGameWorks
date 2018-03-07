/************************************************************************************

Filename    :   VrApi_Ext.h
Content     :   VrApi extensions support
Created     :   February 3, 2016
Authors     :   Cass Everitt

Copyright   :   Copyright 2016 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrApi_Ext_h
#define OVR_VrApi_Ext_h

#include "VrApi_Types.h"
#include "string.h"				// for memset()

//-----------------------------------------------------------------
// Basic Ext Types
//-----------------------------------------------------------------

// This type is just to make parm chain traversal simple
typedef struct ovrFrameParmsExtBase
{
	ovrStructureType Type;
	OVR_VRAPI_PADDING_64_BIT( 4 );
	ovrFrameParmsExtBase * Next;
} ovrFrameParmsExtBase;

///--BEGIN_SDK_REMOVE

/* Enumerants that can be used in calls to vrapi_GetSystemProperty() */
#define VRAPI_EXT_BASE 0x10000000

#define VRAPI_REMAP_2D_EXT 					((ovrSystemProperty)(VRAPI_EXT_BASE + 2))
#define VRAPI_EXTENDED_FRAME_PARMS_EXT		((ovrSystemProperty)(VRAPI_EXT_BASE + 3))
#define VRAPI_REMAP_2D_HEMICYL_EXT			((ovrSystemProperty)(VRAPI_EXT_BASE + 4))
#define VRAPI_OFFCENTER_CUBE_MAP_EXT		((ovrSystemProperty)(VRAPI_EXT_BASE + 5))
#define VRAPI_LAYERLIST_EXT					((ovrSystemProperty)(VRAPI_EXT_BASE + 6))

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
			eq.ScaleBias[j][i].Scale = { 1.0f, 1.0f };
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

#endif // OVR_VrApi_Ext_h
