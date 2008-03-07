import shm_wrapper as shm
import pyfits

GUPPI_STATUS_KEY = 16783408

def header_from_string(str):
    cl = cardlist_from_string(str)
    return pyfits.Header(cl)

def card_from_string(str):
    card = pyfits.Card()
    return card.fromstring(str)

def cardlist_from_string(str):
    cardlist = []
    numcards = len(str)/80
    for ii in range(numcards):
        str_part = str[ii*80:(ii+1)*80]
        if str_part.strip()=="END":
            break
        else:
            cardlist.append(card_from_string(str_part))
    return cardlist

if __name__=="__main__":
    status_buffer = shm.SharedMemoryHandle(GUPPI_STATUS_KEY)
    hdr = header_from_string(status_buffer.read())
    for k, v in hdr.items():
        print "%8s :"%k, v

