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

#include "ipc.h"

#include <cassert>
#include <cerrno>

#include <sys/socket.h>

using namespace IPC;

Channel::Channel(MessageHandler& handler) noexcept : m_handler(handler)
{
    int sockets[2] = {};
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0)
    {
        g_critical("Cannot create Unix sockets pair for IPC channel");
        return;
    }

    if (configureLocalEndpoint(sockets[0]))
        m_peerFd = sockets[1];
    else
        close(sockets[1]);
}

Channel::Channel(MessageHandler& handler, int peerFd) noexcept : m_handler(handler)
{
    if (peerFd == -1)
    {
        g_critical("Invalid peer file descriptor for IPC channel");
        return;
    }

    configureLocalEndpoint(peerFd);
}

bool Channel::sendMessage(const Message& message) noexcept
{
    if (m_localFd == -1)
        return false;

    const ssize_t ret = send(m_localFd, &message, Message::MESSAGE_SIZE, MSG_EOR | MSG_NOSIGNAL);
    if (ret == 0)
    {
        closeChannel();
        m_handler.handlePeerClosed(*this);
        return false;
    }
    else if (ret == -1)
    {
        closeChannel();
        m_handler.handleError(*this, errno);
        return false;
    }

    assert(ret == Message::MESSAGE_SIZE);

    auto fdCount = message.getFDCount();
    if (fdCount > 0)
    {
        assert(fdCount * sizeof(int) < Message::PAYLOAD_SIZE);
        auto buffer = message.getPayload<int>();
        for (uint16_t i = 0; i < fdCount; ++i)
        {
            if (!writeFileDescriptor(buffer[i]))
                return false;
        }
    }

    return true;
}

int Channel::detachPeerFd() noexcept
{
    int peerFd = m_peerFd;
    m_peerFd = -1;
    return peerFd;
}

void Channel::closeChannel() noexcept
{
    if (m_idleSourceId)
    {
        g_source_remove(m_idleSourceId);
        m_idleSourceId = 0;
    }

    if (m_peerFd != -1)
    {
        close(m_peerFd);
        m_peerFd = -1;
    }

    if (m_localFd != -1)
    {
        close(m_localFd);
        m_localFd = -1;
    }
}

bool Channel::configureLocalEndpoint(int localFd) noexcept
{
    assert(localFd != -1);
    assert(!m_idleSourceId);

    m_localFd = localFd;
    m_idleSourceId = g_idle_add(G_SOURCE_FUNC(idleCallback), this);
    if (!m_idleSourceId)
    {
        g_critical("Cannot attach idle source for IPC channel");
        return false;
    }

    return true;
}

gboolean Channel::idleCallback(Channel* channel) noexcept
{
    Message message;
    if (channel->readNextMessage(message))
        channel->m_handler.handleMessage(*channel, message);

    return G_SOURCE_CONTINUE;
}

bool Channel::readNextMessage(Message& message) noexcept
{
    if (m_localFd == -1)
        return false;

    uint8_t byte;
    ssize_t ret = recv(m_localFd, &byte, sizeof(byte), MSG_DONTWAIT | MSG_PEEK);
    if (ret != sizeof(byte))
    {
        if (ret == 0)
        {
            closeChannel();
            m_handler.handlePeerClosed(*this);
        }
        else if ((ret == -1) && (errno != EWOULDBLOCK))
        {
            closeChannel();
            m_handler.handleError(*this, errno);
        }

        return false;
    }

    ret = recv(m_localFd, &message, Message::MESSAGE_SIZE, MSG_WAITALL);
    if (ret == 0)
    {
        closeChannel();
        m_handler.handlePeerClosed(*this);
        return false;
    }
    else if (ret == -1)
    {
        closeChannel();
        m_handler.handleError(*this, errno);
        return false;
    }

    assert(ret == Message::MESSAGE_SIZE);

    auto fdCount = message.getFDCount();
    if (fdCount > 0)
    {
        assert(fdCount * sizeof(int) < Message::PAYLOAD_SIZE);
        auto buffer = message.getPayload<int>();
        for (uint16_t i = 0; i < fdCount; ++i)
        {
            int fd = readFileDescriptor();
            if (fd == -1)
            {
                for (uint16_t j = 0; j < i; ++j)
                    close(buffer[j]);

                return false;
            }

            buffer[i] = fd;
        }
    }

    return true;
}

bool Channel::writeFileDescriptor(int fd) noexcept
{
    assert(m_localFd != -1);
    if (fd == -1)
        return false;

    union {
        cmsghdr header;
        char buffer[CMSG_SPACE(sizeof(int))];
    } control = {};

    msghdr msg = {};
    msg.msg_control = control.buffer;
    msg.msg_controllen = sizeof(control.buffer);

    cmsghdr* header = CMSG_FIRSTHDR(&msg);
    header->cmsg_len = CMSG_LEN(sizeof(int));
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    *reinterpret_cast<int*>(CMSG_DATA(header)) = fd;

    if (sendmsg(m_localFd, &msg, MSG_EOR | MSG_NOSIGNAL) == -1)
    {
        closeChannel();
        m_handler.handleError(*this, errno);
        return false;
    }

    return true;
}

int Channel::readFileDescriptor() noexcept
{
    assert(m_localFd != -1);

    union {
        cmsghdr header;
        char buffer[CMSG_SPACE(sizeof(int))];
    } control = {};

    msghdr msg = {};
    msg.msg_control = control.buffer;
    msg.msg_controllen = sizeof(control.buffer);

    if (recvmsg(m_localFd, &msg, MSG_WAITALL) == -1)
    {
        closeChannel();
        m_handler.handleError(*this, errno);
        return -1;
    }

    cmsghdr* header = CMSG_FIRSTHDR(&msg);
    if (header && (header->cmsg_len == CMSG_LEN(sizeof(int))) && (header->cmsg_level == SOL_SOCKET) &&
        (header->cmsg_type == SCM_RIGHTS))
    {
        int fd = *reinterpret_cast<int*>(CMSG_DATA(header));
        return fd;
    }

    return -1;
}
