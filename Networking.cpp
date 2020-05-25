
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


static SOCKET cs = INVALID_SOCKET;
static bool connected = false;

bool InitNetworking()
{
    WSADATA wsa_data;
    int rc = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (rc != 0)
    {
        printf("Failed to initialize winsock\n");
        return false;
    }

    cs = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (cs == INVALID_SOCKET)
    {
        printf("Failed to initialize client socket\n");
        return false;
    }
}

void UpdateNetworking(Client &client)
{
    DWORD opt_val;
    int opt_size = sizeof(opt_val);
    int rc = getsockopt(client.cs, SOL_SOCKET, SO_CONNECT_TIME, (char *)&opt_val, &opt_size);
    if (rc != 0)
    {
        // TODO: handle this.
    }


    
}