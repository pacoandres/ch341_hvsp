#ifndef _CH341I_H_ 
#define _CH341I_H_
#include <libusb-1.0/libusb.h>

#define MAX_ERROR_LEN 256

typedef struct chipmem {
  int flashpages;
  int flashpagebytes;
  int eeprompages;
  int eeprompagebytes;
  unsigned signature;
  const char *name;
} chipmem;


class CH341i {

private:
	libusb_device_handle *m_udh;
	libusb_context *m_usb_context;
	uint8_t m_outmask;
	uint8_t m_outstate;
    int m_chiptype;
    
    int dataReady ();
    int addTic (uint8_t *buf, int pos);
    
    int sendreadHVSP (uint8_t val, uint8_t val1, uint8_t *ret); //Returns 1 on error
    int sendHVSP (uint8_t val, uint8_t val1); //Returns 1 on error
    char mylasterror[MAX_ERROR_LEN+1];
	
public:
	//using enum fusetype;
    enum fusetype: uint8_t {HF = 0, LF, EF};
    int start (); //Returns 1 on error
    int end (); //Returns 1 on error
    int chipErase ();
    int readSignature (unsigned *signature);
    void close ();
    CH341i ();
    ~CH341i ();
    int open (); //Opens device. Returns 1 on error.

    int writeFlashPage (int pageno, uint8_t *pagedata, int length); //Returns 1 on error
    int startFlashWrite ();
    int startFlashRead ();
    int readFlash (int address, uint8_t *hibit, uint8_t *lobit);
    
    
    int startEEPROMWrite ();
    int writeEEPROMPage (int pageno, uint8_t *pagedata, int length); //Returns 1 on error
    int startEEPROMRead ();
    int readEEPROM (int address, uint8_t *value);
    int finishOperation ();
    
    int writeFuse (uint8_t fuse, uint8_t value); //Returns 1 on error
    int readFuse (uint8_t fuse, uint8_t *value); //Returns 1 on error
    
    int readCalib (uint8_t *calib); //Returns 1 on error
    
    int readLock (uint8_t *lockbits);
    int writeLock (uint8_t lockbits);
    const char *getChipName ();
    const chipmem *getChip ();
    const char *getError ();
    void test ();
    
};


#endif //_CH341I_H_
