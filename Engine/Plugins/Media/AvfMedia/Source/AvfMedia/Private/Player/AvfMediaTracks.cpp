// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaTracks.h"
#include "AvfMediaPrivate.h"

#include "Containers/ResourceArray.h"
#include "MediaSamples.h"
#include "Misc/ScopeLock.h"

#if WITH_ENGINE
	#include "RenderingThread.h"
#endif

#include "AvfMediaAudioSample.h"
#include "AvfMediaOverlaySample.h"
#include "AvfMediaVideoSampler.h"
#include "AvfMediaUtils.h"

#import <AudioToolbox/AudioToolbox.h>

#define AUDIO_PLAYBACK_VIA_ENGINE (PLATFORM_MAC && 0)

/* FAVPlayerItemLegibleOutputPushDelegate
 *****************************************************************************/

@interface FAVPlayerItemLegibleOutputPushDelegate : NSObject<AVPlayerItemLegibleOutputPushDelegate>
{
	FAvfMediaTracks* Tracks;
}
- (id)initWithMediaTracks:(FAvfMediaTracks*)Tracks;
- (void)legibleOutput:(AVPlayerItemLegibleOutput *)output didOutputAttributedStrings:(NSArray<NSAttributedString *> *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime;
@end

@implementation FAVPlayerItemLegibleOutputPushDelegate

- (id)initWithMediaTracks:(FAvfMediaTracks*)InTracks
{
	id Self = [super init];
	if (Self)
	{
		Tracks = InTracks;
	}
	return Self;
}

- (void)legibleOutput:(AVPlayerItemLegibleOutput *)Output didOutputAttributedStrings:(NSArray<NSAttributedString *> *)Strings nativeSampleBuffers:(NSArray *)NativeSamples forItemTime:(CMTime)ItemTime
{
	Tracks->ProcessCaptions(Output, Strings, NativeSamples, ItemTime);
}

@end


/* FAvfMediaTracks structors
 *****************************************************************************/

FAvfMediaTracks::FAvfMediaTracks(FMediaSamples& InSamples)
	: AudioPaused(false)
	, AudioSamplePool(new FAvfMediaAudioSamplePool)
	, PlayerItem(nullptr)
	, Samples(InSamples)
	, SeekTime(-1.0)
	, SelectedAudioTrack(INDEX_NONE)
	, SelectedCaptionTrack(INDEX_NONE)
	, SelectedVideoTrack(INDEX_NONE)
	, Zoomed(false)
{
	LastAudioSampleTime = kCMTimeZero;
	VideoSampler = MakeShared<FAvfMediaVideoSampler, ESPMode::ThreadSafe>(Samples);
}


FAvfMediaTracks::~FAvfMediaTracks()
{
	Reset();

	delete AudioSamplePool;
	AudioSamplePool = nullptr;
}


/* FAvfMediaTracks interface
 *****************************************************************************/

void FAvfMediaTracks::AppendStats(FString &OutStats) const
{
	FScopeLock Lock(&CriticalSection);

	// audio tracks
	OutStats += TEXT("Audio Tracks\n");

	if (AudioTracks.Num() == 0)
	{
		OutStats += TEXT("    none\n");
	}
	else
	{
		for (uint32 TrackIndex = 0; TrackIndex < AudioTracks.Num(); TrackIndex++)
		{
			const FTrack& Track = AudioTracks[TrackIndex];

			OutStats += FString::Printf(TEXT("    %s\n"), *GetTrackDisplayName(EMediaTrackType::Audio, TrackIndex).ToString());
			OutStats += FString::Printf(TEXT("        Not implemented yet"));
		}
	}

	// video tracks
	OutStats += TEXT("Video Tracks\n");

	if (VideoTracks.Num() == 0)
	{
		OutStats += TEXT("    none\n");
	}
	else
	{
		for (uint32 TrackIndex = 0; TrackIndex < VideoTracks.Num(); TrackIndex++)
		{
			const FTrack& Track = VideoTracks[TrackIndex];

			OutStats += FString::Printf(TEXT("    %s\n"), *GetTrackDisplayName(EMediaTrackType::Video, TrackIndex).ToString());
			OutStats += FString::Printf(TEXT("        BitRate: %i\n"), [Track.AssetTrack estimatedDataRate]);
		}
	}
}


void FAvfMediaTracks::Initialize(AVPlayerItem* InPlayerItem, FString& OutInfo)
{
	Reset();

	FScopeLock Lock(&CriticalSection);

	PlayerItem = InPlayerItem;

	// initialize tracks
	NSArray* PlayerTracks = PlayerItem.tracks;
	NSError* Error = nil;

	int32 StreamIndex = 0;

	for (AVPlayerItemTrack* PlayerTrack in PlayerItem.tracks)
	{
		// create track
		FTrack* Track = nullptr;
		AVAssetTrack* AssetTrack = PlayerTrack.assetTrack;
		NSString* MediaType = AssetTrack.mediaType;

		OutInfo += FString::Printf(TEXT("Stream %i\n"), StreamIndex);
		OutInfo += FString::Printf(TEXT("    Type: %s\n"), *AvfMedia::MediaTypeToString(MediaType));

		if ([MediaType isEqualToString:AVMediaTypeAudio])
		{
			int32 TrackIndex = AudioTracks.AddDefaulted();
			Track = &AudioTracks[TrackIndex];
			
			Track->Name = FString::Printf(TEXT("Audio Track %i"), TrackIndex);
			Track->Converter = nullptr;
			
#if AUDIO_PLAYBACK_VIA_ENGINE
			// create asset reader
			AVAssetReader* Reader = [[AVAssetReader alloc] initWithAsset: [AssetTrack asset] error:&Error];
			
			if (Error != nil)
			{
				FString ErrorStr([Error localizedDescription]);
				UE_LOG(LogAvfMedia, Error, TEXT("Failed to create asset reader for track %i: %s"), AssetTrack.trackID, *ErrorStr);
				
				continue;
			}

			NSMutableDictionary* OutputSettings = [NSMutableDictionary dictionary];
			[OutputSettings setObject : [NSNumber numberWithInt : kAudioFormatLinearPCM] forKey : (NSString*)AVFormatIDKey];
			
			AVAssetReaderTrackOutput* AudioReaderOutput = [[AVAssetReaderTrackOutput alloc] initWithTrack:AssetTrack outputSettings:OutputSettings];
			check(AudioReaderOutput);
			
			AudioReaderOutput.alwaysCopiesSampleData = NO;
			AudioReaderOutput.supportsRandomAccess = YES;
			
			// Assign the track to the reader.
			[Reader addOutput:AudioReaderOutput];

			Track->Output = AudioReaderOutput;
			Track->Loaded = [Reader startReading];
			Track->Reader = Reader;
#else
			Track->Output = [PlayerTrack retain];
			Track->Loaded = true;
			Track->Reader = nil;
#endif

			CMFormatDescriptionRef DescRef = (CMFormatDescriptionRef)[AssetTrack.formatDescriptions objectAtIndex:0];
			const AudioStreamBasicDescription* Desc = CMAudioFormatDescriptionGetStreamBasicDescription(DescRef);

			if (Desc)
			{
				OutInfo += FString::Printf(TEXT("    Channels: %u\n"), Desc->mChannelsPerFrame);
				OutInfo += FString::Printf(TEXT("    Sample Rate: %g Hz\n"), Desc->mSampleRate);
				if (Desc->mBitsPerChannel > 0)
				{
					OutInfo += FString::Printf(TEXT("    Bits Per Channel: %u\n"), Desc->mBitsPerChannel);
				}
				else
				{
					OutInfo += FString::Printf(TEXT("    Bits Per Channel: n/a\n"));
				}
			}
			else
			{
				OutInfo += TEXT("    failed to get audio track information\n");
			}
		}
		else if (([MediaType isEqualToString:AVMediaTypeClosedCaption]) || ([MediaType isEqualToString:AVMediaTypeSubtitle]) || ([MediaType isEqualToString:AVMediaTypeText]))
		{
			FAVPlayerItemLegibleOutputPushDelegate* Delegate = [[FAVPlayerItemLegibleOutputPushDelegate alloc] initWithMediaTracks:this];
			AVPlayerItemLegibleOutput* Output = [AVPlayerItemLegibleOutput new];
			check(Output);
			
			// We don't want AVPlayer to render the frame, just decode it for us
			Output.suppressesPlayerRendering = YES;
			
			[Output setDelegate:Delegate queue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];

			int32 TrackIndex = CaptionTracks.AddDefaulted();
			Track = &CaptionTracks[TrackIndex];

			Track->Name = FString::Printf(TEXT("Caption Track %i"), TrackIndex);
			Track->Output = Output;
			Track->Loaded = true;
			Track->Reader = nil;
			Track->Converter = nullptr;
		}
		else if ([MediaType isEqualToString:AVMediaTypeTimecode])
		{
			// not implemented yet - not sure they should be as these are SMTPE editing timecodes for iMovie/Final Cut/etc. not playback timecodes. They only make sense in editable Quicktime Movies (.mov).
			OutInfo += FString::Printf(TEXT("    Type: Timecode (UNSUPPORTED)\n"));
		}
		else if ([MediaType isEqualToString:AVMediaTypeVideo])
		{
			NSMutableDictionary* OutputSettings = [NSMutableDictionary dictionary];
			// Mac:
			// On Mac kCVPixelFormatType_422YpCbCr8 is the preferred single-plane YUV format but for H.264 bi-planar formats are the optimal choice
			// The native RGBA format is 32ARGB but we use 32BGRA for consistency with iOS for now.
			//
			// iOS/tvOS:
			// On iOS only bi-planar kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange/kCVPixelFormatType_420YpCbCr8BiPlanarFullRange are supported for YUV so an additional conversion is required.
			// The only RGBA format is 32BGRA
#if COREVIDEO_SUPPORTS_METAL
			[OutputSettings setObject : [NSNumber numberWithInt : kCVPixelFormatType_420YpCbCr8BiPlanarFullRange] forKey : (NSString*)kCVPixelBufferPixelFormatTypeKey];
#else
			[OutputSettings setObject : [NSNumber numberWithInt : kCVPixelFormatType_32BGRA] forKey : (NSString*)kCVPixelBufferPixelFormatTypeKey];
#endif

#if WITH_ENGINE
			// Setup sharing with RHI's starting with the optional Metal RHI
			if (FPlatformMisc::HasPlatformFeature(TEXT("Metal")))
			{
				[OutputSettings setObject:[NSNumber numberWithBool:YES] forKey:(NSString*)kCVPixelBufferMetalCompatibilityKey];
			}

#if PLATFORM_MAC
			[OutputSettings setObject:[NSNumber numberWithBool:YES] forKey:(NSString*)kCVPixelBufferOpenGLCompatibilityKey];
#else
			[OutputSettings setObject:[NSNumber numberWithBool:YES] forKey:(NSString*)kCVPixelBufferOpenGLESCompatibilityKey];
#endif
#endif //WITH_ENGINE

			// Use unaligned rows
			[OutputSettings setObject:[NSNumber numberWithInteger:1] forKey:(NSString*)kCVPixelBufferBytesPerRowAlignmentKey];
		
			// Then create the video output object from which we will grab frames as CVPixelBuffer's
			AVPlayerItemVideoOutput* Output = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:OutputSettings];
			check(Output);
			// We don't want AVPlayer to render the frame, just decode it for us
			Output.suppressesPlayerRendering = YES;

			int32 TrackIndex = VideoTracks.AddDefaulted();
			Track = &VideoTracks[TrackIndex];

			Track->Name = FString::Printf(TEXT("Video Track %i"), TrackIndex);
			Track->Output = Output;
			Track->Loaded = true;
			Track->Reader = nil;
			Track->Converter = nullptr;

			CMFormatDescriptionRef DescRef = (CMFormatDescriptionRef)[AssetTrack.formatDescriptions objectAtIndex:0];
			CMVideoCodecType CodecType = CMFormatDescriptionGetMediaSubType(DescRef);
			OutInfo += FString::Printf(TEXT("    Codec: %s\n"), *AvfMedia::CodecTypeToString(CodecType));
			OutInfo += FString::Printf(TEXT("    Dimensions: %i x %i\n"), (int32)AssetTrack.naturalSize.width, (int32)AssetTrack.naturalSize.height);
			OutInfo += FString::Printf(TEXT("    Frame Rate: %g fps\n"), AssetTrack.nominalFrameRate);
			OutInfo += FString::Printf(TEXT("    BitRate: %i\n"), (int32)AssetTrack.estimatedDataRate);
		}

		OutInfo += TEXT("\n");

		PlayerTrack.enabled = NO;

		if (Track != nullptr)
		{
			Track->AssetTrack = AssetTrack;
			Track->DisplayName = FText::FromString(Track->Name);
			Track->StreamIndex = StreamIndex;
		}

		++StreamIndex;
	}
}


