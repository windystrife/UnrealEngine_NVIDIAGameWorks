/*
   Copyright (C) Synametrics Technologies, Inc 2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
//---------------------------------------------------------------------------


#pragma hdrstop

#include "TProfile.h"
#include <iterator>
#include <algorithm>
#include <Forms.hpp>
#include "RSync.h"
#include "ConsoleRunner.h"
#include "Logger.h"
#include "GenUtils.h"
#include "DCConfig.h"
//---------------------------------------------------------------------------

#pragma package(smart_init)

#include "assert.h"


TProfile::TProfile() {
    Port = 873;
    ProfileName = "";
    ServerIP = "";
	ModuleName = "";
	UserName = "";
	Password = "";


    DeleteOlderFiles = true;
    Scheduled = false;
    UseRecursive = true;
    UseCompression = true;
	SkipNewerFiles = false;
	UseSSH = false;
	AdditionalParams = "";
	VerboseLogging = true;
	AssignPermissions = true;

}
bool TProfile::operator==(TProfile comparee) const{
    return ProfileName == comparee.ProfileName;
}
//---------------------------------------------------------------------------
void TProfile::AddFile(AnsiString s){
    FileList.push_back(s.c_str());
}
//---------------------------------------------------------------------------
AnsiString TProfile::BuildOptionString(bool restore){
	AnsiString results;


	if(VerboseLogging){
		results = " -v";
	}else{
		results = "";
	}
	if(UseRecursive){
		results += " -rlt";
	}
	if(UseCompression){
		results += " -z";
	}

    if(AssignPermissions){
        results += " --chmod=a=rw,Da+x";
    }

    if(SkipNewerFiles){
        results += " -u";
    }

    if(DeleteOlderFiles && !restore){
        results += " --delete";
    }
    if(AdditionalParams.Length() > 0){
        results += " " + AdditionalParams;
    }

    return results;

}
//---------------------------------------------------------------------------
void TProfile::ClearFiles(){
    FileList.clear();
}
//---------------------------------------------------------------------------
//Takes a datetime and returns number of milliseconds since midnight
long TProfile::DateTimeToMillis(TDateTime dt){
    unsigned short hour;
    unsigned short minute;
    unsigned short sec;
    unsigned short ms;

    dt.DecodeTime(&hour, &minute, &sec, &ms);

    long answer = (hour * 24 * 60 * 1000) +
                  (minute * 60 * 1000) +
                  (sec * 1000) +
                  ms;

    return answer;


}
//---------------------------------------------------------------------------
AnsiString TProfile::GetKey(){
    char* pName = ProfileName.c_str();

    AnsiString result;
    for(int i = 0; i < ProfileName.Length(); i++){
        if(isalnum(pName[i])){
            result += pName[i];
        }
    }
    return result;
}

//---------------------------------------------------------------------------
AnsiString TProfile::GetTargetFolder(AnsiString fileName){
    int pos = 0;
    int totalSeparators = 0;
    int i;


    //First check if this is a folder name
    bool isFolder = DirectoryExists(fileName);

    for(i = fileName.Length(); i > 0; i--){
        if(fileName[i] == '\\'){
            if(totalSeparators == 0){
                totalSeparators = 1;
            }else{
                pos = i;
                break;
            }
        }
    }

    AnsiString result = fileName;
    if(pos > 0){
        result = fileName.SubString(pos + 1, fileName.Length());
        result = GenericUtils::ConvertPathWindowsToCygwin(result);
        return result;
    }


    if(totalSeparators == 1){ //This is off of route

        if(isFolder){
            result = fileName.SubString(4, fileName.Length());
        }else{
            result = ExtractFileName(fileName);
        }
    }

    result = GenericUtils::ConvertPathWindowsToCygwin(result);
    return result;
}
//---------------------------------------------------------------------------
void TProfile::Read(TIniFile* source, AnsiString pName/* Profile Name */){
    TStringList* items = new TStringList();
    source->ReadSection(pName, items);

    ProfileName = pName;
    ServerIP = source->ReadString(pName, "ServerIP", "");
    ModuleName = source->ReadString(pName, "ModuleName", "");
    Port = source->ReadInteger(pName, "Port", 873);
    UseRecursive = source->ReadBool(pName, "Recursive", true);
    UseCompression = source->ReadBool(pName, "Compression", true);
    DeleteOlderFiles = source->ReadBool(pName, "DeleteOlderFiles", true);
    SkipNewerFiles = source->ReadBool(pName, "SkipNewerFiles", false);
	AdditionalParams = source->ReadString(pName, "AdditionalParams", "");
	UseSSH = source->ReadBool(pName, "UseSSH", false);
	VerboseLogging = source->ReadBool(pName, "VerboseLogging", true);
	AssignPermissions = source->ReadBool(pName, "AssignPermissions", true);

	UserName = source->ReadString(pName, "UserID", "");
	Password = source->ReadString(pName, "Password", "");

	for(int i = 0; i < items->Count; i++){
        String aKey = items->Strings[i];
        if(aKey.Pos("File_") == 1){
            FileList.push_back(source->ReadString(pName, aKey, "").c_str());
        }
    }

    delete items;
}

