project('wpebackend-offscreen', 'cpp',
        version: '1.0',
        default_options: ['warning_level=3',
                          'werror=true',
                          'cpp_std=c++20'])

build_args = ['-DEGL_NO_PLATFORM_SPECIFIC_TYPES']

wpe_dep = dependency('wpe-1.0', version: '>=1.14', required: true)
glib_dep = dependency('glib-2.0', version: '>=2.6', required: true)
egl_dep = dependency('egl', version: '>=1.5', required: true)
build_deps = [wpe_dep, glib_dep, egl_dep]
link_deps = [wpe_dep, egl_dep]

build_src = [
    'application-side/RendererHost.cpp',
    'application-side/RendererHostClient.cpp',
    'application-side/ViewBackend.cpp',
    'common/ipc.cpp',
    'common/wpebackend-offscreen.cpp',
    'wpewebprocess-side/RendererBackendEGL.cpp',
    'wpewebprocess-side/RendererBackendEGLTarget.cpp']

wpebackendoffscreen_lib = shared_library('wpebackend-offscreen', build_src,
                                         dependencies: build_deps,
                                         cpp_args: build_args,
                                         gnu_symbol_visibility: 'inlineshidden',
                                         install: true)

install_headers('wpebackend-offscreen.h')
pkg = import('pkgconfig')
pkg.generate(wpebackendoffscreen_lib,
             description: 'Offscreen rendering backend for wpe-webkit',
             requires: link_deps)

wpebackendoffscreen_dep = declare_dependency(include_directories: '.',
                                             dependencies: link_deps,
                                             link_with: wpebackendoffscreen_lib)
