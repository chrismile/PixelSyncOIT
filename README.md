# OIT Rendering Tool (PixelSyncOIT)
A visualization tool for rendering triangle, line and point data sets using order independent transparency (OIT).

This tool uses OpenGL 4.5 together with pixel synchronization (GL_ARB_fragment_shader_interlock) to render these data sets.
It was created for the paper "A Comparison of Rendering Techniques for Large 3D Line Sets with Transparency" (submitted to TVCG).

IMPORTANT NOTICE: This tool was created as a testing tool for comparing the performance, quality and memory consumption of different OIT rendering algorithms.
It is not meant to be a stable and full-fledged visualization tool like ParaView. Use at your own risk!

The followings rendering algorithms are currently supported.
- Per-pixel linked lists (LL)
- Moment-based Order-Independent Transparency (MBOIT)
- Multi-Layer Alpha Blending (MLAB)
- Multi-Layer Alpha Blending using depth buckets (MLABDB)
- K-Buffer (KB)
- Hybrid Transparency (HT)
- Depth Peeling (DP)
- Voxel Ray Casting (VRC)
- Ray tracing using OSPRay (triangles, lines using Embree, generalized tube primitives, see https://github.com/MengjiaoH/ospray-module-tubes)

The .glsl shaders of all algorithms can be found in 'Data/Shaders'.

Prerequisites (build currently only supported on Linux):
- sgl: https://github.com/chrismile/sgl (recommended to use sudo make install to install this library on your system)
- The datasets are not supplied with the repository. Currently, to render all datasets, the following files are needed:
    * Rings: Data/Rings/rings.obj
    * Aneurysm: Data/Trajectories/9213_streamlines.obj
    * Convection Rolls: Data/ConvectionRolls/output.obj
    * Turbulence: Data/ConvectionRolls/turbulence80000.obj
    * Convection Rolls (Small): Data/ConvectionRolls/turbulence20000.obj
    * ... (TODO)

For internal use, these data sets are converted to .binmesh files (.binmesh_lines files for line data sets not converted to triangle hulls).

## Building and running the programm

On Ubuntu 18.04 for example, you can install all necessary packages with this command (additionally to the prerequisites required by sgl):

```
sudo apt-get install libnetcdf-dev netcdf-bin
```

After installing sgl (see above) execute in the repository directory:

```
mkdir build
cd build
cmake ..
make -j 4
ln -s ../Data .
```
(Alternatively, use 'cp -R ../Data .' to copy the Data directory instead of creating a soft link to it).

The build process was also tested on Windows 10 64-bit using MSYS2 and Mingw-w64 (http://www.msys2.org/). Using MSYS2 and Pacman, the following packages need to be installed additionally to the prerequisites required by sgl.

```
pacman -S mingw64/mingw-w64-x86_64-netcdf
```

On Windows, using MSYS2 and Mingw-w64 (http://www.msys2.org/), it is best to use the following CMake command:
```
cmake .. -G"MSYS Makefiles"
```


To run the program, execute:
```
export LD_LIBRARY_PATH=/usr/local/lib
./PixelSyncOIT
```

## Ray tracing with OSPRay

If the user wants to build the program with support for ray tracing with OSPRay, USE_RAYTRACING must be set to ON when using cmake.
Additonally, the user also needs to compile OSPRay with support for generalized tube primitives (see https://github.com/MengjiaoH/ospray-module-tubes).

## How to add new data sets

In src/Performance/InternalState.hpp, add the file name of the data set to MODEL_FILENAMES and the display name to MODEL_DISPLAYNAMES.
In src/MainApp.cpp, the name of the data set that should be loaded at start-up can be set in startupModelName.

Currently, the program supports line and triangle data sets stored in .obj files and triangle data sets stored in .bobj files.
Additionally, it has loaders for data set specific NetCDF .nc formats for lines and .xml and .bin formats for point data sets.
Internally, these data sets are converted to .binmesh files (.binmesh_lines for line data sets not converted to triangle hulls).
