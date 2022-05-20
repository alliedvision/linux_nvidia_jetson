/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef UNIT_NVGPU_RUNLIST_H
#define UNIT_NVGPU_RUNLIST_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-runlist
 *  @{
 *
 * Software Unit Test Specification for fifo/runlist */

/**
 * Test specification for: test_runlist_setup_sw
 *
 * Description: Test runlist context initialization.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_runlist_setup_sw, nvgpu_init_active_runlist_mapping,
 *          nvgpu_init_runlist_enginfo, nvgpu_runlist_cleanup_sw
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Determine runlist details and allocate memory for runlist buffers, bitmaps.
 * - Check clean-up code for failing conditions.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_runlist_setup_sw(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_runlist_get_mask
 *
 * Description: Check lists of runlists servicing engine/PBDMA/TSG.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_get_runlists_mask
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Look up runlists servicing known engines or PBDMA.
 * - From given id_type, look up runlists servicing channels/TSG.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_runlist_get_mask(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_runlist_lock_unlock_active_runlists
 *
 * Description: Acquire and release runlist lock.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_lock_active_runlists,
 *          nvgpu_runlist_unlock_active_runlists, nvgpu_runlist_unlock_runlists
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Acquire lock for active runlists.
 * - Release runlist lock for active runlists.
 * - Release runlist lock for selected runlists.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_runlist_lock_unlock_active_runlists(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_runlist_set_state
 *
 * Description: Test enable/disable of runlists.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_set_state
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Enable/Disable given selected runlist.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_runlist_set_state(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_runlist_interleave_level_name
 *
 * Description: Get runlist interleave level name as string.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_interleave_level_name
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Check correct string is returned for runlist interleave level name.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_runlist_interleave_level_name(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_runlist_reload_ids
 *
 * Description: Reload given runlists.
 *
 * Test Type: Feature, Boundary Value
 *
 * Targets: gops_runlist.reload, nvgpu_runlist_reload,
 *          nvgpu_runlist_reload_ids, nvgpu_runlist_update
 *
 * Input: test_fifo_init_support
 * Equivalence classes:
 * runlist id
 * - Invalid : { 2 - U32_MAX }
 * - Valid: { 0, 1 }
 *
 * Steps:
 * - Reload runlist with different conditions:
 *   - Null GPU pointer
 *   - No runlist selected. Function returns without reloading any runlist.
 *   - Pending wait times out.
 *   - Runlist update pending wait is interrupted.
 *   - Remove/Restore all channels.
 *   - Verify error for runlist ids for Invalid range
 *   - Verify pass for runlist ids for valid range
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_runlist_reload_ids(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_runlist_update_locked
 *
 * Description: Add/remove channel from runlist.
 *
 * Test Type: Feature, Boundary Value
 *
 * Targets: nvgpu_runlist_update_locked, gk20a_runlist_modify_active_locked,
 *          gk20a_runlist_reconstruct_locked
 *
 * Input: test_fifo_init_support
 * Equivalence classes:
 * runlist_id
 * - Invalid : { 2 - U32_MAX }
 * - Valid :   { 0 - 1 }
 *
 * Steps:
 * - Check that channels can be added to runlist.
 * - Check that channels can be removed from runlist.
 * - Check that runlist update fails for invalid tsg id and zero runlist entries
 * - Check that runlist update fails for invalid runlist ids
 * - Check that runlist update passes for valid runlist ids
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_runlist_update_locked(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_runlist_update_for_channel
 *
 * Description: Add/remove channel to/from runlist.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_update_for_channel
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Check that this API can be used to remove channels from runlist.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_runlist_update_for_channel(struct unit_module *m, struct gk20a *g,
			void *args);

/**
 * Test specification for: test_tsg_format_gen
 *
 * Description: Test format of TSG runlist entry
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_construct_locked, nvgpu_runlist_append_flat,
 *          nvgpu_runlist_append_prio, nvgpu_runlist_append_tsg
 *
 * Input: None
 *
 * Steps:
 * - Check that inserting a single TSG of any level with a number of channels
 *   works as expected:
 *   - priority 0, one channel.
 *   - priority 1, two channels.
 *   - priority 2, five channels.
 *   - priority 0, one channel, nondefault timeslice timeout.
 *   - priority 0, three channels with two inactives in the middle.
 * - Each entry in #tsg_fmt_tests describes one subtest above:
 *   - number of channels to be allocated.
 *   - runlist interleavel level.
 *   - runlist timeslice.
 *   - expected RL entry (header+channels).
 * - Check that failure conditions also work as expected:
 *   - TSG entry with zero entries
 *   - TSG entry with one entry (i.e. no space channel)
 *
 * After allocating channels and binding them to TSG, the TSG is added to
 * runlist by calling nvgpu_runlist_construct_locked.
 * Then entries of the runlist are checked against expected values (as
 * specified by #tsg_fmt_tests).
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_tsg_format_gen(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_flat_gen
 *
 * Description: Build runlist without interleaving (aka "flat")
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_construct_locked, nvgpu_runlist_append_flat,
 *          nvgpu_runlist_append_prio, nvgpu_runlist_append_tsg
 *
 * Input: None
 *
 * Steps:
 * - Allocate TSGs, each bound to multiple channels.
 * - Add TSGs to runlist, using pseudo-random priorites for TSGs.
 * - Flat runlist is used: interleave is set to false when calling
 *   nvgpu_runlist_construct_locked.
 * - Check that resulting runlist is ordered according to priorities
 *   (higher priorities first).
 * - Check that flat runlists are generated with given sizelimit constraints:
 *   - Max sizelimit. Generated runlist indices match expected order.
 *   - Sizelimit = 1. Failure is expected, space for just one TSG header.
 *   - Sizelimit = 2. Only one TSG header with its channels fits the runlist.
 *   - Sizelimit = 3. Second TSG's channels are chopped off.
 *   - Sizelimit = 4. Two full TSG entries fit exactly.
 *   - Sizelimit = 11. Sizelimit set to (n x TSG - 1) entries. All but the last
 *     channel entry fit.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_flat_gen(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_interleave_single
 *
 * Description: Build runlist with interleaving, single level only
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_construct_locked, nvgpu_runlist_append_low,
 *          nvgpu_runlist_append_med, nvgpu_runlist_append_hi,
 *          nvgpu_runlist_append_tsg, nvgpu_runlist_append_prio
 *
 * Input: None
 *
 * Steps:
 * - Create TSGs and channels, such that the first two TSGs, IDs 0 and 1
 *   (with just one channel each) are at interleave level "low" ("l0"), the
 *   next IDs 2 and 3 are at level "med" ("l1"), and the last IDs 4 and 5
 *   are at level "hi" ("l2"). Runlist construction doesn't care, so we use
 *   an easy to understand order.
 * - TSGs are added to runlists using nvgpu_runlist_construct_locked, with
 *   interleave enabled.
 * - Expected order of TSGs is stored in an array, and actual runlist is
 *   compared with expected order.
 * - If runlist is too small to accommodate all TSGs/channels, an error
 *   is expected for nvgpu_runlist_construct_locked, and actual truncated
 *   runlist is compared with first elements of expected array.
 * - When debugging this test and/or the runlist code, the logs of any
 *   interleave test should follow the order in the "expected" array. Since
 *   only one level is available every run, IDs added should be 0 and 1.
 * - This test runs for levels: l0, l1, l2 separately.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_interleave_single(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_interleave_dual
 *
 * Description: Build runlist with interleaving, two different levels
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_construct_locked, nvgpu_runlist_append_low,
 *          nvgpu_runlist_append_med, nvgpu_runlist_append_hi,
 *          nvgpu_runlist_append_tsg, nvgpu_runlist_append_prio
 *
 * Input: None
 *
 * Steps:
 * - Create TSGs and channels, such that the first two TSGs, IDs 0 and 1
 *   (with just one channel each) are at interleave level "low" ("l0"), the
 *   next IDs 2 and 3 are at level "med" ("l1"), and the last IDs 4 and 5
 *   are at level "hi" ("l2"). Runlist construction doesn't care, so we use
 *   an easy to understand order.
 * - TSGs are added to runlists using nvgpu_runlist_construct_locked, with
 *   interleave enabled.
 * - Expected order of TSGs is stored in an array, and actual runlist is
 *   compared with expected order.
 * - If runlist is too small to accommodate all TSGs/channels, an error
 *   is expected for nvgpu_runlist_construct_locked, and actual truncated
 *   runlist is compared with first elements of expected array.
 * - When debugging this test and/or the runlist code, the logs of any
 *   interleave test should follow the order in the "expected" array. Every
 *   test includes two different priority levels, so the expected array
 *   should interleave higher priorities before lower priority items.
 * - This test runs for level combinations: l0 and l1, l1 and l2, l0 and l2.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_interleave_dual(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_interleaving_gen_all_run
 *
 * Description: Build runlist with interleaving, all levels
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_runlist_construct_locked, nvgpu_runlist_append_low,
 *          nvgpu_runlist_append_med, nvgpu_runlist_append_hi,
 *          nvgpu_runlist_append_tsg, nvgpu_runlist_append_prio
 *
 * Input: None
 *
 * Steps:
 * - Create TSGs and channels, such that the first two TSGs, IDs 0 and 1
 *   (with just one channel each) are at interleave level "low" ("l0"), the
 *   next IDs 2 and 3 are at level "med" ("l1"), and the last IDs 4 and 5
 *   are at level "hi" ("l2"). Runlist construction doesn't care, so we use
 *   an easy to understand order.
 * - When debugging this test and/or the runlist code, the logs of any
 *   interleave test should follow the order in the "expected" array. We
 *   start at the highest level, so the first IDs added should be h1 and
 *   h2, i.e., 4 and 5, etc.
 * - TSGs are added to runlists using nvgpu_runlist_construct_locked, with
 *   interleave enabled.
 * - Expected order of TSGs is stored in an array, and actual runlist is
 *   compared with expected order.
 * - If runlist is too small to accommodate all TSGs/channels, an error
 *   is expected for nvgpu_runlist_construct_locked, and actual truncated
 *   runlist is compared with first elements of expected array.
 * - Following conditions are tested:
 *   - All priority items are interleaved.
 *   - Space for only one TSG header. Failure expected.
 *   - Space for both l2 entries, no space for l1 entry.
 *   - Insert both l2 entries, one l1, and just one l2: fail at last l2.
 *   - Stop at exactly the first l2 entry in the first l1-l0 transition.
 *   - Stop at exactly the first l0 entry that doesn't fit.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_interleaving_levels(struct unit_module *m, struct gk20a *g,
								void *args);

#endif /* UNIT_NVGPU_RUNLIST_H */