void FAvfMediaTracks::ProcessAudio()
{
	FScopeLock Lock(&CriticalSection);

#if AUDIO_PLAYBACK_VIA_ENGINE
	if (!AudioTracks.IsValidIndex(SelectedAudioTrack) || !PlayerItem || PlayerItem.status != AVPlayerItemStatusReadyToPlay || CMTimeCompare(PlayerItem.duration, CMTimeMakeWithSeconds(0.0, 1000)) < 1)
	{
		return;
	}

	CMTime CurrentTime = [PlayerItem currentTime];

	ESyncStatus Sync = Default;

	AVAssetReaderTrackOutput* AudioReaderOutput = (AVAssetReaderTrackOutput*)AudioTracks[SelectedAudioTrack].Output;
	check(AudioReaderOutput);

	while (Sync < Ready)
	{
		float Delta = CMTimeGetSeconds(LastAudioSampleTime) - CMTimeGetSeconds(CurrentTime);

		if ((Delta <= 1.0f) && (Delta > 0.0f))
		{
			Sync = Ready;

			break;
		}

		Sync = Behind;

		CMSampleBufferRef LatestSamples = [AudioReaderOutput copyNextSampleBuffer];

		if (!LatestSamples)
		{
			break;
		}

		CMTime FrameTimeStamp = CMSampleBufferGetPresentationTimeStamp(LatestSamples);
		CMTime Duration = CMSampleBufferGetOutputDuration(LatestSamples);
		CMTime FinalTimeStamp = CMTimeAdd(FrameTimeStamp, Duration);
		CMTime Seek = CMTimeMakeWithSeconds(SeekTime, 1000);

		if ((SeekTime < 0.0f) || (CMTimeCompare(Seek, FrameTimeStamp) >= 0 && CMTimeCompare(Seek, FinalTimeStamp) < 0))
		{
			SeekTime = -1.0f;

			CMItemCount NumSamples = CMSampleBufferGetNumSamples(LatestSamples);
			CMFormatDescriptionRef Format = CMSampleBufferGetFormatDescription(LatestSamples);
			check(Format);

			const AudioStreamBasicDescription* ASBD = CMAudioFormatDescriptionGetStreamBasicDescription(Format);
			check(ASBD);

			CMBlockBufferRef Buffer = CMSampleBufferGetDataBuffer(LatestSamples);
			check(Buffer);

			// create & add sample to queue
			uint32 InputLength = CMBlockBufferGetDataLength(Buffer);

			if (InputLength)
			{
				uint32 OutputLength = (NumSamples * TargetDesc.mBytesPerPacket);
				FTrack& AudioTrack = AudioTracks[SelectedAudioTrack];

				auto AudioSample = MakeShared<FAvfMediaAudioSample, ESPMode::ThreadSafe>();

				if (!AudioSample->Initialize(
					OutputLength,
					NumSamples,
					TargetDesc.mChannelsPerFrame,
					TargetDesc.mSampleRate,
					FTimespan::FromSeconds(CMTimeGetSeconds(FrameTimeStamp))))
				{
					continue;
				}

				if (FMemory::Memcmp(ASBD, &TargetDesc, sizeof(AudioStreamBasicDescription)) != 0)
				{
					// conversion to 16-bit PCM required
					uint8* Data = new uint8[InputLength];

					OSErr Err = CMBlockBufferCopyDataBytes(Buffer, 0, InputLength, Data);
					check(Err == noErr);

					if (AudioTrack.Converter == nullptr)
					{
						OSStatus CAErr = AudioConverterNew(ASBD, &TargetDesc, &AudioTrack.Converter);
						check(CAErr == noErr);
					}

					OSStatus CAErr = AudioConverterConvertBuffer(AudioTrack.Converter, InputLength, Data, &OutputLength, AudioSample->GetMutableBuffer());
					check(CAErr == noErr);

					delete[] Data;
				}
				else
				{
					// data is already in 16-bit PCM
					OSErr Err = CMBlockBufferCopyDataBytes(Buffer, 0, OutputLength, AudioSample->GetMutableBuffer());
					check(Err == noErr);
				}

				Samples.AddAudio(AudioSample);
				LastAudioSampleTime = FinalTimeStamp;
			}
		}

		CFRelease(LatestSamples);
	}
#endif
}


