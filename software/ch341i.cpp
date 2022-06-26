
#include <libusb-1.0/libusb.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "ch341i.h"





#define US_MASK 0x1F  //Up to 32 (approx) us delay



#define MAX_BUFFER_LENGTH 32 //Cant transfer to CH341 more than 32 bytes at once
#define MAX_BB_BUF_LEN (MAX_BUFFER_LENGTH - 2)


#define CMD_UIO_READ (uint8_t)0xA0 
#define CMD_UIO_STREAM (uint8_t)0xAB
#define CMD_UIO_STM_DIR (uint8_t)0x40
#define CMD_UIO_STM_OUT (uint8_t)0x80
#define CMD_UIO_STM_END	(uint8_t)0x20
#define CMD_UIO_READ_END (uint8_t)0x20 
#define	CMD_UIO_STM_US (uint8_t)0xC0  
#define CMD_UIO_STM_IN (uint8_t)0x00

#define DELAY_US CMD_UIO_STM_US | 1
#define DELAY_31US CMD_UIO_STM_US | US_MASK
#define DELAY_10US CMD_UIO_STM_US | 10


#define ENDPOINT_IN 0x82
#define ENDPOINT_OUT 0x02

#define MAX_PIN_READ 7
#define MAX_PIN_WRITE 5

#define USB_TIMEOUT 1000

#define DEV_CONTROL_CFG 0xC0
#define DEV_CONTROL_BUF_LEN 8


//CH341 pinout
#define D0 (uint8_t)0x01
#define D1 (uint8_t)0x02
#define D2 (uint8_t)0x04
#define D3 (uint8_t)0x08
#define D4 (uint8_t)0x10
#define D5 (uint8_t)0x20


//CH341-HVSP pinout
#define  RST  D0// Output !RESET from transistor to Pin 1 (PB5) on ATTiny85
#define  CLKOUT  D1    // Connect to Serial Clock Input (SCI) Pin 2 (PB3) on ATTiny85
#define  DATAIN  D2    // Connect to Serial Data Output (SDO) Pin 7 (PB2) on ATTiny85
#define  INSTOUT D3    // Connect to Serial Instruction Input (SII) Pin 6 (PB1) on ATTiny85
#define  DATAOUT D4     // Connect to Serial Data Input (SDI) Pin 5  (PB0) on ATTiny85
#define  VCC     D5     // Connect to !VCC through pnp transistor


#define SET(REG,X) (uint8_t)(REG | X)
#define UNSET(REG,X) (uint8_t)(REG & ~X)
#define ISSET(REG,X) (uint8_t)(REG & X)

char mylasterror[MAX_ERROR_LEN+1]="";

static const uint16_t vid = 0x1a86;
static const uint16_t pid = 0x5512;



const chipmem chipmemarr[] = {
  {64,32,32,4,0x1E9108, "ATTiny25"},
  {64,64,64,4,0x1E9206, "ATTiny45"},
  {128,64,128,4,0x1E930B, "ATTiny85"},
  {64,32,32,4,0x1E910B, "ATTiny24"},
  {64,64,64,4,0x1E9207, "ATTiny44"},
  {128,64,128,4,0x1E930C, "ATTiny84"}
  
};
#define NCHIPS 6
#define TIMEOUT_LOOPS 200

#define SEND(X,Y) if (sendHVSP (X,Y) == 1)\
    return 1;
#define SENDREAD(X,Y,Z) if (sendreadHVSP (X, Y, Z) == 1)\
    return 1;

void CH341i::close (){
  if (m_usb_context){
    if (m_udh){
      libusb_close (m_udh);
      m_udh = NULL;
    }
    libusb_exit (m_usb_context);
    m_usb_context = NULL;
  }
}


CH341i::~CH341i (){
  close ();
}

CH341i::CH341i ():
m_udh(NULL),
m_usb_context(NULL),
m_chiptype(-1)
{
  *mylasterror = '\0';
}

