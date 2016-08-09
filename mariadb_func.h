#ifndef MARIADB_FUNC_H
#define MARIADB_FUNC_H


/**
 * @file mariadb_func.h - basic DB interaction routines
 *
 * @verbatim
 * Revision History
 *
 * Date		Who		Description
 * 17/11/14	Timofey Turenko	Initial implementation
 *
 * @endverbatim
 */



#include <my_config.h>
#include <my_global.h>
#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

MYSQL * open_conn_db_flags(int port, const char* ip, const char* db, const char* User, const char* Password, unsigned long flag, bool ssl);
MYSQL * open_conn_db_timeout(int port, const char* ip, const char* db, const char* User, const char* Password, unsigned long timeout, bool ssl);
MYSQL * open_conn_db(int port, const char* ip, const char* db, const char* User, const char* Password, bool ssl);
MYSQL * open_conn(int port, const char* ip, const char* User, const char* Password, bool ssl);
MYSQL * open_conn_no_db(int port, const char* ip, const char* User, const char* Password, bool ssl);
int set_ssl(MYSQL * conn);
int execute_query(MYSQL *conn, const char *sql);
int execute_query_silent(MYSQL *conn, const char *sql);
int execute_query1(MYSQL *conn, const char *sql, bool silent);
int execute_query_affected_rows(MYSQL *conn, const char *sql, my_ulonglong * affected_rows);
int execute_query_check_one(MYSQL *conn, const char *sql, const char *expected);
int get_conn_num(MYSQL *conn, char * ip, char * hostname, char * db);
int find_field(MYSQL *conn, const char * sql, const char * field_name, char * value);
unsigned int get_seconds_behind_master(MYSQL *conn);
int read_log(char * name, char **err_log_content_p);

#endif // MARIADB_FUNC_H
