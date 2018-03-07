///////////////////////////////////////////////////////////////////////  
//	Timer.h
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		http://www.idvinc.com


///////////////////////////////////////////////////////////////////////
// Preprocessor

#pragma once
#if defined(WIN32) && !defined(_WINDOWS_)
	#include <windows.h>
#endif
#if defined(_XBOX)
	#include <Xtl.h>
#elif defined(_DURANGO)
	#include <wrl.h>
#elif defined(__CELLOS_LV2__)
	#include <sys/sys_time.h>
#elif defined(__psp2__)
	#include <stdint.h>
	#include <kernel.h>
#elif defined(NDEV)
	#include <cafe/os.h>
#elif defined(__GNUC__)
	#include <sys/time.h>
#endif
#include "Core/ExportBegin.h"
#include "Core/Types.h"


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	//	class CTimer
	/// CTimer is used like a stopwatch timer and can return the time in various units.

	class ST_DLL_LINK CTimer
	{
	public:
								CTimer(st_bool bStart = false);

			st_float32			Stop(void) const;
			void				Start(void);

			st_float32			GetSec(void) const;
			st_float32			GetMilliSec(void) const;
			st_float32			GetMicroSec(void) const;

	private:
	#if defined(WIN32) || defined(_XBOX) || defined(_DURANGO)
			LARGE_INTEGER		m_liFreq;
			LARGE_INTEGER		m_liStart;
	mutable	LARGE_INTEGER		m_liStop;
	#endif
	#if defined(__CELLOS_LV2__)
			sys_time_sec_t		m_tStartSeconds;
			sys_time_nsec_t		m_tStartNanoseconds;
	mutable sys_time_sec_t		m_tStopSeconds;
	mutable sys_time_nsec_t		m_tStopNanoseconds;
	#elif defined(__psp2__)
			uint64_t			m_uiStart;
	mutable	uint64_t			m_uiStop;
	#elif defined(NDEV)
			OSTick				m_tStart;
	mutable	OSTick				m_tStop;
	#elif defined(__GNUC__)
			timeval				m_tStart;
	mutable	timeval				m_tStop;
	#endif
	};


	///////////////////////////////////////////////////////////////////////  
	//	CTimer::CTimer
	/// CTimer constructor. If \a bStart is true, the timer will start by calling Start() internally.

	inline CTimer::CTimer(st_bool bStart)
	{
		#if defined(WIN32) || defined(_XBOX) || defined(_DURANGO)
			(void) QueryPerformanceFrequency(&m_liFreq);
			m_liStart.QuadPart = 0;
			m_liStop.QuadPart = 0;
		#endif

		if (bStart)
			Start( );
	}


	///////////////////////////////////////////////////////////////////////  
	//	CTimer::Start
	/// Starts the timer

	inline void CTimer::Start(void)
	{
		#if defined(WIN32) || defined(_XBOX)
			m_liStart.QuadPart = 0;
			QueryPerformanceCounter(&m_liStart);
		#endif
		#if defined(__CELLOS_LV2__)
			sys_time_get_current_time(&m_tStartSeconds, &m_tStartNanoseconds);
		#elif defined(__psp2__)
			m_uiStart = sceKernelGetProcessTimeWide( );
		#elif defined(NDEV)
			m_tStart = OSGetTick( );
		#elif defined(__GNUC__)
			gettimeofday(&m_tStart, NULL);
		#endif
	}


	///////////////////////////////////////////////////////////////////////  
	//	CTimer::Stop
	/// Stops the timer and returns the milliseconds elapsed since it was started.

	inline st_float32 CTimer::Stop(void) const
	{
		#if defined(WIN32) || defined(_XBOX)
			QueryPerformanceCounter(&m_liStop);
		#endif 
		#if defined(__CELLOS_LV2__)
			sys_time_get_current_time(&m_tStopSeconds, &m_tStopNanoseconds);
		#elif defined(__psp2__)
			m_uiStop = sceKernelGetProcessTimeWide( );
		#elif defined(NDEV)
			m_tStop = OSGetTick( );
		#elif defined(__GNUC__)
			gettimeofday(&m_tStop, NULL);
		#endif

		return GetMilliSec( );
	}


	///////////////////////////////////////////////////////////////////////  
	//	CTimer::GetMicroSec
	/// Returns the elapsed microseconds between the last Start() and Stop()

	inline st_float32 CTimer::GetMicroSec(void) const
	{
		return GetSec( ) * 1.0e6f;
	}


	///////////////////////////////////////////////////////////////////////  
	//	CTimer::GetMilliSec
	/// Returns the elapsed milliseconds between the last Start() and Stop()

	inline st_float32 CTimer::GetMilliSec(void) const
	{
		return GetSec( ) * 1.0e3f;
	}


	///////////////////////////////////////////////////////////////////////  
	//	CTimer::GetSec
	/// Returns the elapsed seconds between the last Start() and Stop()

	inline st_float32 CTimer::GetSec(void) const
	{
		#if defined(WIN32) || defined(_XBOX) || defined(_DURANGO)
			return (m_liStop.QuadPart - m_liStart.QuadPart) / st_float32(m_liFreq.QuadPart);
		#endif
		#if defined(__CELLOS_LV2__)
			return (m_tStopSeconds - m_tStartSeconds) + (m_tStopNanoseconds - m_tStartNanoseconds) / 1.0e9f;
		#elif defined(__psp2__)
			return (m_uiStop - m_uiStart) / 1.0e6f;
		#elif defined(NDEV)
			return OSTicksToSeconds(m_tStop - m_tStart);
		#elif defined(__GNUC__)
			return (m_tStop.tv_sec - m_tStart.tv_sec) + (m_tStop.tv_usec - m_tStart.tv_usec) / 1.0e6f;
		#endif
	}

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

