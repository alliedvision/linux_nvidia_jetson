/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PROFILE_H
#define NVGPU_PROFILE_H

#include <nvgpu/lock.h>
#include <nvgpu/types.h>
#include <nvgpu/kref.h>

struct nvgpu_debug_context;

/*
 * Number of entries in the kickoff latency buffer used to calculate the
 * profiling and histogram. This number is calculated to be statistically
 * significant on a histogram on a 5% step.
 */
#define PROFILE_ENTRIES		16384U

struct nvgpu_swprofiler {
	struct nvgpu_mutex    lock;

	/**
	 * The number of sample components that make up a sample for this
	 * profiler.
	 */
	u32                   psample_len;

	/**
	 * Sample array: this is essentially a matrix where rows correspond to
	 * a given sample and rows correspond to a type of sample. Number of
	 * samples is always %PROFILING_ENTRIES. This 1d array is accessed with
	 * row-major indexing.
	 */
	u64                  *samples;

	/**
	 * Array of u64 timestamps for each sample to reference against. This
	 * way each subsample in .samples can reference this, not the 0th entry
	 * of each sample.
	 */
	u64                  *samples_start;

	/**
	 * Pointer to next sample array to write. Will be wrapped at
	 * %PROFILING_ENTRIES.
	 */
	u32                   sample_index;

	/**
	 * Column names used for printing the histogram. This is NULL terminated
	 * so that the profiler can infer the number of subsamples in a
	 * psample.
	 */
	const char          **col_names;

	struct nvgpu_ref      ref;

	/**
	 * Necessary since we won't have an access to a gk20a struct to vfree()
	 * against when this profiler is freed via an nvgpu_ref.
	 */
	struct gk20a         *g;
};

/**
 * @brief Create a profiler with the passed column names.
 *
 * @param[in] g          The GPU that owns this profiler.
 * @param[in] p          Pointer to a profiler object to initialize.
 * @param[in] col_names  %NULL terminated list of column names.
 *
 * The sample array length is determined by the NULL terminated %col_names
 * array. This will not allocate the underlying data; that's controlled by
 * the open and close functions:
 *
 *    nvgpu_swprofile_open()
 *    nvgpu_swprofile_close()
 *
 * Once nvgpu_swprofile_initialize() is called all of the below functions
 * may also be called. All of the sampling related functions will become
 * no-ops if the SW profiler is not opened.
 */
void nvgpu_swprofile_initialize(struct gk20a *g,
				struct nvgpu_swprofiler *p,
				const char **col_names);

/**
 * @brief Open a profiler for use.
 *
 * @param[in] g   The GPU that owns this profiler.
 * @param[in] p   The profiler to open.
 *
 * This functions prepares a SW profiler object for actual profiling. Necessary
 * data structures are allocated and subsequent snapshots will be captured.
 *
 * SW profiler objects are reference counted: for each open call made, a
 * corresponding close call must also be made.
 *
 * @return Returns 0 on success, otherwise a negative error code.
 */
int nvgpu_swprofile_open(struct gk20a *g, struct nvgpu_swprofiler *p);

/**
 * @brief Close a profiler.
 *
 * @param[in] p  The profiler to close.
 *
 * Close call corresponding to nvgpu_swprofile_open().
 */
void nvgpu_swprofile_close(struct nvgpu_swprofiler *p);

/**
 * @brief Check if a profiler is enabled.
 *
 * @param[in] p  The profiler to check.
 *
 * Returns true if the profiler is currently enabled. Do not rely on this
 * to ensure that the underlying data and fields remain initialized. This does
 * not take the profiler's lock.
 *
 * However, you can rely on the profiler's state not changing if you take the
 * profiler's lock before calling this. In that scenario the profiler's state
 * (but not necessarily the actual data) will be unchanged until you release
 * the lock.
 *
 * @return %true if the profiler is enabled; %false otherwise.
 */
bool nvgpu_swprofile_is_enabled(struct nvgpu_swprofiler *p);

/**
 * @brief Begin a series of timestamp samples.
 *
 * @param[in] p  The profiler to start sampling with.
 *
 * Each iteration through a given SW sequence requires one call to this
 * function. It essentially just increments (with wraparound) an internal
 * tracker which points to the sample space in the internal sample array.
 * Typical usage is to call nvgpu_swprofile_begin_sample() and then a
 * sequence of calls to nvgpu_swprofile_snapshot().
 *
 * Once done with the sequence being profiled nothing needs to happen. When
 * the next iteration of the sequence is executed this function should be
 * called again.
 */
void nvgpu_swprofile_begin_sample(struct nvgpu_swprofiler *p);

/**
 * @brief Capture a timestamp sample.
 *
 * @param[in] p    The profiler to sample with.
 * @param[in] idx  The index to the subsample to capture.
 *
 * This captures a subsample. Any given run through a SW sequence that is
 * being profiled will result in one or more subsamples which together make
 * up a sample.
 */
void nvgpu_swprofile_snapshot(struct nvgpu_swprofiler *p, u32 idx);

/**
 * @brief Print percentile ranges for a SW profiler.
 *
 * @param[in] g   The GPU that owns this profiler.
 * @param[in] p   The profiler to print.
 * @param[in] o   A debug context object used for printing.
 *
 * Print a percentile table for all columns of sub-samples. This gives a
 * good overview of the collected data.
 */
void nvgpu_swprofile_print_ranges(struct gk20a *g,
				  struct nvgpu_swprofiler *p,
				  struct nvgpu_debug_context *o);

/**
 * @brief Print raw data in a SW profiler.
 *
 * @param[in] g   The GPU that owns this profiler.
 * @param[in] p   The profiler to print.
 * @param[in] o   A debug context object used for printing.
 *
 * Print out the raw data captured by this profiler. The data is formatted
 * as one row per sample.
 */
void nvgpu_swprofile_print_raw_data(struct gk20a *g,
				    struct nvgpu_swprofiler *p,
				    struct nvgpu_debug_context *o);

/**
 * @brief Print a few basic statistical measures for each subsample of data.
 *
 * @param[in] g   The GPU that owns this profiler.
 * @param[in] p   The profiler to print.
 * @param[in] o   A debug context object used for printing.
 *
 * The following statistical measures are printed:
 *
 *   { Min, Max, Mean, Median, Sample Variance }
 *
 * This set of data is provided to allow basic first pass analysis.
 */
void nvgpu_swprofile_print_basic_stats(struct gk20a *g,
				       struct nvgpu_swprofiler *p,
				       struct nvgpu_debug_context *o);

#endif /* NVGPU_PROFILE_H */
