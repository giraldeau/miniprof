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
GQueue *fqueue = NULL;

static inline struct mp_ev *get_ev(int idx);

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

void print_fqueue_entry(gpointer data, gpointer userdata) {
	int idx = GPOINTER_TO_INT(data);
	int *depth = (int *) userdata;
	printf("%8d %6d ", *depth, idx);
	miniprof_dump_event(get_ev(idx), NULL, NULL);
	*depth = *depth + 1;
}

void sym_print_entry(gpointer key, gpointer val, gpointer data) {
	struct mp_stat *stat = (struct mp_stat *) val;
	double self = 0.0;
	if (stat->total > stat->children)
		self = stat->total - stat->children;
	printf("%10p %10.3f %10.3f %10.3f %10.3f %10.3f %10d %s\n", key, stat->total, stat->children,
			self, stat->min, stat->max, stat->count, stat->fname);
}

void miniprof_print_symtable() {
	printf("%10s %10s %10s %10s %10s %10s %10s %s\n",
			"addr", "total", "children", "self", "min", "max", "count", "fname");
	g_hash_table_foreach(symtable, sym_print_entry, NULL);
}

void miniprof_print_queue(GQueue *queue) {
	int depth = 0;
	if (g_queue_is_empty(queue)) {
		printf("queue is empty\n");
		return;
	}
	printf("%8s %6s ", "depth", "idx");
	miniprof_dump_event_header();
	g_queue_foreach(fqueue, print_fqueue_entry, &depth);
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

enum timescale {
	TS_NSEC, TS_USEC, TS_MSEC, TS_SSEC
};

#define TS_SCALE_NSEC 1000000000.0
#define TS_SCALE_USEC 1000000.0
#define TS_SCALE_MSEC 1000.0
#define TS_SCALE_SSEC 1.0

double convert_ts(struct timespec *t, int timescale) {
	if (t == NULL)
		return 0.0;
	double factor_sec = 0.0;
	double factor_nsec = 0.0;
	switch(timescale) {
		case TS_NSEC:
			factor_sec = TS_SCALE_NSEC;
			factor_nsec = TS_SCALE_SSEC;
			break;
		case TS_USEC:
			factor_sec = TS_SCALE_USEC;
			factor_nsec = TS_SCALE_MSEC;
			break;
		case TS_MSEC:
			factor_sec = TS_SCALE_MSEC;
			factor_nsec = TS_SCALE_USEC;
			break;
		case TS_SSEC:
			factor_sec = TS_SCALE_SSEC;
			factor_nsec = TS_SCALE_NSEC;
			break;
		default:
			break;
	}
	double time = ((double) t->tv_sec) * factor_sec;
	time += t->tv_nsec / factor_nsec;
	return time;
}

/*
 * Core tracing functions
 */

static inline void __miniprof_event(unsigned long entry, void *this_fn, void *call_site)
{
	struct timespec ts;

	if(!enabled)
		return;

	if (ringbuffer == NULL)
		return;

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	ringbuffer[pos].depth		= level;
	ringbuffer[pos].entry		= entry;
	ringbuffer[pos].this_fn		= this_fn;
	ringbuffer[pos].call_site	= call_site;
	ringbuffer[pos].ts			= ts;

	if (debug)
		miniprof_dump_event(get_ev(pos), NULL, NULL);

	pos = (pos + 1) % numev;

	if (entry) level++;
	else level--;
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

void miniprof_dump_event_header() {
	fprintf(stdout, "%s %-5s %-8s %-8s %10s %10s %15s %s\n",
			"e", "depth", "thisfn", "callsite", "sec", "nsec", "delta", "symname");
}

void miniprof_dump_event(struct mp_ev *ev, const char *fname, struct timespec *diff) {
	fprintf(stdout, "%d %5d %-8p %-8p %10ld %10ld %15.3f %s\n",
			ev->entry, ev->depth, ev->this_fn, ev->call_site,
			ev->ts.tv_sec, ev->ts.tv_nsec, convert_ts(diff, TS_USEC), fname);
}

void miniprof_dump_events() {
	int i;
	int idx = (evcount >= numev) ? pos : 0;
	int len = (evcount >= numev) ? numev : evcount;
	int max = 0;
	const char *fname;
	struct mp_ev *curr, *prev = NULL;
	struct timespec diff;

	miniprof_dump_event_header();
	prev = get_ev(idx);
	for (i = 0; i < len; i++) {
		curr = get_ev(idx);
		diff = diffts(&prev->ts, &curr->ts);
		fname = symname(curr->this_fn);
		miniprof_dump_event(curr, fname, &diff);
		if (curr->depth > max)
			max = curr->depth;
		idx = (idx + 1) % numev;
		prev = curr;
	}
	printf("maxdepth=%d\n", max);
}

struct mp_stat *make_mp_stat() {
	struct mp_stat *stat = calloc(sizeof(struct mp_stat), 1);
	if (stat == NULL)
		return NULL;
	return stat;
}

void free_mp_stat(gpointer data) {
	free(data);
}

static inline struct mp_ev *get_ev(int idx) {
	struct mp_ev *ev = NULL;
	if (idx >= 0 && idx < numev)
		ev = &ringbuffer[idx];
	assert(ev != NULL);
	return ev;
}

void miniprof_report() {
	int i, top;
	struct mp_ev *ev, *parent, *sibling;
	struct mp_stat *stat;
	int idx = (evcount >= numev) ? pos : 0;
	int len = (evcount >= numev) ? numev : evcount;
	struct timespec delta;
	double time;
	const char *fname;

	/*
	 * Init queue according to initial depth. This situation
	 * may occur in case of ringbuffer overwrite
	 */
	fqueue = g_queue_new();
	ev = get_ev(idx);
	for (i = 0; i < ev->depth; i++) {
		g_queue_push_tail(fqueue, GINT_TO_POINTER(idx));
	}

	// event processing loop
	for (i = 0; i < len; i++) {
		ev = get_ev(idx);

		// insert function stat entry if required
		if (!g_hash_table_contains(symtable, ev->this_fn)) {
			fname = symname(ev->this_fn);
			stat = make_mp_stat();
			if (stat == NULL)
				goto done;
			stat->fname = fname;
			g_hash_table_insert(symtable, ev->this_fn, stat);
		}

		// process one event
		if (ev->entry) {
			// update count
			stat = g_hash_table_lookup(symtable, ev->this_fn);
			assert(stat != NULL);
			stat->count++;

			// push current function on the stack
			g_queue_push_tail(fqueue, GINT_TO_POINTER(idx));
			printf("push %d\n", idx);
		} else {
			// compute time inside this function
			assert(!g_queue_is_empty(fqueue));
			top = GPOINTER_TO_INT(g_queue_pop_tail(fqueue));
			sibling = get_ev(top);
			stat = g_hash_table_lookup(symtable, sibling->this_fn);
			assert(stat != NULL);
			delta = diffts(&sibling->ts, &ev->ts);
			time = convert_ts(&delta, TS_USEC);
			stat->total = time;

			// update min: use flag to avoid bootstrap float comparison
			if (!stat->min_is_set) {
				stat->min = time;
				stat->min_is_set = 1;
			} else {
				if (stat->min > time)
					stat->min = time;
			}

			// update max
			if (stat->max < time)
				stat->max = time;

			// remove self to parent
			if (!g_queue_is_empty(fqueue)) {
				printf("add children time to parent\n");
				miniprof_print_queue(fqueue);
				top = GPOINTER_TO_INT(g_queue_peek_tail(fqueue));
				parent = get_ev(top);
				stat = g_hash_table_lookup(symtable, parent->this_fn);
				assert(stat != NULL);
				stat->children += time;
			}

			printf("pop  %d\n", top);
		}

		// advance
		idx = (idx + 1) % numev;
	}
	assert(g_queue_is_empty(fqueue));
done:
	miniprof_print_queue(fqueue);
	g_queue_free(fqueue);
	fqueue = NULL;
}
