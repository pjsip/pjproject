/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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

int main(int argc, char *argv[])
{
    FILE *in, *out;
    char frm[2];
    unsigned count;

    if (argc != 3) {
        puts("Usage: swapendian input.pcm OUTPUT.PCM");
        return 1;
    }

    in = fopen(argv[1], "rb");
    if (!in) {
        puts("Open error");
        return 1;
    }

    out = fopen(argv[2], "wb");
    if (!out) {
        puts("Open error");
        fclose(in);
        return 1;
    }

    count = 0;
    for (;;) {
        char tmp;

        if (fread(frm, 2, 1, in) != 1)
            break;

        tmp = frm[0];
        frm[0] = frm[1];
        frm[1] = tmp;

        if (fwrite(frm, 2, 1, out) != 1) {
            puts("Write error");
            break;
        }

        ++count;
    }

    printf("%d samples converted\n", count);

    fclose(in);
    fclose(out);

    return 0;
}


