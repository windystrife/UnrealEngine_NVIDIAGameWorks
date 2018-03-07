// UnrealUSDWrapper.cpp : Defines the exported functions for the DLL application.
//

#include "UnrealUSDWrapper.h"
#include "pxr/usd/usd/usdFileFormat.h"
#include "pxr/usd/usd/common.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/base/tf/errorMark.h"
#include "pxr/base/plug/registry.h"
#include "pxr/base/plug/plugin.h"
#include "pxr/usd/sdf/schema.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/relationship.h"
#include "pxr/usd/usd/references.h"
#include "pxr/usd/usdGeom/modelAPI.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/xformCommonAPI.h"
#include "pxr/usd/usdGeom/faceSetAPI.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/setenv.h"
#include "pxr/usd/ar/defaultResolver.h"
#include "pxr/usd/ar/defineResolver.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/usd/usd/variantSets.h"
#include "pxr/usd/usd/debugCodes.h"
#include "pxr/usd/kind/registry.h"

using std::vector;
using std::string;

using namespace pxr;

#ifdef _WINDOWS
#pragma warning(disable:4267)

#define USDWRAPPER_USE_XFORMACHE	1
#else
#define USDWRAPPER_USE_XFORMACHE	0
#endif


#if USDWRAPPER_USE_XFORMACHE
static UsdGeomXformCache XFormCache;
#endif // USDWRAPPER_USE_XFORMACHE

namespace UnrealIdentifiers
{
	/**
	* Identifies the LOD variant set on a primitive which means this primitive has child prims that LOD meshes
	* named LOD0, LOD1, LOD2, etc
	*/
	static const TfToken LOD("LOD");

	static const TfToken AssetPath("unrealAssetPath");

	static const TfToken ActorClass("unrealActorClass");

	static const TfToken PropertyPath("unrealPropertyPath");
}


void Log(const char* Format, ...)
{
	static char Buffer[2048];

	va_list args;

	va_start(args, Format);

#if _WINDOWS
	vsprintf_s(Buffer, Format, args);

	OutputDebugString(Buffer);
#else
	vsprintf(Buffer, Format, args);
	printf("%s", Buffer);
#endif

	va_end(args);
}

class USDHelpers
{

public:
	static void LogPrimTree(const UsdPrim& Root)
	{
		LogPrimTreeHelper("", Root);
	}

private:
	static void LogPrimTreeHelper(const string& Concat, const UsdPrim& Prim)
	{
		string TypeName = Prim.GetTypeName().GetString();
		bool bIsModel = Prim.IsModel();
		bool bIsAbstract = Prim.IsAbstract();
		bool bIsGroup = Prim.IsGroup();
		bool bIsInstance = Prim.IsInstance();
		bool bIsActive = Prim.IsActive();
		bool bInMaster = Prim.IsInMaster();
		bool bIsMaster = Prim.IsMaster();
		Log(string(Concat + "Prim: [%s] %s Model:%d Abstract:%d Group:%d Instance:%d Active:%d InMaster:%d IsMaster:%d\n").c_str(),
			TypeName.c_str(), Prim.GetName().GetText(), bIsModel, bIsAbstract, bIsGroup, bIsInstance, bIsActive, bInMaster, bIsMaster);
		{
			UsdMetadataValueMap Metadata = Prim.GetAllMetadata();
			if(Metadata.size())
			{
				Log(string(Concat+"\tMetaData:\n").c_str());
				for(auto KeyValue : Metadata)
				{
					Log(string(Concat+"\t\t[%s] %s\n").c_str(), KeyValue.second.GetTypeName().c_str(), KeyValue.first.GetText());
				}
			}

		

			vector<UsdAttribute> Attributes = Prim.GetAttributes();
			if(Attributes.size())
			{
				Log(string(Concat+"\tAttributes:\n").c_str());
				for(const UsdAttribute& Attribute : Attributes)
				{
					if (Attribute.IsAuthored())
					{
						Log(string(Concat + "\t\t[%s] %s %s\n").c_str(), Attribute.GetTypeName().GetAsToken().GetText(), Attribute.GetBaseName().GetText(), Attribute.GetDisplayName().c_str());
					}
				}
			}

			if (Prim.HasVariantSets())
			{
				Log(string(Concat + "\tVariant Sets:\n").c_str());
				UsdVariantSets VariantSets = Prim.GetVariantSets();
				vector<string> SetNames = VariantSets.GetNames();
				for (const string& SetName : SetNames)
				{
					Log(string(Concat + "\t\t%s:\n").c_str(), SetName.c_str());

					UsdVariantSet Set = Prim.GetVariantSet(SetName);

					vector<string> VariantNames = Set.GetVariantNames();
					for (const string& VariantName : VariantNames)
					{
						char ActiveChar = ' ';
						if (Set.GetVariantSelection() == VariantName)
						{
							ActiveChar = '*';
						}
						Log(string(Concat + "\t\t\t%s%c\n").c_str(), VariantName.c_str(), ActiveChar);
					}
				}
			}
		}


		for(const UsdPrim& Child : Prim.GetChildren())
		{
			LogPrimTreeHelper(Concat+"\t", Child);
		}

		//Log("\n");
	}
};

