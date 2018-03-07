// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerTypes.h"
#include "InputCoreTypes.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "GameplayDebuggerConfig.h"
#include "DrawDebugHelpers.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"

DEFINE_LOG_CATEGORY(LogGameplayDebug);

//////////////////////////////////////////////////////////////////////////
// FGameplayDebuggerShape

FGameplayDebuggerShape FGameplayDebuggerShape::MakePoint(const FVector& Location, const float Radius, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShape NewElement;
	NewElement.ShapeData.Add(Location);
	NewElement.ShapeData.Add(FVector(Radius, 0, 0));
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShape::Point;

	return NewElement;
}

FGameplayDebuggerShape FGameplayDebuggerShape::MakeSegment(const FVector& StartLocation, const FVector& EndLocation, const float Thickness, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShape NewElement;
	NewElement.ShapeData.Add(StartLocation);
	NewElement.ShapeData.Add(EndLocation);
	NewElement.ShapeData.Add(FVector(Thickness, 0, 0));
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShape::Segment;

	return NewElement;
}

FGameplayDebuggerShape FGameplayDebuggerShape::MakeSegment(const FVector& StartLocation, const FVector& EndLocation, const FColor& Color, const FString& Description)
{
	return MakeSegment(StartLocation, EndLocation, 1.0f, Color, Description);
}

FGameplayDebuggerShape FGameplayDebuggerShape::MakeBox(const FVector& Center, const FVector& Extent, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShape NewElement;
	NewElement.ShapeData.Add(Center);
	NewElement.ShapeData.Add(Extent);
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShape::Box;

	return NewElement;
}

FGameplayDebuggerShape FGameplayDebuggerShape::MakeCone(const FVector& Location, const FVector& Direction, const float Length, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShape NewElement;
	NewElement.ShapeData.Add(Location);
	NewElement.ShapeData.Add(Direction);
	NewElement.ShapeData.Add(FVector(Length, 0, 0));
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShape::Cone;

	return NewElement;
}

FGameplayDebuggerShape FGameplayDebuggerShape::MakeCylinder(const FVector& Center, const float Radius, const float HalfHeight, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShape NewElement;
	NewElement.ShapeData.Add(Center);
	NewElement.ShapeData.Add(FVector(Radius, 0, HalfHeight));
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShape::Cylinder;

	return NewElement;
}

FGameplayDebuggerShape FGameplayDebuggerShape::MakeCapsule(const FVector& Center, const float Radius, const float HalfHeight, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShape NewElement;
	NewElement.ShapeData.Add(Center);
	NewElement.ShapeData.Add(FVector(Radius, 0, HalfHeight));
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShape::Capsule;

	return NewElement;
}

FGameplayDebuggerShape FGameplayDebuggerShape::MakePolygon(const TArray<FVector>& Verts, const FColor& Color, const FString& Description)
{
	FGameplayDebuggerShape NewElement;
	NewElement.ShapeData = Verts;
	NewElement.Color = Color;
	NewElement.Description = Description;
	NewElement.Type = EGameplayDebuggerShape::Polygon;
	return NewElement;
}

