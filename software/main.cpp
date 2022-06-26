#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ch341i.h"
#include <charconv>
#include <iostream>
#include <fstream>
#include "intelhex/intelhexclass.h"
#define SEPARATOR ':'

static const chipmem *chip;

int fusesOperation (CH341i *h, const char *memory, const char *operation, const char *file, const char *format){
  CH341i::fusetype fuse;
  uint8_t value;
  if (!strcmp (memory, "hfuse"))
    fuse = h->HF;
  else if (!strcmp (memory, "lfuse"))
    fuse = h->LF;
  else if (!strcmp (memory, "efuse"))
    fuse = h->EF;
  else{
    fprintf (stderr, "ERROR: unknow fuse %s.\n", memory);
    return 1;
  }
  
  if (*operation == 'r'){
    printf ("Reading fuse %s....\n", memory);
    if (h->readFuse (fuse, &value) == 1){
      fprintf (stderr, "ERROR: error %s reading fuse %s.\n", h->getError (), memory);
      return 1;
    }
    if (*file == '-'){
      printf ("%s value is 0x%X.\n", memory, value);
    }
    else {
      fprintf (stderr, "ERROR: Write to file for fuse is not supported.\n");
      printf ("%s value is 0x%X.\n", memory, value);
    }
  }
  else if (*operation == 'w'){
    if (*format == 'm'){
      
      std::from_chars_result res;
      int valint;
      if (file[0] == '0'){
        if (file[1] == 'x' || file[1] == 'X'){
          file += 2;
          res = std::from_chars (file, file + strlen(file), valint, 16);
          file -=2;
        }
        else if (file[1] == 'b' || file[1] == 'B'){
          file += 2;
          res = std::from_chars (file, file + strlen(file), valint, 2);
          file -=2;
        }
        else
          res = std::from_chars (file, file + strlen(file), valint);
      }
      else
        res = std::from_chars (file, file + strlen(file), valint);
      
      
      if (res.ec == std::errc())
      {
        if (valint < 0 || valint > 255 ){
          fprintf (stderr, "ERROR: %s value must be between 0 and 255.\n", file);
          return 1;
        }
        value = valint;
      }
      else if (res.ec == std::errc::invalid_argument || (res.ptr - file == 2))
      {
        fprintf (stderr, "ERROR: invalid value %s for %s!\n", file, memory);
        return 1;
      }
      else if (res.ec == std::errc::result_out_of_range)
      {
        fprintf (stderr, "ERROR: number conversion out of range for %s! value distance: %d\n" 
         ,memory, (int) (res.ptr - file));
        return 1;
      }
      printf ("Writing fuse %s...\n", memory);
      if (h->writeFuse (fuse, value) == 1){
        fprintf (stderr, "ERROR: error %s writing fuse %s to value 0x%X.\n", h->getError (), memory, value);
        return 1;
      }
      printf ("Written. Verifying...\n");
      return fusesOperation (h, memory, "r", "-", "m");
    }
    else{
      fprintf (stderr, "ERROR: only immediate (m) format is supported for fuse writing.\n");
      return 1;
    }
  }
  else {
    fprintf (stderr, "ERROR: invalid operation %s for %s.\n", operation, memory);
    return 1;
  }
  return 0;
}
int eepromOperation (CH341i *h, const char *memory, const char *operation, const char *file, const char *format){
  
  if (*format != 'i'){
    fprintf (stderr, "ERROR: only intel hex format (i) is allowed in EEPROM operation.\n");
    return 1;
  }
  
  if (*operation == 'w'){
    if (*file == '-'){
      fprintf (stderr, "ERROR: only file input is allowed in EEPROM writing.\n");
      return 1;
    }
    printf ("Writing EEPROM.....\n");
    std::ifstream infile;
    unsigned long size, pos = 0, pct;
    intelhex inhex;
    uint8_t data[chip->eeprompagebytes], page;
    unsigned long startad, ad, endad;
    int c;
    infile.open (file);
    if (!infile.is_open ()){
      fprintf (stderr, "ERROR: can't find file %s.\n", file);
      return 1;
    }
    infile >> inhex;
    size = inhex.size ();
    if (size <= 0){
      fprintf (stderr, "ERROR: %s is empty or bad format.\n", file);
    }
    if (!inhex.startAddress (&startad)){
      fprintf (stderr, "ERROR: %s is empty or bad format.\n", file);
      infile.close ();
      return 1;
    }
    if (!inhex.endAddress (&endad)){
      fprintf (stderr, "ERROR: %s is empty or bad format.\n", file);
      infile.close ();
      return 1;
    }
    if (h->startEEPROMWrite () == 1){
      fprintf (stderr, "ERROR: %s when writing EEPROM,\n", h->getError ());
      infile.close ();
      return 1;
    }
    page = startad/chip->eeprompagebytes;
    
    ad = startad;
    while (ad<=endad){
      for (c = 0; c < chip->eeprompagebytes && ad<=endad; c++){
        if (!inhex.getData (data + c, ad)){
          data[c] = 0xFF;
        }
        else
          pos++;
        
        ad++;
      }
      if (h->writeEEPROMPage (page, data, c) == 1){
        fprintf (stderr, "ERROR: %s when writing EEPROM.\n", h->getError ());
        infile.close ();
        return 1;
      }
      //pos += c;
      pct = (pos*100)/size;
      //printf ("%ld%% written\n", pct);
      std::cout << "\r" << pct << "% written" << std::flush;
      page++;
    }
    std::cout << "\r" << "100% written\n";
    if (h->finishOperation () == 1){
      fprintf (stderr, "ERROR: %s when ending writing EEPROM.\n", h->getError ());
      infile.close ();
      return 1;
    }
    infile.close ();
  }
  else if (*operation == 'r'){
    printf ("Reading EEPROM...\n");
    intelhex inhex;
    uint8_t data;
    int size = chip->eeprompagebytes * chip->eeprompages;
    int pct;
    if (h->startEEPROMRead () == 1){
      fprintf (stderr, "ERROR: %s when reading EEPROM.\n", h->getError ());
      return 1;
    }
    for (int i = 0; i < size; i++){
      if (h->readEEPROM (i, &data) == 1){
        fprintf (stderr, "ERROR: %s when reading EEPROM.\n", h->getError ());
        return 1;
      }
      inhex.insertData (data);
      pct = (100*i)/size;
      std::cout << "\r" << pct << "% read" << std::flush;
    }
    std::cout << "\r" << "100% read\n";
    h->finishOperation ();
    if (*file != '-'){
      std::ofstream outfile;
      outfile.open (file);
      if (!outfile.is_open ()){
        fprintf (stderr, "ERROR: can't create file %s.\n", file);
        return 1;
      }
      outfile << inhex;
      outfile.close ();
    }
    else
      std::cout << inhex;
  }
  else {
    fprintf (stderr, "ERROR: invalid operation %s for %s.\n", operation, memory);
    return 1;
  }

  return 0;
}

