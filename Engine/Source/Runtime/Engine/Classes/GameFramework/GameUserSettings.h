// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "GenericPlatform/GenericWindow.h"
#include "Scalability.h"
#include "GameUserSettings.generated.h"

#if !CPP      //noexport class

/** Supported windowing modes (mirrored from GenericWindow.h) */
UENUM(BlueprintType)
namespace EWindowMode
{
	enum Type
	{
		/** The window is in true fullscreen mode */
		Fullscreen,
		/** The window has no border and takes up the entire area of the screen */
		WindowedFullscreen,
		/** The window has a border and may not take up the entire screen area */
		Windowed,
	};
}

#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameUserSettingsUINeedsUpdate);

/**
 * Stores user settings for a game (for example graphics and sound settings), with the ability to save and load to and from a file.
 */
UCLASS(config=GameUserSettings, configdonotcheckdefaults)
class ENGINE_API UGameUserSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Applies all current user settings to the game and saves to permanent storage (e.g. file), optionally checking for command line overrides. */
	UFUNCTION(BlueprintCallable, Category=Settings, meta=(bCheckForCommandLineOverrides=true))
	virtual void ApplySettings(bool bCheckForCommandLineOverrides);
	
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void ApplyNonResolutionSettings();

	UFUNCTION(BlueprintCallable, Category=Settings)
	void ApplyResolutionSettings(bool bCheckForCommandLineOverrides);

	/** Returns the user setting for game screen resolution, in pixels. */
	UFUNCTION(BlueprintPure, Category=Settings)
	FIntPoint GetScreenResolution() const;

	/** Returns the last confirmed user setting for game screen resolution, in pixels. */
	UFUNCTION(BlueprintPure, Category=Settings)
	FIntPoint GetLastConfirmedScreenResolution() const;

	/** Returns user's desktop resolution, in pixels. */
	UFUNCTION(BlueprintPure, Category = Settings)
	FIntPoint GetDesktopResolution() const;

	/** Sets the user setting for game screen resolution, in pixels. */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetScreenResolution(FIntPoint Resolution);

	/** Returns the user setting for game window fullscreen mode. */
	UFUNCTION(BlueprintPure, Category=Settings)
	EWindowMode::Type GetFullscreenMode() const;

	/** Returns the last confirmed user setting for game window fullscreen mode. */
	UFUNCTION(BlueprintPure, Category=Settings)
	EWindowMode::Type GetLastConfirmedFullscreenMode() const;

	/** Sets the user setting for the game window fullscreen mode. See UGameUserSettings::FullscreenMode. */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetFullscreenMode(EWindowMode::Type InFullscreenMode);

	/** Returns the user setting for game window fullscreen mode. */
	UFUNCTION(BlueprintPure, Category = Settings)
	EWindowMode::Type GetPreferredFullscreenMode() const;

	/** Sets the user setting for vsync. See UGameUserSettings::bUseVSync. */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetVSyncEnabled(bool bEnable);

	/** Returns the user setting for vsync. */
	UFUNCTION(BlueprintPure, Category=Settings)
	bool IsVSyncEnabled() const;

	/** Checks if the Screen Resolution user setting is different from current */
	UFUNCTION(BlueprintPure, Category=Settings)
	bool IsScreenResolutionDirty() const;

	/** Checks if the FullscreenMode user setting is different from current */
	UFUNCTION(BlueprintPure, Category=Settings)
	bool IsFullscreenModeDirty() const;

	/** Checks if the vsync user setting is different from current system setting */
	UFUNCTION(BlueprintPure, Category=Settings)
	bool IsVSyncDirty() const;

	/** Mark current video mode settings (fullscreenmode/resolution) as being confirmed by the user */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void ConfirmVideoMode();

	/** Revert video mode (fullscreenmode/resolution) back to the last user confirmed values */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void RevertVideoMode();

	/** Set scalability settings to sensible fallback values, for use when the benchmark fails or potentially causes a crash */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetBenchmarkFallbackValues();

	/** Sets the user's audio quality level setting */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetAudioQualityLevel(int32 QualityLevel);

	/** Returns the user's audio quality level setting */
	UFUNCTION(BlueprintPure, Category=Settings)
	int32 GetAudioQualityLevel() const { return AudioQualityLevel; }

	/** Sets the user's frame rate limit (0 will disable frame rate limiting) */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetFrameRateLimit(float NewLimit);

	/** Gets the user's frame rate limit (0 indiciates the frame rate limit is disabled) */
	UFUNCTION(BlueprintPure, Category=Settings)
	float GetFrameRateLimit() const;

	// Changes all scalability settings at once based on a single overall quality level
	// @param Value 0:low, 1:medium, 2:high, 3:epic
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void SetOverallScalabilityLevel(int32 Value);

	// Returns the overall scalability level (can return -1 if the settings are custom)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetOverallScalabilityLevel() const;

	// Returns the current resolution scale and the range
	DEPRECATED(4.12, "Please call GetResolutionScaleInformationEx")
	UFUNCTION(BlueprintCallable, Category = Settings, meta = (DeprecatedFunction, DisplayName = "GetResolutionScaleInformation_Deprecated"))
	void GetResolutionScaleInformation(float& CurrentScaleNormalized, int32& CurrentScaleValue, int32& MinScaleValue, int32& MaxScaleValue) const;

	// Returns the current resolution scale and the range
	UFUNCTION(BlueprintCallable, Category=Settings, meta=(DisplayName="GetResolutionScaleInformation"))
	void GetResolutionScaleInformationEx(float& CurrentScaleNormalized, float& CurrentScaleValue, float& MinScaleValue, float& MaxScaleValue) const;

	// Sets the current resolution scale
	DEPRECATED(4.12, "Please call SetResolutionScaleValueEx")
	UFUNCTION(BlueprintCallable, Category=Settings, meta=(DeprecatedFunction, DisplayName="SetResolutionScaleValue_Deprecated"))
	void SetResolutionScaleValue(int32 NewScaleValue);

	// Sets the current resolution scale
	UFUNCTION(BlueprintCallable, Category=Settings, meta=(DisplayName="SetResolutionScaleValue"))
	void SetResolutionScaleValueEx(float NewScaleValue);

	// Sets the current resolution scale as a normalized 0..1 value between MinScaleValue and MaxScaleValue
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetResolutionScaleNormalized(float NewScaleNormalized);

	// Sets the view distance quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetViewDistanceQuality(int32 Value);

	// Returns the view distance quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetViewDistanceQuality() const;

	// Sets the shadow quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetShadowQuality(int32 Value);

	// Returns the shadow quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetShadowQuality() const;

	// Sets the anti-aliasing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetAntiAliasingQuality(int32 Value);

	// Returns the anti-aliasing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetAntiAliasingQuality() const;

	// Sets the texture quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetTextureQuality(int32 Value);

	// Returns the texture quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetTextureQuality() const;

	// Sets the visual effects quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetVisualEffectQuality(int32 Value);

	// Returns the visual effects quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetVisualEffectQuality() const;

	// Sets the post-processing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetPostProcessingQuality(int32 Value);

	// Returns the post-processing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetPostProcessingQuality() const;
	
	// Sets the post-processing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetFoliageQuality(int32 Value);

	// Returns the post-processing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetFoliageQuality() const;

	/** Checks if any user settings is different from current */
	UFUNCTION(BlueprintPure, Category=Settings)
	virtual bool IsDirty() const;

	/** Validates and resets bad user settings to default. Deletes stale user settings file if necessary. */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void ValidateSettings();

	/** Loads the user settings from persistent storage */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void LoadSettings(bool bForceReload = false);

	/** Save the user settings to persistent storage (automatically happens as part of ApplySettings) */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void SaveSettings();

	/** This function resets all settings to the current system settings */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void ResetToCurrentSettings();

	virtual void SetWindowPosition(int32 WindowPosX, int32 WindowPosY);

	virtual FIntPoint GetWindowPosition();
	
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void SetToDefaults();

	/** Gets the desired resolution quality based on DesiredScreenWidth/Height and the current screen resolution */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual float GetDefaultResolutionScale();

	/** Gets the recommended resolution quality based on LastRecommendedScreenWidth/Height and the current screen resolution */
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual float GetRecommendedResolutionScale();

	/** Loads the resolution settings before is object is available */
	static void PreloadResolutionSettings();

	/** @return The default resolution when no resolution is set */
	UFUNCTION(BlueprintCallable, Category=Settings)
	static FIntPoint GetDefaultResolution();

	/** @return The default window position when no position is set */
	UFUNCTION(BlueprintCallable, Category=Settings)
	static FIntPoint GetDefaultWindowPosition();

	/** @return The default window mode when no mode is set */
	UFUNCTION(BlueprintCallable, Category=Settings)
	static EWindowMode::Type GetDefaultWindowMode();

	/** Loads the user .ini settings into GConfig */
	static void LoadConfigIni(bool bForceReload = false);

	/** Request a change to the specified resolution and window mode. Optionally apply cmd line overrides. */
	static void RequestResolutionChange(int32 InResolutionX, int32 InResolutionY, EWindowMode::Type InWindowMode, bool bInDoOverrides = true);

	/** Returns the game local machine settings (resolution, windowing mode, scalability settings, etc...) */
	UFUNCTION(BlueprintCallable, Category=Settings)
	static UGameUserSettings* GetGameUserSettings();

	/** Runs the hardware benchmark and populates ScalabilityQuality as well as the last benchmark results config members, but does not apply the settings it determines. Designed to be called in conjunction with ApplyHardwareBenchmarkResults */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void RunHardwareBenchmark(int32 WorkScale = 10, float CPUMultiplier = 1.0f, float GPUMultiplier = 1.0f);

	/** Applies the settings stored in ScalabilityQuality and saves settings */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void ApplyHardwareBenchmarkResults();

	/** Whether the curently running system supports HDR display output */
	UFUNCTION(BlueprintCallable, Category=Settings, meta = (DisplayName = "Supports HDR Display Output"))
	virtual bool SupportsHDRDisplayOutput() const;

	/** Enables or disables HDR display output. Can be called again to change the desired nit level */
	UFUNCTION(BlueprintCallable, Category=Settings, meta=(DisplayName = "Enable HDR Display Output"))
	void EnableHDRDisplayOutput(bool bEnable, int32 DisplayNits = 1000);

	/** Returns 0 if HDR isn't supported or is turned off */
	UFUNCTION(BlueprintCallable, Category = Settings, meta = (DisplayName = "Get Current HDR Display Nits"))
	int32 GetCurrentHDRDisplayNits() const;

	UFUNCTION(BlueprintCallable, Category = Settings, meta = (DisplayName = "Is HDR Enabled"))
	bool IsHDREnabled() const;

	/** Whether to use VSync or not. (public to allow UI to connect to it) */
	UPROPERTY(config)
	bool bUseVSync;

	// cached for the UI, current state if stored in console variables
	Scalability::FQualityLevels ScalabilityQuality;

