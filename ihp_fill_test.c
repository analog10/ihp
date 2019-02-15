#include <stdlib.h>
#include <string.h>
#include "ihpa.h"

int main(int argc, const char* argv[]){
	if(argc < 3){
		fprintf(stderr, "Usage: %s IMG_SIZE FILL_BYTE\n", argv[0]);
		return 1;
	}

	/* Parse image size from command line */
	size_t img_size = strtoul(argv[1], NULL, 10);

	/* Parse fille byte command line */
	uint8_t b = strtoul(argv[2], NULL, 16);

	/* Initialize initial image. */
	uint8_t mem[img_size];

	unsigned err = ihpa_populate(img_size, mem, b, stdin);
	if(err){
		fprintf(stderr, "Encountered error %u\n", err);
		return 1;
	}

	/* Write binary data to stdout. */
	fwrite(mem, 1, img_size, stdout);

	return 0;
}
