#ifndef _GUPPI_THREAD_ARGS_H
/* Generic thread args type with input/output buffer
 * id numbers.  Not all threads have both a input and a
 * output.
 */
struct guppi_thread_args {
    int input_buffer;
    int output_buffer;
};
#endif
