// some relations to bug#425
// connect to no-existing DB

#include <my_config.h>
#include <iostream>
#include "testconnections.h"
#include "sql_t1.h"

using namespace std;

int main()
{

    TestConnections * Test = new TestConnections();
    int global_result = 0;

    Test->ReadEnv();
    Test->PrintIP();

    printf("Connecting to RWSplit\n");
    Test->conn_rwsplit = open_conn_no_db(Test->rwsplit_port, Test->Maxscale_IP);
    printf("Removing 'test' DB\n");
    execute_query(Test->conn_rwsplit, (char *) "DROP DATABASE IF EXISTS test;");
    printf("Closing connections and waiting 5 seconds\n");
    Test->CloseRWSplit();
    sleep(5);

    printf("Connection to non-existing DB (all routers)\n");
    global_result += Test->ConnectMaxscale();

    Test->CloseMaxscaleConn();

    Test->conn_rwsplit = open_conn_no_db(Test->rwsplit_port, Test->Maxscale_IP);

    printf("Creating and selecting 'test' DB\n");
    global_result += execute_query(Test->conn_rwsplit, (char *) "CREATE DATABASE test; USE test");
    printf("Creating 't1' table\n");
    global_result += create_t1(Test->conn_rwsplit);
    Test->CloseRWSplit();

    printf("Reconnectiong\n");
    global_result += Test->ConnectMaxscale();
    printf("Trying simple operations with t1 \n");
    global_result += execute_query(Test->conn_rwsplit, (char *) "INSERT INTO t1 (x1, fl) VALUES(0, 1);");
    global_result += execute_select_query_and_check(Test->conn_rwsplit, (char *) "SELECT * FROM t1;", 1);

    Test->CloseMaxscaleConn();

    return(global_result);
}