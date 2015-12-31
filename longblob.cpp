/**
 * @file longblob.cpp - trying to use LONGBLOD
 */

#include <my_config.h>
#include "testconnections.h"

int main(int argc, char *argv[])
{
    TestConnections * Test = new TestConnections(argc, argv);
    Test->set_timeout(60);
    long int size = 100000;
    long int * data;
    long int i;
    MYSQL_BIND param[1];

    char *insert_stmt = (char *) "INSERT INTO long_blob_table(x, b) VALUES(1, ?)";

    Test->connect_maxscale();

    execute_query(Test->conn_rwsplit, (char *) "DROP TABLE IF EXISTS long_blob_table");
    execute_query(Test->conn_rwsplit, (char *) "CREATE TABLE long_blob_table(x INT, b LONGBLOB");

    MYSQL_STMT * stmt = mysql_stmt_init(Test->conn_rwsplit);

    mysql_stmt_prepare(stmt, insert_stmt, strlen(insert_stmt));

    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].is_null = 0;

    mysql_stmt_bind_param(stmt, param);

    data = (long int *) malloc(size * sizeof(long int));

    for (i = 0; i < size; i++)
    {
        data[i] = i;
    }


    for (i = 0; i < 1000; i++) {
        mysql_stmt_send_long_data(stmt, 0, (char *) data, sizeof(data));
    }

    Test->add_result(mysql_stmt_execute(stmt), mysql_stmt_error(stmt));

    Test->copy_all_logs(); return(Test->global_result);
}
