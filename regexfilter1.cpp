/**
 * @file regexfilter1.cpp Simple regexfilter tests
 *
 * Three services are configured with regexfilter, each with different parameters.
 * All services are queried with SELECT 123. The first service should replace it
 * with SELECT 0 and the second and third services should not replace it.
 */


#include <my_config.h>
#include <iostream>
#include "testconnections.h"

int main(int argc, char *argv[])
{
    int rval = 0;
    TestConnections * test = new TestConnections(argc, argv);
    test->connect_maxscale();
    if ((rval += execute_query_check_one(test->conn_rwsplit, "SELECT 123", "0")))
    {
        test->tprintf("FAILED: Query to first service should have replaced the query.");
    }
    if ((rval += execute_query_check_one(test->conn_slave, "SELECT 123", "123")))
    {
        test->tprintf("FAILED: Query to second service should not have replaced the query.");
    }
    if ((rval += execute_query_check_one(test->conn_master, "SELECT 123", "123")))
    {
        test->tprintf("FAILED: Query to third service should not have replaced the query.");
    }
    test->close_maxscale_connections();
    test->copy_all_logs();
    return rval;
}