int CH341i::open (){ //Opens device and sets out mask to 0x3F. Returns null on error.
	int res;
	*mylasterror= '\0';
	m_usb_context = NULL;
	res = libusb_init (&m_usb_context);
	if (res < 0){
		sprintf (mylasterror, "Error %d getting usb context", res);
		return 1;
	}
	
	//libusb_set_debug(m_usb_context, 3); //deprecated
	 libusb_set_option (m_usb_context, LIBUSB_OPTION_LOG_LEVEL, 3);
	
	m_udh = libusb_open_device_with_vid_pid(m_usb_context, vid, pid);
	if (!m_udh){
		sprintf (mylasterror, "Error %d getting opening device", errno);
		libusb_exit (m_usb_context);
        m_usb_context = NULL;
		return 1;
	}
	
	if (libusb_kernel_driver_active(m_udh, 0) == 1) { // find out if kernel driver is attached
		//kernel driver is active
        if (libusb_detach_kernel_driver(m_udh, 0) != 0){ // detach it
            sprintf(mylasterror, "Kernel Driver Detach failed!");
			close ();
			return 1;
		}
    }
	
	res = libusb_claim_interface(m_udh, 0); // claim interface 0
	if (res < 0){
		sprintf (mylasterror, "Error %d claiming interface", res);
		close ();
		return 1;
	}
	//Configure device.
	
	unsigned char buf[DEV_CONTROL_BUF_LEN];
	libusb_control_transfer (m_udh, DEV_CONTROL_CFG, 82, 0, 0, buf, DEV_CONTROL_BUF_LEN, USB_TIMEOUT);
	libusb_control_transfer (m_udh, DEV_CONTROL_CFG, 95, 0, 0, buf, DEV_CONTROL_BUF_LEN, USB_TIMEOUT);
	
	
	
	return 0;
}





int CH341i::start (){
  unsigned char command[] = {CMD_UIO_STREAM, CMD_UIO_STM_DIR | 0x3F
		,CMD_UIO_STM_OUT | 0x21, DELAY_31US, CMD_UIO_STM_OUT | 0x01,
		DELAY_31US, CMD_UIO_STM_OUT | 0x00, DELAY_31US,CMD_UIO_STM_DIR | 0x3B,CMD_UIO_STM_END};
  int lcmd = sizeof (command);
  int lsnd;
  //chipOff ();
  int res = libusb_bulk_transfer (m_udh, ENDPOINT_OUT, command, lcmd, &lsnd, USB_TIMEOUT);
  if (res < 0){
    sprintf (mylasterror, "Error %d starting device connection", res);
    return 1;
  }
  m_outstate = 0x00;
  usleep (400);
  return 0;
}

int CH341i::dataReady (){
  unsigned char buf[MAX_BUFFER_LENGTH];
  unsigned char command[] = {CMD_UIO_STREAM, CMD_UIO_STM_IN, DELAY_10US,
	CMD_UIO_STM_IN, DELAY_10US,CMD_UIO_STM_IN, DELAY_10US,CMD_UIO_STM_IN, DELAY_10US,CMD_UIO_STM_IN, DELAY_10US, CMD_UIO_STM_END};
  int lcmd = sizeof (command);
  int lsnd;
  int res = libusb_bulk_transfer (m_udh, ENDPOINT_OUT, command, lcmd, &lsnd, USB_TIMEOUT);
  if (res < 0){
    sprintf (mylasterror, "Error %d writting to chip", res);
    return 1;
  }
  res = libusb_bulk_transfer (m_udh, ENDPOINT_IN, buf, MAX_BUFFER_LENGTH, &lsnd, USB_TIMEOUT);
  if (res < 0){
    sprintf (mylasterror, "Error %d reading from chip", res);
    return 1;
  }
  for (int i = 0; i < lsnd; i++){
	if (ISSET(buf[i],DATAIN))
	  return 1;
  }
  return 0;
}


int CH341i::addTic (uint8_t *buf, int pos){
  uint8_t tic[] ={CMD_UIO_STM_OUT | SET(m_outstate,CLKOUT), CMD_UIO_STM_OUT |UNSET(m_outstate,CLKOUT)};
  memcpy (buf + pos, tic, sizeof(tic));

  return sizeof(tic);
  //return 4;
}

