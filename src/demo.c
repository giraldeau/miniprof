/*
 * demo.c
 *
 *  Created on: 2012-09-29
 *      Author: francis
 */

#include <stdio.h>
#include "miniprof.h"

int function1 (void) {
  return 0;
}

int function2 (void) {
  return function1 () + 1;
}

int function3 (void) {
  return function2 () + 1;
}

int main (void) {
  //printf ("Logfile: %s\n", cygprofile_getfilename ());
  miniprof_enable();
  function3();
  miniprof_disable();
  function3();
  return 0;
}
