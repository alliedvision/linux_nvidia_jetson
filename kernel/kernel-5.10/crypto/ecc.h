/*
 * Copyright (c) 2013, Kenneth MacKay
 * All rights reserved.
 * Copyright (c) 2017-2020, NVIDIA Corporation. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _CRYPTO_ECC_H
#define _CRYPTO_ECC_H

/* One digit is u64 qword. */
#define ECC_CURVE_NIST_P192_DIGITS  3
#define ECC_CURVE_NIST_P256_DIGITS  4
#include <crypto/ecc.h>

#include "ecc_curve_defs.h"

const struct ecc_curve *ecc_get_curve(unsigned int curve_id);
struct ecc_point *ecc_alloc_point(unsigned int ndigits);
void ecc_free_point(struct ecc_point *p);

void vli_clear(u64 *vli, unsigned int ndigits);
bool vli_is_zero(const u64 *vli, unsigned int ndigits);
unsigned int vli_num_digits(const u64 *vli, unsigned int ndigits);
unsigned int vli_num_bits(const u64 *vli, unsigned int ndigits);
void vli_set(u64 *dest, const u64 *src, unsigned int ndigits);
void vli_copy_to_buf(u8 *dst_buf, unsigned int buf_len,
		     const u64 *src_vli, unsigned int ndigits);
void vli_copy_from_buf(u64 *dst_vli, unsigned int ndigits,
		       const u8 *src_buf, unsigned int buf_len);
int vli_cmp(const u64 *left, const u64 *right, unsigned int ndigits);
u64 vli_lshift(u64 *result, const u64 *in, unsigned int shift,
	       unsigned int ndigits);
void vli_rshift1(u64 *vli, unsigned int ndigits);
u64 vli_add(u64 *result, const u64 *left, const u64 *right,
	    unsigned int ndigits);
u64 vli_sub(u64 *result, const u64 *left, const u64 *right,
	    unsigned int ndigits);
void vli_mult(u64 *result, const u64 *left, const u64 *right,
	      unsigned int ndigits);
void vli_square(u64 *result, const u64 *left, unsigned int ndigits);
void vli_mod_add(u64 *result, const u64 *left, const u64 *right,
		 const u64 *mod, unsigned int ndigits);
void vli_mod_sub(u64 *result, const u64 *left, const u64 *right,
		 const u64 *mod, unsigned int ndigits);
void vli_mod(u64 *result, const u64 *input, const u64 *mod,
	     unsigned int ndigits);
void vli_print(char *vli_name, const u64 *vli, unsigned int ndigits);
void vli_mod_mult(u64 *result, const u64 *left, const u64 *right,
		  const u64 *mod, unsigned int ndigits);
bool vli_mmod_fast(u64 *result, u64 *product,
		   const u64 *curve_prime, unsigned int ndigits);
void vli_mod_mult_fast(u64 *result, const u64 *left, const u64 *right,
		       const u64 *curve_prime, unsigned int ndigits);
void vli_mod_square_fast(u64 *result, const u64 *left,
			 const u64 *curve_prime, unsigned int ndigits);
void vli_mod_inv(u64 *result, const u64 *input, const u64 *mod,
		 unsigned int ndigits);

bool ecc_point_is_zero(const struct ecc_point *point);
void ecc_point_double_jacobian(u64 *x1, u64 *y1, u64 *z1,
			       u64 *curve_prime, unsigned int ndigits);
void ecc_point_add(u64 *x1, u64 *y1, u64 *x2, u64 *y2, u64 *curve_prime,
		   unsigned int ndigits);
void ecc_point_mult(struct ecc_point *result,
		    const struct ecc_point *point, const u64 *scalar,
		    u64 *initial_z, const struct ecc_curve *curve,
		    unsigned int ndigits);
void ecc_swap_digits(const u64 *in, u64 *out, unsigned int ndigits);

/**
 * struct ecc_point - elliptic curve point in affine coordinates
 *
 * @x:		X coordinate in vli form.
 * @y:		Y coordinate in vli form.
 * @ndigits:	Length of vlis in u64 qwords.
 */
struct ecc_point {
	u64 *x;
	u64 *y;
	u8 ndigits;
};

#define ECC_POINT_INIT(x, y, ndigits)	(struct ecc_point) { x, y, ndigits }

/**
 * struct ecc_curve - definition of elliptic curve
 *
 * @name:	Short name of the curve.
 * @g:		Generator point of the curve.
 * @p:		Prime number, if Barrett's reduction is used for this curve
 *		pre-calculated value 'mu' is appended to the @p after ndigits.
 *		Use of Barrett's reduction is heuristically determined in
 *		vli_mmod_fast().
 * @n:		Order of the curve group.
 * @a:		Curve parameter a.
 * @b:		Curve parameter b.
 */
struct ecc_curve {
	char *name;
	struct ecc_point g;
	u64 *p;
	u64 *n;
	u64 *a;
	u64 *b;
};

/**
 * ecc_is_key_valid() - Validate a given ECC private key
 *
 * @curve_id:		id representing the curve to use
 * @ndigits:		curve's number of digits
 * @private_key:	private key to be used for the given curve
 * @private_key_len:	private key length
 *
 * Returns 0 if the key is acceptable, a negative value otherwise
 */
int ecc_is_key_valid(unsigned int curve_id, unsigned int ndigits,
		     const u64 *private_key, unsigned int private_key_len);

