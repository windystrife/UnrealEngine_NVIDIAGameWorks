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

#include "RSync.h"
#include "ConsoleRunner.h"
//---------------------------------------------------------------------------

#pragma package(smart_init)


//---------------------------------------------------------------------------
bool TRsync::IsError(AnsiString firstLine){
    if(firstLine.Pos("rsync:") > 0){
        lastError = firstLine;
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------
int TRsync::FetchModules(AnsiString server, TStrings* results){
    TConsoleRunner cRunner;

    SetCurrentDirectory(ExtractFilePath(Application->ExeName).c_str());
	AnsiString cmdLine = "rsync.exe " + server + "::";
    vector<string> list;
    int totalModules = 0;
    if(cRunner.Run(cmdLine, &list, NULL)){

        //Although it is quite possible that the list is already broken into
        //lines, that is not always the case. Therefore, I have to break this
        //into lines.
        TStringList* lines = new TStringList();
        AnsiString oneBigText;
        for(unsigned int i = 0; i < list.size(); i++){
            oneBigText += list.at(i).data();

        }

        lines->Text = oneBigText;
        for(int i = 0; i < lines->Count; i++){
            AnsiString oneLine = lines->Strings[i];
            if(i == 0 && IsError(oneLine)){
                return -1;
            }
            char* mName = strtok(oneLine.c_str(), " ");
            results->Add(mName);
            totalModules++;
        }
        delete lines;
    }

    return totalModules;
}

//---------------------------------------------------------------------------
AnsiString TRsync::FixPath(AnsiString original, bool source){
    //In case of source path I have to fix the Drive letter as well
	if(source){


		//If the original string contains a UNC formatted path, I leave the original string as-is.
		//Thanks to "Brendan Grieve" for letting me know about this problem.


		if(original.Length() > 2 && original[1] == '\\' && original[2] == '\\'){
			 //Don't do anything here.
		}else{
			AnsiString driveLetter = ExtractFileDrive(original);
			if(driveLetter != ""){
				original = "/cygdrive/" + AnsiString(driveLetter[1]) + "/" + original.SubString(4, original.Length());
			}
		}
    }else{
        //In case of target, I remove the file name
        original = ExtractFilePath(original);
    }


    AnsiString answer;

    for(int i = 1; i <= original.Length(); i++){
        if(original[i] == '\\'){
            original[i] = '/';
        }
    }

    return original;
}
//---------------------------------------------------------------------------
AnsiString TRsync::GetErrorReason(DWORD code){

    switch(code){
        case 0:
            return "Success";
        case 1:
            return "Syntax or usage error";
        case 2:
            return "Protocol incompatibility";
        case 3:
            return "Errors selecting input/output files, dirs";
        case 4:
            return "Requested action not supported: an attempt was made to manipulate 64-bit files on a platform that cannot support them; or an option was specified that is supported by the client and not by the server.";
        case 5:
            return "Error starting client-server protocol";
        case 6:
            return "Daemon unable to append to log-file";
        case 10:
            return "Error in socket I/O";
        case 11:
            return "Error in file I/O";
        case 12:
            return "Error in rsync protocol data stream";
        case 13:
            return "Errors with program diagnostics";
        case 14:
            return "Error in IPC code";
        case 20:
            return "Received SIGUSR1 or SIGINT";
        case 21:
            return "Some error returned by waitpid()";
        case 22:
            return "Error allocating core memory buffers";
        case 23:
            return "Partial transfer due to error";
        case 24:
            return "Partial transfer due to vanished source files";
        case 25:
            return "The --max-delete limit stopped deletions";
        case 30:
            return "Timeout in data send/receive";
        default:
            return "Generic error occurred";
    }
}
//---------------------------------------------------------------------------
int TRsync::Run(AnsiString server, AnsiString parameters, bool isSSH,
                AnsiString sourceFile,
                AnsiString moduleName,
				AnsiString targetDir,
				AnsiString userID,
				AnsiString password,
				vector<string>* list,
                HWND parentWindow,
                bool displayCommandOnly,
                bool restore){
    TConsoleRunner cRunner;

    SetCurrentDirectory(ExtractFilePath(Application->ExeName).c_str());

    sourceFile = FixPath(sourceFile, true);


    AnsiString cmdLine;

	AnsiString separator = "::";

	if(isSSH) separator = ":";


	if(userID != NULL && userID.Trim().Length() > 0){
		userID = userID.Trim() + "@";
	}else{
		userID = "";
	}


	if(restore){
		cmdLine = "rsync.exe " + parameters + " \"" + userID + server + separator + moduleName +
                         "/" + targetDir + "\" \"" + sourceFile + "\"";
    }else{
		cmdLine = "rsync.exe " + parameters + " \"" + sourceFile + "\" \"" +
						 userID + server + separator + moduleName + "/" + targetDir + "\"";
    }

	if(password.Length() > 0){
		SetEnvironmentVariable("RSYNC_PASSWORD", password.c_str());
	}


    if(parentWindow){
        String message = "Executing: " + cmdLine;
        list->push_back(message.c_str());
        SendMessage(parentWindow, MSG_TO_STDOUT, list->size() - 1, 0);
        Application->ProcessMessages();
    }

    if(displayCommandOnly){
        return 1;
    }

    if(cRunner.Run(cmdLine, list, parentWindow)){

		DWORD exitCode = cRunner.GetExitCode();

		if(exitCode > 0){
			AnsiString reason = GetErrorReason(exitCode);
			list->push_back(reason.c_str());
			if(parentWindow){
				SendMessage(parentWindow, MSG_TO_STDOUT, list->size() - 1, 0);
				Application->ProcessMessages();
			}
			return 0;
		}

    }

    return 1;
}
