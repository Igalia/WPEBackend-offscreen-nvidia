# WPEBackend Offscreen NVidia

[![License](https://img.shields.io/badge/License-BSD_2--Clause-orange.svg)](./LICENSE)

This code provides a video backend for [WPEWebkit](https://wpewebkit.org/) on
Linux.

The purpose of this backend is to enable hardware accelerated rendering with
offscreen contexts from the WPEWebkit compositor to the end-user application
process with zero-copy of the video buffers.

The backend doesn't either depend nor need the presence of any windowing system
and, so, can work perfectly from within a docker container with a minimal
configuration. Indeed, it relies on EGL >= 1.5 and needs an NVidia GPU with the
following extensions:
- [EGL_MESA_platform_surfaceless](https://registry.khronos.org/EGL/extensions/MESA/EGL_MESA_platform_surfaceless.txt)
- [EGL_KHR_stream](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_stream.txt)
- [EGL_KHR_stream_cross_process_fd](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_stream_cross_process_fd.txt)
- [EGL_KHR_stream_producer_eglsurface](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_stream_producer_eglsurface.txt)
- [EGL_NV_stream_consumer_eglimage](https://registry.khronos.org/EGL/extensions/NV/EGL_NV_stream_consumer_eglimage.txt)

To use the host NVidia GPU from within a docker container, you need to install
the [NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html)

The backend will pass an EGLImage for each new frame produced by the WPEWebkit
compositor.

## Build the project

### Using docker

The backend itself doesn't need any windowing system, but, in order to display
the produced frames, the webview-sample opens an X11 window. In order to
authorize the sample container to access the host X11 server, you will need to
call `xhost +` on the host.

Then just call `docker compose up` to build automatically the backend and the
sample. It will run the webview-sample from a docker container.

The [Dockerfile](./Dockerfile) defines two containers:
- *webview-builder* in charge of building the WPEWebkit dependencies and the
  backend,
- *webview-sample* in charge of building and running the demonstration sample,
  copying the needed resources from the previous container.

### On Ubuntu Jammy

If working on the backend itself, it is more suitable to build the dependencies
and the library directly on the system. You can do it on Ubuntu Jammy
(22.04 LTS), using the *buildenv-webkit-ubuntu* script.

Calling this script will build locally all dependencies and open a shell with
all needed environment variables set to proceed with the backend build.

```shell
./buildenv-webkit-ubuntu
```

Once inside the build environment, call:

```shell
meson setup build
ninja -C build
```

It will build the backend library into
*build/subprojects/wpebackend-offscreen-nvidia* and the sample
application into *build/subprojects/webview-sample*. Before launching the
`webview-sample` application you must ensure that the `LD_LIBRARY_PATH`
environment variable points to the
*build/subprojects/wpebackend-offscreen-nvidia* directory (this variable is
automatically set by the *buildenv-webkit-ubuntu* script).

## Debugging the WPEWebProcess side of the backend

You need to have built the WPEWebkit dependencies locally using the
*buildenv-webkit-ubuntu* script. Just define the environment variable
`WEBKIT2_PAUSE_WEB_PROCESS_ON_LAUNCH=1` before launching the `webview-sample`
application and then connect your debugger to the *WPEWebProcess*.

If you have built *libWPEWebKit-1.0.so* in debug mode, you may need to replace
*webkit-2.38/dist/lib/libWPEWebKit-1.0.so.3.18.9* by
*libWPEWebKit-1.0.so.debug* as the *buildenv-webkit-ubuntu* script strips the
library debug symbols by default to prevent huge loading times when not needing
to directly debug the WPEWebkit library.
