/* guppi_data_acq.c
 *
 * Main GUPPI program to read data via network from the BEE2 then
 * format and write it to disk.  Listens for commands and config
 * info from GUPPI control software.
 */

/* Descibe command line options */
void usage() {
    fprintf(stderr, 
            "Usage: guppi_data_acq [options]\n"
            "Options:\n"
            "  None yet!\n"
           );
}

/* Basic SIGINT catcher */
int run=1;
void stop_daq(int sig) { run=0; }

int main(int argc, char *argv[]) {

    /* Process command line options */

    /* Initialize things like:
     *   - socket connection to control
     *   - shared mem buffers for data (?)
     *   - ?
     */

    /* Main loop */
    while (run) {

        /* Check for commands from control s/w. */

        /* Command processing */
        if (got_cmd_start) {
            if (status==READY) { 
                /* Parse config option that came w/ cmd */
                /* Set up internal config variables */
                /* Launch UDP read thread */
                /* Launch disk write thread */
                /* Wait for "ok" from both threads (w/ timeout?) */
                /* If timed out or error, reply error to control */
                /* Else, reply w/ "ok" */
                /* Update status variable appropriately */
            } else {
                /* Not ready to start, reply w/ error */
            }
        }

        if (got_cmd_stop) {
            if (status==RUNNING) {
                /* Send stop to all sub-threads */
                /* Wait for threads to finish (join?) */
                /* If finished ok, send ok back to control */
                /* Else send error */
            } else {
                /* Can't stop if we're not running - send error */
            }
        }

        /* Status check */
        if (status==RUNNING) {
            /* Check status of all threads */
            /* Update indicator vars appropriately */
        }
        /* Any generic status check here */

        /* Broadcast current status, "heartbeat" */

    }

    /* Cleanup:
     *   - close sockets
     *   - detach/remove shared mem
     */

    exit(0);
}