protected:
	/** Game screen resolution width, in pixels. */
	UPROPERTY(config)
	uint32 ResolutionSizeX;

	/** Game screen resolution height, in pixels. */
	UPROPERTY(config)
	uint32 ResolutionSizeY;

	/** Game screen resolution width, in pixels. */
	UPROPERTY(config)
	uint32 LastUserConfirmedResolutionSizeX;

	/** Game screen resolution height, in pixels. */
	UPROPERTY(config)
	uint32 LastUserConfirmedResolutionSizeY;

	/** Window PosX */
	UPROPERTY(config)
	int32 WindowPosX;

	/** Window PosY */
	UPROPERTY(config)
	int32 WindowPosY;

	/**
	 * Game window fullscreen mode
	 *	0 = Fullscreen
	 *	1 = Windowed fullscreen
	 *	2 = Windowed
	 */
	UPROPERTY(config)
	int32 FullscreenMode;

	/** Last user confirmed fullscreen mode setting. */
	UPROPERTY(config)
	int32 LastConfirmedFullscreenMode;

	/** Fullscreen mode to use when toggling between windowed and fullscreen. Same values as r.FullScreenMode. */
	UPROPERTY(config)
	int32 PreferredFullscreenMode;

	/** All settings will be wiped and set to default if the serialized version differs from UE_GAMEUSERSETTINGS_VERSION. */
	UPROPERTY(config)
	uint32 Version;

	UPROPERTY(config)
	int32 AudioQualityLevel;

	/** Frame rate cap */
	UPROPERTY(config)
	float FrameRateLimit;

	/** Min resolution scale we allow in current display mode */
	float MinResolutionScale;

	/** Desired screen width used to calculate the resolution scale when user changes display mode */
	UPROPERTY(config)
	int32 DesiredScreenWidth;

	/** If true, the desired screen height will be used to scale the render resolution automatically. */
	UPROPERTY(globalconfig)
	bool bUseDesiredScreenHeight;

	/** Desired screen height used to calculate the resolution scale when user changes display mode */
	UPROPERTY(config)
	int32 DesiredScreenHeight;

	/** Result of the last benchmark; calculated resolution to use. */
	UPROPERTY(config)
	float LastRecommendedScreenWidth;

	/** Result of the last benchmark; calculated resolution to use. */
	UPROPERTY(config)
	float LastRecommendedScreenHeight;

	/** Result of the last benchmark (CPU); -1 if there has not been a benchmark run */
	UPROPERTY(config)
	float LastCPUBenchmarkResult;

	/** Result of the last benchmark (GPU); -1 if there has not been a benchmark run */
	UPROPERTY(config)
	float LastGPUBenchmarkResult;

	/** Result of each individual sub-section of the last CPU benchmark; empty if there has not been a benchmark run */
	UPROPERTY(config)
	TArray<float> LastCPUBenchmarkSteps;

	/** Result of each individual sub-section of the last GPU benchmark; empty if there has not been a benchmark run */
	UPROPERTY(config)
	TArray<float> LastGPUBenchmarkSteps;

	/**
	 * Multiplier used against the last GPU benchmark
	 */
	UPROPERTY(config)
	float LastGPUBenchmarkMultiplier;

	/** HDR */
	UPROPERTY(config)
	bool bUseHDRDisplayOutput;

	/** HDR */
	UPROPERTY(config)
	int32 HDRDisplayOutputNits;

