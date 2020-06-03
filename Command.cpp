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

#include "Command.hpp"
#include <stdlib.h>
#include <time.h>
#include "sqlite3.h"

const int QUERY_BUFF_SIZE = 1024;

static sqlite3 *db;
static sqlite3_stmt *sqlstmt;
static char query_buff[QUERY_BUFF_SIZE];
static string cb_string;

void HandlePrivateMsg(const IrcMessage &msg, MsgQueue *tx_queue);
void HandleUserCmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd);
static int DynCmdCallback(void *data, int argc, char **argv, char **az_col_name);

bool InitCmdProcessing(const char *db_path)
{
    srand((unsigned int)time(NULL));

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK)
    {
        printf("Failed to open db: %d\n", rc);
        return false;
    }
    printf("Opened database file %s\n", db_path);

    // This may be the first time the application is run with the specified
    // database, we need to ensure that the database has been initialized.
    string sqlstr = "SELECT count(*) FROM sqlite_master WHERE type ='table' ";
    sqlstr += "AND name ='operators'";
    rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &sqlstmt, 
        NULL);
    if (rc != SQLITE_OK)
    {
        printf("ERROR: Failed to query operators table existence %d\n", 
            rc);
        return false;
    }
    rc = sqlite3_step(sqlstmt);
    if (rc != SQLITE_ROW)
    {
        printf("ERROR: Failed to query operators table existence\n");
        return false;
    }
    int count = sqlite3_column_int(sqlstmt, 0);
    if (count == 0)
    {
        // Table doesn't exist
        sqlstr = "CREATE TABLE operators ";
        sqlstr += "(id int NOT NULL PRIMARY KEY, name text)";

        sqlite3_finalize(sqlstmt);
        rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), 
            &sqlstmt, NULL);
        if (rc != SQLITE_OK) return false;
        rc = sqlite3_step(sqlstmt);
        if (rc != SQLITE_DONE) return false;
        printf("Database operators table created...\n");
    }
    else 
    {
        printf("Verified database has operators table\n");
    }

    sqlstr = "SELECT count(*) FROM sqlite_master WHERE type ='table' AND ";
    sqlstr += "name ='static_cmds'";
    rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &sqlstmt, 
        NULL);
    if (rc != SQLITE_OK)
    {
        printf("ERROR: Failed to check if static command table exists %d\n", 
            rc);
        return false;
    }
    rc = sqlite3_step(sqlstmt);
    if (rc != SQLITE_ROW)
    {
        printf("ERROR: Failed to query static_cmd table existence\n");
        return false;
    }
    count = sqlite3_column_int(sqlstmt, 0);
    if (count == 0)
    {
        // Table doesn't exist
        sqlstr = "CREATE TABLE static_cmds ";
        sqlstr += "(id int NOT NULL PRIMARY KEY, cmd text, response text)";

        sqlite3_finalize(sqlstmt);
        rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), 
            &sqlstmt, NULL);
        if (rc != SQLITE_OK) return false;
        rc = sqlite3_step(sqlstmt);
        if (rc != SQLITE_DONE) return false;
        printf("Database static_cmds table created...\n");
    }
    else 
    {
        printf("Verified database has static_cmds table\n");
    }



    return true;
}

bool ConvertLineToMsg(const string &line, IrcMessage *msg)
{
    size_t cursor = 0;
    while ((line[cursor] == ' ' || line[cursor] == '\t')) 
    {
        cursor++;
        if (cursor >= line.size()) return false;
    }
    
    if (line[cursor] == '@') // Line has tags
    {
        size_t end = line.find(' ', cursor);
        cursor++;
        msg->tags = line.substr(cursor, end - cursor);
        cursor = end;
        while ((line[cursor] == ' ' || line[cursor] == '\t')) 
        {
            cursor++;
            if (cursor >= line.size()) return false;
        }
    }
    else
    {
        msg->tags = "";
    }

    if (line[cursor] == ':')
    {
        size_t end = line.find(' ', cursor);
        cursor++;
        msg->source = line.substr(cursor, end - cursor);
        cursor = end;
        while ((line[cursor] == ' ' || line[cursor] == '\t')) 
        {
            cursor++;
            if (cursor >= line.size()) return false;
        }
    }
    else
    {
        msg->source = "";
    }
    

    size_t end = line.find(' ', cursor);
    msg->command = line.substr(cursor, end - cursor);
    cursor = end;
    while (line[cursor] == ' ' || line[cursor] == '\t') 
    {
        cursor++;
        if (cursor >= line.size())
        {
            msg->parameters = "";
            return true;
        }
    }

    msg->parameters = line.substr(cursor, line.size() - cursor);
    return true;
}

