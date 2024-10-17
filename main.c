#include <stdio.h>

#include "debug.h"
#include "config.h"
#include "ib.h"
#include "setup_ib.h"
#include "client.h"
#include "server.h"

FILE	*log_fp	     = NULL;

int	init_env    ();
void	destroy_env ();

int main (int argc, char *argv[])
{
    int	ret = 0;

    if (argc == 3) {
        config_info.is_server   = false;
        config_info.server_name = argv[1];
        config_info.sock_port   = argv[2];
        printf("Client mode: server_name=%s, sock_port=%s\n", argv[1], argv[2]);
    } else if (argc == 2) {
        config_info.is_server = true;
        config_info.sock_port = argv[1];
        printf("Server mode: sock_port=%s\n", argv[1]);
    } else {
        printf("Server: %s sock_port\n", argv[0]);
        printf("Client: %s server_name sock_port\n", argv[0]);
        return 0;
    }

    config_info.msg_size         = 64; 
    config_info.num_concurr_msgs = 1;
    printf("Config: msg_size=%d, num_concurr_msgs=%d\n", config_info.msg_size, config_info.num_concurr_msgs);

    printf("Initializing environment\n");
    ret = init_env ();
    check (ret == 0, "Failed to init env");

    printf("Setting up IB\n");
    ret = setup_ib ();
    check (ret == 0, "Failed to setup IB");

    if (config_info.is_server) {
        printf("Running server\n");
        ret = run_server ();
    } else {
        printf("Running client\n");
        ret = run_client ();
    }
    check (ret == 0, "Failed to run workload");

 error:
    printf("Closing IB connection\n");
    close_ib_connection ();
    printf("Destroying environment\n");
    destroy_env         ();
    return ret;
}    

int init_env ()
{
    if (config_info.is_server) {
	log_fp = fopen ("server.log", "w");
    } else {
	log_fp = fopen ("client.log", "w");
    }
    check (log_fp != NULL, "Failed to open log file");

    log (LOG_HEADER, "IB Echo Server");
    print_config_info ();

    return 0;
 error:
    return -1;
}

void destroy_env ()
{
    log (LOG_HEADER, "Run Finished");
    if (log_fp != NULL) {
        fclose (log_fp);
    }
}
