/*
 * librdkafka - Apache Kafka C library
 *
 * Copyright (c) 2012-2015, Magnus Edenhill
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _RDKAFKA_BUF_H_
#define _RDKAFKA_BUF_H_

#include "rdkafka_int.h"
#include "rdcrc32.h"
#include "rdlist.h"
#include "rdbuf.h"


typedef struct rd_kafka_broker_s rd_kafka_broker_t;

#define RD_KAFKA_HEADERS_IOV_CNT   2


/**
 * Temporary buffer with memory aligned writes to accommodate
 * effective and platform safe struct writes.
 */
typedef struct rd_tmpabuf_s {
	size_t size;
	size_t of;
	char  *buf;
	int    failed;
	int    assert_on_fail;
} rd_tmpabuf_t;

/**
 * @brief Allocate new tmpabuf with \p size bytes pre-allocated.
 */
static RD_UNUSED void
rd_tmpabuf_new (rd_tmpabuf_t *tab, size_t size, int assert_on_fail) {
	tab->buf = rd_malloc(size);
	tab->size = size;
	tab->of = 0;
	tab->failed = 0;
	tab->assert_on_fail = assert_on_fail;
}

/**
 * @brief Free memory allocated by tmpabuf
 */
static RD_UNUSED void
rd_tmpabuf_destroy (rd_tmpabuf_t *tab) {
	rd_free(tab->buf);
}

/**
 * @returns 1 if a previous operation failed.
 */
static RD_UNUSED RD_INLINE int
rd_tmpabuf_failed (rd_tmpabuf_t *tab) {
	return tab->failed;
}

/**
 * @brief Allocate \p size bytes for writing, returning an aligned pointer
 *        to the memory.
 * @returns the allocated pointer (within the tmpabuf) on success or
 *          NULL if the requested number of bytes + alignment is not available
 *          in the tmpabuf.
 */
static RD_UNUSED void *
rd_tmpabuf_alloc0 (const char *func, int line, rd_tmpabuf_t *tab, size_t size) {
	void *ptr;

	if (unlikely(tab->failed))
		return NULL;

	if (unlikely(tab->of + size > tab->size)) {
		if (tab->assert_on_fail) {
			fprintf(stderr,
				"%s: %s:%d: requested size %zd + %zd > %zd\n",
				__FUNCTION__, func, line, tab->of, size,
				tab->size);
			assert(!*"rd_tmpabuf_alloc: not enough size in buffer");
		}
		return NULL;
	}

        ptr = (void *)(tab->buf + tab->of);
	tab->of += RD_ROUNDUP(size, 8);

	return ptr;
}

#define rd_tmpabuf_alloc(tab,size) \
	rd_tmpabuf_alloc0(__FUNCTION__,__LINE__,tab,size)

/**
 * @brief Write \p buf of \p size bytes to tmpabuf memory in an aligned fashion.
 *
 * @returns the allocated and written-to pointer (within the tmpabuf) on success
 *          or NULL if the requested number of bytes + alignment is not available
 *          in the tmpabuf.
 */
static RD_UNUSED void *
rd_tmpabuf_write0 (const char *func, int line,
		   rd_tmpabuf_t *tab, const void *buf, size_t size) {
	void *ptr = rd_tmpabuf_alloc0(func, line, tab, size);

	if (ptr)
		memcpy(ptr, buf, size);

	return ptr;
}
#define rd_tmpabuf_write(tab,buf,size) \
	rd_tmpabuf_write0(__FUNCTION__, __LINE__, tab, buf, size)


/**
 * @brief Wrapper for rd_tmpabuf_write() that takes a nul-terminated string.
 */
static RD_UNUSED char *
rd_tmpabuf_write_str0 (const char *func, int line,
		       rd_tmpabuf_t *tab, const char *str) {
	return rd_tmpabuf_write0(func, line, tab, str, strlen(str)+1);
}
#define rd_tmpabuf_write_str(tab,str) \
	rd_tmpabuf_write_str0(__FUNCTION__, __LINE__, tab, str)



/**
 * @name Read buffer interface
 *
 * Memory reading helper macros to be used when parsing network responses.
 *
 * Assumptions:
 *   - an 'err_parse:' goto-label must be available for error bailouts,
 *                     the error code will be set in rkbuf->rkbuf_err
 *   - local `int log_decode_errors` variable set to the logging level
 *     to log parse errors (or 0 to turn off logging).
 */

