//---------------------------------------------------------------------------
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

#pragma hdrstop

#include "DCConfig.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)

TDCConfig::TDCConfig(){
    autoSave = true;
    InitRegistry();
    Read();
}
//---------------------------------------------------------------------------
TDCConfig::TDCConfig(bool aSave){
    autoSave = aSave;
    InitRegistry();
    Read();
}

//---------------------------------------------------------------------------
TDCConfig::~TDCConfig(){
    if(autoSave){
        Save();
    }
}
//---------------------------------------------------------------------------
void TDCConfig::InitRegistry(){
	theRegistry = new TRegistry();

    if(FileExists(ExtractFilePath(Application->ExeName) + "UserConf.dat")){
		theRegistry->RootKey = HKEY_CURRENT_USER;
	}else{
		theRegistry->RootKey = HKEY_LOCAL_MACHINE;
    }
	theRegistry->OpenKey("SOFTWARE\\Synametrics\\DeltaCopy", true);
}
//---------------------------------------------------------------------------
void TDCConfig::Read(){

    appPath = ReadFromRegistry("AppPath", "");
    smtpServer = ReadFromRegistry("SmtpServer", "");
    recipients = ReadFromRegistry("Recipients", "");
	retryCount = ReadFromRegistry("RetryCount", "5");
	smtpUser = ReadFromRegistry("SmtpUser", "");
	smtpPass = ReadFromRegistry("SmtpPass", "");
    sendersEmail = ReadFromRegistry("SendersEmail", "notification@yourcompany.com");
    notifyOnSuccess = ReadFromRegistry("NotifyOnSuccess", "0") == "1";
	notifyOnFailure = ReadFromRegistry("NotifyOnFailure", "0") == "1";
	saveLogToDisk = ReadFromRegistry("SaveLogToDisk", "0") == "1";

}
//---------------------------------------------------------------------------
AnsiString TDCConfig::ReadFromRegistry(AnsiString  token , AnsiString DefaultVal){
//Registry = new TRegistry;
AnsiString retVal;
    try{
        retVal = theRegistry->ReadString(token);
        if (retVal == "") retVal = DefaultVal;
    }
    catch(...){
       //If no value is set the return value is defaultVal.
       retVal = DefaultVal;
    }

    return retVal;
}
//---------------------------------------------------------------------------
void TDCConfig::Save(){


    WriteToRegistry("AppPath", appPath);
	WriteToRegistry("SendersEmail", sendersEmail);
	WriteToRegistry("SmtpPass", smtpPass);
	WriteToRegistry("SmtpServer", smtpServer);
	WriteToRegistry("SmtpUser", smtpUser);
    WriteToRegistry("Recipients", recipients);
    WriteToRegistry("RetryCount", retryCount);
    WriteToRegistry("NotifyOnSuccess", notifyOnSuccess ? "1" : "0");
	WriteToRegistry("NotifyOnFailure", notifyOnFailure ? "1" : "0");
	WriteToRegistry("SaveLogToDisk", saveLogToDisk ? "1" : "0");

}
//---------------------------------------------------------------------------
void TDCConfig::WriteToRegistry(AnsiString token, AnsiString value){
//Registry = new TRegistry;

    try{
        theRegistry->WriteString(token, value);
    }
    catch(...){

    }
}

