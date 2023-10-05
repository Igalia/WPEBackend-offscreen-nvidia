/*
 * Copyright (C) 2023 Igalia S.L.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "NativeSurface.h"

#include <wpe/webkit.h>
#include <wpebackend-offscreen.h>

namespace
{
WebKitWebViewBackend* createWebViewBackend(const NativeSurface& nativeSurface)
{
    auto* offscreenBackend = wpe_offscreen_view_backend_create(
        reinterpret_cast<wpe_offscreen_on_frame_available_callback>(
            +[](wpe_offscreen_view_backend* backend, EGLImage frame, const NativeSurface* nativeSurface) {
                nativeSurface->draw(frame);
                wpe_offscreen_view_backend_dispatch_frame_complete(backend);
            }),
        const_cast<void*>(reinterpret_cast<const void*>(&nativeSurface)), nativeSurface.getWidth(),
        nativeSurface.getHeight());

    return webkit_web_view_backend_new(wpe_offscreen_view_backend_get_wpe_backend(offscreenBackend), nullptr, nullptr);
}

WebKitWebView* createWebView(const NativeSurface& nativeSurface)
{
    auto* wkManager = webkit_website_data_manager_new_ephemeral();
    webkit_website_data_manager_set_tls_errors_policy(wkManager, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    auto* wkWebContext = webkit_web_context_new_with_website_data_manager(wkManager);
    g_object_unref(wkManager);

    auto* wkWebView = webkit_web_view_new_with_context(createWebViewBackend(nativeSurface), wkWebContext);
    g_object_unref(wkWebContext);

    auto* settings = webkit_web_view_get_settings(wkWebView);
    webkit_settings_set_enable_webaudio(settings, FALSE);
#ifdef HAS_WEB_SECURITY
    webkit_settings_set_enable_websecurity(settings, FALSE);
#endif // HAS_WEB_SECURITY

    return wkWebView;
}
} // namespace

int main(int /*argc*/, const char* /*argv*/[])
{
    g_setenv("WPE_BACKEND_LIBRARY", "libwpebackend-offscreen.so", TRUE);

    auto nativeSurface = NativeSurface::createNativeSurface(800, 600);
    if (!nativeSurface)
        return -1;

    auto* mainLoop = g_main_loop_new(nullptr, FALSE);

    struct Config
    {
        NativeSurface& surface;
        GMainLoop* mainLoop;
    } config = {*nativeSurface, mainLoop};
    g_timeout_add(
        200,
        +[](gpointer userData) -> gboolean {
            Config* data = reinterpret_cast<Config*>(userData);
            if (data->surface.isClosed())
                g_main_loop_quit(data->mainLoop);
            return G_SOURCE_CONTINUE;
        },
        &config);

    auto* wkWebView = createWebView(*nativeSurface);
    // webkit_web_view_load_uri(wkWebView, "https://webglsamples.org/dynamic-cubemap/dynamic-cubemap.html");
    // webkit_web_view_load_uri(wkWebView, "https://webglsamples.org/electricflower/electricflower.html");
    // webkit_web_view_load_uri(wkWebView, "https://webglsamples.org/field/field.html");
    // webkit_web_view_load_uri(wkWebView, "https://webglsamples.org/aquarium/aquarium.html");
    webkit_web_view_load_uri(wkWebView, "https://alteredqualia.com/three/examples/webgl_terrain_dynamic.html");

    g_main_loop_run(mainLoop);

    g_main_loop_unref(mainLoop);
    g_object_unref(wkWebView);
    return 0;
}
