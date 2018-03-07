/*
* This code contains NVIDIA Confidential Information and is disclosed
* under the Mutual Non-Disclosure Agreement.
*
* Notice
* ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES
* NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
* THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
*
* NVIDIA Corporation assumes no responsibility for the consequences of use of such
* information or for any infringement of patents or other rights of third parties that may
* result from its use. No license is granted by implication or otherwise under any patent
* or patent rights of NVIDIA Corporation. No third party distribution is allowed unless
* expressly authorized by NVIDIA.  Details are subject to change without notice.
* This code supersedes and replaces all information previously supplied.
* NVIDIA Corporation products are not authorized for use as critical
* components in life support devices or systems without express written approval of
* NVIDIA Corporation.
*
* Copyright ?2008- 2016 NVIDIA Corporation. All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software and related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is
* strictly prohibited.
*/

/*=============================================================================
	D3D11WaveWorks.cpp: D3D WaveWorks RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "GFSDK_WaveWorks.h"

DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Simulation CPU Main thread wait time"), STAT_WaveWorksD3D11SimulationWaitTime, STATGROUP_WaveWorksD3D11, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Simulation CPU Threads start to finish time"), STAT_WaveWorksD3D11SimulationStartFinishTime, STATGROUP_WaveWorksD3D11, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Simulation CPU Threads total time"), STAT_WaveWorksD3D11TotalTime, STATGROUP_WaveWorksD3D11, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Simulation GPU Simulation time"), STAT_WaveWorksD3D11GPUSimulationTime, STATGROUP_WaveWorksD3D11, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Simulation GPU FFT Simulation time"), STAT_WaveWorksD3D11GPUFFTSimulationTime, STATGROUP_WaveWorksD3D11, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Simulation GPU GFX Time"), STAT_WaveWorksD3D11GPUGFXTime, STATGROUP_WaveWorksD3D11, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Simulation GPU Update time"), STAT_WaveWorksD3D11GPUUpdateTime, STATGROUP_WaveWorksD3D11, );

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Quadtree Patches drawn"), STAT_WaveWorksD3D11QuadtreePatchesDrawn, STATGROUP_WaveWorksD3D11, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Quadtree CPU Update time"), STAT_WaveWorksD3D11QuadtreeUpdateTime, STATGROUP_WaveWorksD3D11, );

DEFINE_STAT(STAT_WaveWorksD3D11SimulationWaitTime);
DEFINE_STAT(STAT_WaveWorksD3D11SimulationStartFinishTime);
DEFINE_STAT(STAT_WaveWorksD3D11TotalTime);
DEFINE_STAT(STAT_WaveWorksD3D11GPUSimulationTime);
DEFINE_STAT(STAT_WaveWorksD3D11GPUFFTSimulationTime);
DEFINE_STAT(STAT_WaveWorksD3D11GPUGFXTime);
DEFINE_STAT(STAT_WaveWorksD3D11GPUUpdateTime);

DEFINE_STAT(STAT_WaveWorksD3D11QuadtreePatchesDrawn);
DEFINE_STAT(STAT_WaveWorksD3D11QuadtreeUpdateTime);

namespace
{
	FString Platform = PLATFORM_64BITS ? TEXT("win64") : TEXT("win32");
	FString WaveWorksBinariesDir = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/WaveWorks/") + Platform;
	FString WaveWorksDLLName = FString("gfsdk_waveworks") + "." + Platform + ".dll";
	FString CuFFTDLLName = FString("cufft") + Platform.Right(2) + "_55.dll";

	struct DllHandle
	{
		DllHandle(const FString Name)
		{
			FWindowsPlatformProcess::PushDllDirectory(*WaveWorksBinariesDir);
			Handle = FPlatformProcess::GetDllHandle(*Name);
			FWindowsPlatformProcess::PopDllDirectory(*WaveWorksBinariesDir);
		}
		~DllHandle()
		{
			FPlatformProcess::FreeDllHandle(Handle);
		}
		void* Handle;
	};
}

class FD3D11WaveWorks : public FRHIWaveWorks
{
public:
	FD3D11WaveWorks(ID3D11Device* Device, ID3D11DeviceContext* DeviceContext,
		const struct GFSDK_WaveWorks_Simulation_Settings& Settings,
		const struct GFSDK_WaveWorks_Simulation_Params& Params)
		: WaveWorksDllHandle(WaveWorksDLLName)
		, CuFFTDllHandle(CuFFTDLLName)
		, Device(Device)
		, DeviceContext(DeviceContext)
	{
		// init waveworks sdk
		gfsdk_waveworks_result Result = GFSDK_WaveWorks_InitD3D11(Device, nullptr, GFSDK_WAVEWORKS_API_GUID);
		// create SavedState
		Result = GFSDK_WaveWorks_Savestate_CreateD3D11(GFSDK_WaveWorks_StatePreserve_All, Device, &SaveState);
		// create waveworks simulation
		Result = GFSDK_WaveWorks_Simulation_CreateD3D11(Settings, Params, Device, &Simulation);

		if (Result != gfsdk_waveworks_result_OK)
		{
			UE_LOG(LogD3D11RHI, Error, TEXT("WaveWorks: Simulation_CreateD3D11 FAIL"));
		}
		
		GFSDK_WaveWorks_Simulation_UpdateProperties(Simulation, Settings, Params);
	}

	~FD3D11WaveWorks()
	{
		GFSDK_WaveWorks_Simulation_Destroy(Simulation);
		GFSDK_WaveWorks_Savestate_Destroy(SaveState);
		GFSDK_WaveWorks_ReleaseD3D11(Device);
	}
	
	virtual void UpdateTick(float SimulationTime) override
	{
		if(Simulation)
		{
			GFSDK_WaveWorks_Simulation_SetTime(Simulation, SimulationTime);
			GFSDK_WaveWorks_Simulation_KickD3D11(Simulation, nullptr, DeviceContext, SaveState);

			while (gfsdk_waveworks_result_NONE == GFSDK_WaveWorks_Simulation_GetStagingCursor(Simulation, nullptr))
			{
				GFSDK_WaveWorks_Simulation_SetTime(Simulation, SimulationTime);
				GFSDK_WaveWorks_Simulation_KickD3D11(Simulation, nullptr, DeviceContext, SaveState);
			}
			GFSDK_WaveWorks_Savestate_RestoreD3D11(SaveState, DeviceContext);
		
#if WITH_EDITOR
			GFSDK_WaveWorks_Simulation_Stats Stats;
			GFSDK_WaveWorks_Simulation_GetStats(Simulation, Stats);

			SET_FLOAT_STAT(STAT_WaveWorksD3D11SimulationWaitTime, Stats.CPU_main_thread_wait_time);
			SET_FLOAT_STAT(STAT_WaveWorksD3D11SimulationStartFinishTime, Stats.CPU_threads_start_to_finish_time);
			SET_FLOAT_STAT(STAT_WaveWorksD3D11TotalTime, Stats.CPU_threads_total_time);
			SET_FLOAT_STAT(STAT_WaveWorksD3D11GPUSimulationTime, Stats.GPU_simulation_time);
			SET_FLOAT_STAT(STAT_WaveWorksD3D11GPUFFTSimulationTime, Stats.GPU_FFT_simulation_time);
			SET_FLOAT_STAT(STAT_WaveWorksD3D11GPUGFXTime, Stats.GPU_gfx_time);
			SET_FLOAT_STAT(STAT_WaveWorksD3D11GPUUpdateTime, Stats.GPU_update_time);
#endif
		}
	}

	virtual void SetRenderState(const FMatrix ViewMatrix, const TArray<uint32>& ShaderInputMappings)
	{
		FMatrix NewViewMatrix = ViewMatrix;
		// unit from 'cm' to 'm'
		NewViewMatrix.M[3][0] /= 100.0;	
		NewViewMatrix.M[3][1] /= 100.0;
		NewViewMatrix.M[3][2] /= 100.0;

		// set render states
		if (Simulation)
		{
			GFSDK_WaveWorks_Simulation_SetRenderStateD3D11(Simulation, DeviceContext,
				reinterpret_cast<const gfsdk_float4x4&>(NewViewMatrix), ShaderInputMappings.GetData(), SaveState);
		}
	}

	virtual void CreateQuadTree(
		GFSDK_WaveWorks_Quadtree** OutWaveWorksQuadTreeHandle, 
		int32 MeshDim, 
		float MinPatchLength, 
		uint32 AutoRootLOD, 
		float UpperGridCoverage, 
		float SeaLevel, 
		bool UseTessellation, 
		float TessellationLOD, 
		float GeoMoprhingDegree
		)
	{
		GFSDK_WaveWorks_Quadtree_Params Params;
		Params.mesh_dim = MeshDim;
		Params.min_patch_length = MinPatchLength;
		Params.patch_origin = { 0.0f, 0.0f };
		Params.auto_root_lod = AutoRootLOD;
		Params.upper_grid_coverage = UpperGridCoverage;
		Params.sea_level = SeaLevel;
		Params.use_tessellation = UseTessellation;
		Params.tessellation_lod = TessellationLOD;
		Params.geomorphing_degree = GeoMoprhingDegree;
		Params.enable_CPU_timers = true;

		gfsdk_waveworks_result Result = gfsdk_waveworks_result::gfsdk_waveworks_result_OK;
		if (*OutWaveWorksQuadTreeHandle == nullptr)
		{
			Result = GFSDK_WaveWorks_Quadtree_CreateD3D11(Params, Device, OutWaveWorksQuadTreeHandle);
		}
		else
		{
			Result = GFSDK_WaveWorks_Quadtree_UpdateParams(*OutWaveWorksQuadTreeHandle,Params);
		}
		
		if (Result != gfsdk_waveworks_result_OK)
		{			
			*OutWaveWorksQuadTreeHandle = nullptr;
			UE_LOG(LogD3D11RHI, Error, TEXT("WaveWorks: Failed to Create QuadTree"));
		}
		else
		{
			GFSDK_WaveWorks_Quadtree_SetFrustumCullMargin(*OutWaveWorksQuadTreeHandle, GFSDK_WaveWorks_Simulation_GetConservativeMaxDisplacementEstimate(Simulation) * 100.0f);
		}		
	}

	virtual void DrawQuadTree(
		GFSDK_WaveWorks_Quadtree* WaveWorksQuadTreeHandle, 
		FMatrix ViewMatrix, 
		FMatrix ProjMatrix, 
		const TArray<uint32>& ShaderInputMappings
		)
	{
		FD3D11DynamicRHI* D3D11RHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
		D3D11RHI->CommitResources();

		gfsdk_waveworks_result Result = GFSDK_WaveWorks_Quadtree_SetFrustumCullMargin(WaveWorksQuadTreeHandle,GFSDK_WaveWorks_Simulation_GetConservativeMaxDisplacementEstimate(Simulation) * 100.0f);
		if (Result != gfsdk_waveworks_result_OK)
		{
			UE_LOG(LogD3D11RHI, Error, TEXT("WaveWorks: Failed to Set FrustumCullMargin"));
			return; 
		}

		Result = GFSDK_WaveWorks_Quadtree_DrawD3D11(
			WaveWorksQuadTreeHandle,
			DeviceContext,
			reinterpret_cast<const gfsdk_float4x4&>(ViewMatrix),
			reinterpret_cast<const gfsdk_float4x4&>(ProjMatrix),
			ShaderInputMappings.GetData(),
			SaveState
			);
		if (Result != gfsdk_waveworks_result_OK)
		{
			UE_LOG(LogD3D11RHI, Error, TEXT("WaveWorks: Failed to Draw QuadTree"));
		}

#if WITH_EDITOR
		GFSDK_WaveWorks_Quadtree_Stats Stats;
		GFSDK_WaveWorks_Quadtree_GetStats(WaveWorksQuadTreeHandle, Stats);

		SET_DWORD_STAT(STAT_WaveWorksD3D11QuadtreePatchesDrawn, Stats.num_patches_drawn);
		SET_FLOAT_STAT(STAT_WaveWorksD3D11QuadtreeUpdateTime, Stats.CPU_quadtree_update_time);
#endif

		GFSDK_WaveWorks_Savestate_RestoreD3D11(SaveState, DeviceContext);

		D3D11RHI->CacheWaveWorksQuadTreeState(ShaderInputMappings);
	}

	virtual void DestroyQuadTree(GFSDK_WaveWorks_Quadtree* WaveWorksQuadTreeHandle)
	{
		GFSDK_WaveWorks_Quadtree_Destroy(WaveWorksQuadTreeHandle);
	}

	virtual void GetDisplacements(TArray<FVector> InSamplePoints, FWaveWorksSampleDisplacementsDelegate OnRecieveDisplacementDelegate)
	{
		TArray<FVector2D> SamplePoints;
		for (int index = 0; index < InSamplePoints.Num(); index++)
		{
			SamplePoints.Add(FVector2D(InSamplePoints[index].X, InSamplePoints[index].Y));
		}

		if (Simulation)
		{
			TArray<FVector4> OutDisplacements;
			OutDisplacements.AddUninitialized(InSamplePoints.Num());
			gfsdk_waveworks_result Result = GFSDK_WaveWorks_Simulation_GetDisplacements(
				Simulation,
				reinterpret_cast<gfsdk_float2*>(SamplePoints.GetData()),
				reinterpret_cast<gfsdk_float4*>(OutDisplacements.GetData()),
				InSamplePoints.Num()
				);

			OnRecieveDisplacementDelegate.ExecuteIfBound(InSamplePoints, OutDisplacements);
		}
	}

	virtual void GetIntersectPointWithRay(FVector Position, FVector Direction, float SeaLevel, FWaveWorksRaycastResultDelegate OnRecieveIntersectPointDelegate)
	{
		FVector2D test_point;											// x,z coordinates of current test point
		FVector2D old_test_point;										// x,z coordinates of current test point
		FVector4  displacements;										// displacements returned by GFSDK_WaveWorks library
		float t = 0;													// distance traveled along the ray while tracing
		int num_steps = 0;												// number of steps we've done while tracing
		float max_displacement = GFSDK_WaveWorks_Simulation_GetConservativeMaxDisplacementEstimate(Simulation);
																		// the maximal possible displacements of ocean surface along y axis, 
																		// defining volume we have to trace
		const int max_num_successive_steps = 16;						// we limit ourselves on #of successive steps
		const int max_num_binary_steps = 16;							// we limit ourselves on #of binary search steps
		const float t_threshold = 0.05f;								// we stop successive tracing when we don't progress more than 5 cm each step
		const float refinement_threshold_sqr = 0.1f*0.1f;				// we stop refinement step when we don't progress more than 10cm while doing refinement of current water altitude
		const float t_multiplier = 1.8f / (fabs(Direction.Z) + 1.0f);	// we increase step length at steep angles to speed up the tracing, 
																		// but less than 2 to make sure the process converges 
																		// and to add some safety to minimize chance of overshooting
		FVector PositionBSStart;										// Vectors used at binary search step
		FVector PositionBSEnd;

		// normalizing direction
		Direction.Normalize();

		float sea_level = SeaLevel;

		// checking if ray is outside of ocean surface volume 
		if ((Position.Z >= max_displacement + sea_level) && (Direction.Z >= 0))
		{
			OnRecieveIntersectPointDelegate.ExecuteIfBound(FVector::ZeroVector, true);
			return;
		}

		// getting to the top edge of volume where we can start
		if (Position.Z > max_displacement + sea_level)
		{
			t = -(Position.Z - max_displacement - sea_level) / Direction.Z;
			Position += t*Direction;
		}

		// tracing the ocean surface:
		// moving along the ray by distance defined by vertical distance form current test point, increased/decreased by safety multiplier
		// this process will converge despite our assumption on local flatness of the surface because curvature of the surface is smooth
		// NB: this process guarantees we don't shoot through wave tips
		while (1)
		{
			displacements.X = 0;
			displacements.Y = 0;
			old_test_point.X = 0;
			old_test_point.Y = 0;
			int k = 0;
			for (; k < 4; k++) // few refinement steps to converge at correct intersection point
			{
				// moving back sample points by the displacements read initially, 
				// to get a guess on which undisturbed water surface point moved to the actual sample point 
				// due to x,y motion of water surface, assuming the x,y disturbances are locally constant
				test_point.X = Position.X - displacements.X;
				test_point.Y = Position.Y - displacements.Y;
				GFSDK_WaveWorks_Simulation_GetDisplacements(Simulation, reinterpret_cast<gfsdk_float2*>(&test_point), reinterpret_cast<gfsdk_float4*>(&displacements), 1);
				if (refinement_threshold_sqr >(old_test_point.X - test_point.X)*(old_test_point.X - test_point.X) + (old_test_point.Y - test_point.Y)*(old_test_point.Y - test_point.Y)) break;
				old_test_point.X = test_point.X;
				old_test_point.Y = test_point.Y;
			}

			// getting t to travel along the ray
			t = t_multiplier * (Position.Z - displacements.Z - sea_level);

			// traveling along the ray
			Position += t*Direction;

			if (num_steps >= max_num_successive_steps)  break;
			if (t < t_threshold) break;
			++num_steps;
		}

		// exited the loop, checking if intersection is found
		if (t < t_threshold)
		{
			OnRecieveIntersectPointDelegate.ExecuteIfBound(Position, true);
			return;
		}

		// if we're looking down and we did not hit water surface, doing binary search to make sure we hit water surface,
		// but there is risk of shooting through wave tips if we are tracing at extremely steep angles
		if (Direction.Z < 0)
		{
			PositionBSStart = Position;

			// getting to the bottom edge of volume where we can start
			t = -(Position.Z + max_displacement - sea_level) / Direction.Z;
			PositionBSEnd = Position + t*Direction;

			for (int i = 0; i < max_num_binary_steps; i++)
			{
				Position = (PositionBSStart + PositionBSEnd)*0.5f;
				old_test_point.X = 0;
				old_test_point.Y = 0;
				for (int k = 0; k < 4; k++)
				{
					test_point.X = Position.X - displacements.X;
					test_point.Y = Position.Y - displacements.Y;
					GFSDK_WaveWorks_Simulation_GetDisplacements(Simulation, reinterpret_cast<gfsdk_float2*>(&test_point), reinterpret_cast<gfsdk_float4*>(&displacements), 1);
					if (refinement_threshold_sqr >(old_test_point.X - test_point.X)*(old_test_point.X - test_point.X) + (old_test_point.Y - test_point.Y)*(old_test_point.Y - test_point.Y)) break;
					old_test_point.X = test_point.X;
					old_test_point.Y = test_point.Y;
				}
				if (Position.Z - displacements.Z - sea_level > 0)
				{
					PositionBSStart = Position;
				}
				else
				{
					PositionBSEnd = Position;
				}
			}
			OnRecieveIntersectPointDelegate.ExecuteIfBound(Position, true);
			return;
		}
		
		OnRecieveIntersectPointDelegate.ExecuteIfBound(FVector::ZeroVector, false);
	}

private:
	DllHandle WaveWorksDllHandle;
	DllHandle CuFFTDllHandle;
	ID3D11Device* Device;
	ID3D11DeviceContext* DeviceContext;
	GFSDK_WaveWorks_SavestateHandle SaveState;
};

FWaveWorksRHIRef FD3D11DynamicRHI::RHICreateWaveWorks(
	const struct GFSDK_WaveWorks_Simulation_Settings& Settings,
	const struct GFSDK_WaveWorks_Simulation_Params& Params
	)
{
	return new FD3D11WaveWorks(GetDevice(), GetDeviceContext(), Settings, Params);
}

namespace
{
	TArray<WaveWorksShaderInput> InitializeShaderInput()
	{
		DllHandle WaveWorksDllHandle(WaveWorksDLLName);

		// maps GFSDK_WaveWorks_ShaderInput_Desc::InputType to EShaderFrequency
		EShaderFrequency TypeToFrequencyMap[] =
		{
			SF_Vertex,
			SF_Vertex,
			SF_Vertex,
			SF_Vertex,
			SF_Hull,
			SF_Hull,
			SF_Hull,
			SF_Hull,
			SF_Domain,
			SF_Domain,
			SF_Domain,
			SF_Domain,
			SF_Pixel,
			SF_Pixel,
			SF_Pixel,
			SF_Pixel
		};

		// maps GFSDK_WaveWorks_ShaderInput_Desc::InputType to ERHIResourceType
		ERHIResourceType TypeToResourceMap[] =
		{
			RRT_None,
			RRT_UniformBuffer,
			RRT_ShaderResourceView,
			RRT_SamplerState,
			RRT_None,
			RRT_UniformBuffer,
			RRT_ShaderResourceView,
			RRT_SamplerState,
			RRT_None,
			RRT_UniformBuffer,
			RRT_ShaderResourceView,
			RRT_SamplerState,
			RRT_None,
			RRT_UniformBuffer,
			RRT_ShaderResourceView,
			RRT_SamplerState,
		};

		uint32 Count = GFSDK_WaveWorks_Simulation_GetShaderInputCountD3D11();
		TArray<WaveWorksShaderInput> Result;
		for (uint32 i = 0; i<Count; ++i)
		{
			GFSDK_WaveWorks_ShaderInput_Desc Desc;
			GFSDK_WaveWorks_Simulation_GetShaderInputDescD3D11(i, &Desc);
			check(Desc.Type < ARRAY_COUNT(TypeToFrequencyMap));
			check(Desc.Type < ARRAY_COUNT(TypeToResourceMap));
			WaveWorksShaderInput Input = { 
				TypeToFrequencyMap[Desc.Type], 
				TypeToResourceMap[Desc.Type], 
				Desc.Name, 
			};
			Result.Push(Input);
		}

		return Result;
	}
	TArray<WaveWorksShaderInput> InitializeQuadTreeShaderInput()
	{
		DllHandle WaveWorksDllHandle(WaveWorksDLLName);

		// maps GFSDK_WaveWorks_ShaderInput_Desc::InputType to EShaderFrequency
		EShaderFrequency TypeToFrequencyMap[] =
		{
			SF_Vertex,
			SF_Vertex,
			SF_Vertex,
			SF_Vertex,
			SF_Hull,
			SF_Hull,
			SF_Hull,
			SF_Hull,
			SF_Domain,
			SF_Domain,
			SF_Domain,
			SF_Domain,
			SF_Pixel,
			SF_Pixel,
			SF_Pixel,
			SF_Pixel
		};

		// maps GFSDK_WaveWorks_ShaderInput_Desc::InputType to ERHIResourceType
		ERHIResourceType TypeToResourceMap[] =
		{
			RRT_None,
			RRT_UniformBuffer,
			RRT_ShaderResourceView,
			RRT_SamplerState,
			RRT_None,
			RRT_UniformBuffer,
			RRT_ShaderResourceView,
			RRT_SamplerState,
			RRT_None,
			RRT_UniformBuffer,
			RRT_ShaderResourceView,
			RRT_SamplerState,
			RRT_None,
			RRT_UniformBuffer,
			RRT_ShaderResourceView,
			RRT_SamplerState,
		};

		uint32 Count = GFSDK_WaveWorks_Quadtree_GetShaderInputCountD3D11();
		TArray<WaveWorksShaderInput> Result;
		for (uint32 i = 0; i < Count; ++i)
		{
			GFSDK_WaveWorks_ShaderInput_Desc Desc;
			GFSDK_WaveWorks_Quadtree_GetShaderInputDescD3D11(i, &Desc);
			check(Desc.Type < ARRAY_COUNT(TypeToFrequencyMap));
			check(Desc.Type < ARRAY_COUNT(TypeToResourceMap));
			WaveWorksShaderInput Input = {
				TypeToFrequencyMap[Desc.Type],
				TypeToResourceMap[Desc.Type],
				Desc.Name,
			};
			Result.Push(Input);
		}

		return Result;
	}

	TArray<WaveWorksShaderInput> ShaderInput = InitializeShaderInput();
	TArray<WaveWorksShaderInput> QuadTreeShaderInput = InitializeQuadTreeShaderInput();
}

const TArray<WaveWorksShaderInput>* FD3D11DynamicRHI::RHIGetWaveWorksShaderInput()
{
	return &ShaderInput;
}

const TArray<WaveWorksShaderInput>* FD3D11DynamicRHI::RHIGetWaveWorksQuadTreeShaderInput()
{
	return &QuadTreeShaderInput;
}
