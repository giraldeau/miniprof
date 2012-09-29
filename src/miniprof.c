#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "miniprof.h"

static int level = 0;
static int enabled = 0;

struct timespec diffts(struct timespec *start, struct timespec *end)
{
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 0 };

	if (start == NULL || end == NULL)
		return ts;
	if ((end->tv_nsec - start->tv_nsec) < 0) {
		ts.tv_sec = end->tv_sec - start->tv_sec - 1;
		ts.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
	} else {
		ts.tv_sec = end->tv_sec - start->tv_sec;
		ts.tv_nsec = end->tv_nsec - start->tv_nsec;
	}
	return ts;
}

static inline void __miniprof_event(unsigned long entry, void *this_fn, void *call_site)
{
	struct timespec ts;
	if(!enabled)
		return;

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	entry ? level++ : level--;
	fprintf(stdout, "- %d %p %p %ld %ld\n", level, this_fn, call_site, ts.tv_sec, ts.tv_nsec);
}

void __cyg_profile_func_enter (void *this_fn, void *call_site)
{
	__miniprof_event(FN_ENTRY, this_fn, call_site);
}

void __cyg_profile_func_exit (void *this_fn, void *call_site)
{
	__miniprof_event(FN_EXIT, this_fn, call_site);
}

void miniprof_enable() {
	enabled = 1;
}

void miniprof_disable() {
	enabled = 0;
}