int CH341i::sendreadHVSP (uint8_t val, uint8_t val1, uint8_t *ret){
  unsigned char command[MAX_BUFFER_LENGTH];
  unsigned char buffer[MAX_BUFFER_LENGTH];
  int lcmd;
  int lsnd;
  int cmdpos = 1;
  uint8_t inBits = 0;
  int res;
  int i;
  
  
  command[0] = CMD_UIO_STREAM;
  
  for (i = 0; i < 8; i++){
    command[cmdpos++] = CMD_UIO_STM_OUT | SET(m_outstate,CLKOUT);
    command[cmdpos++] = CMD_UIO_STM_IN;
    if (!!(val & (1 << (7 - i))) == 1)
      m_outstate = SET(m_outstate,DATAOUT);
    else
      m_outstate = UNSET(m_outstate,DATAOUT);
    
    if (!!(val1 & (1 << (7 - i))) == 1)
      m_outstate = SET(m_outstate,INSTOUT);
    else
      m_outstate = UNSET(m_outstate,INSTOUT);
    m_outstate = UNSET(m_outstate, CLKOUT);
    command[cmdpos++] = CMD_UIO_STM_OUT |m_outstate;
    
  }
  
  command[cmdpos++] = CMD_UIO_STM_OUT | SET(m_outstate,CLKOUT);
  m_outstate = UNSET(m_outstate,DATAOUT);
  m_outstate = UNSET(m_outstate,INSTOUT);
  m_outstate = UNSET(m_outstate, CLKOUT);
  command[cmdpos++] = CMD_UIO_STM_OUT |m_outstate;
  cmdpos += addTic(command, cmdpos);
  cmdpos += addTic(command, cmdpos);
  //command[cmdpos] = CMD_UIO_STM_END;
  lcmd = cmdpos;
  
  res = libusb_bulk_transfer (m_udh, ENDPOINT_OUT, command, lcmd, &lsnd, USB_TIMEOUT);
  if (res < 0){
    sprintf (mylasterror, "Error %d writting to chip", res);
    return 1;
  }
  res = libusb_bulk_transfer (m_udh, ENDPOINT_IN, buffer, MAX_BUFFER_LENGTH, &lsnd, USB_TIMEOUT);
  if (res < 0){
    sprintf (mylasterror, "Error %d reading from chip", res);
    return 1;
  }
  for (i = 0; i< 8; i++){
	inBits <<= 1;
	inBits |= (ISSET(buffer[i],DATAIN)) ==DATAIN ? 1:0;
  }
  *ret = inBits;
  return 0;
}



int CH341i::sendHVSP (uint8_t val, uint8_t val1){
  unsigned char command[MAX_BUFFER_LENGTH];
  int lcmd;
  int lsnd;
  int cmdpos = 1;
  int res;
  int i;
  
  
  command[0] = CMD_UIO_STREAM;
  cmdpos += addTic (command, cmdpos);
  
  
  command[0] = CMD_UIO_STREAM;
  cmdpos =1;
  cmdpos += addTic (command, cmdpos);
  for (i = 0; i < 8; i++){ //Byte second part
    if (!!(val & (1 << (7 - i))) == 1)
      m_outstate = SET(m_outstate,DATAOUT);
    else
      m_outstate = UNSET(m_outstate,DATAOUT);
    
    if (!!(val1 & (1 << (7 - i))) == 1)
      m_outstate = SET(m_outstate,INSTOUT);
    else
      m_outstate = UNSET(m_outstate,INSTOUT);
    
    command[cmdpos++] = CMD_UIO_STM_OUT |m_outstate;
    cmdpos += addTic (command, cmdpos);
  }
  
  m_outstate = UNSET(m_outstate,DATAOUT);
  m_outstate = UNSET(m_outstate,INSTOUT);
  command[cmdpos++] = CMD_UIO_STM_OUT |m_outstate;
  cmdpos += addTic(command, cmdpos);
  cmdpos += addTic(command, cmdpos);
  
  lcmd =cmdpos;
  res = libusb_bulk_transfer (m_udh, ENDPOINT_OUT, command, lcmd, &lsnd, USB_TIMEOUT);
  if (res < 0){
    sprintf (mylasterror, "Error %d writting to chip", res);
    return 1;
  }
  return 0;
}

int CH341i::chipErase (){
  printf ("Erasing chip\n");
  
  SEND(0x80, 0x4C);
  SEND(0, 0x64);
  SEND(0, 0x6C);
  
  for (int i = 0; i< TIMEOUT_LOOPS; i++){
    if(dataReady ())
      return finishOperation ();
	usleep (100);
  }
  strcpy (mylasterror, "Timeout waiting for ready signal from chip");
  return 1;
}

int CH341i::readSignature (unsigned *signature){
  unsigned sig = 0;
  uint8_t val;
  printf ("Reading Signature\n");
  SEND (0x08, 0x4C);
  for (int ii = 0; ii < 3; ii++) {
    SEND(ii, 0x0C);
    SEND(0x00, 0x68)
    SENDREAD(0x00, 0x6C, &val);
    sig = (sig << 8) | val;
  }
  for (int i = 0; i < NCHIPS; i++){
    if (chipmemarr[i].signature == sig){
      m_chiptype = i;
      *signature = sig;
      return 0;
    }
  }
  m_chiptype = -1;
  *signature = 0;
  return 0;
}