// Dll entrypoint
void InitWrapper()
{
}

struct FPrimAndData
{
	UsdPrim Prim;
	class FUsdPrim* PrimData;

	FPrimAndData(const UsdPrim& InPrim)
		: Prim(InPrim)
		, PrimData(nullptr)
	{}
};


class FAttribInternalData
{
public:
	FAttribInternalData(UsdAttribute& InAttribute)
		: Attribute(InAttribute)
	{
		VtValue CustomData = Attribute.GetCustomDataByKey(UnrealIdentifiers::PropertyPath);

		AttributeName = Attribute.GetBaseName().GetString();
		TypeName = Attribute.GetTypeName().GetAsToken().GetString();

		if (CustomData.IsHolding<std::string>())
		{
			UnrealPropertyPath = CustomData.Get<std::string>();
		}
	}

	std::string UnrealPropertyPath;
	std::string AttributeName;
	std::string TypeName;
	UsdAttribute Attribute;
};


FUsdAttribute::FUsdAttribute(std::shared_ptr<FAttribInternalData> InInternalData)
	: InternalData(InInternalData)
{
}

FUsdAttribute::~FUsdAttribute()
{
}

const char* FUsdAttribute::GetAttributeName() const
{
	return InternalData->AttributeName.c_str();
}

const char* FUsdAttribute::GetTypeName() const
{
	return InternalData->TypeName.c_str();
}

const char* FUsdAttribute::GetUnrealPropertyPath() const
{
	return InternalData->UnrealPropertyPath.c_str();
}

template<typename T>
bool GetValue(T& OutVal, UsdAttribute Attrib, int ArrayIndex, double Time)
{
	bool bResult = false;

	if (ArrayIndex != -1)
	{
		// Note: VtArray is copy on write so this is cheap
		VtArray<T> Array;
		if (Attrib.Get(&Array, Time))
		{
			OutVal = Array[ArrayIndex];
			bResult = true;
		}
	}
	else
	{
		bResult = Attrib.Get(&OutVal, Time);
	}

	return bResult;
}

template<typename T>
bool IsHolding(const VtValue& Value)
{
	return Value.IsHolding<T>() || Value.IsHolding<VtArray<T>>();
}

bool FUsdAttribute::AsInt(int64_t& OutVal, int ArrayIndex, double Time) const
{
	// We test multiple types of ints here. int64 is always returned as it can hold all other types
	// Unreal expects this
	VtValue Value;
	bool bResult = InternalData->Attribute.Get(&Value, Time);
	if (IsHolding<int8_t>(Value))
	{
		uint8_t Val;
		bResult = GetValue(Val, InternalData->Attribute, ArrayIndex, Time);
		OutVal = Val;
	}
	else if (IsHolding<int32_t>(Value))
	{
		int32_t Val;
		bResult = GetValue(Val, InternalData->Attribute, ArrayIndex, Time);
		OutVal = Val;
	}
	else if (IsHolding<int64_t>(Value))
	{
		int64_t Val;
		bResult = GetValue(Val, InternalData->Attribute, ArrayIndex, Time);
		OutVal = Val;
	}

	return bResult;
}

bool FUsdAttribute::AsUnsignedInt(uint64_t& OutVal, int ArrayIndex, double Time) const
{
	// We test multiple types of ints here. uint64 is always returned as it can hold all other types
	// Unreal expects this
	VtValue Value;
	bool bResult = InternalData->Attribute.Get(&Value, Time);
	if (IsHolding<uint8_t>(Value))
	{
		uint8_t Val;
		bResult = GetValue(Val, InternalData->Attribute, ArrayIndex, Time);
		OutVal = Val;
	}
	else if (IsHolding<uint32_t>(Value))
	{
		uint32_t Val;
		bResult = GetValue(Val, InternalData->Attribute, ArrayIndex, Time);
		OutVal = Val;
	}
	else if (IsHolding<uint64_t>(Value))
	{
		uint64_t Val;
		bResult = GetValue(Val, InternalData->Attribute, ArrayIndex, Time);
		OutVal = Val;
	}

	return bResult;
}

bool FUsdAttribute::AsDouble(double& OutVal, int ArrayIndex, double Time) const
{
	bool bResult = false;


	bResult = GetValue<double>(OutVal, InternalData->Attribute, ArrayIndex, Time);

	if (!bResult)
	{
		float Val = 0.0f;
		bResult = GetValue<float>(Val, InternalData->Attribute, ArrayIndex, Time);
		OutVal = Val;
	}

	return bResult;
}

bool FUsdAttribute::AsString(const char*& OutVal, int ArrayIndex, double Time) const
{
	// this method is very hacky to return temp strings
	// designed to have the string copied immediately
	bool bResult = false;

	VtValue Value;
	InternalData->Attribute.Get(&Value);
	// mem leak
	static std::string Temp;
	if (IsHolding<std::string>(Value))
	{
		bResult = GetValue(Temp, InternalData->Attribute, ArrayIndex, Time);

		OutVal = Temp.c_str();
	}
	else if (IsHolding<TfToken>(Value))
	{
		TfToken Token;
		bResult = GetValue(Token, InternalData->Attribute, ArrayIndex, Time);

		Temp = Token.GetString();

		OutVal = Temp.c_str();
	}

	return bResult;
}

