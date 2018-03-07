// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Factories/IESLoader.h"
#include "Math/RandomStream.h"

enum EIESVersion
{
	EIESV_1986,	// IES LM-63-1986
	EIESV_1991, // IES LM-63-1991
	EIESV_1995, // IES LM-63-1995
	EIESV_2002, // IES LM-63-2002
};

DEFINE_LOG_CATEGORY_STATIC(LogIESLoader, Log, All);


// space and return
static void JumpOverWhiteSpace(const uint8*& BufferPos)
{
	while(*BufferPos)
	{
		if(*BufferPos == 13 && *(BufferPos + 1) == 10)
		{
			BufferPos += 2;
			continue;
		}
		else if(*BufferPos == 10)
		{
			// no valid MSDOS return file
			check(0);
		}
		else if(*BufferPos <= ' ')
		{
			// tab, space, invisible characters
			++BufferPos;
			continue;
		}

		break;
	}
}

static void GetLineContent(const uint8*& BufferPos, char Line[256], bool bStopOnWhitespace)
{
	JumpOverWhiteSpace(BufferPos);

	char* LinePtr = Line;

	uint32 i;

	for(i = 0; i < 255; ++i)
	{
		if(*BufferPos == 0)
		{
			break;
		}
		else if(*BufferPos == 13 && *(BufferPos + 1) == 10)
		{
			BufferPos += 2;
			break;
		}
		else if(*BufferPos == 10)
		{
			// no valid MSDOS return file
			check(0);
		}
		else if(bStopOnWhitespace && (*BufferPos <= ' '))
		{
			// tab, space, invisible characters
			++BufferPos;
			break;
		}

		*LinePtr++ = *BufferPos++;
	}

	Line[i] = 0;
}


// @return success
static bool GetFloat(const uint8*& BufferPos, float& ret)
{
	char Line[256];

	GetLineContent(BufferPos, Line, true);

	ret = FCStringAnsi::Atof(Line);
	return true;
}


static bool GetInt(const uint8*& BufferPos, int32& ret)
{
	char Line[256];

	GetLineContent(BufferPos, Line, true);

	ret = FCStringAnsi::Atoi(Line);
	return true;
}



FIESLoadHelper::FIESLoadHelper(const uint8* Buffer, uint32 BufferLength)
	: Brightness(0)
	, CachedIntegral(MAX_FLT)
	, Error("No data loaded")
{
	check(!IsValid());

	TArray<uint8> ASCIIFile;

	// add 0 termination for easier parsing
	{
		ASCIIFile.AddUninitialized(BufferLength + 1);
		FMemory::Memcpy(ASCIIFile.GetData(), Buffer, BufferLength);
		ASCIIFile[BufferLength] = 0;
	}

	Load(ASCIIFile.GetData());
}


#define PARSE_FLOAT(x) float x; if(!GetFloat(BufferPos, x)) return
#define PARSE_INT(x) int32 x; if(!GetInt(BufferPos, x)) return

