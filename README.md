<img src="https://github.com/wixtoolset/Home/raw/master/imgs/wix-white-bg.png" alt="WiX Toolset" height="128" />

![latest version](https://img.shields.io/nuget/vpre/wix)
![download count](https://img.shields.io/nuget/dt/wix)
![build status](https://img.shields.io/appveyor/build/wixtoolset/wix4)

# WiX Toolset v4

This repository contains the WiX Toolset v4 codebase.

# Developing WiX

## Prerequisites

Visual Studio 2022 (17.0.4 or higher) with the following installed:

| Workloads |
| :-------- |
| ASP.NET and web development |
| .NET desktop development |
| Desktop development with C++ |

| Individual components |
| :-------------------- |
| .NET Framework 4.7.2 SDK |
| .NET Framework 4.7.2 targeting pack |
| MSVC v141 - VS 2017 C++ ARM64 build tools (v14.16) |
| MSVC v141 - VS 2017 C++ x64/x86 build tools (v14.16) |
| MSVC v143 - VS 2022 C++ ARM64 build tools (Latest) |
| MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest) |

##### Getting started:

* Fork the WiX4 repository (https://github.com/wixtoolset/wix4.git)
 into a GitHub domain that you control
* Clone the WiX4 repository from your fork (git clone https://github.com/yourdomain/wix4.git)
 into the directory of your choice

##### To build the WiX toolset:

 * Start a VS2022 'Developer Command Prompt'
 * Change directory to the root of the cloned repository
 * Issue the command `devbuild` (or `devbuild release` if you want to create a release version)

 ##### Executing your newly built WiX toolset
 
 * `build\wix\Debug\publish\wix\wix --help` (Of course changing Debug to Release if you built in Release mode)

 ##### Pull request expectations
 
 * Pick an outstanding WiX4 issue (or create one). Add a comment requesting that you be assigned to the issue. Wait for confirmation.
 * To create a PR fork a new branch from the develop branch
 * Make changes to effect whatever changed behaviour is required for the PR
 * Push the changes to your repository origin as needed
 * If there are multiple commits squash them down to one commit.
 * If the develop branch has changed since you created your new branch rebase to the current development branch.
 * If needed (ie, you squashed or rebased), do a force push of your branch
 * Create a PR with your branch against the WiX4 repository.
 
