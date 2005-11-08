#include <pj/types.h>

#define INCLUDE_XML_TEST	1

extern int xml_test(void);
extern int test_main(void);

extern void app_perror(const char *title, pj_status_t rc);
extern pj_pool_factory *mem;

