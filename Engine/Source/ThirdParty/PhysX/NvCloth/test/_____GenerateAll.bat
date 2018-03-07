echo "Note: You need to run this with admin rights for the first time to set GW_DEPS_ROOT globally."
call "../scripts/locate_gw_root.bat" GW_DEPS_ROOT_F
@echo on
setx GW_DEPS_ROOT "%GW_DEPS_ROOT_F%
echo GW_DEPS_ROOT = %GW_DEPS_ROOT%
call "../scripts/locate_cmake.bat" CMAKE_PATH_F
echo CMAKE_PATH_F = %CMAKE_PATH_F%


SET PATH=%PATH%;"%CMAKE_PATH_F%"
call GenerateProjects.bat
call GenerateProjectsPs4.bat
call GenerateProjectsSwitch.bat
call GenerateProjectsXboxOne.bat
pause