/**
 * @file mxs716.cpp Test for MXS-716
 *
 * Connect using different default databases with database and table level grants.
 */


#include <my_config.h>
#include <iostream>
#include <unistd.h>
#include "testconnections.h"
#include "sql_t1.h"

void run_test(TestConnections* Test, const char* database)
{

    Test->set_timeout(20);
    Test->tprintf("Trying to connect using 'table_privilege'@'%' to database 'test'");

    MYSQL* conn = open_conn_db(Test->rwsplit_port, Test->maxscale_IP, "test", "table_privilege", "pass", Test->ssl);

    if (conn && mysql_errno(conn) == 0)
    {
        Test->set_timeout(20);
        Test->tprintf("Trying SELECT on %s.t1", database);
        Test->try_query(conn, "SELECT * FROM t1");
    }
    else
    {
        Test->add_result(1, "Failed to connect using database '%s': %s", database, mysql_error(conn));   
    }

    mysql_close(conn);
}

int main(int argc, char *argv[])
{
    TestConnections* Test = new TestConnections(argc, argv);

    Test->connect_maxscale();
    Test->tprintf("Preparing test");
    Test->set_timeout(120);
    execute_query(Test->conn_rwsplit, "CREATE USER 'table_privilege'@'%' IDENTIFIED BY 'pass'");
    execute_query(Test->conn_rwsplit, "CREATE DATABASE db1");
    execute_query(Test->conn_rwsplit, "CREATE DATABASE db2");
    execute_query(Test->conn_rwsplit, "CREATE DATABASE db3");
    execute_query(Test->conn_rwsplit, "CREATE DATABASE db4");
    execute_query(Test->conn_rwsplit, "CREATE TABLE db1.t1 (id INT)");
    execute_query(Test->conn_rwsplit, "CREATE TABLE db2.t1 (id INT)");
    execute_query(Test->conn_rwsplit, "CREATE TABLE db3.t1 (id INT)");
    execute_query(Test->conn_rwsplit, "CREATE TABLE db4.t1 (id INT)");
    execute_query(Test->conn_rwsplit, "GRANT SELECT ON db1.* TO 'table_privilege'@'%'");
    execute_query(Test->conn_rwsplit, "GRANT SELECT ON db2.* TO 'table_privilege'@'%'");
    execute_query(Test->conn_rwsplit, "GRANT SELECT ON db3.t1 TO 'table_privilege'@'%'");
    execute_query(Test->conn_rwsplit, "GRANT SELECT ON db4.t1 TO 'table_privilege'@'%'");
    Test->repl->sync_slaves();

    run_test(Test, "db1");
    run_test(Test, "db2");
    run_test(Test, "db3");
    run_test(Test, "db4");
    Test->check_maxscale_alive();

    Test->tprintf("Cleaning up...");
    Test->set_timeout(60);
    Test->try_query(Test->conn_rwsplit, "DROP USER 'table_privilege'@'%'");
    Test->try_query(Test->conn_rwsplit, "DROP DATABASE db1");
    Test->try_query(Test->conn_rwsplit, "DROP DATABASE db2");
    Test->try_query(Test->conn_rwsplit, "DROP DATABASE db3");
    Test->try_query(Test->conn_rwsplit, "DROP DATABASE db4");

    Test->copy_all_logs();
    return Test->global_result;
}

