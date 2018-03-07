<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>PLCrashReporter 1.2</Name>
  <Location>/Engine/Source/ThirdParty/PLCrashReporter/</Location>
  <Date>2016-06-14T11:42:15.691603-04:00</Date>
  <Function>In-process, multi-thread crash recoding &amp; symbolication for Mac OS X &amp; iOS.</Function>
  <Justification>Our naive signal handler crash reporting is insufficient for Darwin platforms (OS X &amp; iOS), it cannot record all crashes and fails to gather correct reporting details for stack-overflows &amp; other errors that require signals to be handled on a thread other than the raising thread which are unfortunately common</Justification>
  <Eula> See installer EULA: https://www.plcrashreporter.org</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensees</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder>/Engine/Source/ThirdParty/Licenses/PLCrashReporter_1.2_LICENSE.txt</LicenseFolder>
</TpsData>