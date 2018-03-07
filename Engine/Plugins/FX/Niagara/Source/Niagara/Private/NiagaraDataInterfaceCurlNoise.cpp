#include "NiagaraDataInterfaceCurlNoise.h"

VectorRegister UNiagaraDataInterfaceCurlNoise::NoiseTable[17][17][17];

UNiagaraDataInterfaceCurlNoise::UNiagaraDataInterfaceCurlNoise(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UNiagaraDataInterfaceCurlNoise::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
	}
}

void UNiagaraDataInterfaceCurlNoise::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("SampleNoiseField");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("NoiseField")));
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("XYZ")));
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));
	//Sig.Owner = *GetName();

	OutFunctions.Add(Sig);
}

DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceCurlNoise, SampleNoiseField);
FVMExternalFunction UNiagaraDataInterfaceCurlNoise::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData)
{
	check(BindingInfo.Name == TEXT("SampleNoiseField"));
	check(BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 3);
	return TNDIParamBinder<0, float, TNDIParamBinder<1, float, TNDIParamBinder<2, float, NDI_FUNC_BINDER(UNiagaraDataInterfaceCurlNoise, SampleNoiseField)>>>::Bind(this, BindingInfo, InstanceData);
}

template<typename XType, typename YType, typename ZType>
void UNiagaraDataInterfaceCurlNoise::SampleNoiseField(FVectorVMContext& Context)
{
	XType XParam(Context);
	YType YParam(Context);
	ZType ZParam(Context);
	FRegisterHandler<float> OutSampleX(Context);
	FRegisterHandler<float> OutSampleY(Context);
	FRegisterHandler<float> OutSampleZ(Context);
	
	for (int32 InstanceIdx = 0; InstanceIdx < Context.NumInstances; ++InstanceIdx)
	{
		VectorRegister InCoords = MakeVectorRegister(XParam.Get(), YParam.Get(), ZParam.Get(), 0.0f);

		const VectorRegister One = MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f);
		const VectorRegister Zero = MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f);
		const VectorRegister VecSize = MakeVectorRegister(16.0f, 16.0f, 16.0f, 16.0f);

		VectorRegister Dst = MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f);

		for (uint32 i = 1; i < 2; i++)
		{
			float Di = 0.2f * (1.0f / (1 << i));
			VectorRegister Div = MakeVectorRegister(Di, Di, Di, Di);
			VectorRegister Coords = VectorMod(VectorAbs(VectorMultiply(InCoords, Div)), VecSize);
			Coords = VectorMin(Coords, VecSize);
			Coords = VectorMax(Coords, Zero);
			const float *CoordPtr = reinterpret_cast<float const*>(&Coords);
			const int32 Cx = CoordPtr[0];
			const int32 Cy = CoordPtr[1];
			const int32 Cz = CoordPtr[2];

			VectorRegister Frac = VectorFractional(Coords);
			VectorRegister Alpha = VectorReplicate(Frac, 0);
			VectorRegister OneMinusAlpha = VectorSubtract(One, Alpha);

			VectorRegister XV1 = VectorMultiplyAdd(NoiseTable[Cx][Cy][Cz], Alpha, VectorMultiply(NoiseTable[Cx + 1][Cy][Cz], OneMinusAlpha));
			VectorRegister XV2 = VectorMultiplyAdd(NoiseTable[Cx][Cy + 1][Cz], Alpha, VectorMultiply(NoiseTable[Cx + 1][Cy + 1][Cz], OneMinusAlpha));
			VectorRegister XV3 = VectorMultiplyAdd(NoiseTable[Cx][Cy][Cz + 1], Alpha, VectorMultiply(NoiseTable[Cx + 1][Cy][Cz + 1], OneMinusAlpha));
			VectorRegister XV4 = VectorMultiplyAdd(NoiseTable[Cx][Cy + 1][Cz + 1], Alpha, VectorMultiply(NoiseTable[Cx + 1][Cy + 1][Cz + 1], OneMinusAlpha));

			Alpha = VectorReplicate(Frac, 1);
			OneMinusAlpha = VectorSubtract(One, Alpha);
			VectorRegister YV1 = VectorMultiplyAdd(XV1, Alpha, VectorMultiply(XV2, OneMinusAlpha));
			VectorRegister YV2 = VectorMultiplyAdd(XV3, Alpha, VectorMultiply(XV4, OneMinusAlpha));

			Alpha = VectorReplicate(Frac, 2);
			OneMinusAlpha = VectorSubtract(One, Alpha);
			VectorRegister ZV = VectorMultiplyAdd(YV1, Alpha, VectorMultiply(YV2, OneMinusAlpha));

			Dst = VectorAdd(Dst, ZV);
		}

		float *RegPtr = reinterpret_cast<float*>(&Dst);
		*OutSampleX.GetDest() = RegPtr[0];
		*OutSampleY.GetDest() = RegPtr[1];
		*OutSampleZ.GetDest() = RegPtr[2];

		XParam.Advance();
		YParam.Advance();
		ZParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
	}
}



// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here
// 
bool UNiagaraDataInterfaceCurlNoise::GetFunctionHLSL(FString FunctionName, TArray<DIGPUBufferParamDescriptor> &Descriptors, FString &HLSLInterfaceID, FString &OutHLSL)
{
	FString BufferName = Descriptors[0].BufferParamName;
	OutHLSL += TEXT("void ") + FunctionName + TEXT("(in float3 In_XYZ, out float3 Out_Value) \n{\n");
	OutHLSL += TEXT("\t float3 a = trunc((In_XYZ*0.2) / 16.0);\n");
	OutHLSL += TEXT("\t float3 ModXYZ = (In_XYZ*0.2) - a*16.0;\n");
	OutHLSL += TEXT("\t int3 IntCoord = int3(ModXYZ.x, ModXYZ.y, ModXYZ.z);\n");
	OutHLSL += TEXT("\t float3 frc = frac(ModXYZ);\n");
	OutHLSL += TEXT("\t float3 V1 = ") + BufferName + TEXT("[IntCoord.x + IntCoord.y*17 + IntCoord.z*17*17].xyz;\n");
	OutHLSL += TEXT("\t float3 V2 = ") + BufferName + TEXT("[IntCoord.x+1 + IntCoord.y*17 + IntCoord.z*17*17].xyz;\n");
	OutHLSL += TEXT("\t float3 XV1 = lerp(V1, V2, frc.xxx);\n");
	OutHLSL += TEXT("\t V1 = ") + BufferName + TEXT("[IntCoord.x + (IntCoord.y+1)*17 + IntCoord.z*17*17].xyz;\n");
	OutHLSL += TEXT("\t V2 = ") + BufferName + TEXT("[IntCoord.x+1 + (IntCoord.y+1)*17 + IntCoord.z*17*17].xyz;\n");
	OutHLSL += TEXT("\t float3 XV2 = lerp(V1, V2, frc.xxx);\n");
	OutHLSL += TEXT("\t V1 = ") + BufferName + TEXT("[IntCoord.x + IntCoord.y*17 + (IntCoord.z+1)*17*17].xyz;\n");
	OutHLSL += TEXT("\t V2 = ") + BufferName + TEXT("[IntCoord.x+1 + IntCoord.y*17 + (IntCoord.z+1)*17*17].xyz;\n");
	OutHLSL += TEXT("\t float3 XV3 = lerp(V1, V2, frc.xxx);\n");
	OutHLSL += TEXT("\t V1 = ") + BufferName + TEXT("[IntCoord.x + (IntCoord.y+1)*17 + (IntCoord.z+1)*17*17].xyz;\n");
	OutHLSL += TEXT("\t V2 = ") + BufferName + TEXT("[IntCoord.x+1 + (IntCoord.y+1)*17 + (IntCoord.z+1)*17*17].xyz;\n");
	OutHLSL += TEXT("\t float3 XV4 = lerp(V1, V2, frc.xxx);\n");
	OutHLSL += TEXT("\t float3 YV1 = lerp(XV1, XV2, frc.yyy);\n");
	OutHLSL += TEXT("\t float3 YV2 = lerp(XV3, XV4, frc.yyy);\n");
	OutHLSL += TEXT("\t Out_Value = lerp(YV1, YV2, frc.zzz);\n");
	OutHLSL += TEXT("\n}\n");
	return true;
}

// build buffer definition hlsl
// 1. Choose a buffer name, add the data interface ID (important!)
// 2. add a DIGPUBufferParamDescriptor to the array argument; that'll be passed on to the FNiagaraShader for binding to a shader param, that can
// then later be found by name via FindDIBufferParam for setting; 
// 3. store buffer declaration hlsl in OutHLSL
// multiple buffers can be defined at once here
//
void UNiagaraDataInterfaceCurlNoise::GetBufferDefinitionHLSL(FString DataInterfaceID, TArray<DIGPUBufferParamDescriptor> &BufferDescriptors, FString &OutHLSL)
{
	FString BufferName = "CurlNoiseLUT" + DataInterfaceID;
	OutHLSL += TEXT("Buffer<float4> ") + BufferName + TEXT(";\n");

	BufferDescriptors.Add(DIGPUBufferParamDescriptor(BufferName, 0));		// add a descriptor for shader parameter binding

}

// called after translate, to setup buffers matching the buffer descriptors generated during hlsl translation
// need to do this because the script used during translate is a clone, including its DIs
//
void UNiagaraDataInterfaceCurlNoise::SetupBuffers(TArray<DIGPUBufferParamDescriptor> &BufferDescriptors)
{
	for (DIGPUBufferParamDescriptor &Desc : BufferDescriptors)
	{
		FNiagaraDataInterfaceBufferData BufferData(*Desc.BufferParamName);	// store off the data for later use
		GPUBuffers.Add(BufferData);
	}
	bGPUBufferDirty = true;
}

