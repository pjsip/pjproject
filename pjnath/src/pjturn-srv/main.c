#include "turn.h"

int err(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    printf("%s: %s\n", title, errmsg);
    return 1;
}

int main()
{
    pj_caching_pool cp;
    pj_turn_srv *srv;
    pj_turn_listener *listener;
    pj_status_t status;

    status = pj_init();
    if (status != PJ_SUCCESS)
	return err("pj_init() error", status);

    pj_caching_pool_init(&cp, NULL, 0);

    status = pj_turn_srv_create(&cp.factory, &srv);
    if (status != PJ_SUCCESS)
	return err("Error creating server", status);

    status = pj_turn_listener_create_udp(srv, pj_AF_INET(), NULL, 3478, 1, 0, &listener);
    if (status != PJ_SUCCESS)
	return err("Error creating listener", status);

    status = pj_turn_srv_add_listener(srv, listener);
    if (status != PJ_SUCCESS)
	return err("Error adding listener", status);

    puts("Server is running");
    puts("Press <ENTER> to quit");

    {
	char line[10];
	fgets(line, sizeof(line), stdin);
    }

    pj_turn_srv_destroy(srv);
    pj_caching_pool_destroy(&cp);
    pj_shutdown();

    return 0;
}