void FAvfMediaTracks::ProcessCaptions(AVPlayerItemLegibleOutput* Output, NSArray<NSAttributedString*>* Strings, NSArray* NativeSamples, CMTime ItemTime)
{
	if (SelectedCaptionTrack != INDEX_NONE)
	{
		return;
	}

	FScopeLock Lock(&CriticalSection);
		
	NSDictionary* DocumentAttributes = @{NSDocumentTypeDocumentAttribute:NSPlainTextDocumentType};
	FTimespan DisplayTime = FTimespan::FromSeconds(CMTimeGetSeconds(ItemTime));
	
	FString OutputString;
	bool bFirst = true;

	for (NSAttributedString* String in Strings)
	{
		if (!String)
		{
			continue;
		}

		// strip attributes from the string (we don't care for them)
		NSRange Range = NSMakeRange(0, String.length);
		NSData* Data = [String dataFromRange:Range documentAttributes:DocumentAttributes error:NULL];
		NSString* Result = [[NSString alloc] initWithData:Data encoding:NSUTF8StringEncoding];
				
		// append the string
		if (!bFirst)
		{
			OutputString += TEXT("\n");
		}

		bFirst = false;
		OutputString += FString(Result);
	}

	if (OutputString.IsEmpty())
	{
		return;
	}

	// create & add sample to queue
	auto OverlaySample = MakeShared<FAvfMediaOverlaySample, ESPMode::ThreadSafe>();

	if (OverlaySample->Initialize(OutputString, DisplayTime))
	{
		Samples.AddCaption(OverlaySample);
	}
}


