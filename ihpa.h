#ifndef __IHEX_PARSER_ALGO_H__
#define __IHEX_PARSER_ALGO_H__

#include "ihp.h"

struct ihpa_range {
	/** @brief  */
	struct ihp_ctx ctx;

	/** @brief Start of address range. */
	uint32_t start;

	/** @brief Address range length. */
	size_t length;

	/** @brief If true, call cb with an address relative to the start. */
	bool relative;
};

/** @brief . */
unsigned ihpa_range_run(struct ihpa_range* ranges, unsigned range_count, size_t max, FILE* input);

/** @brief Populate an area of RAM with the hex file contents. */
unsigned ihpa_populate(size_t img_len, uint8_t* img_mem, uint8_t pad, FILE* input);

#endif
