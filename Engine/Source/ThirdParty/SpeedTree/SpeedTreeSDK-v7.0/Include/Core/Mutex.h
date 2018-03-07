///////////////////////////////////////////////////////////////////////
//  Mutex.h
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with
//  the terms of that agreement.
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com


///////////////////////////////////////////////////////////////////////
//  Preprocessor

#pragma once
#include "Core/Core.h"

#ifdef _XBOX
	#include <xtl.h>
#elif defined(_WIN32)
	#include <windows.h>
#elif defined(__CELLOS_LV2__)
	#include <sys/synchronization.h>
	#include <pthread.h>
	//#define SPEEDTREE_USE_LIGHTWEIGHT_MUTEX
#elif defined(__psp2__)
	#include <kernel/threadmgr.h>
#elif defined(__APPLE__) || defined(__GNUC__)
	#include <pthread.h>
#elif defined(_OPENMP)
	#include <omp.h>
#elif defined(NDEV)
	#include <cafe/os.h>
#else
	#error SpeedTree CMutex not yet defined for this platform
#endif


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
	//  Class CMutex

	class ST_DLL_LINK CMutex
	{
	public:
									CMutex( );
									~CMutex( );

			void					Lock(void);
			void					Unlock(void);
	private:
		#ifdef _WIN32 // Windows or Xbox 360
			CRITICAL_SECTION		m_sLock;
		#elif defined(__CELLOS_LV2__)
			#ifdef SPEEDTREE_USE_LIGHTWEIGHT_MUTEX
				sys_lwmutex_t		m_sLock;
			#else
				pthread_mutex_t		m_sLock;
			#endif
		#elif defined (__psp2__)
			SceUID					m_sLock;
		#elif defined(__APPLE__) || defined(__GNUC__)
			pthread_mutex_t			m_sLock;
		#elif defined(_OPENMP)
			omp_lock_t				m_sLock;
		#elif defined(NDEV)
			OSMutex					m_sLock;
		#endif
	};


	///////////////////////////////////////////////////////////////////////
	//  CMutex::CMutex

	inline CMutex::CMutex( )
	{
		#ifdef _WIN32 // Windows or Xbox 360
			InitializeCriticalSection(&m_sLock);
		#elif defined(__CELLOS_LV2__)
			#ifdef SPEEDTREE_USE_LIGHTWEIGHT_MUTEX
				sys_lwmutex_attribute_t sAttribute;
				sys_lwmutex_attribute_initialize(sAttribute);
				assert(sys_lwmutex_create(&m_sLock, &sAttribute) == CELL_OK);
			#else
				pthread_mutexattr_t sAttribute;
				pthread_mutexattr_init(&sAttribute);
				assert(pthread_mutex_init(&m_sLock, &sAttribute) == CELL_OK);
			#endif
		#elif defined(__psp2__)
			m_sLock = sceKernelCreateMutex("SpeedTree", SCE_KERNEL_MUTEX_ATTR_TH_PRIO, 0, NULL);
		#elif defined(__APPLE__) || defined(__GNUC__)
			pthread_mutex_init(&m_sLock, NULL);
		#elif defined(_OPENMP)
			omp_init_lock(&m_sLock);
		#elif defined(NDEV)
			OSInitMutex(&m_sLock);
		#endif
	}


	///////////////////////////////////////////////////////////////////////
	//  CMutex::~CMutex

	inline CMutex::~CMutex( )
	{
		#ifdef _WIN32 // Windows or Xbox 360
			DeleteCriticalSection(&m_sLock);
		#elif defined(__CELLOS_LV2__)
			#ifdef SPEEDTREE_USE_LIGHTWEIGHT_MUTEX
				assert(sys_lwmutex_destroy(&m_sLock) == CELL_OK);
			#else
				assert(pthread_mutex_destroy(&m_sLock) == CELL_OK);
			#endif
		#elif defined(__psp2__)
			sceKernelDeleteMutex(m_sLock);
		#elif defined(__APPLE__) || defined(__GNUC__)
			pthread_mutex_destroy(&m_sLock);
		#elif defined(_OPENMP)
			omp_destroy_lock(&m_sLock);
		#elif defined(NDEV)
			// do nothing
		#endif
	}


	///////////////////////////////////////////////////////////////////////
	//  CMutex::Lock

	inline void CMutex::Lock(void)
	{
		#ifdef _WIN32 // Windows or Xbox 360
			EnterCriticalSection(&m_sLock);
		#elif defined(__CELLOS_LV2__)
			#ifdef SPEEDTREE_USE_LIGHTWEIGHT_MUTEX
				assert(sys_lwmutex_lock(&m_sLock, SYS_NO_TIMEOUT) == CELL_OK);
			#else
				assert(pthread_mutex_lock(&m_sLock) == CELL_OK);
			#endif
		#elif defined(__psp2__)
			sceKernelLockMutex(m_sLock, 1, NULL);
		#elif defined(__APPLE__) || defined(__GNUC__)
			pthread_mutex_lock(&m_sLock);
		#elif defined(_OPENMP)
			omp_set_lock(&m_sLock);
		#elif defined(NDEV)
			OSLockMutex(&m_sLock);
		#endif
	}


	///////////////////////////////////////////////////////////////////////
	//  CMutex::Unlock

	inline void CMutex::Unlock(void)
	{
		#ifdef _WIN32 // Windows or Xbox 360
			LeaveCriticalSection(&m_sLock);
		#elif defined(__CELLOS_LV2__)
			#ifdef SPEEDTREE_USE_LIGHTWEIGHT_MUTEX
				assert(sys_lwmutex_unlock(&m_sLock) == CELL_OK);
			#else
				assert(pthread_mutex_unlock(&m_sLock) == CELL_OK);
			#endif
		#elif defined(__psp2__)
			sceKernelUnlockMutex(m_sLock, 1);
		#elif defined(__APPLE__) || defined(__GNUC__)
			pthread_mutex_unlock(&m_sLock);
		#elif defined(_OPENMP)
			omp_unset_lock(&m_sLock);
		#elif defined(NDEV)
			OSUnlockMutex(&m_sLock);
		#endif
	}

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif
