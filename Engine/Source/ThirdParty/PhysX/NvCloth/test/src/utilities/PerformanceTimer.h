#pragma once
#include <chrono>
#include <vector>

class PerformanceTimer
{
	public:
		void Begin(int loops);
		void FrameEnd();
		void End();

	private:
		std::chrono::steady_clock::time_point mStart;
		std::chrono::steady_clock::time_point mEnd;
		std::vector<std::chrono::steady_clock::time_point> mFrameTimes;
		int mLoops;
};