bool FUsdAttribute::AsBool(bool& OutVal, int ArrayIndex, double Time) const
{
	return GetValue(OutVal, InternalData->Attribute, ArrayIndex, Time);
}

bool FUsdAttribute::AsVector2(FUsdVector2Data& OutVal, int ArrayIndex, double Time) const
{
	GfVec2f Value;
	const bool bResult = GetValue(Value, InternalData->Attribute, ArrayIndex, Time);

	OutVal.X = Value[0];
	OutVal.Y = Value[1];

	return bResult;
}

bool FUsdAttribute::AsVector3(FUsdVectorData& OutVal, int ArrayIndex, double Time) const
{
	GfVec3f Value;
	const bool bResult = GetValue(Value, InternalData->Attribute, ArrayIndex, Time);

	OutVal.X = Value[0];
	OutVal.Y = Value[1];
	OutVal.Z = Value[2];

	return bResult;
}

bool FUsdAttribute::AsVector4(FUsdVector4Data& OutVal, int ArrayIndex, double Time) const
{
	GfVec4f Value;
	const bool bResult = GetValue(Value, InternalData->Attribute, ArrayIndex, Time);

	OutVal.X = Value[0];
	OutVal.Y = Value[1];
	OutVal.Z = Value[2];
	OutVal.W = Value[3];

	return bResult;
}

bool FUsdAttribute::AsColor(FUsdVector4Data& OutVal, int ArrayIndex, double Time) const
{
	GfVec4f Value;
	bool bResult = GetValue(Value, InternalData->Attribute, ArrayIndex, Time);

	if (bResult)
	{
		OutVal.X = Value[0];
		OutVal.Y = Value[1];
		OutVal.Z = Value[2];
		OutVal.W = Value[3];
	}
	else
	{
		// Try color 3 with a = 1;
		GfVec3f Value;
		bResult = GetValue<GfVec3f>(Value, InternalData->Attribute, ArrayIndex, Time);
		OutVal.X = Value[0];
		OutVal.Y = Value[1];
		OutVal.Z = Value[2];
		OutVal.W = 1;
	}

	return bResult;
}

bool FUsdAttribute::IsUnsigned() const
{
	VtValue Value;
	InternalData->Attribute.Get(&Value);

	return IsHolding<uint8_t>(Value)
		|| IsHolding<uint32_t>(Value)
		|| IsHolding<uint64_t>(Value);
}

int FUsdAttribute::GetArraySize() const
{
	VtValue Value;
	InternalData->Attribute.Get(&Value);

	return Value.IsArrayValued() ? (int)Value.GetArraySize() : -1;
}


class FUsdPrim : public IUsdPrim
{
public:
	FUsdPrim(const UsdPrim& InPrim)
		: Prim(InPrim)
		, GeomData(nullptr)
	{
		PrimName = Prim.GetName().GetString();
		PrimPath = Prim.GetPath().GetString();

		UsdModelAPI Model(Prim);
		if (Model)
		{
			TfToken KindType;
			Model.GetKind(&KindType);

			Kind = KindType.GetString();
		}
		else
		{
			// Prim is not a model, read kind directly from metadata
			static TfToken KindMetaDataToken("kind");
			TfToken KindType;
			if (Prim.GetMetadata(KindMetaDataToken, &KindType))
			{
				Kind = KindType.GetString();
			}
		}

		UsdAttribute UnrealAssetPathAttr = Prim.GetAttribute(UnrealIdentifiers::AssetPath);
		if (UnrealAssetPathAttr.HasValue())
		{
			UnrealAssetPathAttr.Get(&UnrealAssetPath);
		}

		UsdAttribute UnrealActorClassAttr = Prim.GetAttribute(UnrealIdentifiers::ActorClass);
		if (UnrealActorClassAttr.HasValue())
		{
			UnrealActorClassAttr.Get(&UnrealActorClass);
		}
	
		VtValue CustomData = Prim.GetCustomDataByKey(UnrealIdentifiers::PropertyPath);
		if (CustomData.IsHolding<std::string>())
		{
			UnrealPropertyPath = CustomData.Get<std::string>();

		}

		for (const UsdPrim& Child : Prim.GetChildren())
		{
			Children.push_back(FPrimAndData(Child));
		}
	}

	~FUsdPrim()
	{
		if (GeomData)
		{
			delete GeomData;
		}

		for (FPrimAndData& Child : Children)
		{
			if (Child.PrimData)
			{
				delete Child.PrimData;
			}
		}		

		for (FPrimAndData& Child : VariantData)
		{
			if (Child.PrimData)
			{
				delete Child.PrimData;
			}
		}

		Children.clear();

		VariantData.clear();
		
	}

	virtual const char* GetPrimName() const override
	{
		return PrimName.c_str();
	}

	virtual const char* GetPrimPath() const override
	{
		return PrimPath.c_str();
	}

	virtual const char* GetUnrealPropertyPath() const override
	{
		return UnrealPropertyPath.c_str();
	}

	virtual const char* GetKind() const override
	{
		return Kind.c_str();
	}

