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
#include <winsock2.h>
#include <thread>
#include <chrono>
using namespace std;

static const char *TWITCH_IRC_ADDR = "irc.chat.twitch.tv";
static const char *TWITCH_IRC_PORT = "6667";
static const int TX_BUFFER_SIZE = 2048;
static const int RX_BUFFER_SIZE = 2048;
static const int LINE_BUFFER_SIZE = 2048;

static SOCKET cs;
static bool connected;
static char tx_buffer[TX_BUFFER_SIZE];
static char rx_buffer[RX_BUFFER_SIZE];
static AuthData credentials;
static struct addrinfo *hint_results = NULL;

static char line_buffer[LINE_BUFFER_SIZE];
static int line_length;

void CloseSocket();

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

    memset(line_buffer, 0, LINE_BUFFER_SIZE);
    line_length = 0;

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
                printf("ERROR: Failed to create socket: %d\n", 
                    WSAGetLastError());
                return NET_ERROR;
            }

            int rc = connect(cs, addr_info->ai_addr, 
                (int)addr_info->ai_addrlen);
            addr_info = addr_info->ai_next;
            if (rc == SOCKET_ERROR)
            {
                CloseSocket();
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

        sprintf_s(tx_buffer, TX_BUFFER_SIZE, "PASS %s\r\n", 
            credentials.oauth.c_str());
        int length = (int)strlen(tx_buffer);
        int rc = send(cs, tx_buffer, length, 0);
        if (rc != length)
        {
            printf("WARNING: Failed to send OAUTH to Twitch: %d\n", 
                WSAGetLastError());
            CloseSocket();
            return NET_CONNECT_FAILED;
        }

        this_thread::sleep_for(chrono::milliseconds(100));

        sprintf_s(tx_buffer, TX_BUFFER_SIZE, "NICK %s\r\n", 
            credentials.nick.c_str());
        length = (int)strlen(tx_buffer);
        rc = send(cs, tx_buffer, length, 0);
        if (rc != length)
        {
            printf("WARNING: Failed to send NICK to Twitch: %d\n", 
                WSAGetLastError());
            CloseSocket();
            return NET_CONNECT_FAILED;
        }
        printf("Successfully sent credentials to Twitch\n");

        this_thread::sleep_for(chrono::milliseconds(100));

        sprintf_s(tx_buffer, TX_BUFFER_SIZE, "JOIN #%s\r\n", 
            credentials.channel.c_str());
        length = (int)strlen(tx_buffer);
        rc = send(cs, tx_buffer, length, 0);
        if (rc != length)
        {
            printf("WARNING: Failed to send JOIN to Twitch: %d\n", 
                WSAGetLastError());
            CloseSocket();
            return NET_CONNECT_FAILED;
        }
        connected = true;
        line_length = 0;
        while (rx_queue->size() > 0) rx_queue->pop();
        while (tx_queue->size() > 0) tx_queue->pop();

        string resp = "PRIVMSG #" + credentials.channel + " :@" + 
            credentials.channel + " Have no fear, I is here! :D";
        tx_queue->push(resp);
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
    int rc = select((int)cs + 1, &rx_set, NULL, &err_set, &tv);
    if (rc == SOCKET_ERROR)
    {
        printf("WARNING: Socket select error %d\n", WSAGetLastError());
        CloseSocket();
        return NET_CONNECT_FAILED;
    }
    if (rc != 0 && FD_ISSET(cs, &err_set))
    {
        printf("WARNING: Connection failure with Twitch IRC server\n");
        CloseSocket();
        return NET_CONNECT_FAILED;
    }
    if (rc != 0 && FD_ISSET(cs, &rx_set))
    {
        rc = recv(cs, rx_buffer, RX_BUFFER_SIZE, 0);
        if (rc == 0)
        {
            printf("Twitch disconnected socket...\n");
            CloseSocket();
            return NET_CONNECT_FAILED;
        }
        else if (rc < 0)
        {
            printf("WARNING: Socket receive error : %d\n", WSAGetLastError());
            CloseSocket();
            return NET_CONNECT_FAILED;
        }

        for (int i = 0; i < rc; i++)
        {
            if (rx_buffer[i] == '\r')
            {
                if (rx_buffer[i + 1] == '\n')
                {
                    if (line_length == 0) continue;

                    i++;
                    line_buffer[line_length] = 0;
                    string line = string(line_buffer);
                    rx_queue->push(line);
                    line_length = 0;
                    printf("> %s\n", line.c_str());
                    continue;
                }
            }
            line_buffer[line_length] = rx_buffer[i];
            line_length++;
            
            if (line_length == LINE_BUFFER_SIZE)
            {
                printf("WARNING: Twitch violated line buffer length\n");
                printf("Reconnecting...\n");
                CloseSocket();
                break;
            }
        }
    }

    if (connected && tx_queue->size() > 0)
    {
        string line = tx_queue->front();
        tx_queue->pop();

        if (line.size() >= (TX_BUFFER_SIZE - 3))
        {
            printf("WARNING: Dropped msg that exceeded max length\n");
        }
        else
        {
            printf("< %s\n", line.c_str());
            sprintf_s(tx_buffer, TX_BUFFER_SIZE, "%s\r\n", line.c_str());
            int length = (int)strlen(tx_buffer);
            int bytes_sent = 0;
            while (bytes_sent < length)
            {
                int rc = send(cs, &tx_buffer[bytes_sent], length - bytes_sent, 
                    0);
                if (rc == 0)
                {
                    printf("Twitch disconnected socket...\n");
                    CloseSocket();
                    return NET_CONNECT_FAILED;
                }
                if (rc < 0)
                {
                    printf("WARNING: Failed to send msg to Twitch: %d\n", 
                        WSAGetLastError());
                    CloseSocket();
                    return NET_CONNECT_FAILED;
                }
                bytes_sent += rc;
            }
        }
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

void CloseSocket()
{
    closesocket(cs);
    cs = INVALID_SOCKET;
    connected = false;
}