void FAvfMediaTracks::ProcessVideo()
{
	struct FVideoSamplerTickParams
	{
		TWeakPtr<FAvfMediaVideoSampler, ESPMode::ThreadSafe> VideoSamplerPtr;
	}
	VideoSamplerTickParams = { VideoSampler };

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(AvfMediaVideoSamplerTick,
		FVideoSamplerTickParams, Params, VideoSamplerTickParams,
		{
			auto PinnedVideoSampler = Params.VideoSamplerPtr.Pin();

			if (PinnedVideoSampler.IsValid())
			{
				PinnedVideoSampler->Tick();
			}
		});
}


void FAvfMediaTracks::Reset()
{
	FScopeLock Lock(&CriticalSection);

	// reset video sampler
	struct FResetOutputParams
	{
		TWeakPtr<FAvfMediaVideoSampler, ESPMode::ThreadSafe> VideoSamplerPtr;
	}
	ResetOutputParams = { VideoSampler };

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(AvfMediaVideoSamplerResetOutput,
		FResetOutputParams, Params, ResetOutputParams,
	    {
			auto PinnedVideoSampler = Params.VideoSamplerPtr.Pin();

			if (PinnedVideoSampler.IsValid())
			{
				PinnedVideoSampler->SetOutput(nil, 0.0f);
			}
		});
	
	// reset tracks
	SelectedAudioTrack = INDEX_NONE;
	SelectedCaptionTrack = INDEX_NONE;
	SelectedVideoTrack = INDEX_NONE;

	for (FTrack& Track : AudioTracks)
	{
		if (Track.Converter)
		{
			OSStatus CAErr = AudioConverterDispose(Track.Converter);
			check(CAErr == noErr);
		}

		[Track.Output release];
		[Track.Reader release];
	}

	for (FTrack& Track : CaptionTracks)
	{
		AVPlayerItemLegibleOutput* Output = (AVPlayerItemLegibleOutput*)Track.Output;

		[Output.delegate release];
		[Track.Output release];
		[Track.Reader release];
	}

	for (FTrack& Track : VideoTracks)
	{
		[Track.Output release];
		[Track.Reader release];
	}

	AudioTracks.Empty();
	CaptionTracks.Empty();
	VideoTracks.Empty();
	
	LastAudioSampleTime = kCMTimeZero;
	AudioPaused = false;
	SeekTime = -1.0;
	Zoomed = false;
}