void FIESLoadHelper::Load(const uint8* Buffer)
{
	// file format as described here: http://www.ltblight.com/English.lproj/LTBLhelp/pages/iesformat.html

	const uint8* BufferPos = Buffer;

	EIESVersion Version = EIESV_1986;
	{
		Error = "VersionError";

		char Line1[256];

		GetLineContent(BufferPos, Line1, false);

		if(FCStringAnsi::Stricmp(Line1, "IESNA:LM-63-1995") == 0)
		{
			Version = EIESV_1995;
		}
		else if(FCStringAnsi::Stricmp(Line1, "IESNA91") == 0)
		{
			Version = EIESV_1991;
		}
		else if(FCStringAnsi::Stricmp(Line1, "IESNA:LM-63-2002") == 0)
		{
			Version = EIESV_2002;
		}
		else
		{
			Version = EIESV_1986;

			// Return buffer to start of file since line read was not the version
			BufferPos = Buffer;
		}
	}

	Error = "HeaderError";

	while(*BufferPos)
	{
		char Line[256];

		GetLineContent(BufferPos, Line, false);

		if(FCStringAnsi::Strcmp(Line, "TILT=NONE") == 0)
		{
			// at the moment we don't support only profiles with TILT=NONE
			break;
		}
		else if(FCStringAnsi::Strncmp(Line, "TILT=", 5) == 0)
		{
			// "TILT=NONE", "TILT=INCLUDE", and "TILT={filename}"
			// not supported yet, seems rare
			return;
		}
	}

	Error = "HeaderParameterError";

	PARSE_INT(LightCount);

	if(LightCount < 1)
	{
		Error = "Light count needs to be positive.";
		return;
	}

	// if there is any file with that - do we need to parse it differently?
	check(LightCount >= 1);

	PARSE_FLOAT(LumensPerLamp);

	Brightness = LumensPerLamp / LightCount;

	PARSE_FLOAT(CandalaMult);

	if(CandalaMult < 0)
	{
		Error = "CandalaMult is negative";
		return;
	}

	PARSE_INT(VAnglesNum);
	PARSE_INT(HAnglesNum);

	if(VAnglesNum < 0)
	{
		Error = "VAnglesNum is not valid";
		return;
	}

	if(HAnglesNum < 0)
	{
		Error = "HAnglesNum is not valid";
		return;
	}

	PARSE_INT(PhotometricType);

	// 1:feet, 2:meter
	PARSE_INT(UnitType);

	PARSE_FLOAT(Width);
	PARSE_FLOAT(Length);
	PARSE_FLOAT(Height);

	PARSE_FLOAT(BallastFactor);
	PARSE_FLOAT(FutureUse);

//	check(FutureUse == 1.0f);

	PARSE_FLOAT(InputWatts);

	Error = "ContentError";

	{
		float MinSoFar = -FLT_MAX;

		VAngles.Empty(VAnglesNum);
		for(uint32 y = 0; y < (uint32)VAnglesNum; ++y)
		{
			PARSE_FLOAT(Value);

			if(Value < MinSoFar)
			{
				// binary search later relies on that
				Error = "V Values are not in increasing order";
				return;
			}

			MinSoFar = Value;
			VAngles.Add(Value);
		}
	}

	{
		float MinSoFar = -FLT_MAX;

		HAngles.Empty(HAnglesNum);
		for(uint32 x = 0; x < (uint32)HAnglesNum; ++x)
		{
			PARSE_FLOAT(Value);

			if(Value < MinSoFar)
			{
				// binary search later relies on that
				Error = "H Values are not in increasing order";
				return;
			}

			MinSoFar = Value;
			HAngles.Add(Value);
		}
	}

	CandalaValues.Empty(HAnglesNum * VAnglesNum);
	for(uint32 y = 0; y < (uint32)HAnglesNum; ++y)
	{
		for(uint32 x = 0; x < (uint32)VAnglesNum; ++x)
		{
			PARSE_FLOAT(Value);

			CandalaValues.Add(Value * CandalaMult);
		}
	}

	Error = "Unexpected content after candala values.";

	JumpOverWhiteSpace(BufferPos);

	if(*BufferPos)
	{
		// some files are terminated with "END"
		char Line[256];

		GetLineContent(BufferPos, Line, true);

		if(FCStringAnsi::Strcmp(Line, "END") == 0)
		{
			JumpOverWhiteSpace(BufferPos);
		}
	}

	if(*BufferPos)
	{
		Error = "Unexpected content after END.";
		return;
	}

	Error = 0;

	if(Brightness <= 0)
	{
		// some samples have -1, then the brightness comes from the samples
//		Brightness = ComputeFullIntegral();

		// use some reasonable value
		Brightness = 1000;
	}
}
#undef PARSE_FLOAT
#undef PARSE_INT

uint32 FIESLoadHelper::GetWidth() const
{
	return 256;
}

uint32 FIESLoadHelper::GetHeight() const
{
	return 1;
}

bool FIESLoadHelper::IsValid() const
{
	return Error == 0;
}

float FIESLoadHelper::GetBrightness() const
{
	return Brightness;
}

float FIESLoadHelper::ExtractInRGBA16F(TArray<uint8>& OutData)
{
	check(IsValid());

	uint32 Width = GetWidth();
	uint32 Height = GetHeight();

	check(!OutData.Num());
	OutData.AddZeroed(Width * Height * sizeof(FFloat16Color));

	FFloat16Color* Out = (FFloat16Color*)OutData.GetData();

	float InvWidth = 1.0f / Width;
	float MaxValue = ComputeMax();
	float InvMaxValue= 1.0f / MaxValue;

	for(uint32 y = 0; y < Height; ++y)
	{
		for(uint32 x = 0; x < Width; ++x)
		{
			// 0..1
			float Fraction = x * InvWidth;

			// distort for better quality?
//				Fraction = Square(Fraction);

			float FloatValue = InvMaxValue * Interpolate1D(Fraction * 180.0f);

			{
				FFloat16 HalfValue(FloatValue);

				FFloat16Color HalfColor;

				HalfColor.R = HalfValue;
				HalfColor.G = HalfValue;
				HalfColor.B = HalfValue;
				HalfColor.A = HalfValue;

				*Out++ = HalfColor;
			}
		}
	}

	float Integral = ComputeFullIntegral();

	return MaxValue / Integral;
}

