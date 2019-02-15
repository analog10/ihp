#include <stdlib.h>
#include <string.h>
#include "ihpa.h"

static bool info(struct ihp_ctx* ctx, uint32_t address, const uint8_t* data, size_t len)
{
	if(!data)
		return len ? true : false;

	fprintf(stderr, "INFO %s %04x %zu\n", (const char*)ctx->user_data, address, len);

	return true;
}

static bool interrupt_vectors(struct ihp_ctx* ctx, uint32_t address, const uint8_t* data, size_t len)
{
	if(!data)
		return len ? true : false;

	if(len != 2){
		fprintf(stderr, "Must specify entire IRQ!\n");
		return false;
	}

	uint16_t a;
	memcpy(&a, data, 2);
	a = le16toh(a);

	fprintf(stderr, "IRQ %2lu %04hx\n", (unsigned long) ctx->user_data, a);

	return true;
}

int main(int argc, const char* argv[]){
	struct ihpa_range msp430_ranges[] = {
		{
			.start = 0x1000
			,.length = 0x40
			,.ctx = {
				.cb = info
				,.user_data = "INFO.D"
			}
		}

		,{
			.start = 0x1040
			,.length = 0x40
			,.ctx = {
				.cb = info
				,.user_data = "INFO.C"
			}
		}

		,{
			.start = 0x1080
			,.length = 0x40
			,.ctx = {
				.cb = info
				,.user_data = "INFO.B"
			}
		}

		,{
			.start = 0x10C0
			,.length = 0x40
			,.ctx = {
				.cb = info
				,.user_data = "INFO.A"
			}
		}

		,{
			.start = 0xC000
			,.length = 0x3FDE
			,.ctx = {
				.cb = info
				,.user_data = "FLASH"
			}
		}

		,{
			.start = 0xFFE0
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)1
			}
		}

		,{
			.start = 0xFFE2
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)2
			}
		}

		,{
			.start = 0xFFE4
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)3
			}
		}

		,{
			.start = 0xFFE6
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)4
			}
		}

		,{
			.start = 0xFFE8
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)5
			}
		}

		,{
			.start = 0xFFEA
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)6
			}
		}

		,{
			.start = 0xFFEC
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)7
			}
		}

		,{
			.start = 0xFFEE
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)8
			}
		}

		,{
			.start = 0xFFF0
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)9
			}
		}

		,{
			.start = 0xFFF2
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)10
			}
		}

		,{
			.start = 0xFFF4
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)11
			}
		}

		,{
			.start = 0xFFF6
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)12
			}
		}

		,{
			.start = 0xFFF8
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)13
			}
		}

		,{
			.start = 0xFFFA
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)14
			}
		}

		,{
			.start = 0xFFFC
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)15
			}
		}

		,{
			.start = 0xFFFE
			,.length = 2
			,.ctx = {
				.cb = interrupt_vectors
				,.user_data = (void*)16
			}
		}

	};

	unsigned err = ihpa_range_run(
		msp430_ranges, sizeof(msp430_ranges) / sizeof(msp430_ranges[0]), 64, stdin);
	if(err){
		fprintf(stderr, "Encountered error %u\n", err);
		return 1;
	}

	return 0;
}
