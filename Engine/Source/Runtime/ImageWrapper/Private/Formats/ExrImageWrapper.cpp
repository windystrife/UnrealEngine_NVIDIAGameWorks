// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ExrImageWrapper.h"
#include "ImageWrapperPrivate.h"

#include "HAL/PlatformTime.h"
#include "Math/Float16.h"


#if WITH_UNREALEXR

FExrImageWrapper::FExrImageWrapper()
	: FImageWrapperBase()
	, bUseCompression(true)
{
}


template <typename sourcetype> class FSourceImageRaw
{
public:
	FSourceImageRaw(const TArray<uint8>& SourceImageBitmapIN, uint32 ChannelsIN, uint32 WidthIN, uint32 HeightIN)
		: SourceImageBitmap(SourceImageBitmapIN),
			Width( WidthIN ),
			Height(HeightIN),
			Channels(ChannelsIN)
	{		
		check(SourceImageBitmap.Num() == Channels*Width*Height*sizeof(sourcetype));
	}

	uint32 GetXStride() const	{return sizeof(sourcetype) * Channels;}
	uint32 GetYStride() const	{return GetXStride() * Width;}

	const sourcetype* GetHorzLine(uint32 y, uint32 Channel)
	{
		return &SourceImageBitmap[(sizeof(sourcetype) * Channel) + (GetYStride() * y)];
	}

private:
	const TArray<uint8>& SourceImageBitmap;
	uint32 Width;
	uint32 Height;
	uint32 Channels;
};


class FMemFileOut : public Imf::OStream
{
public:
	//-------------------------------------------------------
	// A constructor that opens the file with the given name.
	// The destructor will close the file.
	//-------------------------------------------------------

	FMemFileOut(const char fileName[]) :
		Imf::OStream(fileName),
		Pos(0)
	{
	}

	virtual void write(const char c[/*n*/], int n)
	{
		for (int32 i = 0; i < n; ++i)
		{
			Data[Pos + i] = c[i];
		}
		Pos += n;
	}


	//---------------------------------------------------------
	// Get the current writing position, in bytes from the
	// beginning of the file.  If the next call to write() will
	// start writing at the beginning of the file, tellp()
	// returns 0.
	//---------------------------------------------------------

	virtual Imf::Int64 tellp()
	{
		return Pos;
	}


	//-------------------------------------------
	// Set the current writing position.
	// After calling seekp(i), tellp() returns i.
	//-------------------------------------------

	virtual void seekp(Imf::Int64 pos)
	{
		Pos = pos;
	}


	int64 Pos;
	TArray<uint8> Data;
};


class FMemFileIn : public Imf::IStream
{
public:
	//-------------------------------------------------------
	// A constructor that opens the file with the given name.
	// The destructor will close the file.
	//-------------------------------------------------------

	FMemFileIn(const void* InData, int32 InSize)
		: Imf::IStream("")
		, Data((const char *)InData)
		, Size(InSize)
		, Pos(0)
	{
	}

	//------------------------------------------------------
	// Read from the stream:
	//
	// read(c,n) reads n bytes from the stream, and stores
	// them in array c.  If the stream contains less than n
	// bytes, or if an I/O error occurs, read(c,n) throws
	// an exception.  If read(c,n) reads the last byte from
	// the file it returns false, otherwise it returns true.
	//------------------------------------------------------

	virtual bool read (char c[/*n*/], int n)
	{
		if(Pos + n > Size)
		{
			return false;
		}

		for (int32 i = 0; i < n; ++i)
		{
			c[i] = Data[Pos];
			++Pos;
		}

		return Pos >= Size;
	}

	//--------------------------------------------------------
	// Get the current reading position, in bytes from the
	// beginning of the file.  If the next call to read() will
	// read the first byte in the file, tellg() returns 0.
	//--------------------------------------------------------

	virtual Imf::Int64 tellg()
	{
		return Pos;
	}

	//-------------------------------------------
	// Set the current reading position.
	// After calling seekg(i), tellg() returns i.
	//-------------------------------------------

	virtual void seekg(Imf::Int64 pos)
	{
		Pos = pos;
	}

private:

	const char* Data;
	int32 Size;
	int64 Pos;
};


namespace
{
	/////////////////////////////////////////
	// 8 bit per channel source
	float ToLinear(uint8 LDRValue)
	{
		return pow((float)(LDRValue) / 255.f, 2.2f);
	}

	void ExtractAndConvertChannel(const uint8*Src, uint32 SrcChannels, uint32 x, uint32 y, float* ChannelOUT)
	{
		for (uint32 i = 0; i < x*y; i++, Src += SrcChannels)
		{
			ChannelOUT[i] = ToLinear(*Src);
		}
	}

