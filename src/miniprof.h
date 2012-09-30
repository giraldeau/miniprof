/*
 * miniprof.h
 *
 *  Created on: 2012-09-29
 *      Author: francis
 */

#ifndef MINIPROF_H_
#define MINIPROF_H_

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

void __cyg_profile_func_enter (void *this_fn, void *call_site)
	__attribute__ ((no_instrument_function));
void __cyg_profile_func_exit (void *this_fn, void *call_site)
	__attribute__ ((no_instrument_function));

void miniprof_enable(void) __attribute__ ((no_instrument_function));
void miniprof_disable(void) __attribute__ ((no_instrument_function));
struct timespec diffts(struct timespec *start, struct timespec *end);
int miniprof_maxdepth(void);
int miniprof_init(int maxev);
void miniprof_close();
void miniprof_dump_events(void);
void miniprof_save(const char *filename);
void miniprof_report();
#ifdef __cplusplus
};
#endif

#define FN_ENTRY 1
#define FN_EXIT 0
#define MINIPROF_DEFAULT_NUMEVENTS

struct mp_ev {
	struct timespec	ts;
	void 			*this_fn;
	void 			*call_site;
	unsigned char 	depth;
	unsigned char	entry;
};

struct mp_stat {
	double total;
	double self;
	int count;
};

#endif /* MINIPROF_H_ */
