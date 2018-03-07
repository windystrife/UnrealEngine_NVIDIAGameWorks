//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononMaterial.h"

static TMap<EPhononMaterial, IPLMaterial> GetMaterialPresets()
{
	TMap<EPhononMaterial, IPLMaterial> Presets;

	IPLMaterial mat;
	mat.scattering = 0.05f;

	mat.lowFreqAbsorption = 0.10f;
	mat.midFreqAbsorption = 0.20f;
	mat.highFreqAbsorption = 0.30f;
	mat.lowFreqTransmission = 0.100f;
	mat.midFreqTransmission = 0.050f;
	mat.highFreqTransmission = 0.030f;
	Presets.Add(EPhononMaterial::GENERIC, mat);

	mat.lowFreqAbsorption = 0.03f;
	mat.midFreqAbsorption = 0.04f;
	mat.highFreqAbsorption = 0.07f;
	mat.lowFreqTransmission = 0.015f;
	mat.midFreqTransmission = 0.015f;
	mat.highFreqTransmission = 0.015f;
	Presets.Add(EPhononMaterial::BRICK, mat);

	mat.lowFreqAbsorption = 0.05f;
	mat.midFreqAbsorption = 0.07f;
	mat.highFreqAbsorption = 0.08f;
	mat.lowFreqTransmission = 0.015f;
	mat.midFreqTransmission = 0.002f;
	mat.highFreqTransmission = 0.001f;
	Presets.Add(EPhononMaterial::CONCRETE, mat);

	mat.lowFreqAbsorption = 0.01f;
	mat.midFreqAbsorption = 0.02f;
	mat.highFreqAbsorption = 0.02f;
	mat.lowFreqTransmission = 0.060f;
	mat.midFreqTransmission = 0.044f;
	mat.highFreqTransmission = 0.011f;
	Presets.Add(EPhononMaterial::CERAMIC, mat);

	mat.lowFreqAbsorption = 0.60f;
	mat.midFreqAbsorption = 0.70f;
	mat.highFreqAbsorption = 0.80f;
	mat.lowFreqTransmission = 0.031f;
	mat.midFreqTransmission = 0.012f;
	mat.highFreqTransmission = 0.008f;
	Presets.Add(EPhononMaterial::GRAVEL, mat);

	mat.lowFreqAbsorption = 0.24f;
	mat.midFreqAbsorption = 0.69f;
	mat.highFreqAbsorption = 0.73f;
	mat.lowFreqTransmission = 0.020f;
	mat.midFreqTransmission = 0.005f;
	mat.highFreqTransmission = 0.003f;
	Presets.Add(EPhononMaterial::CARPET, mat);

	mat.lowFreqAbsorption = 0.06f;
	mat.midFreqAbsorption = 0.03f;
	mat.highFreqAbsorption = 0.02f;
	mat.lowFreqTransmission = 0.060f;
	mat.midFreqTransmission = 0.044f;
	mat.highFreqTransmission = 0.011f;
	Presets.Add(EPhononMaterial::GLASS, mat);

	mat.lowFreqAbsorption = 0.12f;
	mat.midFreqAbsorption = 0.06f;
	mat.highFreqAbsorption = 0.04f;
	mat.lowFreqTransmission = 0.056f;
	mat.midFreqTransmission = 0.056f;
	mat.highFreqTransmission = 0.004f;
	Presets.Add(EPhononMaterial::PLASTER, mat);

	mat.lowFreqAbsorption = 0.11f;
	mat.midFreqAbsorption = 0.07f;
	mat.highFreqAbsorption = 0.06f;
	mat.lowFreqTransmission = 0.070f;
	mat.midFreqTransmission = 0.014f;
	mat.highFreqTransmission = 0.005f;
	Presets.Add(EPhononMaterial::WOOD, mat);

	mat.lowFreqAbsorption = 0.20f;
	mat.midFreqAbsorption = 0.07f;
	mat.highFreqAbsorption = 0.06f;
	mat.lowFreqTransmission = 0.200f;
	mat.midFreqTransmission = 0.025f;
	mat.highFreqTransmission = 0.010f;
	Presets.Add(EPhononMaterial::METAL, mat);

	mat.lowFreqAbsorption = 0.13f;
	mat.midFreqAbsorption = 0.20f;
	mat.highFreqAbsorption = 0.24f;
	mat.lowFreqTransmission = 0.015f;
	mat.midFreqTransmission = 0.002f;
	mat.highFreqTransmission = 0.001f;
	Presets.Add(EPhononMaterial::ROCK, mat);

	mat.lowFreqAbsorption = 0.00f;
	mat.midFreqAbsorption = 0.00f;
	mat.highFreqAbsorption = 0.00f;
	mat.lowFreqTransmission = 0.00f;
	mat.midFreqTransmission = 0.00f;
	mat.highFreqTransmission = 0.00f;
	Presets.Add(EPhononMaterial::CUSTOM, mat);

	return Presets;
}

TMap<EPhononMaterial, IPLMaterial> SteamAudio::MaterialPresets = GetMaterialPresets();