	void ExtractAndConvertChannel(const uint8*Src, uint32 SrcChannels, uint32 x, uint32 y, FFloat16* ChannelOUT)
	{
		for (uint32 i = 0; i < x*y; i++, Src += SrcChannels)
		{
			ChannelOUT[i] = FFloat16(ToLinear(*Src));
		}
	}

	/////////////////////////////////////////
	// 16 bit per channel source
	void ExtractAndConvertChannel(const FFloat16*Src, uint32 SrcChannels, uint32 x, uint32 y, float* ChannelOUT)
	{
		for (uint32 i = 0; i < x*y; i++, Src += SrcChannels)
		{
			ChannelOUT[i] = Src->GetFloat();
		}
	}

	void ExtractAndConvertChannel(const FFloat16*Src, uint32 SrcChannels, uint32 x, uint32 y, FFloat16* ChannelOUT)
	{
		for (uint32 i = 0; i < x*y; i++, Src += SrcChannels)
		{
			ChannelOUT[i] = *Src;
		}
	}

	/////////////////////////////////////////
	// 32 bit per channel source
	void ExtractAndConvertChannel(const float* Src, uint32 SrcChannels, uint32 x, uint32 y, float* ChannelOUT)
	{
		for (uint32 i = 0; i < x*y; i++, Src += SrcChannels)
		{
			ChannelOUT[i] = *Src;
		}
	}

	void ExtractAndConvertChannel(const float* Src, uint32 SrcChannels, uint32 x, uint32 y, FFloat16* ChannelOUT)
	{
		for (uint32 i = 0; i < x*y; i++, Src += SrcChannels)
		{
			ChannelOUT[i] = *Src;
		}
	}

	/////////////////////////////////////////
	int32 GetNumChannelsFromFormat(ERGBFormat Format)
	{
		switch (Format)
		{
			case ERGBFormat::RGBA:
			case ERGBFormat::BGRA:
				return 4;
			case ERGBFormat::Gray:
				return 1;
		}
		checkNoEntry();
		return 1;
	}
}

const char* FExrImageWrapper::GetRawChannelName(int ChannelIndex) const
{
	const int32 MaxChannels = 4;
	static const char* RGBAChannelNames[] = { "R", "G", "B", "A" };
	static const char* BGRAChannelNames[] = { "B", "G", "R", "A" };
	static const char* GrayChannelNames[] = { "G" };
	check(ChannelIndex < MaxChannels);

	const char** ChannelNames = BGRAChannelNames;

	switch (RawFormat)
	{
		case ERGBFormat::RGBA:
		{
			ChannelNames = RGBAChannelNames;
		}
		break;
		case ERGBFormat::BGRA:
		{
			ChannelNames = BGRAChannelNames;
		}
		break;
		case ERGBFormat::Gray:
		{
			check(ChannelIndex < ARRAY_COUNT(GrayChannelNames));
			ChannelNames = GrayChannelNames;
		}
		break;
		default:
			checkNoEntry();
	}
	return ChannelNames[ChannelIndex];
}

template <typename Imf::PixelType OutputFormat>
struct TExrImageOutputChannelType;

template <> struct TExrImageOutputChannelType<Imf::HALF>  { typedef FFloat16 Type; };
template <> struct TExrImageOutputChannelType<Imf::FLOAT> { typedef float Type; };

template <Imf::PixelType OutputFormat, typename sourcetype>
void FExrImageWrapper::WriteFrameBufferChannel(Imf::FrameBuffer& ImfFrameBuffer, const char* ChannelName, const sourcetype* SrcData, TArray<uint8>& ChannelBuffer)
{
	const int32 OutputPixelSize = ((OutputFormat == Imf::HALF) ? 2 : 4);
	ChannelBuffer.AddUninitialized(Width*Height*OutputPixelSize);
	uint32 SrcChannels = GetNumChannelsFromFormat(RawFormat);
	ExtractAndConvertChannel(SrcData, SrcChannels, Width, Height, (typename TExrImageOutputChannelType<OutputFormat>::Type*)&ChannelBuffer[0]);
	Imf::Slice FrameChannel = Imf::Slice(OutputFormat, (char*)&ChannelBuffer[0], OutputPixelSize, Width*OutputPixelSize);
	ImfFrameBuffer.insert(ChannelName, FrameChannel);
}

