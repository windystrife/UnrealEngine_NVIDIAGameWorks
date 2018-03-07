// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderingCompositionGraph.cpp: Scene pass order and dependency system.
=============================================================================*/

#include "PostProcess/RenderingCompositionGraph.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "PostProcess/RenderTargetPool.h"
#include "RendererModule.h"
#include "HighResScreenshot.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRendering.h"

void ExecuteCompositionGraphDebug();

static TAutoConsoleVariable<int32> CVarCompositionGraphOrder(
	TEXT("r.CompositionGraphOrder"),
	1,
	TEXT("Defines in which order the nodes in the CompositionGraph are executed (affects postprocess and some lighting).\n")
	TEXT("Option 1 provides more control, which can be useful for preserving ESRAM, avoid GPU sync, cluster up compute shaders for performance and control AsyncCompute.\n")
	TEXT(" 0: tree order starting with the root, first all inputs then dependencies (classic UE4, unconnected nodes are not getting executed)\n")
	TEXT(" 1: RegisterPass() call order, unless the dependencies (input and additional) require a different order (might become new default as it provides more control, executes all registered nodes)"),
	ECVF_RenderThreadSafe);

#if !UE_BUILD_SHIPPING
FAutoConsoleCommand CmdCompositionGraphDebug(
	TEXT("r.CompositionGraphDebug"),
	TEXT("Execute this command to get a single frame dump of the composition graph of one frame (post processing and lighting)."),
	FConsoleCommandDelegate::CreateStatic(ExecuteCompositionGraphDebug)
	);
#endif


// render thread, 0:off, >0 next n frames should be debugged
uint32 GDebugCompositionGraphFrames = 0;

class FGMLFileWriter
{
public:
	// constructor
	FGMLFileWriter()
		: GMLFile(0)
	{		
	}

	void OpenGMLFile(const TCHAR* Name)
	{
#if !UE_BUILD_SHIPPING
		// todo: do we need to create the directory?
		FString FilePath = FPaths::ScreenShotDir() + TEXT("/") + Name + TEXT(".gml");
		GMLFile = IFileManager::Get().CreateDebugFileWriter(*FilePath);
#endif
	}

	void CloseGMLFile()
	{
#if !UE_BUILD_SHIPPING
		if(GMLFile)
		{
			delete GMLFile;
			GMLFile = 0;
		}
#endif
	}

	// .GML file is to visualize the post processing graph as a 2d graph
	void WriteLine(const char* Line)
	{
#if !UE_BUILD_SHIPPING
		if(GMLFile)
		{
			GMLFile->Serialize((void*)Line, FCStringAnsi::Strlen(Line));
			GMLFile->Serialize((void*)"\r\n", 2);
		}
#endif
	}

private:
	FArchive* GMLFile;
};

FGMLFileWriter GGMLFileWriter;


bool ShouldDebugCompositionGraph()
{
#if !UE_BUILD_SHIPPING
	return GDebugCompositionGraphFrames > 0;
#else 
	return false;
#endif
}

void Test()
{
	struct ObjectSize4
	{
		void SetBaseValues(){}
		static FName GetFName()
		{
			static const FName Name(TEXT("ObjectSize4"));
			return Name;
		}
		uint8 Data[4];
	};
 
	MS_ALIGN(16) struct ObjectAligned16
	{
		void SetBaseValues(){}
		static FName GetFName()
		{
			static const FName Name(TEXT("ObjectAligned16"));
			return Name;
		}
		uint8 Data[16];
	} GCC_ALIGN(16);

	// https://udn.unrealengine.com/questions/274066/fblendablemanager-returning-wrong-or-misaligned-da.html
	FBlendableManager Manager;
	Manager.GetSingleFinalData<ObjectSize4>();
	ObjectAligned16& AlignedData = Manager.GetSingleFinalData<ObjectAligned16>();

	check((reinterpret_cast<ptrdiff_t>(&AlignedData) & 16) == 0);
}

void ExecuteCompositionGraphDebug()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		StartDebugCompositionGraph,
	{
		GDebugCompositionGraphFrames = 1;
		Test();
	}
	);
}

// main thread
void CompositionGraph_OnStartFrame()
{
#if !UE_BUILD_SHIPPING
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		DebugCompositionGraphDec,
	{
		if(GDebugCompositionGraphFrames)
		{
			--GDebugCompositionGraphFrames;
		}
	}
	);		
#endif
}

FRenderingCompositePassContext::FRenderingCompositePassContext(FRHICommandListImmediate& InRHICmdList, const FViewInfo& InView)
	: View(InView)
	, ViewState((FSceneViewState*)InView.State)
	, Pass(0)
	, RHICmdList(InRHICmdList)
	, ViewPortRect(0, 0, 0 ,0)
	, FeatureLevel(View.GetFeatureLevel())
	, ShaderMap(InView.ShaderMap)
	, bWasProcessed(false)
	, bHasHmdMesh(false)
{
	check(!IsViewportValid());
}

FRenderingCompositePassContext::~FRenderingCompositePassContext()
{
	Graph.Free();
}

