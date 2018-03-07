///////////////////////////////////////////////////////////////////////  
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com
//
//  *** Release version 7.0.0 ***


///////////////////////////////////////////////////////////////////////  
//  Preprocessor
//
//	The SpeedTree SDK generally depends on the following external macros to 
//	distinguish platforms:
//
//	  Windows 32/64: _WIN32, defined in both 32- and 64-bit environments
//	  PLAYSTATION 4: __ORBIS__
//	  Xbox One:      _DURANGO
//    PLAYSTATION 3: __CELLOS_LV2__ (SPEEDTREE_BIG_ENDIAN is #defined if detected)
//	  XBOX 360:      _XBOX (SPEEDTREE_BIG_ENDIAN is #defined if detected)
//	  WiiU:          NDEV
//	  PS Vita:       __psp2__
//	  Mac OSX:       __APPLE__
//	  Linux:		 __linux__

#pragma once
#ifdef _WIN32
	#pragma warning(push)
	#pragma warning (disable : 4121 4315) // 64-bit alignment warnings
#endif


///////////////////////////////////////////////////////////////////////  
//  SDK Compile-time configuration options

// to prevent an excessive number of heap allocations and any allocations
// during render, the SpeedTree SDK uses a ring buffer of buffers made available
// for temporary uses. #define USE_SDK_TMP_HEAP_RING_BUFFER for it to be used,
// or leave it undefined and allocation requests will be made through the
// custom allocator interface instead, with ALLOC_TYPE_TEMPORARY passed in
#define USE_SDK_TMP_HEAP_RING_BUFFER

// uncomment to actively track any render-time allocations that may occur due to
// too-low values used in heap reserves; this will assist in determining what new
// reservation values should be used
//#define SPEEDTREE_RUNTIME_HEAP_CHECK


///////////////////////////////////////////////////////////////////////  
//  Includes

#include "Core/ExportBegin.h"
#include "Core/Extents.h"
#include "Core/String.h"
#include "Core/Wind.h"


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////  
//  Version info

#define SPEEDTREE_VERSION_MAJOR		7
#define SPEEDTREE_VERSION_MINOR		0
#define SPEEDTREE_SUBMINOR_VERSION	0
#define SPEEDTREE_VERSION_STRING	"7.0.0"


///////////////////////////////////////////////////////////////////////  
//  Platform specifics

