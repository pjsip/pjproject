/* 
 * Copyright (C) 2023 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia/wav_port.h>

#define kMinInputLength 50
#define kMaxInputLength 5120
#define PTIME       20

pj_pool_factory *mem;

int wav_parse(char *filename) {

    pj_status_t status;
    pj_pool_t *pool;
    pjmedia_port *wav_port = NULL;

    pool = pj_pool_create(mem, "wav", 1000, 1000, NULL);

    status = pjmedia_wav_player_port_create(pool, filename, PTIME, PJMEDIA_FILE_NO_LOOP, 0, &wav_port);

    if (wav_port){
        pjmedia_port_destroy(wav_port);
    }

    pj_pool_release(pool);

    return status;
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    int ret = 0;
    char filename[256];
    pj_caching_pool caching_pool;

    sprintf(filename, "/tmp/libfuzzer.%d", getpid());
    FILE *fp = fopen(filename, "wb");

    if (!fp) {
        return 1;
    }

    fwrite(Data, Size, 1, fp);
    fclose(fp);

    /* init Calls */
    pj_init();
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0);
    pj_log_set_level(0);

    mem = &caching_pool.factory;

    /* Call fuzzer */
    ret = wav_parse(filename);

    pj_caching_pool_destroy(&caching_pool);
    unlink(filename);
    return ret;
}