	virtual bool IsKindChildOf(const std::string& InKind) const override
	{
		TfToken TestKind(InKind);
	
		KindRegistry& Registry = KindRegistry::GetInstance();

		TfToken PrimKind(Kind);

		return Registry.IsA(TestKind, PrimKind);
	}


	virtual bool IsGroup() const override
	{
		return Prim.IsGroup();
	}

	virtual bool IsModel() const override
	{
		return Prim.IsModel();
	}

	virtual bool IsUnrealProperty() const override
	{
		return Prim.HasCustomDataKey(UnrealIdentifiers::PropertyPath);
	}

	virtual bool HasTransform() const override
	{
		return UsdGeomXformable(Prim);
	}

	static GfMatrix4d GetLocalToWorldTransform(const UsdPrim& Prim, double Time, const SdfPath& AbsoluteRootPath)
	{
		SdfPath PrimPath = Prim.GetPath();
		if (!Prim || PrimPath == AbsoluteRootPath)
		{
			return GfMatrix4d(1);
		}

		GfMatrix4d AccumulatedTransform(1.);
		bool bResetsXFormStack = false;
		UsdGeomXformable XFormable(Prim);
		// silently ignoring errors
		XFormable.GetLocalTransformation(&AccumulatedTransform, &bResetsXFormStack, Time);

		if (!bResetsXFormStack)
		{
			AccumulatedTransform = AccumulatedTransform * GetLocalToWorldTransform(Prim.GetParent(), Time, AbsoluteRootPath);
		}

		return AccumulatedTransform;
	}
	
	virtual FUsdMatrixData GetLocalToWorldTransform(double Time) const override
	{
#if USDWRAPPER_USE_XFORMACHE
		XFormCache.SetTime(Time);
		GfMatrix4d LocalToWorld = XFormCache.GetLocalToWorldTransform(Prim);
#else
		UsdGeomXformable XFormable(Prim);
		SdfPath WorldPath = SdfPath::AbsoluteRootPath();
		GfMatrix4d LocalToWorld;
		if (!Prim.GetPath().HasPrefix(WorldPath))
		{
			LocalToWorld = GfMatrix4d(1);
		}
		else
		{
			LocalToWorld = GetLocalToWorldTransform(Prim, Time, WorldPath);
		}
#endif

		FUsdMatrixData Ret;
		memcpy(Ret.Data, LocalToWorld.GetArray(), sizeof(double) * 16);

		return Ret;

	}

	virtual FUsdMatrixData GetLocalToAncestorTransform(IUsdPrim* Ancestor, double Time) const override
	{
		if (Ancestor)
		{
			FUsdPrim* InternalAncestorPrim = (FUsdPrim*)Ancestor;

			GfMatrix4d LocalToAncestor;
			LocalToAncestor = GetLocalToWorldTransform(Prim, Time, InternalAncestorPrim->GetUSDPrim().GetPath());

			FUsdMatrixData Ret;
			memcpy(Ret.Data, LocalToAncestor.GetArray(), sizeof(double) * 16);

			return Ret;
		}
		else
		{
			return GetLocalToWorldTransform(Time);
		}
	}

	virtual FUsdMatrixData GetLocalToParentTransform(double Time) const override
	{
		bool bResetsXFormStack = false;
#if USDWRAPPER_USE_XFORMACHE
		XFormCache.SetTime(Time);
		GfMatrix4d LocalToParent = XFormCache.GetLocalTransformation(Prim, &bResetsXFormStack);
#else
		UsdGeomXformable XFormable(Prim);
		GfMatrix4d LocalToParent;
		// silently ignoring errors
		XFormable.GetLocalTransformation(&LocalToParent, &bResetsXFormStack, Time);
#endif
		FUsdMatrixData Ret;
		memcpy(Ret.Data, LocalToParent.GetArray(), sizeof(double) * 16);
		
		return Ret;
	}

	virtual int GetNumChildren() const override
	{
		return (int)Children.size();
	}

	virtual IUsdPrim* GetChild(int ChildIndex) override
	{
		FPrimAndData& PrimAndData = Children[ChildIndex];
		if (!PrimAndData.PrimData)
		{
			PrimAndData.PrimData = new FUsdPrim(PrimAndData.Prim);
		}

		return PrimAndData.PrimData;
	}

	virtual const char* GetUnrealAssetPath() const override
	{
		return UnrealAssetPath.length() > 0 ? UnrealAssetPath.c_str() : nullptr;
	}

	virtual const char* GetUnrealActorClass() const override
	{
		return UnrealActorClass.length() > 0 ? UnrealActorClass.c_str() : nullptr;
	}

	virtual bool HasGeometryData() const override
	{
		UsdGeomMesh Mesh(Prim);

		return Mesh || GetNumLODs() > 0;
	}

	virtual const FUsdGeomData* GetGeometryData() override
	{
		return GetGeometryData(UsdTimeCode::Default().GetValue());
	}

