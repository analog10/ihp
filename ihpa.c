#include <string.h>
#include "ihpa.h"

struct ihpa_fillbuf {
	size_t max;
	uint8_t* mem;
};

struct ihpa_range_data {
	struct ihpa_range* ranges;

	unsigned range_count;

	/** @brief Current range being buffered. */
	unsigned cur_range;

	/** @brief Current offset in the range. */
	unsigned cur_offset;

	/** @brief Number of bytes written into buffer. */
	size_t curlength;

	/** @brief Length of following buffer. */
	size_t maxbuff;

	/** @brief Where to buffer RAM so as to meet max user spec. */
	uint8_t buffer[];
};

static bool ihpa_fill_cb(struct ihp_ctx* ctx, uint32_t address, const uint8_t* data, size_t len);

static size_t ihpa_range_size(size_t max_buffer);

static bool ihpa_range_cb(struct ihp_ctx* ctx, uint32_t address,
	const uint8_t* data, size_t len);

unsigned ihpa_populate(size_t img_len, uint8_t* img_mem, uint8_t pad, FILE* input)
{
	/* Initialize the callback data and the initial image. */
	struct ihpa_fillbuf f = {
		.max = img_len
		,.mem = img_mem
	};
	memset(img_mem, pad, img_len);

	/* Initialize the ihp context */
	size_t isize = ihp_size(64);
	uint8_t ihp_mem[isize];
	struct ihp_ctx* ic = ihp_file(ihp_mem, 64, input);
	ic->user_data = &f;
	ic->cb = ihpa_fill_cb;

	unsigned err = ihp_run(ic);
	ihp_destroy(ic);
	return err;
}

static bool ihpa_fill_cb(struct ihp_ctx* ctx, uint32_t address, const uint8_t* data, size_t len)
{
	if(!data){
		return len ? true : false;
	}

	/* Bounds check the data. */
	struct ihpa_fillbuf* f = (struct ihpa_fillbuf*)ctx->user_data;
	if(address + len > f->max)
		return false;

	memcpy(f->mem + address, data, len);
	return true;
}


unsigned ihpa_range_run(struct ihpa_range* ranges, unsigned range_count, size_t max, FILE* input)
{
	/* Make sure all ranges monotonically increase. */
	for(unsigned i = 0; i < range_count - 1; ++i){
		if(ranges[i].start >= ranges[i + 1].start)
			return COUNT_IHP_ERR;
	}

	/* Make sure range does not extend into next range. */
	for(unsigned i = 0; i < range_count - 1; ++i){
		if(ranges[i].start + ranges[i].length > ranges[i + 1].start)
			return COUNT_IHP_ERR;
	}

	/* Initialize the callback data
	 * -Set the cur_range field to an invalid value */
	size_t rsize = ihpa_range_size(max);
	uint8_t ihpa_mem[rsize];
	__auto_type ird  = (struct ihpa_range_data*)ihpa_mem;
	ird->ranges = ranges;
	ird->range_count = range_count;
	ird->cur_range = range_count;
	ird->curlength = 0;
	ird->maxbuff = max;

	/* Initialize the ihp context */
	size_t isize = ihp_size(max);
	uint8_t ihp_mem[isize];
	struct ihp_ctx* ic = ihp_file(ihp_mem, max, input);
	ic->user_data = ird;
	ic->cb = ihpa_range_cb;

	unsigned err = ihp_run(ic);
	ihp_destroy(ic);
	return err;
}


static size_t ihpa_range_size(size_t max_buffer){
	return sizeof(struct ihpa_range_data) + max_buffer;
}

static bool ihpa_range_cb(struct ihp_ctx* ctx, uint32_t address,
	const uint8_t* data, size_t len)
{
	if(!data)
		return len ? true : false;

	/* If no data, then trivially true. */
	if(!len)
		return true;

	__auto_type ird = (struct ihpa_range_data*)ctx->user_data;

	/* If there is a current range defined AND
	 * If the start of this range does not coincide with
	 * the end of the current range, then flush */
	if(ird->cur_range < ird->range_count){
		__auto_type c = ird->ranges + ird->cur_range;
		if(address != c->start + ird->cur_offset){
			/* The address is the start
			 *  plus the current offset (which points to END of current buffer)
			 *  minus the amount of data in the buffer. */
			uint32_t user_address = ird->cur_offset - ird->curlength;
			if(!c->relative)
				user_address += c->start;
			c->ctx.cb(&c->ctx, user_address, ird->buffer, ird->curlength);
			ird->cur_offset = 0;
			ird->curlength = 0;
			ird->cur_range = ird->range_count;

			/* TODO optimization: if address is past this range,
			 * find the next range. */
		}
	}

	/* If no range defined, determine if this block intersects a range. */
	if(ird->cur_range == ird->range_count){
		/* Skip over all ranges that end before the beginning of this data */
		unsigned idx = 0;
		while(idx < ird->range_count &&
			ird->ranges[idx].start + ird->ranges[idx].length <= address)
		{
			++idx;
		}

		/* If no range found, then just discard data. */
		if(idx == ird->range_count)
			return true;

		/* If this range starts past the current window, then discard. */
		__auto_type c = ird->ranges + idx;
		if(c->start > address + len)
			return true;

		/* Otherwise, this range must intersect the data window somewhere. */
		ird->cur_range = idx;
		ird->cur_offset = address - c->start;
		ird->curlength = 0;

		/* If the data window starts before the range,
		 * discard the preceding bytes */
		if(address < c->start){
			unsigned diff = c->start - address;
			address += diff;
			len -= diff;
			data += diff;
		}
	}

	/* At this point we have a defined range, so append data to it. */
	__auto_type c = ird->ranges + ird->cur_range;
	unsigned append = len;

	/* Check against the remaining data in the range. */
	if(append > c->length - ird->cur_offset)
		append = c->length - ird->cur_offset;

	/* Check against remaining data in the buffer. */
	if(append > ird->maxbuff - ird->curlength)
		append = ird->maxbuff - ird->curlength;

	memcpy(ird->buffer + ird->curlength, data, append);
	ird->curlength += append;
	ird->cur_offset += append;
	data += append;
	len -= append;
	address += append;

	/* If the buffer is full, 
	 * OR the range is filled out, then flush. */
	if(ird->curlength == ird->maxbuff || c->length == ird->cur_offset){
		uint32_t user_address = ird->cur_offset - ird->curlength;
		if(!c->relative)
			user_address += c->start;
		c->ctx.cb(&c->ctx, user_address, ird->buffer, ird->curlength);

		/* Flushed the buffer, so reset the length */
		ird->curlength = 0;

		/* If we flushed because the range was finished out, reset
		 * the range variables. */
		if(c->length == ird->cur_offset){
			ird->cur_offset = 0;
			ird->cur_range = ird->range_count;
		}
	}

	return ihpa_range_cb(ctx, address, data, len);
}
