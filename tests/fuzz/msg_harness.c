#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef CACHING_POOL_SIZE
#   define CACHING_POOL_SIZE   (256*1024*1024)
#endif

int param_log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME |
		      PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_INDENT;

static void print_stack(int sig)
{
	void *array[16];
	size_t size;

	size = backtrace(array, 16);
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	exit(1);
}

static void init_signals(void)
{
	signal(SIGSEGV, &print_stack);
	signal(SIGABRT, &print_stack);
}

int main(int argc, char ** argv){
	int fd;
	int rc;
	struct stat st;
	pjsip_endpoint *endpt;
	pj_caching_pool caching_pool;
	pjsip_parser_err_report err_list;
	pjsip_msg *parsed_msg;
	// pj_pool_factory * mem = &caching_pool.factory;
	pj_log_set_level(3);
	pj_log_set_decor(param_log_decor);
	init_signals();
	rc = pj_init();
	if (rc != 0) {
		puts("PJ GG");
		return 1;
	}
	pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, CACHING_POOL_SIZE);
	rc = pjsip_endpt_create(&caching_pool.factory, "msg_fuzz", &endpt);
	if (rc != PJ_SUCCESS){
		puts("endpt GG");
		return 1;
	}
	rc = pjsip_tsx_layer_init_module(endpt);
	if (rc != PJ_SUCCESS){
		puts("transaction GG");
		return 1;
	}
	rc = pjsip_loop_start(endpt, NULL);
	if (rc != PJ_SUCCESS) {
		puts("datagram_loop GG");
		return 1;
	}
	pj_pool_t * pool = pjsip_endpt_create_pool(endpt, NULL, 8000, 8000);
	if (argc != 2) {
		puts("ARG GG");
		return 1;
	}
	if (access(argv[1], R_OK) != 0){
		puts("INFILE GG");
		return 1;
	}
	stat(argv[1], &st);
	char * data = (char*)calloc(st.st_size + 0x10, 1);
	fd = open(argv[1], O_RDONLY);
	rc = read(fd, data, st.st_size);
	if (rc != st.st_size){
		puts("RDFILE GG");
		return 1;
	}
	pj_list_init(&err_list);
	parsed_msg = pjsip_parse_msg(pool, data, st.st_size, &err_list);
	if (parsed_msg == NULL){
		puts("MSG parser GG");
		return 1;
	}
	return 0;
}