int flashOperation (CH341i *h, const char *memory, const char *operation, const char *file, const char *format){
  if (*format != 'i'){
    fprintf (stderr, "ERROR: only intel hex format (i) is allowed in Flash operation.\n");
    return 1;
  }
  
  if (*operation == 'w'){
    if (*file == '-'){
      fprintf (stderr, "ERROR: only file input is allowed in EEPROM writing.\n");
      return 1;
    }
    unsigned long size, pos = 0, pct;
    unsigned long startad, ad, endad;
    std::ifstream infile;
    intelhex inhex;
    uint8_t data[chip->flashpagebytes], page;
    int c;
    printf ("Writing Flash...\n");
    infile.open (file);
    if (!infile.is_open ()){
      fprintf (stderr, "ERROR: can't find file %s.\n", file);
      return 1;
    }
    infile >> inhex;
    size = inhex.size ();
    if (size <= 0){
      fprintf (stderr, "ERROR: %s is empty or bad format.\n", file);
      infile.close ();
      return 1;
    }
    if (!inhex.startAddress (&startad)){
      fprintf (stderr, "ERROR: %s is empty or bad format.\n", file);
      infile.close ();
      return 1;
    }
    if (!inhex.endAddress (&endad)){
      fprintf (stderr, "ERROR: %s is empty or bad format.\n", file);
      infile.close ();
      return 1;
    }
    if (h->startFlashWrite () == 1){
      fprintf (stderr, "ERROR: %s when writing Flash.\n", h->getError ());
      infile.close ();
      return 1;
    }
    //printf ("Writing from %d to %d
    page = startad/chip->flashpagebytes;

    ad =startad;
    while (ad <= endad){
      for (c = 0; c < chip->flashpagebytes && ad <= endad; c++){
        if (!inhex.getData (data+c, ad)){
          data[c] = 0xFF;
        }
        else{
          pos++;
        }
        ad++;
      }
      if (h->writeFlashPage (page, data, c) == 1){
        fprintf (stderr, "ERROR: %s when writing Flash,\n", h->getError ());
        infile.close ();
        return 1;
      }
      pct = (pos*100)/size;
      //printf ("%ld%% written\n", pct);
      std::cout << "\r" << pct << "% written" << std::flush;
      page++;
    }
    std::cout << "\r" << "100% written\n";
    
    if (h->finishOperation () == 1){
      fprintf (stderr, "ERROR: %s when ending writing Flash.\n", h->getError ());
      infile.close ();
      return 1;
    }
    infile.close ();
  }
  else if (*operation == 'r'){
    intelhex inhex;
    uint8_t hibit, lobit;
    int size = chip->flashpagebytes * chip->flashpages/2;
    int pct;
    printf ("Reading Flash...\n");
    if (h->startFlashRead () == 1){
      fprintf (stderr, "ERROR: %s when reading Flash.\n", h->getError ());
      return 1;
    }
    for (int i = 0; i < size; i++){
      if (h->readFlash (i, &hibit, &lobit) == 1){
        fprintf (stderr, "ERROR: %s when reading Flash.\n", h->getError ());
        return 1;
      }
      inhex.insertData (hibit);
      inhex.insertData (lobit);
      pct = (100*i)/size;
      std::cout << "\r" << pct << "% read" << std::flush;
    }
    std::cout << "\r" << "100% read\n";
    h->finishOperation ();
    if (*file != '-'){
      std::ofstream outfile;
      outfile.open (file);
      if (!outfile.is_open ()){
        fprintf (stderr, "ERROR: can't create file %s.\n", file);
        return 1;
      }
      outfile << inhex;
      outfile.close ();
    }
    else
      std::cout << inhex;
  }
  else {
    fprintf (stderr, "ERROR: invalid operation %s for %s.\n", operation, memory);
    return 1;
  }

  return 0;
}

