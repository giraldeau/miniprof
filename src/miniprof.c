#include <stdlib.h>
#include <stdio.h>

#include "miniprof.h"

static int level = 0;
static int enabled = 0;

void __cyg_profile_func_enter (void *this_fn, void *call_site)
{
	if(enabled)
		fprintf(stdout, "+ %d %p %p\n", level++, this_fn, call_site);
}

void __cyg_profile_func_exit (void *this_fn, void *call_site)
{
	if(enabled)
		fprintf(stdout, "- %d %p %p\n", level--, this_fn, call_site);
}

void miniprof_enable() {
	enabled = 1;
}

void miniprof_disable() {
	enabled = 0;
}