//---------------------------------------------------------------------------
//This method runs the RSYNC program
bool TProfile::Run(HWND parent, AnsiString logFile, vector<string>* logData,
            bool displayCommandOnly, bool restore, int* keepRunning){
    TRsync rsync;

    TStringList* results = new TStringList();

    TDCConfig config(false);
    int retryCount = StrToIntDef(config.GetRetryCount(), 5);

    theLogger->Log("Running profile " + ProfileName);
    long startTime = DateTimeToMillis(TDateTime::CurrentDateTime());
    bool runResult = true;
    for(unsigned int i = 0; i < FileList.size(); i++){
        if(*keepRunning == 0){
            continue;
        }
        AnsiString sourcePlusTargetFile(FileList.at(i).data());

        AnsiString sourceFile = StripSource(sourcePlusTargetFile);
        AnsiString targetFile = StripTarget(sourcePlusTargetFile);

        if(restore && !FileExists(sourceFile) && !DirectoryExists(sourceFile)){
            logData->push_back(("Error: File does not exist " + sourceFile + "\r\n").c_str());
            if(parent){
                SendMessage(parent, MSG_TO_STDOUT, logData->size() - 1, 0);
                Application->ProcessMessages();
            }
            runResult = false;
        }else{

            for(int i = 0; i < retryCount; i++){
                if(rsync.Run(ServerIP, BuildOptionString(restore), UseSSH,
                            sourceFile,
                            ModuleName,
							targetFile,
							UserName,
							Password,
                            logData,
                            parent,
                            displayCommandOnly, restore)){

                    break;
                }

                if(i == retryCount - 1){
                    runResult = false;
                    break;
                }

                AnsiString msg = "Rsync.exe returned an error. Will try again. This is retry number " +
                    IntToStr(i + 1) + " of " + IntToStr(retryCount) + "\r\n";

                logData->push_back(msg.c_str());
                if(parent){
                    SendMessage(parent, MSG_TO_STDOUT, logData->size() - 1, 0);
                    Application->ProcessMessages();
                }

            }
            
        }
    }

    if(FileList.size() == 0){
        if(parent){

        }
        logData->push_back("There is nothing to backup.\r\n");
        if(parent){
            SendMessage(parent, MSG_TO_STDOUT, logData->size() - 1, 0);
            Application->ProcessMessages();
        }
    }

    long endTime = DateTimeToMillis(TDateTime::CurrentDateTime());

    AnsiString finalMsg = "Profile '" + ProfileName + "' executed in " +
        IntToStr((__int64 )endTime - startTime) + " milliseconds. " +
        (runResult ? "It ran successfully." : "One or more errors were encountered.");

    logData->push_back(finalMsg.c_str());
    if(parent){
        SendMessage(parent, MSG_TO_STDOUT, logData->size() - 1, 0);
        Application->ProcessMessages();
    }
    
    delete results;
    return runResult;
}
//---------------------------------------------------------------------------

