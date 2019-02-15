#include <endian.h>
#include <string.h>
#include <assert.h>

#include "ihp.h"

/* Parsing support includes hex codes up to 05
 * The maximum fixed length line is code 03, as in
 *
 * :0400000300003800C1\r\n
 *
 * which is 21 bytes
 *
 * */

#define MAX_HEADER 19

enum {
	ST_BEGIN
	,ST_PARSING
	,ST_END
	,ST_ERROR
};

enum {
	IHP_CODE_DATA
	,IHP_CODE_EOF
	,IHP_CODE_EXT_SEG
	,IHP_CODE_START_SEG
	,IHP_CODE_EXT_LIN
	,IHP_CODE_START_LIN
};

struct IHP {
	struct ihp_ctx ctx;
	unsigned state;
	FILE* f;

	uint32_t base_address;
	uint32_t next_address;

	/** @brief Address at the start of the buffer. */
	uint32_t bufaddress;

	size_t bufpos;
	size_t max;
	uint8_t buffer[];
};

static uint32_t checksum(const uint8_t* src, size_t len, unsigned checksum);
static uint8_t fin(unsigned checksum);
static int ihp_hex_parse(uint8_t* dest, const char* src, size_t src_len);
static bool ihp_on_payload(struct IHP* ctx, const uint8_t* src, size_t src_len);

size_t ihp_size(size_t max_buffer){
	return sizeof(struct IHP) + 2 * max_buffer;
}

struct ihp_ctx* ihp_file(uint8_t* mem, size_t max_buffer, FILE* f){
	memset(mem, 0, ihp_size(max_buffer));
	__auto_type ret = (struct IHP*)mem;

	ret->f = f;
	ret->max = max_buffer;
	return &(ret->ctx);
}

void ihp_destroy(struct ihp_ctx* ctx){
	__auto_type ihp = (struct IHP*)ctx;

	/* If callbacks are set, then call them if not finished parsing. */
	if(ST_ERROR != ihp->state && ST_END != ihp->state){
		if(ihp->ctx.cb)
			ihp->ctx.cb(&ihp->ctx, 0, NULL, IHP_ERR_EARLY_ABORT);
	}

	if(ihp->f)
		fclose(ihp->f);
}

unsigned ihp_run(struct ihp_ctx* ctx){
	__auto_type ihp = (struct IHP*)ctx;
	ihp->state = ST_PARSING;

	char ibuff[MAX_HEADER];
	uint8_t hbuff[1 + 2 + 1 + 4 + 1];

	int ret;
	unsigned err = IHP_ERR_OK;
	while(ihp->state == ST_PARSING && (ret = fread(ibuff, 1, 9, ihp->f))){
		if(ret < 9){
			err = IHP_ERR_EARLY_ABORT;
			break;
		}

		/* Check that line begins with ':' */
		if(ibuff[0] != ':'){
			err = IHP_ERR_BAD_HEADER;
			break;
		}

		uint8_t byte_count;
		uint16_t hdr_address;
		uint8_t code;
		if(ihp_hex_parse(hbuff, ibuff + 1, 8) < 0){
			err = IHP_ERR_BAD_HEX;
			break;
		}

		byte_count = hbuff[0];
		memcpy(&hdr_address, hbuff + 1, 2);
		hdr_address = be16toh(hdr_address);
		code = hbuff[3];

		if(code > 5){
			err = IHP_ERR_BAD_HEX;
			break;
		}

		uint32_t ck = checksum(hbuff, 4, 0);
		if(IHP_CODE_DATA == code){
			/* Check the line address.
			 * If this does not match next_address, then
			 * flush the current data buffer. */
			uint32_t addr = ihp->base_address + hdr_address;
			if(addr != ihp->next_address){
				if(!ihp_on_payload(ihp, NULL, 0)){
					err = IHP_ERR_USER_ABORT;
					break;
				}

				/* Reset the buffer address */
				ihp->bufaddress = addr;
				ihp->next_address = addr + byte_count;
			}
			else{
				ihp->next_address += byte_count;
			}

			/* Read in remaining data + checksum */
			unsigned data_left = byte_count * 2;

			while(data_left){
				/* Can only read in what is remaining in buffer. */
				unsigned cur = data_left;
				uint8_t* dest = ihp->buffer + ((ihp->bufpos + 1) & ~1);
				if(cur > 2 * ihp->max - (dest - ihp->buffer))
					cur = 2 * ihp->max - (dest - ihp->buffer);
				ret = fread(dest, 1, cur, ihp->f);
				if(ret < cur){
					err = IHP_ERR_EARLY_ABORT;
					break;
				}

				/* Convert ASCII hex to regular hex. */
				if(ihp_hex_parse(dest, (char*)dest, cur) < 0){
					err = IHP_ERR_BAD_HEX;
					break;
				}

				/* Update checksum. */
				ck = checksum(dest, cur / 2, ck);

				if(!ihp_on_payload(ihp, dest, cur / 2)){
					err = IHP_ERR_USER_ABORT;
					break;
				}

				data_left -= cur;
			}
			if(err != IHP_ERR_OK)
				break;

		}
		else if(IHP_CODE_EOF == code){
			/* Byte count must be 0. */
			if(byte_count){
				err = IHP_ERR_BAD_BYTE_COUNT;
				break;
			}

			/* Flush data buffer. */
			if(!ihp_on_payload(ihp, NULL, 0))
				err = IHP_ERR_USER_ABORT;

			/* Address is a don't care. */
			ihp->state = ST_END;

			/* Nothing more to do, just break.  */
			break;
		}
		else if(IHP_CODE_EXT_SEG == code){
			/* Byte count must be 2. */
			if(byte_count != 2){
				err = IHP_ERR_BAD_BYTE_COUNT;
				break;
			}

			if(fread(ibuff, 1, 4, ihp->f) < 4){
				err = IHP_ERR_EARLY_ABORT;
				break;
			}

			if(ihp_hex_parse(hbuff, ibuff, 4) < 0){
				err = IHP_ERR_BAD_HEX;
				break;
			}

			ck = checksum(hbuff, 2, ck);

			/* Copy payload address, scale as per spec */
			memcpy(&hdr_address, hbuff, 2);
			hdr_address = be16toh(hdr_address);
			ihp->base_address = hdr_address;
			ihp->base_address *= 16;
		}
		else if(IHP_CODE_START_SEG == code){
			/* Byte count must be 4. */
			if(byte_count != 4){
				err = IHP_ERR_BAD_BYTE_COUNT;
				break;
			}

			if(fread(ibuff, 1, 8, ihp->f) < 8){
				err = IHP_ERR_EARLY_ABORT;
				break;
			}

			if(ihp_hex_parse(hbuff, ibuff, 8) < 0){
				err = IHP_ERR_BAD_HEX;
				break;
			}

			ck = checksum(hbuff, 4, ck);

			/* Do nothing....as per nrf-intel-hex behaviour */

		}
		else if(IHP_CODE_EXT_LIN == code){
			/* Byte count must be 2. */
			if(byte_count != 2){
				err = IHP_ERR_BAD_BYTE_COUNT;
				break;
			}

			if(fread(ibuff, 1, 4, ihp->f) < 4){
				err = IHP_ERR_EARLY_ABORT;
				break;
			}

			if(ihp_hex_parse(hbuff, ibuff, 4) < 0){
				err = IHP_ERR_BAD_HEX;
				break;
			}

			ck = checksum(hbuff, 2, ck);

			/* Copy payload address, scale as per spec */
			memcpy(&hdr_address, hbuff, 2);
			hdr_address = be16toh(hdr_address);
			ihp->base_address = hdr_address;
			ihp->base_address <<= 16;
		}
		else if(IHP_CODE_START_LIN == code){
			/* Byte count must be 4. */
			if(byte_count != 4){
				err = IHP_ERR_BAD_BYTE_COUNT;
				break;
			}

			if(fread(ibuff, 1, 8, ihp->f) < 8){
				err = IHP_ERR_EARLY_ABORT;
				break;
			}

			if(ihp_hex_parse(hbuff, ibuff, 8) < 0){
				err = IHP_ERR_BAD_HEX;
				break;
			}

			ck = checksum(hbuff, 4, ck);

			/* Do nothing....as per nrf-intel-hex behaviour */
		}

		/* Read last 2 checksum bytes and the \r\n,
		 * parse the checksum
		 * and verify checksum */
		char ckbuff[4];
		ret = fread(ckbuff, 1, 4, ihp->f);
		if(ret < 4){
			err = IHP_ERR_EARLY_ABORT;
		}
		else {
			uint8_t c;
			if(ihp_hex_parse(&c, ckbuff, 2) < 0){
				err = IHP_ERR_BAD_HEX;
			}
			else if(c != fin(ck)){
				err = IHP_ERR_CHECKSUM;
			}
		}
	}

	ihp->ctx.cb(&ihp->ctx, 0, NULL, err);
	return err;
}

