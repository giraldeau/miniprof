/*
 * demo.c
 *
 *  Created on: 2012-09-29
 *      Author: francis
 */

#include <stdio.h>
#include "foo.h"
#include "miniprof.h"

int main (void) {
	miniprof_init(10);
	miniprof_enable();
	function3();
	miniprof_disable();
	function3();
	miniprof_dump_events();
	miniprof_close();
	printf("maxdepth=%d\n", miniprof_maxdepth());
	return 0;
}