void TProfile::Save(TIniFile* target){


    target->WriteString(ProfileName, "ServerIP", ServerIP);
    target->WriteString(ProfileName, "ModuleName", ModuleName);
    target->WriteInteger(ProfileName, "Port", Port);

	target->WriteString(ProfileName, "AdditionalParams", AdditionalParams);

    target->WriteBool(ProfileName, "Compression", UseCompression);
    target->WriteBool(ProfileName, "DeleteOlderFiles", DeleteOlderFiles);
    target->WriteBool(ProfileName, "SkipNewerFiles", SkipNewerFiles);
	target->WriteBool(ProfileName, "Recursive", UseRecursive);
	target->WriteBool(ProfileName, "UseSSH", UseSSH);
    target->WriteBool(ProfileName, "VerboseLogging", VerboseLogging);
	target->WriteBool(ProfileName, "AssignPermissions", AssignPermissions);
	target->WriteString(ProfileName, "UserID", UserName);
	target->WriteString(ProfileName, "Password", Password);


    for(unsigned int i = 0; i < FileList.size(); i++){
        target->WriteString(ProfileName, "File_" + IntToStr(i + 1), FileList.at(i).data());
    }
}
//---------------------------------------------------------------------------
AnsiString TProfile::StripSource(AnsiString input){
    int pos = input.Pos(SOURCE_TARGET_DELIMITER);

    if(pos > 0){
        return input.SubString(1, pos - 1);
    }

    return input;
}
//---------------------------------------------------------------------------
AnsiString TProfile::StripTarget(AnsiString input){
    int pos = input.Pos(SOURCE_TARGET_DELIMITER);

    if(pos > 0){
        return input.SubString(pos + AnsiString(SOURCE_TARGET_DELIMITER).Length(), input.Length());
    }

    return GetTargetFolder(input);
}

//---------------------------------------------------------------------------














//---------------------------------------------------------------------------
TProfileManager::TProfileManager(){
    
    ReadAllProfiles();
}
//---------------------------------------------------------------------------
TProfileManager::~TProfileManager(){
    //ToDO: Have to clean memory.
    //It is ok because this destructor will only get called when the program is
    //exiting. Therefore, I am skipping clean up here.
}
//---------------------------------------------------------------------------
void TProfileManager::AddProfile(TProfile* aProfile){
    Profiles.push_back(aProfile);
}


//---------------------------------------------------------------------------
AnsiString TProfileManager::GetIniFileName(){
    return ExtractFilePath(Application->ExeName) + PROFILE_FILE_NAME;
}
//---------------------------------------------------------------------------

TProfile* TProfileManager::GetProfile(int i){
    return Profiles.at(i);
}
//---------------------------------------------------------------------------

TProfile* TProfileManager::GetProfile(AnsiString profileName){
    for(unsigned int i = 0; i < Profiles.size(); i++){
        TProfile* aProfile = Profiles.at(i);
        AnsiString pName = aProfile->GetProfileName();
        if(pName == profileName){
            return aProfile;
        }
    }

    return NULL;
}
//---------------------------------------------------------------------------

TProfile* TProfileManager::GetProfileByKey(AnsiString key){
    for(unsigned int i = 0; i < Profiles.size(); i++){
        TProfile* aProfile = Profiles.at(i);
        AnsiString pName = aProfile->GetKey();
        if(pName == key){
            return aProfile;
        }
    }

    return NULL;
}
//---------------------------------------------------------------------------
int TProfileManager::GetProfileIndex(AnsiString profileName){

    for(unsigned int i = 0; i < Profiles.size(); i++){
        TProfile* aProfile = Profiles.at(i);
        AnsiString pName = aProfile->GetProfileName();
        if(pName == profileName){
            return (int)i;
        }
    }
    return -1;
}
//---------------------------------------------------------------------------
void TProfileManager::ReadAllProfiles(){
	TIniFile* ini = new TIniFile(GetIniFileName());
    TStringList* profileNames = new TStringList();

    ini->ReadSections(profileNames);

    for(int i = 0; i < profileNames->Count; i++){
        TProfile* aProfile = new TProfile();
        aProfile->Read(ini, profileNames->Strings[i]);
        Profiles.push_back(aProfile);
    }

    delete profileNames;
    delete ini;
}
//---------------------------------------------------------------------------
void TProfileManager::RemoveProfile(TProfile* aProfile){

    //This means a match is found
    vector<TProfile*>::iterator candidate = find(Profiles.begin(), Profiles.end(), aProfile);
	Profiles.erase(candidate);
	delete aProfile;



}
//---------------------------------------------------------------------------

void TProfileManager::SaveProfiles(){

    if(FileExists(GetIniFileName())){
        DeleteFile(GetIniFileName());
    }
    TIniFile* ini = new TIniFile(GetIniFileName());

    for(unsigned int i = 0; i < Profiles.size(); i++){
        TProfile* aProfile = Profiles.at(i);
        aProfile->Save(ini);
    }

    delete ini;
}

