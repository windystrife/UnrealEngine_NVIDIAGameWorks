// This code contains NVIDIA Confidential Information and is disclosed 
// under the Mutual Non-Disclosure Agreement. 
// 
// Notice 
// ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES 
// NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO 
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT, 
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 
// 
// NVIDIA Corporation assumes no responsibility for the consequences of use of such 
// information or for any infringement of patents or other rights of third parties that may 
// result from its use. No license is granted by implication or otherwise under any patent 
// or patent rights of NVIDIA Corporation. No third party distribution is allowed unless 
// expressly authorized by NVIDIA.  Details are subject to change without notice. 
// This code supersedes and replaces all information previously supplied. 
// NVIDIA Corporation products are not authorized for use as critical 
// components in life support devices or systems without express written approval of 
// NVIDIA Corporation. 
// 
// Copyright © 2008- 2013 NVIDIA Corporation. All rights reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and proprietary
// rights in and to this software and related documentation and any modifications thereto.
// Any use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation is
// strictly prohibited.
//

/*
 * WaveWorks is a library for simulating terrain water surfaces, such as lakes and oceans, using the GPU.
 * The library includes shader fragments which can be used to reference the results of the simulation.
 *
 * See documentation for details
 */

#ifndef _GFSDK_WAVEWORKS_H
#define _GFSDK_WAVEWORKS_H

#include <GFSDK_WaveWorks_Common.h>
#include <GFSDK_WaveWorks_Types.h>
#include <GFSDK_WaveWorks_GUID.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
  Preamble
  ===========================================================================*/

#ifndef GFSDK_WAVEWORKS_LINKAGE
    #ifndef GFSDK_WAVEWORKS_DLL
    #define GFSDK_WAVEWORKS_DLL 0
    #endif
    #if GFSDK_WAVEWORKS_DLL
    #define GFSDK_WAVEWORKS_LINKAGE __GFSDK_IMPORT__
    #else
    #define GFSDK_WAVEWORKS_LINKAGE
    #endif
#endif

#define GFSDK_WAVEWORKS_DECL(ret_type) GFSDK_WAVEWORKS_LINKAGE ret_type GFSDK_WAVEWORKS_CALL_CONV


/*===========================================================================
  Memory management definitions
  ===========================================================================*/

#ifndef GFSDK_WAVEWORKS_MALLOC_HOOKS_DEFINED
#define GFSDK_WAVEWORKS_MALLOC_HOOKS_DEFINED

typedef void* (GFSDK_WAVEWORKS_CALL_CONV *GFSDK_WAVEWORKS_MALLOC) (size_t size);
typedef void (GFSDK_WAVEWORKS_CALL_CONV *GFSDK_WAVEWORKS_FREE) (void *p);
typedef void* (GFSDK_WAVEWORKS_CALL_CONV *GFSDK_WAVEWORKS_ALIGNED_MALLOC) (size_t size, size_t alignment);
typedef void (GFSDK_WAVEWORKS_CALL_CONV *GFSDK_WAVEWORKS_ALIGNED_FREE) (void *p);

struct GFSDK_WaveWorks_Malloc_Hooks
{
	GFSDK_WAVEWORKS_MALLOC pMalloc;
	GFSDK_WAVEWORKS_FREE pFree;
	GFSDK_WAVEWORKS_ALIGNED_MALLOC pAlignedMalloc;
	GFSDK_WAVEWORKS_ALIGNED_FREE pAlignedFree;
};

#endif


/*===========================================================================
  Globals/init
  ===========================================================================*/

GFSDK_WAVEWORKS_DECL(gfsdk_cstr) GFSDK_WaveWorks_GetBuildString();

