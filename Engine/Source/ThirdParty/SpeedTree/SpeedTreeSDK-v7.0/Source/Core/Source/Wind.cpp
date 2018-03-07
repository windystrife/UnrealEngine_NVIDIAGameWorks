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


///////////////////////////////////////////////////////////////////////  
// Preprocessor / Includes

#include "Utilities/Utility.h"
#include <cstdlib>
#include <ctime>
#include "Core/Wind.h"
#include "Core/Random.h"
using namespace SpeedTree;


///////////////////////////////////////////////////////////////////////  
//	CWind::SBranchWindLevel::SBranchWindLevel

CWind::SBranchWindLevel::SBranchWindLevel( ) :
	m_fTurbulence(0.3f),
	m_fTwitch(0.75f),
	m_fTwitchFreqScale(0.3f)
{
	for (st_int32 i = 0; i < c_nNumWindPointsInCurves; ++i)
	{
		m_afDistance[i] = 0.0f;
		m_afDirectionAdherence[i] = 0.0f;
		m_afWhip[i] = 0.0f;
	}
}


///////////////////////////////////////////////////////////////////////  
//	CWind::SWindGroup::SWindGroup

CWind::SWindGroup::SWindGroup( ) :
	m_fTwitchSharpness(20.0f),
	m_fRollMaxScale(1.0f),
	m_fRollMinScale(1.0f),
	m_fRollSpeed(0.3f),
	m_fRollSeparation(0.005f),
	m_fLeewardScalar(1.0f)
{
	for (st_int32 i = 0; i < c_nNumWindPointsInCurves; ++i)
	{
		m_afRippleDistance[i] = 0.0f;
		m_afTumbleFlip[i] = 0.0f;
		m_afTumbleTwist[i] = 0.0f;
		m_afTumbleDirectionAdherence[i] = 0.0f;
		m_afTwitchThrow[i] = 0.0f;
	}
}


///////////////////////////////////////////////////////////////////////  
//	CWind::SParams::SParams

CWind::SParams::SParams( ) :
	m_fStrengthResponse(5.0f),
	m_fDirectionResponse(2.5f),
	m_fAnchorOffset(0.0f),
	m_fAnchorDistanceScale(1.0f),
	m_fGlobalHeight(50.0f),
	m_fGlobalHeightExponent(2.0f),
	m_fFrondRippleTile(10.0f),
	m_fFrondRippleLightingScalar(1.0f),
	m_fRollingNoiseSize(0.005f),
	m_fRollingNoiseTwist(9.0f),
	m_fRollingNoiseTurbulence(32.0f),
	m_fRollingNoisePeriod(0.4f),
	m_fRollingNoiseSpeed(0.05f),
	m_fRollingBranchFieldMin(0.5f),
	m_fRollingBranchLightingAdjust(0.5f),
	m_fRollingBranchVerticalOffset(-0.5f),
	m_fRollingLeafRippleMin(0.5f),
	m_fRollingLeafTumbleMin(0.5f),
	m_fGustFrequency(0.0f),
	m_fGustStrengthMin(0.5f),
	m_fGustStrengthMax(1.0f),
	m_fGustDurationMin(1.0f),
	m_fGustDurationMax(4.0f),
	m_fGustRiseScalar(1.0f),
	m_fGustFallScalar(1.0f)
{
	for (st_int32 i = 0; i < CWind::NUM_OSC_COMPONENTS; ++i)
		for (st_int32 j = 0; j < c_nNumWindPointsInCurves; ++j)
			m_afFrequencies[i][j] = 0.0f;

	for (st_int32 i = 0; i < c_nNumWindPointsInCurves; ++i)
	{
		m_afFrondRippleDistance[i] = 0.0f;
		m_afGlobalDistance[i] = 0.0f;
		m_afGlobalDirectionAdherence[i] = 0.0f;
	}
}


///////////////////////////////////////////////////////////////////////  
//	CWind::CWind
/// Default constructor for CWind.

CWind::CWind( ) :
	m_fStrength(0.0f),
	m_fLastTime(-1.0f),
	m_fElapsedTime(0.0f),
	m_bGustingEnabled(false),
	m_fGust(0.0f),
	m_fGustTarget(0.0f),
	m_fGustRiseTarget(0.0f),
	m_fGustFallTarget(0.0f),
	m_fGustStart(0.0f),
	m_fGustAtStart(1.0f),
	m_fGustFallStart(0.0f),
	m_fStrengthTarget(0.0f),
	m_fStrengthChangeStartTime(0.0f),
	m_fStrengthChangeEndTime(0.0f),
	m_fStrengthAtStart(0.0f),
	m_fDirectionChangeStartTime(0.0f),
	m_fDirectionChangeEndTime(0.0f),
	m_fCombinedStrength(0.0f),
	m_fMaxBranchLevel1Length(0.0f)