#define rd_kafka_buf_parse_fail(rkbuf,...) do {				\
                if (log_decode_errors > 0) {                            \
			rd_kafka_assert(NULL, rkbuf->rkbuf_rkb);	\
                        rd_rkb_log(rkbuf->rkbuf_rkb, log_decode_errors, \
                                   "PROTOERR",                          \
                                   "Protocol parse failure "            \
                                   "at %"PRIusz"/%"PRIusz" (%s:%i) "    \
                                   "(incorrect broker.version.fallback?)", \
                                   rd_slice_offset(&rkbuf->rkbuf_reader), \
                                   rd_slice_size(&rkbuf->rkbuf_reader), \
                                   __FUNCTION__, __LINE__);             \
                        rd_rkb_log(rkbuf->rkbuf_rkb, log_decode_errors, \
				   "PROTOERR", __VA_ARGS__);		\
                }                                                       \
                (rkbuf)->rkbuf_err = RD_KAFKA_RESP_ERR__BAD_MSG;        \
                goto err_parse;                                         \
	} while (0)

/**
 * @name Fail buffer reading due to buffer underflow.
 */
#define rd_kafka_buf_underflow_fail(rkbuf,wantedlen,...) do {           \
                if (log_decode_errors > 0) {                            \
                        rd_kafka_assert(NULL, rkbuf->rkbuf_rkb);        \
                        char __tmpstr[256];                             \
                        rd_snprintf(__tmpstr, sizeof(__tmpstr),         \
                                    ": " __VA_ARGS__);                  \
                        if (strlen(__tmpstr) == 2) __tmpstr[0] = '\0';  \
                        rd_rkb_log(rkbuf->rkbuf_rkb, log_decode_errors, \
                                   "PROTOUFLOW",                        \
                                   "Protocol read buffer underflow "    \
                                   "at %"PRIusz"/%"PRIusz" (%s:%i): "   \
                                   "expected %"PRIusz" bytes > "        \
                                   "%"PRIusz" remaining bytes (%s)%s",  \
                                   rd_slice_offset(&rkbuf->rkbuf_reader), \
                                   rd_slice_size(&rkbuf->rkbuf_reader), \
                                   __FUNCTION__, __LINE__,              \
                                   wantedlen,                           \
                                   rd_slice_remains(&rkbuf->rkbuf_reader), \
                                   rkbuf->rkbuf_uflow_mitigation ?      \
                                   rkbuf->rkbuf_uflow_mitigation :      \
                                   "incorrect broker.version.fallback?", \
                                   __tmpstr);                           \
                }                                                       \
                (rkbuf)->rkbuf_err = RD_KAFKA_RESP_ERR__UNDERFLOW;      \
                goto err_parse;                                         \
        } while (0)


/**
 * Returns the number of remaining bytes available to read.
 */
#define rd_kafka_buf_read_remain(rkbuf) \
        rd_slice_remains(&(rkbuf)->rkbuf_reader)

/**
 * Checks that at least 'len' bytes remain to be read in buffer, else fails.
 */
#define rd_kafka_buf_check_len(rkbuf,len) do {                          \
                size_t __len0 = (size_t)(len);                          \
                if (unlikely(__len0 > rd_kafka_buf_read_remain(rkbuf))) { \
                        rd_kafka_buf_underflow_fail(rkbuf, __len0);     \
                }                                                       \
        } while (0)

/**
 * Skip (as in read and ignore) the next 'len' bytes.
 */
#define rd_kafka_buf_skip(rkbuf, len) do {                              \
                size_t __len1 = (size_t)(len);                          \
                if (__len1 &&                                           \
                    !rd_slice_read(&(rkbuf)->rkbuf_reader, NULL, __len1)) \
                        rd_kafka_buf_check_len(rkbuf, __len1);           \
        } while (0)

/**
 * Skip (as in read and ignore) up to fixed position \p pos.
 */
#define rd_kafka_buf_skip_to(rkbuf, pos) do {                           \
                size_t __len1 = (size_t)(pos) -                         \
                        rd_slice_offset(&(rkbuf)->rkbuf_reader);        \
                if (__len1 &&                                           \
                    !rd_slice_read(&(rkbuf)->rkbuf_reader, NULL, __len1)) \
                        rd_kafka_buf_check_len(rkbuf, __len1);           \
        } while (0)



/**
 * Read 'len' bytes and copy to 'dstptr'
 */
#define rd_kafka_buf_read(rkbuf,dstptr,len) do {                        \
                size_t __len2 = (size_t)(len);                          \
                if (!rd_slice_read(&(rkbuf)->rkbuf_reader, dstptr, __len2))  \
                        rd_kafka_buf_check_len(rkbuf, __len2);          \
        } while (0)


/**
 * @brief Read \p len bytes at slice offset \p offset and copy to \p dstptr
 *        without affecting the current reader position.
 */
#define rd_kafka_buf_peek(rkbuf,offset,dstptr,len) do {                 \
                size_t __len2 = (size_t)(len);                          \
                if (!rd_slice_peek(&(rkbuf)->rkbuf_reader, offset,      \
                                   dstptr, __len2))                     \
                        rd_kafka_buf_check_len(rkbuf, (offset)+(__len2)); \
        } while (0)


