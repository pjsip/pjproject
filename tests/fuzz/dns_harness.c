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
#include <sys/stat.h>
#include <sys/types.h>

#ifndef CACHING_POOL_SIZE
#   define CACHING_POOL_SIZE   (256*1024*1024)
#endif

int param_log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME |
		      PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_INDENT;

int main(int argc, char ** argv){
	int fd;
	int rc;
	struct stat st;
	pj_caching_pool caching_pool;
	pj_dns_parsed_packet *dns;
	pj_pool_factory * mem = &caching_pool.factory;
	pj_log_set_level(3);
	pj_log_set_decor(param_log_decor);
	rc = pj_init();
	if (rc != 0) {
		puts("PJ GG");
		return 1;
	}
	pj_caching_pool_init( &caching_pool, NULL, 0 );
	pj_pool_t * pool = pj_pool_create(mem, NULL, 1024, 1024, NULL);
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
	pj_status_t status = pj_dns_parse_packet(pool, (void*)data, st.st_size, &dns);//pjmedia_sdp_parse(pool, data, st.st_size, &sdp);
	if (status != PJ_SUCCESS){
		puts("DNS parser GG");
		return 1;
	}
	return 0;
}
