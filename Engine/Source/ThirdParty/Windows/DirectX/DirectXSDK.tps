<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>June 2010 DirectX SDK</Name>
  <Location>/Engine/Source/ThirdParty/Windows/DirectX/</Location>
  <Date>2016-06-10T14:10:42.9950458-04:00</Date>
  <Function>We would like to redistribute parts of the June 2010 DirectX SDK with UE4 Source (Github).</Function>
  <Justification>The reason we want to do this is that the installer for this SDK has a bug that requires the user to painfully uninstall the 2010 Visual C++ Runtime from their computer (which our Launcher installer auto-installs), and then reinstall it themselves after the DirectX SDK install completes.  It is a mess.</Justification>
  <Eula>http://www.microsoft.com/en-us/download/details.aspx?id=6812</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensees</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder>None</LicenseFolder>
</TpsData>