/* Internal functions. */

static uint32_t checksum(const uint8_t* src, size_t len, unsigned checksum){
	for(unsigned i = 0; i < len; ++i)
		checksum += src[i];
	return checksum;
}

static uint8_t fin(unsigned checksum){
	checksum = ~checksum;
	++checksum;
	return checksum & 0xFF;
}

static int ihp_hex_parse(uint8_t* dest, const char* src, size_t src_len){
	/* Only even lengths are valid. */
	if(src_len & 1)
		return -1;

	const char* begin = src;
	const char* end = src + src_len;
	while(src != end){
		if(src[0] >= '0' && src[0] <= '9')
			*dest = src[0] - '0';
		else if(src[0] >= 'A' && src[0] <= 'F')
			*dest = src[0] - 'A' + 0xA;
		else
			return -(src - begin);
		*dest <<= 4;

		if(src[1] >= '0' && src[1] <= '9')
			*dest |= src[1] - '0';
		else if(src[1] >= 'A' && src[1] <= 'F')
			*dest |= src[1] - 'A' + 0xA;
		else
			return -(src - begin - 1);

		++dest;
		src += 2;
	}

	return end - begin;
}


static bool ihp_on_payload(struct IHP* ihp, const uint8_t* src, size_t src_len){
	/* If no data, then this is a flush, due to an address change. */
	if(!src){
		if(!ihp->ctx.cb || !ihp->bufpos)
			return true;

		size_t len = ihp->bufpos;
		ihp->bufpos = 0;
		return ihp->ctx.cb(&ihp->ctx, ihp->bufaddress, ihp->buffer, len);
	}

	while(src_len){
		unsigned amt = src_len;
		if(amt + ihp->bufpos > ihp->max)
			amt = ihp->max - ihp->bufpos;

		assert(amt);

		memmove(ihp->buffer + ihp->bufpos, src, amt);
		ihp->bufpos += amt;
		src_len -= amt;
		src += amt;

		if(ihp->bufpos == ihp->max){
			if(!ihp->ctx.cb(&ihp->ctx, ihp->bufaddress, ihp->buffer, ihp->max))
				return false;
			ihp->bufaddress += ihp->max;
			ihp->bufpos = 0;
		}
	}

	return true;
}