void FAvfMediaTracks::Seek(const FTimespan& Time)
{
#if AUDIO_PLAYBACK_VIA_ENGINE
	FScopeLock Lock(&CriticalSection);

	if (SelectedAudioTrack != INDEX_NONE)
	{
		return;
	}

	double LastSampleTime = CMTimeGetSeconds(LastAudioSampleTime);
	SeekTime = Time.GetTotalSeconds();

	if (SeekTime >= LastSampleTime)
	{
		return;
	}

	AVAssetReaderTrackOutput* AudioReaderOutput = (AVAssetReaderTrackOutput*)AudioTracks[SelectedAudioTrack].Output;
	check(AudioReaderOutput);

	LastAudioSampleTime = CMTimeMakeWithSeconds(0, 1000);

	CMSampleBufferRef LatestSamples = nullptr;
	while ((LatestSamples = [AudioReaderOutput copyNextSampleBuffer]))
	{
		if (LatestSamples)
		{
			CFRelease(LatestSamples);
		}
	}

	CMTime Start = CMTimeMakeWithSeconds(SeekTime, 1000);
	CMTime Duration = PlayerItem.asset.duration;
	Duration = CMTimeSubtract(Duration, Start);

	CMTimeRange TimeRange = CMTimeRangeMake(Start, Duration);
	NSValue* Value = [NSValue valueWithBytes : &TimeRange objCType : @encode(CMTimeRange)];
	NSArray* Array = @[ Value ];

	[AudioReaderOutput resetForReadingTimeRanges : Array];
#endif
}