void FGameplayDebuggerShape::Draw(UWorld* World, FGameplayDebuggerCanvasContext& Context)
{
	FVector DescLocation;
	switch (Type)
	{
	case EGameplayDebuggerShape::Point:
		if (ShapeData.Num() == 2 && ShapeData[1].X > 0)
		{
			DrawDebugSphere(World, ShapeData[0], ShapeData[1].X, 16, Color);
			DescLocation = ShapeData[0];
		}
		break;

	case EGameplayDebuggerShape::Segment:
		if (ShapeData.Num() == 3 && ShapeData[2].X > 0)
		{
			DrawDebugLine(World, ShapeData[0], ShapeData[1], Color, false, -1.0f, 0, ShapeData[2].X);
			DescLocation = (ShapeData[0] + ShapeData[1]) * 0.5f;
		}
		break;

	case EGameplayDebuggerShape::Box:
		if (ShapeData.Num() == 2)
		{
			DrawDebugBox(World, ShapeData[0], ShapeData[1], Color);
			DescLocation = ShapeData[0];
		}
		break;

	case EGameplayDebuggerShape::Cone:
		if (ShapeData.Num() == 3 && ShapeData[2].X > 0)
		{
			DrawDebugCone(World, ShapeData[0], ShapeData[1], ShapeData[2].X, PI * 0.5f, PI * 0.5f, 16, Color);
			DescLocation = ShapeData[0];
		}
		break;

	case EGameplayDebuggerShape::Cylinder:
		if (ShapeData.Num() == 2)
		{
			DrawDebugCylinder(World, ShapeData[0] - FVector(0, 0, ShapeData[1].Z), ShapeData[0] + FVector(0, 0, ShapeData[1].Z), ShapeData[1].X, 16, Color);
			DescLocation = ShapeData[0];
		}
		break;

	case EGameplayDebuggerShape::Capsule:
		if (ShapeData.Num() == 2)
		{
			DrawDebugCapsule(World, ShapeData[0], ShapeData[1].Z, ShapeData[1].X, FQuat::Identity, Color);
			DescLocation = ShapeData[0];
		}
		break;

	case EGameplayDebuggerShape::Polygon:
		if (ShapeData.Num() > 0)
		{
			FVector MidPoint = FVector::ZeroVector;
			TArray<int32> Indices;
			for (int32 Idx = 0; Idx < ShapeData.Num(); Idx++)
			{
				Indices.Add(Idx);
				MidPoint += ShapeData[Idx];
			}

			DrawDebugMesh(World, ShapeData, Indices, Color);
			DescLocation = MidPoint / ShapeData.Num();
		}
		break;

	default:
		break;
	}

	if (Description.Len() && Context.IsLocationVisible(DescLocation))
	{
		const FVector2D ScreenLoc = Context.ProjectLocation(DescLocation);
		Context.PrintAt(ScreenLoc.X, ScreenLoc.Y, Color, Description);
	}
}

