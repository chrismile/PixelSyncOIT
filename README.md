# PixelSyncOIT
A demo for order independent transparency using pixel synchronization (GL_ARB_fragment_shader_interlock)

Prerequisites (build currently only supported on Linux):
- sgl: https://github.com/chrismile/sgl (use sudo make install to install this library on your system)
- The datasets are not supplied with the repository.

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
