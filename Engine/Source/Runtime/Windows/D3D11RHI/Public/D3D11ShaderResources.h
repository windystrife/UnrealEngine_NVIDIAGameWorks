// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Resources.h: D3D resource RHI definitions.
=============================================================================*/

#pragma once

struct FD3D11ShaderResourceTable : public FBaseShaderResourceTable
{
	/** Mapping of bound Textures to their location in resource tables. */
	TArray<uint32> TextureMap;

	friend bool operator==(const FD3D11ShaderResourceTable& A, const FD3D11ShaderResourceTable& B)
	{
		const FBaseShaderResourceTable& BaseA = A;
		const FBaseShaderResourceTable& BaseB = B;
		return BaseA == BaseB && (FMemory::Memcmp(A.TextureMap.GetData(), B.TextureMap.GetData(), A.TextureMap.GetTypeSize()*A.TextureMap.Num()) == 0);
	}
};

inline FArchive& operator<<(FArchive& Ar, FD3D11ShaderResourceTable& SRT)
{
	FBaseShaderResourceTable& BaseSRT = SRT;
	Ar << BaseSRT;
	Ar << SRT.TextureMap;
	return Ar;
}
