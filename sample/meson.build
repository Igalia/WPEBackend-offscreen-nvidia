project('webview-sample', 'cpp',
        version: '1.0',
        default_options: ['warning_level=3',
                          'werror=true',
                          'cpp_std=c++20'])

build_args = []
wpewebkit_dep = dependency('wpe-webkit-1.0', version: '>=2.38', required: true)
cc = meson.get_compiler('cpp')
if cc.has_function('webkit_settings_set_enable_websecurity', dependencies: wpewebkit_dep, prefix: '#include <wpe/webkit.h>')
    build_args += '-DHAS_WEB_SECURITY'
endif

wpebackend_dep = dependency('wpebackend-offscreen', required: true)

executable('webview-sample', 'main.cpp',
           cpp_args: build_args,
           dependencies: [wpewebkit_dep, wpebackend_dep])