// Use these calls to globally initialize/release on D3D device create/destroy.
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_InitNoGraphics(const GFSDK_WaveWorks_Malloc_Hooks* pOptionalMallocHooks, const GFSDK_WaveWorks_API_GUID& apiGUID);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_ReleaseNoGraphics();
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_InitD3D9(IDirect3DDevice9* pD3DDevice, const GFSDK_WaveWorks_Malloc_Hooks* pOptionalMallocHooks, const GFSDK_WaveWorks_API_GUID& apiGUID);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_ReleaseD3D9(IDirect3DDevice9* pD3DDevice);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_InitD3D10(ID3D10Device* pD3DDevice, const GFSDK_WaveWorks_Malloc_Hooks* pOptionalMallocHooks, const GFSDK_WaveWorks_API_GUID& apiGUID);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_ReleaseD3D10(ID3D10Device* pD3DDevice);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_InitD3D11(ID3D11Device* pD3DDevice, const GFSDK_WaveWorks_Malloc_Hooks* pOptionalMallocHooks, const GFSDK_WaveWorks_API_GUID& apiGUID);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_ReleaseD3D11(ID3D11Device* pD3DDevice);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_InitGL2(const GFSDK_WAVEWORKS_GLFunctions* pGLFuncs, const GFSDK_WaveWorks_Malloc_Hooks* pOptionalMallocHooks, const GFSDK_WaveWorks_API_GUID& apiGUID);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_ReleaseGL2();

// Utility function to test whether a given GL attrib is a match for a given shader input, based on name
GFSDK_WAVEWORKS_DECL(gfsdk_bool) GFSDK_WaveWorks_GLAttribIsShaderInput(gfsdk_cstr attribName, const GFSDK_WaveWorks_ShaderInput_Desc& inputDesc);


/*===========================================================================
  Save/restore of graphics device state
  ===========================================================================*/