	virtual const FUsdGeomData* GetGeometryData(double Time) override
	{
		bool bValid = false;
		UsdGeomMesh Mesh(Prim);
		if (Mesh)
		{
			if (GeomData)
			{
				delete GeomData;
			}

			GeomData = new FUsdGeomData;

			// Faces and points
			{
				UsdAttribute FaceCounts = Mesh.GetFaceVertexCountsAttr();
				if (FaceCounts)
				{
					GeomData->FaceVertexCounts.clear();

					VtArray<int> FaceCountArray;
					FaceCounts.Get(&FaceCountArray, Time);
					GeomData->FaceVertexCounts.assign(FaceCountArray.begin(), FaceCountArray.end());
				}

				UsdAttribute FaceIndices = Mesh.GetFaceVertexIndicesAttr();
				if (FaceIndices)
				{
					GeomData->FaceIndices.clear();

					VtArray<int> FaceIndicesArray;
					FaceIndices.Get(&FaceIndicesArray, Time);
					GeomData->FaceIndices.assign(FaceIndicesArray.begin(), FaceIndicesArray.end());
				}

				UsdAttribute Points = Mesh.GetPointsAttr();
				if (Points)
				{
					GeomData->Points.clear();

					VtArray<GfVec3f> PointsArray;
					Points.Get(&PointsArray, Time);

					// Bug??  Usd returns nothing for UsdTimeCode::Default if there are animated points
					if (PointsArray.size() == 0 && Time == UsdTimeCode::Default())
					{
						// Try to get at time = 0
						Points.Get(&PointsArray, UsdTimeCode(0));
					}
					
					GeomData->Points.resize(PointsArray.size());
					memcpy(&GeomData->Points[0], &PointsArray[0], PointsArray.size() * sizeof(GfVec3f));
				}

				UsdAttribute Normals = Mesh.GetNormalsAttr();
				if (Normals)
				{
					GeomData->Normals.clear();

					VtArray<GfVec3f> NormalsArray;
					Normals.Get(&NormalsArray, Time);

					GeomData->Normals.resize(NormalsArray.size());
					memcpy(&GeomData->Normals[0], &NormalsArray[0], NormalsArray.size() * sizeof(GfVec3f));
				} 

				UsdGeomPrimvar DisplayColorPrimVar = Mesh.GetDisplayColorPrimvar();
				if (DisplayColorPrimVar)
				{
					GeomData->VertexColors.clear();

					TfToken Interpolation = DisplayColorPrimVar.GetInterpolation();

					if (Interpolation == UsdGeomTokens->faceVarying || Interpolation == UsdGeomTokens->uniform)
					{
						GeomData->VertexColorInterpMethod = Interpolation == UsdGeomTokens->faceVarying ? EUsdInterpolationMethod::FaceVarying : EUsdInterpolationMethod::Uniform;
						VtArray<GfVec3f> DisplayColorArray;
						DisplayColorPrimVar.ComputeFlattened(&DisplayColorArray, Time);
		
						GeomData->VertexColors.resize(DisplayColorArray.size());
						memcpy(&GeomData->VertexColors[0], &DisplayColorArray[0], DisplayColorArray.size() * sizeof(GfVec3f));
					}
					else if (Interpolation == UsdGeomTokens->vertex)
					{
						GeomData->VertexColorInterpMethod = EUsdInterpolationMethod::Vertex;
						VtIntArray VertexColorIndices;
						VtArray<GfVec3f> DisplayColorArray;
						DisplayColorPrimVar.GetIndices(&VertexColorIndices, Time);
						DisplayColorPrimVar.Get(&DisplayColorArray, Time);

						if (VertexColorIndices.size() == GeomData->Points.size())
						{
							GeomData->VertexColors.resize(VertexColorIndices.size());
							for (int PointIdx = 0; PointIdx < GeomData->Points.size(); ++PointIdx)
							{
								GfVec3f Color = DisplayColorArray[VertexColorIndices[PointIdx]];
								GeomData->VertexColors[PointIdx] = FUsdVectorData(Color[0], Color[1], Color[2]);
							}
						}
						else
						{
							// Assume mapping is identical
							GeomData->VertexColors.resize(DisplayColorArray.size());
							memcpy(&GeomData->VertexColors[0], &DisplayColorArray[0], DisplayColorArray.size() * sizeof(GfVec3f));
						}
					}
					else if (Interpolation == UsdGeomTokens->constant)
					{
						VtArray<GfVec3f> DisplayColorArray;
						DisplayColorPrimVar.Get(&DisplayColorArray, Time);

						if(DisplayColorArray.size() > 0)
						{
							GfVec3f Color = DisplayColorArray[0];
							GeomData->VertexColors.push_back(FUsdVectorData(Color[0], Color[1], Color[2]));
						}
					}
				}

				GeomData->Orientation = EUsdGeomOrientation::RightHanded;
				UsdAttribute Orientation = Mesh.GetOrientationAttr();
				if(Orientation)
				{ 
					static TfToken RightHanded("rightHanded");
					static TfToken LeftHanded("leftHanded");

					TfToken OrientationValue;
					Orientation.Get(&OrientationValue, Time);

					GeomData->Orientation = OrientationValue == RightHanded ? EUsdGeomOrientation::RightHanded : EUsdGeomOrientation::LeftHanded;
				}

			}

			// UVs
			{
				static TfToken UVSetName("primvars:st");

				UsdGeomPrimvar STPrimvar = Mesh.GetPrimvar(UVSetName);

				int UVIndex = GeomData->NumUVs;
				if(STPrimvar)
				{
					// We are creating one UV set
					++GeomData->NumUVs;

					GeomData->UVs[UVIndex].Coords.clear();

					if (STPrimvar.GetInterpolation() == UsdGeomTokens->faceVarying)
					{
						GeomData->UVs[UVIndex].UVInterpMethod = EUsdInterpolationMethod::FaceVarying;
						VtVec2fArray UVs;
						STPrimvar.ComputeFlattened(&UVs, Time);
						if (UVs.size() == GeomData->FaceIndices.size())
						{
							GeomData->UVs[UVIndex].Coords.resize(UVs.size());
							memcpy(&GeomData->UVs[UVIndex].Coords[0], &UVs[0], UVs.size() * sizeof(GfVec2f));
						}
					}
					else if (STPrimvar.GetInterpolation() == UsdGeomTokens->vertex)
					{
						GeomData->UVs[UVIndex].UVInterpMethod = EUsdInterpolationMethod::Vertex;
						VtIntArray UVIndices;
						VtVec2fArray UVs;
						STPrimvar.GetIndices(&UVIndices, Time);
						STPrimvar.Get(&UVs, Time);

						if (UVIndices.size() == GeomData->Points.size())
						{
							GeomData->UVs[UVIndex].Coords.resize(UVIndices.size());
							for (int PointIdx = 0; PointIdx < GeomData->Points.size(); ++PointIdx)
							{
								GfVec2f UV = UVs[UVIndices[PointIdx]];
								GeomData->UVs[UVIndex].Coords[PointIdx] = FUsdVector2Data(UV[0], UV[1]);

							}
						}
					}
				}
			}

			// Material mappings
			// @todo time not supported yet
			if(GeomData->FaceMaterialIndices.size() == 0)
			{
				vector<UsdGeomFaceSetAPI> FaceSets = UsdGeomFaceSetAPI::GetFaceSets(Prim);

				GeomData->FaceMaterialIndices.resize(GeomData->FaceVertexCounts.size());
				memset(&GeomData->FaceMaterialIndices[0], 0, sizeof(int)*GeomData->FaceMaterialIndices.size());
			
				// Figure out a zero based mateiral index for each face.  The mapping is FaceMaterialIndices[FaceIndex] = MaterialIndex;
				// This is done by walking the face sets and for each face set getting the number number of unique groups of faces in the set
				// Each one of these groups represents a material index for that face set.  If there are multiple face sets the material index is offset by the face set index
				// Once the groups of faces are determined, walk the indices for the total number of faces in each group.  Each element in the face indices array represents a single global face index
				// Assign the current material index to it

				// @todo USD/Unreal.  This is probably wrong for multiple face sets.  They don't make a ton of sense for unreal as there can only be one "set" of materials at once and there is no construct in the engine for material sets
			
				//GeomData->MaterialNames.resize(FaceSets)
				for (int FaceSetIdx = 0; FaceSetIdx < FaceSets.size(); ++FaceSetIdx)
				{
					const UsdGeomFaceSetAPI& FaceSet = FaceSets[FaceSetIdx];
					
					SdfPathVector BindingTargets;
					FaceSet.GetBindingTargets(&BindingTargets);

					
					UsdStageWeakPtr Stage = Prim.GetStage();
					for(const SdfPath& Path : BindingTargets)
					{
						// load each material at the material path; 
						UsdPrim MaterialPrim = Stage->Load(Path);

						// Default to using the prim path name as the path for this material in Unreal
						std::string MaterialName = MaterialPrim.GetName().GetString();

						// See if the material has an "unrealAssetPath" attribute.  This should be the full name of the material
						static const TfToken AssetPathToken = TfToken(UnrealIdentifiers::AssetPath);
						UsdAttribute UnrealAssetPathAttr = MaterialPrim.GetAttribute(AssetPathToken);
						if (UnrealAssetPathAttr.HasValue())
						{
							UnrealAssetPathAttr.Get(&MaterialName);
						}

						GeomData->MaterialNames.push_back(MaterialName);
					}
					// Faces must be mutually exclusive
					if (FaceSet.GetIsPartition())
					{
						// Get the list of faces in the face set.  The size of this list determines the number of materials in this set
						VtIntArray FaceCounts;
						FaceSet.GetFaceCounts(&FaceCounts, Time);

						// Get the list of global face indices mapped in this set
						VtIntArray FaceIndices;
						FaceSet.GetFaceIndices(&FaceIndices, Time);

						// How far we are into the face indices list
						int Offset = 0;

						// Walk each face group in the set
						for (int FaceCountIdx = 0; FaceCountIdx < FaceCounts.size(); ++FaceCountIdx)
						{
							int MaterialIdx = FaceSetIdx * FaceSets.size() + FaceCountIdx;


							// Number of faces with the material index
							int FaceCount = FaceCounts[FaceCountIdx];

							// Walk each face and map it to the computed material index
							for (int FaceNum = 0; FaceNum < FaceCount; ++FaceNum)
							{
								int Face = FaceIndices[FaceNum + Offset];
								GeomData->FaceMaterialIndices[Face] = MaterialIdx;
							}
							Offset += FaceCount;
						}
					}
				}
			}

			// SubD
			{
				GeomData->SubdivisionScheme = EUsdSubdivisionScheme::CatmullClark;
				UsdAttribute SubDivScheme = Mesh.GetSubdivisionSchemeAttr();
				if (SubDivScheme)
				{
					static const TfToken CatmullClark("catmullClark");
					static const TfToken Loop("loop");
					static const TfToken Bilinear("bilinear");
					static const TfToken None("none");
					TfToken SchemeName;
					SubDivScheme.Get(&SchemeName, Time);

					if (SchemeName == CatmullClark)
					{
						GeomData->SubdivisionScheme = EUsdSubdivisionScheme::CatmullClark;
					}
					else if (SchemeName == Loop)
					{
						GeomData->SubdivisionScheme = EUsdSubdivisionScheme::Loop;
					}
					else if (SchemeName == Bilinear)
					{
						GeomData->SubdivisionScheme = EUsdSubdivisionScheme::Bilinear;
					}
					else if (SchemeName == None)
					{
						GeomData->SubdivisionScheme = EUsdSubdivisionScheme::None;
					}

				}
				UsdAttribute CreaseIndices = Mesh.GetCreaseIndicesAttr();
				if (CreaseIndices)
				{
					GeomData->CreaseIndices.clear();

					VtIntArray CreaseIndicesArray;
					CreaseIndices.Get(&CreaseIndicesArray, Time);
					GeomData->CreaseIndices.assign(CreaseIndicesArray.begin(), CreaseIndicesArray.end());
				}

				UsdAttribute CreaseLengths = Mesh.GetCreaseLengthsAttr();
				if (CreaseLengths)
				{
					GeomData->CreaseLengths.clear();

					VtIntArray CreaseLengthsArray;
					CreaseLengths.Get(&CreaseLengthsArray);
					GeomData->CreaseLengths.assign(CreaseLengthsArray.begin(), CreaseLengthsArray.end());
				}

				UsdAttribute CreaseSharpnesses = Mesh.GetCreaseSharpnessesAttr();
				if (CreaseSharpnesses)
				{
					GeomData->CreaseSharpnesses.clear();

					VtFloatArray CreaseSharpnessesArray;
					CreaseSharpnesses.Get(&CreaseSharpnessesArray);
					GeomData->CreaseSharpnesses.assign(CreaseSharpnessesArray.begin(), CreaseSharpnessesArray.end());
				}

				UsdAttribute CornerCreaseIndices = Mesh.GetCornerIndicesAttr();
				if (CornerCreaseIndices)
				{
					GeomData->CornerCreaseIndices.clear();

					VtIntArray CornerCreaseIndicesArray;
					CornerCreaseIndices.Get(&CornerCreaseIndicesArray);
					GeomData->CornerCreaseIndices.assign(CornerCreaseIndicesArray.begin(), CornerCreaseIndicesArray.end());
				}
				
				UsdAttribute CornerSharpnesses = Mesh.GetCornerSharpnessesAttr();
				if (CornerSharpnesses)
				{
					GeomData->CornerSharpnesses.clear();

					VtFloatArray CornerSharpnessesArray;
					CornerSharpnesses.Get(&CornerSharpnessesArray);
					GeomData->CornerSharpnesses.assign(CornerSharpnessesArray.begin(), CornerSharpnessesArray.end());
				}

			}

		}

		return GeomData;
	}

