// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma  once 

// A real simple blocking curl implementation specifically for Network File System - used just for HTML5Win32 test builds

namespace HTML5Win32 
{
	namespace NFSHttp
	{
		void Init(char* URL); 
		bool SendPayLoadAndRecieve(const unsigned char* In, unsigned int Size, unsigned char** Out, unsigned int& size); 
	};
}