{
	for (st_int32 i = 0; i < CWind::NUM_OSC_COMPONENTS; ++i)
		m_afOscillationTimes[i] = 0.0f;

	for (st_int32 i = 0; i < CWind::NUM_WIND_OPTIONS; ++i)
		m_abOptions[i] = false;

	m_afDirectionTarget[0] = m_afDirectionAtStart[0] = m_afDirectionMidTarget[0] = m_afDirection[0] = 1.0f;
	m_afDirectionTarget[1] = m_afDirectionAtStart[1] = m_afDirectionMidTarget[1] = m_afDirection[1] = 0.0f;
	m_afDirectionTarget[2] = m_afDirectionAtStart[2] = m_afDirectionMidTarget[2] = m_afDirection[2] = 0.0f;

	m_afBranchWindAnchor[0] = m_afBranchWindAnchor[1] = m_afBranchWindAnchor[2] = 0.0f;

	m_afRollingOffset[0] = m_afRollingOffset[1] = 0.0f;
}


///////////////////////////////////////////////////////////////////////  
//	CWind::Advance

void CWind::Advance(st_bool bEnabled, st_float32 fTime)
{
	// keep track of time
	if (m_fLastTime == -1.0f)
		m_fElapsedTime = 0.0f;
	else
		m_fElapsedTime = fTime - m_fLastTime;
	m_fLastTime = fTime;

	if (bEnabled)
	{
		if (m_bGustingEnabled)
			Gust(fTime);

		// adjust direction
		st_float32 fDirectionFactor = 1.0f;
		if (m_fDirectionChangeEndTime != m_fDirectionChangeStartTime)
			fDirectionFactor = st_min(1.0f, st_max(0.0f, (fTime - m_fDirectionChangeStartTime) / (m_fDirectionChangeEndTime - m_fDirectionChangeStartTime)));
		fDirectionFactor = LinearSigmoid(fDirectionFactor, 0.5f);

		if (fDirectionFactor < 0.5f)
		{
			// go toward the mid-vector (the mid vector prevents fast swoops when making 180 degree direction changes)
			fDirectionFactor *= 2.0f;
			m_afDirection[0] = Interpolate(m_afDirectionAtStart[0], m_afDirectionMidTarget[0], fDirectionFactor);
			m_afDirection[1] = Interpolate(m_afDirectionAtStart[1], m_afDirectionMidTarget[1], fDirectionFactor);
			m_afDirection[2] = Interpolate(m_afDirectionAtStart[2], m_afDirectionMidTarget[2], fDirectionFactor);
		}
		else
		{
			// go away from the mid-vector (the mid vector prevents fast swoops when making 180 degree direction changes)
			fDirectionFactor = (fDirectionFactor - 0.5f) * 2.0f;
			m_afDirection[0] = Interpolate(m_afDirectionMidTarget[0], m_afDirectionTarget[0], fDirectionFactor);
			m_afDirection[1] = Interpolate(m_afDirectionMidTarget[1], m_afDirectionTarget[1], fDirectionFactor);
			m_afDirection[2] = Interpolate(m_afDirectionMidTarget[2], m_afDirectionTarget[2], fDirectionFactor);
		}
		Normalize(m_afDirection);

		// adjust strength
		st_float32 fStrengthFactor = 1.0f;
		if (m_fStrengthChangeEndTime != m_fStrengthChangeStartTime)
			fStrengthFactor = st_min(1.0f, st_max(0.0f, (fTime - m_fStrengthChangeStartTime) / (m_fStrengthChangeEndTime - m_fStrengthChangeStartTime)));
		else
			fStrengthFactor = 0.0f;

		m_fStrength = Interpolate(m_fStrengthAtStart, m_fStrengthTarget, LinearSigmoid(fStrengthFactor, 0.0f));

		// combine it with the gust value
		m_fCombinedStrength = st_max(0.0f, st_min(1.0f, m_fStrength + m_fGust));

		// update the rolling wind offset
		m_afRollingOffset[0] += m_afDirection[0] * m_fCombinedStrength * m_sParams.m_fRollingNoiseSpeed * m_fElapsedTime;
		m_afRollingOffset[1] += m_afDirection[1] * m_fCombinedStrength * m_sParams.m_fRollingNoiseSpeed * m_fElapsedTime;

		// compute oscillation indices
		st_float32 fIndex = m_fCombinedStrength * (c_nNumWindPointsInCurves - 1.0f);
		st_int32 nBefore = static_cast<int>(fIndex);
		st_int32 nAfter = nBefore + 1;
		st_float32 fInterpolate;
		if (nAfter >= c_nNumWindPointsInCurves)
		{
			nBefore = nAfter = c_nNumWindPointsInCurves - 1;
			fInterpolate = 0.0f;
		}
		else if (nBefore < 0)
		{
			nBefore = nAfter = 0;
			fInterpolate = 0.0f;
		}
		else
		{
			fInterpolate = fIndex - static_cast<float>(nBefore);
		}

		// update oscillation times
		for (st_int32 i = 0; i < NUM_OSC_COMPONENTS; ++i)
		{
			m_afOscillationTimes[i] += m_fElapsedTime * Interpolate(m_sParams.m_afFrequencies[i][nBefore], m_sParams.m_afFrequencies[i][nAfter], fInterpolate);
		}

		// set shader values
		m_afShaderTable[SH_WIND_DIR_X] = m_afDirection[0];
		m_afShaderTable[SH_WIND_DIR_Y] = m_afDirection[1]; 
		m_afShaderTable[SH_WIND_DIR_Z] = m_afDirection[2];

		// general
		m_afShaderTable[SH_GENERAL_STRENGTH] = m_fCombinedStrength;

		// global
		m_afShaderTable[SH_GLOBAL_DISTANCE] = Interpolate(m_sParams.m_afGlobalDistance[nBefore], m_sParams.m_afGlobalDistance[nAfter], fInterpolate);
		m_afShaderTable[SH_GLOBAL_TIME] = m_afOscillationTimes[OSC_GLOBAL];
		m_afShaderTable[SH_GLOBAL_HEIGHT] = m_sParams.m_fGlobalHeight != 0.0f ? (1.0f / m_sParams.m_fGlobalHeight) : 1.0f;
		m_afShaderTable[SH_GLOBAL_HEIGHT_EXPONENT] = m_sParams.m_fGlobalHeightExponent;
		m_afShaderTable[SH_GLOBAL_DIRECTION_ADHERENCE] = Interpolate(m_sParams.m_afGlobalDirectionAdherence[nBefore], m_sParams.m_afGlobalDirectionAdherence[nAfter], fInterpolate);

		// branch
		m_afShaderTable[SH_BRANCH_1_DISTANCE] = Interpolate(m_sParams.m_asBranch[0].m_afDistance[nBefore], m_sParams.m_asBranch[0].m_afDistance[nAfter], fInterpolate);
		m_afShaderTable[SH_BRANCH_1_TIME] = m_afOscillationTimes[OSC_BRANCH_1];
		m_afShaderTable[SH_BRANCH_2_DISTANCE] = Interpolate(m_sParams.m_asBranch[1].m_afDistance[nBefore], m_sParams.m_asBranch[1].m_afDistance[nAfter], fInterpolate);
		m_afShaderTable[SH_BRANCH_2_TIME] = m_afOscillationTimes[OSC_BRANCH_2];

		st_float32 afComputedWindAnchor[3];
		ComputeWindAnchor(afComputedWindAnchor);
		m_afShaderTable[SH_WIND_ANCHOR_X] = afComputedWindAnchor[0];
		m_afShaderTable[SH_WIND_ANCHOR_Y] = afComputedWindAnchor[1];
		m_afShaderTable[SH_WIND_ANCHOR_Z] = afComputedWindAnchor[2];

		m_afShaderTable[SH_BRANCH_1_DIRECTION_ADHERENCE] = Interpolate(m_sParams.m_asBranch[0].m_afDirectionAdherence[nBefore], m_sParams.m_asBranch[0].m_afDirectionAdherence[nAfter], fInterpolate);
		if (m_abOptions[BRANCH_DIRECTIONAL_1])
			m_afShaderTable[SH_BRANCH_1_DIRECTION_ADHERENCE] *= m_fMaxBranchLevel1Length;
		m_afShaderTable[SH_BRANCH_1_TURBULENCE] = m_sParams.m_asBranch[0].m_fTurbulence;
		m_afShaderTable[SH_BRANCH_2_DIRECTION_ADHERENCE] = Interpolate(m_sParams.m_asBranch[1].m_afDirectionAdherence[nBefore], m_sParams.m_asBranch[1].m_afDirectionAdherence[nAfter], fInterpolate);
		if (m_abOptions[BRANCH_DIRECTIONAL_2])
			m_afShaderTable[SH_BRANCH_2_DIRECTION_ADHERENCE] *= m_fMaxBranchLevel1Length;
		m_afShaderTable[SH_BRANCH_2_TURBULENCE] = m_sParams.m_asBranch[1].m_fTurbulence;

		m_afShaderTable[SH_BRANCH_1_TWITCH] = m_sParams.m_asBranch[0].m_fTwitch;
		m_afShaderTable[SH_BRANCH_1_TWITCH_FREQ_SCALE] = m_sParams.m_asBranch[0].m_fTwitchFreqScale;

		m_afShaderTable[SH_BRANCH_2_TWITCH] = m_sParams.m_asBranch[1].m_fTwitch;
		m_afShaderTable[SH_BRANCH_2_TWITCH_FREQ_SCALE] = m_sParams.m_asBranch[1].m_fTwitchFreqScale;

		m_afShaderTable[SH_BRANCH_1_WHIP] = Interpolate(m_sParams.m_asBranch[0].m_afWhip[nBefore], m_sParams.m_asBranch[0].m_afWhip[nAfter], fInterpolate);
		m_afShaderTable[SH_BRANCH_2_WHIP] = Interpolate(m_sParams.m_asBranch[1].m_afWhip[nBefore], m_sParams.m_asBranch[1].m_afWhip[nAfter], fInterpolate);

		// leaf ripple
		m_afShaderTable[SH_LEAF_1_RIPPLE_TIME] = m_afOscillationTimes[OSC_LEAF_1_RIPPLE];
		m_afShaderTable[SH_LEAF_1_RIPPLE_DISTANCE] = Interpolate(m_sParams.m_asLeaf[0].m_afRippleDistance[nBefore], m_sParams.m_asLeaf[0].m_afRippleDistance[nAfter], fInterpolate);

		m_afShaderTable[SH_LEAF_2_RIPPLE_TIME] = m_afOscillationTimes[OSC_LEAF_2_RIPPLE];
		m_afShaderTable[SH_LEAF_2_RIPPLE_DISTANCE] = Interpolate(m_sParams.m_asLeaf[1].m_afRippleDistance[nBefore], m_sParams.m_asLeaf[1].m_afRippleDistance[nAfter], fInterpolate);

		// leaf tumble
		m_afShaderTable[SH_LEAF_1_TUMBLE_TIME] = m_afOscillationTimes[OSC_LEAF_1_TUMBLE];
		m_afShaderTable[SH_LEAF_1_TUMBLE_FLIP] = Interpolate(m_sParams.m_asLeaf[0].m_afTumbleFlip[nBefore], m_sParams.m_asLeaf[0].m_afTumbleFlip[nAfter], fInterpolate);
		m_afShaderTable[SH_LEAF_1_TUMBLE_TWIST] = Interpolate(m_sParams.m_asLeaf[0].m_afTumbleTwist[nBefore], m_sParams.m_asLeaf[0].m_afTumbleTwist[nAfter], fInterpolate);
		m_afShaderTable[SH_LEAF_1_TUMBLE_DIRECTION_ADHERENCE] = Interpolate(m_sParams.m_asLeaf[0].m_afTumbleDirectionAdherence[nBefore], m_sParams.m_asLeaf[0].m_afTumbleDirectionAdherence[nAfter], fInterpolate);

		m_afShaderTable[SH_LEAF_2_TUMBLE_TIME] = m_afOscillationTimes[OSC_LEAF_2_TUMBLE];
		m_afShaderTable[SH_LEAF_2_TUMBLE_FLIP] = Interpolate(m_sParams.m_asLeaf[1].m_afTumbleFlip[nBefore], m_sParams.m_asLeaf[1].m_afTumbleFlip[nAfter], fInterpolate);
		m_afShaderTable[SH_LEAF_2_TUMBLE_TWIST] = Interpolate(m_sParams.m_asLeaf[1].m_afTumbleTwist[nBefore], m_sParams.m_asLeaf[1].m_afTumbleTwist[nAfter], fInterpolate);
		m_afShaderTable[SH_LEAF_2_TUMBLE_DIRECTION_ADHERENCE] = Interpolate(m_sParams.m_asLeaf[1].m_afTumbleDirectionAdherence[nBefore], m_sParams.m_asLeaf[1].m_afTumbleDirectionAdherence[nAfter], fInterpolate);

		m_afShaderTable[SH_LEAF_1_TWITCH_THROW] = Interpolate(m_sParams.m_asLeaf[0].m_afTwitchThrow[nBefore], m_sParams.m_asLeaf[0].m_afTwitchThrow[nAfter], fInterpolate);
		const st_float32 c_fTwitchSharpness1Denom = Interpolate(m_sParams.m_afFrequencies[OSC_LEAF_1_TWITCH][nBefore], m_sParams.m_afFrequencies[OSC_LEAF_1_TWITCH][nAfter], fInterpolate);
		if (c_fTwitchSharpness1Denom < FLT_EPSILON)
			m_afShaderTable[SH_LEAF_1_TWITCH_SHARPNESS] = 0.0f;
		else
			m_afShaderTable[SH_LEAF_1_TWITCH_SHARPNESS] = (1.0f / c_fTwitchSharpness1Denom) * m_sParams.m_asLeaf[0].m_fTwitchSharpness * 10.0f;
		m_afShaderTable[SH_LEAF_1_TWITCH_TIME] = m_afOscillationTimes[OSC_LEAF_1_TWITCH];

		m_afShaderTable[SH_LEAF_2_TWITCH_THROW] = Interpolate(m_sParams.m_asLeaf[1].m_afTwitchThrow[nBefore], m_sParams.m_asLeaf[1].m_afTwitchThrow[nAfter], fInterpolate);
		const st_float32 c_fTwitchSharpness2Denom = Interpolate(m_sParams.m_afFrequencies[OSC_LEAF_2_TWITCH][nBefore], m_sParams.m_afFrequencies[OSC_LEAF_2_TWITCH][nAfter], fInterpolate);
		if (c_fTwitchSharpness2Denom < FLT_EPSILON)
			m_afShaderTable[SH_LEAF_2_TWITCH_SHARPNESS] = 0.0f;
		else
			m_afShaderTable[SH_LEAF_2_TWITCH_SHARPNESS] = (1.0f / c_fTwitchSharpness2Denom) * m_sParams.m_asLeaf[1].m_fTwitchSharpness * 10.0f;
		m_afShaderTable[SH_LEAF_2_TWITCH_TIME] = m_afOscillationTimes[OSC_LEAF_2_TWITCH];

		// occlusion
		m_afShaderTable[SH_LEAF_1_LEEWARD_SCALAR] = m_sParams.m_asLeaf[0].m_fLeewardScalar;
		m_afShaderTable[SH_LEAF_2_LEEWARD_SCALAR] = m_sParams.m_asLeaf[1].m_fLeewardScalar;
	
		// frond ripple
		m_afShaderTable[SH_FROND_RIPPLE_TIME] = m_afOscillationTimes[OSC_FROND_RIPPLE];
		m_afShaderTable[SH_FROND_RIPPLE_DISTANCE] = Interpolate(m_sParams.m_afFrondRippleDistance[nBefore], m_sParams.m_afFrondRippleDistance[nAfter], fInterpolate);
		m_afShaderTable[SH_FROND_RIPPLE_TILE] = m_sParams.m_fFrondRippleTile;
		m_afShaderTable[SH_FROND_RIPPLE_LIGHTING_SCALAR] = m_sParams.m_fFrondRippleLightingScalar;

		// rolling
		m_afShaderTable[SH_ROLLING_NOISE_SIZE] = m_sParams.m_fRollingNoiseSize;
		m_afShaderTable[SH_ROLLING_NOISE_TWIST] = m_sParams.m_fRollingNoiseTwist;
		m_afShaderTable[SH_ROLLING_NOISE_TURBULENCE] = m_sParams.m_fRollingNoiseTurbulence;
		m_afShaderTable[SH_ROLLING_NOISE_PERIOD] = m_sParams.m_fRollingNoisePeriod;
		m_afShaderTable[SH_ROLLING_LEAF_RIPPLE_MIN] = m_sParams.m_fRollingLeafRippleMin;
		m_afShaderTable[SH_ROLLING_LEAF_TUMBLE_MIN] = m_sParams.m_fRollingLeafTumbleMin;
		m_afShaderTable[SH_ROLLING_BRANCH_FIELD_MIN] = m_sParams.m_fRollingBranchFieldMin;
		m_afShaderTable[SH_ROLLING_BRANCH_LIGHTING_ADJUST] = m_sParams.m_fRollingBranchLightingAdjust;
		m_afShaderTable[SH_ROLLING_BRANCH_VERTICAL_OFFSET] = m_sParams.m_fRollingBranchVerticalOffset;
		m_afShaderTable[SH_ROLLING_X] = m_afRollingOffset[0];
		m_afShaderTable[SH_ROLLING_Y] = m_afRollingOffset[1];
	}
	else
	{
		m_afShaderTable[SH_WIND_DIR_X] = 1.0f;
		m_afShaderTable[SH_WIND_DIR_Y] = 0.0f;
		m_afShaderTable[SH_WIND_DIR_Z] = 0.0f;

		// general
		m_afShaderTable[SH_GENERAL_STRENGTH] = 0.0f;

		// global
		m_afShaderTable[SH_GLOBAL_DISTANCE] = 0.0f;
		m_afShaderTable[SH_GLOBAL_TIME] = 0.0f;
		m_afShaderTable[SH_GLOBAL_HEIGHT] = 1.0f;
		m_afShaderTable[SH_GLOBAL_HEIGHT_EXPONENT] = 1.0f;
		m_afShaderTable[SH_GLOBAL_DIRECTION_ADHERENCE] = 0.0f;

		// branch
		m_afShaderTable[SH_BRANCH_1_DISTANCE] = 0.0f;
		m_afShaderTable[SH_BRANCH_1_TIME] = 0.0f;
		m_afShaderTable[SH_BRANCH_2_DISTANCE] = 0.0f;
		m_afShaderTable[SH_BRANCH_2_TIME] = 0.0f;

		m_afShaderTable[SH_WIND_ANCHOR_X] = 0.0f;
		m_afShaderTable[SH_WIND_ANCHOR_Y] = 0.0f;
		m_afShaderTable[SH_WIND_ANCHOR_Z] = 0.0f;
		m_afShaderTable[SH_BRANCH_1_TURBULENCE] = 0.0f;
		m_afShaderTable[SH_BRANCH_2_TURBULENCE] = 0.0f;
		m_afShaderTable[SH_BRANCH_1_DIRECTION_ADHERENCE] = 0.0f;
		m_afShaderTable[SH_BRANCH_2_DIRECTION_ADHERENCE] = 0.0f;
		m_afShaderTable[SH_BRANCH_1_TWITCH] = 0.0f;
		m_afShaderTable[SH_BRANCH_1_TWITCH_FREQ_SCALE] = 0.0f;
		m_afShaderTable[SH_BRANCH_2_TWITCH] = 0.0f;
		m_afShaderTable[SH_BRANCH_2_TWITCH_FREQ_SCALE] = 0.0f;
		m_afShaderTable[SH_BRANCH_1_WHIP] = 0.0f;
		m_afShaderTable[SH_BRANCH_2_WHIP] = 0.0f;

		// leaf ripple
		m_afShaderTable[SH_LEAF_1_RIPPLE_TIME] = 0.0f;
		m_afShaderTable[SH_LEAF_1_RIPPLE_DISTANCE] = 0.0f;
		m_afShaderTable[SH_LEAF_2_RIPPLE_TIME] = 0.0f;
		m_afShaderTable[SH_LEAF_2_RIPPLE_DISTANCE] = 0.0f;

		// leaf tumble
		m_afShaderTable[SH_LEAF_1_TUMBLE_TIME] = 0.0f;
		m_afShaderTable[SH_LEAF_1_TUMBLE_FLIP] = 0.0f;
		m_afShaderTable[SH_LEAF_1_TUMBLE_TWIST] = 0.0f;
		m_afShaderTable[SH_LEAF_1_TUMBLE_DIRECTION_ADHERENCE] = 0.0f;
		m_afShaderTable[SH_LEAF_2_TUMBLE_TIME] = 0.0f;
		m_afShaderTable[SH_LEAF_2_TUMBLE_FLIP] = 0.0f;
		m_afShaderTable[SH_LEAF_2_TUMBLE_TWIST] = 0.0f;
		m_afShaderTable[SH_LEAF_2_TUMBLE_DIRECTION_ADHERENCE] = 0.0f;

		// twitch
		m_afShaderTable[SH_LEAF_1_TWITCH_THROW] = 0.0f;
		m_afShaderTable[SH_LEAF_1_TWITCH_SHARPNESS] = 0.0f;
		m_afShaderTable[SH_LEAF_1_TWITCH_TIME] = 0.0f;

		m_afShaderTable[SH_LEAF_2_TWITCH_THROW] = 0.0f;
		m_afShaderTable[SH_LEAF_2_TWITCH_SHARPNESS] = 0.0f;
		m_afShaderTable[SH_LEAF_2_TWITCH_TIME] = 0.0f;

		// occlusion
		m_afShaderTable[SH_LEAF_1_LEEWARD_SCALAR] = 1.0f;
		m_afShaderTable[SH_LEAF_2_LEEWARD_SCALAR] = 1.0f;

		// frond ripple
		m_afShaderTable[SH_FROND_RIPPLE_TIME] = 0.0f;
		m_afShaderTable[SH_FROND_RIPPLE_DISTANCE] = 0.0f;
		m_afShaderTable[SH_FROND_RIPPLE_TILE] = 0.0f;
		m_afShaderTable[SH_FROND_RIPPLE_LIGHTING_SCALAR] = 1.0f;

		// rolling
		m_afShaderTable[SH_ROLLING_NOISE_SIZE] = m_sParams.m_fRollingNoiseSize;
		m_afShaderTable[SH_ROLLING_NOISE_TWIST] = m_sParams.m_fRollingNoiseTwist;
		m_afShaderTable[SH_ROLLING_NOISE_TURBULENCE] = m_sParams.m_fRollingNoiseTurbulence;
		m_afShaderTable[SH_ROLLING_NOISE_PERIOD] = m_sParams.m_fRollingNoisePeriod;
		m_afShaderTable[SH_ROLLING_LEAF_RIPPLE_MIN] = m_sParams.m_fRollingLeafRippleMin;
		m_afShaderTable[SH_ROLLING_LEAF_TUMBLE_MIN] = m_sParams.m_fRollingLeafTumbleMin;
		m_afShaderTable[SH_ROLLING_BRANCH_FIELD_MIN] = m_sParams.m_fRollingBranchFieldMin;
		m_afShaderTable[SH_ROLLING_BRANCH_LIGHTING_ADJUST] = m_sParams.m_fRollingBranchLightingAdjust;
		m_afShaderTable[SH_ROLLING_BRANCH_VERTICAL_OFFSET] = m_sParams.m_fRollingBranchVerticalOffset;
		m_afShaderTable[SH_ROLLING_X] = 0.0f;
		m_afShaderTable[SH_ROLLING_Y] = 0.0f;
	}
}