	virtual int GetNumLODs() const override
	{
		// 0 indicates no variant or no lods in variant. 
		int NumLODs = 0;
		if (Prim.HasVariantSets())
		{
			UsdVariantSet LODVariantSet = Prim.GetVariantSet(UnrealIdentifiers::LOD);
			if(LODVariantSet.IsValid())
			{
				vector<string> VariantNames = LODVariantSet.GetVariantNames();
				NumLODs = VariantNames.size();
			}
		}

		return NumLODs;
	}

	virtual IUsdPrim* GetLODChild(int LODIndex) override
	{
		if (Prim.HasVariantSets())
		{
			UsdVariantSet LODVariantSet = Prim.GetVariantSet(UnrealIdentifiers::LOD);
			if (LODVariantSet.IsValid())
			{
				char LODName[10];
#if _WINDOWS
				sprintf_s(LODName, "LOD%d", LODIndex);
#else
				sprintf(LODName, "LOD%d", LODIndex);
#endif
				LODVariantSet.SetVariantSelection(string(LODName));

				string ChildPrimName = PrimName + "_" + LODName;
				UsdPrim LODChild = Prim.GetChild(TfToken(ChildPrimName));
				if (LODChild)
				{

					auto Result = std::find_if(
						std::begin(VariantData),
						std::end(VariantData),
						[&LODChild](const FPrimAndData& Elem)
					{
						return Elem.Prim == LODChild;
					});

					if (Result == std::end(VariantData))
					{
						FUsdPrim* LODChildData = new FUsdPrim(LODChild);
						FPrimAndData LODData(LODChild);
						LODData.PrimData = LODChildData;

						VariantData.push_back(LODData);
						return LODData.PrimData;
					}
					else
					{
						return Result->PrimData;
					}
			
				}
			}
		}

		return nullptr;
	}

