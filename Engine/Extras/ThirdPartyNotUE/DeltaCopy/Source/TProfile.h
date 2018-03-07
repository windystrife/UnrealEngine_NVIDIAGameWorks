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

#ifndef TProfileH
#define TProfileH
//---------------------------------------------------------------------------

#include <vector.h>
#include <string>

#include <Classes.hpp>
#include "IniFiles.hpp"

#define PROFILE_FILE_NAME "Profiles.ini"
#define SOURCE_TARGET_DELIMITER "@%@%@%"

class TProfile{
private:
    AnsiString ProfileName;
    AnsiString ServerIP;
	AnsiString ModuleName;
	AnsiString UserName;
	AnsiString Password;


    int Port;
    vector<string> FileList;

    bool DeleteOlderFiles;
    bool Scheduled;
    bool UseRecursive;
    bool UseCompression;
	bool SkipNewerFiles;
	bool UseSSH;
	bool VerboseLogging;
	bool AssignPermissions;

    AnsiString AdditionalParams;

    long DateTimeToMillis(TDateTime dt);
public:

    TProfile();

    bool operator==(TProfile comparee) const;
    void AddFile(AnsiString s);
    void ClearFiles();
    vector<string> GetFileList() { return FileList;}

    AnsiString BuildOptionString(bool restore);
    AnsiString GetAdditionalParams(){ return AdditionalParams;}
    AnsiString GetModuleName() { return ModuleName;}
	AnsiString GetKey();
	AnsiString GetPassword() { return Password;}
    AnsiString GetProfileName(){ return ProfileName;}
    AnsiString GetServerIP() { return ServerIP;}
    static AnsiString GetTargetFolder(AnsiString);
	AnsiString GetTaskName() { return GetKey() + ".job";}
	AnsiString GetUserName() { return UserName;}


	bool IsDeleteOlderFiles(){ return DeleteOlderFiles;}
	bool IsScheduled(){ return Scheduled;}
	bool IsUseRecursive() { return UseRecursive;}
	bool IsUseCompression() { return UseCompression;}
	bool IsSkipNewerFiles() { return SkipNewerFiles;}
	bool IsUseSSH() { return UseSSH;}
	bool IsVerboseLogging() { return VerboseLogging;}
	bool IsAssignPermissions(){return AssignPermissions;}

    
    void Read(TIniFile*, AnsiString /* Profile Name */);
    bool Run(HWND parent, AnsiString logFile, vector<string>* logData, bool, bool, int*);
    void Save(TIniFile* target);
    void SetAdditionalParams(AnsiString s){ AdditionalParams = s;}
    void SetDeleteOlderFiles(bool b) { DeleteOlderFiles = b;}
	void SetModuleName(AnsiString s) { ModuleName = s;}
	void SetPassword(AnsiString s) { Password = s;}
    void SetProfileName(AnsiString s){ ProfileName = s;}
    void SetServerIP(AnsiString s) { ServerIP = s;}
    void SetScheduled(bool b){ Scheduled = b;}
    void SetUseRecursive(bool b){ UseRecursive = b;}
    void SetUseCompression(bool b) {UseCompression = b;}
	void SetSkipNewerFiles(bool b) {SkipNewerFiles = b;}
	void SetUserName(AnsiString s) {UserName = s;}
	void SetUseSSH(bool b) {UseSSH = b;}
	void SetVerboseLogging(bool b){VerboseLogging = b;}
	void SetAssignPermissions(bool b) {AssignPermissions = b;}



    
    static AnsiString StripSource(AnsiString);
    static AnsiString StripTarget(AnsiString);

};


//---------------------------------------------------------------------------


class TProfileManager{
    vector<TProfile*> Profiles;

    AnsiString GetIniFileName();
public:
    TProfileManager();
    ~TProfileManager();
    void AddProfile(TProfile*);
    TProfile* GetProfile(int i);
    TProfile* GetProfile(AnsiString profileName);
    TProfile* GetProfileByKey(AnsiString key);
    int GetProfileIndex(AnsiString profileName);
    unsigned int GetProfileCount() { return Profiles.size();}
    void ReadAllProfiles();
    void RemoveProfile(TProfile*);
    void SaveProfiles();


};

#endif
