/**
 * @file mxs710_bad_socket.cpp mxs710_bad_socket regression case (Maxscale does not startup properly and crashes after trying to login to database)
 * - try to start maxscale with "socket=/var/lib/mysqld/mysql.sock" in the listener definition
 * - do not expect crash
 * - try the same with two listers for one service, one of them uses unreachable port
 */


#include <my_config.h>
#include <iostream>
#include <unistd.h>
#include "testconnections.h"

using namespace std;

int main(int argc, char *argv[])
{
    TestConnections * Test = new TestConnections(argc, argv);
    Test->set_timeout(10);

    Test->check_log_err((char *) "Unsupported router option \"slave\"", TRUE);

    Test->check_maxscale_alive();
    Test->copy_all_logs(); return(Test->global_result);
}
