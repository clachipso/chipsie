
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

#include "jsmn.h"
#include "Networking.hpp"

#include <stdio.h>
#include <string>
#include <map>
#include <chrono>
#include <thread>
using namespace std;

static const char *DEF_AUTH_CFG_FILE = "auth/auth.json";
static const char *TWITCH_IRC_ADDR = "irc://irc.chat.twitch.tv";
static const uint16_t TWITCH_IRC_PORT = 6667;

struct AuthData
{
    string oauth;
    string client_id;
    string nick;
    string channel;
};

static AuthData auth_data;
static Client client;

bool LoadAuthCfg(const char *auth_cfg_file);

int main(const int argc, const char **argv)
{
    printf("Chipsy the Twitch Chat Bot Starting Up...\n");

    if (!LoadAuthCfg(DEF_AUTH_CFG_FILE))
    {
        printf("Failed to load authorization credentials from file :(\n");
        return -1;
    }

    if (!InitNetworking())
    {
        printf("Failed to intiailze networking\r\n");
        return -1;
    }


    while (true)
    {
        UpdateNetworking(client);
    }


    printf("Chipsy the Twitch Chat Bot Shutting Down...Bye Bye!\n");
    return 0;
}

bool LoadAuthCfg(const char *auth_cfg_file)
{
    FILE *auth_file = fopen(auth_cfg_file, "r");
    if (auth_file == NULL) 
    {
        printf("Failed to open auth config file\n");
        return false;
    }

    fseek(auth_file, 0, SEEK_END);
    long file_len = ftell(auth_file);
    if (file_len <= 0) 
    {
        printf("Invalid auth credential file length\n");
        return false;
    }
    fseek(auth_file, 0, SEEK_SET);

    char *file_str = (char *)malloc(file_len);
    fread(file_str, 1, file_len, auth_file);
    fclose(auth_file);
    printf("Read in auth credential file of %d bytes\n", file_len);

    jsmntok tokens[12];
    jsmn_parser parser;
    jsmn_init(&parser);
    int rc = jsmn_parse(&parser, file_str, (size_t)file_len, tokens, 12);
    if (rc <= 0) 
    {
        printf("Wanted 8 json tokens, got %d tokens\n", rc);
        free(file_str);
        return false;
    }

    bool got_oauth = false;
    bool got_client_id = false;
    bool got_nick = false;
    bool got_channel = false;
    int tok_it = 0;
    while (tok_it < rc)
    {
        string key = string(&file_str[tokens[tok_it].start], 
            tokens[tok_it].end - tokens[tok_it].start);

        if (key == "token")
        {
            tok_it++;
            auth_data.oauth = string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            got_oauth = true;
        }
        else if (key == "client_id")
        {
            tok_it++;
            auth_data.client_id = string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            got_client_id = true;
        }
        else if (key == "nick")
        {
            tok_it++;
            auth_data.nick = string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            got_nick = true;
        }
        else if (key == "channel")
        {
            tok_it++;
            auth_data.channel = string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            got_channel = true;
        }
        tok_it++;
    }

    free(file_str);
    if (got_oauth && got_client_id && got_nick && got_channel)
    {
        printf("Loaded auth credentials successfully! 8D\n");
        return true;
    }
    else
    {
        printf("Got oauth %d client_id %d nick %d channel %d\n", got_oauth, 
            got_client_id, got_nick, got_channel);
    }
    return false;
}