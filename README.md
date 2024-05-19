The goal of this branch is to be able to create our own dlls that are compatible with gmod.  
There is also the [gmod-improvements](https://github.com/RaphaelIT7/obsolete-source-engine/tree/gmod-improvements) branch, which will contain bug fixes or improvements.

List of dlls that are worked on:
- `shaderapidx9.dll` (Everything was reverted.)
- `vstdlist.dll` (Everything was reverted.)
- `filesystem_stdio.dll`
Progress:  
- [ ] Update Steamworks
- [x] Implementing basic Gmod functions  
- [x] Implement Language System  
- [x] Implement Gamemode System  
- [ ] Implement GameDepot System
- [ ] Implement Addon System
- - [x] Implement GetSubscriptions  
- - [ ] Implement `cfg/addonnomount.txt` (BUG: Crashes when Saved)  
- - [ ] Implement Addon Mounting  
- - [ ] Implement Addon Download  

List of currently working dlls:
- `datacache.dll`
- `icudt.dll` (IDK what it even does xd)
- `icudt42.dll` (IDK what it even does xd)
- `unicode.dll`
- `vvis_dll.dll` (~40% faster than Gmod's version. Compile it in Release for the Performance benefit)
- `vvis.exe`
- `vrad.exe`
- `vphysics.dll` (Also, fixes ShouldCollide breaking the physics engine)

List of broken dlls:
- launcher
- engine
- vgui2
- ServerBrowser
- vguimatsurface

# Source engine

![CI](https://github.com/Source-Authors/obsolete-source-engine/actions/workflows/build.yml/badge.svg?branch=master)

## How to build

* [windows] Visual Studio 2022 with v143+ platform toolset.
* Use `git clone --recurse-submodules`
* Add registry keys:
  | Path                        | Name  | Type | Data |
  | ----                        | ----  | ---- | ---- |
  | `HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\10.0\Projects\{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}` | `DefaultProjectExtension` | string | vcproj
* Run `creategameprojects_debug.bat` from Developer command prompt.
* Open `hl2.sln`
* Build