void FAvfMediaTracks::SetRate(float Rate)
{
	FScopeLock Lock(&CriticalSection);

	// Can only play sensible audio at full rate forward - when seeking, scrubbing or reversing we can't supply
	// the correct samples.
	const bool bNearOne = FMath::IsNearlyEqual(Rate, 1.0f);	
	const bool bWasPaused = AudioPaused;
		
	AudioPaused = !bNearOne;
		
#if AUDIO_PLAYBACK_VIA_ENGINE
	if (!AudioPaused && Zoomed)
	{
		CMTime CurrentTime = [PlayerItem currentTime];
		FTimespan Time = FTimespan::FromSeconds(CMTimeGetSeconds(CurrentTime));
		Seek(Time);
	}
#endif
		
	Zoomed = !bNearOne;
}


/* IMediaTracks interface
 *****************************************************************************/

bool FAvfMediaTracks::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	if ((FormatIndex != 0) || !AudioTracks.IsValidIndex(TrackIndex))
	{
		return false;
	}

	const FTrack& Track = AudioTracks[TrackIndex];
	checkf(Track.AssetTrack.formatDescriptions.count == 1, TEXT("Can't handle non-uniform audio streams!"));

	CMFormatDescriptionRef DescRef = (CMFormatDescriptionRef)[AudioTracks[TrackIndex].AssetTrack.formatDescriptions objectAtIndex : 0];
	AudioStreamBasicDescription const* Desc = CMAudioFormatDescriptionGetStreamBasicDescription(DescRef);

	OutFormat.BitsPerSample = 16;
	OutFormat.NumChannels = (Desc != nullptr) ? Desc->mChannelsPerFrame : 0;
	OutFormat.SampleRate = (Desc != nullptr) ? Desc->mSampleRate : 0;
	OutFormat.TypeName = TEXT("PCM"); // @todo trepka: fix me (should be input type, not output type)

	return true;
}


int32 FAvfMediaTracks::GetNumTracks(EMediaTrackType TrackType) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return AudioTracks.Num();

	case EMediaTrackType::Caption:
		return CaptionTracks.Num();

	case EMediaTrackType::Video:
		return VideoTracks.Num();

	default:
		return 0;
	}
}


int32 FAvfMediaTracks::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return ((TrackIndex >= 0) && (TrackIndex < GetNumTracks(TrackType))) ? 1 : 0;
}


int32 FAvfMediaTracks::GetSelectedTrack(EMediaTrackType TrackType) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return SelectedAudioTrack;

	case EMediaTrackType::Caption:
		return SelectedCaptionTrack;

	case EMediaTrackType::Video:
		return SelectedVideoTrack;

	default:
		return INDEX_NONE;
	}
}


FText FAvfMediaTracks::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			return AudioTracks[TrackIndex].DisplayName;
		}
		break;

	case EMediaTrackType::Caption:
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			return CaptionTracks[TrackIndex].DisplayName;
		}
		break;

	case EMediaTrackType::Video:
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			return VideoTracks[TrackIndex].DisplayName;
		}
		break;

	default:
		break;
	}

	return FText::GetEmpty();
}


int32 FAvfMediaTracks::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return (GetSelectedTrack(TrackType) != INDEX_NONE) ? 0 : INDEX_NONE;
}


FString FAvfMediaTracks::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (TrackType == EMediaTrackType::Audio)
	{
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			return FString(AudioTracks[TrackIndex].AssetTrack.languageCode);
		}
	}
	else if (TrackType == EMediaTrackType::Caption)
	{
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			return FString(CaptionTracks[TrackIndex].AssetTrack.languageCode);
		}
	}
	else if (TrackType == EMediaTrackType::Video)
	{
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			return FString(VideoTracks[TrackIndex].AssetTrack.languageCode);
		}
	}

	return FString();
}


