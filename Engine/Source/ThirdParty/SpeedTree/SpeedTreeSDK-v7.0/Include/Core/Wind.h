///////////////////////////////////////////////////////////////////////  
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement.
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		Web: http://www.idvinc.com


///////////////////////////////////////////////////////////////////////  
// Preprocessor / Includes

#pragma once
#include "Core/ExportBegin.h"
#include "Core/Types.h"


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	// Constants

	const st_int32 c_nNumWindPointsInCurves = 10;

	// adjusting these constants is not enough to add more levels or groups (need more shaders, different data uploaded, CWind modifications, etc.)
	const st_int32 c_nNumBranchLevels = 2;
	const st_int32 c_nNumLeafGroups = 2;


	///////////////////////////////////////////////////////////////////////  
	// class CWind

	class ST_DLL_LINK CWind
	{
	public:
			// shader state that are set at compile time
			enum EOptions
			{
				GLOBAL_WIND,
				GLOBAL_PRESERVE_SHAPE,

				BRANCH_SIMPLE_1,
				BRANCH_DIRECTIONAL_1,
				BRANCH_DIRECTIONAL_FROND_1,
				BRANCH_TURBULENCE_1,
				BRANCH_WHIP_1,
				BRANCH_OSC_COMPLEX_1,

				BRANCH_SIMPLE_2,
				BRANCH_DIRECTIONAL_2,
				BRANCH_DIRECTIONAL_FROND_2,
				BRANCH_TURBULENCE_2,
				BRANCH_WHIP_2,
				BRANCH_OSC_COMPLEX_2,

				LEAF_RIPPLE_VERTEX_NORMAL_1,
				LEAF_RIPPLE_COMPUTED_1,
				LEAF_TUMBLE_1,
				LEAF_TWITCH_1,
				LEAF_OCCLUSION_1,

				LEAF_RIPPLE_VERTEX_NORMAL_2,
				LEAF_RIPPLE_COMPUTED_2,
				LEAF_TUMBLE_2,
				LEAF_TWITCH_2,
				LEAF_OCCLUSION_2,

				FROND_RIPPLE_ONE_SIDED,
				FROND_RIPPLE_TWO_SIDED,
				FROND_RIPPLE_ADJUST_LIGHTING,

				ROLLING,

				NUM_WIND_OPTIONS
			};

			// values to be uploaded as shader constants
			enum EShaderValues
			{
				// each comment specifies corresponding member variable in SWindDynamicsCBLayout in ShaderConstantsBuffer.h 

				//m_vDirection
				SH_WIND_DIR_X, SH_WIND_DIR_Y, SH_WIND_DIR_Z, 
				// m_fStrength
				SH_GENERAL_STRENGTH,
				// m_vAnchor
				SH_WIND_ANCHOR_X, SH_WIND_ANCHOR_Y, SH_WIND_ANCHOR_Z, SH_WIND_PAD0,

				// SGlobal
				//
				// m_sGlobal.m_fGlobalWindTime
				SH_GLOBAL_TIME, 
				// m_sGlobal.m_fGlobalWindDistance
				SH_GLOBAL_DISTANCE, 
				// m_sGlobal.m_fHeight
				SH_GLOBAL_HEIGHT,
				// m_sGlobal.m_fHeightExponent
				SH_GLOBAL_HEIGHT_EXPONENT,
				// m_sGlobal.m_fAdherence
				SH_GLOBAL_DIRECTION_ADHERENCE, SH_WIND_PAD1, SH_WIND_PAD2, SH_WIND_PAD3,

				// SBranchWind (first instance)
				//
				// m_sBranch1.m_fBranchWindTime
				SH_BRANCH_1_TIME,
				// m_sBranch1.m_fBranchWindDistance
				SH_BRANCH_1_DISTANCE,
				// m_sBranch1.m_fTwitch
				SH_BRANCH_1_TWITCH,
				// m_sBranch1.m_fTwitchFreqScale
				SH_BRANCH_1_TWITCH_FREQ_SCALE,
				// m_sBranch1.m_fWhip
				SH_BRANCH_1_WHIP,
				// m_sBranch1.m_fDirectionAdherence
				SH_BRANCH_1_DIRECTION_ADHERENCE,
				// m_sBranch1.m_fTurbulence
				SH_BRANCH_1_TURBULENCE, SH_WIND_PAD4,

				// SBranchWind (second instance)
				//
				// m_sBranch2.m_fBranchWindTime
				SH_BRANCH_2_TIME,
				// m_sBranch2.m_fBranchWindDistance
				SH_BRANCH_2_DISTANCE,
				// m_sBranch2.m_fTwitch
				SH_BRANCH_2_TWITCH,
				// m_sBranch2.m_fTwitchFreqScale
				SH_BRANCH_2_TWITCH_FREQ_SCALE,
				// m_sBranch2.m_fWhip
				SH_BRANCH_2_WHIP,
				// m_sBranch2.m_fDirectionAdherence
				SH_BRANCH_2_DIRECTION_ADHERENCE,
				// m_sBranch2.m_fTurbulence
				SH_BRANCH_2_TURBULENCE, SH_WIND_PAD5,

				// SLeaf (first instance)
				//
				// m_sLeaf1.m_fRippleTime
				SH_LEAF_1_RIPPLE_TIME,
				// m_sLeaf1.m_fRippleDistance
				SH_LEAF_1_RIPPLE_DISTANCE,
				// m_sLeaf1.m_fLeewardScalar
				SH_LEAF_1_LEEWARD_SCALAR,
				// m_sLeaf1.m_fTumbleTime
				SH_LEAF_1_TUMBLE_TIME,
				// m_sLeaf1.m_fTumbleFlip
				SH_LEAF_1_TUMBLE_FLIP,
				// m_sLeaf1.m_fTumbleTwist
				SH_LEAF_1_TUMBLE_TWIST,
				// m_sLeaf1.m_fTumbleDirectionAdherence
				SH_LEAF_1_TUMBLE_DIRECTION_ADHERENCE,
				// m_sLeaf1.m_fTwitchThrow
				SH_LEAF_1_TWITCH_THROW,
				// m_sLeaf1.m_fTwitchSharpness
				SH_LEAF_1_TWITCH_SHARPNESS,
				// m_sLeaf1.m_fTwitchTime
				SH_LEAF_1_TWITCH_TIME, SH_WIND_PAD6, SH_WIND_PAD7,

				// SLeaf (second instance)
				//
				// m_sLeaf2.m_fRippleTime
				SH_LEAF_2_RIPPLE_TIME,
				// m_sLeaf2.m_fRippleDistance
				SH_LEAF_2_RIPPLE_DISTANCE,
				// m_sLeaf2.m_fLeewardScalar
				SH_LEAF_2_LEEWARD_SCALAR,
				// m_sLeaf2.m_fTumbleTime
				SH_LEAF_2_TUMBLE_TIME,
				// m_sLeaf2.m_fTumbleFlip
				SH_LEAF_2_TUMBLE_FLIP,
				// m_sLeaf2.m_fTumbleTwist
				SH_LEAF_2_TUMBLE_TWIST,
				// m_sLeaf2.m_fTumbleDirectionAdherence
				SH_LEAF_2_TUMBLE_DIRECTION_ADHERENCE,
				// m_sLeaf2.m_fTwitchThrow
				SH_LEAF_2_TWITCH_THROW,
				// m_sLeaf2.m_fTwitchSharpness
				SH_LEAF_2_TWITCH_SHARPNESS,
				// m_sLeaf2.m_fTwitchTime
				SH_LEAF_2_TWITCH_TIME, SH_WIND_PAD8, SH_WIND_PAD9,

				// SFrondRipple
				//
				// m_sFrondRipple.m_fFrondRippleTime
				SH_FROND_RIPPLE_TIME,
				// m_sFrondRipple.m_fFrondRippleDistance
				SH_FROND_RIPPLE_DISTANCE,
				// m_sFrondRipple.m_fTile
				SH_FROND_RIPPLE_TILE,
				// m_sFrondRipple.m_fLightingScalar
				SH_FROND_RIPPLE_LIGHTING_SCALAR,

				// SRolling
				//
				// m_sRolling.m_fBranchFieldMin
				SH_ROLLING_BRANCH_FIELD_MIN,
				// m_sRolling.m_fBranchLightingAdjust
				SH_ROLLING_BRANCH_LIGHTING_ADJUST,
				// m_sRolling.m_fBranchVerticalOffset
				SH_ROLLING_BRANCH_VERTICAL_OFFSET,
				// m_sRolling.m_fLeafRippleMin
				SH_ROLLING_LEAF_RIPPLE_MIN,
				// m_sRolling.m_fLeafTumbleMin
				SH_ROLLING_LEAF_TUMBLE_MIN,
				// m_sRolling.m_fNoisePeriod
				SH_ROLLING_NOISE_PERIOD,
				// m_sRolling.m_fNoiseSize
				SH_ROLLING_NOISE_SIZE,
				// m_sRolling.m_fNoiseTurbulence
				SH_ROLLING_NOISE_TURBULENCE,
				// m_sRolling.m_fNoiseTwist
				SH_ROLLING_NOISE_TWIST,
				// m_sRolling.m_vOffset
				SH_ROLLING_X, SH_ROLLING_Y, SH_WIND_PAD10,
				   
				NUM_SHADER_VALUES
			};

			// wind simulation components that oscillate
			enum EOscillationComponents
			{
				OSC_GLOBAL, 
				OSC_BRANCH_1, 
				OSC_BRANCH_2, 
				OSC_LEAF_1_RIPPLE, 
				OSC_LEAF_1_TUMBLE, 
				OSC_LEAF_1_TWITCH, 
				OSC_LEAF_2_RIPPLE, 
				OSC_LEAF_2_TUMBLE, 
				OSC_LEAF_2_TWITCH, 
				OSC_FROND_RIPPLE, 
				NUM_OSC_COMPONENTS
			};

			struct ST_DLL_LINK SBranchWindLevel
			{
									SBranchWindLevel( );

				st_float32			m_afDistance[c_nNumWindPointsInCurves];
				st_float32			m_afDirectionAdherence[c_nNumWindPointsInCurves];
				st_float32			m_afWhip[c_nNumWindPointsInCurves];
				st_float32			m_fTurbulence;
				st_float32			m_fTwitch;
				st_float32			m_fTwitchFreqScale;
			};

			struct ST_DLL_LINK SWindGroup
			{
									SWindGroup( );

				st_float32			m_afRippleDistance[c_nNumWindPointsInCurves];
				st_float32			m_afTumbleFlip[c_nNumWindPointsInCurves];
				st_float32			m_afTumbleTwist[c_nNumWindPointsInCurves];
				st_float32			m_afTumbleDirectionAdherence[c_nNumWindPointsInCurves];
				st_float32			m_afTwitchThrow[c_nNumWindPointsInCurves];
				st_float32			m_fTwitchSharpness;
				st_float32			m_fRollMaxScale;
				st_float32			m_fRollMinScale;
				st_float32			m_fRollSpeed;
				st_float32			m_fRollSeparation;
				st_float32			m_fLeewardScalar;
			};

			struct ST_DLL_LINK SParams
			{
									SParams( );

				// settings
				st_float32			m_fStrengthResponse;
				st_float32			m_fDirectionResponse;

				st_float32			m_fAnchorOffset;
				st_float32			m_fAnchorDistanceScale;

				// oscillation components
				st_float32			m_afFrequencies[NUM_OSC_COMPONENTS][c_nNumWindPointsInCurves];

				// global motion
				st_float32			m_fGlobalHeight;
				st_float32			m_fGlobalHeightExponent;
				st_float32			m_afGlobalDistance[c_nNumWindPointsInCurves];
				st_float32			m_afGlobalDirectionAdherence[c_nNumWindPointsInCurves];

				// branch motion
				SBranchWindLevel	m_asBranch[c_nNumBranchLevels];

				// leaf motion
				SWindGroup			m_asLeaf[c_nNumLeafGroups];

				// frond ripple
				st_float32			m_afFrondRippleDistance[c_nNumWindPointsInCurves];
				st_float32			m_fFrondRippleTile;
				st_float32			m_fFrondRippleLightingScalar;

				// rolling
				st_float32			m_fRollingNoiseSize;
				st_float32			m_fRollingNoiseTwist;
				st_float32			m_fRollingNoiseTurbulence;
				st_float32			m_fRollingNoisePeriod;
				st_float32			m_fRollingNoiseSpeed;

				st_float32			m_fRollingBranchFieldMin;
				st_float32			m_fRollingBranchLightingAdjust;
				st_float32			m_fRollingBranchVerticalOffset;
				st_float32			m_fRollingLeafRippleMin;
				st_float32			m_fRollingLeafTumbleMin;

				// gusting
				st_float32			m_fGustFrequency;
				st_float32			m_fGustStrengthMin;
				st_float32			m_fGustStrengthMax;
				st_float32			m_fGustDurationMin;
				st_float32			m_fGustDurationMax;
				st_float32			m_fGustRiseScalar;
				st_float32			m_fGustFallScalar;
			};

									CWind( );

			// settings
			void					SetParams(const CWind::SParams& sParams);		// this should be called infrequently and never when trees that use it are visible
			const CWind::SParams&	GetParams(void) const;
			void					SetStrength(st_float32 fStrength);				// use this function to set a new desired strength (it will reach that strength smoothly)
			void					SetDirection(const Vec3& vDir);					// use this function to set a new desired direction (it will reach that direction smoothly)
			void					SetInitDirection(const Vec3& vDir);				// use this function to set a starting direction, once
			void					EnableGusting(st_bool bEnabled);
			void					SetGustFrequency(st_float32 fGustFreq);
			void					Scale(st_float32 fScalar);

			// tree-specific values
			void					SetTreeValues(const Vec3& vBranchAnchor, st_float32 fMaxBranchLength);
			const st_float32*		GetBranchAnchor(void) const;
			st_float32				GetMaxBranchLength(void) const;

			// shader options
			void					SetOption(EOptions eOption, st_bool bState);
			st_bool					IsOptionEnabled(EOptions eOption) const;	// query individual wind options
			st_bool					IsGlobalWindEnabled(void) const;			// query global wind options as a group
			st_bool					IsBranchWindEnabled(void) const;			// query branch-related wind options as a group
		
			// animation
			void					Advance(st_bool bEnabled, st_float32 fTime);	// called every frame to 'tick' the wind
			const st_float32*		GetShaderTable(void) const;

	protected:
			void					Gust(st_float32 fTime);
			st_float32				RandomFloat(st_float32 fMin, st_float32 fMax) const;
			st_float32				LinearSigmoid(st_float32 fInput, st_float32 fLinearness);
			void					Normalize(st_float32* pVector);
			void					ComputeWindAnchor(st_float32* pPos);

			SParams					m_sParams;

			st_float32				m_fStrength;
			st_float32				m_afDirection[3];

			st_float32				m_fLastTime;
			st_float32				m_fElapsedTime;

			st_bool					m_bGustingEnabled;
			st_float32				m_fGust;
			st_float32				m_fGustTarget;
			st_float32				m_fGustRiseTarget;
			st_float32				m_fGustFallTarget;
			st_float32				m_fGustStart;
			st_float32				m_fGustAtStart;
			st_float32				m_fGustFallStart;

			st_float32				m_fStrengthTarget;
			st_float32				m_fStrengthChangeStartTime;
			st_float32				m_fStrengthChangeEndTime;
			st_float32				m_fStrengthAtStart;

			st_float32				m_afDirectionTarget[3];
			st_float32				m_afDirectionMidTarget[3];
			st_float32				m_fDirectionChangeStartTime;
			st_float32				m_fDirectionChangeEndTime;
			st_float32				m_afDirectionAtStart[3];

			st_float32				m_afRollingOffset[2];

			st_float32				m_fCombinedStrength;

			st_float32				m_afOscillationTimes[NUM_OSC_COMPONENTS];

			st_bool					m_abOptions[NUM_WIND_OPTIONS];

			st_float32				m_afBranchWindAnchor[3];
			st_float32				m_fMaxBranchLevel1Length;

			st_float32				m_afShaderTable[NUM_SHADER_VALUES];
	};


	// include inline functions
	#include "Core/Wind_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"

