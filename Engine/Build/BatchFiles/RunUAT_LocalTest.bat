@echo off

pushd .

if not exist LocalP4Settings_editme.bat (
	echo failed: this batchfile must be run from Engine\Build\BatchFiles
	goto fail
)

call LocalP4Settings_editme.bat

echo *************** P4 settings
echo uebp_PORT=%uebp_PORT%
echo uebp_USER=%uebp_USER%
echo uebp_CLIENT=%uebp_CLIENT%
echo uebp_PASS=[redacted]
echo P4PORT=%P4PORT%
echo P4USER=%P4USER%
echo P4CLIENT=%P4CLIENT%
echo P4PASSWD=[redacted]

echo *************** builder settings
echo uebp_CL=%uebp_CL%
echo uebp_LOCAL_ROOT=%uebp_LOCAL_ROOT%
echo uebp_BuildRoot_P4=%uebp_BuildRoot_P4%
echo uebp_BuildRoot_Escaped=%uebp_BuildRoot_Escaped%
echo uebp_CLIENT_ROOT=%uebp_CLIENT_ROOT%

cd /D %uebp_LOCAL_ROOT%\Engine\Build\BatchFiles

call RunUAT.bat %*

if errorlevel 1 (
	echo failed: RunUAT.bat %*
	goto fail
)

popd
exit /B 0
:fail
echo BUILD FAILED
popd
exit /B 1