void FRenderingCompositePassContext::Process(FRenderingCompositePass* Root, const TCHAR *GraphDebugName)
{
	// call this method only once after the graph is finished
	check(!bWasProcessed);

	bWasProcessed = true;

	// query if we have a custom HMD post process mesh to use
	static const auto* const HiddenAreaMaskCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.HiddenAreaMask"));
	bHasHmdMesh = (HiddenAreaMaskCVar != nullptr &&
		HiddenAreaMaskCVar->GetValueOnRenderThread() == 1 &&
		GEngine &&
		GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() &&
		GEngine->XRSystem->GetHMDDevice()->HasVisibleAreaMesh());

	if(Root)
	{
		if(ShouldDebugCompositionGraph())
		{
			UE_LOG(LogConsoleResponse,Log, TEXT(""));
			UE_LOG(LogConsoleResponse,Log, TEXT("FRenderingCompositePassContext:Debug '%s' ---------"), GraphDebugName);
			UE_LOG(LogConsoleResponse,Log, TEXT(""));

			GGMLFileWriter.OpenGMLFile(GraphDebugName);
			GGMLFileWriter.WriteLine("Creator \"UnrealEngine4\"");
			GGMLFileWriter.WriteLine("Version \"2.10\"");
			GGMLFileWriter.WriteLine("graph");
			GGMLFileWriter.WriteLine("[");
			GGMLFileWriter.WriteLine("\tcomment\t\"This file can be viewed with yEd from yWorks. Run Layout/Hierarchical after loading.\"");
			GGMLFileWriter.WriteLine("\thierarchic\t1");
			GGMLFileWriter.WriteLine("\tdirected\t1");
		}

		bool bNewOrder = CVarCompositionGraphOrder.GetValueOnRenderThread() != 0;

		Graph.RecursivelyGatherDependencies(Root);

		if(bNewOrder)
		{
			// process in the order the nodes have been created (for more control), unless the dependencies require it differently
			for (FRenderingCompositePass* Node : Graph.Nodes)
			{
				// only if this is true the node is actually needed - no need to compute it when it's not needed
				if(Node->WasComputeOutputDescCalled())
				{
					Graph.RecursivelyProcess(Node, *this);
				}
			}
		}
		else
		{
			// process in the order of the dependencies, starting from the root (without processing unreferenced nodes)
			Graph.RecursivelyProcess(Root, *this);
		}

		if(ShouldDebugCompositionGraph())
		{
			UE_LOG(LogConsoleResponse,Log, TEXT(""));

			GGMLFileWriter.WriteLine("]");
			GGMLFileWriter.CloseGMLFile();
		}
	}
}

// --------------------------------------------------------------------------

FRenderingCompositionGraph::FRenderingCompositionGraph()
{
}

FRenderingCompositionGraph::~FRenderingCompositionGraph()
{
	Free();
}

void FRenderingCompositionGraph::Free()
{
	for(uint32 i = 0; i < (uint32)Nodes.Num(); ++i)
	{
		FRenderingCompositePass *Element = Nodes[i];
		if (FMemStack::Get().ContainsPointer(Element))
		{
			Element->~FRenderingCompositePass();
		}
		else
		{
			// Call release on non-stack allocated elements
			Element->Release();
		}
	}

	Nodes.Empty();
}

void FRenderingCompositionGraph::RecursivelyGatherDependencies(FRenderingCompositePass *Pass)
{
	checkSlow(Pass);

	if(Pass->bComputeOutputDescWasCalled)
	{
		// already processed
		return;
	}
	Pass->bComputeOutputDescWasCalled = true;

	// iterate through all inputs and additional dependencies of this pass
	uint32 Index = 0;
	while(const FRenderingCompositeOutputRef* OutputRefIt = Pass->GetDependency(Index++))
	{
		FRenderingCompositeOutput* InputOutput = OutputRefIt->GetOutput();
				
		if(InputOutput)
		{
			// add a dependency to this output as we are referencing to it
			InputOutput->AddDependency();
		}
		
		if(FRenderingCompositePass* OutputRefItPass = OutputRefIt->GetPass())
		{
			// recursively process all inputs of this Pass
			RecursivelyGatherDependencies(OutputRefItPass);
		}
	}

	// the pass is asked what the intermediate surface/texture format needs to be for all its outputs.
	for(uint32 OutputId = 0; ; ++OutputId)
	{
		EPassOutputId PassOutputId = (EPassOutputId)(OutputId);
		FRenderingCompositeOutput* Output = Pass->GetOutput(PassOutputId);

		if(!Output)
		{
			break;
		}

		Output->RenderTargetDesc = Pass->ComputeOutputDesc(PassOutputId);

		// Allow format overrides for high-precision work
		static const auto CVarPostProcessingColorFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessingColorFormat"));

		if (CVarPostProcessingColorFormat->GetValueOnRenderThread() == 1)
		{
			if (Output->RenderTargetDesc.Format == PF_FloatRGBA ||
				Output->RenderTargetDesc.Format == PF_FloatRGB ||
				Output->RenderTargetDesc.Format == PF_FloatR11G11B10)
			{
				Output->RenderTargetDesc.Format = PF_A32B32G32R32F;
			}
		}
	}
}

template<typename TColor> struct TAsyncBufferWrite;

struct FAsyncBufferWriteQueue
{
	template<typename T>
	static TFuture<void> Dispatch(TAsyncBufferWrite<T>&& In)
	{
		NumInProgressWrites.Increment();

		while (NumInProgressWrites.GetValue() >= MaxAsyncWrites)
		{
			// Yield until we can write another
			FPlatformProcess::Sleep(0.f);
		}

		return Async<void>(EAsyncExecution::ThreadPool, MoveTemp(In));
	}
	
	static FThreadSafeCounter NumInProgressWrites;
	static const int32 MaxAsyncWrites = 6;
};
FThreadSafeCounter FAsyncBufferWriteQueue::NumInProgressWrites;