/////////////////////////////////////////////////////////////////////////////
// CWind::SetStrength
/// Sets the desired strength (\a fStrength).  The CWind::Advance( ) function will make the actual strength get there smoothly based on the strength response time.

void CWind::SetStrength(st_float32 fStrength)
{
	if (fStrength != m_fStrength)
	{
		m_fStrengthChangeStartTime = m_fLastTime;

		st_float32 fAmount = Interpolate(m_sParams.m_fStrengthResponse * 0.5f, m_sParams.m_fStrengthResponse, fabs(fStrength - m_fStrength));
		m_fStrengthChangeEndTime = m_fStrengthChangeStartTime + fAmount;
		m_fStrengthAtStart = m_fStrength;
		m_fStrengthTarget = fStrength;
	}
}


//////////////////////////////////////////////////////////////////////////
// CWind::Scale

void CWind::Scale(st_float32 fScalar)
{
	ScaleWindCurve(m_sParams.m_afGlobalDistance, fScalar);
	ScaleWindCurve(m_sParams.m_afFrondRippleDistance, fScalar);

	for (st_int32 i = 0; i < c_nNumBranchLevels; ++i)
		ScaleWindCurve(m_sParams.m_asBranch[i].m_afDistance, fScalar);

	for (st_int32 i = 0; i < c_nNumLeafGroups; ++i)
	{
		ScaleWindCurve(m_sParams.m_asLeaf[i].m_afRippleDistance, fScalar);
		if (fScalar != 0.0f)
			m_sParams.m_asLeaf[i].m_fRollSeparation /= fScalar;
	}

	m_sParams.m_fGlobalHeight *= fScalar;
	m_sParams.m_fRollingBranchLightingAdjust /= fScalar;

	m_fMaxBranchLevel1Length *= fScalar;
	m_afBranchWindAnchor[0] *= fScalar;
	m_afBranchWindAnchor[1] *= fScalar;
	m_afBranchWindAnchor[2] *= fScalar;
}


