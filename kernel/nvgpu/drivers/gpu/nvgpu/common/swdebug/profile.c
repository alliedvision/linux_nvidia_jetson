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

#include <nvgpu/swprofile.h>
#include <nvgpu/lock.h>
#include <nvgpu/kref.h>
#include <nvgpu/debug.h>
#include <nvgpu/kmem.h>
#include <nvgpu/timers.h>
#include <nvgpu/sort.h>
#include <nvgpu/log.h>

/*
 * A simple profiler, capable of generating simple stats for a set of samples.
 */

/*
 * The sample array is a 1d array comprised of repeating rows of data. To
 * index the array as though it were a row-major matrix, we need to do some
 * simple math.
 */
static inline u32 matrix_to_linear_index(struct nvgpu_swprofiler *p,
					 u32 row, u32 col)
{
	return (row * p->psample_len) + col;
}

/*
 * Just check the samples field; it'll be allocated for an enabled profiler.
 * This is an intrisically racy call; don't rely on it to determine whether the
 * underlying pointers/fields really are initialized or not.
 *
 * However, since this doesn't take the profiler lock, if you use it under the
 * profiler lock, you can be sure the state won't change while you hold the
 * lock.
 */
bool nvgpu_swprofile_is_enabled(struct nvgpu_swprofiler *p)
{
	return p->samples != NULL;
}

void nvgpu_swprofile_initialize(struct gk20a *g,
				struct nvgpu_swprofiler *p,
				const char *col_names[])
{
	if (p->col_names != NULL) {
		/*
		 * Profiler is already initialized.
		 */
		return;
	}

	nvgpu_mutex_init(&p->lock);
	p->g = g;

	p->col_names = col_names;

	p->psample_len = 0U;
	while (col_names[p->psample_len] != NULL) {
		p->psample_len++;
	}
}

int nvgpu_swprofile_open(struct gk20a *g, struct nvgpu_swprofiler *p)
{
	int ret = 0;

	nvgpu_mutex_acquire(&p->lock);

	/*
	 * If this profiler is already opened, just take a ref and return.
	 */
	if (p->samples != NULL) {
		nvgpu_ref_get(&p->ref);
		nvgpu_mutex_release(&p->lock);
		return 0;
	}

	/*
	 * Otherwise allocate the necessary data structures, etc.
	 */
	p->samples = nvgpu_vzalloc(g,
				   PROFILE_ENTRIES * p->psample_len *
				   sizeof(*p->samples));
	if (p->samples == NULL) {
		ret = -ENOMEM;
		goto fail;
	}

	p->samples_start = nvgpu_vzalloc(g,
				   PROFILE_ENTRIES * sizeof(*p->samples_start));
	if (p->samples_start == NULL) {
		ret = -ENOMEM;
		goto fail;
	}

	nvgpu_ref_init(&p->ref);

	nvgpu_mutex_release(&p->lock);

	return 0;

fail:
	if (p->samples != NULL) {
		nvgpu_vfree(g, p->samples);
		p->samples = NULL;
	}
	nvgpu_mutex_release(&p->lock);

	return ret;
}

static void nvgpu_swprofile_free(struct nvgpu_ref *ref)
{
	struct nvgpu_swprofiler *p = container_of(ref, struct nvgpu_swprofiler, ref);

	nvgpu_vfree(p->g, p->samples);
	nvgpu_vfree(p->g, p->samples_start);
	p->samples = NULL;
	p->samples_start = NULL;
}

void nvgpu_swprofile_close(struct nvgpu_swprofiler *p)
{
	nvgpu_ref_put(&p->ref, nvgpu_swprofile_free);
}

static void nvgpu_profile_print_col_header(struct nvgpu_swprofiler *p,
					   struct nvgpu_debug_context *o)
{
	u32 i;

	for (i = 0U; i < p->psample_len; i++) {
		gk20a_debug_output(o, " %15s", p->col_names[i]);
	}
	gk20a_debug_output(o, "\n");

}

/*
 * Note: this does _not_ lock the profiler. This is a conscious choice. If we
 * do lock the profiler then there's the possibility that you get bad data due
 * to the snapshot blocking on some other user printing the contents of the
 * profiler.
 *
 * Instead, this way, it's possible that someone printing the data in the
 * profiler gets a sample that's a mix of old and new. That's not great, but
 * IMO worse than a completely bogus sample.
 *
 * Also it's really quite unlikely for this race to happen in practice as the
 * print function is executed as a result of a debugfs call.
 */
void nvgpu_swprofile_snapshot(struct nvgpu_swprofiler *p, u32 idx)
{
	u32 index;

	/*
	 * Handle two cases: the first allows calling code to simply skip
	 * any profiling by passing in a NULL profiler; see the CDE code
	 * for this. The second case is if a profiler is not "opened".
	 */
	if (p == NULL || p->samples == NULL) {
		return;
	}

	/*
	 * p->sample_index is the current row, aka sample, we are writing to.
	 * idx is the column - i.e the sub-sample.
	 */
	index = matrix_to_linear_index(p, p->sample_index, idx);

	p->samples[index] = nvgpu_current_time_ns();
}

