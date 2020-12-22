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

#include "Database.hpp"
#include <stdio.h>
#include <string>
using namespace std;

bool Database::Init(const char *db_file) {
    int rc = sqlite3_open(db_file, &db);
    if (rc != SQLITE_OK) {
        printf("Failed to open db %s: %d\n", db_file, rc);
        return false;
    }
    if (db == NULL) {
        printf("Failed to allocate db\n");
        return false;
    }

    if (!TableExists("admins")) {
        sqlite3_stmt *stmt = NULL;
        bool success = false;
        string sqlstr = "CREATE TABLE admins (name TEXT)";
        rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &stmt,
            NULL);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE) {
                printf("Database admins table created...\n");
                success = true;
            }
            else {
                printf("Failed to create admins table %d\n", rc);
            }
        } else {
            printf("Failed to create admins table %d\n", rc);
        }
        sqlite3_finalize(stmt);
        if (!success) return false;
    }

    if (!TableExists("commands")) {
        sqlite3_stmt *stmt = NULL;
        bool success = false;
        string sqlstr = "CREATE TABLE commands (name TEXT, response TEXT)";
        rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &stmt,
            NULL);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE) {
                printf("Database commands table created...\n");
                success = true;
            }
            else {
                printf("Failed to create commands table %d\n", rc);
            }
        } else {
            printf("Failed to create commands table %d\n", rc);
        }
        sqlite3_finalize(stmt);
        if (!success) return false;
    }

    if (!TableExists("motd")) {
        sqlite3_stmt *stmt = NULL;
        bool success = false;
        string sqlstr = "CREATE TABLE motd ";
        sqlstr += "(motd TEXT, rate INTEGER, enabled BOOL)";
        rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &stmt,
            NULL);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE) {
                printf("Database motd table created...\n");
                success = true;
            }
            else {
                printf("Failed to create motd table %d\n", rc);
            }
        } else {
            printf("Failed to create motd table %d\n", rc);
        }
        sqlite3_finalize(stmt);
        if (!success) return false;

        sqlite3_stmt *motd_create = NULL;
        success = false;
        sqlstr = "INSERT INTO motd (rate, enabled) VALUES (20, 0)";
        rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), 
            &motd_create, NULL);
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(motd_create);
            if (rc == SQLITE_DONE) {
                printf("Added empty motd\n");
                success = true;
            } 
        }
        sqlite3_finalize(motd_create);
        if (!success) {
            printf("ERROR: Failed to create basic motd\n");
            return false;
        }
    }
    
    return true;
}

void Database::AddAdmin(const std::string &admin) {
    if (IsAdmin(admin)) {
        printf("WARN: Attempted to re-add admin to DB\n");
        return;
    }
    sqlite3_stmt *stmt = NULL;
    string sqlstr = "INSERT INTO admins (name) VALUES (\'";
    sqlstr += admin;
    sqlstr += "\');";
    int rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), 
        &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to insert admin into DB: %d\n", rc);
    } else {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            printf("Failed to insert admin into DB: %d\n", rc);
        }
    }
    sqlite3_finalize(stmt);
}

void Database::RemAdmin(const std::string &admin) {
    sqlite3_stmt *stmt = NULL;
    string sqlstr = "DELETE FROM admins WHERE name=\'";
    sqlstr += admin;
    sqlstr += "\';";
    int rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &stmt,
        NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to create admin delete statement %d\n", rc);
    } else {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            printf("Failed to delete admin from DB: %d\n", rc);
        }
    }
    sqlite3_finalize(stmt);
}

bool Database::IsAdmin(const std::string &name) {
    bool is_admin = false;
    sqlite3_stmt *stmt = NULL;
    string sqlstr = "SELECT * FROM admins WHERE name = \'";
    sqlstr += name;
    sqlstr += "\'";
    int rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &stmt,
        NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to create admin check statement %d\n", rc);
    } else {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {// If a row was returned, player was in table
            is_admin = true;
        }
    }
    sqlite3_finalize(stmt);
    return is_admin;
}

void Database::AddCmd(const std::string &name, const std::string &response) {
    // Need to double up backticks in response to make SQL happy
    string mod_resp = response;
    size_t cursor = mod_resp.find('\'');
    while (cursor != string::npos) {
        mod_resp.insert(cursor, 1, '\'');
        cursor = mod_resp.find('\'', cursor + 2);
    }

    sqlite3_stmt *stmt = NULL;
    string sqlstr = "INSERT INTO commands (name, response) VALUES ";
    sqlstr += "(\'" + name + "\', \'" + mod_resp + "\')";
    int rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), 
        &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to insert command into DB: %d\n", rc);
    } else {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            printf("Failed to insert command into DB: %d\n", rc);
        }
    }
    sqlite3_finalize(stmt);
}

void Database::RemCmd(const std::string &name) {
    sqlite3_stmt *stmt = NULL;
    string sqlstr = "DELETE FROM commands WHERE name=\'";
    sqlstr += name;
    sqlstr += "\';";
    int rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &stmt,
        NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to create command delete statement %d\n", rc);
    } else {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            printf("Failed to delete command from DB: %d\n", rc);
        }
    }
    sqlite3_finalize(stmt);
}

bool Database::CmdExists(const std::string &name) {
    bool cmd_exists = false;
    sqlite3_stmt *stmt = NULL;
    string sqlstr = "SELECT * FROM commands WHERE name = \'";
    sqlstr += name;
    sqlstr += "\'";
    int rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &stmt,
        NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to create command check statement %d\n", rc);
    } else {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            cmd_exists = true;
        }
    }
    sqlite3_finalize(stmt);
    return cmd_exists;
}

void Database::GetCmdResp(const std::string &name, std::string *out_resp) {
    sqlite3_stmt *stmt = NULL;
    string sqlstr = "SELECT * FROM commands WHERE name = \'";
    sqlstr += name;
    sqlstr += "\'";
    int rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &stmt,
        NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to create get command response statement %d\n", rc);
    } else {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            *out_resp = (const char *)sqlite3_column_text(stmt, 1);
        } else {
            printf("Failed to get command response from command table\n");
        }
    }
    sqlite3_finalize(stmt);
}

bool Database::TableExists(const char *table_name) {
    string sqlstr = "SELECT count(*) FROM sqlite_master WHERE type = 'table' ";
    sqlstr += "AND name ='";
    sqlstr += table_name;
    sqlstr += "'";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sqlstr.c_str(), (int)sqlstr.length(), &stmt, 
        NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to query %s table existence %d\n", table_name, rc);
        return false;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        printf("Failed to query %s table existence\n", table_name);
        return false;
    }
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    if (count == 0) return false;
    return true;
}
