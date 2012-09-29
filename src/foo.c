/*
 * foo.c
 *
 *  Created on: 2012-09-29
 *      Author: francis
 */


int function1 (void) {
  return 0;
}

int function2 (void) {
  return function1 () + 1;
}

int function3 (void) {
  return function2 () + 1;
}
