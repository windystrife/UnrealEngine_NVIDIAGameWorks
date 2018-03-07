#include "PerformanceTimer.h"
#include <iostream>
#include <algorithm>
#include <cmath>

void PerformanceTimer::Begin(int loops)
{
	mFrameTimes.clear();
	mFrameTimes.reserve(loops+1);
	mStart = std::chrono::steady_clock::now();
	mFrameTimes.push_back(mStart);
	mLoops = 0;
}

void PerformanceTimer::FrameEnd()
{
	mLoops++;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	mFrameTimes.push_back(now);
}

void PerformanceTimer::End()
{
	mEnd = std::chrono::steady_clock::now();

	std::chrono::duration<double> average = std::chrono::duration<double>::zero();
	double min = 999999999999999.0;
	double max = -1.0;
	for(int i = 1; i<mFrameTimes.size(); i++)
	{
		std::chrono::duration<double> frameTime = std::chrono::duration_cast<std::chrono::microseconds>(mFrameTimes[i]-mFrameTimes[i-1]); 
		min = std::min(frameTime.count(),min);
		max = std::max(frameTime.count(),max);
		average+=frameTime;
	}
	average/=mLoops;

	std::cout<<"Avg:"<<average.count()*1000.0<<"ms/frame"<<std::endl;
	double variance = 0.0;
	for(int i = 1; i<mFrameTimes.size(); i++)
	{
		std::chrono::duration<double> frameTime = std::chrono::duration_cast<std::chrono::microseconds>(mFrameTimes[i]-mFrameTimes[i-1]); 
		variance += (frameTime-average).count()*(frameTime-average).count();
	}
	variance/=mLoops;
	std::cout<<"StdDeviation:"<<sqrt(variance)*1000.0<<"ms/frame"<<std::endl;
	std::cout<<"Min:"<<min*1000.0<<"ms/frame \tMax:"<<max*1000.0<<"ms/frame"<<std::endl;
	std::chrono::duration<double> total = std::chrono::duration_cast<std::chrono::microseconds>(mEnd-mStart);
	std::cout<<"Total:"<<total.count()*1000.0<<"ms"<<std::endl;

#ifdef _MSC_VER
#if _MSC_VER < 1900
	std::cout<<"Note: Timings in visual studio 2013 and earlier are not precise. Use 2015 or later for better measurements"<<std::endl;
#endif
#endif
#ifdef _DEBUG
	std::cout<<"Warning: You are profiling a debug build."<<std::endl;
#endif
}