FString FAvfMediaTracks::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			return AudioTracks[TrackIndex].Name;
		}
		break;

	case EMediaTrackType::Caption:
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			return CaptionTracks[TrackIndex].Name;
		}
		break;

	case EMediaTrackType::Video:
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			return VideoTracks[TrackIndex].Name;
		}
		break;

	default:
		break;
	}

	return FString();
}


bool FAvfMediaTracks::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if ((FormatIndex != 0) || !VideoTracks.IsValidIndex(TrackIndex))
	{
		return false;
	}

	const FTrack& Track = VideoTracks[TrackIndex];

	OutFormat.Dim = FIntPoint(
		[Track.AssetTrack naturalSize].width,
		[Track.AssetTrack naturalSize].height
	);

	OutFormat.FrameRate = [Track.AssetTrack nominalFrameRate];
	OutFormat.FrameRates = TRange<float>(OutFormat.FrameRate);
	OutFormat.TypeName = TEXT("BGRA"); // @todo trepka: fix me (should be input format, not output format)

	return true;
}


bool FAvfMediaTracks::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	FScopeLock Lock(&CriticalSection);

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (TrackIndex != SelectedAudioTrack)
		{
			UE_LOG(LogAvfMedia, Verbose, TEXT("Selecting audio track %i instead of %i (%i tracks)."), TrackIndex, SelectedAudioTrack, AudioTracks.Num());

			// disable current track
			if (SelectedAudioTrack != INDEX_NONE)
			{
				UE_LOG(LogAvfMedia, VeryVerbose, TEXT("Disabling audio track %i"), SelectedAudioTrack);

#if AUDIO_PLAYBACK_VIA_ENGINE
				AVAssetReaderTrackOutput* AudioReaderOutput = (AVAssetReaderTrackOutput*)AudioTracks[SelectedAudioTrack].Output;
				check(AudioReaderOutput);

				CMSampleBufferRef LatestSamples = nullptr;

				while ((LatestSamples = [AudioReaderOutput copyNextSampleBuffer]))
				{
					if (LatestSamples)
					{
						CFRelease(LatestSamples);
					}
				}

				CMTimeRange TimeRange = CMTimeRangeMake(kCMTimeZero, PlayerItem.asset.duration);
				NSValue* Value = [NSValue valueWithBytes : &TimeRange objCType : @encode(CMTimeRange)];
				NSArray* Array = @[ Value ];

				[AudioReaderOutput resetForReadingTimeRanges : Array];

#else
				AVPlayerItemTrack* PlayerTrack = (AVPlayerItemTrack*)AudioTracks[SelectedAudioTrack].Output;
				check(PlayerTrack);

				PlayerTrack.enabled = NO;
#endif

				SelectedAudioTrack = INDEX_NONE;
			}

			// enable new track
			if (TrackIndex != INDEX_NONE)
			{
				if (!AudioTracks.IsValidIndex(TrackIndex))
				{
					return false;
				}

				UE_LOG(LogAvfMedia, VeryVerbose, TEXT("Enabling audio track %i"), TrackIndex);
			}

			SelectedAudioTrack = TrackIndex;

			// update output
			if (SelectedAudioTrack != INDEX_NONE)
			{
				const FTrack& SelectedTrack = AudioTracks[SelectedAudioTrack];
				
#if AUDIO_PLAYBACK_VIA_ENGINE
				CMFormatDescriptionRef DescRef = (CMFormatDescriptionRef)[SelectedTrack.AssetTrack.formatDescriptions objectAtIndex : 0];
				AudioStreamBasicDescription const* ASBD = CMAudioFormatDescriptionGetStreamBasicDescription(DescRef);

				TargetDesc.mSampleRate = ASBD->mSampleRate;
				TargetDesc.mFormatID = kAudioFormatLinearPCM;
				TargetDesc.mFormatFlags = kAudioFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
				TargetDesc.mBytesPerPacket = ASBD->mChannelsPerFrame * sizeof(int16);
				TargetDesc.mFramesPerPacket = 1;
				TargetDesc.mBytesPerFrame = ASBD->mChannelsPerFrame * sizeof(int16);
				TargetDesc.mChannelsPerFrame = ASBD->mChannelsPerFrame;
				TargetDesc.mBitsPerChannel = 16;
				TargetDesc.mReserved = 0;
#else
				AVPlayerItemTrack* PlayerTrack = (AVPlayerItemTrack*)SelectedTrack.Output;
				check(PlayerTrack);

				PlayerTrack.enabled = YES;
#endif
			}
		}
		break;

	case EMediaTrackType::Caption:
		if (TrackIndex != SelectedCaptionTrack)
		{
			UE_LOG(LogAvfMedia, Verbose, TEXT("Selecting caption track %i instead of %i (%i tracks)."), TrackIndex, SelectedCaptionTrack, CaptionTracks.Num());

			// disable current track
			if (SelectedCaptionTrack != INDEX_NONE)
			{
				UE_LOG(LogAvfMedia, VeryVerbose, TEXT("Disabling caption track %i"), SelectedCaptionTrack);

				const FTrack& Track = CaptionTracks[SelectedCaptionTrack];

				[PlayerItem removeOutput : (AVPlayerItemOutput*)Track.Output];
				PlayerItem.tracks[Track.StreamIndex].enabled = NO;

				SelectedCaptionTrack = INDEX_NONE;
			}

			// enable new track
			if (TrackIndex != INDEX_NONE)
			{
				if (!CaptionTracks.IsValidIndex(TrackIndex))
				{
					return false;
				}

				UE_LOG(LogAvfMedia, VeryVerbose, TEXT("Enabling caption track %i"), TrackIndex);

				const FTrack& SelectedTrack = CaptionTracks[TrackIndex];
				PlayerItem.tracks[SelectedTrack.StreamIndex].enabled = YES;
			}

			SelectedCaptionTrack = TrackIndex;

			// update output
			if (SelectedCaptionTrack != INDEX_NONE)
			{
				[PlayerItem addOutput:(AVPlayerItemOutput*)CaptionTracks[SelectedCaptionTrack].Output];
			}
		}
		break;

	case EMediaTrackType::Video:
		if (TrackIndex != SelectedVideoTrack)
		{
			UE_LOG(LogAvfMedia, Verbose, TEXT("Selecting video track %i instead of %i (%i tracks)"), TrackIndex, SelectedVideoTrack, VideoTracks.Num());

			// disable current track
			if (SelectedVideoTrack != INDEX_NONE)
			{
				UE_LOG(LogAvfMedia, VeryVerbose, TEXT("Disabling video track %i"), SelectedVideoTrack);

				const FTrack& Track = VideoTracks[SelectedVideoTrack];

				[PlayerItem removeOutput : (AVPlayerItemOutput*)Track.Output];
				PlayerItem.tracks[Track.StreamIndex].enabled = NO;

				SelectedVideoTrack = INDEX_NONE;
			}

			// enable new track
			if (TrackIndex != INDEX_NONE)
			{
				if (!VideoTracks.IsValidIndex(TrackIndex))
				{
					return false;
				}

				UE_LOG(LogAvfMedia, VeryVerbose, TEXT("Enabling video track %i"), TrackIndex);

				const FTrack& SelectedTrack = VideoTracks[TrackIndex];
				PlayerItem.tracks[SelectedTrack.StreamIndex].enabled = YES;
			}

			SelectedVideoTrack = TrackIndex;

			// update output
			if (SelectedVideoTrack != INDEX_NONE)
			{
				[PlayerItem addOutput : (AVPlayerItemOutput*)VideoTracks[SelectedVideoTrack].Output];

				struct FSetOutputParams
				{
					AVPlayerItemVideoOutput* Output;
					TWeakPtr<FAvfMediaVideoSampler, ESPMode::ThreadSafe> VideoSamplerPtr;
					float FrameRate;
				}
				SetOutputParams = {
					(AVPlayerItemVideoOutput*)VideoTracks[SelectedVideoTrack].Output,
					VideoSampler,
					1.0f / [VideoTracks[TrackIndex].AssetTrack nominalFrameRate]
				};

				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(AvfMediaVideoSamplerSetOutput,
					FSetOutputParams, Params, SetOutputParams,
					{
						auto PinnedVideoSampler = Params.VideoSamplerPtr.Pin();

						if (PinnedVideoSampler.IsValid())
						{
							PinnedVideoSampler->SetOutput(Params.Output, Params.FrameRate);
						}
					});
			}
		}
		break;

	default:
		return false;
	}

	return true;
}


bool FAvfMediaTracks::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	if (FormatIndex != 0)
	{
		return false;
	}

	FScopeLock Lock(&CriticalSection);

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return AudioTracks.IsValidIndex(TrackIndex);

	case EMediaTrackType::Caption:
		return CaptionTracks.IsValidIndex(TrackIndex);

	case EMediaTrackType::Video:
		return VideoTracks.IsValidIndex(TrackIndex);

	default:
		return false;
	}
}

