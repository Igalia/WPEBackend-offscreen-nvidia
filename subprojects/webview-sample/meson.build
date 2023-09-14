project('webview-sample', 'cpp',
        version: '1.0',
        default_options: ['warning_level=3',
                          'werror=true',
                          'cpp_std=c++20'])

build_args = ['-DEGL_NO_PLATFORM_SPECIFIC_TYPES']

wpewebkit_dep = dependency('wpe-webkit-1.0', version: '>=2.38', required: true)
cc = meson.get_compiler('cpp')
if cc.has_function('webkit_settings_set_enable_websecurity', dependencies: wpewebkit_dep, prefix: '#include <wpe/webkit.h>')
    build_args += '-DHAS_WEB_SECURITY'
endif

wpebackendoffscreen_dep = dependency('wpebackend-offscreen',
                                      fallback: ['wpebackend-offscreen', 'wpebackendoffscreen_dep'],
                                      required: true)

egl_dep = dependency('egl', version: '>=1.5', required: true)
x11_dep = dependency('x11', required: true)
build_deps = [wpewebkit_dep, wpebackendoffscreen_dep, egl_dep, x11_dep]

build_src = [
    'main.cpp',
    'NativeSurface.cpp']

executable('webview-sample', build_src,
           cpp_args: build_args,
           dependencies: build_deps)
