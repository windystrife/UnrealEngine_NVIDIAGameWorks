// UsdWrapperTest.cpp : Defines the entry point for the console application.
//

#include "UnrealUSDWrapper.h"
#include <stdarg.h>
#include <windows.h>


#pragma warning(disable:4267)

void Log(const char* Format, ...)
{
	static char Buffer[2048];

	va_list args;

	va_start(args, Format);

	vsprintf_s(Buffer, Format, args);

	OutputDebugString(Buffer);

	va_end(args);
}


void FindUsdMeshes(IUsdPrim* StartPrim, std::vector<IUsdPrim*>& UsdMeshes)
{
	std::string Name = StartPrim->GetPrimName();

	FUsdMatrixData Mtx = StartPrim->GetLocalToWorldTransform();
	FUsdMatrixData Mtx2 = StartPrim->GetLocalToParentTransform();

	int NumLODs = StartPrim->GetNumLODs();
	if (NumLODs == 0)
	{
		const FUsdGeomData* GeomData = StartPrim->GetGeometryData();
		if (GeomData)
		{
			UsdMeshes.push_back(StartPrim);
		}
	}
	else
	{
		for (int LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
		{
			StartPrim->GetLODChild(LODIndex);
		}
	}

	int NumChildren = StartPrim->GetNumChildren();

	for (int ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		FindUsdMeshes(StartPrim->GetChild(ChildIdx), UsdMeshes);
	}
}

void LogPrimRecursive(IUsdPrim* Prim, const std::string& Concat)
{
	std::string name = Prim->GetPrimName();
	bool bHasGeometryData = Prim->HasGeometryData();

	Log(std::string(Concat + "%s %d\n").c_str(), name.c_str(), bHasGeometryData);

	int NumChildren = Prim->GetNumChildren();
	for (int ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		LogPrimRecursive(Prim->GetChild(ChildIdx), Concat + "\t");
	}
}

int main()
{
	UnrealUSDWrapper::Initialize("");

	IUsdStage* Stage = UnrealUSDWrapper::ImportUSDFile("", "");
	if (Stage)
	{
		IUsdPrim* RootPrim = Stage->GetRootPrim();

		IUsdPrim* TestPrim = RootPrim->GetChild(0);

		const char* Errors = UnrealUSDWrapper::GetErrors();
		if (Errors != nullptr)
		{
			Log("%s", Errors);
		}

	/*	LogPrimRecursive(RootPrim, "");

		volatile EUsdUpAxis UpAxis = Stage->GetUpAxis();
		std::vector<IUsdPrim*> UsdMeshes;
		FindUsdMeshes(RootPrim, UsdMeshes);*/
		volatile int i = 0;
		++i;
	}

	UnrealUSDWrapper::CleanUp();
    return 0;
}

