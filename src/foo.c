/*
 * foo.c
 *
 *  Created on: 2012-09-29
 *      Author: francis
 */

#include <stdlib.h>
#include <unistd.h>

#define SLEEP_TIME 10000

int function1 (void) {
	usleep(SLEEP_TIME);
	return 0;
}

int function2 (void) {
	usleep(SLEEP_TIME);
	return function1() + 1;
}

int function3 (void) {
	usleep(SLEEP_TIME);
	return function2() + 1;
}