	virtual const std::vector<FUsdAttribute>& GetAttributes() const override
	{
		AllAttributes.clear();
		PrivateGetAttributes(AllAttributes, TfToken());

		return AllAttributes;
	}

	virtual const std::vector<FUsdAttribute>& GetUnrealPropertyAttributes() const override
	{
		UnrealPropAttributes.clear();
		PrivateGetAttributes(UnrealPropAttributes, UnrealIdentifiers::PropertyPath);

		return UnrealPropAttributes;
	}

	UsdPrim GetUSDPrim() { return Prim; }

private:
	void PrivateGetAttributes(std::vector<FUsdAttribute>& OutAttributes, const TfToken& ByMetadata) const
	{
		std::vector<UsdAttribute> Attributes = Prim.GetAttributes();
		OutAttributes.reserve(Attributes.size());

		for (UsdAttribute& Attr : Attributes)
		{
			if (ByMetadata.IsEmpty() || Attr.HasCustomDataKey(ByMetadata))
			{
				OutAttributes.push_back(
					FUsdAttribute(std::shared_ptr<FAttribInternalData>(new FAttribInternalData(Attr)))
				);
			}
		}
	}
private:
	UsdPrim Prim;
	vector<FPrimAndData> Children;
	vector<FPrimAndData> VariantData;
	mutable std::vector<FUsdAttribute> AllAttributes;
	mutable std::vector<FUsdAttribute> UnrealPropAttributes;
	string PrimName;
	string PrimPath;
	string UnrealAssetPath;
	string UnrealActorClass;
	string UnrealPropertyPath;
	string Kind;
	FUsdGeomData* GeomData;
};

