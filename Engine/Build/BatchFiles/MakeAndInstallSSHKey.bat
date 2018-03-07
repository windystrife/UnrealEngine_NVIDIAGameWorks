@echo off
setlocal

set SSH=%1
set SSHDIR=%~d1%~p1
set SSHPORT=%2
set RSYNC=%3
set USER=%4
set MACHINE=%5
set DOCUMENTS=%6
set CYGWIN_DOCUMENTS=%7
set ENGINE=%~8

set KEY_DIR=%DOCUMENTS%\Unreal Engine\UnrealBuildTool\SSHKeys\%MACHINE%\%USER%
set KEY_PATH=%DOCUMENTS%\Unreal Engine\UnrealBuildTool\SSHKeys\%MACHINE%\%USER%\RemoteToolChainPrivate.key
set CYGWIN_KEY_PATH=%CYGWIN_DOCUMENTS%/Unreal Engine/UnrealBuildTool/SSHKeys/%MACHINE%/%USER%/RemoteToolChainPrivate.key

echo %SSHDIR%

echo.
echo ================================================================================
echo Connecting to %MACHINE% as user %USER% to create an SSH Key.
echo.
echo Please note the following:
echo   It may prompt you about "authenticity of host can't be established." You must enter yes
echo   It will then prompt your for your password on %MACHINE%
echo   Finally it will ask your for a 'passphrase' for your new SSH key. Do NOT give it a passcode, just hit enter
echo   Hit enter again when prompted to confirm.
echo ================================================================================
echo.

pause

%SSH% -p %SSHPORT% %USER%@%MACHINE% "if [[ ! -e .ssh ]]; then mkdir .ssh; fi && cd .ssh && if [[ -e authorized_keys ]]; then cp -f authorized_keys authorized_keys_UEBackup; fi && ssh-keygen -t rsa -f RemoteToolChain && mv -f RemoteToolChain.pub RemoteToolChainPublic.key && mv -f RemoteToolChain RemoteToolChainPrivate.key && cat RemoteToolChainPublic.key >> authorized_keys";

echo.
echo ================================================================================
echo Now we will connect again to %MACHINE% to download the generated private key.
echo You will have to enter your password again.
echo The private key will be stored in:
echo    %KEY_PATH%
echo.
echo If you want to share this key with others on your team, copy the contents of SSHKeys directory to:
echo    %ENGINE%\Build\SSHKeys
echo and then put it into your source control.
echo ================================================================================
echo.

pause

mkdir "%KEY_DIR%" 2> NUL
pushd %SSHDIR%
%RSYNC% -az -e '%SSH% -p %SSHPORT%' %USER%@%MACHINE%:.ssh/RemoteToolChainPrivate.key "%CYGWIN_KEY_PATH%"
popd

echo.
echo ===================================================================================================
echo Assuming you have seen no errors above, you are ready to compile UE4 remotely on %MACHINE%!
echo ===================================================================================================
echo.

pause
