#include "OculusAudio.h"

TAudioSpatializationPtr FOculusSpatializationPluginFactory::CreateNewSpatializationPlugin(FAudioDevice* OwningDevice)
{
	if (OwningDevice->IsAudioMixerEnabled())
	{
		return TAudioSpatializationPtr(new OculusAudioSpatializationAudioMixer());
	}
	else
	{
		return TAudioSpatializationPtr(new OculusAudioLegacySpatialization());
	}
}
