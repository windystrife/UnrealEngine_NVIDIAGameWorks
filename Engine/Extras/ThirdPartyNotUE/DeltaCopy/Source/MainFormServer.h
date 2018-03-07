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

#ifndef MainFormServerH
#define MainFormServerH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <ComCtrls.hpp>
#include <ImgList.hpp>
#include <Graphics.hpp>
#include <Buttons.hpp>
#include "RSyncConfigAdapter.h"
#include <Menus.hpp>

#define NT_SERVICE_NAME "DeltaCopyService"
#define SECRET_DIR "secrets\\"
//---------------------------------------------------------------------------
class TfrmMainFormServer : public TForm
{
__published:	// IDE-managed Components
    TPanel *Panel1;
    TPanel *Panel2;
    TPageControl *pgMain;
    TTabSheet *tbStatus;
    TTabSheet *tbVirtualDir;
    TImage *imgMain;
    TButton *btnClose;
    TImage *imgStopped;
    TImage *imgStarting;
    TImage *imgRunning;
    TBitBtn *btnStart;
    TBitBtn *btnStop;
    TTimer *tmrMain;
    TPanel *Panel3;
    TLabel *Label1;
    TPanel *Panel4;
    TListBox *lstModules;
    TLabel *Label2;
    TPanel *Panel5;
    TGroupBox *GroupBox1;
    TLabel *lblPath;
    TEdit *txtPath;
    TButton *btnBrowse;
    TLabel *lblComment;
    TEdit *txtComment;
    TCheckBox *chkReadonly;
    TImage *Image1;
    TImageList *imgList;
    TBitBtn *btnInstallService;
    TLabel *lblInstallService;
    TPopupMenu *ppmMain;
    TMenuItem *RenameDirectory1;
    TMenuItem *AddNewDirectory1;
    TMenuItem *N1;
    TMenuItem *DeleteDirectory1;
    TMainMenu *mmnMain;
    TMenuItem *File1;
    TMenuItem *Exit1;
    TMenuItem *Help1;
    TMenuItem *DeltaCopyHelp1;
    TMenuItem *N2;
    TMenuItem *About1;
	TLabel *lblUID;
	TEdit *txtUID;
	TEdit *txtPWD;
	TLabel *lblPWD;
	TLabel *lblTIP;
	TCheckBox *chkUseAuth;
	TMenuItem *N3;
	TMenuItem *FixFilePermissions1;
    void __fastcall FormShow(TObject *Sender);
    void __fastcall btnStartClick(TObject *Sender);
    void __fastcall tmrMainTimer(TObject *Sender);
    void __fastcall btnStopClick(TObject *Sender);
    void __fastcall btnCloseClick(TObject *Sender);
    void __fastcall btnBrowseClick(TObject *Sender);
    void __fastcall lstModulesClick(TObject *Sender);
    void __fastcall txtCommentExit(TObject *Sender);
    void __fastcall chkReadonlyClick(TObject *Sender);
    void __fastcall lstModulesDblClick(TObject *Sender);
    void __fastcall lstModulesDrawItem(TWinControl *Control, int Index,
          TRect &Rect, TOwnerDrawState State);
    void __fastcall btnInstallServiceClick(TObject *Sender);
    void __fastcall FormCreate(TObject *Sender);
    void __fastcall AddNewDirectory1Click(TObject *Sender);
    void __fastcall RenameDirectory1Click(TObject *Sender);
    void __fastcall DeleteDirectory1Click(TObject *Sender);
    void __fastcall Exit1Click(TObject *Sender);
    void __fastcall DeltaCopyHelp1Click(TObject *Sender);
    void __fastcall About1Click(TObject *Sender);
	void __fastcall chkUseAuthClick(TObject *Sender);
	void __fastcall FixFilePermissions1Click(TObject *Sender);
private:	// User declarations

	TRsyncConfigAdapter adapter;

    void AddANewModule();
	void FillModuleValues();
	bool FixPermissions(AnsiString path);
    void InstallNTService();
    void LoadModuleNames();
    void SaveModuleValues();
    void UpdateServiceStatus();
    void UninstallNTService();
public:		// User declarations
    __fastcall TfrmMainFormServer(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TfrmMainFormServer *frmMainFormServer;
//---------------------------------------------------------------------------
#endif
