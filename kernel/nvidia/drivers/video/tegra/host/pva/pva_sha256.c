/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include "pva_sha256.h"

#define ROTLEFT(a, b) (((a) << (b)) | ((a) >> (32 - (b))))
#define ROTRIGHT(a, b) (((a) >> (b)) | ((a) << (32 - (b))))

#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA_EP0(x) (ROTRIGHT(x, 2) ^ ROTRIGHT(x, 13) ^ ROTRIGHT(x, 22))
#define SHA_EP1(x) (ROTRIGHT(x, 6) ^ ROTRIGHT(x, 11) ^ ROTRIGHT(x, 25))
#define SIG0(x) (ROTRIGHT(x, 7) ^ ROTRIGHT(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x, 17) ^ ROTRIGHT(x, 19) ^ ((x) >> 10))

#define SWAP32(x) __builtin_bswap32(x)
#define SWAP64(x) __builtin_bswap64(x)

/**
 * This variable is used internally by \ref sha256_transform()
 */
static const uint32_t k[64] = {
	U32(0x428a2f98U), U32(0x71374491U), U32(0xb5c0fbcfU), U32(0xe9b5dba5U),
	U32(0x3956c25bU), U32(0x59f111f1U), U32(0x923f82a4U), U32(0xab1c5ed5U),
	U32(0xd807aa98U), U32(0x12835b01U), U32(0x243185beU), U32(0x550c7dc3U),
	U32(0x72be5d74U), U32(0x80deb1feU), U32(0x9bdc06a7U), U32(0xc19bf174U),
	U32(0xe49b69c1U), U32(0xefbe4786U), U32(0x0fc19dc6U), U32(0x240ca1ccU),
	U32(0x2de92c6fU), U32(0x4a7484aaU), U32(0x5cb0a9dcU), U32(0x76f988daU),
	U32(0x983e5152U), U32(0xa831c66dU), U32(0xb00327c8U), U32(0xbf597fc7U),
	U32(0xc6e00bf3U), U32(0xd5a79147U), U32(0x06ca6351U), U32(0x14292967U),
	U32(0x27b70a85U), U32(0x2e1b2138U), U32(0x4d2c6dfcU), U32(0x53380d13U),
	U32(0x650a7354U), U32(0x766a0abbU), U32(0x81c2c92eU), U32(0x92722c85U),
	U32(0xa2bfe8a1U), U32(0xa81a664bU), U32(0xc24b8b70U), U32(0xc76c51a3U),
	U32(0xd192e819U), U32(0xd6990624U), U32(0xf40e3585U), U32(0x106aa070U),
	U32(0x19a4c116U), U32(0x1e376c08U), U32(0x2748774cU), U32(0x34b0bcb5U),
	U32(0x391c0cb3U), U32(0x4ed8aa4aU), U32(0x5b9cca4fU), U32(0x682e6ff3U),
	U32(0x748f82eeU), U32(0x78a5636fU), U32(0x84c87814U), U32(0x8cc70208U),
	U32(0x90befffaU), U32(0xa4506cebU), U32(0xbef9a3f7U), U32(0xc67178f2U)
};

/**
 * \brief
 * This function is a helper function used by \ref pva_sha256_update
 * to hash 512-bit blocks and forms the core of the algorithm.
 * Use \ref sha256_init(), \ref pva_sha256_update(), and
 * \ref sha256_finalize() instead of calling sha256_transform() directly.
 * \param[in] ctx  pointer of struct sha256_ctx_s context.
 * \param[in] data_in  pointer to the data block to be hashed.
 * \return Void
 */
static void
sha256_transform(struct sha256_ctx_s *ctx,
		 const void *data_in)
{
	uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
	const uint32_t * const data = data_in;
	size_t i;

	for (i = 0; i < U32(16); i++)
		m[i] = SWAP32(data[i]);

	for (i = 0; i < U32(64) - U32(16); ++i)
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		m[i + U32(16)] = SIG1(m[U32(14) + i]) + m[U32(9) + i] +
				 SIG0(m[U32(1) + i]) + m[i];


	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];
	f = ctx->state[5];
	g = ctx->state[6];
	h = ctx->state[7];

	for (i = 0; i < U32(64); ++i) {
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		t1 = h + SHA_EP1(e) + CH(e, f, g) + k[i] + m[i];
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		t2 = SHA_EP0(a) + MAJ(a, b, c);
		h = g;
		g = f;
		f = e;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		e = d + t1;
		d = c;
		c = b;
		b = a;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		a = t1 + t2;
	}

	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[0] += a;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[1] += b;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[2] += c;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[3] += d;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[4] += e;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[5] += f;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[6] += g;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[7] += h;
}

void
sha256_init(struct sha256_ctx_s *ctx)
{
	ctx->bitlen = 0;
	ctx->state[0] = U32(0x6a09e667);
	ctx->state[1] = U32(0xbb67ae85);
	ctx->state[2] = U32(0x3c6ef372);
	ctx->state[3] = U32(0xa54ff53a);
	ctx->state[4] = U32(0x510e527f);
	ctx->state[5] = U32(0x9b05688c);
	ctx->state[6] = U32(0x1f83d9ab);
	ctx->state[7] = U32(0x5be0cd19);
}

void
pva_sha256_update(struct sha256_ctx_s *ctx,
		   const void *data,
		   size_t len)
{
	uint i;

	/*assert(len % 64 == 0); */

	for (i = 0; i < len; i += U32(64)) {
		ctx->bitlen &= U32(0xffffffff);
		sha256_transform(ctx, ((const uint8_t *)data) + i);
		ctx->bitlen += U32(512);
	}
}

void
sha256_copy(const struct sha256_ctx_s *ctx_in,
	    struct sha256_ctx_s *ctx_out)
{
	*ctx_out = *ctx_in;
}

void
sha256_finalize(struct sha256_ctx_s *ctx,
		const void *input,
		size_t input_size,
		uint32_t out[8])
{
	uint8_t data[64];
	void *p = data;
	uint32_t t;

	input_size  &= U32(0xffffffff);
	ctx->bitlen &= U32(0xffffffff);

	/* the false of this condition is illegal for this API agreement */
	/* this check is here only for Coverity INT30-C */
	ctx->bitlen += input_size * U32(8);
	(void)memcpy(p, input, input_size);
	data[input_size] = 0x80;

	/* can we fit an 8-byte counter? */
	if (input_size < U32(56)) {
		/* Pad whatever data is left in the buffer. */
		(void)memset(data + input_size + U32(1), 0,
			     U32(56) - input_size - U32(1));
	} else {
		/* Go into another block. We are here only for message hashing */
		if (input_size + U32(1) < U32(64))
			(void)memset(data + input_size + U32(1), 0,
				     U32(64) - input_size - U32(1));

		sha256_transform(ctx, data);
		(void)memset(data, 0, 56);
	}

	t = ctx->bitlen_low;

	*(uint32_t *)(void *)(data + 56) = 0;
	*(uint32_t *)(void *)(data + 60) = SWAP32(t);

	sha256_transform(ctx, data);

	out[0] = SWAP32(ctx->state[0]);
	out[1] = SWAP32(ctx->state[1]);
	out[2] = SWAP32(ctx->state[2]);
	out[3] = SWAP32(ctx->state[3]);
	out[4] = SWAP32(ctx->state[4]);
	out[5] = SWAP32(ctx->state[5]);
	out[6] = SWAP32(ctx->state[6]);
	out[7] = SWAP32(ctx->state[7]);
}