int calibrationOperation (CH341i *h, const char *memory, const char *operation, const char *file, const char *format){
  uint8_t calib;
  if (*operation != 'r'){
    fprintf (stderr, "ERROR: only read is allowed for calibration.\n");
    return 1;
  }
  
  if (*file != '-'){
    fprintf (stderr, "ERROR: write to file for calibration is not supported.\n");
    return 1;
  }
  printf ("Reading calibration...\n");
  if (h->readCalib (&calib) == 1){
    fprintf (stderr, "ERROR: %s while reading calibration.\n", h->getError ());
    return 1;
  }
  
  printf ("Calibration value: %X.\n", calib);
  return 0;
}

int lockOperation (CH341i *h, const char *memory, const char *operation, const char *file, const char *format){
  uint8_t value;
  if (*operation == 'r'){
    if (*file != '-'){
      fprintf (stderr, "ERROR: write lock bits to file is not allowed.\n");
      return 1;
    }
    printf ("Reading lock bits...\n");
    if (h->readLock (&value) == 1){
      fprintf (stderr, "ERROR: %s when reading lock bits.\n", h->getError ());
      return 1;
    }
    printf ("Locks bits: %X.\n", value);
  }
  else if (*operation == 'w'){
    if (*format == 'm'){
      std::from_chars_result res;
      int valint;
      if (file[0] == '0'){
        if (file[1] == 'x' || file[1] == 'X'){
          file += 2;
          res = std::from_chars (file, file + strlen(file), valint, 16);
          file -=2;
        }
        else if (file[1] == 'b' || file[1] == 'B'){
          file += 2;
          res = std::from_chars (file, file + strlen(file), valint, 2);
          file -=2;
        }
        else
          res = std::from_chars (file, file + strlen(file), valint);
      }
      else
        res = std::from_chars (file, file + strlen(file), valint);
      
      if (res.ec == std::errc())
      {
        if (valint < 0 || valint > 255 ){
          fprintf (stderr, "ERROR: %s value must be between 0 and 255.\n", file);
          return 1;
        }
        value = valint;
      }
      else if (res.ec == std::errc::invalid_argument || (res.ptr - file == 2))
      {
        fprintf (stderr, "ERROR: invalid value %s for lock bits!\n", file);
        return 1;
      }
      else if (res.ec == std::errc::result_out_of_range)
      {
        fprintf (stderr, "ERROR: number conversion out of range for lock bits! value distance: %d\n" 
         , (int) (res.ptr - file));
        return 1;
      }
      printf ("Writing lock bits...\n");
      if (h->writeLock (value) == 1){
        fprintf (stderr, "ERROR: %s when writing lock bits.\n", h->getError ());
        return 1;
      }
    }
    else {
      fprintf (stderr, "ERROR: only immediate (m) format is supported for lock bits writing.\n");
      return 1;
    }
  }
  else {
    fprintf (stderr, "ERROR: Unknow operation on lock bits.\n");
    return 1;
  }
  return 0;
}


