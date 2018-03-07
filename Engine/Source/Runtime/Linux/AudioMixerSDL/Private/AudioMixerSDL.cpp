#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformSDL.h"


class FAudioMixerModuleSDL : public IAudioDeviceModule
{
public:
	virtual bool IsAudioMixerModule() const override { return true; }

	virtual void StartupModule() override
	{
#if PLATFORM_WINDOWS
		FString DllPath = FPaths::EngineDir() / "Binaries/ThirdParty/SDL2/Win64/SDL2.dll";
		FString SDL2_Dll = DllPath + "SDL2.dll";
		FPlatformProcess::GetDllHandle(*DllPath);
#endif
	}

	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new Audio::FMixerDevice(new Audio::FMixerPlatformSDL());
	}
};

IMPLEMENT_MODULE(FAudioMixerModuleSDL, AudioMixerSDL);