/** Callable type used to save a color buffer on an async task without allocating/copying into a new one */
template<typename TColor>
struct TAsyncBufferWrite
{
	TAsyncBufferWrite(FString InFilename, FIntPoint InDestSize, TArray<TColor> InBitmap) : Filename(MoveTemp(InFilename)), DestSize(InDestSize), Bitmap(MoveTemp(InBitmap)) {}

	TAsyncBufferWrite(TAsyncBufferWrite&& In) : Filename(MoveTemp(In.Filename)), DestSize(In.DestSize), Bitmap(MoveTemp(In.Bitmap)) {}
	TAsyncBufferWrite& operator=(TAsyncBufferWrite&& In) { Filename = MoveTemp(In.Filename); DestSize = In.DestSize; Bitmap = MoveTemp(In.Bitmap); return *this; }

	/** Call operator that saves the color buffer data */
	void operator()()
	{
		FString ResultPath;
		GetHighResScreenshotConfig().SaveImage(Filename, Bitmap, DestSize, &ResultPath);
		UE_LOG(LogConsoleResponse, Display, TEXT("Content was saved to \"%s\""), *ResultPath);

		FAsyncBufferWriteQueue::NumInProgressWrites.Decrement();
	}

	/** Copy semantics are only defined to appease TFunction, whose virtual CloneByCopy function is always instantiated, even if the TFunction itself is never copied */
	TAsyncBufferWrite(const TAsyncBufferWrite& In) : Filename(In.Filename), DestSize(In.DestSize), Bitmap(In.Bitmap) { ensureMsgf(false, TEXT("Type should not be copied")); }
	TAsyncBufferWrite& operator=(const TAsyncBufferWrite& In) { Filename = In.Filename; DestSize = In.DestSize; Bitmap = In.Bitmap; ensureMsgf(false, TEXT("Type should not be copied")); return *this; }

private:

	/** The filename to save to */
	FString Filename;
	/** The size of the bitmap */
	FIntPoint DestSize;
	/** The bitmap data itself */
	TArray<TColor> Bitmap;
};

TFuture<void> FRenderingCompositionGraph::DumpOutputToFile(FRenderingCompositePassContext& Context, const FString& Filename, FRenderingCompositeOutput* Output) const
{
	FSceneRenderTargetItem& RenderTargetItem = Output->PooledRenderTarget->GetRenderTargetItem();
	FHighResScreenshotConfig& HighResScreenshotConfig = GetHighResScreenshotConfig();
	FTextureRHIRef Texture = RenderTargetItem.TargetableTexture ? RenderTargetItem.TargetableTexture : RenderTargetItem.ShaderResourceTexture;
	check(Texture);
	check(Texture->GetTexture2D());

	FIntRect SourceRect = Context.View.ViewRect;

	int32 MSAAXSamples = Texture->GetNumSamples();

	if (GIsHighResScreenshot && HighResScreenshotConfig.CaptureRegion.Area())
	{
		SourceRect = HighResScreenshotConfig.CaptureRegion;
	}

	SourceRect.Min.X *= MSAAXSamples;
	SourceRect.Max.X *= MSAAXSamples;

	FIntPoint DestSize(SourceRect.Width(), SourceRect.Height());

	EPixelFormat PixelFormat = Texture->GetFormat();
	
	switch (PixelFormat)
	{
		case PF_FloatRGBA:
		{
			TArray<FFloat16Color> Bitmap;
			Context.RHICmdList.ReadSurfaceFloatData(Texture, SourceRect, Bitmap, (ECubeFace)0, 0, 0);

			return FAsyncBufferWriteQueue::Dispatch(TAsyncBufferWrite<FFloat16Color>(Filename, DestSize, MoveTemp(Bitmap)));
		}

		case PF_A32B32G32R32F:
		{
			FReadSurfaceDataFlags ReadDataFlags(RCM_MinMax);
			ReadDataFlags.SetLinearToGamma(false);
			TArray<FLinearColor> Bitmap;
			Context.RHICmdList.ReadSurfaceData(Texture, SourceRect, Bitmap, ReadDataFlags);

			return FAsyncBufferWriteQueue::Dispatch(TAsyncBufferWrite<FLinearColor>(Filename, DestSize, MoveTemp(Bitmap)));
		}

		case PF_R8G8B8A8:
		case PF_B8G8R8A8:
		{
			FReadSurfaceDataFlags ReadDataFlags;
			ReadDataFlags.SetLinearToGamma(false);
			TArray<FColor> Bitmap;
			Context.RHICmdList.ReadSurfaceData(Texture, SourceRect, Bitmap, ReadDataFlags);
			FColor* Pixel = Bitmap.GetData();
			for (int32 i = 0, Count = Bitmap.Num(); i < Count; i++, Pixel++)
			{
				Pixel->A = 255;
			}

			return FAsyncBufferWriteQueue::Dispatch(TAsyncBufferWrite<FColor>(Filename, DestSize, MoveTemp(Bitmap)));
		}
	}

	return TFuture<void>();
}

