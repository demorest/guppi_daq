import shm_wrapper as shm
from GBTStatus import GBTStatus
import pyfits, possem
import psr_utils as psr

def header_from_string(str):
    """
    header_from_string(str):
        Convert an input string (which should be the ASCII header from
            a FITS HFU) into an instantiation of a pyfits 'Header' class.
    """
    cl = cardlist_from_string(str)
    return pyfits.Header(cl)

def card_from_string(str):
    """
    card_from_string(str):
        Return a pyfits 'Card' based on the input 'str', which should
            be an 80-character 'line' from a FITS header.
    """
    card = pyfits.Card()
    return card.fromstring(str)

def cardlist_from_string(str):
    """
    cardlist_from_string(str):
        Return a list of pyfits 'Cards' from the input string.
            'str' should be the ASCII from a FITS header.
    """
    cardlist = []
    numcards = len(str)/80
    for ii in range(numcards):
        str_part = str[ii*80:(ii+1)*80]
        if str_part.strip()=="END":
            break
        else:
            cardlist.append(card_from_string(str_part))
    return cardlist

GUPPI_STATUS_KEY = 16783408
GUPPI_STATUS_SEMID = "/guppi_status"

class guppi_status:

    def __init__(self):
        self.stat_buf = shm.SharedMemoryHandle(GUPPI_STATUS_KEY)
        self.sem = possem.sem_open(GUPPI_STATUS_SEMID, possem.O_CREAT, 00644, 1)
        self.hdr = None
        self.gbtstat = None

    def lock(self):
        return possem.sem_wait(self.sem)

    def unlock(self):
        return possem.sem_post(self.sem)

    def read(self):
        self.hdr = header_from_string(self.stat_buf.read())

    def write(self):
        if self.hdr is None:
            self.read()
        self.lock()
        self.stat_buf.write(repr(self.hdr.ascard)+"END"+" "*77)
        self.unlock()

    def update(self, key, value, comment=None):
        if self.hdr is None:
            self.read()
        else:
            self.hdr.update(key, value, comment)

    def show(self):
        if self.hdr is None:
            self.read()
        else:
            for k, v in self.hdr.items():
                print "'%8s' :"%k, v
            print ""

    def update_with_gbtstatus(self):
        if self.gbtstat is None:
            self.gbtstat = GBTStatus()
        self.gbtstat.collectKVPairs()
        g = self.gbtstat.kvPairs
        self.update("OBSERVER", g['observer'])
        self.update("PROJID", g['data_dir'])
        self.update("FRONTEND", g['receiver'])
        self.update("NRCVR", 2) # I think all the GBT receivers have 2...
        if 'inear' in g['rcvr_pol']:
            self.update("FD_POLN", 'LIN')
        else:
            self.update("FD_POLN", 'CIRC')
        freq = float(g['freq'])
        self.update("OBSFREQ", freq)
        self.update("SRC_NAME", g['source'])
        if g['ant_motion']=='Tracking':
            self.update("TRK_MODE", 'TRACK')
        else:
            self.update("TRK_MODE", 'UNKNOWN')
        if g['epoch']=='J2000':
            self.ra_str = self.gbtstat.getMajor().split()[0]
            self.ra = float(g['major'].split()[0])
            self.dec_str = self.gbtstat.getMinor().split()[0]
            self.dec = float(g['minor'].split()[0])
            self.update("RA_STR", self.ra_str)
            self.update("RA", self.ra)
            self.update("DEC_STR", self.dec_str)
            self.update("DEC", self.dec)
        h, m, s = g['lst'].split(":")
        lst = int(round(psr.hms_to_rad(int(h),int(m),float(s))*86400.0/psr.TWOPI))
        self.update("LST", lst)
        self.update("AZ", float(g['az_actual']))
        self.update("ZA", 90.0-float(g['el_actual']))
        beam_deg = 2.0*psr.beam_halfwidth(freq, 100.0)/60.0
        self.update("BMAJ", beam_deg)
        self.update("BMIN", beam_deg)

if __name__=="__main__":
    g = guppi_status()
    g.read()
    g.show()

    g.update("UPDATE", 3.12343545)
    g.write()
    g.show()

    g.update_with_gbtstatus()
    g.write()
    g.show()
