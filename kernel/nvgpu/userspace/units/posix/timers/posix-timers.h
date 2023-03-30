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

/**
 * @addtogroup SWUTS-posix-timers
 * @{
 *
 * Software Unit Test Specification for posix-timers
 */
#ifndef __UNIT_POSIX_TIMERS_H__
#define __UNIT_POSIX_TIMERS_H__

/**
 * Test specification for test_timer_init
 *
 * Description: Test the timer initialization routine.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_timeout_init_flags
 *
 * Inputs:
 * 1) The type of timer to be tested is passed as an argument to the test.
 * 2) Global defines for flag and duration values.
 * 3) Global nvgpu_timeout structure instance.
 *
 * Steps:
 * 1) Check for the type of timer to be tested.
 * 2) Populate the flags and duration values depending on the timer type.
 * 3) Invoke the timer init function.
 * 4) Check the return value for errors.
 * 5) Check the internal parameters in nvgpu_timeout structure to ensure
 *    proper initialisation.
 *
 * Output:
 * The test returns PASS if the return value from timer init function
 * indicates success and the internal parameter values in nvgpu_timeout
 * structure is initialised as per the passed arguments.
 * Test returns FAIL if timer init function fails or if any of the
 * parameters inside the nvgpu_timeout struct is not initialised properly.
 *
 */
int test_timer_init(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_timer_init_err
 *
 * Description: Test the timer initialisation routine error path.
 *
 * Test Type: Boundary values
 *
 * Targets: nvgpu_timeout_init_flags
 *
 * Inputs:
 * 1) Global nvgpu_timeout structure instance.
 *
 * Steps:
 * 1) Invoke timer initialisation routine in loop with different value for
 *    flags parameter for each invocation.
 * 2) Check for the corresponding return value.  The timer initialisation
 *    function should return error for invalid flag values and return success
 *    for valid flag values.
 *
 * Output:
 * The test returns PASS if the initialisation routine returns an appropriate
 * return value as per the flag value passed for each invocation.
 * The test returns FAIL if the initialisation routine does not return the
 * expected value for a particular flag for any of the invocation.
 *
 */
int test_timer_init_err(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_timer_counter
 *
 * Description: Test the counter based timer functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_timeout_init_retry, nvgpu_timeout_expired,
 *          nvgpu_timeout_peek_expired
 *
 * Input:
 * 1) Global nvgpu_timeout structure instance.
 * 2) Global defines for flag and duration parameters.
 *
 * Steps:
 * 1) Reset the global nvgpu_timeout structure with all 0s.
 * 2) Initialise the timeout structure.
 * 3) Check the return value for error.
 * 4) Loop and check for the timer expiry.  Sleep is introduced
 *    between each loop.
 * 5) Confirm the status of the timer expiry by verifying the
 *    counter value.
 *
 * Output:
 * Test returns PASS if the timer expires after the programmed
 * counter value.
 * Test returns FAIL if the initialisation routine returns error or
 * timer expires before the programmed counter value is reached.
 *
 */
int test_timer_counter(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_timer_duration
 *
 * Description: Test the duration based timer functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_timeout_init_cpu_timer, nvgpu_timeout_expired,
 *          nvgpu_timeout_peek_expired
 *
 * Input:
 * 1) Global nvgpu_timeout structure instance.
 * 2) Global defines for flag and duration parameters.
 *
 * Steps:
 * 1) Reset the global nvgpu_timeout structure to all 0s.
 * 2) Initialise the timeout structure.
 * 3) Check the return value for error.
 * 4) Check for timer status. Confirm that timer has not expired.
 * 5) Sleep for the required duration + 500ms to ensure the timer expires.
 * 6) Check for the timer status.
 * 7) Reconfirm the timer status.
 *
 * Output:
 * Test returns PASS if the timer expires after the programmed
 * duration.
 * Test returns FAIL if initialisation routine returns error, if the timer
 * does not expire after programmed duration or if timer expires before
 * programmed duration.
 *
 */
