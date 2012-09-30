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
	int i;
	miniprof_init(100);
	miniprof_enable();
	for (i = 0; i < 2; i++)
		function3();
	miniprof_disable();
	function3();
	miniprof_dump_events();
	miniprof_save("miniprof.out");
	miniprof_report();
	miniprof_close();
	return 0;
}
