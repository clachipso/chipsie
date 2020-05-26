
/*
 * MIT License
 *
 * Copyright (c) 2020 Aaron C. Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Networking.hpp"
#include <stdio.h>
#include <ws2tcpip.h>
#include <thread>
#include <chrono>
using namespace std;

static const char *TWITCH_IRC_ADDR = "irc.chat.twitch.tv";
static const char *TWITCH_IRC_PORT = "6667";

static SOCKET cs;
static bool connected;
static char tx_buffer[1024];
static char rx_buffer[1024];
static AuthData credentials;
static struct addrinfo *hint_results = NULL;

NetStatus InitNetworking(const AuthData &auth_data)
{
    WSADATA wsa_data;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (rc != 0)
    {
        printf("ERROR: Failed to initialize winsock. errno %d\n", rc);
        return NET_ERROR;
    }

    struct addrinfo hints;
    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    rc = getaddrinfo(TWITCH_IRC_ADDR, TWITCH_IRC_PORT, &hints, &hint_results);
    if (rc != 0)
    {
        printf("ERROR: Could not resolve Twitch IRC server host: %d\n", rc);
        return NET_ERROR;
    }

    cs = INVALID_SOCKET;
    connected = false;

    credentials = auth_data;

    return NET_OK;
}

NetStatus UpdateNetworking(MsgQueue *rx_queue, MsgQueue *tx_queue)
{
    if (!connected)
    {
        bool connect_ok = false;
        addrinfo *addr_info = hint_results;
        while (addr_info != NULL && !connect_ok)
        {
            cs = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (cs == INVALID_SOCKET)
            {
                printf("ERROR: Failed to create socket: %d\n", WSAGetLastError());
                return NET_ERROR;
            }

            int rc = connect(cs, addr_info->ai_addr, (int)addr_info->ai_addrlen);
            addr_info = addr_info->ai_next;
            if (rc == SOCKET_ERROR)
            {
                closesocket(cs);
                cs = INVALID_SOCKET;
                continue;
            }
            connect_ok = true;
        }
        
        if (!connect_ok)
        {
            printf("WARNING: Failed to connect to Twitch IRC Server\n");
            printf("\tSocket errno %d\n", WSAGetLastError());
            return NET_CONNECT_FAILED;
        }
        printf("Connected to Twitch IRC server\n");

        sprintf(tx_buffer, "PASS %s\r\n", credentials.oauth.c_str());
        int length = strlen(tx_buffer);
        int rc = send(cs, tx_buffer, length, 0);
        if (rc != length)
        {
            printf("WARNING: Failed to send OAUTH to Twitch: %d\n", WSAGetLastError());
            closesocket(cs);
            return NET_CONNECT_FAILED;
        }

        this_thread::sleep_for(chrono::milliseconds(100));

        sprintf(tx_buffer, "NICK %s\r\n", credentials.nick.c_str());
        length = strlen(tx_buffer);
        rc = send(cs, tx_buffer, length, 0);
        if (rc != length)
        {
            printf("WARNING: Failed to send NICK to Twitch: %d\n", WSAGetLastError());
            closesocket(cs);
            return NET_CONNECT_FAILED;
        }
        printf("Successfully sent credentials to Twitch\n");

        this_thread::sleep_for(chrono::milliseconds(100));

        sprintf(tx_buffer, "JOIN #%s\r\n", credentials.channel.c_str());
        length = strlen(tx_buffer);
        rc = send(cs, tx_buffer, length, 0);
        if (rc != length)
        {
            printf("WARNING: Failed to send JOIN to Twitch: %d\n", WSAGetLastError());
            closesocket(cs);
            return NET_CONNECT_FAILED;
        }

        this_thread::sleep_for(chrono::milliseconds(100));

        sprintf_s(tx_buffer, 512, "PRIVMSG #%s :Chipsy is back online!\r\n", 
            credentials.channel.c_str());
        length = strlen(tx_buffer);
        rc = send(cs, tx_buffer, length, 0);
        if (rc != length)
        {
            printf("WARNING: Failed to send greeting to chat: %d\n", WSAGetLastError());
            closesocket(cs);
            return NET_CONNECT_FAILED;
        }
        connected = true;
    }

    FD_SET rx_set;
    FD_SET err_set;
    FD_ZERO(&rx_set);
    FD_ZERO(&err_set);
    FD_SET(cs, &rx_set);
    FD_SET(cs, &err_set);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    int rc = select(cs + 1, &rx_set, NULL, &err_set, &tv);
    if (rc == SOCKET_ERROR)
    {
        printf("WARNING: Socket select error %d\n", WSAGetLastError());
        closesocket(cs);
        cs = INVALID_SOCKET;
        connected = false;
        return NET_CONNECT_FAILED;
    }
    else if (rc == 0) 
    {
        return NET_OK;
    }

    if (FD_ISSET(cs, &err_set))
    {
        printf("WARNING: Connection failure with Twitch IRC server\n");
        closesocket(cs);
        cs = INVALID_SOCKET;
        connected = false;
        return NET_CONNECT_FAILED;
    }
    if (FD_ISSET(cs, &rx_set))
    {
        rc = recv(cs, rx_buffer, 1024, 0);
        if (rc == 0)
        {
            printf("Twitch disconnected socket...\n");
            closesocket(cs);
            cs = INVALID_SOCKET;
            connected = false;
            return NET_CONNECT_FAILED;
        }
        else if (rc < 0)
        {
            printf("WARNING: Socket receive error : %d\n", WSAGetLastError());
            closesocket(cs);
            cs = INVALID_SOCKET;
            connected = false;
            return NET_CONNECT_FAILED;
        }

        rx_buffer[rc] = 0;
        printf("%s", rx_buffer);
    }

    return NET_OK;
}

void StopNetworking()
{
    if (connected)
    {
        closesocket(cs);
        connected = false;
        cs = INVALID_SOCKET;
    }

    WSACleanup();
}