Blast Plugin Guide
=======================

[Blast](https://github.com/NVIDIAGameWorks/Blast) is a new NVIDIA GameWorks destruction library. It was fully integrated in UE4 as a separate plugin. This plugin is intended to replace APEX destruction as faster, more flexible and feature rich solution for making destructible game objects.

This is a plugin guide. For the original Blast library documentation and sources visit
https://github.com/NVIDIAGameWorks/Blast .

# Features

* Rich in-editor asset authoring
* Flexible damage system 
* Impact damage
* Stress-based damage
* Debris
* Effects/Sounds hook up callbacks
* Both static and dynamic actors
* _Blueprint_ friendly
* Minimum engine changes

# Quick Start

If you want to try it out before getting into more details, just do the following:

1. Create a new UE4 project selecting _First Person Template_ with _Starter Content_.
2. In _Content Browser_, select the _Content_ folder and type `1M_Cube` in the search field. You should see the cube static mesh. You can also use any other static mesh if you like. Right click on it and select _Create Blast Mesh_. 
3. Double click the created _Blast Asset_ (`1M_Cube_Blast`). This will open the [Blast Mesh Editor](BlastMeshEditor.md) window. Click on the _Fracture_ button on the top panel. You should now see the fractured cube. You can use the _Explode Amount_ slider and adjust the camera position to view each chunk.
4. Close [Blast Mesh Editor](BlastMeshEditor.md) and save the _Blast Asset_. Drag the resulting _Blast Asset_ into the UE4 scene. A _Blast Actor_ will be created from it.
5. The easiest way to fracture it, is to make it fragile and let it fall on the ground. To do so, select the created _Blast Actor_ and select _BlastMeshComponent_ in the _Details_ panel. In the _Blast_ section, turn on _Impact Damage Properties_ override and change _Hardness_ to `0.01` or lower to make it fragile. 

![Alt text](images/Intro_QuickStart.png?raw=true "Impact Damage Settings")

6. Raise the object above the ground and hit the _Play_ button. The cube should fall and fracture in pieces. 

7. If you want to fracture it by shooting with the gun, you need to edit the `FirstPersonProjectile` asset. There are 2 ways to do it:
    * Pass `BoneName` from _Event Hit_ `Hit` field into the _Add Impulse At Location_ function. So that the impulse will reach the _Blast Actor_ bone. ![Alt text](images/Intro_FirstPersonProjectileBP.png?raw=true "Passing BoneName")
    * Even better, remove all BP logic (or just detach the _Event Hit_ callback). Select _CollisionComponent_, toggle _Simulate Physics_ ON and set _Collision Enabled_ to _Physics Only_ (in the _Collision_/_Collision Presets_ section). That will make the projectile a regular physical object that will bounce and break _Blast Actors_ too. 
    
        ![Alt text](images/Intro_FirstPersonProjectileSimulate.png?raw=true "Simulate Physics") ![Alt text](images/Intro_FirstPersonProjectileCollision.png?raw=true "Physics Only") 

# Overview

In order to add destructible objects, you first need a _Blast Asset_. It is essentially a new type of _Skinned Mesh Asset_ which represents a destructible asset. There are 2 ways to create it: either import one or create it from static mesh. Look in [Blast Mesh Editor Documentation](BlastMeshEditor.md) for more details.

Once you have a _Blast Asset_ you can place it in the scene as usual. A simple _Actor_ will be created with a `BlastMeshComponent` attached. We will refer to it as _Blast Actor_ from now on, but it all refers to this `BlastMeshComponent`.

By default, _Blast Actor_ behaves like a regular physically simulated dynamic object. To make it static see [Making _Blast Actor_ Static](#making-blast-actor-static). 

_Blast Actor_ can be damaged and broken into pieces, there are few ways to do it:

* [Impact damage](#impact-damage) (colliding it with other physical objects)
* Calling _Blast Actor_ (or `BlastMeshComponent` in particular) [damage API](#damage-api) from BP or from code
* [Stress-based damage](#stress-based-damage)

# Damage

_Blast_ damage model is described in details in [Blast Docs](https://github.com/NVIDIAGameWorks/Blast). But essentially all you need to know is that _Blast Actor_ contains a _Support Graph_ (collection of nodes and bonds). Think of it as a skeleton or carcass which holds the object together. Damage is applied on bonds (if present) or on the chunk itself (usually on small chunks of lower level of hierarchy). 

In every damage event, some damage program is involved (so called _Damage Shader_) which takes as input damage parameters and material, and produces damage commands to reduce health on some bonds and/or chunks. The Plugin allows you to write your own damage programs as well, but provides default ones which cover most cases.

All default damage settings are set per _Blast Asset_, but can be overwritten per _Blast Actor_. So all the parameters below can be set either in [Blast Mesh Editor](BlastMeshEditor.md) by double clicking on an asset or on the _Blast Actor_ itself (in _Details Panel_), by toggling on the respective override flag. In this example, _Impact Damage Properties_ and _Stress Properties_ are overriden: 

![Alt text](images/Intro_OverrideFlags.png?raw=true "Override Flags")

All damage settings have a descriptive hint when hovering over them (as well as in-code comments), so this documentation will only give a general overview.

You can view/debug render the _Support Graph_, see [Debug Render](#debug-render).

_Support Graph_ debug render (red bonds have lower health because of prior damage events):

![Alt text](images/Intro_SupportGraph.png?raw=true "Support Graph")

## Blast Material 

![Alt text](images/Intro_BlastMaterial.png?raw=true "Blast Material")

Mainly, material sets how much health every bond and chunk in destructible object have. Every damage event can diminish health. Once it reaches 0, the bond/chunk is broken. _Blast Material_ also allows to filter out small damage events, so that small damage events do not affect health and will not accumulate. It also allows to clamp large damage events, for example if you do not want to fracture objects in pieces with only one damage event ever.

_Blast Material_ is involved in all types of damage except for the [stress-based](#stress-based-gamage). 


## Impact Damage

![Alt text](images/Intro_ImpactDamageSettings.png?raw=true "Impact Damage Settings")

Impact damage happens on collision of _Blast Actor_ with any physical object, both kinematic and dynamic (marked as _physically simulated_). To do that, a _Blast Actor_ subscribes to its _OnComponentHit_ callback, so all the usual UE4 rules of collision callbacks apply. If impact damage does not happen:

1. Check on _Blast Actor_ if _Impact Damage_ _Enabled_ is ON.
2. Set _Blast Actor_ _Hardness_ to a very low value like `0.001` to make it super fragile while testing.
3. On the colliding object check in the _Collision_ section specifically that _Simulation Generates Hit Events_ is ON, _Collision Enabled_ is set to _Query and Physics_ and _Object Types_ are compatible for _Block_ collision.
 

## Damage API

There is a set of damage functions which can be called from BP or from code. You can call them on specific _Blast Actor_ or on an area, so that it will search for _Blast Actor_ around in overlap volume. For that every function has _static_ version with _All_ postfix. Examples:

* `ApplyRadialDamage` and `ApplyRadialDamageAll` - radial/sphere-shaped damage
* `ApplyCapsuleDamage` and `ApplyCapsuleDamageAll` - capsule/capsule shaped damage (can be used for cut-like, laser beam, sword damage)

There are also some methods which can be called only from code, but they allow to execute custom damage program, like: `ApplyDamageProgram`, `ApplyDamageProgramOverlap`, `ApplyDamageProgramOverlapAll`

## Stress-Based Damage

![Alt text](images/Intro_StressProperties.png?raw=true "Stress Properties")

On any _Blast Actor_/_Blast Asset_ you can enable stress solver. The most common usage is just applying gravity force on a static construction so that it will fall apart at some point when the carcass cannot hold anymore. Dynamic actors are also supported, you could for example add centrifugal force so that rotating an object fast enough will break bonds.

It calculates stress on _Support Graph_ every frame. The quality vs computational cost trade off can be set on both ends of spectrum with its `BondIterationsPerFrame` setting.

It also can be used as another way to apply impact damage, which can give the visually pleasant result of an actor breaking in a weak place instead of the place of contact.

It uses _Support Graph_, but optionally simplifies it with _GraphReductionLevel_ setting. The smaller the stress graph is, the more precise stress calculation happens in the same time interval, but the overall structure representation by this simplified graph becomes more imprecise.

You can view/debug render the stress graph, see [Debug Render](#debug-render).

# Adding Effects

_Blast Actor_/_BlastMeshComponent_ provides a set of damage callbacks on which you can subscribe. This is a good place to hookup additional effects, like smoke and dust.

From _Blast_ perspective, _Actor_ has a bit different meaning. _Actor_ is any independent island of chunks, just like the PhysX kind of actor consisting of shapes. A _Blast Actor_ can have a lot of live _Actors_ (also called chunk islands, bones) at any moment. So when _Blast Actor_ fractures in parts, one big _Actor_ is destroyed and few small _Actor_s are created. Hence there are _OnActorDestroyed_ and _OnActorCreated_ callbacks. There are also methods to get an _Actor_'s COM and position: `GetActorWorldTransform` and `GetActorCOMWorldPosition`.

For more detailed effect placement use per-bond or per-chunk damage callbacks: _OnBondsDamaged_ and _OnChunksDamaged_.

# Making _Blast Actor_ Static

By default any _Blast Actor_ is dynamic. There are 2 ways to make it static:

* Mark 1 or more chunks as static in [Blast Mesh Editor](BlastMeshEditor.md). So that any _Blast Actor_ which contains this chunk becomes static. For example if you have a statue on a pedestal, you can mark the pedestal chunk as static. While it exists, the statue will be static. Once the pedestal is broken into pieces, the statue detaches.

* The same effect can be achieved with the special _Blast Glue Volume_. Place it in the scene and any chunk inside of this volume becomes static. Note: It is important to call _Build Blast Glue_ or the whole _Build Scene_ step in order to apply _Blast Glue Volume_. _Blast Actor_/_BlastMeshComponent_ must have the _Supported By World_ setting truned ON. Moving volumes or actors requires rebuilding.

![Alt text](images/Intro_GlueVolume.png?raw=true "Glue Volume/Blast Build")

# Creating an _Extended Support Struture_

You may join any number of Blast actors in a level, so that they form one actor with a combined ("extended") support structure.  The combined actor can hold together with chunks from one or more of the individual actors.  To create an extended suport structure:

* Place the actors in the level, configured as you would like them to be in the combined structure
* Select the actors and right-click
* Choose the context menu item "Add New Extended Support Group"

![Alt text](images/Intro_CreateExtendedSupportStructure.png?raw=true "Extended Support Group")

Note: It is important to call _Build Blast Glue_ or the whole _Build Scene_ step in order to create the extended support structure for the whole group of actors.

Internally, a new Blast asset is created with the combined support structure, and a single Blast actor is created as an instance.  However, the original (individual) actors still appear in the level editor.

After the _Build Blast Glue_ step, you can see the extended support graph by selecting the BlastExtendedSupportStructure, and choosing the SupportGraph option for the Blast Debug Rendering Mode (in the Blast subpanel):

![Alt text](images/Intro_ExtendedSupportGraph.png?raw=true "Extended Support Graph")

You'll probably need to change the viewport rendering mode to wireframe in order to see the graph.

In the Blast subpanel you may also manually add or remove individual actors to the structure.  Keep in mind, any such change will require a _Build Blast Glue_ step.

# Debris Properties

![Alt text](images/Intro_DebrisProperties.png?raw=true "Impact Damage Settings")

You can limit chunks lifetime using _Debris Filter_. Intuitively it means that for example you can limit lifetime of chunks that are too small, or too separated from origin, or leave some defined bounding box. Use _Debris Filter_ properties for that. You can define multiple filters each of them will act separately. For example you can set that all chunks which fly too far away from origin must be removed with one filter. And set that chunks on depth 5 should live 10 seconds and be removed.

# Debug Render

![Alt text](images/Intro_DebugRender.png?raw=true "Debug Render")

_Blast Actor_/_BlastMeshComponent_ comes with debug render options in editor in order to see what is going on under the hood. Look for _Blast Debug Render Mode_ setting under _Blast_/_Advanced_ section. It allows to draw:

* _Support Graph_
* _Stress Graph_
* _Chunk Centroids_

Consider using wireframe mode (F1) together with it.

You can also view collision shapes by toggling _Show_/_Collision_ in the usual UE4 viewport menu on the top.
