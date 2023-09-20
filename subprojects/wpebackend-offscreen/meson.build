project('wpebackend-offscreen', 'cpp',
        version: '1.0',
        default_options: ['warning_level=3',
                          'werror=true',
                          'cpp_std=c++20'])

exported_args = ['-DEGL_NO_PLATFORM_SPECIFIC_TYPES']
build_args = exported_args

wpe_dep = dependency('wpe-1.0', version: '>=1.14', required: true)
egl_dep = dependency('egl', version: '>=1.5', required: true)
exported_deps = [wpe_dep, egl_dep]

glib_dep = dependency('glib-2.0', version: '>=2.6', required: true)
glesv2_dep = dependency('glesv2', version: '>=3.0', required: true)
build_deps = exported_deps + [glib_dep, glesv2_dep]

build_src = [
    'application-side/RendererHost.cpp',
    'application-side/RendererHostClient.cpp',
    'application-side/ViewBackend.cpp',
    'common/EGLStream.cpp',
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
             extra_cflags: exported_args,
             requires: exported_deps)

wpebackendoffscreen_dep = declare_dependency(include_directories: '.',
                                             compile_args: exported_args,
                                             dependencies: exported_deps,
                                             link_with: wpebackendoffscreen_lib)