void nvgpu_swprofile_begin_sample(struct nvgpu_swprofiler *p)
{
	nvgpu_mutex_acquire(&p->lock);

	if (p == NULL || p->samples == NULL) {
		nvgpu_mutex_release(&p->lock);
		return;
	}

	p->sample_index++;

	/* Handle wrap. */
	if (p->sample_index >= PROFILE_ENTRIES) {
		p->sample_index = 0U;
	}

	/*
	 * Reference time for subsequent subsamples in this sample.
	 */
	p->samples_start[p->sample_index] = nvgpu_current_time_ns();

	nvgpu_mutex_release(&p->lock);
}

static int profile_cmp(const void *a, const void *b)
{
	return *((const u64 *) a) - *((const u64 *) b);
}

#define PERCENTILE_WIDTH	5
#define PERCENTILE_RANGES	(100/PERCENTILE_WIDTH)

static u32 nvgpu_swprofile_build_ranges(struct nvgpu_swprofiler *p,
					u64 *storage,
					u64 *percentiles,
					u32 index_end,
					u32 index_start)
{
	u32 i;
	u32 nelem = 0U;

	/*
	 * Iterate through a column and build a temporary slice array of samples
	 * so that we can sort them without corrupting the current data.
	 *
	 * Note that we have to first convert the row/column indexes into linear
	 * indexes to access the underlying sample array.
	 */
	for (i = 0; i < PROFILE_ENTRIES; i++) {
		u32 linear_idx_start = matrix_to_linear_index(p, i, index_start);
		u32 linear_idx_end = matrix_to_linear_index(p, i, index_end);

		if (p->samples[linear_idx_end] <=
		    p->samples[linear_idx_start]) {
			/* This is an invalid element */
			continue;
		}

		storage[nelem] = p->samples[linear_idx_end] -
				 p->samples[linear_idx_start];
		nelem++;
	}

	/* sort it */
	sort(storage, nelem, sizeof(u64), profile_cmp, NULL);

	/* build ranges */
	for (i = 0; i < PERCENTILE_RANGES; i++) {
		percentiles[i] = nelem < PERCENTILE_RANGES ? 0 :
			storage[(PERCENTILE_WIDTH * (i + 1) * nelem)/100 - 1];
	}

	return nelem;
}

/*
 * Print a list of percentiles spaced by 5%. Note that the debug_context needs
 * to be special here. _Most_ print functions in NvGPU automatically add a new
 * line to the end of each print statement. This function _specifically_
 * requires that your debug print function does _NOT_ do this.
 */
void nvgpu_swprofile_print_ranges(struct gk20a *g,
				  struct nvgpu_swprofiler *p,
				  struct nvgpu_debug_context *o)
{
	u32 nelem = 0U, i, j;
	u64 *sorted_data = NULL;
	u64 *percentiles = NULL;

	nvgpu_mutex_acquire(&p->lock);

	if (p->samples == NULL) {
		gk20a_debug_output(o, "Profiler not enabled.\n");
		goto done;
	}

	sorted_data = nvgpu_vzalloc(g,
				    PROFILE_ENTRIES * p->psample_len *
				    sizeof(u64));
	percentiles = nvgpu_vzalloc(g,
				    PERCENTILE_RANGES * p->psample_len *
				    sizeof(u64));
	if (!sorted_data || !percentiles) {
		nvgpu_err(g, "vzalloc: OOM!");
		goto done;
	}

	/*
	 * Loop over each column; sort the column's data and then build
	 * percentile ranges based on that sorted data.
	 */
	for (i = 0U; i < p->psample_len; i++) {
		nelem = nvgpu_swprofile_build_ranges(p,
						   &sorted_data[i * PROFILE_ENTRIES],
						   &percentiles[i * PERCENTILE_RANGES],
						   i, 0U);
	}

	gk20a_debug_output(o, "Samples: %u\n", nelem);
	gk20a_debug_output(o, "%6s", "Perc");
	nvgpu_profile_print_col_header(p, o);

	gk20a_debug_output(o, "%6s", "----");
	for (i = 0U; i < p->psample_len; i++) {
		gk20a_debug_output(o, " %15s", "---------------");
	}
	gk20a_debug_output(o, "\n");

	/*
	 * percentiles is another matrix, but this time it's using column major indexing.
	 */
	for (i = 0U; i < PERCENTILE_RANGES; i++) {
		gk20a_debug_output(o, "%3upc ", PERCENTILE_WIDTH * (i + 1));
		for (j = 0U; j < p->psample_len; j++) {
			gk20a_debug_output(o, " %15llu",
					   percentiles[(j * PERCENTILE_RANGES) + i]);
		}
		gk20a_debug_output(o, "\n");
	}
	gk20a_debug_output(o, "\n");

done:
	nvgpu_vfree(g, sorted_data);
	nvgpu_vfree(g, percentiles);
	nvgpu_mutex_release(&p->lock);
}

/*
 * Print raw data for the profiler. Can be useful if you want to do more sophisticated
 * analysis in python or something like that.
 *
 * Note this requires a debug context that does not automatically add newlines.
 */