/////////////////////////////////////////////////////////////////////////////
// CWind::SetDirection

void CWind::SetDirection(const Vec3& vDir)
{
	if (vDir.x != m_afDirection[0] ||
		vDir.y != m_afDirection[1] ||
		vDir.z != m_afDirection[2])
	{
		m_afDirectionTarget[0] = vDir.x;
		m_afDirectionTarget[1] = vDir.y;
		m_afDirectionTarget[2] = vDir.z;

		st_float32 fDot = m_afDirection[0] * vDir.x + m_afDirection[1] * vDir.y + m_afDirection[2] * vDir.z;
		st_float32 fDistanceToTravel = 1.0f - ((fDot + 1.0f) * 0.5f);

		m_fDirectionChangeStartTime = m_fLastTime;
		st_float32 fAmount = Interpolate(m_sParams.m_fDirectionResponse * 0.5f, m_sParams.m_fDirectionResponse, fDistanceToTravel);
		m_fDirectionChangeEndTime = m_fDirectionChangeStartTime + fAmount;

		m_afDirectionAtStart[0] = m_afDirection[0];
		m_afDirectionAtStart[1] = m_afDirection[1];
		m_afDirectionAtStart[2] = m_afDirection[2];

		m_afDirectionMidTarget[0] = (m_afDirectionAtStart[0] + m_afDirectionTarget[0]) * 0.5f;
		m_afDirectionMidTarget[1] = (m_afDirectionAtStart[1] + m_afDirectionTarget[1]) * 0.5f;
		m_afDirectionMidTarget[2] = (m_afDirectionAtStart[2] + m_afDirectionTarget[2]) * 0.5f;
		Normalize(m_afDirectionMidTarget);
	}
}


