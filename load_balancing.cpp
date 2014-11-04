#include <my_config.h>
#include "testconnections.h"
#include "sql_t1.h"
#include "get_com_select_insert.h"

char sql[1000000];

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
int exit_flag = 0;
void *query_thread1( void *ptr );
void *query_thread2( void *ptr );


TestConnections * Test;

int main()
{

    Test = new TestConnections();
    int global_result = 0;

    int selects[256];
    int inserts[256];
    int new_selects[256];
    int new_inserts[256];


    Test->ReadEnv();
    Test->PrintIP();
    Test->repl->Connect();
    Test->ConnectRWSplit();

    // connect to the MaxScale server (rwsplit)

    if (Test->conn_rwsplit == NULL ) {
        printf("Can't connect to MaxScale\n");
        exit(1);
    } else {

        create_t1(Test->conn_rwsplit);

        create_insert_string(sql, 50000, 1);
        printf("sql_len=%lu\n", strlen(sql));
        global_result += execute_query(Test->conn_rwsplit, sql);
        // close connections
        Test->CloseRWSplit();

        printf("COM_INSERT and COM_SELECT before executing test\n");
        get_global_status_allnodes(&selects[0], &inserts[0], Test->repl, 0);

        pthread_t thread1;
        pthread_t thread2;
        //pthread_t check_thread;
        int  iret1;
        int  iret2;

        exit_flag=0;
        /* Create independent threads each of them will execute function */

        iret1 = pthread_create( &thread1, NULL, query_thread1, NULL);
        iret2 = pthread_create( &thread2, NULL, query_thread2, NULL);

        sleep(100);
        exit_flag = 1;

        printf("COM_INSERT and COM_SELECT after executing test\n");
        get_global_status_allnodes(&new_selects[0], &new_inserts[0], Test->repl, 0);

        print_delta(&new_selects[0], &new_inserts[0], &selects[0], &inserts[0], Test->repl->N);
    }
    Test->repl->CloseConn();

    exit(global_result);
}


void *query_thread1( void *ptr )
{
    MYSQL * conn;
    conn = Test->OpenRWSplitConn();
    while (exit_flag == 0) {
        execute_query(conn, (char *) "SELECT * FROM t1;");
    }
    mysql_close(conn);
    return NULL;
}

void *query_thread2( void *ptr )
{
    MYSQL * conn;
    conn = Test->OpenRWSplitConn();
    int i = 0;
    while (exit_flag == 0) {
        if (i > 100) {
            execute_query(conn, (char *) "SELECT * FROM t1;");
            i = 0;
        } else { i++;}
    }
    mysql_close(conn);
    return NULL;
}