template <Imf::PixelType OutputFormat, typename sourcetype>
void FExrImageWrapper::CompressRaw(const sourcetype* SrcData, bool bIgnoreAlpha)
{
	const double StartTime = FPlatformTime::Seconds();
	uint32 NumWriteComponents = GetNumChannelsFromFormat(RawFormat);
	if (bIgnoreAlpha && NumWriteComponents == 4)
	{
		NumWriteComponents = 3;
	}

	Imf::Compression Comp = bUseCompression ? Imf::Compression::ZIP_COMPRESSION : Imf::Compression::NO_COMPRESSION;
	Imf::Header Header(Width, Height, 1, Imath::V2f(0, 0), 1, Imf::LineOrder::INCREASING_Y, Comp);
	
	for (uint32 Channel = 0; Channel < NumWriteComponents; Channel++)
	{
		Header.channels().insert(GetRawChannelName(Channel), Imf::Channel(OutputFormat));
	}

	// OutputFormat is 0 UINT, 1 HALF or 2 FLOAT. Size evaluates to 2/4/8 or 1/2/4 compressed
	const int32 OutputPixelSize = (bUseCompression ? 1 : 2) << OutputFormat;
	checkSlow(OutputPixelSize >= 1 && OutputPixelSize <= 8);

	FMemFileOut MemFile("");
	MemFile.Data.AddUninitialized(Width * Height * NumWriteComponents * OutputPixelSize);

	Imf::FrameBuffer ImfFrameBuffer;
	TArray<uint8> ChannelOutputBuffers[4];

	for (uint32 Channel = 0; Channel < NumWriteComponents; Channel++)
	{
		WriteFrameBufferChannel<OutputFormat>(ImfFrameBuffer, GetRawChannelName(Channel), SrcData + Channel, ChannelOutputBuffers[Channel]);
	}

	Imf::OutputFile ImfFile(MemFile, Header);
	ImfFile.setFrameBuffer(ImfFrameBuffer);
	ImfFile.writePixels(Height);

	CompressedData.AddUninitialized(MemFile.tellp());
	FMemory::Memcpy(CompressedData.GetData(), MemFile.Data.GetData(), MemFile.tellp());

	const double DeltaTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogImageWrapper, Verbose, TEXT("Compressed image in %.3f seconds"), DeltaTime);
}

void FExrImageWrapper::Compress( int32 Quality )
{
	check(RawData.Num() != 0);
	check(Width > 0);
	check(Height > 0);
	check(RawBitDepth == 8 || RawBitDepth == 16 || RawBitDepth == 32);

	const int32 MaxComponents = 4;

	bUseCompression = (Quality != (int32)EImageCompressionQuality::Uncompressed);

	switch (RawBitDepth)
	{
		case 8:
			CompressRaw<Imf::HALF>(&RawData[0], false);
		break;
		case 16:
			CompressRaw<Imf::HALF>((const FFloat16*)&RawData[0], false);
		break;
		case 32:
			CompressRaw<Imf::FLOAT>((const float*)&RawData[0], false);
		break;
		default:
			checkNoEntry();
	}
}

void FExrImageWrapper::Uncompress( const ERGBFormat InFormat, const int32 InBitDepth )
{
	// Ensure we haven't already uncompressed the file.
	if ( RawData.Num() != 0 )
	{
		return;
	}

	FMemFileIn MemFile(&CompressedData[0], CompressedData.Num());

	Imf::RgbaInputFile ImfFile(MemFile);

	Imath::Box2i win = ImfFile.dataWindow();

	check(BitDepth == 16);
	check(Width);
	check(Height);

	uint32 Channels = 4;

	RawData.Empty();
	RawData.AddUninitialized( Width * Height * Channels * (BitDepth / 8) );

	int dx = win.min.x;
	int dy = win.min.y;

	ImfFile.setFrameBuffer((Imf::Rgba*)&RawData[0] - dx - dy * Width, 1, Width);
	ImfFile.readPixels(win.min.y, win.max.y);
}

// from http://www.openexr.com/ReadingAndWritingImageFiles.pdf
bool IsThisAnOpenExrFile(Imf::IStream& f)
{
	char b[4];
	f.read(b, sizeof(b));

	f.seekg(0);

	return b[0] == 0x76 && b[1] == 0x2f && b[2] == 0x31 && b[3] == 0x01;
}

bool FExrImageWrapper::SetCompressed( const void* InCompressedData, int32 InCompressedSize )
{
	if(!FImageWrapperBase::SetCompressed( InCompressedData, InCompressedSize))
	{
		return false;
	}

	FMemFileIn MemFile(InCompressedData, InCompressedSize);

	if(!IsThisAnOpenExrFile(MemFile))
	{
		return false;
	}

	Imf::RgbaInputFile ImfFile(MemFile);

	Imath::Box2i win = ImfFile.dataWindow();

	Imath::V2i dim(win.max.x - win.min.x + 1, win.max.y - win.min.y + 1);

	BitDepth = 16;

	Width = dim.x;
	Height = dim.y;

	// ideally we can specify float here
	Format = ERGBFormat::RGBA;

	return true;
}

#endif // WITH_UNREALEXR
