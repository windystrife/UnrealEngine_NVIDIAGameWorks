Before running these Android scripts you will have to run Config and Make for Windows.

Make sure your NDKROOT path only has forward slashes (/) as the backward slashes (\) will cause issues.

If you have to change any of the files, make sure before trying to run any of them to run 'dos2unix' on the file eg 'dos2unix Make\ for\ Android.bat' in cygwin.  


This is the order to run the scripts in from the Engine\Source\ThirdParty\ICU\icu4c-53_1 directory in cygwin...

1. Config for Windows - Release.bat
2. Make on Windows.bat
3. Run Config for Android.bat with Usage options. like "Config for Android.bat ARMv7 d" or "Config for Android.bat ARM64"
4. Run Make for Android.bat with Usage options. like "Make for Android.bat ARMv7 d" 

You'd need to follow these steps for each arch you want to rebuild.