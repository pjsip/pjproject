/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pj/xml.h>
#include <stdio.h>
#include <stdlib.h>
#include <pj/pool.h>
#include "libpj_test.h"
#include <string.h>

static const char *test_files[] = {
    "../src/test/pidf-diff.xml"
};

static int xml_parse_print_test(const char *file)
{
    FILE *fhnd = fopen(file, "rb");
    if (!fhnd) {
	printf("  Error: unable to open file %s\n", file);
	return 1;
    }

    char *msg = (char*)malloc(10*1024);
    int len = fread(msg, 1, 10*1024, fhnd);
    if (len < 1) {
	return -1;
    }

    fclose(fhnd);

    pj_pool_t *pool = pj_pool_create(mem, "xml", 1024, 1024, NULL);
    pj_xml_node *root = pj_xml_parse(pool, msg, len);
    if (!root) {
	printf("  Error: unable to parse XML file %s\n", file);
	return -1;
    }

    char *output = (char*)malloc(len + 512);
    memset(output, 0, len+512);
    int output_len = pj_xml_print(root, output, len+512, PJ_TRUE);
    if (output_len < 1) {
	printf("  Error: buffer too small to print XML file %s\n", file);
	return -1;
    }
    output[output_len] = '\0';

    fhnd = fopen("out.xml", "wb");
    fwrite(output, output_len, 1, fhnd);
    fclose(fhnd);

    pj_pool_release(pool);
    free(output);
    free(msg);

    return 0;
}

int xml_test()
{
    int i;
    for (i=0; i<sizeof(test_files)/sizeof(test_files[0]); ++i) {
	if (xml_parse_print_test(test_files[i]) != 0)
	    return -1;
    }
    return 0;
}
