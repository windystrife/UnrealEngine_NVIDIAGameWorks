@echo off
call "../../../scripts/locate_google_test.bat" GOOGLETEST
call "../../../scripts/locate_nvcloth.bat" NVCLOTH_PATH
set USE_CUDA=true
if %USE_CUDA% == "true" (
call "../../../scripts/locate_cuda.bat" CUDA_PATH
)
call "../../../scripts/locate_px_shared.bat" PX_SHARED_PATH
call "../../../scripts/locate_win8sdk.bat" WIN_8_SDK
call "../../../scripts/locate_xpj.bat" XPJ
rem set GOOGLETEST=..\..\..\..\..\..\..\physx\externals\GoogleTest\gtest-1.4.0
"%XPJ%\xpj4.exe" -t vc12 -p win64 -x "%CD%\NvClothUnitTests.xpj"
"%XPJ%\xpj4.exe" -t vc12 -p win32 -x "%CD%\NvClothUnitTests.xpj"
"%XPJ%\xpj4.exe" -t vc11 -p win64 -x "%CD%\NvClothUnitTests.xpj"
"%XPJ%\xpj4.exe" -t vc11 -p win32 -x "%CD%\NvClothUnitTests.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p win64 -x "%CD%\NvClothUnitTests.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p win32 -x "%CD%\NvClothUnitTests.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p xboxone -x "%CD%\NvClothUnitTests.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p ps4 -x "%CD%\NvClothUnitTests.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p switch -x "%CD%\NvClothUnitTests.xpj"

call "../GetFoundationDLLs.bat"

xcopy ..\..\src\xbone\data\* ..\vc14xboxone\Durango\NvClothUnitTests\Layout\Image\Loose\ /D /C /I /R
xcopy ..\..\src\xbone\data\* ..\vc14xboxone\Durango\NvClothPerfTests\Layout\Image\Loose\ /D /C /I /R

pause