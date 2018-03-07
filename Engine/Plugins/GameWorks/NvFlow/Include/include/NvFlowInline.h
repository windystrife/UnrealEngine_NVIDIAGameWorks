/*
* Copyright (c) 2014-2017, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "NvFlow.h"

/**
 * Allows the application to request a default grid description from Flow.
 *
 * @param[out] desc The description for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowGridDescDefaultsInline(NvFlowGridDesc* desc)
{
	if (desc)
	{
		desc->initialLocation = { 0.f, 0.f, 0.f };
		desc->halfSize = { 8.f, 8.f, 8.f };

		desc->virtualDim = { 512u, 512u, 512u };
		desc->densityMultiRes = eNvFlowMultiRes2x2x2;

		desc->residentScale = 0.125f * 0.075f;
		desc->coarseResidentScaleFactor = 1.5f;

		desc->enableVTR = false;
		desc->lowLatencyMapping = false;
	}
}

/**
 * Allows the application to request a default grid reset description from Flow.
 *
 * @param[out] desc The description for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowGridResetDescDefaultsInline(NvFlowGridResetDesc* desc)
{
	if (desc)
	{
		desc->initialLocation = { 0.f, 0.f, 0.f };
		desc->halfSize = { 8.f, 8.f, 8.f };
	}
}

/**
 * Allows the application to request default grid parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowGridParamsDefaultsInline(NvFlowGridParams* params)
{
	if (params)
	{
		params->gravity = { 0.f, -1.f, 0.f };

		params->singlePassAdvection = true;

		params->pressureLegacyMode = false;

		params->bigEffectMode = false;
		params->bigEffectPredictTime = 0.1f;

		params->debugVisFlags = eNvFlowGridDebugVisBlocks;
	}
}

/**
 * Allows the application to request default grid material parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowGridMaterialParamsDefaultsInline(NvFlowGridMaterialParams* params)
{
	if (params)
	{
		params->velocity.damping = 0.1f;
		params->velocity.fade = 0.1f;
		params->velocity.macCormackBlendFactor = 0.5f;
		params->velocity.macCormackBlendThreshold = 0.001f;
		params->velocity.allocWeight = 0.f;
		params->velocity.allocThreshold = 0.f;

		params->smoke.damping = 0.1f;
		params->smoke.fade = 0.1f;
		params->smoke.macCormackBlendFactor = 0.5f;
		params->smoke.macCormackBlendThreshold = 0.001f;
		params->smoke.allocWeight = 0.f;
		params->smoke.allocThreshold = 0.f;

		params->temperature.damping = 0.0f;
		params->temperature.fade = 0.0f;
		params->temperature.macCormackBlendFactor = 0.5f;
		params->temperature.macCormackBlendThreshold = 0.001f;
		params->temperature.allocWeight = 1.f;
		params->temperature.allocThreshold = 0.05f;

		params->fuel.damping = 0.002f;
		params->fuel.fade = 0.002f;
		params->fuel.macCormackBlendFactor = 0.5f;
		params->fuel.macCormackBlendThreshold = 0.001f;
		params->fuel.allocWeight = 0.f;
		params->fuel.allocThreshold = 0.f;

		params->vorticityStrength = 9.f;
		params->vorticityVelocityMask = 1.f;
		params->vorticityTemperatureMask = 0.f;
		params->vorticitySmokeMask = 0.f;
		params->vorticityFuelMask = 0.f;
		params->vorticityConstantMask = 0.f;

		params->ignitionTemp = 0.05f;
		params->burnPerTemp = 4.f;
		params->fuelPerBurn = 1.f;
		params->tempPerBurn = 5.f;
		params->smokePerBurn = 3.f;
		params->divergencePerBurn = 4.f;
		params->buoyancyPerTemp = 4.f;
		params->coolingRate = 1.5f;
	}
}

/**
 * Allows the application to request a default signed distance field object description from Flow.
 *
 * @param[out] desc The description for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowShapeSDFDescDefaultsInline(NvFlowShapeSDFDesc* desc)
{
	if (desc)
	{
		desc->resolution = { 16u, 16u, 16u };
	}
}

/**
 * Allows the application to request default emit parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowGridEmitParamsDefaultsInline(NvFlowGridEmitParams* params)
{
	if (params)
	{
		params->shapeRangeOffset = 0u;
		params->shapeRangeSize = 1u;
		params->shapeType = eNvFlowShapeTypeSphere;
		params->shapeDistScale = 1.f;

		params->bounds = {
			{ 1.f,0.f,0.f,0.f },
			{ 0.f,1.f,0.f,0.f },
			{ 0.f,0.f,1.f,0.f },
			{ 0.f,0.f,0.f,1.f }
		};
		params->localToWorld = {
			{ 1.f,0.f,0.f,0.f },
			{ 0.f,1.f,0.f,0.f },
			{ 0.f,0.f,1.f,0.f },
			{ 0.f,0.f,0.f,1.f }
		};
		params->centerOfMass = { 0.f, 0.f, 0.f };

		params->deltaTime = 0.f;

		params->emitMaterialIndex = ~0u;
		params->emitMode = 0u;

		params->allocationScale = { 1.f, 1.f, 1.f };
		params->allocationPredict = 0.125f;
		params->predictVelocity = { 0.f, 0.f, 0.f };
		params->predictVelocityWeight = 0.f;

		params->minActiveDist = -1.f;
		params->maxActiveDist = 0.f;
		params->minEdgeDist = 0.f;
		params->maxEdgeDist = 0.1f;
		params->slipThickness = 0.f;
		params->slipFactor = 0.f;

		params->velocityLinear = { 0.f, 0.f, 0.f };
		params->velocityAngular = { 0.f, 0.f, 0.f };
		params->velocityCoupleRate = { 0.5f, 0.5f, 0.5f };

		params->smoke = 0.5f;
		params->smokeCoupleRate = 0.5f;

		params->temperature = 2.f;
		params->temperatureCoupleRate = 0.5f;

		params->fuel = 1.f;
		params->fuelCoupleRate = 0.5f;
		params->fuelReleaseTemp = 0.1f;
		params->fuelRelease = 0.f;
	}
}

/**
 * Allows the application to request default volume render material parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowRenderMaterialParamsDefaultsInline(NvFlowRenderMaterialParams* params)
{
	if (params)
	{
		params->material = NvFlowGridMaterialHandle{ nullptr, 0u };

		params->alphaScale = 0.1f;
		params->additiveFactor = 0.f;

		params->colorMapCompMask = { 1.f, 0.f, 0.f, 0.f };
		params->alphaCompMask = { 0.f, 0.f, 0.f, 1.f };
		params->intensityCompMask = { 0.f, 0.f, 0.f, 0.f };

		params->colorMapMinX = 0.f;
		params->colorMapMaxX = 1.f;
		params->alphaBias = 0.f;
		params->intensityBias = 1.f;
	}
}

/**
 * Allows the application to request default volume render parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowVolumeRenderParamsDefaultsInline(NvFlowVolumeRenderParams* params)
{
	if (params)
	{
		params->projectionMatrix = {
			{ 1.f,0.f,0.f,0.f },
			{ 0.f,1.f,0.f,0.f },
			{ 0.f,0.f,1.f,0.f },
			{ 0.f,0.f,0.f,1.f }
		};
		params->viewMatrix = {
			{ 1.f,0.f,0.f,0.f },
			{ 0.f,1.f,0.f,0.f },
			{ 0.f,0.f,1.f,0.f },
			{ 0.f,0.f,0.f,1.f }
		};

		params->depthStencilView = nullptr;
		params->renderTargetView = nullptr;

		params->materialPool = nullptr;

		params->renderMode = eNvFlowVolumeRenderMode_colormap;
		params->renderChannel = eNvFlowGridTextureChannelDensity;

		params->debugMode = false;

		params->downsampleFactor = eNvFlowVolumeRenderDownsample2x2;
		params->screenPercentage = 1.f;
		params->multiResRayMarch = eNvFlowMultiResRayMarchDisabled;
		params->multiResSamplingScale = 2.f;

		params->smoothColorUpsample = false;

		params->preColorCompositeOnly = false;
		params->colorCompositeOnly = false;
		params->generateDepth = false;
		params->generateDepthDebugMode = false;
		params->depthAlphaThreshold = 0.9f;
		params->depthIntensityThreshold = 4.f;

		params->multiRes.enabled = false;
		params->multiRes.centerWidth = 0.4f;
		params->multiRes.centerHeight = 0.4f;
		params->multiRes.centerX = 0.5f;
		params->multiRes.centerY = 0.5f;
		params->multiRes.densityScaleX[0] = 0.5f;
		params->multiRes.densityScaleX[1] = 1.f;
		params->multiRes.densityScaleX[2] = 0.5f;
		params->multiRes.densityScaleY[0] = 0.5f;
		params->multiRes.densityScaleY[1] = 1.f;
		params->multiRes.densityScaleY[2] = 0.5f;
		params->multiRes.viewport.topLeftX = 0.f;
		params->multiRes.viewport.topLeftY = 0.f;
		params->multiRes.viewport.width = 0.f;
		params->multiRes.viewport.height = 0.f;
		params->multiRes.nonMultiResWidth = 0.f;
		params->multiRes.nonMultiResHeight = 0.f;

		params->lensMatchedShading.enabled = false;
		params->lensMatchedShading.warpLeft = 0.471f;
		params->lensMatchedShading.warpRight = 0.471f;
		params->lensMatchedShading.warpUp = 0.471f;
		params->lensMatchedShading.warpDown = 0.471f;
		params->lensMatchedShading.sizeLeft = 552.1f;
		params->lensMatchedShading.sizeRight = 735.9f;
		params->lensMatchedShading.sizeUp = 847.f;
		params->lensMatchedShading.sizeDown = 584.4f;
		params->lensMatchedShading.viewport.topLeftX = 0.f;
		params->lensMatchedShading.viewport.topLeftY = 0.f;
		params->lensMatchedShading.viewport.width = 0.f;
		params->lensMatchedShading.viewport.height = 0.f;
		params->lensMatchedShading.nonLMSWidth = 0.f;
		params->lensMatchedShading.nonLMSHeight = 0.f;
	}
}

/**
 * Allows the application to request default cross section parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowCrossSectionParamsDefaultsInline(NvFlowCrossSectionParams* params)
{
	if (params)
	{
		params->gridExport = nullptr;
		params->gridExportDebugVis = nullptr;

		params->projectionMatrix = {
			{ 1.f,0.f,0.f,0.f },
			{ 0.f,1.f,0.f,0.f },
			{ 0.f,0.f,1.f,0.f },
			{ 0.f,0.f,0.f,1.f }
		};
		params->viewMatrix = {
			{ 1.f,0.f,0.f,0.f },
			{ 0.f,1.f,0.f,0.f },
			{ 0.f,0.f,1.f,0.f },
			{ 0.f,0.f,0.f,1.f }
		};

		params->depthStencilView = nullptr;
		params->renderTargetView = nullptr;

		params->materialPool = nullptr;

		params->renderMode = eNvFlowVolumeRenderMode_colormap;
		params->renderChannel = eNvFlowGridTextureChannelDensity;

		params->crossSectionAxis = 0u;
		params->crossSectionPosition = { 0.f, 0.f, 0.f };
		params->crossSectionScale = 2.f;

		params->intensityScale = 1.f;

		params->pointFilter = false;

		params->velocityVectors = true;
		params->velocityScale = 1.f;
		params->vectorLengthScale = 1.f;

		params->outlineCells = false;

		params->fullscreen = false;

		params->lineColor = { 1.f, 1.f, 1.f, 1.f };
		params->backgroundColor = { 0.0f, 0.0f, 0.0f, 1.f };
		params->cellColor = { 1.f, 1.f, 1.f, 1.f };
	}
}

/**
 * Allows the application to request a default Flow device description from Flow.
 *
 * @param[out] desc The description for Flow to fill out.
 */
NV_FLOW_API_INLINE void NvFlowDeviceDescDefaultsInline(NvFlowDeviceDesc* desc)
{
	if (desc)
	{
		desc->mode = eNvFlowDeviceModeUnique;
		desc->autoSelectDevice = true;
		desc->adapterIdx = 1;
	}
}