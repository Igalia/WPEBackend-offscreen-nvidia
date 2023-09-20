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

#include "RendererHost.h"

wpe_renderer_host_interface* RendererHost::getWPEInterface() noexcept
{
    static wpe_renderer_host_interface s_interface = {
        // void* create()
        +[]() -> void* { return new RendererHost(); },
        // void destroy(void* data)
        +[](void* data) { delete static_cast<RendererHost*>(data); },
        // int create_client(void* data) => called each time a new WPEWebProcess needs to be launched, it fetches the
        //                                  IPC client file descriptor used later from the WPEWebProcess side by
        //                                  RendererBackendEGL to communicate with the RendererHostClient on the
        //                                  application process side.
        +[](void* data) -> int { return static_cast<RendererHost*>(data)->addClient().detachIPCChannelPeerFd(); },
        nullptr, nullptr, nullptr, nullptr};

    return &s_interface;
}

RendererHost::~RendererHost()
{
    for (RendererHostClient* client : m_clients)
        delete client;
}

RendererHostClient& RendererHost::addClient() noexcept
{
    m_clients.push_back(new RendererHostClient(this));
    return *m_clients.back();
}

void RendererHost::removeClient(const RendererHostClient* client) noexcept
{
    if (!client)
        return;

    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it)
    {
        if (*it == client)
        {
            m_clients.erase(it);
            delete client;
            break;
        }
    }
}