void nvgpu_swprofile_print_raw_data(struct gk20a *g,
				    struct nvgpu_swprofiler *p,
				    struct nvgpu_debug_context *o)
{
	u32 i, j;

	nvgpu_mutex_acquire(&p->lock);

	if (p->samples == NULL) {
		gk20a_debug_output(o, "Profiler not enabled.\n");
		goto done;
	}

	gk20a_debug_output(o, "max samples: %u, sample len: %u\n",
			   PROFILE_ENTRIES, p->psample_len);

	nvgpu_profile_print_col_header(p, o);

	for (i = 0U; i < PROFILE_ENTRIES; i++) {
		for (j = 0U; j < p->psample_len; j++) {
			u32 index = matrix_to_linear_index(p, i, j);

			gk20a_debug_output(o, " %15llu",
					   p->samples[index] - p->samples_start[i]);
		}
		gk20a_debug_output(o, "\n");
	}

done:
	nvgpu_mutex_release(&p->lock);
}

/*
 * Print stats for a single column. This covers:
 *
 *   Min
 *   Max
 *   Mean
 *   Median
 *   Sigma ^ 2
 *
 * Note that the results array has to be at least 5 entries long. Storage should be
 * an array that is at least PROFILE_ENTRIES long. This is used for working out the
 * median - we need a sorted sample set for that.
 *
 * Note: this skips empty samples.
 *
 * Note: there's a limit to the sensitivity of these profiling stats. For things that
 * happen faster than the granularity of the underlying timer, you'll need to use
 * something more sophisticated. It's ok to have some zeros, but too many and you
 * won't get a very interesting picture of the data.
 */
static u32 nvgpu_swprofile_subsample_basic_stats(struct gk20a *g,
						 struct nvgpu_swprofiler *p,
						 u32 subsample,
						 u64 *results,
						 u64 *storage)
{
	u64 sum = 0U, samples = 0U;
	u64 min = U64_MAX, max = 0U;
	u64 mean, median;
	u64 sigma_2 = 0U;
	u32 i;

	/*
	 * First, let's work out min, max, sum, and number of samples of data. With this we
	 * can then get the mean, median, and sigma^2.
	 */
	for (i = 0U; i < PROFILE_ENTRIES; i++) {
		u32 ss       = matrix_to_linear_index(p, i, subsample);
		u64 sample   = p->samples[ss] - p->samples_start[i];

		if (p->samples_start[i] == 0U) {
			continue;
		}

		if (sample < min) {
			min = sample;
		}
		if (sample > max) {
			max = sample;
		}

		storage[samples] = sample;
		sum += sample;
		samples += 1U;
	}

	/*
	 * If min is still U64_MAX it means that we almost certainly did not actually
	 * get a single valid sample.
	 */
	if (min == U64_MAX) {
		min = 0U;
	}

	/* With the sorted list of samples we can easily compute the median. */
	sort(storage, samples, sizeof(u64), profile_cmp, NULL);

	mean = sum / samples;
	median = storage[samples / 2];

	/* Compute the sample variance (i.e sigma squared). */
	for (i = 0U; i < samples; i++) {
		sigma_2 += storage[i] * storage[i];
	}

	/* Remember: _sample_ variance. */
	sigma_2 /= (samples - 1U);
	sigma_2 -= (mean * mean);

	results[0] = min;
	results[1] = max;
	results[2] = mean;
	results[3] = median;
	results[4] = sigma_2;

	return samples;
}

/*
 * Print the following stats for each column:
 *
 *   Min, Max, Mean, Median, Sigma^2
 */
void nvgpu_swprofile_print_basic_stats(struct gk20a *g,
				       struct nvgpu_swprofiler *p,
				       struct nvgpu_debug_context *o)
{
	u32 i;
	const char *fmt_header = "%-18s %15s %15s %15s %15s %15s\n";
	const char *fmt_output = "%-18s %15llu %15llu %15llu %15llu %15llu\n";
	u64 *storage;
	u32 samples = 0U;

	if (p->samples == NULL) {
		gk20a_debug_output(o, "Profiler not enabled.\n");
		return;
	}

	storage = nvgpu_kzalloc(g, sizeof(u64) * PROFILE_ENTRIES);
	if (storage == NULL) {
		gk20a_debug_output(o, "OOM!");
		return;
	}

	nvgpu_mutex_acquire(&p->lock);

	gk20a_debug_output(o, fmt_header,
			   "SubSample", "Min", "Max",
			   "Mean", "Median", "Sigma^2");
	gk20a_debug_output(o, fmt_header,
			   "---------", "---", "---",
			   "----", "------", "-------");

	for (i = 0U; i < p->psample_len; i++) {
		u64 results[5];

		samples = nvgpu_swprofile_subsample_basic_stats(g, p, i,
								results, storage);

		gk20a_debug_output(o, fmt_output, p->col_names[i],
				   results[0], results[1],
				   results[2], results[3], results[4]);
	}

	gk20a_debug_output(o, "Number of samples: %u\n", samples);

	nvgpu_mutex_release(&p->lock);
	nvgpu_kfree(g, storage);
}