/**
 * Read a 16,32,64-bit integer and store it in 'dstptr'
 */
#define rd_kafka_buf_read_i64(rkbuf,dstptr) do {                        \
                int64_t _v;                                             \
                rd_kafka_buf_read(rkbuf, &_v, sizeof(_v));              \
                *(dstptr) = be64toh(_v);                                \
        } while (0)

#define rd_kafka_buf_peek_i64(rkbuf,of,dstptr) do {                     \
                int64_t _v;                                             \
                rd_kafka_buf_peek(rkbuf, of, &_v, sizeof(_v));          \
                *(dstptr) = be64toh(_v);                                \
        } while (0)

#define rd_kafka_buf_read_i32(rkbuf,dstptr) do {                        \
                int32_t _v;                                             \
                rd_kafka_buf_read(rkbuf, &_v, sizeof(_v));              \
                *(dstptr) = be32toh(_v);                                \
        } while (0)

/* Same as .._read_i32 but does a direct assignment.
 * dst is assumed to be a scalar, not pointer. */
#define rd_kafka_buf_read_i32a(rkbuf, dst) do {				\
                int32_t _v;                                             \
		rd_kafka_buf_read(rkbuf, &_v, 4);			\
		dst = (int32_t) be32toh(_v);				\
	} while (0)

#define rd_kafka_buf_read_i16(rkbuf,dstptr) do {                        \
                int16_t _v;                                             \
                rd_kafka_buf_read(rkbuf, &_v, sizeof(_v));              \
                *(dstptr) = be16toh(_v);                                \
        } while (0)


#define rd_kafka_buf_read_i16a(rkbuf, dst) do {				\
                int16_t _v;                                             \
		rd_kafka_buf_read(rkbuf, &_v, 2);			\
                dst = (int16_t)be16toh(_v);				\
	} while (0)

#define rd_kafka_buf_read_i8(rkbuf, dst) rd_kafka_buf_read(rkbuf, dst, 1)

#define rd_kafka_buf_peek_i8(rkbuf,of,dst) rd_kafka_buf_peek(rkbuf,of,dst,1)


/**
 * @brief Read varint and store in int64_t \p dst
 */
#define rd_kafka_buf_read_varint(rkbuf,dst) do {                        \
                int64_t _v;                                             \
                size_t _r = rd_varint_dec_slice(&(rkbuf)->rkbuf_reader, &_v); \
                if (unlikely(RD_UVARINT_UNDERFLOW(_r)))                 \
                        rd_kafka_buf_underflow_fail(rkbuf, (size_t)0,   \
                                                    "varint parsing failed");\
                *(dst) = _v;                                            \
        } while (0)

/* Read Kafka String representation (2+N).
 * The kstr data will be updated to point to the rkbuf. */
#define rd_kafka_buf_read_str(rkbuf, kstr) do {                         \
                int _klen;                                              \
                rd_kafka_buf_read_i16a(rkbuf, (kstr)->len);             \
                _klen = RD_KAFKAP_STR_LEN(kstr);                        \
                if (RD_KAFKAP_STR_LEN0(_klen) == 0)                     \
                        (kstr)->str = NULL;                             \
                else if (!((kstr)->str =                                \
                           rd_slice_ensure_contig(&rkbuf->rkbuf_reader, \
                                                     _klen)))           \
                        rd_kafka_buf_check_len(rkbuf, _klen);           \
        } while (0)

/* Read Kafka String representation (2+N) and write it to the \p tmpabuf
 * with a trailing nul byte. */
#define rd_kafka_buf_read_str_tmpabuf(rkbuf, tmpabuf, dst) do {		\
                rd_kafkap_str_t _kstr;					\
		size_t _slen;						\
		char *_dst;						\
		rd_kafka_buf_read_str(rkbuf, &_kstr);			\
		_slen = RD_KAFKAP_STR_LEN(&_kstr);			\
		if (!(_dst =						\
		      rd_tmpabuf_write(tmpabuf, _kstr.str, _slen+1)))	\
			rd_kafka_buf_parse_fail(			\
				rkbuf,					\
				"Not enough room in tmpabuf: "		\
				"%"PRIusz"+%"PRIusz			\
				" > %"PRIusz,				\
				(tmpabuf)->of, _slen+1, (tmpabuf)->size); \
		_dst[_slen] = '\0';					\
		dst = (void *)_dst;					\
	} while (0)

/**
 * Skip a string.
 */
#define rd_kafka_buf_skip_str(rkbuf) do {			\
		int16_t _slen;					\
		rd_kafka_buf_read_i16(rkbuf, &_slen);		\
		rd_kafka_buf_skip(rkbuf, RD_KAFKAP_STR_LEN0(_slen));	\
	} while (0)

