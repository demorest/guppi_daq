from guppi_utils import guppi_status, guppi_databuf
import curses, curses.wrapper
import time

def display_status(stdscr,stat,data):
    # Set non-blocking input
    stdscr.nodelay(1)
    run = 1

    # Loop 
    while (run):
        # Refresh status info
        stat.read()

        # Refresh current block info
        try:
            curblock = stat["CURBLOCK"]
        except KeyError:
            curblock=-1

        # Reset screen
        stdscr.erase()

        # Draw border
        stdscr.border()

        # Get dimensions
        (ymax,xmax) = stdscr.getmaxyx()

        # Display main status info
        col = 2
        curline = 0
        stdscr.addstr(curline,col,"Current GUPPI status info:");
        curline += 1
        for k,v in stat.hdr.items():
            if (curline < ymax-3):
                stdscr.addstr(curline,col,"%8s : %s"%(k,v))
            else:
                stdscr.addstr(ymax-3,col, "-- Increase window size --");
            curline += 1

        # Display current packet index, etc
        if (curblock>=0 and curline < ymax-4):
            curline += 1
            stdscr.addstr(curline,col,"Current data block info:")
            curline += 1
            data.read_hdr(curblock)
            try:
                pktidx = data.hdr[curblock]["PKTIDX"]
            except KeyError:
                pktidx = "Unknown"
            stdscr.addstr(curline,col,"%8s : %s" % ("PKTIDX", pktidx))

        stdscr.addstr(ymax-2,col,"Last update: " + time.asctime() \
                + "  -  Press 'q' or Ctrl-C to quit")

        # Redraw screen
        stdscr.refresh()

        # Sleep a bit
        time.sleep(0.25)

        # Look for input
        c = stdscr.getch()
        while (c != curses.ERR):
            if (c==ord('q')):
                run = 0
            c = stdscr.getch()

# Connect to guppi status, data bufs
g = guppi_status()
d = guppi_databuf()

# Wrapper calls the main func
try:
    curses.wrapper(display_status,g,d)
except KeyboardInterrupt:
    print "Exiting..."


