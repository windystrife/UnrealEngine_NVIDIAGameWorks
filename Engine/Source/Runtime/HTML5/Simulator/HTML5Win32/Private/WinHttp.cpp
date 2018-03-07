// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WinHttp.h"
#include "curl/curl.h"

static CURL *curl;


struct MemoryStruct {
	unsigned char *memory;
	size_t size;
};


static size_t
	WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	mem->memory = (unsigned char*)realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL) {
		/* out of memory! */ 
		printf("not enough memory (realloc returned NULL)\n");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

static char GURL[1024];
void HTML5Win32::NFSHttp::Init(char* URL)
{
	 strcpy_s(GURL,URL); 
	 curl_global_init(CURL_GLOBAL_ALL); 
	 curl = curl_easy_init();
}


bool HTML5Win32::NFSHttp::SendPayLoadAndRecieve(const unsigned char* In, unsigned int InSize, unsigned char** Out, unsigned int& size)
{
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:"; 

	curl_easy_setopt(curl, CURLOPT_URL, GURL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	MemoryStruct chunk;
	chunk.memory = 0;
	chunk.size = 0; 

	if (  InSize != 0 )
	{
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, In);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, InSize);
		
	}
	else
	{
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	}

	headerlist = curl_slist_append(headerlist, buf);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	CURLcode  result = curl_easy_perform(curl);

	/* check for errors */ 
	if(result != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(result));
		return false; 
	}

	*Out = chunk.memory; 
	size= chunk.size; 

	curl_easy_reset(curl);

	return true;
}