/* Read Kafka Bytes representation (4+N).
 *  The 'kbytes' will be updated to point to rkbuf data */
#define rd_kafka_buf_read_bytes(rkbuf, kbytes) do {                     \
                int _klen;                                              \
                rd_kafka_buf_read_i32a(rkbuf, _klen);                   \
                (kbytes)->len = _klen;                                  \
                if (RD_KAFKAP_BYTES_IS_NULL(kbytes)) {                  \
                        (kbytes)->data = NULL;                          \
                        (kbytes)->len = 0;                              \
                } else if (RD_KAFKAP_BYTES_LEN(kbytes) == 0)            \
                        (kbytes)->data = "";                            \
                else if (!((kbytes)->data =                             \
                           rd_slice_ensure_contig(&(rkbuf)->rkbuf_reader, \
                                                  _klen)))              \
                        rd_kafka_buf_check_len(rkbuf, _klen);           \
        } while (0)


/**
 * @brief Read \p size bytes from buffer, setting \p *ptr to the start
 *        of the memory region.
 */
#define rd_kafka_buf_read_ptr(rkbuf,ptr,size) do {                      \
                size_t _klen = size;                                    \
                if (!(*(ptr) = (void *)                                 \
                      rd_slice_ensure_contig(&(rkbuf)->rkbuf_reader, _klen))) \
                        rd_kafka_buf_check_len(rkbuf, _klen);           \
        } while (0)


/**
 * @brief Read varint-lengted Kafka Bytes representation
 */
#define rd_kafka_buf_read_bytes_varint(rkbuf,kbytes) do {               \
                int64_t _len2;                                          \
                size_t _r = rd_varint_dec_slice(&(rkbuf)->rkbuf_reader, \
                                                &_len2);                \
                if (unlikely(RD_UVARINT_UNDERFLOW(_r)))                 \
                        rd_kafka_buf_underflow_fail(rkbuf, (size_t)0,   \
                                                    "varint parsing failed"); \
                (kbytes)->len = (int32_t)_len2;                         \
                if (RD_KAFKAP_BYTES_IS_NULL(kbytes)) {                  \
                        (kbytes)->data = NULL;                          \
                        (kbytes)->len = 0;                              \
                } else if (RD_KAFKAP_BYTES_LEN(kbytes) == 0)            \
                        (kbytes)->data = "";                            \
                else if (!((kbytes)->data =                             \
                           rd_slice_ensure_contig(&(rkbuf)->rkbuf_reader, \
                                                  (size_t)_len2)))      \
                        rd_kafka_buf_check_len(rkbuf, _len2);           \
        } while (0)


/**
 * Response handling callback.
 *
 * NOTE: Callbacks must check for 'err == RD_KAFKA_RESP_ERR__DESTROY'
 *       which indicates that some entity is terminating (rd_kafka_t, broker,
 *       toppar, queue, etc) and the callback may not be called in the
 *       correct thread. In this case the callback must perform just
 *       the most minimal cleanup and dont trigger any other operations.
 *
 * NOTE: rkb, reply and request may be NULL, depending on error situation.
 */
typedef void (rd_kafka_resp_cb_t) (rd_kafka_t *rk,
				   rd_kafka_broker_t *rkb,
                                   rd_kafka_resp_err_t err,
                                   rd_kafka_buf_t *reply,
                                   rd_kafka_buf_t *request,
                                   void *opaque);

struct rd_kafka_buf_s { /* rd_kafka_buf_t */
	TAILQ_ENTRY(rd_kafka_buf_s) rkbuf_link;

	int32_t rkbuf_corrid;

	rd_ts_t rkbuf_ts_retry;    /* Absolute send retry time */

	int     rkbuf_flags; /* RD_KAFKA_OP_F */

        rd_buf_t rkbuf_buf;        /**< Send/Recv byte buffer */
        rd_slice_t rkbuf_reader;   /**< Buffer slice reader for rkbuf_buf */

	int     rkbuf_connid;      /* broker connection id (used when buffer
				    * was partially sent). */
        size_t  rkbuf_totlen;      /* recv: total expected length,
                                    * send: not used */

	rd_crc32_t rkbuf_crc;      /* Current CRC calculation */

	struct rd_kafkap_reqhdr rkbuf_reqhdr;   /* Request header.
                                                 * These fields are encoded
                                                 * and written to output buffer
                                                 * on buffer finalization. */
	struct rd_kafkap_reshdr rkbuf_reshdr;   /* Response header.
                                                 * Decoded fields are copied
                                                 * here from the buffer
                                                 * to provide an ease-of-use
                                                 * interface to the header */

	int32_t rkbuf_expected_size;  /* expected size of message */

