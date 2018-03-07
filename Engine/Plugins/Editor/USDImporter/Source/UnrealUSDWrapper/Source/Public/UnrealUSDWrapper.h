#pragma once

#include <string>
#include <vector>
#include <memory>

#ifdef UNREALUSDWRAPPER_EXPORTS
	#if _WINDOWS
		#define UNREALUSDWRAPPER_API __declspec(dllexport)
	#elif defined(__linux__) || defined(__APPLE__)
		#define UNREALUSDWRAPPER_API __attribute__((visibility("default")))
	#else
		#error "Add definition for UNREALUSDWRAPPER_API macro for your platform."
	#endif
#else
#define UNREALUSDWRAPPER_API //__declspec(dllimport)
#endif

void InitWrapper();

class IUsdPrim;

enum EUsdInterpolationMethod
{
	/** Each element in a buffer maps directly to a specific vertex */
	Vertex,
	/** Each element in a buffer maps to a specific face/vertex pair */
	FaceVarying,
	/** Each vertex on a face is the same value */
	Uniform,
};

enum EUsdGeomOrientation
{
	/** Right handed coordinate system */
	RightHanded,
	/** Left handed coordinate system */
	LeftHanded,
};

enum EUsdSubdivisionScheme
{
	None,
	CatmullClark,
	Loop,
	Bilinear,

};

enum EUsdUpAxis
{
	XAxis,
	YAxis,
	ZAxis,
};



struct FUsdVector2Data
{
	FUsdVector2Data(float InX = 0, float InY = 0)
		: X(InX)
		, Y(InY)
	{}

	float X;
	float Y;
};


struct FUsdVectorData
{
	FUsdVectorData(float InX = 0, float InY = 0, float InZ = 0)
		: X(InX)
		, Y(InY)
		, Z(InZ)
	{}

	float X;
	float Y;
	float Z;
};

struct FUsdVector4Data
{
	FUsdVector4Data(float InX = 0, float InY = 0, float InZ = 0, float InW = 0)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{}

	float X;
	float Y;
	float Z;
	float W;
};


struct FUsdUVData
{
	FUsdUVData()
	{}

	/** Defines how UVs are mapped to faces */
	EUsdInterpolationMethod UVInterpMethod;

	/** Raw UVs */
	std::vector<FUsdVector2Data> Coords;
};

struct FUsdQuatData
{
	FUsdQuatData(float InX=0, float InY = 0, float InZ = 0, float InW = 0)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{}

	float X;
	float Y;
	float Z;
	float W;
};

struct FUsdMatrixData
{
	static const int NumRows = 4;
	static const int NumColumns = 4;

	double Data[NumRows*NumColumns];

	double* operator[](int Row)
	{
		return Data + (Row*NumColumns);
	}

	const double* operator[](int Row) const
	{
		return Data + (Row*NumColumns);
	}
};

struct FUsdGeomData
{
	FUsdGeomData()
		: NumUVs(0)
	{}

	/** How many vertices are in each face.  The size of this array tells you how many faces there are */
	std::vector<int> FaceVertexCounts;
	/** Index buffer which matches faces to Points */
	std::vector<int> FaceIndices;
	/** Maps a face to a material index.  MaterialIndex = FaceMaterialIndices[FaceNum] */
	std::vector<int> FaceMaterialIndices;
	/** For subdivision surfaces these are the indices to vertices that have creases */
	std::vector<int> CreaseIndices;
	/** For subdivision surfaces.  Each element gives the number of (must be adjacent) vertices in each crease, whose indices are linearly laid out in the 'CreaseIndices' array. */
	std::vector<int> CreaseLengths;
	/** The per-crease or per-edge sharpness for all creases*/
	std::vector<float> CreaseSharpnesses;
	/** Indices to points that have sharpness */
	std::vector<int> CornerCreaseIndices;
	/** The per-corner sharpness for all corner creases*/
	std::vector<float> CornerSharpnesses;
	/** List of all vertices in the mesh. This just holds the untransformed position of the vertex */
	std::vector<FUsdVectorData> Points;
	/** List of all normals in the mesh.  */
	std::vector<FUsdVectorData> Normals;
	/** List of all vertex colors in the mesh */
	std::vector<FUsdVectorData> VertexColors;
	/** List of all materials in the mesh.  The size of this array represents the number of materials */
	std::vector<std::string> MaterialNames;

	/** Raw UVs.  The size of this array represents how many UV sets there are */
	FUsdUVData UVs[8];

	FUsdUVData SeparateUMap;
	FUsdUVData SeparateVMap;

	/** Orientation of the points */
	EUsdGeomOrientation Orientation;

	EUsdSubdivisionScheme SubdivisionScheme;

	EUsdInterpolationMethod VertexColorInterpMethod;