int CH341i::end (){
  uint8_t command[MAX_BUFFER_LENGTH];
  int lsnd;
  int res;
  command[0] = CMD_UIO_STREAM;
  
  m_outstate = UNSET(m_outstate, CLKOUT);
  m_outstate = SET(m_outstate, RST);
  command[1] = CMD_UIO_STM_OUT | m_outstate;
  command[2] = DELAY_31US;
  m_outstate = SET(m_outstate, VCC);
  command[3] = CMD_UIO_STM_OUT | m_outstate;
  command[4] = CMD_UIO_STM_END;
  res = libusb_bulk_transfer (m_udh, ENDPOINT_OUT, command, 5, &lsnd, USB_TIMEOUT);
  if (res < 0){
    sprintf (mylasterror, "Error %d ending communication", res);
    return 1;
  }
  return 0;
}

int CH341i::writeFlashPage (int pageno, uint8_t *pagedata, int length){
  //Attiny 24-25 64 pages of 16 words (2 bytes/word)
  //Attiny 44-45 64 pages of 32 words
  //Attiny 84-85 128 pages of 32 words
  int address = pageno * chipmemarr[m_chiptype].flashpagebytes/2;
  int address2 = address;
  int sended = 0;
  if (length > chipmemarr[m_chiptype].flashpagebytes){
    strcpy (mylasterror, "Flash data larger than chip specification.");
    return 1;
  }
  while (length > 1){
    if (pagedata[0] != 0xFF || pagedata[1] != 0xFF){
      SEND(address & 0xFF, 0x0C);
      SEND(*pagedata++, 0x2C);
      /*SEND (0x00, 0x6D);
       S *END (0x00, 0x6C);//*///Maybe needed for 24/44/84
       SEND(*pagedata++, 0x3C);
       SEND(0x00, 0x7D);
       SEND(0x00, 0x7C);
       sended++;
    }
    length -= 2;
    address++;
  }
  if (length == 1 && pagedata[0] != 0xFF){ //I think this shouldn't happen, but just in case.
    SEND(address & 0xFF, 0x0C);
    SEND(*pagedata, 0x2C);
    /*sendHVSP (0x00, 0x6D);
    sendHVSP (0x00, 0x6C);//*///Maybe needed for 24/44/84
    SEND(0x00, 0x7D);
    SEND(0x00, 0x7C);
    sended++;
  }
  if (sended == 0) //No data sended.
    return 0;
  SEND(address2>>8, 0x1C);
  SEND(0x00, 0x64);
  SEND(0x00, 0x6C);
  for (int i = 0; i< TIMEOUT_LOOPS; i++){
    if(dataReady ())
      return 0;
	usleep (100);
  }
  strcpy (mylasterror, "Timeout waiting for ready signal from chip");
  return 1;
}


int CH341i::startFlashWrite (){
  SEND(0x10, 0x4C);
  return 0;
}

int CH341i::startFlashRead (){
  SEND(0x02, 0x4C);
  return 0;
}

int CH341i::readFlash (int address, uint8_t *hibit, uint8_t *lobit){
  SEND(address & 0xFF, 0x0C);
  SEND (address>>8, 0x1C);
  SEND (0x00, 0x68);
  SENDREAD (0x00, 0x6C, hibit);
  SEND (0x00, 0x78);
  SENDREAD (0x00, 0x7C, lobit);
  return 0;
}

int CH341i::startEEPROMWrite (){
  SEND(0x11, 0x4C);
  return 0;
}

int CH341i::writeEEPROMPage (int pageno, uint8_t *pagedata, int length){
  //Attiny 24-25 64 pages of 16 words (2 bytes/word)
  //Attiny 44-45 64 pages of 32 words
  //Attiny 84-85 128 pages of 32 words
  int address = pageno * chipmemarr[m_chiptype].eeprompagebytes;
  int sended = 0;
  if (length > chipmemarr[m_chiptype].eeprompagebytes){
    strcpy (mylasterror, "EEPROM data larger than chip specification.");
    return 1;
  }
  while (length > 0){
    if (pagedata[0] != 0xFF){
      SEND(address & 0xFF, 0x0C);
      
      SEND(address>>8, 0x1C);
      SEND(*pagedata++, 0x2C);
      SEND(0x00, 0x6D);
      SEND(0x00, 0x6C);
      sended++;
    }
    length--;
    address++;
  }
  if (sended == 0) //No data sended.
    return 0;
  SEND(0x00, 0x64);
  SEND(0x00, 0x6C);
  for (int i = 0; i< TIMEOUT_LOOPS; i++){
    if(dataReady ())
      return 0;
	usleep (100);
  }
  //Something goes wrong.
  strcpy (mylasterror, "Timeout waiting for ready signal from chip");
  return 1;
}