int test_timer_duration(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_timer_duration
 *
 * Description: Test fault injection timer functionality.
 *
 * Test Type: Feature
 *
 * Input:
 * 1) Global nvgpu_timeout structure instance.
 * 2) Global defines for flag and duration parameters.
 *
 * Steps:
 * 1) Reset the global nvgpu_timeout structure to all 0s.
 * 2) Initialise the timeout structure.
 * 3) Check the return value for error.
 * 4) Initialize fault injection counter and enable fault injection.
 * 5) Check for the timer status. Confirm that return value is 0.
 * 6) Check for the timer status. Confirm that return value is 0.
 * 7) Sleep for the required duration + 500ms to ensure the timer expires.
 * 8) Check for the timer status.
 *
 * Output:
 * Test returns PASS if the timer expires after the programmed
 * duration.
 * Test returns FAIL if the initialisation routine returns error, if function
 * returns non-zero value when fault injection is enabled or if the timer does
 * not expire even after the programmed duration.
 *
 */
int test_timer_fault_injection(struct unit_module *m,
			      struct gk20a *g, void *args);

/**
 * Test specification for test_timer_delay
 *
 * Description: Test the delay functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_current_time_us, nvgpu_udelay,
 *          nvgpu_usleep_range
 *
 * Input: None.
 *
 * Steps:
 * 1) Get the current time in us.
 * 2) Delay the execution using nvpgu_udelay function.
 * 3) Get the time after the delay function is executed.
 * 4) Calculate the difference between both timestamps.
 * 5) Convert it into msec.
 * 6) If the difference is less than the duration for which the
 *    delay was requested, return fail.
 * 7) Continue steps 1-6 for the wrapper api nvgpu_usleep_range
 *    which internally uses nvgpu_udelay itself.
 *
 * Output:
 * Test returns PASS if the delay function actually delays the execution
 * for required amount of time.  It also returns PASS if there is a
 * reordering of instructions resulting in the test check being invalid
 * and the test is skipped by returning PASS.
 * Test returns FAIL if the delay function returns before the required
 * duration.
 *
 */
int test_timer_delay(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_timer_msleep
 *
 * Description: Test the sleep functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_current_time_ms, nvgpu_msleep
 *
 * Input: None.
 *
 * Steps:
 * 1) Get the current time in ms.
 * 2) Call sleep function for 5ms.
 * 3) Get the time after the sleep call.
 * 4) Calculate the difference between both the timestamps.
 * 5) Compare the difference to deduce the test result.
 *
 * Output:
 * Test returns PASS if the sleep function is completed for required duration.
 * Test returns FAIL if the sleep function returns before the requested
 * duration.
 *
 */
int test_timer_msleep(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_timer_hrtimestamp
 *
 * Description: Test the high resolution counter based functionalities.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_hr_timestamp
 *
 * Input: None.
 *
 * Steps:
 * 1) Initialise two counter variables to 0.
 * 2) Read the value of HR counter into one of the counter variables.
 * 3) Compare the value of read counter value with the bkp counter value.
 * 4) If read counter variable is less than the previously read counter value
 *    return fail.
 * 5) Store the read counter value in bkp counter value.
 * 6) Suspend execution by calling usleep.
 * 7) Loop steps 1 - 6 for multiple times.
 *
 * Output:
 * Test returns PASS if for every read of HR counter, the value returned is
 * either greater than or equal to the previous value.
 * Test returns FAIL if any of the subsequent read of HR counter returns
 * a value less than the previous value.
 *
 */
int test_timer_hrtimestamp(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_timer_compare
 *
 * Description: Compare the timers in various resoutions.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_current_time_ms, nvgpu_current_time_ns
 *
 * Input: None.
 *
 * Steps:
 * 1) Initialise two timestamp variables.
 * 2) Read the time in ms and store in one timestamp variable.
 * 3) Read the time in ns and store in the second timestamp variable.
 * 4) Do the necessary conversion to make both timers in same resolution.
 * 5) Compare the timer values to determine the test results.
 *
 * Output:
 * Test returns PASS if various timer resolutions match each other.
 * Test returns FAIL if various timer resolutions does not match with
 * each other.
 *
 */
int test_timer_compare(struct unit_module *m,
		struct gk20a *g, void *args);

#endif /* __UNIT_POSIX_TIMERS_H__ */