        rd_kafka_replyq_t   rkbuf_replyq;       /* Enqueue response on replyq */
        rd_kafka_replyq_t   rkbuf_orig_replyq;  /* Original replyq to be used
                                                 * for retries from inside
                                                 * the rkbuf_cb() callback
                                                 * since rkbuf_replyq will
                                                 * have been reset. */
        rd_kafka_resp_cb_t *rkbuf_cb;           /* Response callback */
        struct rd_kafka_buf_s *rkbuf_response;  /* Response buffer */

        struct rd_kafka_broker_s *rkbuf_rkb;

	rd_refcnt_t rkbuf_refcnt;
	void   *rkbuf_opaque;

	int     rkbuf_retries;            /* Retries so far. */
#define RD_KAFKA_BUF_NO_RETRIES  1000000  /* Do not retry */

        int     rkbuf_features;   /* Required feature(s) that must be
                                   * supported by broker. */

	rd_ts_t rkbuf_ts_enq;
	rd_ts_t rkbuf_ts_sent;    /* Initially: Absolute time of transmission,
				   * after response: RTT. */

        /* Request timeouts:
         *  rkbuf_ts_timeout is the effective absolute request timeout used
         *  by the timeout scanner to see if a request has timed out.
         *  It is set when a request is enqueued on the broker transmit
         *  queue based on the relative or absolute timeout:
         *
         *  rkbuf_rel_timeout is the per-request-transmit relative timeout,
         *  this value is reused for each sub-sequent retry of a request.
         *
         *  rkbuf_abs_timeout is the absolute request timeout, spanning
         *  all retries.
         *  This value is effectively limited by socket.timeout.ms for
         *  each transmission, but the absolute timeout for a request's
         *  lifetime is the absolute value.
         *
         *  Use rd_kafka_buf_set_timeout() to set a relative timeout
         *  that will be reused on retry,
         *  or rd_kafka_buf_set_abs_timeout() to set a fixed absolute timeout
         *  for the case where the caller knows the request will be
         *  semantically outdated when that absolute time expires, such as for
         *  session.timeout.ms-based requests.
         *
         * The decision to retry a request is delegated to the rkbuf_cb
         * response callback, which should use rd_kafka_err_action()
         * and check the return actions for RD_KAFKA_ERR_ACTION_RETRY to be set
         * and then call rd_kafka_buf_retry().
         * rd_kafka_buf_retry() will enqueue the request on the rkb_retrybufs
         * queue with a backoff time of retry.backoff.ms.
         * The rkb_retrybufs queue is served by the broker thread's timeout
         * scanner.
         * @warning rkb_retrybufs is NOT purged on broker down.
         */
        rd_ts_t rkbuf_ts_timeout; /* Request timeout (absolute time). */
        rd_ts_t rkbuf_abs_timeout;/* Absolute timeout for request, including
                                   * retries.
                                   * Mutually exclusive with rkbuf_rel_timeout*/
        int     rkbuf_rel_timeout;/* Relative timeout (ms), used for retries.
                                   * Defaults to socket.timeout.ms.
                                   * Mutually exclusive with rkbuf_abs_timeout*/


        int64_t rkbuf_offset;     /* Used by OffsetCommit */

	rd_list_t *rkbuf_rktp_vers;    /* Toppar + Op Version map.
					* Used by FetchRequest. */

	rd_kafka_msgq_t rkbuf_msgq;

        rd_kafka_resp_err_t rkbuf_err;      /* Buffer parsing error code */

        union {
                struct {
                        rd_list_t *topics;  /* Requested topics (char *) */
                        char *reason;       /* Textual reason */
                        rd_kafka_op_t *rko; /* Originating rko with replyq
                                             * (if any) */
                        int all_topics;     /* Full/All topics requested */

                        int *decr;          /* Decrement this integer by one
                                             * when request is complete:
                                             * typically points to metadata
                                             * cache's full_.._sent.
                                             * Will be performed with
                                             * decr_lock held. */
                        mtx_t *decr_lock;

                } Metadata;
        } rkbuf_u;

        const char *rkbuf_uflow_mitigation; /**< Buffer read underflow
                                             *   human readable mitigation
                                             *   string (const memory).
                                             *   This is used to hint the
                                             *   user why the underflow
                                             *   might have occurred, which
                                             *   depends on request type. */
};


typedef struct rd_kafka_bufq_s {
	TAILQ_HEAD(, rd_kafka_buf_s) rkbq_bufs;
	rd_atomic32_t  rkbq_cnt;
	rd_atomic32_t  rkbq_msg_cnt;
} rd_kafka_bufq_t;

#define rd_kafka_bufq_cnt(rkbq) rd_atomic32_get(&(rkbq)->rkbq_cnt)

/**
 * @brief Set buffer's request timeout to relative \p timeout_ms measured
 *        from the time the buffer is sent on the underlying socket.
 *
 * @param now Reuse current time from existing rd_clock() var, else 0.
 *
 * The relative timeout value is reused upon request retry.
 */