float FIESLoadHelper::ComputeFullIntegral()
{
	// compute only if needed
	if(CachedIntegral == MAX_FLT)
	{
		// monte carlo integration
		// if quality is a problem we can improve on this algorithm or increase SampleCount

		// larger number costs more time but improves quality
		uint32 SampleCount = 1000000;

		FRandomStream RandomStream(0x1234);

		double Sum = 0;
		for(uint32 i = 0; i < SampleCount; ++i)
		{
			FVector Dir = RandomStream.GetUnitVector();

			// http://en.wikipedia.org/wiki/Spherical_coordinate_system

			// 0..180
			float HAngle = FMath::Acos(Dir.Z) / PI * 180;
			// 0..360
			float VAngle = FMath::Atan2(Dir.Y, Dir.X) / PI * 180 + 180;

			check(HAngle >= 0 && HAngle <= 180);
			check(VAngle >= 0 && VAngle <= 360);

			Sum += Interpolate2D(HAngle, VAngle);
		}

		CachedIntegral = Sum / SampleCount;
	}

	return CachedIntegral;
}



float FIESLoadHelper::ComputeMax() const
{
	float ret = 0.0f;

	for(uint32 i = 0; i < (uint32)CandalaValues.Num(); ++i)
	{
		float Value = CandalaValues[i];

		ret = FMath::Max(ret, Value);
	}

	return ret;
}


float FIESLoadHelper::ComputeFilterPos(float Value, const TArray<float>& SortedValues)
{
	check(SortedValues.Num());

	uint32 StartPos = 0;
	uint32 EndPos = SortedValues.Num() - 1;

	if(Value < SortedValues[StartPos])
	{
		return 0.0f;
	}

	if(Value > SortedValues[EndPos])
	{
		return (float)EndPos;
	}

	// binary search
	while(StartPos < EndPos)
	{
		uint32 TestPos = (StartPos + EndPos + 1) / 2;

		float TestValue = SortedValues[TestPos];

		if(Value >= TestValue)
		{
			// prevent endless loop
			check(StartPos != TestPos);

			StartPos = TestPos;
		}
		else
		{
			// prevent endless loop
			check(EndPos != TestPos - 1);

			EndPos = TestPos - 1;
		}
	}

	float LeftValue = SortedValues[StartPos];

	float Fraction = 0.0f;

	if(StartPos + 1 < (uint32)SortedValues.Num())
	{
		// if not at right border
		float RightValue = SortedValues[StartPos + 1];
		float DeltaValue = RightValue - LeftValue;

		if(DeltaValue > 0.0001f)
		{
			Fraction = (Value - LeftValue) / DeltaValue;
		}
	}

	return StartPos + Fraction;
}


float FIESLoadHelper::InterpolatePoint(int X, int Y) const
{
	uint32 HAnglesNum = HAngles.Num();
	uint32 VAnglesNum = VAngles.Num();

	check(X >= 0);
	check(Y >= 0);

	X %= HAnglesNum;
	Y %= VAnglesNum;

	check(X < (int)HAnglesNum);
	check(Y < (int)VAnglesNum);

	return CandalaValues[Y + VAnglesNum * X];
}


float FIESLoadHelper::InterpolateBilinear(float fX, float fY) const
{
	int X = (int)fX;
	int Y = (int)fY;

	float fracX = fX - X;
	float fracY = fY - Y;

	float p00 = InterpolatePoint(X + 0, Y + 0);
	float p10 = InterpolatePoint(X + 1, Y + 0);
	float p01 = InterpolatePoint(X + 0, Y + 1);
	float p11 = InterpolatePoint(X + 1, Y + 1);
	
	float p0 = FMath::Lerp(p00, p01, fracY); 
	float p1 = FMath::Lerp(p10, p11, fracY); 

	return FMath::Lerp(p0, p1, fracX);
}

float FIESLoadHelper::Interpolate2D(float HAngle, float VAngle) const
{
	float u = ComputeFilterPos(HAngle, HAngles);
	float v = ComputeFilterPos(VAngle, VAngles);

	return InterpolateBilinear(u, v);
}

float FIESLoadHelper::Interpolate1D(float VAngle) const
{
	float v = ComputeFilterPos(VAngle, VAngles);

	float ret = 0.0f;

	uint32 HAnglesNum = (uint32)HAngles.Num();

	for(uint32 i = 0; i < HAnglesNum; ++i)
	{
		ret += InterpolateBilinear((float)i, v);
	}

	return ret / HAnglesNum;
}
