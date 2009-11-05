from guppi_utils import guppi_status, guppi_databuf
from guppi.client import Client
import curses, curses.wrapper
import time

def display_status(stdscr,stat,data,client):
    # Set non-blocking input
    stdscr.nodelay(1)
    run = 1

    # Look like gbtstatus (why not?)
    curses.init_pair(1, curses.COLOR_CYAN, curses.COLOR_BLACK)
    curses.init_pair(2, curses.COLOR_GREEN, curses.COLOR_BLACK)
    curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_RED)
    keycol = curses.color_pair(1)
    valcol = curses.color_pair(2)
    errcol = curses.color_pair(3)

    # Loop 
    while (run):
        # Refresh status info
        stat.read()

        # Reset screen
        stdscr.erase()

        # Draw border
        stdscr.border()

        # Get dimensions
        (ymax,xmax) = stdscr.getmaxyx()

        # Display main status info
        onecol = False # Set True for one-column format
        col = 2
        curline = 0
        stdscr.addstr(curline,col,"Current GUPPI status:", keycol);
        curline += 2
        flip=0
        for k,v in stat.hdr.items():
            if (curline < ymax-3):
                stdscr.addstr(curline,col,"%8s : "%k, keycol)
                stdscr.addstr("%s" % v, valcol)
            else:
                stdscr.addstr(ymax-3,col, "-- Increase window size --", errcol);
            if (flip or onecol):
                curline += 1
                col = 2
                flip = 0
            else:
                col = 40
                flip = 1
        col = 2
        if (flip and not onecol):
            curline += 1

        # Get relevant HW values
        unk = "Unknown"
        hw_acclen = 0
        hw_bw = 0
        try:
            hw_acclen = int(client.get("BEE2/FPGA2/ACC_LENGTH"),16) + 1
        except:
            hw_acclen = unk
        try:
            hw_bw = float(client.get("SYNTH/CFRQ/VALUE").strip("MHz"))
        except:
            hw_bw = unk
        hw_nchan1 = 0
        hw_nchan3 = 0
        hw_nchan = 0
        for bof in client.unload():
            fields = bof.split("_")
            if fields[1]=="U1":
                hw_nchan1 = int(fields[2])
            if fields[1]=="U3":
                hw_nchan3 = int(fields[2])
        if hw_nchan1 == hw_nchan3:
            hw_nchan = hw_nchan1
        else:
            hw_nchan = unk
        if hw_nchan == 0:
            hw_nchan = unk
            
        # Test for consistency
        hw_sw_ok = True
        try:
            if hw_acclen!=unk and hw_acclen!=stat.hdr["ACC_LEN"]: hw_sw_ok = False
            if hw_nchan!=unk and hw_nchan!=stat.hdr["OBSNCHAN"]: hw_sw_ok = False
            if hw_bw!=unk and hw_bw!=abs(stat.hdr["OBSBW"]): hw_sw_ok = False
        except KeyError:
            pass

        # Print HW values
        if (curline < ymax-4):
            curline += 1 
            stdscr.addstr(curline,col,"Current GUPPI hardware parameters:",
                    keycol)
            curline += 1
            if hw_sw_ok == False:
                stdscr.addstr(curline, col+5, 
                        "-- WARNING: Hardware and software values are inconsistent! --", 
                        errcol)
                curline += 1
            stdscr.addstr(curline,col,"%8s : " % "NCHAN", keycol)
            stdscr.addstr("%s" % hw_nchan, valcol)
            col = 40
            stdscr.addstr(curline,col,"%8s : " % "BW", keycol)
            stdscr.addstr("%s" % hw_bw, valcol)
            col = 2
            curline += 1
            stdscr.addstr(curline,col,"%8s : " % "ACC_LEN", keycol)
            stdscr.addstr("%s" % hw_acclen, valcol)
            col = 2
            curline += 1

        # Refresh current block info
        try:
            curblock = stat["CURBLOCK"]
        except KeyError:
            curblock=-1

        # Display current packet index, etc
        if (curblock>=0 and curline < ymax-4):
            curline += 1
            stdscr.addstr(curline,col,"Current data block info:",keycol)
            curline += 1
            data.read_hdr(curblock)
            try:
                pktidx = data.hdr[curblock]["PKTIDX"]
            except KeyError:
                pktidx = "Unknown"
            stdscr.addstr(curline,col,"%8s : " % "PKTIDX", keycol)
            stdscr.addstr("%s" % pktidx, valcol)

        # Figure out if we're folding
        foldmode = False
        try:
            foldstat = stat["FOLDSTAT"]
            curfold = stat["CURFOLD"]
            if (foldstat!="exiting"):
                foldmode = True
        except KeyError:
            foldmode = False

        # Display fold info
        if (foldmode and curline < ymax-4):
            folddata = guppi_databuf(2)
            curline += 2
            stdscr.addstr(curline,col,"Current fold block info:",keycol)
            curline += 1
            folddata.read_hdr(curfold)
            try:
                npkt = folddata.hdr[curfold]["NPKT"]
                ndrop = folddata.hdr[curfold]["NDROP"]
            except KeyError:
                npkt = "Unknown"
                ndrop = "Unknown"
            stdscr.addstr(curline,col,"%8s : " % "NPKT", keycol)
            stdscr.addstr("%s" % npkt, valcol)
            curline += 1
            stdscr.addstr(curline,col,"%8s : " % "NDROP", keycol)
            stdscr.addstr("%s" % ndrop, valcol)

        # Bottom info line
        stdscr.addstr(ymax-2,col,"Last update: " + time.asctime() \
                + "  -  Press 'q' to quit")

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

# Connect to guppi status, data bufs, client
g = guppi_status()
d = guppi_databuf()
c = Client()

# Wrapper calls the main func
try:
    curses.wrapper(display_status,g,d,c)
except KeyboardInterrupt:
    print "Exiting..."


