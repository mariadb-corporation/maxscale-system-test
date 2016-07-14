#include "testconnections.h"
#include "sql_t1.h"
#include <getopt.h>
#include <time.h>
#include <libgen.h>
#include "maxadmin_operations.h"
#include "templates.h"

TestConnections::TestConnections(int argc, char *argv[])
{
    //char str[1024];
    gettimeofday(&start_time, NULL);
    galera = new Mariadb_nodes((char *)"galera");
    repl   = new Mariadb_nodes((char *)"repl");

    test_name = basename(argv[0]);

    rwsplit_port = 4006;
    readconn_master_port = 4008;
    readconn_slave_port = 4009;
    ports[0] = rwsplit_port;
    ports[1] = readconn_master_port;
    ports[2] = readconn_slave_port;

    binlog_port = 5306;

    global_result = 0;

    read_env();

    char short_path[1024];
    strcpy(short_path, dirname(argv[0]));
    realpath(short_path, test_dir);
    printf("test_dir is %s\n", test_dir);
    strcpy(repl->test_dir, test_dir);
    strcpy(galera->test_dir, test_dir);
    sprintf(get_logs_command, "%s/get_logs.sh", test_dir);

    no_maxscale_stop = false;
    no_maxscale_start = false;
    //no_nodes_check = false;

    int c;
    bool run_flag = true;

    while (run_flag)
    {
        static struct option long_options[] =
        {

            {"verbose", no_argument, 0, 'v'},
            {"silent", no_argument, 0, 'n'},
            {"help",   no_argument,  0, 'h'},
            {"no-maxscale-start", no_argument, 0, 's'},
            {"no-maxscale-stop",  no_argument, 0, 'd'},
            {"no-nodes-check",  no_argument, 0, 'r'},
            {"quiet",  no_argument, 0, 'q'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "h:q",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {
        case 'v':
            verbose = true;
            break;

        case 'n':
            verbose = false;
            break;

        case 'q':
            freopen("/dev/null", "w", stdout);
            break;

        case 'h':
            printf ("Options: --help --verbose --silent --no-maxscale-start --no-maxscale-stop");
            break;

        case 's':
            printf ("Maxscale won't be started and Maxscale.cnf won't be uploaded\n");
            no_maxscale_start = true;
            break;

        case 'd':
            printf ("Maxscale won't be stopped\n");
            no_maxscale_stop = true;
            break;

        case 'r':
            printf ("Nodes are not checked before test and are not restarted\n");
            no_nodes_check = true;
            break;

        default:
            run_flag = false;
        }
    }

    repl->truncate_mariadb_logs();
    galera->truncate_mariadb_logs();
    ssh_maxscale(TRUE, "iptables -I INPUT -p tcp --dport 8080 -j ACCEPT");
    ssh_maxscale(TRUE, "iptables -I INPUT -p tcp --dport 4000 -j ACCEPT");
    ssh_maxscale(TRUE, "iptables -I INPUT -p tcp --dport 4001 -j ACCEPT");

    // Create DB user on master and on first Galera node
    //sprintf(str, "%s/create_user.sh", test_dir);
    //repl->copy_to_node(str, (char *) "~/", 0);
    //sprintf(str, "%s/create_user_galera.sh", test_dir);
    //galera->copy_to_node(str, (char *) "~/", 0);

    //sprintf(str, "export repl_user=\"%s\"; export repl_password=\"%s\"; ./create_user.sh", repl->user_name, repl->password);
    //tprintf("cmd: %s\n", str);
    //repl->ssh_node(0, str, FALSE);

    //sprintf(str, "export galera_user=\"%s\"; export galera_password=\"%s\"; ./create_user_galera.sh", galera->user_name, galera->password);
    //galera->ssh_node(0, str, FALSE);

    if (!no_nodes_check) {
        //  checking all nodes and restart if needed
        repl->unblock_all_nodes();
        galera->unblock_all_nodes();
        repl->check_and_restart_nodes_vm();
        galera->check_and_restart_nodes_vm();
        //  checking repl
        if (repl->check_replication(0) != 0) {
            printf("Backend broken! Restarting replication nodes\n");
            repl->start_replication();
        }
        //  checking galera
        if  (galera->check_galera() != 0) {
            printf("Backend broken! Restarting Galera nodes\n");
            galera->start_galera();
        }
    }

    repl->flush_hosts();
    galera->flush_hosts();

    if (!no_nodes_check)
    {
        if ((repl->check_replication(0) != 0) || (galera->check_galera() != 0)) {
            printf("****** BACKEND IS STILL BROKEN! Exiting\n *****");
            exit(200);
        }
    }
    //repl->start_replication();
    if (!no_maxscale_start) {init_maxscale();}
    if (backend_ssl)
    {
        tprintf("Configuring backends for ssl \n");
        repl->configure_ssl(TRUE);
        ssl = TRUE;
        repl->ssl = TRUE;
        galera->configure_ssl(FALSE);
        galera->ssl = TRUE;
        galera->start_galera();
        /*tprintf("Restarting Maxscale\n");
        restart_maxscale();
        tprintf("Restarting Maxscale again\n");
        restart_maxscale();*/
    }
    timeout = 999999999;
    set_log_copy_interval(999999999);
    pthread_create( &timeout_thread_p, NULL, timeout_thread, this);
    pthread_create( &log_copy_thread_p, NULL, log_copy_thread, this);
    gettimeofday(&start_time, NULL);
}

TestConnections::~TestConnections()
{
    if (backend_ssl)
    {
        repl->disable_ssl();
        //galera->disable_ssl();
    }
}

TestConnections::TestConnections()
{
    galera = new Mariadb_nodes((char *)"galera");
    repl   = new Mariadb_nodes((char *)"repl");

    global_result = 0;

    rwsplit_port = 4006;
    readconn_master_port = 4008;
    readconn_slave_port = 4009;
    ports[0] = rwsplit_port;
    ports[1] = readconn_master_port;
    ports[2] = readconn_slave_port;

    read_env();

    timeout = 999999999;
    set_log_copy_interval(999999999);
    pthread_create( &timeout_thread_p, NULL, timeout_thread, this);
    pthread_create( &log_copy_thread_p, NULL, log_copy_thread, this);
    gettimeofday(&start_time, NULL);
}

void TestConnections::add_result(int result, const char *format, ...)
{
    timeval t2;
    gettimeofday(&t2, NULL);
    double elapsedTime = (t2.tv_sec - start_time.tv_sec);
    elapsedTime += (double) (t2.tv_usec - start_time.tv_usec) / 1000000.0;

    if (result != 0) {
        global_result += result;

        printf("%04f: TEST_FAILED! ", elapsedTime);

        va_list argp;
        va_start(argp, format);
        vprintf(format, argp);
        va_end(argp);
    }
}

int TestConnections::read_env()
{

    char * env;
    int i;
    printf("Reading test setup configuration from environmental variables\n");
    galera->read_env();
    repl->read_env();

    env = getenv("maxscale_IP"); if (env != NULL) {sprintf(maxscale_IP, "%s", env);}
    env = getenv("maxscale_user"); if (env != NULL) {sprintf(maxscale_user, "%s", env); } else {sprintf(maxscale_user, "skysql");}
    env = getenv("maxscale_password"); if (env != NULL) {sprintf(maxscale_password, "%s", env); } else {sprintf(maxscale_password, "skysql");}
    env = getenv("maxadmin_password"); if (env != NULL) {sprintf(maxadmin_password, "%s", env); } else {sprintf(maxadmin_password, "mariadb");}
    env = getenv("maxscale_sshkey"); if (env != NULL) {sprintf(maxscale_sshkey, "%s", env); } else {sprintf(maxscale_sshkey, "skysql");}

    //env = getenv("get_logs_command"); if (env != NULL) {sprintf(get_logs_command, "%s", env);}

    env = getenv("sysbench_dir"); if (env != NULL) {sprintf(sysbench_dir, "%s", env);}

    env = getenv("maxdir"); if (env != NULL) {sprintf(maxdir, "%s", env);}
    env = getenv("maxscale_cnf"); if (env != NULL) {sprintf(maxscale_cnf, "%s", env);} else {sprintf(maxscale_cnf, "/etc/maxscale.cnf");}
    env = getenv("maxscale_log_dir"); if (env != NULL) {sprintf(maxscale_log_dir, "%s", env);} else {sprintf(maxscale_log_dir, "%s/logs/", maxdir);}
    env = getenv("maxscale_binlog_dir"); if (env != NULL) {sprintf(maxscale_binlog_dir, "%s", env);} else {sprintf(maxscale_binlog_dir, "%s/Binlog_Service/", maxdir);}
    //env = getenv("test_dir"); if (env != NULL) {sprintf(test_dir, "%s", env);}
    env = getenv("maxscale_access_user"); if (env != NULL) {sprintf(maxscale_access_user, "%s", env);}
    env = getenv("maxscale_access_sudo"); if (env != NULL) {sprintf(maxscale_access_sudo, "%s", env);}
    ssl = false;
    env = getenv("ssl"); if ((env != NULL) && ((strcasecmp(env, "yes") == 0) || (strcasecmp(env, "true") == 0) )) {ssl = true;}
    env = getenv("mysql51_only"); if ((env != NULL) && ((strcasecmp(env, "yes") == 0) || (strcasecmp(env, "true") == 0) )) {no_nodes_check = true;}

    env = getenv("maxscale_hostname"); if (env != NULL) {sprintf(maxscale_hostname, "%s", env);} else {sprintf(maxscale_hostname, "%s", maxscale_IP);}

    env = getenv("backend_ssl"); if (env != NULL && ((strcasecmp(env, "yes") == 0) || (strcasecmp(env, "true") == 0) )) {backend_ssl = true;} else {backend_ssl = false;}

    if (strcmp(maxscale_access_user, "root") == 0) {
        sprintf(maxscale_access_homedir, "/%s/", maxscale_access_user);
    } else {
        sprintf(maxscale_access_homedir, "/home/%s/", maxscale_access_user);
    }

    env = getenv("smoke"); if ((env != NULL) && ((strcasecmp(env, "yes") == 0) || (strcasecmp(env, "true") == 0) )) {smoke = true;} else {smoke = false;}
    env = getenv("maxscale_restart"); if ((env != NULL) && ((strcasecmp(env, "no") == 0) || (strcasecmp(env, "false") == 0) )) {maxscale_restart = false;} else {maxscale_restart = true;}
    env = getenv("threads"); if ((env != NULL)) {sscanf(env, "%d", &threads);} else {threads = 4;}
}

int TestConnections::print_env()
{
    int  i;
    printf("Maxscale IP\t%s\n", maxscale_IP);
    printf("Maxscale User name\t%s\n", maxscale_user);
    printf("Maxscale Password\t%s\n", maxscale_password);
    printf("Maxscale SSH key\t%s\n", maxscale_sshkey);
    printf("Maxadmin password\t%s\n", maxadmin_password);
    printf("Access user\t%s\n", maxscale_access_user);
    repl->print_env();
    galera->print_env();
}

const char * get_template_name(char * test_name)
{
    int i = 0;
    while ((strcmp(cnf_templates[i].test_name, test_name) != 0) && (strcmp(cnf_templates[i].test_name, "NULL") != 0))  i++;
    if (strcmp(cnf_templates[i].test_name, "NULL") == 0)
    {
        return default_template;
    } else {
        return cnf_templates[i].test_template;
    }
}

int TestConnections::init_maxscale()
{
    char str[4096];
    const char * template_name = get_template_name(test_name);
    char template_file[1024];
    tprintf("Template is %s\n", template_name);

    sprintf(template_file, "%s/cnf/maxscale.cnf.template.%s", test_dir, template_name);
    sprintf(str, "cp %s maxscale.cnf", template_file);
    if (system(str) != 0)
    {
        tprintf("Error copying maxscale.cnf template\n");
        return 1;
    }

    if (backend_ssl)
    {
        system("sed -i \"s|type=server|type=server\nssl=required\nssl_cert=/###access_homedir###/certs/client-cert.pem\nssl_key=/###access_homedir###/certs/client-key.pem\nssl_ca_cert=/###access_homedir###/certs/ca.pem|g\" maxscale.cnf");
    }

    sprintf(str, "sed -i \"s/###threads###/%d/\"  maxscale.cnf", threads); system(str);

    Mariadb_nodes * mdn[2];
    mdn[0] = repl;
    mdn[1] = galera;
    int i, j;

    for (j = 0; j < 2; j++)
    {
        for (i = 0; i < mdn[j]->N; i++)
        {
            sprintf(str, "sed -i \"s/###%s_server_IP_%0d###/%s/\" maxscale.cnf", mdn[j]->prefix, i+1, mdn[j]->IP[i]); system(str);
            sprintf(str, "sed -i \"s/###%s_server_port_%0d###/%d/\" maxscale.cnf", mdn[j]->prefix, i+1, mdn[j]->port[i]); system(str);
        }
        mdn[j]->connect();
        execute_query(mdn[j]->nodes[0], (char *) "CREATE DATABASE IF NOT EXISTS test");
        mdn[j]->close_connections();
    }

    sprintf(str, "sed -i \"s/###access_user###/%s/g\" maxscale.cnf", maxscale_access_user); system(str);
    sprintf(str, "sed -i \"s|###access_homedir###|%s|g\" maxscale.cnf", maxscale_access_homedir);  system(str);

    if (repl->v51)
    {
        system("sed -i \"s/###repl51###/mysql51_replication=true/g\" maxscale.cnf");
    }

    copy_to_maxscale((char *) "maxscale.cnf", (char *) "./");
    ssh_maxscale(TRUE, "cp maxscale.cnf %s", maxscale_cnf);
    ssh_maxscale(TRUE, "rm -rf %s/certs", maxscale_access_homedir);
    ssh_maxscale(FALSE, "mkdir %s/certs", maxscale_access_homedir);
    sprintf(str, "%s/ssl-cert/*", test_dir);
    copy_to_maxscale(str, (char *) "./certs/");
    sprintf(str, "cp %s/ssl-cert/* .", test_dir); system(str);
    ssh_maxscale(TRUE,  "chown maxscale:maxscale -R %s/certs", maxscale_access_homedir);
    ssh_maxscale(TRUE, "chmod 664 %s/certs/*.pem; chmod a+x %s", maxscale_access_homedir, maxscale_access_homedir);

    if (maxscale_restart)
    {
        ssh_maxscale(TRUE, "service maxscale stop");
        ssh_maxscale(TRUE, "killall -9 maxscale");
    }
    ssh_maxscale(TRUE, "truncate -s 0 %s/maxscale.log ; %s chown maxscale:maxscale %s/maxscale.log", maxscale_log_dir, maxscale_access_sudo, maxscale_log_dir);
    ssh_maxscale(TRUE, "truncate -s 0 %s/maxscale1.log ; %s chown maxscale:maxscale %s/maxscale1.log", maxscale_log_dir, maxscale_access_sudo, maxscale_log_dir);
    ssh_maxscale(TRUE, "rm /tmp/core*");
    ssh_maxscale(TRUE, "rm -rf /dev/shm/*");
    if ((!maxscale_restart) && (ssh_maxscale(TRUE, "service maxscale status | grep running") == 0))
    {
        ssh_maxscale(TRUE, "ulimit -c unlimited; %s killall -HUP maxscale", maxscale_access_sudo);
    } else {
        ssh_maxscale(TRUE, "ulimit -c unlimited; %s service maxscale restart", maxscale_access_sudo);
    }
    ssh_maxscale(FALSE, "printenv > test.environment");
    fflush(stdout);
    tprintf("Waiting 15 seconds\n");
    sleep(15);
}

int TestConnections::connect_maxscale()
{
    return(
                connect_rwsplit() +
                connect_readconn_master() +
                connect_readconn_slave()
                );
}

int TestConnections::close_maxscale_connections()
{
    mysql_close(conn_master);
    mysql_close(conn_slave);
    mysql_close(conn_rwsplit);
}

int TestConnections::restart_maxscale()
{
    int res = ssh_maxscale(true, "service maxscale restart");
    sleep(10);
    return(res);
}

int TestConnections::start_maxscale()
{
    int res = ssh_maxscale(true, "service maxscale start");
    sleep(10);
    return(res);
}

int TestConnections::stop_maxscale()
{
    int res = ssh_maxscale(true, "service maxscale stop");
    check_maxscale_processes(0);
    return(res);
}

int TestConnections::copy_mariadb_logs(Mariadb_nodes * repl, char * prefix)
{
    int local_result = 0;
    char * mariadb_log;
    FILE * f;
    int i;
    char str[4096];

    sprintf(str, "mkdir -p LOGS/%s", test_name);
    system(str);
    for (i = 0; i < repl->N; i++)
    {
        mariadb_log = repl->ssh_node_output(i, (char *) "cat /var/lib/mysql/*.err", TRUE);
        sprintf(str, "LOGS/%s/%s%d_mariadb_log", test_name, prefix, i);
        f = fopen(str, "w");
        if (f != NULL)
        {
            fwrite(mariadb_log, sizeof(char), strlen(mariadb_log), f);
            fclose(f);
        } else {
            printf("Error writing MariaDB log");
            local_result = 1;
        }
        free(mariadb_log);
    }
    return local_result;
}

int TestConnections::copy_all_logs()
{
    char str[4096];
    set_timeout(300);

    copy_mariadb_logs(repl, (char *) "node");
    copy_mariadb_logs(galera, (char *) "galera");

    sprintf(str, "%s/copy_logs.sh %s", test_dir, test_name);
    tprintf("Executing %s\n", str);
    if (system(str) !=0) {
        tprintf("copy_logs.sh executing FAILED!\n");
        return(1);
    } else {
        tprintf("copy_logs.sh OK!\n");
        return(0);
    }
}

int TestConnections::copy_all_logs_periodic()
{
    char str[4096];
    //set_timeout(300);

    timeval t2;
    gettimeofday(&t2, NULL);
    double elapsedTime = (t2.tv_sec - start_time.tv_sec);
    elapsedTime += (double) (t2.tv_usec - start_time.tv_usec) / 1000000.0;

    sprintf(str, "%s/copy_logs.sh %s %04f", test_dir, test_name, elapsedTime);
    tprintf("Executing %s\n", str);
    if (system(str) !=0) {
        tprintf("copy_logs.sh executing FAILED!\n");
        return(1);
    } else {
        tprintf("copy_logs.sh OK!\n");
        return(0);
    }
}

int TestConnections::prepare_binlog()
{
    char version_str[1024];
    find_field(repl->nodes[0], "SELECT @@VERSION", "@@version", version_str);
    tprintf("Master server version %s\n", version_str);

    if ((strstr(version_str, "10.0") != NULL) ||
            (strstr(version_str, "10.1") != NULL) ||
            (strstr(version_str, "10.2") != NULL))
    {
        tprintf("10.0!\n");
    }
    else {
        add_result(ssh_maxscale(true,
                                "sed -i \"s/,mariadb10-compatibility=1//\" %s",
                                maxscale_cnf), "Error editing maxscale.cnf");
    }

    tprintf("Removing all binlog data from Maxscale node\n");
    add_result(ssh_maxscale(true, "rm -rf %s", maxscale_binlog_dir),
               "Removing binlog data failed\n");

    tprintf("Creating binlog dir\n");
    add_result(ssh_maxscale(true, "mkdir -p %s", maxscale_binlog_dir),
               "Creating binlog data dir failed\n");
    tprintf("Set 'maxscale' as a owner of binlog dir\n");
    add_result(ssh_maxscale(false,
                            "%s mkdir -p %s; %s chown maxscale:maxscale -R %s",
                            maxscale_access_sudo, maxscale_binlog_dir,
                            maxscale_access_sudo, maxscale_binlog_dir),
               "directory ownership change failed\n");
    return 0;
}

int TestConnections::start_binlog()
{
    char sys1[4096];
    MYSQL * binlog;
    char log_file[256];
    char log_pos[256];
    char cmd_opt[256];

    int i;
    int global_result = 0;
    bool no_pos;

    no_pos = repl->no_set_pos;

    switch (binlog_cmd_option) {
    case 1:
        sprintf(cmd_opt, "--binlog-checksum=CRC32");
        break;
    case 2:
        sprintf(cmd_opt, "--binlog-checksum=NONE");
        break;
    default:
        sprintf(cmd_opt, " ");
    }

    repl->stop_nodes();

    binlog = open_conn_no_db(binlog_port, maxscale_IP, repl->user_name, repl->password, ssl);
    execute_query(binlog, (char *) "stop slave");
    execute_query(binlog, (char *) "reset slave all");
    execute_query(binlog, (char *) "reset master");
    mysql_close(binlog);

    tprintf("Stopping maxscale\n");
    add_result(stop_maxscale(), "Maxscale stopping failed\n");

    for (i = 0; i < repl->N; i++)
    {
        repl->start_node(i, cmd_opt);
    }
    sleep(5);

    tprintf("Connecting to all backend nodes\n");
    repl->connect();

    for (i = 0; i < repl->N; i++) {
        execute_query(repl->nodes[i], "stop slave");
        execute_query(repl->nodes[i], "reset slave all");
        execute_query(repl->nodes[i], "reset master");
    }

    prepare_binlog();

    tprintf("Testing binlog when MariaDB is started with '%s' option\n", cmd_opt);

    tprintf("ls binlog data dir on Maxscale node\n");
    add_result(ssh_maxscale(true, "ls -la %s/", maxscale_binlog_dir), "ls failed\n");

    tprintf("show master status\n");
    find_field(repl->nodes[0], (char *) "show master status", (char *) "File", &log_file[0]);
    find_field(repl->nodes[0], (char *) "show master status", (char *) "Position", &log_pos[0]);
    tprintf("Real master file: %s\n", log_file);
    tprintf("Real master pos : %s\n", log_pos);

    tprintf("Stopping first slave (node 1)\n");
    try_query(repl->nodes[1], (char *) "stop slave;");
    //repl->no_set_pos = true;
    repl->no_set_pos = false;
    tprintf("Configure first backend slave node to be slave of real master\n");
    repl->set_slave(repl->nodes[1], repl->IP[0],  repl->port[0], log_file, log_pos);

    tprintf("Starting back Maxscale\n");
    add_result(start_maxscale(), "Maxscale start failed\n");

    tprintf("Connecting to MaxScale binlog router (with any DB)\n");
    binlog = open_conn_no_db(binlog_port, maxscale_IP, repl->user_name, repl->password, ssl);

    add_result(mysql_errno(binlog), "Error connection to binlog router %s\n", mysql_error(binlog));

    repl->no_set_pos = true;
    tprintf("configuring Maxscale binlog router\n");
    repl->set_slave(binlog, repl->IP[0], repl->port[0], log_file, log_pos);
    try_query(binlog, "start slave");

    repl->no_set_pos = false;

    // get Master status from Maxscale binlog
    tprintf("show master status\n");fflush(stdout);
    find_field(binlog, (char *) "show master status", (char *) "File", &log_file[0]);
    find_field(binlog, (char *) "show master status", (char *) "Position", &log_pos[0]);

    tprintf("Maxscale binlog master file: %s\n", log_file); fflush(stdout);
    tprintf("Maxscale binlog master pos : %s\n", log_pos); fflush(stdout);

    tprintf("Setup all backend nodes except first one to be slaves of binlog Maxscale node\n");fflush(stdout);
    for (i = 2; i < repl->N; i++) {
        try_query(repl->nodes[i], (char *) "stop slave;");
        repl->set_slave(repl->nodes[i],  maxscale_IP, binlog_port, log_file, log_pos);
    }
    repl->close_connections();
    mysql_close(binlog);
    repl->no_set_pos = no_pos;
    return(global_result);
}

int TestConnections::start_mm()
{
    int i;
    char log_file1[256];
    char log_pos1[256];
    char log_file2[256];
    char log_pos2[256];

    tprintf("Stopping maxscale\n");fflush(stdout);
    int global_result = stop_maxscale();

    tprintf("Stopping all backend nodes\n");fflush(stdout);
    global_result += repl->stop_nodes();

    for (i = 0; i < 2; i++) {
        tprintf("Starting back node %d\n", i);
        global_result += repl->start_node(i, (char *) "");
    }

    repl->connect();
    for (i = 0; i < 2; i++) {
        execute_query(repl->nodes[i], (char *) "stop slave");
        execute_query(repl->nodes[i], (char *) "reset master");
    }

    execute_query(repl->nodes[0], (char *) "SET GLOBAL READ_ONLY=ON");

    find_field(repl->nodes[0], (char *) "show master status", (char *) "File", log_file1);
    find_field(repl->nodes[0], (char *) "show master status", (char *) "Position", log_pos1);

    find_field(repl->nodes[1], (char *) "show master status", (char *) "File", log_file2);
    find_field(repl->nodes[1], (char *) "show master status", (char *) "Position", log_pos2);

    repl->set_slave(repl->nodes[0], repl->IP[1],  repl->port[1], log_file2, log_pos2);
    repl->set_slave(repl->nodes[1], repl->IP[0],  repl->port[0], log_file1, log_pos1);

    repl->close_connections();

    tprintf("Starting back Maxscale\n");  fflush(stdout);
    global_result += start_maxscale();

    return(global_result);
}

void TestConnections::check_log_err(const char * err_msg, bool expected)
{

    char * err_log_content;

    tprintf("Getting logs\n");
    char sys1[4096];
    set_timeout(100);
    sprintf(&sys1[0], "rm *.log; %s %s", get_logs_command, maxscale_IP);
    //tprintf("Executing: %s\n", sys1);
    system(sys1);
    set_timeout(50);

    tprintf("Reading maxscale.log\n");
    if ( ( read_log((char *) "maxscale.log", &err_log_content) != 0) || (strlen(err_log_content) < 2) ) {
        tprintf("Reading maxscale1.log\n");
        free(err_log_content);
        if (read_log((char *) "maxscale1.log", &err_log_content) != 0) {
            add_result(1, "Error reading log\n");
        }
    }
    //printf("\n\n%s\n\n", err_log_content);
    if (err_log_content != NULL) 
    {
        if (expected) {
            if (strstr(err_log_content, err_msg) == NULL) {
                add_result(1, "There is NO \"%s\" error in the log\n", err_msg);
            } else {
                tprintf("There is proper \"%s \" error in the log\n", err_msg);
            }}
        else {
            if (strstr(err_log_content, err_msg) != NULL) {
                add_result(1, "There is UNEXPECTED error \"%s\" error in the log\n", err_msg);
            } else {
                tprintf("There are no unxpected errors \"%s \" error in the log\n", err_msg);
            }
        }

        free(err_log_content);
   }
}

int TestConnections::find_connected_slave(int * global_result)
{
    int conn_num;
    int all_conn = 0;
    int current_slave = -1;
    repl->connect();
    for (int i = 0; i < repl->N; i++) {
        conn_num = get_conn_num(repl->nodes[i], maxscale_IP, maxscale_hostname, (char *) "test");
        tprintf("connections to %d: %u\n", i, conn_num);
        if ((i == 0) && (conn_num != 1)) {tprintf("There is no connection to master\n"); *global_result = 1;}
        all_conn += conn_num;
        if ((i != 0) && (conn_num != 0)) {current_slave = i;}
    }
    if (all_conn != 2) {tprintf("total number of connections is not 2, it is %d\n", all_conn); *global_result = 1;}
    tprintf("Now connected slave node is %d (%s)\n", current_slave, repl->IP[current_slave]);
    repl->close_connections();
    return(current_slave);
}

int TestConnections::find_connected_slave1()
{
    int conn_num;
    int all_conn = 0;
    int current_slave = -1;
    repl->connect();
    for (int i = 0; i < repl->N; i++) {
        conn_num = get_conn_num(repl->nodes[i], maxscale_IP, maxscale_hostname, (char *) "test");
        tprintf("connections to %d: %u\n", i, conn_num);
        all_conn += conn_num;
        if ((i != 0) && (conn_num != 0)) {current_slave = i;}
    }
    tprintf("Now connected slave node is %d (%s)\n", current_slave, repl->IP[current_slave]);
    repl->close_connections();
    return(current_slave);
}

int TestConnections::check_maxscale_processes(int expected)
{
    char* maxscale_num = ssh_maxscale_output(false, "ps -C maxscale | grep maxscale | wc -l");
    char* nl = strchr(maxscale_num, '\n');
    if (nl)
    {
        *nl = '\0';
    }

    if (atoi(maxscale_num) != expected)
    {
        tprintf("%s maxscale processes detected, trying agin in 5 seconds\n", maxscale_num);
        sleep(5);
        maxscale_num = ssh_maxscale_output(false, "ps -C maxscale | grep maxscale | wc -l");
        if (atoi(maxscale_num) != expected)
        {
            add_result(1, "Number of MaxScale processes is not %d, it is %s\n", expected, maxscale_num);
        }
    }

    return 0;
}

int TestConnections::check_maxscale_alive()
{
    int gr = global_result;
    set_timeout(10);
    tprintf("Connecting to Maxscale\n");
    add_result(connect_maxscale(), "Can not connect to Maxscale\n");
    tprintf("Trying simple query against all sevices\n");
    tprintf("RWSplit \n");
    set_timeout(10);
    try_query(conn_rwsplit, (char *) "show databases;");
    tprintf("ReadConn Master \n");
    set_timeout(10);
    try_query(conn_master, (char *) "show databases;");
    tprintf("ReadConn Slave \n");
    set_timeout(10);
    try_query(conn_slave, (char *) "show databases;");
    set_timeout(10);
    close_maxscale_connections()    ;
    add_result(global_result-gr, "Maxscale is not alive\n");
    stop_timeout();

    check_maxscale_processes(1);

    return(global_result-gr);
}

int TestConnections::test_maxscale_connections(bool rw_split, bool rc_master, bool rc_slave)
{
    int rval = 0;
    int rc;

    tprintf("Testing RWSplit, expecting %s\n", (rw_split ? "success" : "failure"));
    rc = execute_query(conn_rwsplit, "select 1");
    if((rc == 0) != rw_split)
    {
        tprintf("Error: Query %s\n", (rw_split ? "failed" : "succeeded"));
        rval++;
    }

    tprintf("Testing ReadConnRoute Master, expecting %s\n", (rc_master ? "success" : "failure"));
    rc = execute_query(conn_master, "select 1");
    if((rc == 0) != rc_master)
    {
        tprintf("Error: Query %s", (rc_master ? "failed" : "succeeded"));
        rval++;
    }

    tprintf("Testing ReadConnRoute Slave, expecting %s\n", (rc_slave ? "success" : "failure"));
    rc = execute_query(conn_slave, "select 1");
    if((rc == 0) != rc_slave)
    {
        tprintf("Error: Query %s", (rc_slave ? "failed" : "succeeded"));
        rval++;
    }
    return rval;
}

void TestConnections::generate_ssh_cmd(char * cmd, char * ssh, bool sudo)
{
    if (sudo)
    {
        sprintf(cmd, "ssh -i %s -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=quiet %s@%s '%s %s'",
                maxscale_sshkey, maxscale_access_user, maxscale_IP, maxscale_access_sudo, ssh);
    } else
    {
        sprintf(cmd, "ssh -i %s -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=quiet %s@%s '%s\'",
                maxscale_sshkey, maxscale_access_user, maxscale_IP, ssh);
    }
}


char* TestConnections::ssh_maxscale_output(bool sudo, const char* format, ...)
{
    va_list valist;

    va_start(valist, format);
    int message_len = vsnprintf(NULL, 0, format, valist);
    va_end(valist);

    if(message_len < 0)
    {
        return NULL;
    }

    char *sys = (char*)malloc(message_len + 1);

    va_start(valist, format);
    vsnprintf(sys, message_len + 1, format, valist);
    va_end(valist);

    char *cmd = (char*)malloc(message_len + 1024);
    generate_ssh_cmd(cmd, sys, sudo);

    FILE *output = popen(cmd, "r");
    char buffer[1024];
    size_t rsize = sizeof(buffer);
    char* result = (char*)calloc(rsize, sizeof(char));

    while(fgets(buffer, sizeof(buffer), output))
    {
        result = (char*)realloc(result, sizeof(buffer) + rsize);
        rsize += sizeof(buffer);
        strcat(result, buffer);
    }

    free(sys);
    free(cmd);

    return result;
}

int  TestConnections::ssh_maxscale(bool sudo, const char* format, ...)
{
    va_list valist;

    va_start(valist, format);
    int message_len = vsnprintf(NULL, 0, format, valist);
    va_end(valist);

    if(message_len < 0)
    {
        return -1;
    }

    char *sys = (char*)malloc(message_len + 1);

    va_start(valist, format);
    vsnprintf(sys, message_len + 1, format, valist);
    va_end(valist);

    char *cmd = (char*)malloc(message_len + 1024);
    generate_ssh_cmd(cmd, sys, sudo);
    int rc = system(cmd);
    free(sys);
    free(cmd);
    return rc;
}


int TestConnections::copy_to_maxscale(char* src, char* dest)
{
    char sys[strlen(src) + strlen(dest) + 1024];

    sprintf(sys, "scp -i %s -o UserKnownHostsFile=/dev/null "
            "-o StrictHostKeyChecking=no -o LogLevel=quiet %s %s@%s:%s",
            maxscale_sshkey, src, maxscale_access_user, maxscale_IP, dest);

    return system(sys);
}


int TestConnections::copy_from_maxscale(char* src, char* dest)
{
    char sys[strlen(src) + strlen(dest) + 1024];

    sprintf(sys, "scp -i %s -o UserKnownHostsFile=/dev/null "
            "-o StrictHostKeyChecking=no -o LogLevel=quiet %s@%s:%s %s",
            maxscale_sshkey, maxscale_access_user, maxscale_IP, src, dest);

    return system(sys);
}


int TestConnections::reconfigure_maxscale(char* config_template)
{
    char cmd[1024];
    setenv("test_name",config_template,1);
    sprintf(cmd,"%s/configure_maxscale.sh",test_dir);
    return system(cmd); 
}

int TestConnections::create_connections(int conn_N, bool rwsplit_flag, bool master_flag, bool slave_flag, bool galera_flag)
{
    int i;
    int local_result = 0;
    MYSQL * rwsplit_conn[conn_N];
    MYSQL * master_conn[conn_N];
    MYSQL * slave_conn[conn_N];
    MYSQL * galera_conn[conn_N];


    tprintf("Opening %d connections to each router\n", conn_N);
    for (i = 0; i < conn_N; i++) {
        set_timeout(20);
        tprintf("opening %d-connection: ", i+1);
        if (rwsplit_flag)
        {
            printf("RWSplit \t");
            rwsplit_conn[i] = open_rwsplit_connection();
            if (!rwsplit_conn[i]) { local_result++; tprintf("RWSplit connection failed\n");}
        }
        if (master_flag)
        {
            printf("ReadConn master \t");
            master_conn[i] = open_readconn_master_connection();
            if ( mysql_errno(master_conn[i]) != 0 ) { local_result++; tprintf("ReadConn master connection failed, error: %s\n", mysql_error(master_conn[i]) );}
        }
        if (slave_flag)
        {
            printf("ReadConn slave \t");
            slave_conn[i] = open_readconn_slave_connection();
            if ( mysql_errno(slave_conn[i]) != 0 )  { local_result++; tprintf("ReadConn slave connection failed, error: %s\n", mysql_error(slave_conn[i]) );}
        }
        if (galera_flag)
        {
            printf("galera \n");
            galera_conn[i] = open_conn(4016, maxscale_IP, maxscale_user, maxscale_password, ssl);
            if ( mysql_errno(galera_conn[i]) != 0)  { local_result++; tprintf("Galera connection failed, error: %s\n", mysql_error(galera_conn[i]));}
        }
    }
    for (i = 0; i < conn_N; i++) {
        set_timeout(20);
        tprintf("Trying query against %d-connection: ", i+1);
        if (rwsplit_flag)
        {
            tprintf("RWSplit \t");
            local_result += execute_query(rwsplit_conn[i], "select 1;");
        }
        if (master_flag)
        {
            tprintf("ReadConn master \t");
            local_result += execute_query(master_conn[i], "select 1;");
        }
        if (slave_flag)
        {
            tprintf("ReadConn slave \t");
            local_result += execute_query(slave_conn[i], "select 1;");
        }
        if (galera_flag)
        {
            tprintf("galera \n");
            local_result += execute_query(galera_conn[i], "select 1;");
        }
    }

    //global_result += check_pers_conn(Test, pers_conn_expected);
    tprintf("Closing all connections\n");
    for (i=0; i<conn_N; i++) {
        set_timeout(20);
        if (rwsplit_flag) {mysql_close(rwsplit_conn[i]);}
        if (master_flag)  {mysql_close(master_conn[i]);}
        if (slave_flag)   {mysql_close(slave_conn[i]);}
        if (galera_flag)  {mysql_close(galera_conn[i]);}
    }
    stop_timeout();
    //sleep(5);
    return(local_result);
}

int TestConnections::get_client_ip(char * ip)
{
    MYSQL * conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    int ret = 1;
    unsigned long long int num_fields;
    //unsigned long long int row_i=0;
    unsigned long long int rows;
    unsigned long long int i;
    unsigned int conn_num = 0;

    connect_rwsplit();
    if (execute_query(conn_rwsplit, (char *) "CREATE DATABASE IF NOT EXISTS db_to_check_clent_ip") !=0 ) {
        return(ret);
    }
    close_rwsplit();
    conn = open_conn_db(rwsplit_port, maxscale_IP, (char *) "db_to_check_clent_ip", maxscale_user, maxscale_password, ssl);

    if (conn != NULL) {
        if(mysql_query(conn, "show processlist;") != 0) {
            printf("Error: can't execute SQL-query: show processlist\n");
            printf("%s\n\n", mysql_error(conn));
            conn_num = 0;
        } else {
            res = mysql_store_result(conn);
            if(res == NULL) {
                printf("Error: can't get the result description\n");
                conn_num = -1;
            } else {
                num_fields = mysql_num_fields(res);
                rows = mysql_num_rows(res);
                for (i = 0; i < rows; i++) {
                    row = mysql_fetch_row(res);
                    if ( (row[2] != NULL ) && (row[3] != NULL) ) {
                        if  (strstr(row[3], "db_to_check_clent_ip") != NULL) {
                            ret = 0;
                            strcpy(ip, row[2]);
                        }
                    }
                }
            }
            mysql_free_result(res);
        }
    }

    mysql_close(conn);
    return(ret);
}

int TestConnections::set_timeout(long int timeout_seconds)
{
    timeout = timeout_seconds;
    return(0);
}

int TestConnections::set_log_copy_interval(long int interval_seconds)
{
    log_copy_to_go = interval_seconds;
    log_copy_interval = interval_seconds;
    return(0);
}

int TestConnections::stop_timeout()
{
    timeout = 999999999;
    return(0);
}

int TestConnections::tprintf(const char *format, ...)
{
    timeval t2;
    gettimeofday(&t2, NULL);
    double elapsedTime = (t2.tv_sec - start_time.tv_sec);
    elapsedTime += (double) (t2.tv_usec - start_time.tv_usec) / 1000000.0;

    printf("%04f: ", elapsedTime);

    va_list argp;
    va_start(argp, format);
    vprintf(format, argp);
    va_end(argp);
}

void *timeout_thread( void *ptr )
{
    TestConnections * Test = (TestConnections *) ptr;
    struct timespec tim;
    while (Test->timeout > 0) {
        tim.tv_sec = 1;
        tim.tv_nsec = 0;
        nanosleep(&tim, NULL);
        Test->timeout--;
    }
    Test->tprintf("\n **** Timeout! *** \n");
    Test->copy_all_logs();
    exit(250);
}

void *log_copy_thread( void *ptr )
{
    TestConnections * Test = (TestConnections *) ptr;
    struct timespec tim;
    while (true)
    {
        while (Test->log_copy_to_go > 0)
        {
            tim.tv_sec = 1;
            tim.tv_nsec = 0;
            nanosleep(&tim, NULL);
            Test->log_copy_to_go--;
        }
        Test->log_copy_to_go = Test->log_copy_interval;
        Test->tprintf("\n **** Copying all logs *** \n");
        Test->copy_all_logs_periodic();
    }
}

int TestConnections::insert_select(int N)
{
    int global_result = 0;
    tprintf("Create t1\n");
    set_timeout(30);
    create_t1(conn_rwsplit);
    tprintf("Insert data into t1\n");
    set_timeout(30);
    insert_into_t1(conn_rwsplit, N);

    tprintf("SELECT: rwsplitter\n");
    set_timeout(30);
    global_result += select_from_t1(conn_rwsplit, N);
    tprintf("SELECT: master\n");
    set_timeout(30);
    global_result += select_from_t1(conn_master, N);
    tprintf("SELECT: slave\n");
    set_timeout(30);
    global_result += select_from_t1(conn_slave, N);
    tprintf("Sleeping to let replication happen\n");
    stop_timeout();
    if (smoke) {
        sleep(30);
    } else {
        sleep(180);
    }
    for (int i=0; i<repl->N; i++) {
        tprintf("SELECT: directly from node %d\n", i);
        set_timeout(30);
        global_result += select_from_t1(repl->nodes[i], N);
    }
    return(global_result);
}

int TestConnections::use_db(char * db)
{
    int local_result = 0;
    char sql[100];

    sprintf(sql, "USE %s;", db);
    set_timeout(20);
    tprintf("selecting DB '%s' for rwsplit\n", db);
    local_result += execute_query(conn_rwsplit, sql);
    tprintf("selecting DB '%s' for readconn master\n", db);
    local_result += execute_query(conn_slave, sql);
    tprintf("selecting DB '%s' for readconn slave\n", db);
    local_result += execute_query(conn_master, sql);
    for (int i = 0; i < repl->N; i++) {
        tprintf("selecting DB '%s' for direct connection to node %d\n", db, i);
        local_result += execute_query(repl->nodes[i], sql);
    }
    return(local_result);
}

int TestConnections::check_t1_table(bool presence, char * db)
{
    char * expected;
    char * actual;
    set_timeout(30);
    int gr = global_result;
    if (presence) {
        expected = (char *) "";
        actual   = (char *) "NOT";
    } else {
        expected = (char *) "NOT";
        actual   = (char *) "";
    }

    add_result(use_db(db), "use db failed\n");

    tprintf("Checking: table 't1' should %s be found in '%s' database\n", expected, db);
    if ( ((check_if_t1_exists(conn_rwsplit) >  0) && (!presence) ) ||
         ((check_if_t1_exists(conn_rwsplit) == 0) && (presence) ))
    {add_result(1, "Table t1 is %s found in '%s' database using RWSplit\n", actual, db); } else {
        tprintf("RWSplit: ok\n");
    }
    if ( ((check_if_t1_exists(conn_master) >  0) && (!presence) ) ||
         ((check_if_t1_exists(conn_master) == 0) && (presence) ))
    {add_result(1, "Table t1 is %s found in '%s' database using Readconnrouter with router option master\n", actual, db); } else {
        tprintf("ReadConn master: ok\n");
    }
    if ( ((check_if_t1_exists(conn_slave) >  0) && (!presence) ) ||
         ((check_if_t1_exists(conn_slave) == 0) && (presence) ))
    {add_result(1, "Table t1 is %s found in '%s' database using Readconnrouter with router option slave\n", actual, db); } else {
        tprintf("ReadConn slave: ok\n");
    }
    tprintf("Sleeping to let replication happen\n");
    stop_timeout();
    sleep(60);
    for (int i=0; i< repl->N; i++) {
        set_timeout(30);
        if ( ((check_if_t1_exists(repl->nodes[i]) >  0) && (!presence) ) ||
             ((check_if_t1_exists(repl->nodes[i]) == 0) && (presence) ))
        {add_result(1, "Table t1 is %s found in '%s' database using direct connect to node %d\n", actual, db, i); } else {
            tprintf("Node %d: ok\n", i);
        }
    }
    return(global_result-gr);
}

int TestConnections::try_query(MYSQL *conn, const char *sql)
{
    int res = execute_query(conn, sql);
    int len = strlen(sql);
    add_result(res, "Query '%.*s%s' failed!\n", len < 100 ? len : 100, sql, len < 100 ? "" : "...");
    return(res);
}

int TestConnections::find_master_maxadmin(Mariadb_nodes * nodes)
{
    char show_server[32];
    char res[256];
    bool found = false;
    int master = -1;
    for (int i = 0; i < nodes->N; i++)
    {
        sprintf(show_server, "show server server%d", i + 1);
        get_maxadmin_param(show_server, (char *) "Status", res);
        if (strstr(res, "Master") != NULL)
        {
            if (found)
            {
                master = -1;
            } else {
                master = i;
                found = true;
            }
        }
    }
    return(master);
}

int TestConnections::execute_maxadmin_command(char * cmd)
{
    return(ssh_maxscale(TRUE, "maxadmin %s", cmd));
}
int TestConnections::execute_maxadmin_command_print(char * cmd)
{
    printf("%s\n", ssh_maxscale_output(TRUE, "maxadmin %s", cmd));
    return 0;
}
int TestConnections::get_maxadmin_param(char *command, char *param, char *result)
{
    char		* buf;

    buf = ssh_maxscale_output(TRUE, "maxadmin %s", command);

    printf("%s\n", buf);

    char * x =strstr(buf, param);
    if (x == NULL )
        return(1);

    int param_len = strlen(param);
    int cnt = 0;
    while (x[cnt+param_len]  != '\n') {
        result[cnt] = x[cnt+param_len];
        cnt++;
    }
    result[cnt] = '\0';

    return(0);
}

int TestConnections::list_dirs()
{
    for (int i = 0; i < repl->N; i++)
    {
        tprintf("ls on node %d\n", i);
        repl->ssh_node(i, (char *) "ls -la /var/lib/mysql", TRUE); fflush(stdout);
    }
    tprintf("ls maxscale \n");
    ssh_maxscale(TRUE, "ls -la /var/lib/maxscale/"); fflush(stdout);
    return 0;
}

long unsigned TestConnections::get_maxscale_memsize()
{
    char * ps_out = ssh_maxscale_output(FALSE, "ps -e -o pid,vsz,comm= | grep maxscale");
    long unsigned mem = 0;
    pid_t pid;
    sscanf(ps_out, "%d %lu", &pid, &mem);
    return mem;
}
