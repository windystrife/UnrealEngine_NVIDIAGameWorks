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

#include "GenUtils.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)


//---------------------------------------------------------------------------
AnsiString GenericUtils::BrowseForFolder(AnsiString title, HWND ownerHandle){

    BROWSEINFO    info;
    char          szDir[MAX_PATH];
    char          szDisplayName[MAX_PATH];
    LPITEMIDLIST  pidl;
    LPMALLOC      pShellMalloc;
    AnsiString retVal = "";
    // SHBrowseForFolder returns a PIDL. The memory for the PIDL is
    // allocated by the shell. Eventually, we will need to free this
    // memory, so we need to get a pointer to the shell malloc COM
    // object that will free the PIDL later on.
    if(SHGetMalloc(&pShellMalloc) == NO_ERROR)    {
        // if we were able to get the shell malloc object,
        // then proceed by initializing the BROWSEINFO stuct
        memset(&info, 0x00,sizeof(info));
        info.hwndOwner = ownerHandle;                   // Owner window
        info.pidlRoot  = 0;                      // root folder
        info.pszDisplayName = szDisplayName;     // return display name
        info.lpszTitle = title.c_str();         // label caption
        info.ulFlags   = BIF_RETURNONLYFSDIRS;   // config flags
        info.lpfn = 0;                           // callback function
        // execute the browsing dialog
        pidl = SHBrowseForFolder(&info);
        // pidl will be null if they cancel the browse dialog.
        // pidl will be not null when they select a folder

        if(pidl){            // try to convert the pidl to a display string
            // return is true if success
            if(SHGetPathFromIDList(pidl, szDir))            {
                // set one caption to the directory path
                retVal = szDir;
            }

            // set another caption based on the display name

            //Label2->Caption = info.pszDisplayName;

            // use the shell malloc com object to free the pidl.
            // then call Relasee to signal that we don't need
            // the shell malloc object anymore
            pShellMalloc->Free(pidl);
        }

            pShellMalloc->Release();
    }

    return retVal;
}

//---------------------------------------------------------------------------

AnsiString GenericUtils::ConvertPathCygwinToWindows(AnsiString original){
    //In case of source path I have to fix the Drive letter as well

    if(original.Length() > 11 && original.Pos("/cygdrive/") == 1){
        AnsiString driveLetter = original[11];
        original = driveLetter + ":\\" + original.SubString(13, original.Length());
    }

    AnsiString answer;
    //This loop fixes the spaces
    for(int i = 1; i <= original.Length(); i++){
        if(i != original.Length() && original[i] == '\\' && original[i + 1] == ' '){
            answer += ' ';
            i++;      //Skip the next character since it is a space
        }else{
            answer += original[i];
        }
    }

    //Now fix the back slashes
    for(int i = 1; i <= answer.Length(); i++){
        if(answer[i] == '/'){
            answer[i] = '\\';
        }
    }
    return answer;
}
//---------------------------------------------------------------------------

AnsiString GenericUtils::ConvertPathWindowsToCygwin(AnsiString original){
    //In case of source path I have to fix the Drive letter as well
    AnsiString driveLetter = ExtractFileDrive(original);
    if(driveLetter != ""){
        original = "/cygdrive/" + AnsiString(driveLetter[1]).LowerCase() + "/" + original.SubString(4, original.Length());
    }

    AnsiString answer;

    for(int i = 1; i <= original.Length(); i++){
        if(original[i] == '\\'){
            original[i] = '/';
        }
    }

    return original;
    /*
    for(int i = 1; i <= original.Length(); i++){
        if(original[i] == ' '){
            answer += "\\ ";
        }else{
            answer += original[i];
        }
    }
    return answer;
    */
}
//---------------------------------------------------------------------------