/////////////////////////////////////////////////////////////////////////////
// CWind::SetInitDirection

void CWind::SetInitDirection(const Vec3& vDir)
{
	m_afDirectionTarget[0] = m_afDirectionAtStart[0] = m_afDirectionMidTarget[0] = m_afDirection[0] = vDir.x;
	m_afDirectionTarget[1] = m_afDirectionAtStart[1] = m_afDirectionMidTarget[1] = m_afDirection[1] = vDir.y;
	m_afDirectionTarget[2] = m_afDirectionAtStart[2] = m_afDirectionMidTarget[2] = m_afDirection[2] = vDir.z;
}


/////////////////////////////////////////////////////////////////////////////
// CWind::Gust
/// Advances the gust parameter and randomly gusts if it can (e.g., the gust is not dying off or rising) based on the gust frequency.

void CWind::Gust(st_float32 fTime)
{
	const st_float32 c_fGustAdjust = 0.01f;
	if (fTime > m_fGustFallTarget || (fTime < m_fGustFallStart && fTime > m_fGustRiseTarget))
	{
		// it is legal to gust (you can't gust on the way out of a gust to prevent jerks)
		if (RandomFloat(0.0f, m_fElapsedTime) < (m_fElapsedTime * m_sParams.m_fGustFrequency * c_fGustAdjust))
		{
			// we got one, set it up
			m_fGustStart = fTime;
			m_fGustAtStart = m_fGust;
			m_fGustTarget = RandomFloat(m_sParams.m_fGustStrengthMin, m_sParams.m_fGustStrengthMax);
			if (m_fGustTarget > 1.0f - m_fStrength)
				m_fGustTarget = 1.0f - m_fStrength;

			st_float32 fAmount = Interpolate(m_sParams.m_fStrengthResponse * 0.5f, m_sParams.m_fStrengthResponse, fabs(m_fGustTarget - m_fStrength));
			if (m_fGustTarget > m_fGust)
				m_fGustRiseTarget = fTime + m_sParams.m_fGustRiseScalar * RandomFloat(fAmount * 1.0f, fAmount * 2.0f);
			else
				m_fGustRiseTarget = fTime + m_sParams.m_fGustFallScalar * RandomFloat(fAmount, fAmount * 2.0f);

			m_fGustFallStart = m_fGustRiseTarget + RandomFloat(m_sParams.m_fGustDurationMin, m_sParams.m_fGustDurationMax);
			m_fGustFallTarget = m_fGustFallStart + m_sParams.m_fGustFallScalar * RandomFloat(fAmount * 2.0f, fAmount * 3.0f);
		}
	}

	if (fTime < m_fGustRiseTarget)
	{
		// s-curve toward the target
		m_fGust = Interpolate(m_fGustAtStart, m_fGustTarget, LinearSigmoid((fTime - m_fGustStart) / (m_fGustRiseTarget - m_fGustStart), 0.0f));
	}
	else if (fTime > m_fGustFallStart && m_fGustFallTarget > 0.0f && m_fGustFallTarget > m_fGustFallStart)
	{
		// s-curve back to zero
		m_fGust = Interpolate(m_fGustTarget, 0.0f, LinearSigmoid((fTime - m_fGustFallStart) / (m_fGustFallTarget - m_fGustFallStart), 0.5f));
	}

	m_fGust = st_max(0.0f, st_min(1.0f, m_fGust));
}


