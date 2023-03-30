/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_COV_WHITELIST_H
#define NVGPU_COV_WHITELIST_H

/** @name Coverity Whitelisting
 *  These macros are used for whitelisting coverity violations. The macros are
 *  only enabled when a coverity scan is being run.
 */
/**@{*/
#ifdef NV_IS_COVERITY
/**
 * NVGPU_MISRA - Define a MISRA rule for NVGPU_COV_WHITELIST.
 *
 * @param type - This should be Rule or Directive depending on if you're dealing
 *               with a MISRA rule or directive.
 * @param num  - This is the MISRA rule/directive number. Replace hyphens and
 *               periods in the rule/directive number with underscores. Example:
 *               14.2 should be 14_2.
 *
 * This is a convenience macro for defining a MISRA rule for the
 * NVGPU_COV_WHITELIST macro.
 *
 * Example 1: For defining MISRA rule 14.2, use NVGPU_MISRA(Rule, 14_2).\n
 * Example 2: For defining MISRA directive 4.7, use NVGPU_MISRA(Directive, 4_7).
 */
#define NVGPU_MISRA(type, num)	MISRA_C_2012_##type##_##num

/**
 * NVGPU_CERT - Define a CERT C rule for NVGPU_COV_WHITELIST.
 *
 * @param num - This is the CERT C rule number. Replace hyphens and periods in
 *              the rule number with underscores. Example: INT30-C should be
 *              INT30_C.
 *
 * This is a convenience macro for defining a CERT C rule for the
 * NVGPU_COV_WHITELIST macro.
 *
 * Example: For defining CERT C rule INT30-C, use NVGPU_CERT(INT30_C).
 */
#define NVGPU_CERT(num)		CERT_##num

/**
 * Helper macro for stringifying the _Pragma() string
 */
#define NVGPU_COV_STRING(x)	#x

/**
 * NVGPU_COV_WHITELIST - Whitelist a coverity violation on the next line.
 *
 * @param type        - This is the whitelisting category. Valid values are
 *                      deviate or false_positive.\n
 *                      deviate is for an approved rule deviation.\n
 *                      false_positive is normally used for a bug in coverity
 *                      which causes a false violation to appear in the scan.
 * @param checker     - This is the MISRA or CERT C rule causing the violation.
 *                      Use the NVGPU_MISRA() or NVGPU_CERT() macro to define
 *                      this field.
 * @param comment_str - This is the comment that you want associated with this
 *                      whitelisting. This should normally be a bug number
 *                      (ex: coverity bug) or JIRA task ID (ex: RFD). Unlike the
 *                      other arguments, this argument must be a quoted string.
 *
 * Use this macro to whitelist a coverity violation in the next line of code.
 *
 * Example 1: Whitelist a MISRA rule 14.2 violation due to a deviation
 * documented in the JIRA TID-123 RFD:\n
 * NVGPU_COV_WHITELIST(deviate, NVGPU_MISRA(Rule, 14_2), "JIRA TID-123")\n
 * next_line_of_code_with_a_rule_14.2_violation();
 *
 * Example 2: Whitelist violations for CERT C rules INT30-C and STR30-C caused
 * by coverity bugs:\n
 * NVGPU_COV_WHITELIST(false_positive, NVGPU_CERT(INT30_C), "Bug 123456")\n
 * NVGPU_COV_WHITELIST(false_positive, NVGPU_CERT(STR30_C), "Bug 123457")\n
 * next_line_of_code_with_INT30-C_and_STR30-C_violations();
 */
#define NVGPU_COV_WHITELIST(type, checker, comment_str) \
	_Pragma(NVGPU_COV_STRING(coverity compliance type checker comment_str))

/**
 * NVGPU_COV_WHITELIST_BLOCK_BEGIN - Whitelist a coverity violation for a block
 *                                   of code.
 *
 * @param type        - This is the whitelisting category. Valid values are
 *                      deviate or false_positive.\n
 *                      deviate is for an approved rule deviation.\n
 *                      false_positive is normally used for a bug in coverity
 *                      which causes a false violation to appear in the scan.
 * @param num         - This is number of violations expected within the block.
 * @param checker     - This is the MISRA or CERT C rule causing the violation.
 *                      Use the NVGPU_MISRA() or NVGPU_CERT() macro to define
 *                      this field.
 * @param comment_str - This is the comment that you want associated with this
 *                      whitelisting. This should normally be a bug number
 *                      (ex: coverity bug) or JIRA task ID (ex: RFD). Unlike the
 *                      other arguments, this argument must be a quoted string.
 *
 * Use this macro to whitelist a coverity violation for a block of code. It
 * must be terminated by an NVGPU_COV_WHITELIST_BLOCK_END()
 *
 * Example: Whitelist 10 MISRA rule 14.2 violation due to a deviation
 * documented in the JIRA TID-123 RFD:\n
 * NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 10, NVGPU_MISRA(Rule, 14_2), "JIRA TID-123")\n
 *  > Next block of code with 10 rule 14.2 violations
 * NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_2))\n
 *
 */
#define NVGPU_COV_WHITELIST_BLOCK_BEGIN(type, num, checker, comment_str) \
	_Pragma(NVGPU_COV_STRING(coverity compliance block type:num checker comment_str))

/**
 * NVGPU_COV_WHITELIST_BLOCK_END - End whitelist a block of code.that is
 *                                 whitelisted with a
 *                                 NVGPU_COV_WHITELIST_BLOCK_BEGIN
 *
 * @param checker     - This is the MISRA or CERT C rule causing the violation.
 *                      Use the NVGPU_MISRA() or NVGPU_CERT() macro to define
 *                      this field.
 *
 * Use this macro to mark the end of the block whitelisted by
 * NVGPU_COV_WHITELIST_BLOCK_END()
 *
 */
#define NVGPU_COV_WHITELIST_BLOCK_END(checker) \
	_Pragma(NVGPU_COV_STRING(coverity compliance end_block checker))
#else
/**
 * no-op macros for normal compilation - whitelisting is disabled when a
 * coverity scan is NOT being run
 */
#define NVGPU_MISRA(type, num)
#define NVGPU_CERT(num)
#define NVGPU_COV_WHITELIST(type, checker, comment_str)
#define NVGPU_COV_WHITELIST_BLOCK_BEGIN(type, num, checker, comment_str)
#define NVGPU_COV_WHITELIST_BLOCK_END(checker)
#endif
/**@}*/ /* "Coverity Whitelisting" doxygen group */

#endif /* NVGPU_COV_WHITELIST_H */