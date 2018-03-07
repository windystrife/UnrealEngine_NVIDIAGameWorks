// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProfilerSample.h"


struct FProfilerFPSChartEntry
{
	int32 Count;
	double CummulativeTime;
};


enum FPSChartBins
{
	FPSBin_0_5,
	FPSBin_5_10,
	FPSBin_10_15,
	FPSBin_15_20,
	FPSBin_20_25,
	FPSBin_25_30,
	FPSBin_30_35,
	FPSBin_35_40,
	FPSBin_40_45,
	FPSBin_45_50,
	FPSBin_50_55,
	FPSBin_55_60,
	FPSBin_60_65,
	FPSBin_65_70,
	FPSBin_70_75,
	FPSBin_75_80,
	FPSBin_80_85,
	FPSBin_85_90,
	FPSBin_90_INF,
	FPSBin_LastBucketStat,

};


/** Implements a frame rate analyzer */
class FFPSAnalyzer
	: public IHistogramDataSource
{
public:

	FFPSAnalyzer(int32 InInterval, int32 InMinVal, int32 InMaxVal)
	{
		Interval = InInterval;
		MaxVal = InMaxVal;
		MinVal = InMinVal;
		Reset();
	}

	virtual ~FFPSAnalyzer() { }
	virtual int32 GetCount(float MinVal, float MaxVal) override;
	virtual int32 GetTotalCount() override;

	void Reset();
	void AddSample(float FPSSample);
	SIZE_T GetMemoryUsage() const;

public:

	TArray<float> Samples;
	TArray<FProfilerFPSChartEntry> Histogram;
	float MinFPS;
	float MaxFPS;
	float AveFPS;
	int32 FPS90;
	int32 FPS60;
	int32 FPS30;
	int32 FPS25;
	int32 FPS20;
	int32 Interval;
	int32 MaxVal;
	int32 MinVal;
};