// return the GPU buffer array (called from NiagaraInstanceBatcher to get the buffers for setting to the shader)
// we lazily update the buffer with a new LUT here if necessary
//
TArray<FNiagaraDataInterfaceBufferData> &UNiagaraDataInterfaceCurlNoise::GetBufferDataArray()
{
	check(IsInRenderingThread());

	if (bGPUBufferDirty)
	{
		check(GPUBuffers.Num() > 0);
		FNiagaraDataInterfaceBufferData &GPUBuffer = GPUBuffers[0];
		GPUBuffer.Buffer.Release();
		GPUBuffer.Buffer.Initialize(sizeof(float) * 4, 17 * 17 * 17, EPixelFormat::PF_A32B32G32R32F);
		uint32 BufferSize = 17 * 17 * 17 * sizeof(float) * 4;
		int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(GPUBuffer.Buffer.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
		FVector4 TempTable[17 * 17 * 17];
		for (int z = 0; z < 17; z++)
		{
			for (int y = 0; y < 17; y++)
			{
				for (int x = 0; x < 17; x++)
				{
					float *R = (float*)( &(NoiseTable[x][y][z]) );
					TempTable[x + y * 17 + z * 17 * 17] = FVector4(R[0], R[1], R[2], 0.0f);
				}
			}
		}
		FPlatformMemory::Memcpy(BufferData, TempTable, BufferSize);
		//		FPlatformMemory::Memcpy(BufferData, NoiseTable, BufferSize);
		RHIUnlockVertexBuffer(GPUBuffer.Buffer.Buffer);
		bGPUBufferDirty = false;
	}


	return GPUBuffers;
}




void UNiagaraDataInterfaceCurlNoise::InitNoiseLUT()
{
	// seed random stream
	FRandomStream RandStream;

	// random noise
	float TempTable[17][17][17];
	for (int z = 0; z < 17; z++)
	{
		for (int y = 0; y < 17; y++)
		{
			for (int x = 0; x < 17; x++)
			{
				float f1 = RandStream.FRandRange(-1.0f, 1.0f);
				TempTable[x][y][z] = f1;
			}
		}
	}

	// pad
	for (int i = 0; i < 17; i++)
	{
		for (int j = 0; j < 17; j++)
		{
			TempTable[i][j][16] = TempTable[i][j][0];
			TempTable[i][16][j] = TempTable[i][0][j];
			TempTable[16][j][i] = TempTable[0][j][i];
		}
	}

	// compute gradients
	FVector TempTable2[17][17][17];
	for (int z = 0; z < 16; z++)
	{
		for (int y = 0; y < 16; y++)
		{
			for (int x = 0; x < 16; x++)
			{
				FVector XGrad = FVector(1.0f, 0.0f, TempTable[x][y][z] - TempTable[x + 1][y][z]);
				FVector YGrad = FVector(0.0f, 1.0f, TempTable[x][y][z] - TempTable[x][y + 1][z]);
				FVector ZGrad = FVector(0.0f, 1.0f, TempTable[x][y][z] - TempTable[x][y][z + 1]);

				FVector Grad = FVector(XGrad.Z, YGrad.Z, ZGrad.Z);
				TempTable2[x][y][z] = Grad;
			}
		}
	}

	// pad
	for (int i = 0; i < 17; i++)
	{
		for (int j = 0; j < 17; j++)
		{
			TempTable2[i][j][16] = TempTable2[i][j][0];
			TempTable2[i][16][j] = TempTable2[i][0][j];
			TempTable2[16][j][i] = TempTable2[0][j][i];
		}
	}


	// compute curl of gradient field
	for (int z = 0; z < 16; z++)
	{
		for (int y = 0; y < 16; y++)
		{
			for (int x = 0; x < 16; x++)
			{
				FVector Dy = TempTable2[x][y][z] - TempTable2[x][y + 1][z];
				FVector Sy = TempTable2[x][y][z] + TempTable2[x][y + 1][z];
				FVector Dx = TempTable2[x][y][z] - TempTable2[x + 1][y][z];
				FVector Sx = TempTable2[x][y][z] + TempTable2[x + 1][y][z];
				FVector Dz = TempTable2[x][y][z] - TempTable2[x][y][z + 1];
				FVector Sz = TempTable2[x][y][z] + TempTable2[x][y][z + 1];
				FVector Dir = FVector(Dy.Z - Sz.Y, Dz.X - Sx.Z, Dx.Y - Sy.X);

				NoiseTable[x][y][z] = MakeVectorRegister(Dir.X, Dir.Y, Dir.Z, 0.0f);
			}
		}
	}

	// pad
	for (int i = 0; i < 17; i++)
	{
		for (int j = 0; j < 17; j++)
		{
			NoiseTable[i][j][16] = NoiseTable[i][j][0];
			NoiseTable[i][16][j] = NoiseTable[i][0][j];
			NoiseTable[16][j][i] = NoiseTable[0][j][i];
		}
	}

}
