#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <dlfcn.h>
#include <glib.h>
#include <assert.h>

#include "miniprof.h"

static int level = 0;
static int enabled = 0;
static int maxdepth = 0;
static int debug = 0;
static int evcount = 0;
static int pos;
static int numev;

static struct mp_ev *ringbuffer = NULL;
GHashTable *symtable = NULL;

/*
 * HashTable related functions
 */

gboolean sym_equal_func(gconstpointer a, gconstpointer b) {
	if (a == b)
		return TRUE;
	if (a == NULL || b == NULL)
		return FALSE;
	if (*(unsigned long *)a == *(unsigned long *)b)
		return TRUE;
	return FALSE;
}

void sym_print_entry(gpointer key, gpointer val, gpointer data) {
	printf("0x%p %s\n", (unsigned long *)key, (const char *) val);
}

void miniprof_print_symtable() {
	g_hash_table_foreach(symtable, sym_print_entry, NULL);
}

/*
 * Utilities
 */

/*
 * Returns end - start
 */
struct timespec diffts(struct timespec *start, struct timespec *end)
{
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 0 };

	if (start == NULL || end == NULL)
		return ts;
	if ((end->tv_nsec - start->tv_nsec) < 0) {
		ts.tv_sec = end->tv_sec - start->tv_sec - 1;
		ts.tv_nsec = 1000000000L + end->tv_nsec - start->tv_nsec;
	} else {
		ts.tv_sec = end->tv_sec - start->tv_sec;
		ts.tv_nsec = end->tv_nsec - start->tv_nsec;
	}
	return ts;
}

/*
 * Returns t1 + t2
 */
struct timespec addts(struct timespec *t1, struct timespec *t2) {
	struct timespec sum = { .tv_sec = 0, .tv_nsec = 0 };

	if (t1 == NULL || t2 == NULL)
		return sum;
	sum.tv_sec = t1->tv_sec + t2->tv_sec;
	sum.tv_nsec = t1->tv_nsec + t2->tv_nsec;
	if (sum.tv_nsec >= (1000000000L)) {
		sum.tv_sec++;
		sum.tv_nsec = sum.tv_nsec - 1000000000L;
	}
	return sum;
}

/*
 * Core tracing functions
 */

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
		ringbuffer[pos].entry		= entry;
		ringbuffer[pos].this_fn		= this_fn;
		ringbuffer[pos].call_site	= call_site;
		ringbuffer[pos].ts			= ts;
		pos = (pos + 1) % numev;
	}
	if (entry)
		level++;
	if (level > maxdepth)
		maxdepth = level;
	evcount++;
}

void __cyg_profile_func_enter (void *this_fn, void *call_site)
{
	__miniprof_event(FN_ENTRY, this_fn, call_site);
}

void __cyg_profile_func_exit (void *this_fn, void *call_site)
{
	__miniprof_event(FN_EXIT, this_fn, call_site);
}

/*
 * Management function
 */

int miniprof_init(int size) {
	if (size <= 0)
		return -1;
	if (ringbuffer != NULL)
		miniprof_close();
	numev = size;
	pos = 0;
	evcount = 0;
	ringbuffer = calloc(sizeof(struct mp_ev), size);
	if (ringbuffer == NULL)
		goto error;
	symtable = g_hash_table_new(g_direct_hash, sym_equal_func);
	if (symtable == NULL)
		goto error;
	return 0;
error:
	miniprof_close();
	return -1;
}

void miniprof_save(const char *filename) {
	int i;
	size_t ret;
	int idx = (evcount >= numev) ? pos : 0;
	int len = (evcount >= numev) ? numev : evcount;

	FILE *f = fopen(filename, "w");
	fwrite(&len, sizeof(int), 1, f);
	for (i = 0; i < len; i++) {
		ret = fwrite(&ringbuffer[idx], sizeof(struct mp_ev), 1, f);
		if (ret != 1) {
			fprintf(stderr, "ERROR: partial buffer save ret=%ld\n", ret);
			break;
		}
		idx = (idx + 1) % len;
	}
	fclose(f);
}

void miniprof_close() {
	free(ringbuffer);
	ringbuffer = NULL;
	pos = 0;
	numev = 0;
	if (symtable != NULL)
		g_hash_table_destroy(symtable);
}

void miniprof_enable() {
	enabled = 1;
}

void miniprof_disable() {
	enabled = 0;
}

void miniprof_reset() {
	pos = 0;
	evcount = 0;
	level = 0;
	maxdepth = 0;
}

int miniprof_maxdepth() {
	return maxdepth;
}

static inline char const *symname(void *addr) {
	Dl_info sym;
	int ret = dladdr(addr, &sym);
	if (ret != 0)
		return sym.dli_sname;
	return NULL;
}

/*
 * Display and reporting functions
 */

void miniprof_dump_events() {
	int i;
	int curr = pos;
	int max = 0;
	const char *fname;

	fprintf(stdout, "%s %-5s %-10s %-10s %10s %10s %s\n",
			"e", "depth", "thisfn", "callsite", "sec", "nsec", "symname");
	for (i = 0; i < numev; i++) {
		struct mp_ev *ev = &ringbuffer[curr];
		if (ev->ts.tv_sec != 0) {
			fname = symname(ev->this_fn);
			fprintf(stdout, "%d %5d 0x%-8p 0x%-8p %10ld %10ld %s\n",
					ev->entry, ev->depth, ev->this_fn, ev->call_site,
					ev->ts.tv_sec, ev->ts.tv_nsec, fname);
			if (ev->depth > max)
				max = ev->depth;
		}
		curr = (curr + 1) % numev;
	}
	printf("maxdepth=%d\n", max);
}

void miniprof_report() {
	int i;
	struct mp_ev *ev;
	int idx = (evcount >= numev) ? pos : 0;
	int len = (evcount >= numev) ? numev : evcount;

	const char *fname;
	for (i = 0; i < len; i++) {
		ev = &ringbuffer[idx];
		assert(ev->ts.tv_sec != 0);

		// insert function name if required
		if (!g_hash_table_contains(symtable, ev->this_fn)) {
			fname = symname(ev->this_fn);
			g_hash_table_insert(symtable, ev->this_fn, (char *)fname);
		}


		idx = (idx + 1) % numev;
	}

}