// In order to preserve D3D state across certain calls, create a save-state object, pass it to the call
// and then once the call is done, use it to restore the previous D3D state
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_Savestate_CreateD3D9(GFSDK_WaveWorks_StatePreserveFlags PreserveFlags, IDirect3DDevice9* pD3DDevice, GFSDK_WaveWorks_SavestateHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_Savestate_CreateD3D10(GFSDK_WaveWorks_StatePreserveFlags PreserveFlags, ID3D10Device* pD3DDevice, GFSDK_WaveWorks_SavestateHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_Savestate_CreateD3D11(GFSDK_WaveWorks_StatePreserveFlags PreserveFlags, ID3D11Device* pD3DDevice, GFSDK_WaveWorks_SavestateHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_Savestate_RestoreD3D9(GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_Savestate_RestoreD3D10(GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_Savestate_RestoreD3D11(GFSDK_WaveWorks_SavestateHandle hSavestate, ID3D11DeviceContext* pDC);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result) GFSDK_WaveWorks_Savestate_Destroy(GFSDK_WaveWorks_SavestateHandle hSavestate);


/*===========================================================================
  Simulation
  ===========================================================================*/

struct GFSDK_WaveWorks_Simulation_Params
{
    // Global scale factor for simulated wave amplitude
    gfsdk_F32 wave_amplitude;
	// The direction of the wind inducing the waves
    gfsdk_float2 wind_dir;
	// The speed of the wind inducing the waves. If GFSDK_WaveWorks_Simulation_Settings.UseBeaufortScale is set, this is
	// interpreted as a Beaufort scale value. Otherwise, it is interpreted as metres per second
    gfsdk_F32 wind_speed;
	// The degree to which waves appear to move in the wind direction (vs. standing waves), in the [0,1] range
    gfsdk_F32 wind_dependency;
	// In addition to height displacements, the simulation also applies lateral displacements. This controls the non-linearity
	// and therefore 'choppiness' in the resulting wave shapes. Should normally be set in the [0,1] range.
    gfsdk_F32 choppy_scale;
	// The simulation spectrum is low-pass filtered to eliminate wavelengths that could end up under-sampled, this controls
	// how much of the frequency range is considered 'high frequency' (i.e. small wave).
    gfsdk_F32 small_wave_fraction;

    // The global time multiplier
    gfsdk_F32 time_scale;

	// The turbulent energy representing foam and bubbles spread in water starts generating on the tips of the waves if
	// Jacobian of wave curvature gets higher than this threshold. The range is [0,1], the typical values are [0.2,0.4] range.
	gfsdk_F32 foam_generation_threshold;
	// The amount of turbulent energy injected in areas defined by foam_generation_threshold parameter on each simulation step.
	// The range is [0,1], the typical values are [0,0.1] range.
	gfsdk_F32 foam_generation_amount;
	// The speed of spatial dissipation of turbulent energy. The range is [0,1], the typical values are in [0.5,1] range.
	gfsdk_F32 foam_dissipation_speed;
	// In addition to spatial dissipation, the turbulent energy dissolves over time. This parameter sets the speed of
	// dissolving over time. The range is [0,1], the typical values are in [0.9,0.99] range.
	gfsdk_F32 foam_falloff_speed;
};

struct GFSDK_WaveWorks_Simulation_Settings
{
    // The detail level of the simulation: this drives the resolution of the FFT and also determines whether the simulation workload is done
	// on the GPU or CPU
    GFSDK_WaveWorks_Simulation_DetailLevel detail_level;

    // The repeat interval for the fft simulation, in world units
    gfsdk_F32 fft_period;

	// True if wind_speed in GFSDK_WaveWorks_Simulation_Params should accept Beaufort scale value
	// False if wind_speed in GFSDK_WaveWorks_Simulation_Params should accept meters/second
	gfsdk_bool use_Beaufort_scale;

    // Should the displacement data be read back to the CPU?
    gfsdk_bool readback_displacements;

	// If readback is enabled, displacement data can be kept alive in a FIFO for historical lookups
	// e.g. in order to implement predict/correct for a networked application
	gfsdk_U32 num_readback_FIFO_entries;

    // Set max aniso degree for sampling of gradient maps
	gfsdk_U8 aniso_level;
    
	// The threading model to use when the CPU simulation path is active
	// Can be set to none (meaning: simulation is performed on the calling thread, synchronously), automatic, or even
	// an explicitly specified thread count
    GFSDK_WaveWorks_Simulation_CPU_Threading_Model CPU_simulation_threading_model;

	// Number of GPUs used 
	gfsdk_S32 num_GPUs;

	// Usage of texture arrays in GL
	gfsdk_bool use_texture_arrays;

	// Controls whether timer events will be used to gather stats on the CUDA simulation path
	// This can impact negatively on GPU/CPU parallelism, so it is recommended to enable this only when necessary
	gfsdk_bool enable_CUDA_timers;

	// Controls the use of graphics pipeline timers
	gfsdk_bool enable_gfx_timers;

	// Controls the use of CPU timers to gather profiling data
	gfsdk_bool enable_CPU_timers;
};

struct GFSDK_WaveWorks_Simulation_Stats
{
    // the times spent on particular simulation tasks, measured in milliseconds (1e-3 sec)
    gfsdk_F32 CPU_main_thread_wait_time;			 // CPU time spent by main app thread waiting for CPU FFT simulation results using CPU 
    gfsdk_F32 CPU_threads_start_to_finish_time;      // CPU time spent on CPU FFT simulation: time between 1st thread starts work and last thread finishes simulation work
    gfsdk_F32 CPU_threads_total_time;				 // CPU time spent on CPU FFT simulation: sum time spent in threads that perform simulation work
    gfsdk_F32 GPU_simulation_time;					 // GPU time spent on GPU simulation
    gfsdk_F32 GPU_FFT_simulation_time;				 // GPU simulation time spent on FFT
    gfsdk_F32 GPU_gfx_time;							 // GPU time spent on non-simulation e.g. updating gradient maps
    gfsdk_F32 GPU_update_time;						 // Total GPU time spent on simulation workloads
};

struct GFSDK_WaveWorks_Simulation_GL_Pool
{
	// Just like any other GL client, WaveWorks must use a GL texture unit each time it binds a texture
	// as a shader input. The app therefore needs to reserve a small pool of texture units for WaveWorks
	// to use when setting simulation state for GL rendering, in order to avoid clashes with the app's
	// own simultaneous use of texture units. All the slots in the pool must be allocated with valid
	// zero-based GL texture unit indices, without repeats or clashes.
	//
	// There is no particular requirement for the contents of the pool to be consistent from one invocation
	// to the next. The app just needs to ensure that it does not use any of the texture units in the
	// pool for as long as the graphics state set by WaveWorks is required to persist.
	//
	// The actual amount of reserved texture units depends on whether the library was set up to use OpenGL
	// Texture Arrays or not, and can be queried using GFSDK_WaveWorks_Simulation_GetTextureUnitCountGL2()

	enum { MaxNumReservedTextureUnits = 8 };
	gfsdk_U32 Reserved_Texture_Units[MaxNumReservedTextureUnits];
};

// These functions can be used to check whether a particular graphics device supports a particular detail level,
// *before* initialising the graphics device
GFSDK_WAVEWORKS_DECL(gfsdk_bool) GFSDK_WaveWorks_Simulation_DetailLevelIsSupported_NoGraphics(GFSDK_WaveWorks_Simulation_DetailLevel detailLevel);
GFSDK_WAVEWORKS_DECL(gfsdk_bool) GFSDK_WaveWorks_Simulation_DetailLevelIsSupported_D3D9(IDirect3D9* pD3D9, const _D3DADAPTER_IDENTIFIER9& adapterIdentifier, GFSDK_WaveWorks_Simulation_DetailLevel detailLevel);
GFSDK_WAVEWORKS_DECL(gfsdk_bool) GFSDK_WaveWorks_Simulation_DetailLevelIsSupported_D3D10(IDXGIAdapter* adapter, GFSDK_WaveWorks_Simulation_DetailLevel detailLevel);
GFSDK_WAVEWORKS_DECL(gfsdk_bool) GFSDK_WaveWorks_Simulation_DetailLevelIsSupported_D3D11(IDXGIAdapter* adapter, GFSDK_WaveWorks_Simulation_DetailLevel detailLevel);
GFSDK_WAVEWORKS_DECL(gfsdk_bool) GFSDK_WaveWorks_Simulation_DetailLevelIsSupported_GL2(GFSDK_WaveWorks_Simulation_DetailLevel detailLevel);

// Simulation lifetime management
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_CreateNoGraphics(const GFSDK_WaveWorks_Simulation_Settings& settings, const GFSDK_WaveWorks_Simulation_Params& params, GFSDK_WaveWorks_SimulationHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_CreateD3D9(const GFSDK_WaveWorks_Simulation_Settings& settings, const GFSDK_WaveWorks_Simulation_Params& params, IDirect3DDevice9* pD3DDevice, GFSDK_WaveWorks_SimulationHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_CreateD3D10(const GFSDK_WaveWorks_Simulation_Settings& settings, const GFSDK_WaveWorks_Simulation_Params& params, ID3D10Device* pD3DDevice, GFSDK_WaveWorks_SimulationHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_CreateD3D11(const GFSDK_WaveWorks_Simulation_Settings& settings, const GFSDK_WaveWorks_Simulation_Params& params, ID3D11Device* pD3DDevice, GFSDK_WaveWorks_SimulationHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_CreateGL2(const GFSDK_WaveWorks_Simulation_Settings& settings, const GFSDK_WaveWorks_Simulation_Params& params, void *pGLContext, GFSDK_WaveWorks_SimulationHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_Destroy(GFSDK_WaveWorks_SimulationHandle hSim);

// A simulation can be 'updated' with new settings and properties - this is universally preferable to recreating
// a simulation from scratch, since WaveWorks will only do as much reinitialization work as is necessary to implement
// the changes in the setup. For instance, simple changes of wind speed require no reallocations and no interruptions
// to the simulation and rendering pipeline
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_UpdateProperties(GFSDK_WaveWorks_SimulationHandle hSim,const GFSDK_WaveWorks_Simulation_Settings& settings, const GFSDK_WaveWorks_Simulation_Params& params);

// Sets the absolute simulation time for the next kick. WaveWorks guarantees that the same displacements will be
// generated for the same settings and input times, even across different platforms (e.g. to enable network-
// synchronized implementations)
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_SetTime(GFSDK_WaveWorks_SimulationHandle hSim, gfsdk_F64 dAppTime);

// Retrieve information about the WaveWorks shader inputs for a given platform. This information can be used to
// query compiled shaders via a reflection interface to obtain register or constant buffer indices for subsequent
// calls to SetRenderState
GFSDK_WAVEWORKS_DECL(gfsdk_U32                 ) GFSDK_WaveWorks_Simulation_GetShaderInputCountD3D9();
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_GetShaderInputDescD3D9(gfsdk_U32 inputIndex, GFSDK_WaveWorks_ShaderInput_Desc* pDesc);
GFSDK_WAVEWORKS_DECL(gfsdk_U32                 ) GFSDK_WaveWorks_Simulation_GetShaderInputCountD3D10();
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_GetShaderInputDescD3D10(gfsdk_U32 inputIndex, GFSDK_WaveWorks_ShaderInput_Desc* pDesc);
GFSDK_WAVEWORKS_DECL(gfsdk_U32                 ) GFSDK_WaveWorks_Simulation_GetShaderInputCountD3D11();
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_GetShaderInputDescD3D11(gfsdk_U32 inputIndex, GFSDK_WaveWorks_ShaderInput_Desc* pDesc);
GFSDK_WAVEWORKS_DECL(gfsdk_U32                 ) GFSDK_WaveWorks_Simulation_GetShaderInputCountGL2();
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_GetShaderInputDescGL2(gfsdk_U32 inputIndex, GFSDK_WaveWorks_ShaderInput_Desc* pDesc);

// For GL only, get the number of texture units that need to be reserved for WaveWorks in GFSDK_WaveWorks_Simulation_GL_Pool
GFSDK_WAVEWORKS_DECL(gfsdk_U32                 ) GFSDK_WaveWorks_Simulation_GetTextureUnitCountGL2(gfsdk_bool useTextureArrays);

// Set WaveWorks shader inputs ready for rendering - use GetStagingCursor() to identify the kick which produced the simulation
// results that are about to be set
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_SetRenderStateD3D9(GFSDK_WaveWorks_SimulationHandle hSim, const gfsdk_float4x4& matView, const gfsdk_U32 * pShaderInputRegisterMappings, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_SetRenderStateD3D10(GFSDK_WaveWorks_SimulationHandle hSim, const gfsdk_float4x4& matView, const gfsdk_U32 * pShaderInputRegisterMappings, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_SetRenderStateD3D11(GFSDK_WaveWorks_SimulationHandle hSim, ID3D11DeviceContext* pDC, const gfsdk_float4x4& matView, const gfsdk_U32 * pShaderInputRegisterMappings, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_SetRenderStateGL2(GFSDK_WaveWorks_SimulationHandle hSim, const gfsdk_float4x4& matView, const gfsdk_U32 * pShaderInputRegisterMappings, const GFSDK_WaveWorks_Simulation_GL_Pool& glPool);

// Retrieve an array of simulated displacements for some given array of x-y locations - use GetReadbackCursor() to identify the
// kick which produced the simulation results that are about to be retrieved
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_GetDisplacements(GFSDK_WaveWorks_SimulationHandle hSim, const gfsdk_float2* inSamplePoints, gfsdk_float4* outDisplacements, gfsdk_U32 numSamples);

// Get the most recent simulation statistics
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_GetStats(GFSDK_WaveWorks_SimulationHandle hSim, GFSDK_WaveWorks_Simulation_Stats& stats);

// For the current simulation settings and params, calculate an estimate of the maximum displacement that can be generated by the simulation.
// This can be used to conservatively inflate camera frusta for culling purposes (e.g. as a suitable value for Quadtree_SetFrustumCullMargin)
GFSDK_WAVEWORKS_DECL(gfsdk_F32                 ) GFSDK_WaveWorks_Simulation_GetConservativeMaxDisplacementEstimate(GFSDK_WaveWorks_SimulationHandle hSim);

// Kicks off the work to update the simulation to the most recent time specified by SetTime
// The top of the simulation pipeline is always run on the CPU, whereas the bottom may be run on either the CPU or GPU, depending on whether the simulation
// is using the CPU or GPU path internally, and whether graphics interop is required for rendering.
// If necessary, this call will block until the CPU part of the pipeline is able to accept further in-flight work. If the CPU part of the pipeline
// is already completely full, this means waiting for an in-flight kick to exit the CPU pipeline (kicks are processed in FIFO order)
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_KickNoGraphics(GFSDK_WaveWorks_SimulationHandle hSim, gfsdk_U64* pKickID);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_KickD3D9(GFSDK_WaveWorks_SimulationHandle hSim, gfsdk_U64* pKickID, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_KickD3D10(GFSDK_WaveWorks_SimulationHandle hSim, gfsdk_U64* pKickID, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_KickD3D11(GFSDK_WaveWorks_SimulationHandle hSim, gfsdk_U64* pKickID, ID3D11DeviceContext* pDC, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_KickGL2(GFSDK_WaveWorks_SimulationHandle hSim, gfsdk_U64* pKickID);

// The staging cursor points to the most recent kick to exit the CPU part of the simulation pipeline (and therefore the kick whose state would be set by a
// subsequent call to SetRenderState)
// Returns gfsdk_waveworks_result_NONE if no simulation results are staged
// The staging cursor will only ever change during an API call, and is guaranteed to advance by a maximum of one kick in any one call
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_GetStagingCursor(GFSDK_WaveWorks_SimulationHandle hSim, gfsdk_U64* pKickID);

// Advances the staging cursor
// Use block to specify behaviour in the case where there is an in-flight kick in the CPU part of the simulation pipeline
// Returns gfsdk_waveworks_result_NONE if there are no in-flight kicks in the CPU part of the simulation pipeline
// Returns gfsdk_waveworks_result_WOULD_BLOCK if there are in-flight kicks in the CPU part of the pipeline, but they're not ready for staging
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_AdvanceStagingCursorNoGraphics(GFSDK_WaveWorks_SimulationHandle hSim, bool block);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_AdvanceStagingCursorD3D9(GFSDK_WaveWorks_SimulationHandle hSim, bool block, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_AdvanceStagingCursorD3D10(GFSDK_WaveWorks_SimulationHandle hSim, bool block, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_AdvanceStagingCursorD3D11(GFSDK_WaveWorks_SimulationHandle hSim, bool block, ID3D11DeviceContext* pDC, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_AdvanceStagingCursorGL2(GFSDK_WaveWorks_SimulationHandle hSim, bool block);

// Waits until the staging cursor is ready to advance (i.e. waits until a non-blocking call to AdvanceStagingCursor would succeed)
// Returns gfsdk_waveworks_result_NONE if there are no in-flight kicks in the CPU part of the simulation pipeline
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_WaitStagingCursor(GFSDK_WaveWorks_SimulationHandle hSim);

// The readback cursor points to the kick whose results would be fetched by a call to GetDisplacements
// Returns gfsdk_waveworks_result_NONE if no results are available for readback
// The readback cursor will only ever change during an API call, and is guaranteed to advance by a maximum of one kick in any one call
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_GetReadbackCursor(GFSDK_WaveWorks_SimulationHandle hSim, gfsdk_U64* pKickID);

// Advances the readback cursor
// Use block to specify behaviour in the case where there is an in-flight readback
// Returns gfsdk_waveworks_result_NONE if there are no readbacks in-flight beyond staging
// Returns gfsdk_waveworks_result_WOULD_BLOCK if there are readbacks in-flight  beyond staging, but they're not yet ready
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_AdvanceReadbackCursor(GFSDK_WaveWorks_SimulationHandle hSim, bool block);

// Archives the current readback results in the readback FIFO, evicting the oldest FIFO entry if necessary
// Returns gfsdk_waveworks_result_FAIL if no results are available for readback
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_ArchiveDisplacements(GFSDK_WaveWorks_SimulationHandle hSim);

// Identical to GFSDK_WaveWorks_Simulation_GetDisplacements, except values are retrieved from the readback FIFO
// The readback entries to use are specified using the 'coord' parameter, as follows:
//    - specify 0.f to read from the most recent entry in the FIFO
//    - specify (num_readback_FIFO_entries-1) to read from the oldest entry in the FIFO
//    - intervening entries may be accessed the same way, using a zero-based index
//    - if 'coord' is fractional, the nearest pair of entries will be lerp'd accordingly (fractional lookups are therefore more CPU-intensive)
//
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result    ) GFSDK_WaveWorks_Simulation_GetArchivedDisplacements(GFSDK_WaveWorks_SimulationHandle hSim, float coord, const gfsdk_float2* inSamplePoints, gfsdk_float4* outDisplacements, gfsdk_U32 numSamples);

/*===========================================================================
  Quad-tree geometry generator
  ===========================================================================*/

struct GFSDK_WaveWorks_Quadtree_Params
{
    // Dimension of a single square patch, default to 128x128 grids
    gfsdk_S32 mesh_dim;
    // The size of the smallest permissible leaf quad in world space (i.e. the size of a lod zero patch)
    gfsdk_F32 min_patch_length;
    // The coordinates of the min corner of patch (0,0,lod). This is only necessary if AllocPatch/FreePatch
    // is being used to drive the quad-tree structure
    gfsdk_float2 patch_origin;
    // The lod of the root patch used for frustum culling and mesh lodding. This is only necessary if
    // AllocPatch/FreePatch is *not* being used to drive the quad-tree structure. This determines the furthest
    // coverage of the water surface from the eye point
    gfsdk_U32 auto_root_lod;
    // The upper limit of the pixel number a grid can cover in screen space
    gfsdk_F32 upper_grid_coverage;
    // The offset required to place the surface at sea level
    gfsdk_F32 sea_level;
	// The flag that defines if tessellation friendly topology and mesh must be generated
	gfsdk_bool use_tessellation;
    // The tessellation LOD scale parameter
    gfsdk_F32 tessellation_lod;
	// The degree of geomorphing to use when tessellation is not available, in the range [0,1]
	gfsdk_F32 geomorphing_degree;
	// Controls the use of CPU timers to gather profiling data
	gfsdk_bool enable_CPU_timers;
};

struct GFSDK_WaveWorks_Quadtree_Stats
{
    gfsdk_S32 num_patches_drawn;

    // CPU time spent on quadtree update, measured in milliseconds (1e-3 sec)
    gfsdk_F32 CPU_quadtree_update_time;
};

// Quadtree lifetime management
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_CreateD3D9(const GFSDK_WaveWorks_Quadtree_Params& params, IDirect3DDevice9* pD3DDevice, GFSDK_WaveWorks_QuadtreeHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_CreateD3D10(const GFSDK_WaveWorks_Quadtree_Params& params, ID3D10Device* pD3DDevice, GFSDK_WaveWorks_QuadtreeHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_CreateD3D11(const GFSDK_WaveWorks_Quadtree_Params& params, ID3D11Device* pD3DDevice, GFSDK_WaveWorks_QuadtreeHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_CreateGL2(const GFSDK_WaveWorks_Quadtree_Params& params, unsigned int Program, GFSDK_WaveWorks_QuadtreeHandle* pResult);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_Destroy(GFSDK_WaveWorks_QuadtreeHandle hQuadtree);

// A quadtree can be udpated with new parameters. This is something of a corner-case for quadtrees, but is provided for completeness
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_UpdateParams(GFSDK_WaveWorks_QuadtreeHandle hQuadtree, const GFSDK_WaveWorks_Quadtree_Params& params);

// Retrieve information about the WaveWorks shader inputs for a given platform. This information can be used to
// query compiled shaders via a reflection interface to obtain register or constant buffer indices for subsequent
// calls to Draw
GFSDK_WAVEWORKS_DECL(gfsdk_U32                ) GFSDK_WaveWorks_Quadtree_GetShaderInputCountD3D9();
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_GetShaderInputDescD3D9(gfsdk_U32 inputIndex, GFSDK_WaveWorks_ShaderInput_Desc* pDesc);
GFSDK_WAVEWORKS_DECL(gfsdk_U32                ) GFSDK_WaveWorks_Quadtree_GetShaderInputCountD3D10();
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_GetShaderInputDescD3D10(gfsdk_U32 inputIndex, GFSDK_WaveWorks_ShaderInput_Desc* pDesc);
GFSDK_WAVEWORKS_DECL(gfsdk_U32                ) GFSDK_WaveWorks_Quadtree_GetShaderInputCountD3D11();
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_GetShaderInputDescD3D11(gfsdk_U32 inputIndex, GFSDK_WaveWorks_ShaderInput_Desc* pDesc);
GFSDK_WAVEWORKS_DECL(gfsdk_U32                ) GFSDK_WaveWorks_Quadtree_GetShaderInputCountGL2();
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_GetShaderInputDescGL2(gfsdk_U32 inputIndex, GFSDK_WaveWorks_ShaderInput_Desc* pDesc);

// Explicit control over quadtree tiles, primarily for removing tiles that are known to be entirey hidden by terrain.
// If AllocPatch is never called, the quadtree runs in automatic mode and is assumed to be rooted on a eye-centred
// patch whose dimension is determined by GFSDK_WaveWorks_Quadtree_Params.auto_root_lod
// Otherwise, WaveWorks starts frustum culling from the list of patches implied by calls to AllocPatch/FreePatch
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_AllocPatch(GFSDK_WaveWorks_QuadtreeHandle hQuadtree, gfsdk_S32 x, gfsdk_S32 y, gfsdk_U32 lod, gfsdk_bool enabled);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_FreePatch(GFSDK_WaveWorks_QuadtreeHandle hQuadtree, gfsdk_S32 x, gfsdk_S32 y, gfsdk_U32 lod);

// Draw the water surface using the specified quadtree with the specified view and projection matrices
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_DrawD3D9(GFSDK_WaveWorks_QuadtreeHandle hQuadtree, const gfsdk_float4x4& matView, const gfsdk_float4x4& matProj, const gfsdk_U32 * pShaderInputRegisterMappings, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_DrawD3D10(GFSDK_WaveWorks_QuadtreeHandle hQuadtree, const gfsdk_float4x4& matView, const gfsdk_float4x4& matProj, const gfsdk_U32 * pShaderInputRegisterMappings, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_DrawD3D11(GFSDK_WaveWorks_QuadtreeHandle hQuadtree, ID3D11DeviceContext* pDC, const gfsdk_float4x4& matView, const gfsdk_float4x4& matProj, const gfsdk_U32 * pShaderInputRegisterMappings, GFSDK_WaveWorks_SavestateHandle hSavestate);
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_DrawGL2(GFSDK_WaveWorks_QuadtreeHandle hQuadtree, const gfsdk_float4x4& matView, const gfsdk_float4x4& matProj, const gfsdk_U32 * pShaderInputRegisterMappings);

// Get the most recent quadtree rendering statistics
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_GetStats(GFSDK_WaveWorks_QuadtreeHandle hQuadtree, GFSDK_WaveWorks_Quadtree_Stats& stats);

// Patches are culled based on their undisplaced footrpint plus an additional user-supplied margin to take account
// of simulated displacements. Simulation_GetConservativeMaxDisplacementEstimate() can be used for this directly
// when WaveWorks is the only source of displacement on the water surface, otherwise it can be added to estimates
// from any other sources as necessary (e.g. wakes, explosions etc.)
GFSDK_WAVEWORKS_DECL(gfsdk_waveworks_result   ) GFSDK_WaveWorks_Quadtree_SetFrustumCullMargin(GFSDK_WaveWorks_QuadtreeHandle hQuadtree, gfsdk_F32 margin);

/*===========================================================================
  The End
  ===========================================================================*/

#ifdef __cplusplus
}; //extern "C" {
#endif

#endif	// _GFSDK_WAVEWORKS_H
