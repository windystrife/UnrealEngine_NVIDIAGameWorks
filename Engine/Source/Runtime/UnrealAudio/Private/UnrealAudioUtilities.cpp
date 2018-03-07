// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioUtilities.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO

// Useful debug check macros

namespace UAudio
{
	FSafeFloat::FSafeFloat(float InitValue)
	{
		int32* IntVal = reinterpret_cast<int32*>(&InitValue);
		SafeValue.Set(*IntVal);
	}

	void FSafeFloat::Set(float Value)
	{
		int32* IntVal = reinterpret_cast<int32*>(&Value);
		SafeValue.Set(*IntVal);
	}

	float FSafeFloat::Get() const
	{
		int32 Value = SafeValue.GetValue();
		float* FloatValue = reinterpret_cast<float*>(&Value);
		return *FloatValue;
	}

	FCommandData::FCommandData()
		: DataType(ECommandData::INVALID)
	{
		Data.PtrVal = nullptr;
	}

	FCommandData::FCommandData(void* Val)
		: DataType(ECommandData::POINTER)
	{
		Data.PtrVal = Val;
	}

	FCommandData::FCommandData(const FEntityHandle& EntityHandle)
		: DataType(ECommandData::HANDLE)
	{
		Data.Handle.Id = EntityHandle.Id;
	}

	FCommandData::FCommandData(float Val)
		: DataType(ECommandData::FLOAT_32)
	{
		Data.Float32Val = Val;
	}

	FCommandData::FCommandData(double Val)
		: DataType(ECommandData::FLOAT_64)
	{
		Data.Float64Val = Val;
	}

	FCommandData::FCommandData(uint8 Val)
		: DataType(ECommandData::UINT8)
	{
		Data.UnsignedInt8 = Val;
	}

	FCommandData::FCommandData(uint32 Val)
		: DataType(ECommandData::UINT32)
	{
		Data.UnsignedInt32 = Val;
	}

	FCommandData::FCommandData(uint64 Val)
		: DataType(ECommandData::UINT64)
	{
		Data.UnsignedInt64 = Val;
	}

	FCommandData::FCommandData(bool Val)
		: DataType(ECommandData::BOOL)
	{
		Data.BoolVal = Val;
	}

	FCommandData::FCommandData(int32 Val)
		: DataType(ECommandData::INT32)
	{
		Data.Int32Val = Val;
	}

	FCommandData::FCommandData(int64 Val)
		: DataType(ECommandData::INT64)
	{
		Data.Int64Val = Val;
	}

	FCommand::FCommand()
		: Id(0)
		, NumArguments(0)
	{
	}

	FCommand::FCommand(uint32 InCommand)
		: Id(InCommand)
		, NumArguments(0)
	{
	}

	FCommand::FCommand(uint32 InCommand, const FCommandData& Arg0)
		: Id(InCommand)
		, NumArguments(1)
	{
		Arguments[0] = Arg0;
	}

	FCommand::FCommand(uint32 InCommand, const FCommandData& Arg0, const FCommandData& Arg1)
		: Id(InCommand)
		, NumArguments(2)
	{
		Arguments[0] = Arg0;
		Arguments[1] = Arg1;
	}

	FCommand::FCommand(uint32 InCommand, const FCommandData& Arg0, const FCommandData& Arg1, const FCommandData& Arg2)
		: Id(InCommand)
		, NumArguments(3)
	{
		Arguments[0] = Arg0;
		Arguments[1] = Arg1;
		Arguments[2] = Arg2;
	}

	FCommand::FCommand(uint32 InCommand, const FCommandData& Arg0, const FCommandData& Arg1, const FCommandData& Arg2, const FCommandData& Arg3)
		: Id(InCommand)
		, NumArguments(4)
	{
		Arguments[0] = Arg0;
		Arguments[1] = Arg1;
		Arguments[2] = Arg2;
		Arguments[3] = Arg3;
	}

	FCommand::FCommand(uint32 InCommand, const FCommandData& Arg0, const FCommandData& Arg1, const FCommandData& Arg2, const FCommandData& Arg3, const FCommandData& Arg4)
		: Id(InCommand)
		, NumArguments(5)
	{
		Arguments[0] = Arg0;
		Arguments[1] = Arg1;
		Arguments[2] = Arg2;
		Arguments[3] = Arg3;
		Arguments[4] = Arg4;
	}

	float FDynamicParamData::Compute(uint32 Index, float CurrentTimeSec)
	{
		float Result = 0.0f;
		if (bIsDone[Index])
		{
			Result = CurrentValue[Index];
		}
		else
		{
			float Fraction = (float)FMath::Min((CurrentTimeSec - StartTime[Index]) / DeltaTime[Index], 1.0);
			bIsDone[Index] = Fraction >= 1.0f;

			// LERP the volume parameter
			// TODO: other interpolations
			Result = (1.0f - Fraction) * StartValue[Index] + Fraction * EndValue[Index];
			CurrentValue[Index] = Result;
		}
		return Result;
	}

	void FDynamicParamData::Init(uint32 NumElements)
	{
		StartValue.Init(1.0f, NumElements);
		EndValue.Init(1.0f, NumElements);
		CurrentValue.Init(1.0f, NumElements);
		StartTime.Init(0.0f, NumElements);
		DeltaTime.Init(0.0f, NumElements);
		bIsDone.Init(true, NumElements);
	}

	void FDynamicParamData::InitEntry(uint32 Index)
	{
		DEBUG_AUDIO_CHECK(Index < (uint32)StartValue.Num());

		StartValue[Index] = 1.0f;
		EndValue[Index] = 1.0f;
		CurrentValue[Index] = 1.0f;
		StartTime[Index] = 0.0f;
		DeltaTime[Index] = 0.0f;
		bIsDone[Index] = true;
	}

	void FDynamicParamData::SetValue(uint32 Index, float InValue, float InStartTime, float InDeltaTimeSec)
	{
		if (InValue != CurrentValue[Index])
		{
			StartValue[Index] = CurrentValue[Index];
			EndValue[Index] = InValue;
			StartTime[Index] = InStartTime;
			DeltaTime[Index] = InDeltaTimeSec;
			bIsDone[Index] = false;
		}
	}


}

#endif // #if ENABLE_UNREAL_AUDIO