static RD_INLINE void
rd_kafka_buf_set_timeout (rd_kafka_buf_t *rkbuf, int timeout_ms, rd_ts_t now) {
        if (!now)
                now = rd_clock();
        rkbuf->rkbuf_rel_timeout = timeout_ms;
        rkbuf->rkbuf_abs_timeout = 0;
}


/**
 * @brief Calculate the effective timeout for a request attempt
 */
void rd_kafka_buf_calc_timeout (const rd_kafka_t *rk, rd_kafka_buf_t *rkbuf,
                                rd_ts_t now);


/**
 * @brief Set buffer's request timeout to relative \p timeout_ms measured
 *        from \p now.
 *
 * @param now Reuse current time from existing rd_clock() var, else 0.
 *
 * The remaining time is used as timeout for request retries.
 */
static RD_INLINE void
rd_kafka_buf_set_abs_timeout (rd_kafka_buf_t *rkbuf, int timeout_ms,
                              rd_ts_t now) {
        if (!now)
                now = rd_clock();
        rkbuf->rkbuf_rel_timeout = 0;
        rkbuf->rkbuf_abs_timeout = now + (timeout_ms * 1000);
}


#define rd_kafka_buf_keep(rkbuf) rd_refcnt_add(&(rkbuf)->rkbuf_refcnt)
#define rd_kafka_buf_destroy(rkbuf)                                     \
        rd_refcnt_destroywrapper(&(rkbuf)->rkbuf_refcnt,                \
                                 rd_kafka_buf_destroy_final(rkbuf))

void rd_kafka_buf_destroy_final (rd_kafka_buf_t *rkbuf);
void rd_kafka_buf_push0 (rd_kafka_buf_t *rkbuf, const void *buf, size_t len,
                         int allow_crc_calc, void (*free_cb) (void *));
#define rd_kafka_buf_push(rkbuf,buf,len,free_cb)                        \
        rd_kafka_buf_push0(rkbuf,buf,len,1/*allow_crc*/,free_cb)
rd_kafka_buf_t *rd_kafka_buf_new0 (int segcnt, size_t size, int flags);
#define rd_kafka_buf_new(segcnt,size) \
        rd_kafka_buf_new0(segcnt,size,0)
rd_kafka_buf_t *rd_kafka_buf_new_request (rd_kafka_broker_t *rkb, int16_t ApiKey,
                                          int segcnt, size_t size);
rd_kafka_buf_t *rd_kafka_buf_new_shadow (const void *ptr, size_t size,
                                         void (*free_cb) (void *));
void rd_kafka_bufq_enq (rd_kafka_bufq_t *rkbufq, rd_kafka_buf_t *rkbuf);
void rd_kafka_bufq_deq (rd_kafka_bufq_t *rkbufq, rd_kafka_buf_t *rkbuf);
void rd_kafka_bufq_init(rd_kafka_bufq_t *rkbufq);
void rd_kafka_bufq_concat (rd_kafka_bufq_t *dst, rd_kafka_bufq_t *src);
void rd_kafka_bufq_purge (rd_kafka_broker_t *rkb,
                          rd_kafka_bufq_t *rkbufq,
                          rd_kafka_resp_err_t err);
void rd_kafka_bufq_connection_reset (rd_kafka_broker_t *rkb,
				     rd_kafka_bufq_t *rkbufq);
void rd_kafka_bufq_dump (rd_kafka_broker_t *rkb, const char *fac,
			 rd_kafka_bufq_t *rkbq);

int rd_kafka_buf_retry (rd_kafka_broker_t *rkb, rd_kafka_buf_t *rkbuf);

void rd_kafka_buf_handle_op (rd_kafka_op_t *rko, rd_kafka_resp_err_t err);
void rd_kafka_buf_callback (rd_kafka_t *rk,
			    rd_kafka_broker_t *rkb, rd_kafka_resp_err_t err,
                            rd_kafka_buf_t *response, rd_kafka_buf_t *request);



/**
 *
 * Write buffer interface
 *
 */

/**
 * Set request API type version
 */
static RD_UNUSED RD_INLINE void
rd_kafka_buf_ApiVersion_set (rd_kafka_buf_t *rkbuf,
                             int16_t version, int features) {
        rkbuf->rkbuf_reqhdr.ApiVersion = version;
        rkbuf->rkbuf_features = features;
}


/**
 * @returns the ApiVersion for a request
 */
#define rd_kafka_buf_ApiVersion(rkbuf) ((rkbuf)->rkbuf_reqhdr.ApiVersion)



/**
 * Write (copy) data to buffer at current write-buffer position.
 * There must be enough space allocated in the rkbuf.
 * Returns offset to written destination buffer.
 */