#if defined(_XBOX) || defined(__CELLOS_LV2__) || defined(NDEV)
	// Xbox 360, PS3, and WiiU are big endian
	#define SPEEDTREE_BIG_ENDIAN
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	//  Forward references

	class CFileSystem;


    ///////////////////////////////////////////////////////////////////////  
    //  Enumeration EResourceType
	//
	//	Used with CCore static function ResourceAllocated to track graphics 
	//	allocations throughout the SDK.

	enum EGfxResourceType
	{
		GFX_RESOURCE_VERTEX_BUFFER,
		GFX_RESOURCE_INDEX_BUFFER,
		GFX_RESOURCE_VERTEX_SHADER,
		GFX_RESOURCE_PIXEL_SHADER,
		GFX_RESOURCE_TEXTURE,
        GFX_RESOURCE_OTHER,
		GFX_RESOURCE_COUNT
	};


	///////////////////////////////////////////////////////////////////////  
	//  Enumeration ERenderPass
	//
	//	Three main render passes are supported by the SDK by default:
	//
	//		- Main (forward rendering lit, or deferred MRT)
	//		- Depth-only prepass
	//		- Shadow cast

	enum ERenderPass
	{
		RENDER_PASS_MAIN,
		RENDER_PASS_DEPTH_PREPASS,
		RENDER_PASS_SHADOW_CAST,
		RENDER_PASS_COUNT
	};


	///////////////////////////////////////////////////////////////////////  
	//  A word on terminology regarding vertex declarations in this SDK. There
	//	are several key terms that need to be clearly defined. Take a vertex 
	//	shader input declaration like the following in HLSL syntax:
	//
	//	struct SVertexDecl
	//	{
	//	  float3  vSlot0  : POSITION;	// xyz = position.xyz
	//    float4  vSlot1  : TEXCOORD0;	// xy = diffuse texcoords, z = amb occ, w = normal.z
	//  };
	//
	//	Using the second line, vSlot1, of the structure as an example, we wish to
	//  define the following terms (appear in double-quotes):
	//
	//		o The entire float4 group taken collectively is termed a "vertex attribute."
	//      o TEXCOORD0 is the vertex's "semantic," as the term defined is in the HLSL 
	//		  and Cg docs.
	//		o "Component" refers any single x, y, z, or w in the slot.
	//      o The four components hold diffuse texcoords, ambient occlusion, and partial 
	//		  normal "properties."  Not to be confused with "attribute," as there may be multiple
	//        properties in a single attribute, or a single property may be split across
	//        multiple attributes.
	//		o "Format" refers to how the data was stored in the vertex buffer (e.g.
	//		   byte, half float, full float) { not visible in this decl }
	//
	//	The structure taken as a whole is the "vertex declaration."


	///////////////////////////////////////////////////////////////////////  
	//  Enumeration EVertexProperty
	//
	//	Contains a list of all vertex properties that may be used by the SpeedTree
	//	vertex buffers and shaders. Note that some are geometry-specific and,
	//	depending on the compilation mode chosen in the Compiler app, may or
	//	may not appear in the vertex decl for each draw call.
	//
	//	Details for each vertex property can be queried using 
	//	CCore::GetVertexPropertyDesc().
	//
	//	The typedef below forces the enumeration to use 8 bits instead of
	//	32. It helps to keep SVertexDecl small.

	enum EVertexPropertyUntyped
	{
		VERTEX_PROPERTY_UNASSIGNED = -1,

		// these can affect the shape of the tree and therefore come first; this
		// can reduce vertex fetches when rendering depth & shadow passes
		VERTEX_PROPERTY_POSITION,						// 3 components
		VERTEX_PROPERTY_DIFFUSE_TEXCOORDS,				// 2 components
		VERTEX_PROPERTY_NORMAL,	              			// 3 components (normal impacts the wind algorithm)
		VERTEX_PROPERTY_LOD_POSITION,					// 3 components
		VERTEX_PROPERTY_GEOMETRY_TYPE_HINT,  			// 1 component
		VERTEX_PROPERTY_LEAF_CARD_CORNER,				// 3 components (corner x, corner y, z-fight offset)
		VERTEX_PROPERTY_LEAF_CARD_LOD_SCALAR,			// 1 component
		VERTEX_PROPERTY_LEAF_CARD_SELF_SHADOW_OFFSET,	// 1 component
		VERTEX_PROPERTY_WIND_BRANCH_DATA,				// 4 components
		VERTEX_PROPERTY_WIND_EXTRA_DATA,				// 3 components
		VERTEX_PROPERTY_WIND_FLAGS,						// 1 component
		VERTEX_PROPERTY_LEAF_ANCHOR_POINT,				// 3 components
		VERTEX_PROPERTY_BONE_ID,						// 1 component

		// these do not affect the shape of the tree and come later
		VERTEX_PROPERTY_BRANCH_SEAM_DIFFUSE,			// 3 components (s, t, weight)
		VERTEX_PROPERTY_BRANCH_SEAM_DETAIL,				// 2 components (s, t)
		VERTEX_PROPERTY_DETAIL_TEXCOORDS,				// 2 components
		VERTEX_PROPERTY_TANGENT,						// 3 components
		VERTEX_PROPERTY_LIGHTMAP_TEXCOORDS,				// 2 components
		VERTEX_PROPERTY_AMBIENT_OCCLUSION,				// 1 component
			VERTEX_PROPERTY_MISC_SEMANTIC = VERTEX_PROPERTY_AMBIENT_OCCLUSION,

		VERTEX_PROPERTY_COUNT,
		VERTEX_PROPERTY_PAD = VERTEX_PROPERTY_COUNT
	};
	typedef Enumeration<EVertexPropertyUntyped, st_int8> EVertexProperty;


	///////////////////////////////////////////////////////////////////////  
	//  Structure SVertexPropertyDesc
	//
	//	Holds details about each vertex property as returned by
	//	CCore::GetVertexPropertyDesc().

	struct SVertexPropertyDesc
	{
		st_int32		m_nNumComponents;
		const st_char*	m_pFullName;
		const st_char*	m_pShortName;
	};


	///////////////////////////////////////////////////////////////////////  
	//  Enumeration EPixelProperty
	//
	//	Contains a list of all pixel shader properties that may be used by the 
	//	SpeedTree shaders. While this is housed in the Core SDK library, it
	//	is primarily used by the SRT Exporter housed in the Compiler app.
	//
	//	Details for each vertex property can be queried using 
	//	CCore::GetPixelPropertyDesc().
	//
	//	The typedef below forces the enumeration to use 8 bits instead of
	//	32. It helps to keep SVertexDecl small.

	enum EPixelProperty
	{
		PIXEL_PROPERTY_POSITION,
		PIXEL_PROPERTY_FOG_SCALAR,
		PIXEL_PROPERTY_FOG_COLOR,
		PIXEL_PROPERTY_DIFFUSE_TEXCOORDS,
		PIXEL_PROPERTY_DETAIL_TEXCOORDS,
		PIXEL_PROPERTY_PER_VERTEX_LIGHTING_COLOR,
		PIXEL_PROPERTY_NORMAL_MAP_VECTOR,
		PIXEL_PROPERTY_NORMAL,
		PIXEL_PROPERTY_BINORMAL,
		PIXEL_PROPERTY_TANGENT,
		PIXEL_PROPERTY_SPECULAR_HALF_VECTOR,
		PIXEL_PROPERTY_PER_VERTEX_SPECULAR_DOT,
		PIXEL_PROPERTY_PER_VERTEX_AMBIENT_CONTRAST,
		PIXEL_PROPERTY_FADE_TO_BILLBOARD,
		PIXEL_PROPERTY_TRANSMISSION_FACTOR,
		PIXEL_PROPERTY_RENDER_EFFECT_FADE,
		PIXEL_PROPERTY_AMBIENT_OCCLUSION,
		PIXEL_PROPERTY_BRANCH_SEAM_DIFFUSE,
		PIXEL_PROPERTY_BRANCH_SEAM_DETAIL,
		PIXEL_PROPERTY_SHADOW_DEPTH,
		PIXEL_PROPERTY_SHADOW_MAP_0_PROJECTION,
		PIXEL_PROPERTY_SHADOW_MAP_1_PROJECTION,
		PIXEL_PROPERTY_SHADOW_MAP_2_PROJECTION,
		PIXEL_PROPERTY_SHADOW_MAP_3_PROJECTION,
		PIXEL_PROPERTY_HUE_VARIATION,
		PIXEL_PROPERTY_COUNT
	};


	///////////////////////////////////////////////////////////////////////  
	//  Structure SPixelPropertyDesc
	//
	//	Holds details about each pixel property as returned by
	//	CCore::GetPixelPropertyDesc().

	struct SPixelPropertyDesc
	{
		st_int32		m_nNumComponents;
		const st_char*	m_pFullName;
		const st_char*	m_pShortName;
	};


	///////////////////////////////////////////////////////////////////////  
	//  Enumeration EVertexAttribute
	//
	//	List the 16 available vertex attributes. Each can hold 4 floats. Each
	//	platform supported in the SDK will turn these into platform-specific
	//	vertex shader input semantics like POSITION or TEXCOORD[0-15].
	//
	//	The typedef below forces the enumeration to use 8 bits instead of
	//	32. It helps to keep SVertexDecl small.

	enum EVertexAttributeUntyped
	{
		VERTEX_ATTRIB_UNASSIGNED = -1, 

		VERTEX_ATTRIB_0,
		VERTEX_ATTRIB_1,
		VERTEX_ATTRIB_2,
		VERTEX_ATTRIB_3,
		VERTEX_ATTRIB_4,
		VERTEX_ATTRIB_5,
		VERTEX_ATTRIB_6,
		VERTEX_ATTRIB_7,
		VERTEX_ATTRIB_8,
		VERTEX_ATTRIB_9,
		VERTEX_ATTRIB_10,
		VERTEX_ATTRIB_11,
		VERTEX_ATTRIB_12,
		VERTEX_ATTRIB_13,
		VERTEX_ATTRIB_14,
		VERTEX_ATTRIB_15,

		VERTEX_ATTRIB_COUNT,
		VERTEX_DECL_END = VERTEX_ATTRIB_COUNT
	};
	typedef Enumeration<EVertexAttributeUntyped, st_int8> EVertexAttribute;


	///////////////////////////////////////////////////////////////////////  
	//  Enumeration EVertexComponent
	//
	//	Simple enumeration for accessing individual components for attributes.
	//
	//	The typedef below forces the enumeration to use 8 bits instead of
	//	32. It helps to keep SVertexDecl small.

	enum EVertexComponentUntyped
	{
		VERTEX_COMPONENT_UNASSIGNED = -1,

		VERTEX_COMPONENT_X, 
		VERTEX_COMPONENT_Y, 
		VERTEX_COMPONENT_Z, 
		VERTEX_COMPONENT_W, 
		VERTEX_COMPONENT_COUNT
	};
	typedef Enumeration<EVertexComponentUntyped, st_int8> EVertexComponent;


	///////////////////////////////////////////////////////////////////////  
	//  Enumeration EVertexFormat
	//
	//	List of the three data types used to house vertex data.
	//
	//	The typedef below forces the enumeration to use 8 bits instead of
	//	32. It helps to keep SVertexDecl::SAttribDesc small.

	enum EVertexFormatUntyped
	{
		VERTEX_FORMAT_UNASSIGNED = -1,

		VERTEX_FORMAT_FULL_FLOAT,		// 32-bit floats
		VERTEX_FORMAT_HALF_FLOAT,		// 16-bit floats
		VERTEX_FORMAT_BYTE,				// 8-bit values
		VERTEX_FORMAT_COUNT
	};
	typedef Enumeration<EVertexFormatUntyped, st_int8> EVertexFormat;


	///////////////////////////////////////////////////////////////////////  
	//  Structure SVertexDecl
	//
	//	Depending on the compilation mode chosen in the Compiler app, vertex
	//	declarations may be variable from draw call to draw call. This structure
	//	houses a portable definition of the vertex declaration.

	struct ST_DLL_LINK SVertexDecl
	{
			struct ST_DLL_LINK SAttribute
			{

										SAttribute( );

					st_bool				IsUsed(void) const;
					st_int32			NumEmptyComponents(void) const;
					st_int32			NumUsedComponents(void) const;
					st_int32			Size(void) const; // in bytes
					void				Clear(void);
					EVertexComponent	FirstFreeComponent(void) const;				

					st_uint8			m_uiStream;
					EVertexFormat		m_eFormat;
					EVertexProperty		m_aeProperties[VERTEX_COMPONENT_COUNT];
					EVertexComponent	m_aePropertyComponents[VERTEX_COMPONENT_COUNT];
					st_uint8			m_auiVertexOffsets[VERTEX_COMPONENT_COUNT];
			};

			struct ST_DLL_LINK SProperty
			{
										SProperty( );

					st_bool				IsPresent(void) const;
					st_bool				IsContiguous(void) const;
					st_int32			NumComponents(void) const;

					EVertexFormat		m_eFormat;										// all four components in attribute have to be the same format
					EVertexAttribute	m_aeAttribs[VERTEX_COMPONENT_COUNT];
					EVertexComponent	m_aeAttribComponents[VERTEX_COMPONENT_COUNT];
					st_uint8			m_auiOffsets[VERTEX_COMPONENT_COUNT];			// in bytes, offset of each component from beginning of whole vertex 
			};

										SVertexDecl( );

            st_bool                     operator!=(const SVertexDecl& sRight) const;
            st_bool                     operator<(const SVertexDecl& sRight) const;

	static	const char*	   ST_CALL_CONV FormatName(EVertexFormat eFormat);
	static	st_int32	   ST_CALL_CONV FormatSize(EVertexFormat eFormat); // size in bytes
	static	const char*	   ST_CALL_CONV AttributeName(EVertexAttribute eAttrib);
			void						GetDescription(CString& strDesc) const;

			// mesh instancing support; mostly for PS Vita platform
			enum EInstanceType
			{
				INSTANCES_3D_TREES,
				INSTANCES_GRASS,
				INSTANCES_BILLBOARDS,
				INSTANCES_NONE
			};

	static	void						GetInstanceVertexDecl(SVertexDecl& sInstanceDecl, EInstanceType eInstanceType);
	static	st_bool						MergeObjectAndInstanceVertexDecls(SVertexDecl& sMergedDecl, const SVertexDecl& sObjectDecl, EInstanceType eInstanceType);

			// utility for setting manually
			struct ST_DLL_LINK SAttribDesc
			{
				st_uint8				m_uiStream;
				EVertexAttribute		m_eAttrib;
				EVertexFormat			m_eFormat;
				st_uint8				m_uiNumComponents;	// e.g., 3 for (x,y,z)

				struct ST_DLL_LINK SPropertyComponent
				{
					EVertexProperty		m_eProperty;
					EVertexComponent	m_eComponent;
				};
				SPropertyComponent		m_asProperties[VERTEX_COMPONENT_COUNT];
			};
			#define VERTEX_DECL_END( ) { 0, VERTEX_ATTRIB_COUNT, VERTEX_FORMAT_FULL_FLOAT, 0, \
				{ { VERTEX_PROPERTY_UNASSIGNED, VERTEX_COMPONENT_X }, \
				  { VERTEX_PROPERTY_UNASSIGNED, VERTEX_COMPONENT_X }, \
				  { VERTEX_PROPERTY_UNASSIGNED, VERTEX_COMPONENT_X }, \
				  { VERTEX_PROPERTY_UNASSIGNED, VERTEX_COMPONENT_X } } }

			st_bool						Set(const SAttribDesc* pAttribDesc);

			// vertex data organized by attributes
			SAttribute					m_asAttributes[VERTEX_ATTRIB_COUNT];

			// same vertex data, but organized by properties
			SProperty					m_asProperties[VERTEX_PROPERTY_COUNT];

			// shared by both organizations
			st_uint8					m_uiVertexSize;
	};


	///////////////////////////////////////////////////////////////////////  
	//  Enumerations associated with SRenderState

	enum ELightingModel
	{
		LIGHTING_MODEL_PER_VERTEX,
		LIGHTING_MODEL_PER_PIXEL,
		LIGHTING_MODEL_PER_VERTEX_X_PER_PIXEL, // transitional state, forward rendering only
		LIGHTING_MODEL_DEFERRED
	};

	enum ELightingEffect
	{
		EFFECT_OFF,
		EFFECT_ON,
		EFFECT_OFF_X_ON // transitional state
	};

	enum ELodMethod
	{
		LOD_METHOD_POP,
		LOD_METHOD_SMOOTH
	};

	enum ECullType
	{
		CULLTYPE_NONE, 
		CULLTYPE_BACK, 
		CULLTYPE_FRONT
	};

	enum EFogCurve
	{
		FOG_CURVE_NONE,				// no fog effect
		FOG_CURVE_LINEAR,
		FOG_CURVE_EXP,
		FOG_CURVE_EXP2,
		FOG_CURVE_USER
	};

	enum EFogColorType
	{
		FOG_COLOR_TYPE_CONSTANT,
		FOG_COLOR_TYPE_DYNAMIC
	};

	enum EWindLod
	{
		WIND_LOD_NONE,
		WIND_LOD_GLOBAL,
		WIND_LOD_BRANCH,
		WIND_LOD_FULL,

		// transitional states (affect shaders only), 'X' in name denotes cross fade
		WIND_LOD_NONE_X_GLOBAL,
		WIND_LOD_NONE_X_BRANCH,
		WIND_LOD_NONE_X_FULL,
		WIND_LOD_GLOBAL_X_BRANCH,
		WIND_LOD_GLOBAL_X_FULL,
		WIND_LOD_BRANCH_X_FULL
	};

	// used by the Compiler app when writing shaders
	enum EWindEffect
	{
		WIND_EFFECT_LEAF_WIND_1,
		WIND_EFFECT_LEAF_WIND_2
	};

	// these generation modes match one-for-one with the Compiler app's "Merge materials" compilation option
	enum EShaderGenerationMode
	{
		SHADER_GEN_MODE_STANDARD,
		SHADER_GEN_MODE_ACROSS_GEOMETRIES,
		SHADER_GEN_MODE_AGGRESSIVE_ROUND_UP,
		SHADER_GEN_MODE_AGGRESSIVE_ROUND_DOWN,
		SHADER_GEN_MODE_SPEEDTREE_5X_STYLE,
		SHADER_GEN_MODE_UNIFIED_SHADERS,
		SHADER_GEN_MODE_UNREAL_ENGINE_4,
		SHADER_GEN_MODE_COUNT
	};


	///////////////////////////////////////////////////////////////////////  
	//  Enumeration ETextureLayer
	//
	//	Used to access texture layers in SRenderState but also used as texture
	//	registers in shaders.

	enum ETextureLayer
	{
		TL_DIFFUSE,        
		TL_NORMAL,         
		TL_DETAIL_DIFFUSE,        
		TL_DETAIL_NORMAL,
		TL_SPECULAR_MASK,
		TL_TRANSMISSION_MASK,
		TL_AUX_ATLAS1,
		TL_AUX_ATLAS2,

		TL_NUM_TEX_LAYERS
	};

	
	///////////////////////////////////////////////////////////////////////  
	//  Structure SRenderState
	//
	//	Every draw call is associated with a SRenderState object which holds
	//	numerous effect states. A lot of these are determined by the Effect LOD
	//	dialog in the Compiler application.

	typedef CPaddedPtr<const char> CStringPtr;

	// Note that any changes to SRenderState's CStringPtr member variables will necessitate an 
	// update to CCore::DeleteGeometry() and SwapEndianRenderState in Parser.cpp
	struct ST_DLL_LINK SRenderState
	{
			// specify the # of shadow maps to use in the example cascaded shadow map system
            enum EShadowConfig
            {
                SHADOW_CONFIG_OFF,
                SHADOW_CONFIG_1_MAP,
                SHADOW_CONFIG_2_MAPS,
                SHADOW_CONFIG_3_MAPS,
                SHADOW_CONFIG_4_MAPS
            };

										SRenderState( );

			st_bool						operator==(const SRenderState& sRight) const;
			st_bool						operator!=(const SRenderState& sRight) const;

			// utility queries
			st_bool						IsPerPixelModelActive(void) const;
			st_bool						IsLightingModelInTransition(void) const;
			CFixedString				VertexShaderHashName(const CWind& cWind, EShadowConfig eShadowConfig) const;
			CFixedString				PixelShaderHashName(EShadowConfig eShadowConfig) const;
			void						GetPixelProperties(st_bool abPixelProperties[PIXEL_PROPERTY_COUNT]) const;
			SVertexDecl::EInstanceType	GetInstanceType(void) const;
			st_bool						ShaderGenHasFixedDecls(void) const;

			// 5.x compatibility options
			EShaderGenerationMode		ShaderGenerationMode(void) const;

			// textures
			st_bool						IsTextureLayerPresent(ETextureLayer eLayer) const;
			CStringPtr					m_apTextures[TL_NUM_TEX_LAYERS];

			// lighting model
			ELightingModel				m_eLightingModel;

			// ambient
			Vec3						m_vAmbientColor;
			ELightingEffect				m_eAmbientContrast;
			st_float32					m_fAmbientContrastFactor;
			st_bool						m_bAmbientOcclusion;

			// diffuse
			Vec3						m_vDiffuseColor;
			st_float32					m_fDiffuseScalar;
			st_bool						m_bDiffuseAlphaMaskIsOpaque;

			// detail
			ELightingEffect				m_eDetailLayer;

			// specular
			ELightingEffect				m_eSpecular;
			st_float32					m_fShininess;
			Vec3						m_vSpecularColor;

			// transmission
			ELightingEffect				m_eTransmission;
			Vec3						m_vTransmissionColor;
			st_float32					m_fTransmissionShadowBrightness;
			st_float32					m_fTransmissionViewDependency;

			// branch seam smoothing
			ELightingEffect				m_eBranchSeamSmoothing;
			st_float32					m_fBranchSeamWeight;

			// LOD parameters
			ELodMethod					m_eLodMethod;
			st_bool						m_bFadeToBillboard;
			st_bool						m_bVertBillboard;
			st_bool						m_bHorzBillboard;

			// render states
			EShaderGenerationMode		m_eShaderGenerationMode;
			st_bool						m_bUsedAsGrass;
			ECullType					m_eFaceCulling;
			st_bool						m_bBlending;
			ELightingEffect				m_eAmbientImageLighting;
			ELightingEffect				m_eHueVariation;

			// fog
			EFogCurve					m_eFogCurve;					// determines how fog effect is distributed over distance
			EFogColorType				m_eFogColorStyle;				// determines how the fog color is determined (e.g. gradually 
																		// fog to a single color or to a complex value)
			// shadows
			st_bool						m_bCastsShadows;
			st_bool						m_bReceivesShadows;
			st_bool						m_bShadowSmoothing;

			// alpha effects
			st_float32					m_fAlphaScalar;

			// wind
			st_bool						IsBranchWindEnabled(void) const;
			st_bool						IsGlobalWindEnabled(void) const;
			st_bool						IsFullWindEnabled(void) const;
			EWindLod					m_eWindLod;

			// non-lighting shaders
			void						MakeDepthOnly(void);
			void						MakeShadowCast(void);
			ERenderPass					m_eRenderPass;

			// geometry types
			st_bool						HasOnlyBranches(void) const;
			st_bool						HasOnlyFronds(void) const;
			st_bool						HasOnlyLeaves(st_bool bFacing = true, st_bool bNonFacing = true) const;
			st_bool						HasOnlyRigidMeshes(void) const;
			st_bool						m_bBranchesPresent;
			st_bool						m_bFrondsPresent;
			st_bool						m_bLeavesPresent;
			st_bool						m_bFacingLeavesPresent;
			st_bool						m_bRigidMeshesPresent;

			// vertex format data
			SVertexDecl					m_sVertexDecl;

			// misc
			CStringPtr					m_pDescription;
			CStringPtr					m_pUserData;

	private:
			CFixedString				VertexDeclHash(void) const;
			CFixedString				PixelDeclHash(void) const;
	};


	///////////////////////////////////////////////////////////////////////  
	//  Structure SVerticalBillboards
	//
	//	All of the data needed to house 360-degree billboard cutouts as 
	//	generated by the Compiler app.

	struct ST_DLL_LINK SVerticalBillboards
	{
										SVerticalBillboards( );
										~SVerticalBillboards( );

			st_float32					m_fWidth;				// width of the billboard, governed by tree extents		
			st_float32					m_fTopPos;				// top-most point of the billboard, governed by tree height
			st_float32					m_fBottomPos;			// bottom-most point, can be below zero for trees with roots, etc.
			st_int32					m_nNumBillboards;		// number of 360-degree billboards generated by Compiler app

			const st_float32*			m_pTexCoords;			// 4 entries per image (left u, bottom v, width u, height v)
			const st_byte*				m_pRotated;				// one entry per image, 1 = rotated, 0 = standard

			// the Compiler app can generate non-rectangular cutouts, reducing the fill requirements at the
			// cost of added vertices; these vertices are 
			st_int32					m_nNumCutoutVertices;	
			const st_float32*			m_pCutoutVertices;		// # elements = 2 * m_nNumCutoutVertices [ (x,y) pairs ];
																// [x,y] values are range [0,1] as percent across height & width

			st_int32					m_nNumCutoutIndices;
			const st_uint16*			m_pCutoutIndices;		// # elements = m_nNumCutoutIndices, indexed triangles
	};


	///////////////////////////////////////////////////////////////////////  
	//  Structure SHorizontalBillboard
	//
	//	Housing for the much-simpler horizontal billboard (no cutouts or
	//	multiple views used)

	struct ST_DLL_LINK SHorizontalBillboard
	{
								SHorizontalBillboard( );
								~SHorizontalBillboard( );

			st_bool				m_bPresent;					// true if an overhead billboard was exported using Compiler
			Vec3				m_avPositions[4];			// four sets of (xyz) to render the overhead square
			st_float32			m_afTexCoords[8];			// 4 * (s,t) pairs of diffuse/normal texcoords
	};


	///////////////////////////////////////////////////////////////////////  
	//  Structure SCollisionObject
	//
	//	There are two collision object types: sphere and capsules; m_vCenter1 and
	//	m_vCenter2 will be equal for spheres.

	struct ST_DLL_LINK SCollisionObject
	{
								SCollisionObject( );
								~SCollisionObject( );
						
			CStringPtr			m_pUserString;				// any data entered by the artist in the Modeler app
			Vec3				m_vCenter1;					// center of sphere or one end of a capsule
			Vec3				m_vCenter2;					// other end of capsule or same as m_vCenter1 if sphere
			st_float32			m_fRadius;					// radius of the sphere or capsule
	};


	///////////////////////////////////////////////////////////////////////  
	//  Structure SDrawCall

	typedef CPaddedPtr<st_byte> CBytePointer;
	typedef CPaddedPtr<SRenderState> CRenderStatePointer;

	// structure is serialized in Parser.cpp
	struct ST_DLL_LINK SDrawCall
	{
                                        SDrawCall( );
                                        ~SDrawCall( );

			// render state
            CRenderStatePointer         m_pRenderState;      // typecast to SRenderState*
            st_int32                    m_nRenderStateIndex;

			// vertices
            st_int32                    m_nNumVertices;
            CBytePointer                m_pVertexData;       // mixed types, but can typecast to st_byte*

            // vertex data access helper functions; if you know exacly how the vertex attributes are
            // packed in m_pVertexData, you can skip using these functions and access m_pVertexData directly.
            st_bool                     GetProperty(EVertexProperty eProperty, st_int32 nVertex, st_float32 afValues[4]) const;
            st_bool                     GetProperty(EVertexProperty eProperty, st_int32 nVertex, st_float16 ahfValues[4]) const;
            st_bool                     GetProperty(EVertexProperty eProperty, st_int32 nVertex, st_byte abValues[4]) const;

            // internal use
            st_bool                     SetProperty(EVertexProperty eProperty, st_int32 nVertex, const st_float32 afValues[4]);
            st_bool                     SetProperty(EVertexProperty eProperty, st_int32 nVertex, const st_float16 ahfValues[4]);
            st_bool                     SetProperty(EVertexProperty eProperty, st_int32 nVertex, const st_byte abValues[4]);

            // indices
            st_int32                    m_nNumIndices;
            st_bool                     m_b32BitIndices;
            CBytePointer                m_pIndexData;       // can typecast to st_byte*, then to st_uint32* or st_uint16*,
                                                            // depending on m_b32BitIndices
	};


	///////////////////////////////////////////////////////////////////////  
	//  Structure SBone

	struct ST_DLL_LINK SBone
	{
								SBone( );

			st_int32			m_nID;
			st_int32			m_nParentID;
			Vec3				m_vStart;
			Vec3				m_vEnd;
			st_float32			m_fRadius;
			st_float32			m_fMass;
			st_float32			m_fMassWithChildren;
			st_bool				m_bBreakable;
	};


	///////////////////////////////////////////////////////////////////////  
	//  Structure SLod

	typedef CPaddedPtr<SDrawCall> CDrawCallPointer;
	typedef CPaddedPtr<SBone> CBonePointer;

    struct ST_DLL_LINK SLod
    {
                                    SLod( );
                                    ~SLod( );

            st_int32                m_nNumDrawCalls;
            CDrawCallPointer        m_pDrawCalls;   // typecast to SDrawCall*

            st_int32                m_nNumBones;
            CBonePointer            m_pBones;       // typecast to SBone*
    };


	///////////////////////////////////////////////////////////////////////  
	//  Structure SGeometry

	typedef CPaddedPtr<SLod> CLodPointer;

    struct ST_DLL_LINK SGeometry
    {
                                        SGeometry( );
                                        ~SGeometry( );

            // render states
            st_int32                    m_nNum3dRenderStates;
            st_bool                     m_bDepthOnlyIncluded;
            st_bool                     m_bShadowCastIncluded;
			CStringPtr					m_strShaderPath;
            SRenderState*               m_p3dRenderStates[RENDER_PASS_COUNT];
            SRenderState                m_aBillboardRenderStates[RENDER_PASS_COUNT];

            // LODs
            st_int32                    m_nNumLods;
            CLodPointer                 m_pLods;        // typecast to SLod*

            // billboards
            SVerticalBillboards         m_sVertBBs;
            SHorizontalBillboard        m_sHorzBB;
    };


	///////////////////////////////////////////////////////////////////////  
	//  Structure SLodRange

	struct ST_DLL_LINK SLodProfile
	{
							SLodProfile( );

			st_bool			IsValid(void) const;
			void			ComputeDerived(void);
			void			Scale(st_float32 fScale);
			void			Square(SLodProfile& sSquaredProfile) const;

							// useful for shader 4fv upload
							operator const st_float32*(void) const;

			// pack four st_float32 values together for optimal 4fv shader upload
			st_float32		m_fHighDetail3dDistance;		// distance at which LOD transition from highest 3D level begins
			st_float32		m_f3dRange;						// m_fLowDetail3dDistance - m_fHighDetail3dDistance
			st_float32		m_fBillboardStartDistance;		// distance at which the billboard begins to fade in and 3D fade out
			st_float32		m_fBillboardRange;				// m_fBillboardFinalDistance - m_fBillboardStartDistance

			st_float32		m_fLowDetail3dDistance;			// distance at which the lowest 3D level is sustained
			st_float32		m_fBillboardFinalDistance;		// distance at which the billboard is fully visible and 3D is completely gone
			st_bool			m_bLodIsPresent;
	};


	///////////////////////////////////////////////////////////////////////  
	//  Class CAllocatorInterface
	//
	//	Here are two examples of how to use the CAllocatorInterface class
	//	to specify a custom allocator to the SDK:
	//
	//	//  Method #1
	//  //
	//  //  Create a global or static allocator object and pass it into the SDK before
	//  //  main is called.  Note that the CAllocatorInterface object constructor
	//  //  takes a pointer to the allocator object.
	//
	//  static CReferenceAllocator g_cMyAllocator;
	//  static SpeedTree::CAllocatorInterface g_cAllocatorInterface(&g_cMyAllocator);
    //
	//  /////////////////////////////////////////////////////////////////////// 
	//  // main
	//  //
	//  // Method #2
	//  //
	//  // Use the SpeedTree::CAllocatorInterface class to enable or disable the
	//  // allocator object at any point.
	//
	//  void main(void)
	//  {
	//	  ...
	//    // don't use a custom allocator
	//    CAllocatorInterface cOff(NULL);
	//    
	//	   ... // do something
	//
	//    // turn the allocator back on
	//    CAllocatorInterface cOn(&g_cMyAllocator);
	//  }

	class ST_DLL_LINK CAllocatorInterface
	{
	public:
					CAllocatorInterface(CAllocator* pAllocator);
	};


	///////////////////////////////////////////////////////////////////////  
	//  Class CFileSystemInterface
	//
	//	The interface for establishing a custom file system is the same as
	//	CAllocatorInterface above. It's not a simple function call so as to 
	//  facilitate establishing the interface before main() is called if desired.

	class ST_DLL_LINK CFileSystemInterface
	{
	public:
													CFileSystemInterface(CFileSystem* pFileSystem);

	static	SpeedTree::CFileSystem*  ST_CALL_CONV	Get(void);
	};


	///////////////////////////////////////////////////////////////////////  
	//  Class CCore
	    
	class ST_DLL_LINK CCore
	{
	public:
	friend class CParser;

											CCore( );
			virtual							~CCore( );

			// loading
			st_bool							LoadTree(const st_char* pFilename, st_bool bGrassModel = false, st_float32 fScalar = 1.0f);
			st_bool							LoadTree(const st_byte* pMemBlock, 
													 st_uint32 uiNumBytes, 
													 st_bool bCopyBuffer,
													 st_bool bGrassModel = false, 
													 st_float32 fScalar = 1.0f);
			const st_char*					GetFilename(void) const;
	static	size_t			   ST_CALL_CONV	FileSizeInBytes(const st_char* pFilename);
	static	st_byte*		   ST_CALL_CONV LoadFileIntoBuffer(const st_char* pFilename, size_t& siBufferSize, st_byte* pClientSideBuffer = NULL);

			// convenience queries
			st_bool							IsCompiledForDeferred(void) const;
			st_bool							IsCompiledAsGrass(void) const;

			// geometry
			const SGeometry*				GetGeometry(void) const;
			const CExtents&					GetExtents(void) const;
			st_bool							IsGrassModel(void) const;
            st_bool                         AreTexCoordsFlipped(void) const;

			// LOD
			const SLodProfile&				GetLodProfile(void) const;
			const SLodProfile&				GetLodProfileSquared(void) const;
			st_bool							SetLodProfile(const SLodProfile& sLodProfile);
			st_int32						ComputeLodSnapshot(st_float32 fLod) const;
			st_float32						ComputeLodByDistance(st_float32 fDistance) const;
			st_float32						ComputeLodByDistanceSquared(st_float32 fDistanceSquared) const;
	static	st_float32		   ST_CALL_CONV ComputeLodTransition(st_float32 fLod, st_int32 nNumDiscreteLevels);

			// clip-space settings (part of the culling system)
	static	void			   ST_CALL_CONV	SetClipSpaceDepthRange(st_float32 fNear, st_float32 fFar); // opengl is (-1,1), directx is (0,1)
	static	void			   ST_CALL_CONV GetClipSpaceDepthRange(st_float32& fNear, st_float32& fFar);

			// wind
			CWind&							GetWind(void);
			const CWind&					GetWind(void) const;
			const st_float32*				GetWindShaderTable(void) const;

			// collision
			const SCollisionObject*			GetCollisionObjects(st_int32& nNumObjects) const;

			// hue variation
			struct ST_DLL_LINK SHueVariationParams
			{
											SHueVariationParams( ) :
												m_fByPos(0.0f),
												m_fByVertex(0.0f)
											{
											};

				st_float32					m_fByPos;
				st_float32					m_fByVertex;
				Vec3						m_vColor;
			};

			void							SetHueVariationParams(const SHueVariationParams& sParams);
			const SHueVariationParams&		GetHueVariationParams(void) const;

			// image-based ambient lighting
			void							SetAmbientImageScalar(st_float32 fScalar);
			st_float32						GetAmbientImageScalar(void) const;

			// user data
			enum EUserStringOrdinal
			{
				USER_STRING_0,
				USER_STRING_1,
				USER_STRING_2,
				USER_STRING_3,
				USER_STRING_4,
				USER_STRING_COUNT
			};
			const char*						GetUserString(EUserStringOrdinal eOrdinal) const;
			void*							GetUserData(void) const;
			void							SetUserData(void* pData);

			// memory management
			struct ST_DLL_LINK SResourceStats
			{
											SResourceStats( );

				size_t						m_siCurrentUsage;		// bytes
				size_t						m_siPeakUsage;			// bytes
				size_t						m_siCurrentQuantity;
				size_t						m_siPeakQuantity;
			};

			struct ST_DLL_LINK SResourceSummary
			{
				SResourceStats				m_sHeap;
				SResourceStats				m_asGfxResources[GFX_RESOURCE_COUNT];
			};

	static	const SResourceSummary& ST_CALL_CONV GetSdkResourceUsage(void);
	static	void			   ST_CALL_CONV ShutDown(void);
			void							DeleteGeometry(void);

			// error system
	static	void			   ST_CALL_CONV SetError(const st_char* pError, ...);
	static	const st_char*	   ST_CALL_CONV GetError(void);
	static	st_bool		       ST_CALL_CONV	IsRunTimeBigEndian(void);

			// licensing/evaluation system
	static  st_bool   		   ST_CALL_CONV Authorize(const st_char* pKey);
	static  st_bool 		   ST_CALL_CONV IsAuthorized(void);
	static  const st_char*	   ST_CALL_CONV Version(st_bool bShort = false);

			// utility
	static	Vec3		       ST_CALL_CONV UncompressVec3(const st_uint8* pCompressedVector);
	static	st_float32	       ST_CALL_CONV UncompressScalar(st_uint8 uiCompressedScalar);
	static	void			   ST_CALL_CONV CompressVec3(st_uint8 auiCompressedValue[3], const Vec3& vVector);
	static  st_uint8		   ST_CALL_CONV CompressScalar(st_float32 fUncompressedScalar);
	static	const st_char*     ST_CALL_CONV	ComponentName(st_int32 nComponent);
	static	const SVertexPropertyDesc& ST_CALL_CONV GetVertexPropertyDesc(EVertexProperty eProperty);
	static	const SPixelPropertyDesc&  ST_CALL_CONV GetPixelPropertyDesc(EPixelProperty eProperty);

			// mostly internal functions used by the whole SDK to keep heap allocation count low
	static	st_byte*		   ST_CALL_CONV TmpHeapBlockLock(size_t siSizeInBytes, const char* pOwner, st_int32& nHandle);
	static  st_bool			   ST_CALL_CONV TmpHeapBlockUnlock(st_int32 nHandle);
	static  st_int32	       ST_CALL_CONV TmpHeapBlockFindHandle(const st_byte* pBlock);
	static	st_bool			   ST_CALL_CONV TmpHeapBlockDelete(st_int32 nHandle, size_t siSizeThreshold = 0); // delete block if >= siSizeThreshold (in bytes)
	static	st_bool			   ST_CALL_CONV	TmpHeapBlockDeleteAll(size_t siSizeThreshold = 0); // delete anything >= siSizeThreshold (in bytes)

			// internal functions used to track memory usage throughout the SDK
	static	void			   ST_CALL_CONV ResourceAllocated(EGfxResourceType eType, const CFixedString& strResourceKey, size_t siSize);
	static  void			   ST_CALL_CONV ResourceReleased(const CFixedString& strResourceKey);
	
	protected:
											CCore(const CCore& cRight);	// copying CCore disabled

			// internal utilities
			const st_byte*					SrtBuffer(void) const;
			void							ReassignPointer(st_byte*& pPointer, const st_byte* pRefBlock) const;
			void							ReassignRenderState(SRenderState& sRenderState, const st_byte* pOriginalSrtBuffer) const;
			void							ApplyScale(st_float32 fScalar);

			// associated with loading the SRT file
			CFixedString					m_strFilename;						// contains full path of SRT file if loaded through LoadTree(const char*)
			st_byte*						m_pSrtBufferOwned;					// used if a copy of the supplied SRT buffer was made by CCore
			const st_byte*					m_pSrtBufferExternal;				// used if block is owned by the application and cannot be deleted by the SDK
			size_t							m_asiSubSrtBufferOffsets[2];		// used to where the geometry chunk of the m_pSrtBuffer starts and ends ([0] = start, [1] = end)

			// properties of the tree model, read or derived from SRT file
			SGeometry						m_sGeometry;						// contains all geometry for all LODs, 3D and billboard, and all render state blocks
			SLodProfile						m_sLodProfile;						// contains near, far, and billboard LOD distances
			SLodProfile						m_sLodProfileSquared;				// contains squared values of near, far, etc to speed LOD computation during render
			CExtents						m_cExtents;							// contains min/max xyz extents of tree model
			st_bool							m_bGrassModel;
            st_bool                         m_bTexCoordsFlipped;

			// wind control
			CWind							m_cWind;							// parameters governing behavior of the wind system are stored in the SRT file and loaded into
																				// this class which governs wind behavior over time

			// collision data
			st_int32						m_nNumCollisionObjects;				// # of collision objects stored in this tree
			SCollisionObject*				m_pCollisionObjects;				// pointer to block of collision objects

			// shader data
			SHueVariationParams				m_sHueVariationParams;

			// image-based ambient lighting
			st_float32						m_fAmbientImageScalar;

			// misc data
			const st_char*					m_pUserStrings[USER_STRING_COUNT];	// users can enter a few strings of arbitrary length in the Modeler application
			void*							m_pUserData;						// each CCore object stores a single void* for generic user data storage
	};
	typedef CCore CTree;


	// include inline functions
	#include "Core/StaticArray_inl.h"
	#include "Core/Core_inl.h"
	#include "Core/RenderState_inl.h"
	#include "Core/Geometry_inl.h"
	#include "Core/VertexDecl_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"

#ifdef _WIN32
	#pragma warning(pop) // 64-bit alignment warnings
#endif

