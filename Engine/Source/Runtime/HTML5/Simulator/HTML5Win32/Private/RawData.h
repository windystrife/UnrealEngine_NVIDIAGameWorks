// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma  once 

struct  FSocketRawData
{
	SOCKET socket; 
};

struct FInternetAddrRawData
{
	sockaddr_in Addr;  
	FInternetAddrRawData() 
	{  
		memset( &Addr, 0, sizeof sockaddr_in); 
		Addr.sin_family = AF_INET;
	}
};


typedef __int32 SOCKLEN;
