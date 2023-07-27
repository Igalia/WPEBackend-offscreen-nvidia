project('wpebackend-offscreen', 'cpp',
        version: '1.0',
        default_options: ['warning_level=3',
                          'werror=true',
                          'cpp_std=c++20'])

build_args = ['-DEGL_NO_PLATFORM_SPECIFIC_TYPES']

wpe_dep = dependency('wpe-1.0', version: '>=1.14', required: true)
epoxy_dep = dependency('epoxy', version: '>=1.5', required: true)
gio_dep = dependency('gio-2.0', version: '>=2.6', required: true)
x11_dep = dependency('x11', required: true)
build_deps = [wpe_dep, epoxy_dep, gio_dep, x11_dep]

build_src = [
    'application-side/RendererHost.cpp',
    'application-side/RendererHostClient.cpp',
    'application-side/ViewBackend.cpp',
    'common/ipc.cpp',
    'common/wpebackend-offscreen.cpp',
    'wpewebprocess-side/NativeSurface.cpp',
    'wpewebprocess-side/RendererBackendEGL.cpp',
    'wpewebprocess-side/RendererBackendEGLTarget.cpp']

wpebackend_lib = shared_library('wpebackend-offscreen', build_src,
                                dependencies: build_deps,
                                cpp_args: build_args,
                                gnu_symbol_visibility: 'inlineshidden',
                                install: true)

install_headers('wpebackend-offscreen.h')
pkg = import('pkgconfig')
pkg.generate(wpebackend_lib,
             description: 'Offscreen rendering backend for wpe-webkit',
             requires: wpe_dep)

wpebackend_dep = declare_dependency(include_directories: '.',
                                    dependencies: wpe_dep,
                                    link_with: wpebackend_lib)
