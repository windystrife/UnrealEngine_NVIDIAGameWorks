<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>Planarity</Name>
  <Location>/Engine/Source/Developer/GraphColor/</Location>
  <Date>2016-06-17T12:06:55.9504259-04:00</Date>
  <Function>Used for graph coloring of meshes to generate a different color per vertex than any adjacent vertices while only using a total of 5 colors. Calculates in linear time which is unique. Used to generate barycentric coordinates per triangle without the need for specific hardware support.</Function>
  <Justification>Modified the source code and copied into Engine. Compiled with our build system but behind a wrapper.</Justification>
  <Eula>https://github.com/MridulS/planarity/blob/master/c/graph.h</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensees</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder>/Engine/Source/ThirdParty/Licenses/Planarity_License.txt</LicenseFolder>
</TpsData>