FArchive& operator<<(FArchive& Ar, FGameplayDebuggerShape& Shape)
{
	Ar << Shape.ShapeData;
	Ar << Shape.Description;
	Ar << Shape.Color;

	uint8 TypeNum = static_cast<uint8>(Shape.Type);
	Ar << TypeNum;
	Shape.Type = static_cast<EGameplayDebuggerShape>(TypeNum);

	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FGameplayDebuggerCanvasContext

enum class EStringParserToken : uint8
{
	OpenTag,
	CloseTag,
	NewLine,
	EndOfString,
	RegularChar,
	Tab,
};

class FTaggedStringParser
{
public:
	struct FNode
	{
		FString String;
		FColor Color;
		bool bNewLine;

		FNode() : Color(FColor::White), bNewLine(false) {}
		FNode(const FColor& InColor) : Color(InColor), bNewLine(false) {}
	};

	TArray<FNode> NodeList;

	FTaggedStringParser(const FColor& InDefaultColor) : DefaultColor(InDefaultColor) {}

	void ParseString(const FString& StringToParse)
	{
		DataIndex = 0;
		DataString = StringToParse;
		if (DataIndex >= DataString.Len())
		{
			return;
		}

		const FString TabString(TEXT("     "));
		FColor TagColor;
		FNode CurrentNode(DefaultColor);

		for (EStringParserToken Token = ReadToken(); Token != EStringParserToken::EndOfString; Token = ReadToken())
		{
			switch (Token)
			{
			case EStringParserToken::RegularChar:
				CurrentNode.String.AppendChar(DataString[DataIndex]);
				break;

			case EStringParserToken::NewLine:
				NodeList.Add(CurrentNode);
				CurrentNode = FNode(NodeList.Last().Color);
				CurrentNode.bNewLine = true;
				break;

			case EStringParserToken::Tab:
				CurrentNode.String.Append(TabString);
				break;

			case EStringParserToken::OpenTag:
				if (ParseTag(TagColor))
				{
					NodeList.Add(CurrentNode);
					CurrentNode = FNode(TagColor);
				}
				break;
			}

			DataIndex++;
		}

		NodeList.Add(CurrentNode);
	}

private:

	int32 DataIndex;
	FString DataString;
	FColor DefaultColor;

	EStringParserToken ReadToken() const
	{
		EStringParserToken OutToken = EStringParserToken::RegularChar;

		const TCHAR Char = DataIndex < DataString.Len() ? DataString[DataIndex] : TEXT('\0');
		switch (Char)
		{
		case TEXT('\0'):
			OutToken = EStringParserToken::EndOfString;
			break;
		case TEXT('{'):
			OutToken = EStringParserToken::OpenTag;
			break;
		case TEXT('}'):
			OutToken = EStringParserToken::CloseTag;
			break;
		case TEXT('\n'):
			OutToken = EStringParserToken::NewLine;
			break;
		case TEXT('\t'):
			OutToken = EStringParserToken::Tab;
			break;
		default:
			break;
		}

		return OutToken;
	}

	bool ParseTag(FColor& OutColor)
	{
		FString TagString;

		EStringParserToken Token = ReadToken();
		for (; Token != EStringParserToken::EndOfString && Token != EStringParserToken::CloseTag; Token = ReadToken())
		{
			if (Token == EStringParserToken::RegularChar)
			{
				TagString.AppendChar(DataString[DataIndex]);
			}
			
			DataIndex++;
		}

		bool bResult = false;
		if (Token == EStringParserToken::CloseTag)
		{
			const FString TagColorLower = TagString.ToLower();
			const bool bIsColorName = GColorList.IsValidColorName(*TagColorLower);
			
			if (bIsColorName)
			{
				OutColor = GColorList.GetFColorByName(*TagColorLower);
				bResult = true;
			}
			else
			{
				bResult = OutColor.InitFromString(TagString);
			}
		}

		return bResult;
	}
};

FGameplayDebuggerCanvasContext::FGameplayDebuggerCanvasContext(UCanvas* InCanvas, UFont* InFont)
{
	if (InCanvas)
	{
		Canvas = InCanvas;
		Font = InFont;
		CursorX = DefaultX = InCanvas->SafeZonePadX;
		CursorY = DefaultY = InCanvas->SafeZonePadY;
	}
	else
	{
		CursorX = DefaultX = 0.0f;
		CursorY = DefaultY = 0.0f;
	}
}

void FGameplayDebuggerCanvasContext::Print(const FString& String)
{
	Print(FColor::White, String);
}

void FGameplayDebuggerCanvasContext::Print(const FColor& Color, const FString& String)
{
	FTaggedStringParser Parser(Color);
	Parser.ParseString(String);

	const float LineHeight = GetLineHeight();
	for (int32 NodeIdx = 0; NodeIdx < Parser.NodeList.Num(); NodeIdx++)
	{
		const FTaggedStringParser::FNode& NodeData = Parser.NodeList[NodeIdx];
		if (NodeData.bNewLine)
		{
			if (Canvas.IsValid() && (CursorY + LineHeight) > Canvas->ClipY)
			{
				DefaultX += Canvas->ClipX / 2;
				CursorY = 0.0f;
			}

			CursorX = DefaultX;
			CursorY += LineHeight;
		}

		if (NodeData.String.Len() > 0)
		{
			float SizeX = 0.0f, SizeY = 0.0f;
			MeasureString(NodeData.String, SizeX, SizeY);

			FCanvasTextItem TextItem(FVector2D::ZeroVector, FText::FromString(NodeData.String), Font.Get(), FLinearColor(NodeData.Color));
			if (FontRenderInfo.bEnableShadow)
			{
				TextItem.EnableShadow(FColor::Black, FVector2D(1, 1));
			}

			DrawItem(TextItem, CursorX, CursorY);
			CursorX += SizeX;
		}
	}

	MoveToNewLine();
}

void FGameplayDebuggerCanvasContext::PrintAt(float PosX, float PosY, const FString& String)
{
	const float SavedPosX = CursorX;
	const float SavedPosY = CursorY;
	const float SavedDefX = DefaultX;

	DefaultX = CursorX = PosX;
	DefaultY = CursorY = PosY;
	Print(FColor::White, String);

	CursorX = SavedPosX;
	CursorY = SavedPosY;
	DefaultX = SavedDefX;
}

void FGameplayDebuggerCanvasContext::PrintAt(float PosX, float PosY, const FColor& Color, const FString& String)
{
	const float SavedPosX = CursorX;
	const float SavedPosY = CursorY;
	const float SavedDefX = DefaultX;

	DefaultX = CursorX = PosX;
	DefaultY = CursorY = PosY;
	Print(Color, String);

	CursorX = SavedPosX;
	CursorY = SavedPosY;
	DefaultX = SavedDefX;
}

// copied from Core/Private/Misc/VarargsHeler.h 
#define GROWABLE_PRINTF(PrintFunc) \
	int32	BufferSize	= 1024; \
	TCHAR*	Buffer		= NULL; \
	int32	Result		= -1; \
	/* allocate some stack space to use on the first pass, which matches most strings */ \
	TCHAR	StackBuffer[512]; \
	TCHAR*	AllocatedBuffer = NULL; \
\
	/* first, try using the stack buffer */ \
	Buffer = StackBuffer; \
	GET_VARARGS_RESULT( Buffer, ARRAY_COUNT(StackBuffer), ARRAY_COUNT(StackBuffer) - 1, Fmt, Fmt, Result ); \
\
	/* if that fails, then use heap allocation to make enough space */ \
	while(Result == -1) \
	{ \
		FMemory::SystemFree(AllocatedBuffer); \
		/* We need to use malloc here directly as GMalloc might not be safe. */ \
		Buffer = AllocatedBuffer = (TCHAR*) FMemory::SystemMalloc( BufferSize * sizeof(TCHAR) ); \
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result ); \
		BufferSize *= 2; \
	}; \
	Buffer[Result] = 0; \
	; \