void FRenderingCompositionGraph::RecursivelyProcess(const FRenderingCompositeOutputRef& InOutputRef, FRenderingCompositePassContext& Context) const
{
	FRenderingCompositePass *Pass = InOutputRef.GetPass();
	FRenderingCompositeOutput *Output = InOutputRef.GetOutput();

#if !UE_BUILD_SHIPPING
	if(!Pass || !Output)
	{
		// to track down a crash bug
		if(Context.Pass)
		{
			UE_LOG(LogRenderer,Fatal, TEXT("FRenderingCompositionGraph::RecursivelyProcess %s"), *Context.Pass->ConstructDebugName());
		}
	}
#endif

	check(Pass);
	check(Output);

	if(Pass->bProcessWasCalled)
	{
		// already processed
		return;
	}
	Pass->bProcessWasCalled = true;

	// iterate through all inputs and additional dependencies of this pass
	{
		uint32 Index = 0;

		while(const FRenderingCompositeOutputRef* OutputRefIt = Pass->GetDependency(Index++))
		{
			if(OutputRefIt->GetPass())
			{
				if(!OutputRefIt)
				{
					// Pass doesn't have more inputs
					break;
				}

				FRenderingCompositeOutput* Input = OutputRefIt->GetOutput();
				
				// to track down an issue, should never happen
				check(OutputRefIt->GetPass());

				if(GRenderTargetPool.IsEventRecordingEnabled())
				{
					GRenderTargetPool.AddPhaseEvent(*Pass->ConstructDebugName());
				}

				Context.Pass = Pass;
				RecursivelyProcess(*OutputRefIt, Context);
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(ShouldDebugCompositionGraph())
	{
		GGMLFileWriter.WriteLine("\tnode");
		GGMLFileWriter.WriteLine("\t[");

		int32 PassId = ComputeUniquePassId(Pass);
		FString PassDebugName = Pass->ConstructDebugName();

		ANSICHAR Line[MAX_SPRINTF];

		{
			GGMLFileWriter.WriteLine("\t\tgraphics");
			GGMLFileWriter.WriteLine("\t\t[");
			FCStringAnsi::Sprintf(Line, "\t\t\tw\t%d", 200);
			GGMLFileWriter.WriteLine(Line);
			FCStringAnsi::Sprintf(Line, "\t\t\th\t%d", 80);
			GGMLFileWriter.WriteLine(Line);
			GGMLFileWriter.WriteLine("\t\t\tfill\t\"#FFCCCC\"");
			GGMLFileWriter.WriteLine("\t\t]");
		}

		{
			FCStringAnsi::Sprintf(Line, "\t\tid\t%d", PassId);
			GGMLFileWriter.WriteLine(Line);
			GGMLFileWriter.WriteLine("\t\tLabelGraphics");
			GGMLFileWriter.WriteLine("\t\t[");
			FCStringAnsi::Sprintf(Line, "\t\t\ttext\t\"#%d\r%s\"", PassId, (const char *)TCHAR_TO_ANSI(*PassDebugName));
			GGMLFileWriter.WriteLine(Line);
			GGMLFileWriter.WriteLine("\t\t\tanchor\t\"t\"");	// put label internally on top
			GGMLFileWriter.WriteLine("\t\t\tfontSize\t14");
			GGMLFileWriter.WriteLine("\t\t\tfontStyle\t\"bold\"");
			GGMLFileWriter.WriteLine("\t\t]");
		}

		UE_LOG(LogConsoleResponse,Log, TEXT("Node#%d '%s'"), PassId, *PassDebugName);
	
		GGMLFileWriter.WriteLine("\t\tisGroup\t1");
		GGMLFileWriter.WriteLine("\t]");

		uint32 InputId = 0;
		while(FRenderingCompositeOutputRef* OutputRefIt = Pass->GetInput((EPassInputId)(InputId++)))
		{
			if(OutputRefIt->Source)
			{
				// source is hooked up 
				FString InputName = OutputRefIt->Source->ConstructDebugName();

				int32 TargetPassId = ComputeUniquePassId(OutputRefIt->Source);

				UE_LOG(LogConsoleResponse,Log, TEXT("  ePId_Input%d: Node#%d @ ePId_Output%d '%s'"), InputId - 1, TargetPassId, (uint32)OutputRefIt->PassOutputId, *InputName);

				// input connection to another node
				{
					GGMLFileWriter.WriteLine("\tedge");
					GGMLFileWriter.WriteLine("\t[");
					{
						FCStringAnsi::Sprintf(Line, "\t\tsource\t%d", ComputeUniqueOutputId(OutputRefIt->Source, OutputRefIt->PassOutputId));
						GGMLFileWriter.WriteLine(Line);
						FCStringAnsi::Sprintf(Line, "\t\ttarget\t%d", PassId);
						GGMLFileWriter.WriteLine(Line);
					}
					{
						FString EdgeName = FString::Printf(TEXT("ePId_Input%d"), InputId - 1);

						GGMLFileWriter.WriteLine("\t\tLabelGraphics");
						GGMLFileWriter.WriteLine("\t\t[");
						FCStringAnsi::Sprintf(Line, "\t\t\ttext\t\"%s\"", (const char *)TCHAR_TO_ANSI(*EdgeName));
						GGMLFileWriter.WriteLine(Line);
						GGMLFileWriter.WriteLine("\t\t\tmodel\t\"three_center\"");
						GGMLFileWriter.WriteLine("\t\t\tposition\t\"tcentr\"");
						GGMLFileWriter.WriteLine("\t\t]");
					}
					GGMLFileWriter.WriteLine("\t]");
				}
			}
			else
			{
				// source is not hooked up 
				UE_LOG(LogConsoleResponse,Log, TEXT("  ePId_Input%d:"), InputId - 1);
			}
		}

		uint32 DepId = 0;
		while(FRenderingCompositeOutputRef* OutputRefIt = Pass->GetAdditionalDependency(DepId++))
		{
			check(OutputRefIt->Source);

			FString InputName = OutputRefIt->Source->ConstructDebugName();
			int32 TargetPassId = ComputeUniquePassId(OutputRefIt->Source);

			UE_LOG(LogConsoleResponse,Log, TEXT("  Dependency: Node#%d @ ePId_Output%d '%s'"), TargetPassId, (uint32)OutputRefIt->PassOutputId, *InputName);

			// dependency connection to another node
			{
				GGMLFileWriter.WriteLine("\tedge");
				GGMLFileWriter.WriteLine("\t[");
				{
					FCStringAnsi::Sprintf(Line, "\t\tsource\t%d", ComputeUniqueOutputId(OutputRefIt->Source, OutputRefIt->PassOutputId));
					GGMLFileWriter.WriteLine(Line);
					FCStringAnsi::Sprintf(Line, "\t\ttarget\t%d", PassId);
					GGMLFileWriter.WriteLine(Line);
				}
				// dashed line
				{
					GGMLFileWriter.WriteLine("\t\tgraphics");
					GGMLFileWriter.WriteLine("\t\t[");
					GGMLFileWriter.WriteLine("\t\t\tstyle\t\"dashed\"");
					GGMLFileWriter.WriteLine("\t\t]");
				}
				{
					FString EdgeName = TEXT("Dependency");

					GGMLFileWriter.WriteLine("\t\tLabelGraphics");
					GGMLFileWriter.WriteLine("\t\t[");
					FCStringAnsi::Sprintf(Line, "\t\t\ttext\t\"%s\"", (const char *)TCHAR_TO_ANSI(*EdgeName));
					GGMLFileWriter.WriteLine(Line);
					GGMLFileWriter.WriteLine("\t\t\tmodel\t\"three_center\"");
					GGMLFileWriter.WriteLine("\t\t\tposition\t\"tcentr\"");
					GGMLFileWriter.WriteLine("\t\t]");
				}
				GGMLFileWriter.WriteLine("\t]");
			}
		}

		uint32 OutputId = 0;
		while(FRenderingCompositeOutput* PassOutput = Pass->GetOutput((EPassOutputId)(OutputId)))
		{
			UE_LOG(LogConsoleResponse,Log, TEXT("  ePId_Output%d %s %s Dep: %d"), OutputId, *PassOutput->RenderTargetDesc.GenerateInfoString(), PassOutput->RenderTargetDesc.DebugName, PassOutput->GetDependencyCount());

			GGMLFileWriter.WriteLine("\tnode");
			GGMLFileWriter.WriteLine("\t[");

			{
				GGMLFileWriter.WriteLine("\t\tgraphics");
				GGMLFileWriter.WriteLine("\t\t[");
				FCStringAnsi::Sprintf(Line, "\t\t\tw\t%d", 220);
				GGMLFileWriter.WriteLine(Line);
				FCStringAnsi::Sprintf(Line, "\t\t\th\t%d", 40);
				GGMLFileWriter.WriteLine(Line);
				GGMLFileWriter.WriteLine("\t\t]");
			}

			{
				FCStringAnsi::Sprintf(Line, "\t\tid\t%d", ComputeUniqueOutputId(Pass, (EPassOutputId)(OutputId)));
				GGMLFileWriter.WriteLine(Line);
				GGMLFileWriter.WriteLine("\t\tLabelGraphics");
				GGMLFileWriter.WriteLine("\t\t[");
				FCStringAnsi::Sprintf(Line, "\t\t\ttext\t\"ePId_Output%d '%s'\r%s\"", 
					OutputId, 
					(const char *)TCHAR_TO_ANSI(PassOutput->RenderTargetDesc.DebugName),
					(const char *)TCHAR_TO_ANSI(*PassOutput->RenderTargetDesc.GenerateInfoString()));
				GGMLFileWriter.WriteLine(Line);
				GGMLFileWriter.WriteLine("\t\t]");
			}


			{
				FCStringAnsi::Sprintf(Line, "\t\tgid\t%d", PassId);
				GGMLFileWriter.WriteLine(Line);
			}

			GGMLFileWriter.WriteLine("\t]");

			++OutputId;
		}

		UE_LOG(LogConsoleResponse,Log, TEXT(""));
	}
#endif

	Context.Pass = Pass;
	Context.SetViewportInvalid();

	// then process the pass itself
	Pass->Process(Context);

	// for VisualizeTexture and output buffer dumping
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		uint32 OutputId = 0;

		while(FRenderingCompositeOutput* PassOutput = Pass->GetOutput((EPassOutputId)OutputId))
		{
			// use intermediate texture unless it's the last one where we render to the final output
			if(PassOutput->PooledRenderTarget)
			{
				GRenderTargetPool.VisualizeTexture.SetCheckPoint(Context.RHICmdList, PassOutput->PooledRenderTarget);

				// If this buffer was given a dump filename, write it out
				const FString& Filename = Pass->GetOutputDumpFilename((EPassOutputId)OutputId);
				if (!Filename.IsEmpty())
				{
					DumpOutputToFile(Context, Filename, PassOutput);
				}

				// If we've been asked to write out the pixel data for this pass to an external array, do it now
				TArray<FColor>* OutputColorArray = Pass->GetOutputColorArray((EPassOutputId)OutputId);
				if (OutputColorArray)
				{
					Context.RHICmdList.ReadSurfaceData(
						PassOutput->PooledRenderTarget->GetRenderTargetItem().TargetableTexture,
						Context.View.ViewRect,
						*OutputColorArray,
						FReadSurfaceDataFlags()
						);
				}
			}

			OutputId++;
		}
	}
#endif

	// iterate through all inputs of this pass and decrement the references for it's inputs
	// this can release some intermediate RT so they can be reused
	{
		uint32 InputId = 0;

		while(const FRenderingCompositeOutputRef* OutputRefIt = Pass->GetDependency(InputId++))
		{
			FRenderingCompositeOutput* Input = OutputRefIt->GetOutput();

			if(Input)
			{
				Input->ResolveDependencies();
			}
		}
	}
}

// for debugging purpose O(n)
int32 FRenderingCompositionGraph::ComputeUniquePassId(FRenderingCompositePass* Pass) const
{
	for(uint32 i = 0; i < (uint32)Nodes.Num(); ++i)
	{
		FRenderingCompositePass *Element = Nodes[i];

		if(Element == Pass)
		{
			return i;
		}
	}

	return -1;
}

int32 FRenderingCompositionGraph::ComputeUniqueOutputId(FRenderingCompositePass* Pass, EPassOutputId OutputId) const
{
	uint32 Ret = Nodes.Num();

	for(uint32 i = 0; i < (uint32)Nodes.Num(); ++i)
	{
		FRenderingCompositePass *Element = Nodes[i];

		if(Element == Pass)
		{
			return (int32)(Ret + (uint32)OutputId);
		}

		uint32 OutputCount = 0;
		while(Pass->GetOutput((EPassOutputId)OutputCount))
		{
			++OutputCount;
		}

		Ret += OutputCount;
	}

	return -1;
}

FRenderingCompositeOutput *FRenderingCompositeOutputRef::GetOutput() const
{
	if(Source == 0)
	{
		return 0;
	}

	return Source->GetOutput(PassOutputId); 
}

FRenderingCompositePass* FRenderingCompositeOutputRef::GetPass() const
{
	return Source;
}

// -----------------------------------------------------------------

void FPostProcessPassParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	BilinearTextureSampler0.Bind(ParameterMap,TEXT("BilinearTextureSampler0"));
	BilinearTextureSampler1.Bind(ParameterMap,TEXT("BilinearTextureSampler1"));
	ViewportSize.Bind(ParameterMap,TEXT("ViewportSize"));
	ViewportRect.Bind(ParameterMap,TEXT("ViewportRect"));
	ScreenPosToPixel.Bind(ParameterMap,TEXT("ScreenPosToPixel"));
	
	for(uint32 i = 0; i < ePId_Input_MAX; ++i)
	{
		PostprocessInputParameter[i].Bind(ParameterMap, *FString::Printf(TEXT("PostprocessInput%d"), i));
		PostprocessInputParameterSampler[i].Bind(ParameterMap, *FString::Printf(TEXT("PostprocessInput%dSampler"), i));
		PostprocessInputSizeParameter[i].Bind(ParameterMap, *FString::Printf(TEXT("PostprocessInput%dSize"), i));
		PostProcessInputMinMaxParameter[i].Bind(ParameterMap, *FString::Printf(TEXT("PostprocessInput%dMinMax"), i));
	}
}

template <typename TRHICmdList>
void FPostProcessPassParameters::SetPS(TRHICmdList& RHICmdList, const FPixelShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context, FSamplerStateRHIParamRef Filter, EFallbackColor FallbackColor, FSamplerStateRHIParamRef* FilterOverrideArray)
{
	Set(RHICmdList, ShaderRHI, Context, Filter, FallbackColor, FilterOverrideArray);
}

template void FPostProcessPassParameters::SetPS(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context, FSamplerStateRHIParamRef Filter, EFallbackColor FallbackColor, FSamplerStateRHIParamRef* FilterOverrideArray);
template void FPostProcessPassParameters::SetPS(FRHICommandListImmediate& RHICmdList, const FPixelShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context, FSamplerStateRHIParamRef Filter, EFallbackColor FallbackColor, FSamplerStateRHIParamRef* FilterOverrideArray);

template< typename TRHICmdList >
void FPostProcessPassParameters::SetCS(const FComputeShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context, TRHICmdList& RHICmdList, FSamplerStateRHIParamRef Filter, EFallbackColor FallbackColor, FSamplerStateRHIParamRef* FilterOverrideArray)
{
	Set(RHICmdList, ShaderRHI, Context, Filter, FallbackColor, FilterOverrideArray);
}
template void FPostProcessPassParameters::SetCS< FRHICommandListImmediate >(
	const FComputeShaderRHIParamRef& ShaderRHI,
	const FRenderingCompositePassContext& Context,
	FRHICommandListImmediate& RHICmdList,
	FSamplerStateRHIParamRef Filter,
	EFallbackColor FallbackColor,
	FSamplerStateRHIParamRef* FilterOverrideArray
	);

template void FPostProcessPassParameters::SetCS< FRHIAsyncComputeCommandListImmediate >(
	const FComputeShaderRHIParamRef& ShaderRHI,
	const FRenderingCompositePassContext& Context,
	FRHIAsyncComputeCommandListImmediate& RHICmdList,
	FSamplerStateRHIParamRef Filter,
	EFallbackColor FallbackColor,
	FSamplerStateRHIParamRef* FilterOverrideArray
	);

void FPostProcessPassParameters::SetVS(const FVertexShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context, FSamplerStateRHIParamRef Filter, EFallbackColor FallbackColor, FSamplerStateRHIParamRef* FilterOverrideArray)
{
	Set(Context.RHICmdList, ShaderRHI, Context, Filter, FallbackColor, FilterOverrideArray);
}

template< typename TShaderRHIParamRef, typename TRHICmdList >
void FPostProcessPassParameters::Set(
	TRHICmdList& RHICmdList,
	const TShaderRHIParamRef& ShaderRHI,
	const FRenderingCompositePassContext& Context,
	FSamplerStateRHIParamRef Filter,
	EFallbackColor FallbackColor,
	FSamplerStateRHIParamRef* FilterOverrideArray)
{
	// assuming all outputs have the same size
	FRenderingCompositeOutput* Output = Context.Pass->GetOutput(ePId_Output0);

	// Output0 should always exist
	check(Output);

	// one should be on
	check(FilterOverrideArray || Filter);
	// but not both
	check(!FilterOverrideArray || !Filter);

	if(BilinearTextureSampler0.IsBound())
	{
		RHICmdList.SetShaderSampler(
			ShaderRHI, 
			BilinearTextureSampler0.GetBaseIndex(), 
			TStaticSamplerState<SF_Bilinear>::GetRHI()
			);
	}

	if(BilinearTextureSampler1.IsBound())
	{
		RHICmdList.SetShaderSampler(
			ShaderRHI, 
			BilinearTextureSampler1.GetBaseIndex(), 
			TStaticSamplerState<SF_Bilinear>::GetRHI()
			);
	}

	if(ViewportSize.IsBound() || ScreenPosToPixel.IsBound() || ViewportRect.IsBound())
	{
		FIntRect LocalViewport = Context.GetViewport();

		FIntPoint ViewportOffset = LocalViewport.Min;
		FIntPoint ViewportExtent = LocalViewport.Size();

		{
			FVector4 Value(ViewportExtent.X, ViewportExtent.Y, 1.0f / ViewportExtent.X, 1.0f / ViewportExtent.Y);

			SetShaderValue(RHICmdList, ShaderRHI, ViewportSize, Value);
		}

		{
			SetShaderValue(RHICmdList, ShaderRHI, ViewportRect, Context.GetViewport());
		}

		{
			FVector4 ScreenPosToPixelValue(
				ViewportExtent.X * 0.5f,
				-ViewportExtent.Y * 0.5f, 
				ViewportExtent.X * 0.5f - 0.5f + ViewportOffset.X,
				ViewportExtent.Y * 0.5f - 0.5f + ViewportOffset.Y);
			SetShaderValue(RHICmdList, ShaderRHI, ScreenPosToPixel, ScreenPosToPixelValue);
		}
	}

	//Calculate a base scene texture min max which will be pulled in by a pixel for each PP input.
	FIntRect ContextViewportRect = Context.IsViewportValid() ? Context.GetViewport() : FIntRect(0,0,0,0);
	const FIntPoint SceneRTSize = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY();
	FVector4 BaseSceneTexMinMax(	((float)ContextViewportRect.Min.X/SceneRTSize.X), 
									((float)ContextViewportRect.Min.Y/SceneRTSize.Y), 
									((float)ContextViewportRect.Max.X/SceneRTSize.X), 
									((float)ContextViewportRect.Max.Y/SceneRTSize.Y) );

	IPooledRenderTarget* FallbackTexture = 0;
	
	switch(FallbackColor)
	{
		case eFC_0000: FallbackTexture = GSystemTextures.BlackDummy; break;
		case eFC_0001: FallbackTexture = GSystemTextures.BlackAlphaOneDummy; break;
		case eFC_1111: FallbackTexture = GSystemTextures.WhiteDummy; break;
		default:
			ensure(!"Unhandled enum in EFallbackColor");
	}

	// ePId_Input0, ePId_Input1, ...
	for(uint32 Id = 0; Id < (uint32)ePId_Input_MAX; ++Id)
	{
		FRenderingCompositeOutputRef* OutputRef = Context.Pass->GetInput((EPassInputId)Id);

		if(!OutputRef)
		{
			// Pass doesn't have more inputs
			break;
		}

		const auto FeatureLevel = Context.GetFeatureLevel();

		FRenderingCompositeOutput* Input = OutputRef->GetOutput();

		TRefCountPtr<IPooledRenderTarget> InputPooledElement;

		if(Input)
		{
			InputPooledElement = Input->RequestInput();
		}

		FSamplerStateRHIParamRef LocalFilter = FilterOverrideArray ? FilterOverrideArray[Id] : Filter;

		if(InputPooledElement)
		{
			check(!InputPooledElement->IsFree());

			const FTextureRHIRef& SrcTexture = InputPooledElement->GetRenderTargetItem().ShaderResourceTexture;

			SetTextureParameter(RHICmdList, ShaderRHI, PostprocessInputParameter[Id], PostprocessInputParameterSampler[Id], LocalFilter, SrcTexture);

			if(PostprocessInputSizeParameter[Id].IsBound() || PostProcessInputMinMaxParameter[Id].IsBound())
			{
				float Width = InputPooledElement->GetDesc().Extent.X;
				float Height = InputPooledElement->GetDesc().Extent.Y;
				
				FVector2D OnePPInputPixelUVSize = FVector2D(1.0f / Width, 1.0f / Height);

				FVector4 TextureSize(Width, Height, OnePPInputPixelUVSize.X, OnePPInputPixelUVSize.Y);
				SetShaderValue(RHICmdList, ShaderRHI, PostprocessInputSizeParameter[Id], TextureSize);

				//We could use the main scene min max here if it weren't that we need to pull the max in by a pixel on a per input basis.
				FVector4 PPInputMinMax = BaseSceneTexMinMax;
				PPInputMinMax.Z -= OnePPInputPixelUVSize.X;
				PPInputMinMax.W -= OnePPInputPixelUVSize.Y;
				SetShaderValue(RHICmdList, ShaderRHI, PostProcessInputMinMaxParameter[Id], PPInputMinMax);
			}
		}
		else
		{
			// if the input is not there but the shader request it we give it at least some data to avoid d3ddebug errors and shader permutations
			// to make features optional we use default black for additive passes without shader permutations
			SetTextureParameter(RHICmdList, ShaderRHI, PostprocessInputParameter[Id], PostprocessInputParameterSampler[Id], LocalFilter, FallbackTexture->GetRenderTargetItem().TargetableTexture);

			FVector4 Dummy(1, 1, 1, 1);
			SetShaderValue(RHICmdList, ShaderRHI, PostprocessInputSizeParameter[Id], Dummy);
			SetShaderValue(RHICmdList, ShaderRHI, PostProcessInputMinMaxParameter[Id], Dummy);
		}
	}

	// todo warning if Input[] or InputSize[] is bound but not available, maybe set a specific input texture (blinking?)
}

#define IMPLEMENT_POST_PROCESS_PARAM_SET( TShaderRHIParamRef, TRHICmdList ) \
	template void FPostProcessPassParameters::Set< TShaderRHIParamRef >( \
		TRHICmdList& RHICmdList,						\
		const TShaderRHIParamRef& ShaderRHI,			\
		const FRenderingCompositePassContext& Context,	\
		FSamplerStateRHIParamRef Filter,				\
		EFallbackColor FallbackColor,					\
		FSamplerStateRHIParamRef* FilterOverrideArray	\
	);

IMPLEMENT_POST_PROCESS_PARAM_SET( FVertexShaderRHIParamRef, FRHICommandListImmediate );
IMPLEMENT_POST_PROCESS_PARAM_SET( FHullShaderRHIParamRef, FRHICommandListImmediate );
IMPLEMENT_POST_PROCESS_PARAM_SET( FDomainShaderRHIParamRef, FRHICommandListImmediate );
IMPLEMENT_POST_PROCESS_PARAM_SET( FGeometryShaderRHIParamRef, FRHICommandListImmediate );
IMPLEMENT_POST_PROCESS_PARAM_SET( FPixelShaderRHIParamRef, FRHICommandListImmediate );
IMPLEMENT_POST_PROCESS_PARAM_SET( FComputeShaderRHIParamRef, FRHICommandListImmediate );
IMPLEMENT_POST_PROCESS_PARAM_SET( FComputeShaderRHIParamRef, FRHIAsyncComputeCommandListImmediate );

FArchive& operator<<(FArchive& Ar, FPostProcessPassParameters& P)
{
	Ar << P.BilinearTextureSampler0 << P.BilinearTextureSampler1 << P.ViewportSize << P.ScreenPosToPixel << P.ViewportRect;

	for(uint32 i = 0; i < ePId_Input_MAX; ++i)
	{
		Ar << P.PostprocessInputParameter[i];
		Ar << P.PostprocessInputParameterSampler[i];
		Ar << P.PostprocessInputSizeParameter[i];
		Ar << P.PostProcessInputMinMaxParameter[i];
	}

	return Ar;
}

// -----------------------------------------------------------------

const FSceneRenderTargetItem& FRenderingCompositeOutput::RequestSurface(const FRenderingCompositePassContext& Context)
{
	if(PooledRenderTarget)
	{
		Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, PooledRenderTarget->GetRenderTargetItem().TargetableTexture);
		return PooledRenderTarget->GetRenderTargetItem();
	}

	if(!RenderTargetDesc.IsValid())
	{
		// useful to use the CompositingGraph dependency resolve but pass the data between nodes differently
		static FSceneRenderTargetItem Null;

		return Null;
	}

	if(!PooledRenderTarget)
	{
		GRenderTargetPool.FindFreeElement(Context.RHICmdList, RenderTargetDesc, PooledRenderTarget, RenderTargetDesc.DebugName);
	}

	check(!PooledRenderTarget->IsFree());

	FSceneRenderTargetItem& RenderTargetItem = PooledRenderTarget->GetRenderTargetItem();

	return RenderTargetItem;
}


