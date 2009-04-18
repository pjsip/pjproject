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