\
	PrintFunc; \
	FMemory::SystemFree(AllocatedBuffer);

VARARG_BODY(void, FGameplayDebuggerCanvasContext::Printf, const TCHAR*, VARARG_NONE)
{
	GROWABLE_PRINTF(Print(Buffer));
}

VARARG_BODY(void, FGameplayDebuggerCanvasContext::Printf, const TCHAR*, VARARG_EXTRA(const FColor& Color))
{
	GROWABLE_PRINTF(Print(Color, Buffer));
}

VARARG_BODY(void, FGameplayDebuggerCanvasContext::PrintfAt, const TCHAR*, VARARG_EXTRA(float PosX) VARARG_EXTRA(float PosY))
{
	GROWABLE_PRINTF(PrintAt(PosX, PosY, Buffer));
}

VARARG_BODY(void, FGameplayDebuggerCanvasContext::PrintfAt, const TCHAR*, VARARG_EXTRA(float PosX) VARARG_EXTRA(float PosY) VARARG_EXTRA(const FColor& Color))
{
	GROWABLE_PRINTF(PrintAt(PosX, PosY, Color, Buffer));
}

void FGameplayDebuggerCanvasContext::MoveToNewLine()
{
	const float LineHeight = GetLineHeight();
	CursorY += LineHeight;
	CursorX = DefaultX;
}

void FGameplayDebuggerCanvasContext::MeasureString(const FString& String, float& OutSizeX, float& OutSizeY) const
{
	OutSizeX = OutSizeY = 0.0f;

	UCanvas* CanvasOb = Canvas.Get();
	if (CanvasOb)
	{
		FString StringWithoutFormatting = String;
		int32 BracketStart = INDEX_NONE;
		while (StringWithoutFormatting.FindChar(TEXT('{'), BracketStart))
		{
			int32 BracketEnd = INDEX_NONE;
			if (StringWithoutFormatting.FindChar(TEXT('}'), BracketEnd))
			{
				if (BracketEnd > BracketStart)
				{
					StringWithoutFormatting.RemoveAt(BracketStart, BracketEnd - BracketStart + 1, false);
				}
			}
		}

		TArray<FString> Lines;
		StringWithoutFormatting.ParseIntoArrayLines(Lines);
		
		UFont* FontOb = Font.Get();
		for (int32 Idx = 0; Idx < Lines.Num(); Idx++)
		{
			float LineSizeX = 0.0f, LineSizeY = 0.0f;
			CanvasOb->StrLen(FontOb, Lines[Idx], LineSizeX, LineSizeY);

			OutSizeX = FMath::Max(OutSizeX, LineSizeX);
			OutSizeY += LineSizeY;
		}
	}
}

float FGameplayDebuggerCanvasContext::GetLineHeight() const
{
	UFont* FontOb = Font.Get();
	return FontOb ? FontOb->GetMaxCharHeight() : 0.0f;
}

FVector2D FGameplayDebuggerCanvasContext::ProjectLocation(const FVector& Location) const
{
	UCanvas* CanvasOb = Canvas.Get();
	return CanvasOb ? FVector2D(CanvasOb->Project(Location)) : FVector2D::ZeroVector;
}

bool FGameplayDebuggerCanvasContext::IsLocationVisible(const FVector& Location) const
{
	return Canvas.IsValid() && Canvas->SceneView && Canvas->SceneView->ViewFrustum.IntersectSphere(Location, 1.0f);
}