int CH341i::startEEPROMRead (){
  SEND(0x03, 0x4C);
  return 0;
}

int CH341i::readEEPROM (int address, uint8_t *value){
  SEND (address &0xFF, 0x0C);
  SEND (address >> 8, 0x1C);
  SEND (0x00, 0x68);
  SENDREAD (0x00, 0x6C, value);
  return 0;
}

int CH341i::finishOperation (){
  SEND(0x00, 0x4C);
  return 0;
}

int CH341i::writeFuse (uint8_t fuse, uint8_t value){
    int i;
    
	SEND(0x40, 0x4C);
	SEND(value, 0x2C);
	switch (fuse){
		case HF:
			SEND(0x00, 0x74);
			SEND(0x00, 0x7C);
			break;
		case LF:
			SEND(0x00, 0x64);
			SEND(0x00, 0x6C);
			break;
		case EF:
			SEND(0x00, 0x66);
			SEND(0x00, 0x6E);
			break;
		default:
			finishOperation ();
            strcpy (mylasterror, "Error: unknown fuse");
			return 1; //Shouldn't happen
	};


  for (i = 0; i< TIMEOUT_LOOPS; i++){
    if(dataReady ())
      return 0;
	usleep (100);
  }
  strcpy (mylasterror, "Timeout waiting for ready signal from chip");
  return 1;

}

int CH341i::readFuse (uint8_t fuse, uint8_t *value){
    SEND(0x04, 0x4C);
	switch (fuse){
		case HF:
			SEND(0x00, 0x7A);
			SENDREAD(0x00, 0x7E, value);
			break;
		case LF:
			SEND(0x00, 0x68);
			SENDREAD(0x00, 0x6C, value);
			break;
		case EF:
			SEND(0x00, 0x6A);
			SENDREAD(0x00, 0x6E,value);
			break;
		default:
			finishOperation ();
			return 0;
	}
	return 0;
}

int CH341i::readCalib (uint8_t *calib){
  SEND(0x08,0x4C);
  SEND(0x00,0x0C);
  SEND(0x00,0x78);
  SENDREAD(0x00,0x7C, calib);
  return 0;
}

int CH341i::readLock (uint8_t *lockbits){
  SEND(0x04, 0x4C);
  SEND(0x00, 0x78);
  SENDREAD(0x00, 0x7C, lockbits);
  return 0;
}

int CH341i::writeLock (uint8_t lockbits){
  int i;
  SEND(0x02, 0x4C);
  SEND(lockbits & 0x03, 0x2C);
  SEND(0x00, 0x64);
  SEND(0x00, 0x6C);
  
  for (i = 0; i< TIMEOUT_LOOPS; i++){
    if(dataReady ())
      return 0;
    usleep (100);
  }
  strcpy (mylasterror, "Timeout waiting for ready signal from chip");
  return 1;
}

const char *CH341i::getChipName (){
  if (m_chiptype == -1)
    return NULL;
  return chipmemarr[m_chiptype].name;
}

const chipmem *CH341i::getChip (){
  if (m_chiptype == -1)
    return NULL;
  return (chipmemarr+m_chiptype);
}

const char *CH341i::getError (){
  return mylasterror;
}

void CH341i::test (){
 uint8_t cmd[18];
 int res, lsnd;
 uint8_t rec[MAX_BUFFER_LENGTH];
 cmd[0] = CMD_UIO_STREAM;
 addTic (cmd, 1);
 cmd[5] = CMD_UIO_STM_IN;
 addTic (cmd, 6);
 cmd[10] = CMD_UIO_STM_END;
 res = libusb_bulk_transfer (m_udh, ENDPOINT_OUT, cmd, 11, &lsnd, USB_TIMEOUT);
 res = libusb_bulk_transfer (m_udh, ENDPOINT_IN, rec, MAX_BUFFER_LENGTH, &lsnd, USB_TIMEOUT);
 printf ("%d, %d\n", res, lsnd);
}


