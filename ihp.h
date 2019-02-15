#ifndef __IHEX_PARSER_H__
#define __IHEX_PARSER_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Forward declaration. */
struct ihp_ctx;

/** @brief Callback invoked when parsing intel hex data.
 *  @param user_data Pointer to user specific data
 *  @param address Address of start of data
 *  @param data Pointer to data; If NULL, then error
 *  @param len Length of data; if data is NULL, this is an error code.
 *  @return True if parser should continue; false to abort parsing. */
typedef bool (*ihp_cb)(struct ihp_ctx* ctx, uint32_t address, const uint8_t* data, size_t len);

enum {
	/** @brief No error occurred. */
	IHP_ERR_OK

	/** @brief Data stream ended early */
	,IHP_ERR_EARLY_ABORT

	/** @brief Encountered a CRC error */
	,IHP_ERR_CHECKSUM

	/** @brief Line did not begin with ':" */
	,IHP_ERR_BAD_HEADER

	/** @brief Encountered bad hex character. ':" */
	,IHP_ERR_BAD_HEX

	/** @brief Encountered unknown code */
	,IHP_ERR_UNKNOWN_CODE

	/** @brief Encountered an invalid byte count */
	,IHP_ERR_BAD_BYTE_COUNT

	/** @brief Callback initiated abort. */
	,IHP_ERR_USER_ABORT

	/** @brief Maximum number of errors. */
	,COUNT_IHP_ERR
};

/** @brief Opaque type for parsing context;
 * ACTUAL SIZE OF THE STRUCT MUST BE CALCULATED at run time. */
struct ihp_ctx {
	void* user_data;
	ihp_cb cb;
};

/** @brief Calculate RAM required for context parser instance. */
size_t ihp_size(size_t max_buffer);

/** @brief Initialize a an ihp_ctx with an input file.
 *  @param mem Pointer to raw memory of at least size ihp_size(max_buffer)
 *  @param size_t max_buffer Maximum amount of data to pass in a callback call
 *  @param ifile File descriptor of source data.
 *  @return pointer to initialize mem containing context instance.
 *   returns NULL if there was an error opening the file. */
struct ihp_ctx* ihp_file(uint8_t* mem, size_t max_buffer, FILE* f);

/** @brief Destroy an ihex context */
void ihp_destroy(struct ihp_ctx* ctx);

/** @brief Perform parsing operation.
 *  @return Error status IHP_ERR_* */
unsigned ihp_run(struct ihp_ctx* ctx);

#endif
