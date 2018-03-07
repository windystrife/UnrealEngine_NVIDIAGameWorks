// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioDeviceFormat.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/*
	 * This code is segmented here since it involves basically converting to and from various audio format types. Even though the
	 * audio device code assumes floats for the platform-independent mixing code, various audio devices may be in other types of formats
	 * so we need to support converting byte buffers to and from various formats.
	 */

	void IUnrealAudioDeviceModule::SetupBufferFormatConvertInfo()
	{
		FBufferFormatConvertInfo& ConvertInfo = StreamInfo.DeviceInfo.BufferFormatConvertInfo;

		// Setup the convert info to convert from user channels and format to device channels and format
		ConvertInfo.FromChannels = StreamInfo.DeviceInfo.NumChannels;
		ConvertInfo.ToChannels = StreamInfo.DeviceInfo.NumChannels;
		ConvertInfo.FromFormat = EStreamFormat::FLT;
		ConvertInfo.ToFormat = StreamInfo.DeviceInfo.DeviceDataFormat;

		// For either stream types, set the number of channels to use for the conversion to be the smallest number of channels
		ConvertInfo.NumChannels = FMath::Min(ConvertInfo.FromChannels, ConvertInfo.ToChannels);
	}

	template <typename FromType, typename ToType>
	void IUnrealAudioDeviceModule::ConvertToFloatType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo)
	{
		// Converting to float types is easy: just get the max size of the value (
		ToType* ToBufferPtr = TDataFormat<ToType>::GetDataBuffer(ToBuffer);
		FromType* FromBufferPtr = TDataFormat<FromType>::GetDataBuffer(FromBuffer);
		double MaxSize = TDataFormat<FromType>::GetMaxValue();

		double Scale = (1.0 / MaxSize);

		for (uint32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			for (uint32 ChannelIndex = 0; ChannelIndex < ConvertInfo.NumChannels; ++ChannelIndex)
			{
				ToBufferPtr[ChannelIndex] = (ToType)((FromBufferPtr[ChannelIndex] * Scale));
			}
			FromBufferPtr += ConvertInfo.FromChannels;
			ToBufferPtr += ConvertInfo.ToChannels;
		}
	}

	template <typename FloatType>
	void IUnrealAudioDeviceModule::ConvertAllToFloatType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo)
	{
		// Note: even if we're converting float-to-float we still call this because we may be performing a channel-count conversion

		switch (ConvertInfo.FromFormat)
		{
			case EStreamFormat::FLT:
				ConvertToFloatType<float, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::DBL:
				ConvertToFloatType<double, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::INT_16:
				ConvertToFloatType<int16, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::INT_24:
				ConvertToFloatType<int24, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::INT_32:
				ConvertToFloatType<int32, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			default:
				break;
		}
	}

	template <typename FromType, typename ToType>
	void IUnrealAudioDeviceModule::ConvertIntegerToIntegerType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo)
	{
		ToType* ToBufferPtr = TDataFormat<ToType>::GetDataBuffer(ToBuffer);
		FromType* FromBufferPtr = TDataFormat<FromType>::GetDataBuffer(FromBuffer);

		// Figure out the amount of bit-shifting we need to do based on the bytes counts of various formats.
		int32 ShiftAmount = 8 * ((int32)sizeof(ToType) - (int32)sizeof(FromType));

		// If our shift amount is negative (converting to a integer with fewer bytes, we'll be truncating the data
		// so we'll flip the shift amount and then right-shift the FromBuffer data, otherwise we'll shift the data to the left
		if (ShiftAmount < 0)
		{
			ShiftAmount *= -1;
			for (uint32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
			{
				for (uint32 ChannelIndex = 0; ChannelIndex < ConvertInfo.NumChannels; ++ChannelIndex)
				{
					int64 Value = FromBufferPtr[ChannelIndex];
					ToBufferPtr[ChannelIndex] = (ToType)(Value >> ShiftAmount);
				}
				FromBufferPtr += ConvertInfo.FromChannels;
				ToBufferPtr += ConvertInfo.ToChannels;
			}
		}
		else
		{
			for (uint32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
			{
				for (uint32 ChannelIndex = 0; ChannelIndex < ConvertInfo.NumChannels; ++ChannelIndex)
				{
					int64 Value = FromBufferPtr[ChannelIndex];
					ToBufferPtr[ChannelIndex] = (ToType)(Value << ShiftAmount);
				}
				FromBufferPtr += ConvertInfo.FromChannels;
				ToBufferPtr += ConvertInfo.ToChannels;
			}
		}
	}

	template <typename FromType, typename ToType>
	void IUnrealAudioDeviceModule::ConvertFloatToIntegerType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo)
	{
		ToType* ToBufferPtr = TDataFormat<ToType>::GetDataBuffer(ToBuffer);
		FromType* FromBufferPtr = TDataFormat<FromType>::GetDataBuffer(FromBuffer);
		for (uint32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			double MaxValue = TDataFormat<ToType>::GetMaxValue();
			for (uint32 ChannelIndex = 0; ChannelIndex < ConvertInfo.NumChannels; ++ChannelIndex)
			{
				ToBufferPtr[ChannelIndex] = (ToType)(FromBufferPtr[ChannelIndex] * MaxValue);
			}
			FromBufferPtr += ConvertInfo.FromChannels;
			ToBufferPtr += ConvertInfo.ToChannels;
		}
	}

	template <typename FloatType>
	void IUnrealAudioDeviceModule::ConvertAllToIntegerType(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer, uint32 NumFrames, FBufferFormatConvertInfo& ConvertInfo)
	{
		// Note: even if we're converting int32-to-int32 we still call this because we may be performing a channel-count conversion

		switch (ConvertInfo.FromFormat)
		{
			case EStreamFormat::FLT:
				ConvertFloatToIntegerType<float, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::DBL:
				ConvertFloatToIntegerType<double, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::INT_16:
				ConvertIntegerToIntegerType<int16, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::INT_24:
				ConvertIntegerToIntegerType<int24, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::INT_32:
				ConvertIntegerToIntegerType<int32, FloatType>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			default:
				break;
		}
	}

	bool IUnrealAudioDeviceModule::ConvertBufferFormat(TArray<uint8>& ToBuffer, TArray<uint8>& FromBuffer)
	{
		FBufferFormatConvertInfo& ConvertInfo = StreamInfo.DeviceInfo.BufferFormatConvertInfo;

		switch (ConvertInfo.ToFormat)
		{
			case EStreamFormat::FLT:
				ConvertAllToFloatType<float>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::DBL:
				ConvertAllToFloatType<double>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::INT_16:
				ConvertAllToIntegerType<double>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::INT_24:
				ConvertAllToIntegerType<double>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			case EStreamFormat::INT_32:
				ConvertAllToIntegerType<double>(ToBuffer, FromBuffer, StreamInfo.BlockSize, ConvertInfo);
				break;

			default:
				break;
		}

		return true;
	}

}

#endif // #if ENABLE_UNREAL_AUDIO