int main (int argc, char **argv){
  int op;
  char *command = NULL;
  char *memory, *operation, *file, *format, *ptr;
  int erase = 0;
  int commands = 0;
  CH341i h;

  unsigned signature;
  int ret = 0;
  while ((op = getopt (argc, argv, "U:e")) != -1){
    switch (op){
      case 'U':
        if (commands == 1){
          fprintf (stderr, "ERROR: only one update command is allowed.\n");
          return 1;
        }
        command = optarg;
        commands = 1;
        break;
      case 'e':
        erase = 1;
        break;
      default:
        fprintf (stderr, "ERROR: Unkow command line parameter\n");
        return 1;
    }
  }
  if (h.open () == 1){
    fprintf (stderr, "ERROR: %s.\n", h.getError ());
    return 1;
  }
  if (h.start ()){
    fprintf (stderr, "ERROR: %s initializing chip.\n", h.getError ());
    h.close ();
    return 1;
  }
  
  if (h.readSignature (&signature)){
    fprintf (stderr, "ERROR: %s reading chip signature\n", h.getError ());
    return 1;
  }
  
  chip = h.getChip ();
  printf ("Detected %s (signature: 0x%X)\n", chip->name, chip->signature);
  
  if (erase == 1){
    printf ("Erasing chip....\n");
    if (h.chipErase () == 1){
      fprintf (stderr, "ERROR: %s erasing chip.\n", h.getError ());
      h.end ();
      h.close ();
      return 1;
    }
    printf ("Chip erased.\n");
    if (command == NULL){
      h.end ();
      h.close ();
      return 0;
    }
  }
  ptr = strchr (command, SEPARATOR);
  if (!ptr){
    fprintf (stderr, "ERROR: Invalid update specification\nError parsing update operation %s\n", command);
    return 1;
  }
  *ptr = '\0';
  memory = command;
  command = ptr+1;
  ptr = strchr (command, SEPARATOR);
  if (!ptr){
    fprintf (stderr, "ERROR: Invalid update specification\nError parsing update operation %s\n", command);
    return 1;
  }
  *ptr = '\0';
  operation = command;
  command = ptr + 1;
  ptr = strchr (command, SEPARATOR);
  if (!ptr){
    fprintf (stderr, "ERROR: Invalid update specification\nError parsing update operation %s\n", command);
    return 1;
  }
  *ptr = '\0';
  file = command;
  command = ptr + 1;
  
  if (strlen(command) > 0)
    format = command;
  else {
    fprintf (stderr, "ERROR: Invalid update specification\nError parsing update operation %s\n", command);
    return 1;
  }
  
  if (strstr (memory, "fuse"))
    ret = fusesOperation (&h, memory, operation, file, format);
  else if (!strcmp (memory, "eeprom"))
    ret = eepromOperation (&h, memory, operation, file, format);
  else if (!strcmp (memory, "flash"))
    ret = flashOperation (&h, memory, operation, file, format);
  else if (!strcmp (memory, "calibration"))
    ret = calibrationOperation (&h, memory, operation, file, format);
  else if (!strcmp (memory, "lock"))
    ret = lockOperation (&h, memory, operation, file, format);
  else if (!strcmp (memory, "signature"))
    printf ("Chip signature: 0x%X\n", signature);
  else {
    fprintf (stderr, "ERROR: Unknow memory type %s.\n", memory);
    ret = 1;
  }
  h.end ();
  h.close ();
  return ret;
}
