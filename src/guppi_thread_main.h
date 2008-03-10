/* guppi_thread_main.h
 *
 * Include in any main program that will 
 * call the thread functions.
 */
#ifndef _GUPPI_THREAD_MAIN_H
#define _GUPPI_THREAD_MAIN_H

/* Control-C handler */
int run=1;
void cc(int sig) { run=0; }

#endif
