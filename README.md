# PixelSyncOIT
A demo for order independent transparency using pixel synchronization (GL_ARB_fragment_shader_interlock)

Prerequisites (build currently only supported on Linux):
- sgl: https://github.com/chrismile/sgl (use sudo make install to install this library on your system)

## Building and running the programm
After installing sgl (see above) execute in the repository directory:

```
mkdir build
cd build
cmake ..
make -j 4
ln -s ../Data .
```
(Alternatively, use 'cp -R ../Data .' to copy the Data directory instead of creating a soft link to it).

To run the program, execute:
```
export LD_LIBRARY_PATH=/usr/local/lib
./PixelSyncOIT
```
