// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AndroidJava.h"
#include "RHI.h"
#include "RHIResources.h"

// Wrapper for com/epicgames/ue4/MediaPlayer*.java.
class FJavaAndroidMediaPlayer : public FJavaClassObject
{
public:
	struct FAudioTrack
	{
		int32 Index;
		FString MimeType;
		FString DisplayName;
		FString Language;
		FString Name;
		uint32 Channels;
		uint32 SampleRate;
	};

	struct FCaptionTrack
	{
		int32 Index;
		FString MimeType;
		FString DisplayName;
		FString Language;
		FString Name;
	};

	struct FVideoTrack
	{
		int32 Index;
		FString MimeType;
		FString DisplayName;
		FString Language;
		FString Name;
		uint32 BitRate;
		FIntPoint Dimensions;
		float FrameRate;
	};

public:
	FJavaAndroidMediaPlayer(bool swizzlePixels, bool vulkanRenderer);
	int32 GetDuration();
	void Reset();
	void Stop();
	int32 GetCurrentPosition();
	bool IsLooping();
	bool IsPlaying();
	bool IsPrepared();
	bool DidComplete();
	bool SetDataSource(const FString & Url);
	bool SetDataSource(const FString& MoviePathOnDevice, int64 offset, int64 size);
	bool SetDataSource(jobject AssetMgr, const FString& AssetPath, int64 offset, int64 size);
	bool Prepare();
	bool PrepareAsync();
	void SeekTo(int32 Milliseconds);
	void SetLooping(bool Looping);
	void Release();
	int32 GetVideoHeight();
	int32 GetVideoWidth();
	void SetVideoEnabled(bool enabled = true);
	void SetAudioEnabled(bool enabled = true);
	bool GetVideoLastFrameData(void* & outPixels, int64 & outCount);
	void Start();
	void Pause();
	bool GetVideoLastFrame(int32 destTexture);
	bool SelectTrack(int32 index);
	bool GetAudioTracks(TArray<FAudioTrack>& AudioTracks);
	bool GetCaptionTracks(TArray<FCaptionTrack>& CaptionTracks);
	bool GetVideoTracks(TArray<FVideoTrack>& VideoTracks);
	bool DidResolutionChange();
	int32 GetExternalTextureId();
	bool UpdateVideoFrame(int32 ExternalTextureId, int32 *CurrentPosition, bool *bRegionChanged);

private:
	static FName GetClassName();

	bool bTrackInfoSupported;

	FJavaClassMethod GetDurationMethod;
	FJavaClassMethod ResetMethod;
	FJavaClassMethod StopMethod;
	FJavaClassMethod GetCurrentPositionMethod;
	FJavaClassMethod IsLoopingMethod;
	FJavaClassMethod IsPlayingMethod;
	FJavaClassMethod IsPreparedMethod;
	FJavaClassMethod DidCompleteMethod;
	FJavaClassMethod SetDataSourceURLMethod;
	FJavaClassMethod SetDataSourceFileMethod;
	FJavaClassMethod SetDataSourceAssetMethod;
	FJavaClassMethod PrepareMethod;
	FJavaClassMethod PrepareAsyncMethod;
	FJavaClassMethod SeekToMethod;
	FJavaClassMethod SetLoopingMethod;
	FJavaClassMethod ReleaseMethod;
	FJavaClassMethod GetVideoHeightMethod;
	FJavaClassMethod GetVideoWidthMethod;
	FJavaClassMethod SetVideoEnabledMethod;
	FJavaClassMethod SetAudioEnabledMethod;
	FJavaClassMethod GetVideoLastFrameDataMethod;
	FJavaClassMethod StartMethod;
	FJavaClassMethod PauseMethod;
	FJavaClassMethod GetVideoLastFrameMethod;
	FJavaClassMethod SelectTrackMethod;
	FJavaClassMethod GetAudioTracksMethod;
	FJavaClassMethod GetCaptionTracksMethod;
	FJavaClassMethod GetVideoTracksMethod;
	FJavaClassMethod DidResolutionChangeMethod;
	FJavaClassMethod GetExternalTextureIdMethod;
	FJavaClassMethod UpdateVideoFrameMethod;

	// FrameUpdateInfo member field ids
	jclass FrameUpdateInfoClass;
	jfieldID FrameUpdateInfo_CurrentPosition;
	jfieldID FrameUpdateInfo_FrameReady;
	jfieldID FrameUpdateInfo_RegionChanged;
	jfieldID FrameUpdateInfo_UScale;
	jfieldID FrameUpdateInfo_UOffset;
	jfieldID FrameUpdateInfo_VScale;
	jfieldID FrameUpdateInfo_VOffset;

	// AudioDeviceInfo member field ids
	jclass AudioTrackInfoClass;
	jfieldID AudioTrackInfo_Index;
	jfieldID AudioTrackInfo_MimeType;
	jfieldID AudioTrackInfo_DisplayName;
	jfieldID AudioTrackInfo_Language;
	jfieldID AudioTrackInfo_Channels;
	jfieldID AudioTrackInfo_SampleRate;

	// AudioDeviceInfo member field ids
	jclass CaptionTrackInfoClass;
	jfieldID CaptionTrackInfo_Index;
	jfieldID CaptionTrackInfo_MimeType;
	jfieldID CaptionTrackInfo_DisplayName;
	jfieldID CaptionTrackInfo_Language;

	// AudioDeviceInfo member field ids
	jclass VideoTrackInfoClass;
	jfieldID VideoTrackInfo_Index;
	jfieldID VideoTrackInfo_MimeType;
	jfieldID VideoTrackInfo_DisplayName;
	jfieldID VideoTrackInfo_Language;
	jfieldID VideoTrackInfo_BitRate;
	jfieldID VideoTrackInfo_Width;
	jfieldID VideoTrackInfo_Height;
	jfieldID VideoTrackInfo_FrameRate;

	FTextureRHIRef VideoTexture;
	bool bVideoTextureValid;

	float UScale, UOffset;
	float VScale, VOffset;

public:
	FTextureRHIRef GetVideoTexture()
	{
		return VideoTexture;
	}

	void SetVideoTexture(FTextureRHIRef Texture)
	{
		VideoTexture = Texture;
	}

	void SetVideoTextureValid(bool Condition)
	{
		bVideoTextureValid = Condition;
	}

	bool IsVideoTextureValid()
	{
		return bVideoTextureValid;
	}

	float GetUScale() { return UScale; }
	float GetUOffset() { return UOffset; }

	float GetVScale() { return VScale; }
	float GetVOffset() { return VOffset; }
};
