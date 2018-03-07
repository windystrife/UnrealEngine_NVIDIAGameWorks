<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>cxa demangle</Name>
  <Location>/Engine/Source/ThirdParty/Android/cxa_demangle/</Location>
  <Date>2016-04-12T19:25:57.3258913-04:00</Date>
  <Function />
  <Justification>UE needs callstack printing when running on Android devices and we need this function to convert a mangled function name into a human-readable one.  UE links with cxx-stl from stlport, but that doesn’t include the abi::__cxa_demangle function in the library. The gnu-libstdc++ libraries also included with the NDK do include this function, but we don’t link against gnu-libstdc++ and don’t want to do so just for this function. The easiest solution is to compile the __cxa_demangle source code file and link it manually.</Justification>
  <Eula>https://android.googlesource.com/platform/external/libcxxabi_35a/+/master/LICENSE.TXT</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensees</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder>/Engine/Source/ThirdParty/Licenses/CXADemangle_License.txt</LicenseFolder>
</TpsData>