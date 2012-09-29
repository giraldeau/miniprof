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

#ifdef __cplusplus
};
#endif

#define FN_ENTRY 1
#define FN_EXIT 0
#define MINIPROF_DEFAULT_NUMEVENTS

struct mp_ev {
	unsigned long 	depth;
	void 			*this_fn;
	void 			*call_site;
	struct timespec	ts;
};

#endif /* MINIPROF_H_ */
