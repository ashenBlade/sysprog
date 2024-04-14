#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

const char **extract_filenames(int argc, const char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Использование: %s <file1> <file2> ...\n", argv[0]);
        exit(1);
    }

    fprintf(stderr, "Not implemented\n");
    exit(1);
}