const FPooledRenderTargetDesc* FRenderingCompositePass::GetInputDesc(EPassInputId InPassInputId) const
{
	// to overcome const issues, this way it's kept local
	FRenderingCompositePass* This = (FRenderingCompositePass*)this;

	const FRenderingCompositeOutputRef* OutputRef = This->GetInput(InPassInputId);

	if(!OutputRef)
	{
		return 0;
	}

	FRenderingCompositeOutput* Input = OutputRef->GetOutput();

	if(!Input)
	{
		return 0;
	}

	return &Input->RenderTargetDesc;
}

uint32 FRenderingCompositePass::ComputeInputCount()
{
	for(uint32 i = 0; ; ++i)
	{
		if(!GetInput((EPassInputId)i))
		{
			return i;
		}
	}
}

uint32 FRenderingCompositePass::ComputeOutputCount()
{
	for(uint32 i = 0; ; ++i)
	{
		if(!GetOutput((EPassOutputId)i))
		{
			return i;
		}
	}
}

FString FRenderingCompositePass::ConstructDebugName()
{
	FString Name;

	uint32 OutputId = 0;
	while(FRenderingCompositeOutput* Output = GetOutput((EPassOutputId)OutputId))
	{
		Name += Output->RenderTargetDesc.DebugName;

		++OutputId;
	}

	if(Name.IsEmpty())
	{
		Name = TEXT("UnknownName");
	}

	return Name;
}