void FGameplayDebuggerCanvasContext::DrawItem(FCanvasItem& Item, float PosX, float PosY)
{
	UCanvas* CanvasOb = Canvas.Get();
	if (CanvasOb)
	{
		CanvasOb->DrawItem(Item, PosX, PosY);
	}
}

void FGameplayDebuggerCanvasContext::DrawIcon(const FColor& Color, const FCanvasIcon& Icon, float PosX, float PosY, float Scale)
{
	UCanvas* CanvasOb = Canvas.Get();
	if (CanvasOb)
	{
		CanvasOb->SetDrawColor(Color);
		CanvasOb->DrawIcon(Icon, PosX, PosY, Scale);
	}
}

//////////////////////////////////////////////////////////////////////////
// FGameplayDebuggerDataPack

int32 FGameplayDebuggerDataPack::PacketSize = 512;

bool FGameplayDebuggerDataPack::CheckDirtyAndUpdate()
{
	TArray<uint8> UncompressedBuffer;
	FMemoryWriter ArWriter(UncompressedBuffer);
	SerializeDelegate.Execute(ArWriter);

	const uint32 NewDataCRC = FCrc::MemCrc32(UncompressedBuffer.GetData(), UncompressedBuffer.Num());
	if ((NewDataCRC == DataCRC) && !bIsDirty)
	{
		return false;
	}

	DataCRC = NewDataCRC;
	return true;
}

bool FGameplayDebuggerDataPack::RequestReplication(int16 SyncCounter)
{
	if (bNeedsConfirmation && !bReceived)
	{
		return false;
	}

	TArray<uint8> UncompressedBuffer;
	FMemoryWriter ArWriter(UncompressedBuffer);
	SerializeDelegate.Execute(ArWriter);

	const uint32 NewDataCRC = FCrc::MemCrc32(UncompressedBuffer.GetData(), UncompressedBuffer.Num());
	if ((NewDataCRC == DataCRC) && !bIsDirty)
	{
		return false;
	}

	const int32 MaxUncompressedDataSize = PacketSize;

	Header.bIsCompressed = (UncompressedBuffer.Num() > MaxUncompressedDataSize);
	if (Header.bIsCompressed)
	{
		const int32 UncompressedSize = UncompressedBuffer.Num();

		int32 CompressionHeader = UncompressedSize;
		const int32 CompressionHeaderSize = sizeof(CompressionHeader);

		int32 CompressedSize = FMath::TruncToInt(1.1f * UncompressedSize);
		Data.SetNum(CompressionHeaderSize + CompressedSize);

		uint8* CompressedBuffer = Data.GetData();
		FMemory::Memcpy(CompressedBuffer, &CompressionHeader, CompressionHeaderSize);
		CompressedBuffer += CompressionHeaderSize;

		FCompression::CompressMemory((ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory),
			CompressedBuffer, CompressedSize, UncompressedBuffer.GetData(), UncompressedSize);

		Data.SetNum(CompressionHeaderSize + CompressedSize);
	}
	else
	{
		Data = UncompressedBuffer;
	}

	bNeedsConfirmation = IsMultiPacket(Data.Num());
	bReceived = false;
	bIsDirty = false;

	DataCRC = NewDataCRC;
	Header.DataOffset = 0;
	Header.DataSize = Data.Num();
	Header.SyncCounter = SyncCounter;
	Header.DataVersion++;
	return true;
}

void FGameplayDebuggerDataPack::OnReplicated()
{
	if (Header.DataSize == 0)
	{
		ResetDelegate.Execute();
		return;
	}

	if (Header.bIsCompressed)
	{
		uint8* CompressedBuffer = Data.GetData();
		int32 CompressionHeader = 0;
		const int32 CompressionHeaderSize = sizeof(CompressionHeader);

		FMemory::Memcpy(&CompressionHeader, CompressedBuffer, CompressionHeaderSize);
		CompressedBuffer += CompressionHeaderSize;

		const int32 CompressedSize = Data.Num() - CompressionHeaderSize;
		const int32 UncompressedSize = CompressionHeader;
		TArray<uint8> UncompressedBuffer;
		UncompressedBuffer.AddUninitialized(UncompressedSize);

		FCompression::UncompressMemory((ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory),
			UncompressedBuffer.GetData(), UncompressedSize, CompressedBuffer, CompressedSize);

		FMemoryReader ArReader(UncompressedBuffer);
		SerializeDelegate.Execute(ArReader);
	}
	else
	{
		FMemoryReader ArReader(Data);
		SerializeDelegate.Execute(ArReader);
	}

	Header.DataOffset = Header.DataSize;
}

