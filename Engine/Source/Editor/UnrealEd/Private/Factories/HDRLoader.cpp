// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Factories/HDRLoader.h"
#include "DDSLoader.h"

DEFINE_LOG_CATEGORY_STATIC(LogHDRLoader, Log, All);

FHDRLoadHelper::FHDRLoadHelper(const uint8* Buffer, uint32 BufferLength)
	: RGBDataStart(0), Width(0), Height(0)
{
	if(BufferLength < 11)
	{
		return;
	}

	const uint8* FileDataPtr = Buffer;

	char Line[256];

	GetHeaderLine(FileDataPtr, Line);

	if(FCStringAnsi::Strcmp(Line, "#?RADIANCE"))
	{
		return;
	}

	for(;;)
	{
		GetHeaderLine(FileDataPtr, Line);

		char* HeightStr = FCStringAnsi::Strstr(Line, "-Y ");
		char* WidthStr = FCStringAnsi::Strstr(Line, "+X ");

		if (HeightStr != NULL && WidthStr != NULL)
		{
			// insert a /0 after the height value
			*(WidthStr-1) = 0;

			Height = FCStringAnsi::Atoi(HeightStr+3);
			Width = FCStringAnsi::Atoi(WidthStr+3);

			RGBDataStart = FileDataPtr;
			check(IsValid());
			return;
		}
	}
}

uint32 FHDRLoadHelper::GetWidth() const
{
	return Width;
}

uint32 FHDRLoadHelper::GetHeight() const
{
	return Height;
}

void FHDRLoadHelper::GetHeaderLine(const uint8*& BufferPos, char Line[256]) const
{
	char* LinePtr = Line;

	uint32 i;

	for(i = 0; i < 255; ++i)
	{
		if(*BufferPos == 0 || *BufferPos == 10 || *BufferPos == 13)
		{
			++BufferPos;
			break;
		}

		*LinePtr++ = *BufferPos++;
	}

	Line[i] = 0;
}

bool FHDRLoadHelper::IsValid() const
{
	return RGBDataStart && Width && Height;
}

void FHDRLoadHelper::DecompressWholeImage(uint32 *OutRGBEData) const
{
	check(IsValid());

	const uint8* FileDataPtr = RGBDataStart;

	for(uint32 y = 0; y < Height; ++y)
	{
		uint8* Line = (uint8*)&OutRGBEData[Width * y];

		DecompressScanline(Line, FileDataPtr);

		// transform from RGBA to BGRA like the DDS wants it (in place)
		for(uint32 x = 0; x < Width; ++x)
		{
			Swap(Line[x * 4 + 0], Line[x * 4 + 2]);
		}
	}
}

void FHDRLoadHelper::DecompressScanline(uint8* Out, const uint8*& In) const
{
	// minimum and maxmimum scanline length for encoding
	uint32 MINELEN = 8;
	uint32 MAXELEN = 0x7fff;

	uint32 Len = Width;

	if(Len < MINELEN || Len > MAXELEN)
	{
		OldDecompressScanline(Out, In, Len);
		return;
	}

	uint8 r = *In;

	if(r != 2)
	{
		OldDecompressScanline(Out, In, Len);
		return;
	}

	++In;

	uint8 g = *In++;
	uint8 b = *In++;
	uint8 e = *In++;

	if(g != 2 || b & 128)
	{
		*Out++ = r; *Out++ = g; *Out++ = b; *Out++ = e;
		OldDecompressScanline(Out, In, Len - 1);
		return;
	}

	for(uint32 Channel = 0; Channel < 4; ++Channel)
	{
		uint8* OutSingleChannel = Out + Channel;

		for(uint32 MultiRunIndex = 0; MultiRunIndex < Len;)
		{
			uint8 c = *In++;

			if(c > 128)
			{
				uint32 Count = c & 0x7f;

				c = *In++;

				for(uint32 RunIndex = 0; RunIndex < Count; ++RunIndex)
				{
					*OutSingleChannel = c;
					OutSingleChannel += 4;
				}			
				MultiRunIndex += Count;
			}
			else
			{
				uint32 Count = c;

				for(uint32 RunIndex = 0; RunIndex < Count; ++RunIndex)
				{
					*OutSingleChannel = *In++;
					OutSingleChannel += 4;
				}
				MultiRunIndex += Count;
			}
		}
	}
}

void FHDRLoadHelper::OldDecompressScanline(uint8* Out, const uint8*& In, uint32 Len)
{
	int32 Shift = 0;

	while(Len > 0)
	{
		uint8 r = *In++; 
		uint8 g = *In++;
		uint8 b = *In++; 
		uint8 e = *In++; 

		*Out++ = r; *Out++ = g; *Out++ = b; *Out++ = e;

		if(r == 1 && g ==1 && b == 1)
		{
			uint32 Count = ((int32)(e) << Shift);

			for(int32 i = Count; i > 0; --i)
			{
				*Out++ = r; *Out++ = g; *Out++ = b; *Out++ = e;
				--Len;
			}

			Shift += 8;
		}
		else
		{
			Shift = 0;
			Len--;
		}
	}
}

void FHDRLoadHelper::ExtractDDSInRGBE(TArray<uint8>& OutDDSFile) const
{
	// header, one uint32 per texel, no mips
	OutDDSFile.SetNumUninitialized(4 + sizeof(FDDSFileHeader) + GetWidth() * GetHeight() * sizeof(uint32));

	// create the dds header
	{
		uint32* DDSMagicNumber = (uint32*)OutDDSFile.GetData();

		// to identify DDS file format
		*DDSMagicNumber = 0x20534444;

		FDDSFileHeader* header = (FDDSFileHeader*)(OutDDSFile.GetData() + 4);

		FMemory::Memzero(*header);

		header->dwSize = sizeof(FDDSFileHeader);
		header->dwFlags = DDSF_Caps | DDSF_Height | DDSF_Width | DDSF_PixelFormat;
		header->dwWidth = GetWidth();
		header->dwHeight = GetHeight();
		header->dwCaps2 = 0;
		header->dwMipMapCount = 1;
		header->ddpf.dwSize = sizeof(FDDSPixelFormatHeader);
		header->ddpf.dwFlags = DDSPF_RGB;
		header->ddpf.dwRGBBitCount = 32;
		header->ddpf.dwRBitMask = 0x00ff0000;
		header->ddpf.dwGBitMask = 0x0000ff00;
		header->ddpf.dwBBitMask = 0x000000ff;
	}

	uint32 *DDSData = (uint32*)(OutDDSFile.GetData() + 4 + sizeof(FDDSFileHeader));

	// Get the raw input data as 2d image
	DecompressWholeImage(DDSData);
}