void ProcessMsg(const IrcMessage &msg, MsgQueue *tx_queue)
{
    if (msg.command == "PRIVMSG") // Private message
    {
        HandlePrivateMsg(msg, tx_queue);
    }
    else if (msg.command == "PING") // Ping message
    {
        // Always reply with a pong so we don't get booted
        string line = "PONG " + msg.parameters;
        tx_queue->push(line);
    }
    else if (msg.command == "JOIN") // Join command reply
    {

    }
    else if (msg.command == "001") // Welcome message
    {

    }
    else if (msg.command == "002") // Server host ID
    {

    }
    else if (msg.command == "003") // Server creation time
    {

    }
    else if (msg.command == "004") // User info, not currently used by Twitch
    {

    }
    else if (msg.command == "353") // NAME command reply
    {

    }
    else if (msg.command == "366") // End of NAME list reply
    {

    }
    else if (msg.command == "375") // Message of the day start
    {

    }
    else if (msg.command == "372") // Message of the day line
    {
        
    }
    else if (msg.command == "376") // Message of the day end
    {

    }
    else
    {
        printf("ALERT: Unknown command %s\n", msg.command.c_str());
    }
}

void HandlePrivateMsg(const IrcMessage &msg, MsgQueue *tx_queue)
{
    size_t cursor = 0;
    while (msg.parameters[cursor] == ' ' || msg.parameters[cursor] == '\t')
    {
        cursor++;
        if (cursor >= msg.parameters.length())
        {
            cursor = msg.parameters.length() - 1;
            break;
        }
    }
    if (msg.parameters[cursor] != '#')
    {
        printf("WARNING: Received malformed PRIVMSG command\n");
        return;
    }
    size_t end = msg.parameters.find(' ', cursor);
    cursor++;
    string channel = msg.parameters.substr(cursor, end - cursor);
    cursor = end + 1;

    cursor = msg.parameters.find(':', cursor);
    if (cursor == string::npos)
    {
        printf("WARNING: Received PRIVMSG with no text\n");
        return;
    }
    cursor++;
    string priv_msg = msg.parameters.substr(cursor);
    
    // Is the sender trying to issue a command?
    cursor = 0;
    while (priv_msg[cursor] == ' ' || priv_msg[cursor] == '\t')
    {
        cursor++;
        if (cursor >= priv_msg.length())
        {
            cursor = priv_msg.length() - 1;
            break;
        }
    }
    if (priv_msg[cursor] == '!')
    {
        // User is attempting to send command. Extract it
        cursor++;
        end = cursor;
        while (priv_msg[end] != ' ' && priv_msg[end] != '\t')
        {
            end++;
            if (end >= priv_msg.length()) 
            {
                end = priv_msg.length();
                break;
            }
        }
        string user_cmd = priv_msg.substr(cursor, end - cursor);
        HandleUserCmd(msg, tx_queue, channel, user_cmd);
        
    }
    else
    {
        
    }
}

void HandleUserCmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd)
{
    if (cmd == "dice")
    {
        int d6 = rand() % 6 + 1;
        string resp = "PRIVMSG #" + chan + " :You rolled a " + to_string(d6);
        tx_queue->push(resp);
    }
    else
    {
        // No dynamic command found. Check database for a static command
        sprintf_s(query_buff, QUERY_BUFF_SIZE, 
            "SELECT * FROM static_cmds WHERE cmd = \"%s\"", cmd.c_str());
        cb_string = "";

        char *err_msg = 0;
        int rc = sqlite3_exec(db, query_buff, DynCmdCallback, NULL, &err_msg);
        if (rc != SQLITE_OK)
        {
            printf("WARNING: Failed SQLite query %d: %s\n", rc, err_msg);
            sqlite3_free(err_msg);
            return;
        }
        if (!cb_string.empty())
        {
            string resp = "PRIVMSG #" + chan + " :" + cb_string;
            tx_queue->push(resp);
        }
        else
        {
            printf("ALERT: Unable to find command %s\n", cmd.c_str());
        }
    }
}

static int DynCmdCallback(void *data, int argc, char **argv, char **az_col_name)
{
    cb_string = string(argv[2]);
    return 0;
}