/**
 * ecc_gen_privkey() -  Generates an ECC private key.
 * The private key is a random integer in the range 0 < random < n, where n is a
 * prime that is the order of the cyclic subgroup generated by the distinguished
 * point G.
 * @curve_id:		id representing the curve to use
 * @ndigits:		curve number of digits
 * @private_key:	buffer for storing the generated private key
 *
 * Returns 0 if the private key was generated successfully, a negative value
 * if an error occurred.
 */
int ecc_gen_privkey(unsigned int curve_id, unsigned int ndigits, u64 *privkey);

/**
 * ecc_make_pub_key() - Compute an ECC public key
 *
 * @curve_id:		id representing the curve to use
 * @ndigits:		curve's number of digits
 * @private_key:	pregenerated private key for the given curve
 * @public_key:		buffer for storing the generated public key
 *
 * Returns 0 if the public key was generated successfully, a negative value
 * if an error occurred.
 */
int ecc_make_pub_key(const unsigned int curve_id, unsigned int ndigits,
		     const u64 *private_key, u64 *public_key);

/**
 * ecc_is_pubkey_valid_partial() - Partial public key validation
 *
 * @curve:		elliptic curve domain parameters
 * @pk:			public key as a point
 *
 * Valdiate public key according to SP800-56A section 5.6.2.3.4 ECC Partial
 * Public-Key Validation Routine.
 *
 * Note: There is no check that the public key is in the correct elliptic curve
 * subgroup.
 *
 * Return: 0 if validation is successful, -EINVAL if validation is failed.
 */
int ecc_is_pubkey_valid_partial(const struct ecc_curve *curve,
				struct ecc_point *pk);

/**
 * ecc_is_pubkey_valid_full() - Full public key validation
 *
 * @curve:		elliptic curve domain parameters
 * @pk:			public key as a point
 *
 * Valdiate public key according to SP800-56A section 5.6.2.3.3 ECC Full
 * Public-Key Validation Routine.
 *
 * Return: 0 if validation is successful, -EINVAL if validation is failed.
 */
int ecc_is_pubkey_valid_full(const struct ecc_curve *curve,
			     struct ecc_point *pk);

/**
 * vli_is_zero() - Determine is vli is zero
 *
 * @vli:		vli to check.
 * @ndigits:		length of the @vli
 */
bool vli_is_zero(const u64 *vli, unsigned int ndigits);

/**
 * vli_cmp() - compare left and right vlis
 *
 * @left:		vli
 * @right:		vli
 * @ndigits:		length of both vlis
 *
 * Returns sign of @left - @right, i.e. -1 if @left < @right,
 * 0 if @left == @right, 1 if @left > @right.
 */
int vli_cmp(const u64 *left, const u64 *right, unsigned int ndigits);

/**
 * vli_sub() - Subtracts right from left
 *
 * @result:		where to write result
 * @left:		vli
 * @right		vli
 * @ndigits:		length of all vlis
 *
 * Note: can modify in-place.
 *
 * Return: carry bit.
 */
u64 vli_sub(u64 *result, const u64 *left, const u64 *right,
	    unsigned int ndigits);

/**
 * vli_from_be64() - Load vli from big-endian u64 array
 *
 * @dest:		destination vli
 * @src:		source array of u64 BE values
 * @ndigits:		length of both vli and array
 */
void vli_from_be64(u64 *dest, const void *src, unsigned int ndigits);

/**
 * vli_from_le64() - Load vli from little-endian u64 array
 *
 * @dest:		destination vli
 * @src:		source array of u64 LE values
 * @ndigits:		length of both vli and array
 */
void vli_from_le64(u64 *dest, const void *src, unsigned int ndigits);

/**
 * vli_mod_inv() - Modular inversion
 *
 * @result:		where to write vli number
 * @input:		vli value to operate on
 * @mod:		modulus
 * @ndigits:		length of all vlis
 */
void vli_mod_inv(u64 *result, const u64 *input, const u64 *mod,
		 unsigned int ndigits);

/**
 * vli_mod_mult_slow() - Modular multiplication
 *
 * @result:		where to write result value
 * @left:		vli number to multiply with @right
 * @right:		vli number to multiply with @left
 * @mod:		modulus
 * @ndigits:		length of all vlis
 *
 * Note: Assumes that mod is big enough curve order.
 */
void vli_mod_mult_slow(u64 *result, const u64 *left, const u64 *right,
		       const u64 *mod, unsigned int ndigits);

/**
 * ecc_point_mult_shamir() - Add two points multiplied by scalars
 *
 * @result:		resulting point
 * @x:			scalar to multiply with @p
 * @p:			point to multiply with @x
 * @y:			scalar to multiply with @q
 * @q:			point to multiply with @y
 * @curve:		curve
 *
 * Returns result = x * p + x * q over the curve.
 * This works faster than two multiplications and addition.
 */
void ecc_point_mult_shamir(const struct ecc_point *result,
			   const u64 *x, const struct ecc_point *p,
			   const u64 *y, const struct ecc_point *q,
			   const struct ecc_curve *curve);

int ecc_is_pub_key_valid(unsigned int curve_id, unsigned int ndigits,
			 const u8 *pub_key, unsigned int pub_key_len);

#endif /* _CRYPTO_ECC_H */
