// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SimplygonSwarmCommon.h"
#include <algorithm>

struct FSimplygonSSFHelper
{
public:

	/**
	* Return a new GUID as SsfString
	*/
	static ssf::ssfString SSFNewGuid()
	{
		return TCHARToSSFString(*FGuid::NewGuid().ToString());
	}

	/**
	* Return a empty GUID as SsfString
	*/
	static ssf::ssfString SFFEmptyGuid()
	{
		return TCHARToSSFString(*FGuid::FGuid().ToString());
	}

	/**
	* Convert TCHAR to SsfString
	*/
	static ssf::ssfString TCHARToSSFString(const TCHAR* str)
	{
		return ssf::ssfString(std::basic_string<TCHAR>(str));
	}

	/**
	* Compare two ssf strings
	*/
	static bool CompareSSFStr(ssf::ssfString lhs, ssf::ssfString rhs)
	{
		if (lhs.Value == rhs.Value)
			return true;

		return false;
	}

	/**
	* Get TextureCoordinate from SsfTexcoordinateList based on TextureSetName
	* @param TextureCoordsList		List of TextureCoorniates
	* @result a ssf::ssfNamedList<ssf::ssfVector2> object containing uv data nullptr otherwise
	*/
	static ssf::ssfNamedList<ssf::ssfVector2> FindByTextureSetName(std::list < ssf::ssfNamedList<ssf::ssfVector2> > TextureCoordsList, ssf::ssfString Name)
	{
		auto UVSet = std::find_if(TextureCoordsList.begin(), TextureCoordsList.end(),
			[Name](const ssf::ssfNamedList<ssf::ssfVector2> InTextureSet)
		{
			if (FSimplygonSSFHelper::CompareSSFStr(Name, InTextureSet.Name))
			{
				return true;
			}
			return false;
		});

		if (UVSet != std::end(TextureCoordsList))
		{
			return *UVSet;
		}

		return  ssf::ssfNamedList < ssf::ssfVector2>();
	}

	/**
	* Get SsfTexture by Guid
	* @param SsfScene		SsfScene
	* @param TextureId		Texture Guid String
	* @result a ssf::pssfTexture object nullptr otherwise
	*/
	static ssf::pssfTexture FindTextureById(ssf::pssfScene SsfScene, ssf::ssfString TextureId)
	{
		auto Texture = std::find_if(SsfScene->TextureTable->TextureList.begin(), SsfScene->TextureTable->TextureList.end(),
			[TextureId](const ssf::pssfTexture tex)
		{
			if (FSimplygonSSFHelper::CompareSSFStr(tex->Id.Get().Value, TextureId))
			{
				return true;
			}
			return false;
		});


		if (Texture != std::end(SsfScene->TextureTable->TextureList))
		{
			if (!Texture->IsNull())
			{
				return *Texture;
			}
		}

		return nullptr;
	}

	/**
	* Get TextureCoordinate from SsfTexcoordinateList based on TextureSetName
	* @param SsfScene		Input SsfScene
	* @param MaterailId	Input SsfScene
	* @result a ssf::pssfMaterial object nullptr otherwise
	*/
	static ssf::pssfMaterial FindMaterialById(ssf::pssfScene SsfScene, ssf::ssfString MaterailId)
	{
		auto ProxyMaterial = std::find_if(SsfScene->MaterialTable->MaterialList.begin(), SsfScene->MaterialTable->MaterialList.end(),
			[MaterailId](const ssf::pssfMaterial mat)
		{
			if (FSimplygonSSFHelper::CompareSSFStr(mat->Id.Get().Value, MaterailId))
			{
				return true;
			}
			return false;
		});

		if (ProxyMaterial != std::end(SsfScene->MaterialTable->MaterialList))
		{
			if (!ProxyMaterial->IsNull())
			{
				return *ProxyMaterial;
			}
		}

		return nullptr;
	}

	/**
	* Get Relavant Set for Baked Materials from the SsfTexcoordsList.
	* @param TextureCoordsList		List of TextureCoorniates
	* @result a ssf::ssfNamedList<ssf::ssfVector2> object containing uv data nullptr otherwise
	*/
	static ssf::ssfNamedList<ssf::ssfVector2> GetBakedMaterialUVs(std::list < ssf::ssfNamedList<ssf::ssfVector2> > TextureCoordsList)
	{
		return FindByTextureSetName(TextureCoordsList, ssf::ssfString("MaterialLOD"));
	}

};