#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <dlfcn.h>

#include "miniprof.h"

static int level = 0;
static int enabled = 0;
static int maxdepth = 0;
static int debug = 0;
static int pos;
static int numev;

static struct mp_ev *ringbuffer = NULL;

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
	if (!entry)
		level--;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	if (debug)
		fprintf(stdout, "- %d %p %p %ld %ld\n", level, this_fn, call_site, ts.tv_sec, ts.tv_nsec);
	if (ringbuffer != NULL) {
		ringbuffer[pos].depth		= level;
		ringbuffer[pos].this_fn		= this_fn;
		ringbuffer[pos].call_site	= call_site;
		ringbuffer[pos].ts			= ts;
		pos = (pos + 1) % numev;
	}
	if (entry)
		level++;
	if (level > maxdepth)
		maxdepth = level;
}

void __cyg_profile_func_enter (void *this_fn, void *call_site)
{
	__miniprof_event(FN_ENTRY, this_fn, call_site);
}

void __cyg_profile_func_exit (void *this_fn, void *call_site)
{
	__miniprof_event(FN_EXIT, this_fn, call_site);
}

int miniprof_init(int size) {
	if (ringbuffer != NULL)
		miniprof_close();
	numev = size;
	pos = 0;
	ringbuffer = calloc(sizeof(struct mp_ev), size);
	if (ringbuffer == NULL)
		return -1;
	return 0;
}

void miniprof_close() {
	free(ringbuffer);
	ringbuffer = NULL;
	pos = 0;
	numev = 0;
}

void miniprof_enable() {
	enabled = 1;
}

void miniprof_disable() {
	enabled = 0;
}

int miniprof_maxdepth() {
	return maxdepth;
}

void miniprof_dump_events() {
	int i;
	int curr = pos;
	int max = 0;
	Dl_info sym;

	fprintf(stdout, "  %-5s %-10s %-10s %10s %10s %s\n",
			"depth", "thisfn", "callsite", "sec", "nsec", "symname");
	for (i = 0; i < numev; i++) {
		struct mp_ev *ev = &ringbuffer[curr];
		if (ev->ts.tv_sec != 0) {
			int ret = dladdr(ev->this_fn, &sym);
			const char *symname = NULL;
			if (ret != 0)
				symname = sym.dli_sname;
			fprintf(stdout, "- %5ld 0x%-8p 0x%-8p %10ld %10ld %s\n",
					ev->depth, ev->this_fn, ev->call_site,
					ev->ts.tv_sec, ev->ts.tv_nsec, symname);
			if (ev->depth > max)
				max = ev->depth;
		}
		curr = (curr + 1) % numev;
	}
	printf("max=%d\n", max);
}
