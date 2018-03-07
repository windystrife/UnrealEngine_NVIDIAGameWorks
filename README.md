![Alt text](Nvidia_Gameworks.png?raw=true "Unreal Engine 4 with Nvidia Gameworks integration")

Welcome to Unreal Engine 4 with Nvidia Gameworks integration repository!

Here are combined all possible Nvidia Gameworks technologies in one Unreal Engine 4 repository.

1. [Blast](#nvidia-blast)
2. [Flex](#nvidia-flex)
3. [Flow](#nvidia-flow)
4. [HairWorks](#nvidia-hairworks)
5. [WaveWorks](#nvidia-waveworks)
6. [VXGI](#nvidia-vxgi)
7. [HBAO+](#nvidia-hbao)
 
**[Bulding and running](#building-and-running)**
 1. [Windows](#windows)
 2. [Mac](#mac)
 3. [Linux](#linux)
 4. [Additional platforms](#additional-target-platforms)

### NVIDIA Blast

![Alt text](BlastPluginSample/BlastImage.png?raw=true "Nvidia Blast")

[Blast](https://github.com/NVIDIAGameWorks/Blast) is a NVIDIA GameWorks destruction library. It was fully integrated in UE4 as a separate plugin.

To give it a try compile UE4, and open [Blast UE4 Plugin Sample Project.](BlastPluginSample/README.md)

For more information and a quick start tutorial go to 
[Blast UE4 Plugin Guide.](Engine/Plugins/GameWorks/Blast/Documentation/Intro.md)


### NVIDIA Flex

![Alt text](FlexProject/Documentation/_images/FlexImage.jpg?raw=true "Nvidia Flex")

[Flex](https://github.com/NVIDIAGameWorks/Flex) is a GPU-based particle simulation library that supports fluids, clothing, solids, ropes, and more. The UE4 integration adds new actors and components
to Unreal that can be used add more advanced dynamics to your game.

An example project with test assets and levels is included here:

**[/FlexProject/](FlexProject)**

Documentation and tutorials for the Unreal integration can be found in under:

**[/FlexProject/Documentation](FlexProject/Documentation)**

The latest standalone Flex SDK can be downloaded from NVIDIA's developer zone:

**[https://developer.nvidia.com/flex](https://developer.nvidia.com/flex)**

Not all the Flex features are exposed in Unreal yet, but we're continuing work on integration and will be adding features like fluid surface rendering and smoke simulation in upcoming releases.

### NVIDIA Flow

![Alt text](NvFlowPluginSamples/FlowImage.png?raw=true "Nvidia Flow")

Flow is a adaptive sparse voxel fluid simulation library for real-time applications.

Here's it's included along with two sample projects. The integration is mostly isolated to a plugin, 
with minimal code added to the engine. NvFlowPluginSamples provides basic samples to demonstrate different functionality. 

NvFlowSamplesVR is configured to use the forward renderer with MSAA, and includes more self shadowed fire and smoke samples.

### NVIDIA HairWorks

![Alt text](HairWorks_Project/HairWorksImage.jpg?raw=true "Nvidia HairWorks")

NVIDIA HairWorks enables advanced simulation and rendering techniques for richer visual appeal and provides a deeply immersive experience. HairWorks is the culmination of over 8 years of R&D and harnessed into creating a versatile pipeline for a variety of character designs.

Please see **[HairWorks_Ue4_ReleaseNotes.md](HairWorks_Ue4_ReleaseNotes.md)** for more information.

### NVIDIA WaveWorks

WaveWorks is a library for simulating wind-driven waves on large bodies of water, in real time, using GPU acceleration. 

![Alt text](/Engine/Source/ThirdParty/WaveWorks/doc/html/_images/WaveWorks_ocean.png?raw=true "WaveWorks")

NVIDIA WaveWorks enables developers to deliver a cinematic-quality ocean simulation for interactive applications. The simulation runs in the frequency domain using a spectral wave dispersion model. An inverse FFT step then transforms to the spatial domain ready for rendering. Intuitive real-world parameters such as wind speed and direction can be used to tune the look of the sea surface for a wide variety of conditions - from gentle ripples to a heavy storm-tossed ocean based on the Beaufort scale. Water surface displacement, normals, and foam parameters are accessible inside the shader graph editor to create content-specific appearance of the water. 

The Unreal Engine 4 WaveWorks integration offers you the opportunity to explore WaveWorks with the support of the UE4 rendering pipeline and editor system. The WaveWorks library provided in this branch requires a CUDA-capable GPU. For builds supporting CPU and Direct Compute as well as licensing inquiries, please contact us at visualfx-licensing@nvidia.com.

### NVIDIA VXGI

![Alt text](CornellBoxLightingModes.png "Cornell Box Scene")

VXGI stands for [Voxel Global Illumination](http://www.geforce.com/hardware/technology/vxgi), and it's an advanced rendering technique for real-time indirect illumination.

Global illumination (GI) is a way of computing lighting in the scene that includes indirect illumination, i.e. simulating objects that are lit by other objects as well as ideal light sources. Adding GI to the scene greatly improves the realism of the rendered images. Modern real-time rendering engines simulate indirect illumination using different approaches, which include precomputed light maps (offline GI), local light sources placed by artists, and simple ambient light.

**Q:** I loaded a map but there is no indirect lighting.
**A:** Please make sure that...

- Console variable r.VXGI.DiffuseTracingEnable is set to 1
- Directly lit or emissive materials have "Used with VXGI Voxelization" box checked
- Direct lights are Movable and have "VXGI Indirect Lighting" box checked
- There is an active PostProcessVolume and the camera is inside it (or it's unbounded)
- In the PostProcessVolume, the "Settings/VXGI Diffuse/Enable Diffuse Tracing" box is checked

It is also useful to switch the View mode to "VXGI Opacity Voxels" or "VXGI Emittance Voxels" to make sure that the objects you need are represented as voxels and emit (or reflect) light.

**Q:** I'm trying to build the engine, and there are some linker errors related to NVAPI.
**A:** This means Setup.bat has overwritten the NVAPI libraries with older versions. You need to copy the right version (from the original zip file or from GitHub) of this file: `Engine\Source\ThirdParty\NVIDIA\nvapi\amd64\nvapi64.lib`

**Q:** There are no specular reflections on translucent objects, how do I add them?
**A:** You need to modify the translucent material and make it trace specular cones. See [this forum post](https://forums.unrealengine.com/showthread.php?53735-NVIDIA-GameWorks-Integration&p=423841&highlight=vxgi#post423841) for an example.

**Q:** Can specular reflections be less blurry?
**A:** Usually yes, but there is a limit. The quality of reflections is determined by the size of voxels representing the reflected object(s), so you need to reduce that size. There are several ways to do that:

- Place a "VXGI Anchor" actor near the reflected objects. VXGI's scene representation has a region where it is most detailed, and this actor controls the location of that region.
- Reduce r.VXGI.Range, which will make all voxels smaller, but also obviously reduce the range of VXGI effects.
- Increase r.VXGI.MapSize, but there are only 3 options for that parameter: 64, 128 and 256, and the latter is extremely expensive.

**Q:** Is it possible to pre-compute lighting with VXGI to use on low-end PCs or mobile devices?
**A:** No, as VXGI was designed as a fully dynamic solution. It is theoretically possible to use VXGI cone tracing to bake light maps, but such feature is not implemented, and it doesn't add enough value compared to traditional light map solutions like Lightmass: the only advantage is that baking will be faster.

**Q:** Does VXGI support DirectX 12?
**A:** It does, but in a limited and still experimental way. Switching to DX12 is not yet recommended. It will be slower than on DX11.

**Q:** Can I use VXGI in my own rendering engine, without Unreal Engine?
**A:** Yes. The SDK package is available on [NVIDIA GameWorks Developer website](https://developer.nvidia.com/vxgi).

The VXGI integration was ported from UE 4.12 to 4.13 mostly by Unreal Engine Forums and GitHub user "GalaxyMan2015". 

### NVIDIA HBAO+

![Alt text](HBAO+Image.png "Nvidia HBAO+")

HBAO+ stands for [Horizon-Based Ambient Occlusion Plus](http://www.geforce.com/hardware/technology/hbao-plus), and it's a fast and relatively stable screen-space ambient occlusion solution.

### Building and running

  ### Windows

  1. Install **[GitHub for Windows](https://windows.github.com/)** then **[fork and clone our repository](https://guides.github.com/activities/forking/)**. 
     To use Git from the command line, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.

     If you'd prefer not to use Git, you can get the source with the 'Download ZIP' button on the right. The built-in Windows zip utility will mark the contents of zip files 
   downloaded from the Internet as unsafe to execute, so right-click the zip file and select 'Properties...' and 'Unblock' before decompressing it. Third-party zip utilities don't normally do this.

  2. Install **Visual Studio 2017**. 
     All desktop editions of Visual Studio 2017 can build UE4, including [Visual Studio Community 2017](http://www.visualstudio.com/products/visual-studio-community-vs), which is free for small teams and individual developers.
     To install the correct components for UE4 development, check the "Game Development with C++" workload, and the "Unreal Engine Installer" optional component.
  
  3. Open your source folder in Explorer and run **Setup.bat**. 
     This will download binary content for the engine, as well as installing prerequisites and setting up Unreal file associations. 
     On Windows 8, a warning from SmartScreen may appear.  Click "More info", then "Run anyway" to continue.
   
     A clean download of the engine binaries is currently 3-4gb, which may take some time to complete.
     Subsequent checkouts only require incremental downloads and will be much quicker.
 
  4. Run **GenerateProjectFiles.bat** to create project files for the engine. It should take less than a minute to complete.  

  5. Load the project into Visual Studio by double-clicking on the **UE4.sln** file. Set your solution configuration to **Development Editor** and your solution
     platform to **Win64**, then right click on the **UE4** target and select **Build**. It may take anywhere between 10 and 40 minutes to finish compiling, depending on your system specs.

  6. After compiling finishes, you can load the editor from Visual Studio by setting your startup project to **UE4** and pressing **F5** to debug.

  ### Mac
   
  1. Install **[GitHub for Mac](https://mac.github.com/)** then **[fork and clone our repository](https://guides.github.com/activities/forking/)**. 
     To use Git from the Terminal, see the [Setting up Git](https://help.github.com/articles/set-up-git/) and [Fork a Repo](https://help.github.com/articles/fork-a-repo/) articles.
     If you'd rather not use Git, use the 'Download ZIP' button on the right to get the source directly.

  2. Install the latest version of [Xcode](https://itunes.apple.com/us/app/xcode/id497799835).

  3. Open your source folder in Finder and double-click on **Setup.command** to download binary content for the engine. You can close the Terminal window afterwards.

     If you downloaded the source as a .zip file, you may see a warning about it being from an unidentified developer (because .zip files on GitHub aren't digitally signed).
     To work around it, right-click on Setup.command, select Open, then click the Open button.

  4. In the same folder, double-click **GenerateProjectFiles.command**.  It should take less than a minute to complete.  

  5. Load the project into Xcode by double-clicking on the **UE4.xcworkspace** file. Select the **ShaderCompileWorker** for **My Mac** target in the title bar, then select the 'Product > Build' menu item. When Xcode finishes building, do the same for the **UE4** for **My Mac** target. Compiling may take anywhere between 15 and 40 minutes, depending on your system specs.
   
  6. After compiling finishes, select the 'Product > Run' menu item to load the editor.

  ### Linux

  1. [Set up Git](https://help.github.com/articles/set-up-git/) and [fork our repository](https://help.github.com/articles/fork-a-repo/).
     If you'd prefer not to use Git, use the 'Download ZIP' button on the right to get the source as a zip file.

  2. Open your source folder and run **Setup.sh** to download binary content for the engine.

  3. Both cross-compiling and native builds are supported. 

     **Cross-compiling** is handy when you are a Windows (Mac support planned too) developer who wants to package your game for Linux with minimal hassle, and it requires a [cross-compiler toolchain](http://cdn.unrealengine.com/qfe/v8_clang-3.9.0-centos7.zip) to be installed (see the [Linux cross-compiling page on the wiki](https://docs.unrealengine.com/latest/INT/Platforms/Linux/GettingStarted/)).

     **Native compilation** is discussed in [a separate README](Engine/Build/BatchFiles/Linux/README.md) and [community wiki page](https://wiki.unrealengine.com/Building_On_Linux). 

  ### Additional target platforms

  **Android** support will be downloaded by the setup script if you have the Android NDK installed. See the [Android getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/Android/GettingStarted/).

  **iOS** programming requires a Mac. Instructions are in the [iOS getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/iOS/GettingStarted/index.html).

  **HTML5** support will be downloaded by the setup script if you have Emscripten installed. Please see the [HTML5 getting started guide](https://docs.unrealengine.com/latest/INT/Platforms/HTML5/GettingStarted/index.html).

  **Playstation 4** or **XboxOne** development require additional files that can only be provided after your registered developer status is confirmed by Sony or Microsoft. See [the announcement blog post](https://www.unrealengine.com/blog/playstation-4-and-xbox-one-now-supported) for more information.

### Tech Support

This repository of UE4 is primarily discussed on the Unreal Engine forums: [NVIDIA GameWorks Integration](https://forums.unrealengine.com/showthread.php?53735-NVIDIA-GameWorks-Integration). That forum thread contains many questions and answers, and some NVIDIA engineers also participate in the discussion. For VXGI related questions, comment on that thread, or contact [Alexey.Panteleev](https://forums.unrealengine.com/member.php?29363-Alexey-Panteleev) on the forum, or post an issue on GitHub.

### Additional Resources

- [Interactive Indirect Illumination Using Voxel Cone Tracing](http://maverick.inria.fr/Publications/2011/CNSGE11b/GIVoxels-pg2011-authors.pdf) - the original paper on voxel cone tracing.
- [NVIDIA VXGI: Dynamic Global Illumination for Games](http://on-demand.gputechconf.com/gtc/2015/presentation/S5670-Alexey-Panteleev.pdf) - a presentation about VXGI basics and its use in UE4.
- [Practical Real-Time Voxel-Based Global Illumination for Current GPUs](http://on-demand.gputechconf.com/gtc/2014/presentations/S4552-rt-voxel-based-global-illumination-gpus.pdf) - a technical presentation about VXGI while it was still work-in-progress.

### License

Please read [LICENSE.md](LICENSE.md)