	int NumUVs;
};

class UnrealUSDWrapper
{
public:
	UNREALUSDWRAPPER_API static void Initialize(const std::vector<std::string>& PluginDirectories);
	UNREALUSDWRAPPER_API static class IUsdStage* ImportUSDFile(const char* Path, const char* Filename);
	UNREALUSDWRAPPER_API static void CleanUp();
	UNREALUSDWRAPPER_API static double GetDefaultTimeCode();
	UNREALUSDWRAPPER_API static const char* GetErrors();
private:
	static class FUsdStage* CurrentStage;
	static std::string Errors;
	static bool bInitialized;
};

class FAttribInternalData;

class FUsdAttribute
{
public:
	FUsdAttribute(std::shared_ptr<FAttribInternalData> InternalData);
	~FUsdAttribute();

	UNREALUSDWRAPPER_API const char* GetAttributeName() const;

	/** Returns the type name for an attribute or null if the attribute doesn't exist */
	UNREALUSDWRAPPER_API const char* GetTypeName() const;
	
	UNREALUSDWRAPPER_API bool AsInt(int64_t& OutVal, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const;
	UNREALUSDWRAPPER_API bool AsUnsignedInt(uint64_t& OutVal, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const;
	UNREALUSDWRAPPER_API bool AsDouble(double& OutVal, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const;
	UNREALUSDWRAPPER_API bool AsString(const char*& OutVal, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const;
	UNREALUSDWRAPPER_API bool AsBool(bool& OutVal, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const;
	UNREALUSDWRAPPER_API bool AsVector2(FUsdVector2Data& OutVal, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const;
	UNREALUSDWRAPPER_API bool AsVector3(FUsdVectorData& OutVal, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const;
	UNREALUSDWRAPPER_API bool AsVector4(FUsdVector4Data& OutVal, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const;
	UNREALUSDWRAPPER_API bool AsColor(FUsdVector4Data& OutVal, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const;

	UNREALUSDWRAPPER_API bool IsUnsigned() const;

	// Get the number of elements in the array if it is an array.  Otherwise -1
	UNREALUSDWRAPPER_API int GetArraySize() const;

	UNREALUSDWRAPPER_API const char* GetUnrealPropertyPath() const;
private:
	std::shared_ptr<FAttribInternalData> InternalData;
	
};



class IUsdPrim
{
public:
	virtual ~IUsdPrim() {}
	virtual const char* GetPrimName() const = 0;
	virtual const char* GetPrimPath() const = 0;
	virtual const char* GetUnrealPropertyPath() const = 0;
	virtual const char* GetKind() const = 0;
	virtual bool IsKindChildOf(const std::string& InKind) const = 0;
	virtual bool IsGroup() const = 0;
	virtual bool IsModel() const = 0;
	virtual bool IsUnrealProperty() const = 0;
	virtual bool HasTransform() const = 0;
	virtual FUsdMatrixData GetLocalToWorldTransform(double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const = 0;
	virtual FUsdMatrixData GetLocalToParentTransform(double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const = 0;
	virtual FUsdMatrixData GetLocalToAncestorTransform(IUsdPrim* Ancestor, double Time = UnrealUSDWrapper::GetDefaultTimeCode()) const = 0;

	virtual int GetNumChildren() const = 0;
	virtual IUsdPrim* GetChild(int ChildIndex) = 0;
	virtual const char* GetUnrealAssetPath() const = 0;
	virtual const char* GetUnrealActorClass() const = 0;

	virtual bool HasGeometryData() const = 0;
	/** Returns geometry data at the default USD time */
	virtual const FUsdGeomData* GetGeometryData() = 0;

	/** Returns usd geometry data at a given time.  Note that it will reuse internal structures so  */
	virtual const FUsdGeomData* GetGeometryData(double Time) = 0;
	virtual int GetNumLODs() const = 0;
	virtual IUsdPrim* GetLODChild(int LODIndex) = 0;

	virtual const std::vector<FUsdAttribute>& GetAttributes() const = 0;

	/** Get attributes which map to unreal properties (i.e have unrealPropertyPath metadata)*/
	virtual const std::vector<FUsdAttribute>& GetUnrealPropertyAttributes() const = 0;

};

class IUsdStage
{
public:
	virtual ~IUsdStage() {}
	virtual EUsdUpAxis GetUpAxis() const = 0;
	virtual IUsdPrim* GetRootPrim() = 0;
	virtual bool HasAuthoredTimeCodeRange() const = 0;
	virtual double GetStartTimeCode() const = 0;
	virtual double GetEndTimeCode() const = 0;
	virtual double GetFramesPerSecond() const = 0;
	virtual double GetTimeCodesPerSecond() const = 0;
};



