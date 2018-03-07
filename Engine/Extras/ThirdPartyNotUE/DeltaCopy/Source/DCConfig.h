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

#ifndef DCConfigH
#define DCConfigH
//---------------------------------------------------------------------------

#include <Classes.hpp>
#include <Forms.hpp>
#include "IniFiles.hpp"
#include "Registry.hpp"


#define INI_SECTION "DCConfig"

class TDCConfig{

    AnsiString appPath;
    AnsiString smtpServer;
    AnsiString recipients;
    AnsiString sendersEmail;
	AnsiString retryCount;
	AnsiString smtpUser;
	AnsiString smtpPass;

    bool notifyOnSuccess;
	bool notifyOnFailure;
	bool saveLogToDisk;

    bool autoSave;
    TRegistry *theRegistry;

    void Read();
    AnsiString ReadFromRegistry(AnsiString  token , AnsiString DefaultVal);
    void WriteToRegistry(AnsiString token, AnsiString value);
public:


    TDCConfig();
    TDCConfig(bool);
    ~TDCConfig();
    bool IsNotifyOnSuccess(){return notifyOnSuccess;}
	bool IsNotifyOnFailure(){return notifyOnFailure;}
	bool IsSaveLogToDisk(){return saveLogToDisk;}
    AnsiString GetAppPath() {return appPath;}
	AnsiString GetSendersEmail(){return sendersEmail;}
	AnsiString GetSmtpPass(){return smtpPass;}
	AnsiString GetSmtpServer(){return smtpServer;}
	AnsiString GetSmtpUser(){return smtpUser;}
    AnsiString GetRecipients(){return recipients;}
    AnsiString GetRetryCount(){return retryCount;}
    void InitRegistry();
    
    void Save();

    void SetAppPath(AnsiString s) {appPath = s;}
    void SetNofityOnSuccess(bool b){notifyOnSuccess = b;}
	void SetNotifyOnFailure(bool b){notifyOnFailure = b;}
	void SetRetryCount(AnsiString s){retryCount = s;}
	void SetSaveLogToDisk(bool b){saveLogToDisk = b;}
	void SetSendersEmail(AnsiString s){sendersEmail = s;}
	void SetSmtpPass(AnsiString s){smtpPass = s;}
	void SetSmtpServer(AnsiString s){smtpServer = s;}
	void SetSmtpUser(AnsiString s){smtpUser = s;}
    void SetRecipients(AnsiString s){recipients = s;}

};
#endif
