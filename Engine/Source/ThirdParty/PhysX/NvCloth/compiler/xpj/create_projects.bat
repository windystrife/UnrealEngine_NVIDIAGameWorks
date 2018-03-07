@echo off
set USE_CUDA=true
if %USE_CUDA% == "true" (
call "../../scripts/locate_cuda.bat" CUDA_PATH
)
call "../../scripts/locate_px_shared.bat" PX_SHARED_PATH
call "../../scripts/locate_xpj.bat" XPJ

"%XPJ%\xpj4.exe" -t vc12 -p win64 -x "%CD%\NvCloth.xpj"
"%XPJ%\xpj4.exe" -t vc12 -p win32 -x "%CD%\NvCloth.xpj"
"%XPJ%\xpj4.exe" -t vc11 -p win64 -x "%CD%\NvCloth.xpj"
"%XPJ%\xpj4.exe" -t vc11 -p win32 -x "%CD%\NvCloth.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p win64 -x "%CD%\NvCloth.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p win32 -x "%CD%\NvCloth.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p xboxone -x "%CD%\NvCloth.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p ps4 -x "%CD%\NvCloth.xpj"
"%XPJ%\xpj4.exe" -t vc14 -p switch -x "%CD%\NvCloth.xpj"

pause