static RD_INLINE size_t rd_kafka_buf_write (rd_kafka_buf_t *rkbuf,
                                        const void *data, size_t len) {
        size_t r;

        r = rd_buf_write(&rkbuf->rkbuf_buf, data, len);

        if (rkbuf->rkbuf_flags & RD_KAFKA_OP_F_CRC)
                rkbuf->rkbuf_crc = rd_crc32_update(rkbuf->rkbuf_crc, data, len);

        return r;
}



/**
 * Write (copy) 'data' to buffer at 'ptr'.
 * There must be enough space to fit 'len'.
 * This will overwrite the buffer at given location and length.
 *
 * NOTE: rd_kafka_buf_update() MUST NOT be called when a CRC calculation
 *       is in progress (between rd_kafka_buf_crc_init() & .._crc_finalize())
 */
static RD_INLINE void rd_kafka_buf_update (rd_kafka_buf_t *rkbuf, size_t of,
                                          const void *data, size_t len) {
        rd_kafka_assert(NULL, !(rkbuf->rkbuf_flags & RD_KAFKA_OP_F_CRC));
        rd_buf_write_update(&rkbuf->rkbuf_buf, of, data, len);
}

/**
 * Write int8_t to buffer.
 */
static RD_INLINE size_t rd_kafka_buf_write_i8 (rd_kafka_buf_t *rkbuf,
					      int8_t v) {
        return rd_kafka_buf_write(rkbuf, &v, sizeof(v));
}

/**
 * Update int8_t in buffer at offset 'of'.
 * 'of' should have been previously returned by `.._buf_write_i8()`.
 */
static RD_INLINE void rd_kafka_buf_update_i8 (rd_kafka_buf_t *rkbuf,
					     size_t of, int8_t v) {
        rd_kafka_buf_update(rkbuf, of, &v, sizeof(v));
}

/**
 * Write int16_t to buffer.
 * The value will be endian-swapped before write.
 */
static RD_INLINE size_t rd_kafka_buf_write_i16 (rd_kafka_buf_t *rkbuf,
					       int16_t v) {
        v = htobe16(v);
        return rd_kafka_buf_write(rkbuf, &v, sizeof(v));
}

/**
 * Update int16_t in buffer at offset 'of'.
 * 'of' should have been previously returned by `.._buf_write_i16()`.
 */
static RD_INLINE void rd_kafka_buf_update_i16 (rd_kafka_buf_t *rkbuf,
                                              size_t of, int16_t v) {
        v = htobe16(v);
        rd_kafka_buf_update(rkbuf, of, &v, sizeof(v));
}

/**
 * Write int32_t to buffer.
 * The value will be endian-swapped before write.
 */
static RD_INLINE size_t rd_kafka_buf_write_i32 (rd_kafka_buf_t *rkbuf,
                                               int32_t v) {
        v = htobe32(v);
        return rd_kafka_buf_write(rkbuf, &v, sizeof(v));
}

/**
 * Update int32_t in buffer at offset 'of'.
 * 'of' should have been previously returned by `.._buf_write_i32()`.
 */
static RD_INLINE void rd_kafka_buf_update_i32 (rd_kafka_buf_t *rkbuf,
                                              size_t of, int32_t v) {
        v = htobe32(v);
        rd_kafka_buf_update(rkbuf, of, &v, sizeof(v));
}

/**
 * Update int32_t in buffer at offset 'of'.
 * 'of' should have been previously returned by `.._buf_write_i32()`.
 */
static RD_INLINE void rd_kafka_buf_update_u32 (rd_kafka_buf_t *rkbuf,
                                              size_t of, uint32_t v) {
        v = htobe32(v);
        rd_kafka_buf_update(rkbuf, of, &v, sizeof(v));
}


/**
 * Write int64_t to buffer.
 * The value will be endian-swapped before write.
 */
static RD_INLINE size_t rd_kafka_buf_write_i64 (rd_kafka_buf_t *rkbuf, int64_t v) {
        v = htobe64(v);
        return rd_kafka_buf_write(rkbuf, &v, sizeof(v));
}

/**
 * Update int64_t in buffer at address 'ptr'.
 * 'of' should have been previously returned by `.._buf_write_i64()`.
 */
static RD_INLINE void rd_kafka_buf_update_i64 (rd_kafka_buf_t *rkbuf,
                                              size_t of, int64_t v) {
        v = htobe64(v);
        rd_kafka_buf_update(rkbuf, of, &v, sizeof(v));
}


/**
 * @brief Write varint-encoded signed value to buffer.
 */
static RD_INLINE size_t
rd_kafka_buf_write_varint (rd_kafka_buf_t *rkbuf, int64_t v) {
        char varint[RD_UVARINT_ENC_SIZEOF(v)];
        size_t sz;

        sz = rd_uvarint_enc_i64(varint, sizeof(varint), v);

        return rd_kafka_buf_write(rkbuf, varint, sz);
}