class FUsdStage : public IUsdStage
{
public:
	FUsdStage(UsdStageRefPtr& InStage)
		: Stage(InStage)
		, RootPrim(nullptr)
	{
	}

	~FUsdStage()
	{
		if (RootPrim)
		{
			delete RootPrim;
		}
	}

	EUsdUpAxis GetUpAxis() const override
	{
		// note USD does not support X up
		return UsdGeomGetStageUpAxis(Stage) == UsdGeomTokens->y ? EUsdUpAxis::YAxis : EUsdUpAxis::ZAxis;
	}

	IUsdPrim* GetRootPrim() override
	{
		if (Stage && !RootPrim)
		{
			RootPrim = new FUsdPrim(Stage->GetPseudoRoot());
		}

		return RootPrim;
	}

	virtual bool HasAuthoredTimeCodeRange() const override
	{
		return Stage->HasAuthoredTimeCodeRange();
	}

	virtual double GetStartTimeCode() const override
	{
		return Stage->GetStartTimeCode();
	}

	virtual double GetEndTimeCode() const override
	{
		return Stage->GetEndTimeCode();
	}

	virtual double GetFramesPerSecond() const override
	{
		return Stage->GetFramesPerSecond();
	}

	virtual double GetTimeCodesPerSecond() const override
	{
		return Stage->GetTimeCodesPerSecond();
	}

private:
	UsdStageRefPtr Stage;
	FUsdPrim* RootPrim;
};

bool UnrealUSDWrapper::bInitialized = false;
FUsdStage* UnrealUSDWrapper::CurrentStage = nullptr;
std::string UnrealUSDWrapper::Errors;


void UnrealUSDWrapper::Initialize(const std::vector<std::string>& PluginDirectories)
{
	bInitialized = true;

	// Needed to find plugins in non-standard places
	PlugRegistry::GetInstance().RegisterPlugins(PluginDirectories);
}

IUsdStage* UnrealUSDWrapper::ImportUSDFile(const char* Path, const char* Filename)
{
	Errors.clear();

	if (!bInitialized)
	{
		return nullptr;
	}

	bool bImportedSuccessfully = false;

	// Clean up any old data
	CleanUp();

	TfErrorMark ErrorMark;

	string PathAndFilename = string(Path) + string(Filename);

	bool bIsSupported = UsdStage::IsSupportedFile(PathAndFilename);

	UsdStageRefPtr Stage = UsdStage::Open(PathAndFilename);

	if (Stage)
	{
		CurrentStage = new FUsdStage(Stage);
	}

	if (!ErrorMark.IsClean())
	{
		TfErrorMark::Iterator i;
		for (i = ErrorMark.GetBegin(); i != ErrorMark.GetEnd(); ++i)
		{
			Errors += i->GetErrorCodeAsString();
			Errors += " ";
			Errors += i->GetCommentary();
			Errors += "\n";
		}
	}

	return CurrentStage;
}

void UnrealUSDWrapper::CleanUp()
{
	if (CurrentStage)
	{
#if USDWRAPPER_USE_XFORMACHE
		XFormCache.Clear();
#endif // USDWRAPPER_USE_XFORMACHE

		delete CurrentStage;
		CurrentStage = nullptr;
	}
}

double UnrealUSDWrapper::GetDefaultTimeCode()
{
	return UsdTimeCode::Default().GetValue();
}

const char* UnrealUSDWrapper::GetErrors()
{
	return Errors.length() > 0 ? Errors.c_str() : nullptr;
}