void FGameplayDebuggerDataPack::OnPacketRequest(int16 DataVersion, int32 DataOffset)
{
	// client should confirm with the same version and offset that server currently replicates
	if (DataVersion == Header.DataVersion && DataOffset == Header.DataOffset)
	{
		Header.DataOffset = FMath::Min(DataOffset + FGameplayDebuggerDataPack::PacketSize, Header.DataSize);
		bReceived = (Header.DataOffset == Header.DataSize);
	}
	// if for some reason it requests previous data version, rollback to first packet
	else if (DataVersion < Header.DataVersion)
	{
		Header.DataOffset = 0;
	}
	// it may also request a previous packet from the same version, rollback and send next one
	else if (DataVersion == Header.DataVersion && DataOffset < Header.DataOffset)
	{
		Header.DataOffset = FMath::Max(0, DataOffset + FGameplayDebuggerDataPack::PacketSize);
	}
}

//////////////////////////////////////////////////////////////////////////
// FGameplayDebuggerInputModifier

FGameplayDebuggerInputModifier FGameplayDebuggerInputModifier::Shift(true, false, false, false);
FGameplayDebuggerInputModifier FGameplayDebuggerInputModifier::Ctrl(false, true, false, false);
FGameplayDebuggerInputModifier FGameplayDebuggerInputModifier::Alt(false, false, true, false);
FGameplayDebuggerInputModifier FGameplayDebuggerInputModifier::Cmd(false, false, false, true);
FGameplayDebuggerInputModifier FGameplayDebuggerInputModifier::None;

//////////////////////////////////////////////////////////////////////////
// FGameplayDebuggerInputHandler

bool FGameplayDebuggerInputHandler::IsValid() const
{
	return FKey(KeyName).IsValid();
}

FString FGameplayDebuggerInputHandler::ToString() const
{
	FString KeyDesc = KeyName.ToString();
	
	if (Modifier.bShift)
	{
		KeyDesc = FString(TEXT("Shift+")) + KeyDesc;
	}

	if (Modifier.bAlt)
	{
		KeyDesc = FString(TEXT("Alt+")) + KeyDesc;
	}
	
	if (Modifier.bCtrl)
	{
		KeyDesc = FString(TEXT("Ctrl+")) + KeyDesc;
	}
	
	if (Modifier.bCmd)
	{
		KeyDesc = FString(TEXT("Cmd+")) + KeyDesc;
	}

	return KeyDesc;
}

//////////////////////////////////////////////////////////////////////////
// FGameplayDebuggerInputHandlerConfig

FName FGameplayDebuggerInputHandlerConfig::CurrentCategoryName;
FName FGameplayDebuggerInputHandlerConfig::CurrentExtensionName;

FGameplayDebuggerInputHandlerConfig::FGameplayDebuggerInputHandlerConfig(const FName ConfigName, const FName DefaultKeyName)
{
	KeyName = DefaultKeyName;
	UpdateConfig(ConfigName);
}

FGameplayDebuggerInputHandlerConfig::FGameplayDebuggerInputHandlerConfig(const FName ConfigName, const FName DefaultKeyName, const FGameplayDebuggerInputModifier& DefaultModifier)
{
	KeyName = DefaultKeyName;
	Modifier = DefaultModifier;
	UpdateConfig(ConfigName);
}

void FGameplayDebuggerInputHandlerConfig::UpdateConfig(const FName ConfigName)
{
	if (FGameplayDebuggerInputHandlerConfig::CurrentCategoryName != NAME_None)
	{
		UGameplayDebuggerConfig* MutableToolConfig = UGameplayDebuggerConfig::StaticClass()->GetDefaultObject<UGameplayDebuggerConfig>();
		MutableToolConfig->UpdateCategoryInputConfig(FGameplayDebuggerInputHandlerConfig::CurrentCategoryName, ConfigName, KeyName, Modifier);
	}
	else if (FGameplayDebuggerInputHandlerConfig::CurrentExtensionName != NAME_None)
	{
		UGameplayDebuggerConfig* MutableToolConfig = UGameplayDebuggerConfig::StaticClass()->GetDefaultObject<UGameplayDebuggerConfig>();
		MutableToolConfig->UpdateExtensionInputConfig(FGameplayDebuggerInputHandlerConfig::CurrentExtensionName, ConfigName, KeyName, Modifier);
	}
}