public:
	/** Returns the last CPU benchmark result (set by RunHardwareBenchmark) */
	float GetLastCPUBenchmarkResult() const
	{
		return LastCPUBenchmarkResult;
	}

	/** Returns the last GPU benchmark result (set by RunHardwareBenchmark) */
	float GetLastGPUBenchmarkResult() const
	{
		return LastGPUBenchmarkResult;
	}

	/** Returns each individual step of the last CPU benchmark result (set by RunHardwareBenchmark) */
	TArray<float> GetLastCPUBenchmarkSteps() const
	{
		return LastCPUBenchmarkSteps;
	}

	/** Returns each individual step of the last GPU benchmark result (set by RunHardwareBenchmark) */
	TArray<float> GetLastGPUBenchmarkSteps() const
	{
		return LastGPUBenchmarkSteps;
	}

protected:
	/**
	 * Check if the current version of the game user settings is valid. Sub-classes can override this to provide game-specific versioning as necessary.
	 * @return True if the current version is valid, false if it is not
	 */
	virtual bool IsVersionValid();

	/** Update the version of the game user settings to the current version */
	virtual void UpdateVersion();

	/** Picks the best resolution quality for a given screen size */
	float FindResolutionQualityForScreenSize(float Width, float Height);

	/** Sets the frame rate limit CVar to the passed in value, 0.0 indicates no limit */
	static void SetFrameRateLimitCVar(float InLimit);

	/** Returns the effective frame rate limit (by default it returns the FrameRateLimit member) */
	virtual float GetEffectiveFrameRateLimit();

	void UpdateResolutionQuality();

private:

	UPROPERTY(BlueprintAssignable, meta = (AllowPrivateAccess = "true"))
	FOnGameUserSettingsUINeedsUpdate OnGameUserSettingsUINeedsUpdate;

	void SetPreferredFullscreenMode(int32 Mode);
};
