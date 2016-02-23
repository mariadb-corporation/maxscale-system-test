/**
 * @file mxs37_table_privilege.cpp mxs37 (bug719) regression case ("mandatory SELECT privilege on db level?")
 *
 */


#include <my_config.h>
#include <iostream>
#include <unistd.h>
#include "testconnections.h"
#include "sql_t1.h"

using namespace std;

int main(int argc, char *argv[])
{
    TestConnections * Test = new TestConnections(argc, argv);
    Test->set_timeout(20);

    Test->connect_maxscale();

    Test->tprintf("Create t1\n");
    create_t1(Test->conn_rwsplit);
    Test->tprintf("Create user 'table_privilege'\n");
    execute_query_silent(Test->conn_rwsplit, "DROP USER table_privilege");
    execute_query(Test->conn_rwsplit, (char *) "CREATE USER table_privilege");
    Test->tprintf("Create user with only SELECT priviledge\n");
    execute_query(Test->conn_rwsplit, (char *) "GRANT SELECT ON test.t1 TO 'table_privilege'@'%' IDENTIFIED BY 'pass'");
    execute_query(Test->conn_rwsplit, (char *) "flush privileges"); // doeas it work with Maxscale?
    // should this sleep be removed?
    sleep(5);
    Test->tprintf("Trying to connect using this user\n");
    MYSQL * conn = open_conn_db(Test->rwsplit_port, Test->maxscale_IP, (char *) "test", (char *) "table_privilege", (char *) "pass", Test->ssl);
    if (mysql_errno(conn) != 0)
    {
        Test->add_result(1, "%s\n", mysql_error(conn));
    }
    //sleep(5);
    Test->tprintf("Trying SELECT\n");
    //Test->try_query(conn, (char *) "USE test");
    Test->try_query(conn, (char *) "SELECT * FROM t1");

    Test->tprintf("DROP USER\n");
    Test->try_query(Test->conn_rwsplit, "DROP USER table_privilege");
    Test->try_query(Test->conn_rwsplit, "DROP TABLE t1");

    Test->check_maxscale_alive();
    Test->copy_all_logs(); return(Test->global_result);
}