/////////////////////////////////////////////////////////////////////////////
// CWind::ComputeWindAnchor
/// Computes the wind anchor position based on current wind conditions.

void CWind::ComputeWindAnchor(st_float32* pPos)
{
	Vec3 vOffset = CCoordSys::UpAxis( ) * m_sParams.m_fAnchorOffset;
	st_float32 afDirection[3] = { m_afDirection[0] + vOffset.x, m_afDirection[1] + vOffset.y, m_afDirection[2] + vOffset.z };
	Normalize(afDirection);

	pPos[0] = m_afBranchWindAnchor[0] + afDirection[0] * m_fMaxBranchLevel1Length * m_sParams.m_fAnchorDistanceScale;
	pPos[1] = m_afBranchWindAnchor[1] + afDirection[1] * m_fMaxBranchLevel1Length * m_sParams.m_fAnchorDistanceScale;
	pPos[2] = m_afBranchWindAnchor[2] + afDirection[2] * m_fMaxBranchLevel1Length * m_sParams.m_fAnchorDistanceScale;
}


///////////////////////////////////////////////////////////////////////
// CWind::RandomFloat

inline st_float32 CWind::RandomFloat(st_float32 fMin, st_float32 fMax) const
{
	static CRandom s_cDice;

	return s_cDice.GetFloat(fMin, fMax);
}


///////////////////////////////////////////////////////////////////////
// CWind::LinearSigmoid
/// Converts an input value in the range [0.0, 1.0] (\a fInput) to an s-curve.  The parameter \a fLinearness flattens out the s-curve where 0.0 = s-curve and 1.0 = linear.

inline st_float32 CWind::LinearSigmoid(st_float32 fInput, st_float32 fLinearness)
{
	st_float32 fSigmoid = 1.0f / (1.0f + exp(-Interpolate(-6.0f, 6.0f, fInput)));

	return Interpolate(fSigmoid, fInput, fLinearness);
}


