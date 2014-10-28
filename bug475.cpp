#include <my_config.h>
#include <iostream>
#include <unistd.h>
#include "testconnections.h"

using namespace std;

int main()
{

    TestConnections * Test = new TestConnections();
    int global_result = 0;

    Test->ReadEnv();
    Test->PrintIP();
    Test->ConnectMaxscale();

    global_result += execute_query(Test->conn_rwsplit, (char *) "select /* maxscale hintname prepare route to master */ @@server_id;");
    global_result += execute_query(Test->conn_rwsplit, (char *) "select /* maxscale hintname begin */ @@server_id;");
    global_result += execute_query(Test->conn_rwsplit, (char *) "select /* maxscale route to master*/ @@server_id;");

    global_result += CheckLogErr((char *) "Error : Syntax error in hint", FALSE);
    global_result += CheckMaxscaleAlive();
    return(global_result);
}