/**
 * Write (copy) Kafka string to buffer.
 */
static RD_INLINE size_t rd_kafka_buf_write_kstr (rd_kafka_buf_t *rkbuf,
                                                const rd_kafkap_str_t *kstr) {
        return rd_kafka_buf_write(rkbuf, RD_KAFKAP_STR_SER(kstr),
				  RD_KAFKAP_STR_SIZE(kstr));
}

/**
 * Write (copy) char * string to buffer.
 */
static RD_INLINE size_t rd_kafka_buf_write_str (rd_kafka_buf_t *rkbuf,
                                               const char *str, size_t len) {
        size_t r;
        if (!str)
                len = RD_KAFKAP_STR_LEN_NULL;
        else if (len == (size_t)-1)
                len = strlen(str);
        r = rd_kafka_buf_write_i16(rkbuf, (int16_t) len);
        if (str)
                rd_kafka_buf_write(rkbuf, str, len);
        return r;
}


/**
 * Push (i.e., no copy) Kafka string to buffer iovec
 */
static RD_INLINE void rd_kafka_buf_push_kstr (rd_kafka_buf_t *rkbuf,
                                             const rd_kafkap_str_t *kstr) {
	rd_kafka_buf_push(rkbuf, RD_KAFKAP_STR_SER(kstr),
			  RD_KAFKAP_STR_SIZE(kstr), NULL);
}



/**
 * Write (copy) Kafka bytes to buffer.
 */
static RD_INLINE size_t rd_kafka_buf_write_kbytes (rd_kafka_buf_t *rkbuf,
					          const rd_kafkap_bytes_t *kbytes){
        return rd_kafka_buf_write(rkbuf, RD_KAFKAP_BYTES_SER(kbytes),
                                  RD_KAFKAP_BYTES_SIZE(kbytes));
}

/**
 * Push (i.e., no copy) Kafka bytes to buffer iovec
 */
static RD_INLINE void rd_kafka_buf_push_kbytes (rd_kafka_buf_t *rkbuf,
					       const rd_kafkap_bytes_t *kbytes){
	rd_kafka_buf_push(rkbuf, RD_KAFKAP_BYTES_SER(kbytes),
			  RD_KAFKAP_BYTES_SIZE(kbytes), NULL);
}

/**
 * Write (copy) binary bytes to buffer as Kafka bytes encapsulate data.
 */
static RD_INLINE size_t rd_kafka_buf_write_bytes (rd_kafka_buf_t *rkbuf,
                                                 const void *payload, size_t size) {
        size_t r;
        if (!payload)
                size = RD_KAFKAP_BYTES_LEN_NULL;
        r = rd_kafka_buf_write_i32(rkbuf, (int32_t) size);
        if (payload)
                rd_kafka_buf_write(rkbuf, payload, size);
        return r;
}




/**
 * Write Kafka Message to buffer
 * The number of bytes written is returned in '*outlenp'.
 *
 * Returns the buffer offset of the first byte.
 */
size_t rd_kafka_buf_write_Message (rd_kafka_broker_t *rkb,
				   rd_kafka_buf_t *rkbuf,
				   int64_t Offset, int8_t MagicByte,
				   int8_t Attributes, int64_t Timestamp,
				   const void *key, int32_t key_len,
				   const void *payload, int32_t len,
				   int *outlenp);

/**
 * Start calculating CRC from now and track it in '*crcp'.
 */
static RD_INLINE RD_UNUSED void rd_kafka_buf_crc_init (rd_kafka_buf_t *rkbuf) {
	rd_kafka_assert(NULL, !(rkbuf->rkbuf_flags & RD_KAFKA_OP_F_CRC));
	rkbuf->rkbuf_flags |= RD_KAFKA_OP_F_CRC;
	rkbuf->rkbuf_crc = rd_crc32_init();
}

/**
 * Finalizes CRC calculation and returns the calculated checksum.
 */
static RD_INLINE RD_UNUSED
rd_crc32_t rd_kafka_buf_crc_finalize (rd_kafka_buf_t *rkbuf) {
	rkbuf->rkbuf_flags &= ~RD_KAFKA_OP_F_CRC;
	return rd_crc32_finalize(rkbuf->rkbuf_crc);
}





/**
 * @brief Check if buffer's replyq.version is outdated.
 * @param rkbuf: may be NULL, for convenience.
 *
 * @returns 1 if this is an outdated buffer, else 0.
 */
static RD_UNUSED RD_INLINE int
rd_kafka_buf_version_outdated (const rd_kafka_buf_t *rkbuf, int version) {
        return rkbuf && rkbuf->rkbuf_replyq.version &&
                rkbuf->rkbuf_replyq.version < version;
}

#endif /* _RDKAFKA_BUF_H_ */
