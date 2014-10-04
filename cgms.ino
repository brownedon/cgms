/* Arduino interface to CGMS
  by: Don Browne
  date: October 4, 2014
  license: Beerware. Please use, reuse, and modify this code. If
  you find it useful and we meet someday, buy me a beer. (Lifted from Jim Lindblom @ Sparkfun)
  Consider this educational software.  I make no claims as to it's accuracy, or even it's functionality.
  //
  I have stripped this down to the basics.  Works on a teensy 3.1, you'll need to make some mods to compile
  on a Mega ADK.  This should retrieve your current glucose every 5 minutes.  It also populates an array with
  your most recent values.  Limited attempt at power saving / sleep mode.  
  //
*/
#include <cdcacm.h>
#include <spi4teensy3.h>
#include <usbhub.h>
#include <stdio.h>      /* printf */
#include <time.h>   
#include <LowPower_Teensy3.h>

TEENSY3_LP LP = TEENSY3_LP();

struct readings{
  long minutes;
  int glucose;
};

struct readings readings_arr[12];
struct readings reading;

static const int MGDL =1;
static const int MMOL =2;
int units=MGDL;

static const int recordHeader = 32;
static const int glucoseRecord = 13;

int commError=0;
int lastRecord=0;
long offset=0;
int counter=0;
int gotValue=0;
int usbFailCount=0;
int alertcount=0;
int firstime=0;
long currentGMTtime=0;
long sleepPeriod =0;
long systime=0;
long lastGlucoseTime=0;

unsigned char year;
unsigned char month;// = 12;
unsigned char date;// = 14;
unsigned char day;// = WEDNESDAY;
unsigned char hours ;//= 4;
unsigned char minutes ;//= 9;
unsigned char seconds ;//= 0;

//crc table for cgms
//
uint16_t m_crc16Table[]PROGMEM ={ 
  0, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 
  0x1231, 0x210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 
  0x2462, 0x3443, 0x420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d, 
  0x3653, 0x2672, 0x1611, 0x630, 0x76d7, 0x66f6, 0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 
  0x48c4, 0x58e5, 0x6886, 0x78a7, 0x840, 0x1861, 0x2802, 0x3823, 0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b, 
  0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0xa50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a, 
  0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0xc60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49, 
  0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0xe70, 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78, 
  0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0xa1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067, 
  0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e, 0x2b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256, 
  0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3, 0x14a0, 0x481, 0x7466, 0x6447, 0x5424, 0x4405, 
  0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2, 0x691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634, 
  0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x8e1, 0x3882, 0x28a3, 
  0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 0x4a75, 0x5a54, 0x6a37, 0x7a16, 0xaf1, 0x1ad0, 0x2ab3, 0x3a92, 
  0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0xcc1, 
  0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0xed1, 0x1ef0
};

uint8_t CGMS_START[]        = { 
  0x01, 0x06, 0x00, 0x0A, 0x5E, 0x65 };

uint8_t CGMS_PAGE_COUNT[]   = { 
  0x01, 0x07, 0x00, 0x10, 0x04, 0x8B, 0xB8};


// Please set to 1 for maximum debug messages over serial console:
#define DEBUG 1
#define NORMAL_SPD 8
#define SLOW_SPD 2

#if DEBUG
#define DPRINT       Serial.print
#define DPRINTLN     Serial.println
#define DPRINTHEX(x) PrintHex(x, 0x80)
#else
#define DPRINT       if(0) Serial.print
#define DPRINTLN     if(0) Serial.println
#define DPRINTHEX(x) if(0) PrintHex(x, 0x80)
#endif

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

// Regular messages over serial console
#define PRINT       Serial.print
#define PRINTLN     Serial.println
#define PRINTHEX(x) PrintHex(x, 0x80)


// USB ACM initializer

class ACMAsyncOper : 
public CDCAsyncOper
{
public:
  virtual uint8_t OnInit(ACM *pacm);
};

uint8_t ACMAsyncOper::OnInit(ACM *pacm)
{
  uint8_t rcode = 0;

  // Set DTR=1 and RTS=1:
  rcode = pacm->SetControlLineState(3);

  if (rcode) {
    ErrorMessage<uint8_t>(PSTR("SetControlLineState"), rcode);
    return rcode;
  }

  LINE_CODING lc;
  lc.dwDTERate = 115200;	
  lc.bCharFormat = 0;
  lc.bParityType = 0;
  lc.bDataBits = 8;	

  rcode = pacm->SetLineCoding(&lc);

  if (rcode)
    ErrorMessage<uint8_t>(PSTR("SetLineCoding"), rcode);

  return rcode;
}

///////////////////////////////////////////////////////////////////////////////
// USB objects
USB Usb;
USBHub Hub1(&Usb);
ACMAsyncOper AsyncOper;
ACM Acm(&Usb, &AsyncOper);
ACM Acm1(&Usb, &AsyncOper);


void setup()
{
  Serial.begin(9600);
  Serial.println("Setup...");
  initReadings();
  LP.CPU(NORMAL_SPD);

  Serial.println("Setup...");
  if (Usb.Init() == -1)
    Serial.println("OSCOKIRQ failed to assert");

  Serial.flush();  // Get rid of anything else in the serial buffer.
  delay(1000);
  alertcount=0;
}


//////////////////////////////////////////////////////////////////////////////
void print_frame(char* prefix,
uint16_t len, uint8_t* buf,
char* postfix)
{
  PRINT(prefix);
  for(uint16_t i=0; i<len; i++ ) {
    PRINTHEX(buf[i]);
    PRINT(" ");
  }
  PRINT(" #");
  PRINT(len);
  PRINT(postfix);
}

boolean send_frame(int len, uint8_t* frame)
{
  uint8_t rcode;

  if (DEBUG) {
    print_frame("-> ", len, frame, "\r\n");
  }

  rcode = Acm.SndData(len, frame);
  if (rcode) {
    ErrorMessage<uint8_t>(PSTR("SndData"), rcode);
    return false;
  }
  else {
    return true;
  }
}

boolean get_frame(uint8_t* buf, uint16_t *len)
{
  uint8_t rcode;
  rcode = Acm.RcvData(len, buf);
  if (rcode && rcode != hrNAK) {
    ErrorMessage<uint8_t>(PSTR("Ret"), rcode);
    return false;
  }

  if(DEBUG) {
    if (*len) //more than zero bytes received
      print_frame("<- ", *len, buf, "\r\n");
    else
      PRINTLN("<no data>");
  }

  return true;
}

void initReadings(){
  reading.minutes=0;
  reading.glucose=0;
  for (int i=0; i>12; i++ ) {  
    readings_arr[i].minutes=reading.minutes;
    readings_arr[i].glucose=reading.glucose;
  }
}

void loop()
{
  //loops multiple times
  //before ACM is ready

  if (gotValue ==0){
    Usb.Task();
    if ( Acm.isReady()){
      usbFailCount=0;
      Serial.println("Acm Ready");
      gotValue=1;
      getGlucose();
      //you have glucose in the array here is where you should do something with it
    }
    else{
      usbFailCount++;
    }
  }
  if(gotValue==1 || commError==1){

    gotValue=0;

    delay(5000);
    LP.CPU(SLOW_SPD);   
    //
    Serial.print("Sleep:");
    Serial.println(sleepPeriod);
    delay(sleepPeriod);
    //
    LP.CPU(NORMAL_SPD);
  }
  else{
    delay(100);
  }
  //this allows you to hotplug the cgms
  if (Usb.Init() == -1){
    Serial.println("OSCOKIRQ failed to assert");
    usbFailCount++;
  }
  delay(1000);

}

void printTime(long c_time){
  time_t current_time;
  char* c_time_string;

  current_time=c_time;
  c_time_string = ctime(&current_time);

  Serial.print("Time:");
  Serial.println(c_time_string);
}

void addReading(long seconds, int glucose){
  //move everything in the array over 1 place
  //then add the new values
  for (int i=12; i>0; i-- ) {
    readings_arr[i].minutes=readings_arr[i-1].minutes;
    readings_arr[i].glucose=readings_arr[i-1].glucose;
  }

  readings_arr[0].minutes=seconds/60;
  readings_arr[0].glucose=glucose;

}

int getGlucoseRecordCount(uint16_t* page){
  return page[8];
}

int getMinutes(char* time_string){
  char *minu = (char*) malloc(2);
  int p_minutes=0;
  char msd, lsd;

  Serial.println("Parse Minute");

  minu=strndup( time_string+14, 2);
  msd=minu[0];
  lsd=minu[1];
  p_minutes= ((msd - 0x30) * 10) + (lsd - 0x30);

  free(minu);
  return p_minutes;
}

int getSeconds(char* time_string){
  char *sec = (char*) malloc(2);
  int p_seconds=0;
  char msd, lsd;

  Serial.println("Parse Seconds");

  sec=strndup( time_string+17, 2);

  msd = sec[0];
  lsd = sec[1];
  p_seconds = ((msd - 0x30) * 10) + (lsd - 0x30);
  free(sec);

  return p_seconds;
}

void parseGlucose(uint16_t* page, int record){
  int rc=getGlucoseRecordCount(page)-record;
  if (rc>0){
    int startPos = recordHeader + (( glucoseRecord) * (rc - 1)) ;
    int glucose = page[startPos + 8];
    int highFlag = page[startPos + 9];

    if (highFlag > 1){
      highFlag = 0;
    }
    // if you are over 255,  sets a bit to 1 otherwise it's always 0
    glucose = glucose + (highFlag * 255);

    Serial.println(glucose);
    Serial.print("Record Count:");
    Serial.println(rc);
    //
    //
    long lastGlucoseReading = ( ((unsigned long)page[startPos + 7] << 24) 
      + ((unsigned long)page[startPos + 6] << 16) 
      + ((unsigned long)page[startPos + 5] << 8) 
      + ((unsigned long)page[startPos + 4] ) ) ;

    currentGMTtime= ( ((unsigned long)page[startPos + 3] << 24) 
      + ((unsigned long)page[startPos + 2] << 16) 
      + ((unsigned long)page[startPos + 1] << 8) 
      + ((unsigned long)page[startPos + 0] ) ) ;

    Serial.print("Time:");
    Serial.println(lastGlucoseReading);
    Serial.println(currentGMTtime);

    Serial.println(lastRecord);
    Serial.println(rc);

    if(record==0){
      Serial.print("systime:");
      Serial.println(systime+offset);
      Serial.print("lr:");
      Serial.println(lastGlucoseReading);
      printTime(systime+offset);
      printTime(lastGlucoseReading);

      Serial.println(  ( ((systime+offset)-(lastGlucoseReading))) *1000);

      sleepPeriod= 300000- (( ((systime+offset)-(lastGlucoseReading))) *1000);
      Serial.println(sleepPeriod);
      if(sleepPeriod>300000){
        sleepPeriod=300000;
      }
      if (sleepPeriod<0){
        sleepPeriod=60000;
      }

    }

    if ((lastRecord !=rc) && glucose>0 ){

      addReading(lastGlucoseReading,glucose);
    }
    lastRecord=rc;
  }
}

void getGlucose(){
  Serial.println("Get Glucose Start");

  uint8_t  buf[256];
  uint16_t rcvd = 256;
  uint16_t page[1024];

  Serial.println("Accessing CGSMS on...");
  if (!send_frame(sizeof(CGMS_START), CGMS_START)) {
    return;
  }   

  rcvd = 256;
  if (!get_frame(buf, &rcvd)) {
    return;
  }

  PRINTLN("CGMS Ready.");

  //
  //                               1     cnt   0    cmd    crc   crc
  //uint8_t CGMS_REQUEST_TIME[] = {0x01, 0x06, 0x00, 0x22, 0x34, 0xC0 };
  // get units and time
  //template for page request
  //time offset
  Serial.println("OFFSET");
  //01 06 00 1D 88 07 
  uint16_t  CGMS_DATE[]={ 
    0x01, 0x06, 0x00, 0x1D, 0x00, 0x00                    };

  uint16_t CRC1=CalculateCrc16(CGMS_DATE,4);

  uint8_t CGMS_DATE_8[] = { 
    CGMS_DATE[0],
    CGMS_DATE[1],
    CGMS_DATE[2],
    CGMS_DATE[3],
    CGMS_DATE[4],
    CGMS_DATE[5]                    };

  CGMS_DATE_8[4]=lowByte(CRC1);
  CGMS_DATE_8[5]=highByte(CRC1);

  if (!send_frame(sizeof(CGMS_DATE_8), CGMS_DATE_8)) {
    error("ERROR REQ");
    return;
  }
  //
  rcvd = 256;  
  if (!get_frame(buf, &rcvd)) {
    error("ERROR RESP");
    return;
  }
  for (int i = 0; i < 256; i++){
    page[i]=buf[i];
  }

  offset = ( ((unsigned long)buf[7] << 24) 
    + ((unsigned long)buf[6] << 16) 
    + ((unsigned long)buf[5] << 8) 
    + ((unsigned long)buf[4] ) ) ;
  Serial.println(offset);
  //
  //
  //
  Serial.println("System Time");
  // 01 06 00 22 34 C0
  uint16_t  CGMS_DATE1[]={ 
    0x01, 0x06, 0x00, 0x22, 0x00, 0x00                    };
  //0x01, 0x06, 0x00, 0x22, 0x34, 0xC0  
  CRC1=CalculateCrc16(CGMS_DATE1,4);

  uint8_t CGMS_DATE_81[] = { 
    CGMS_DATE1[0],
    CGMS_DATE1[1],
    CGMS_DATE1[2],
    CGMS_DATE1[3],
    CGMS_DATE1[4],
    CGMS_DATE1[5]                    };

  CGMS_DATE_81[4]=lowByte(CRC1);
  CGMS_DATE_81[5]=highByte(CRC1);

  if (!send_frame(sizeof(CGMS_DATE_81), CGMS_DATE_81)) {
    error("ERROR REQ");
    return;
  }
  //
  rcvd = 256;  
  if (!get_frame(buf, &rcvd)) {
    error("ERROR RESP");
    return;
  }
  for (int i = 0; i < 256; i++){
    page[i]=buf[i];
  }

  systime = ( ((unsigned long)buf[7] << 24) 
    + ((unsigned long)buf[6] << 16) 
    + ((unsigned long)buf[5] << 8) 
    + ((unsigned long)buf[4] ) ) ;

  Serial.println("System Time offset");
  Serial.println(systime);

  Serial.println("System Time offset");
  // 01 06 00 22 34 C0
  uint16_t  CGMS_DATE5[]={ 
    0x01, 0x06, 0x00, 0x23, 0x00, 0x00                    };
  //0x01, 0x06, 0x00, 0x22, 0x34, 0xC0  
  CRC1=CalculateCrc16(CGMS_DATE5,4);

  uint8_t CGMS_DATE_85[] = { 
    CGMS_DATE5[0],
    CGMS_DATE5[1],
    CGMS_DATE5[2],
    CGMS_DATE5[3],
    CGMS_DATE5[4],
    CGMS_DATE5[5]                    };

  CGMS_DATE_85[4]=lowByte(CRC1);
  CGMS_DATE_85[5]=highByte(CRC1);

  if (!send_frame(sizeof(CGMS_DATE_85), CGMS_DATE_85)) {
    error("ERROR REQ");
    return;
  }
  //
  rcvd = 256;  
  if (!get_frame(buf, &rcvd)) {
    error("ERROR RESP");
    return;
  }
  for (int i = 0; i < 256; i++){
    page[i]=buf[i];
  }

  long systimeoff = ( ((unsigned long)buf[7] << 24) 
    + ((unsigned long)buf[6] << 16) 
    + ((unsigned long)buf[5] << 8) 
    + ((unsigned long)buf[4] ) ) ;

  Serial.println("System Time off");
  Serial.println(systimeoff);

  printTime(systime+systimeoff);


  //
  //READ_GLUCOSE_UNIT = 37
  Serial.println("Glucose Unit");
  //01 06 00 25 D3 B0
  uint16_t  CGMS_DATE3[]={ 
    0x01, 0x06, 0x00, 0x25, 0x00, 0x00                    };

  CRC1=CalculateCrc16(CGMS_DATE3,4);

  uint8_t CGMS_DATE_83[] = { 
    CGMS_DATE3[0],
    CGMS_DATE3[1],
    CGMS_DATE3[2],
    CGMS_DATE3[3],
    CGMS_DATE3[4],
    CGMS_DATE3[5]                    };

  CGMS_DATE_83[4]=lowByte(CRC1);
  CGMS_DATE_83[5]=highByte(CRC1);

  if (!send_frame(sizeof(CGMS_DATE_83), CGMS_DATE_83)) {
    error("ERROR REQ");
    return;
  }
  //
  rcvd = 256;  
  if (!get_frame(buf, &rcvd)) {
    error("ERROR RESP");
    return;
  }
  for (int i = 0; i < 256; i++){
    page[i]=buf[i];
  }

  units=page[4];

  Serial.println("Get page count");
  if (!send_frame(sizeof(CGMS_PAGE_COUNT), CGMS_PAGE_COUNT)) {
    error("ERROR REQ");
    return;
  }

  rcvd = 256;
  if (!get_frame(buf, &rcvd)) {
    error("ERROR RESP");
    return;
  }

  //template for page request
  uint16_t CGMS_REQUEST_PAGE[]        = { 
    0x01, 0x0C, 0x00, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 0x01                 };
  //                        04 = EGV Data
  //                        03 = Sensor Data

  CGMS_REQUEST_PAGE[5]  = buf[8];
  CGMS_REQUEST_PAGE[6]  = buf[9];

  uint16_t CRC=CalculateCrc16(CGMS_REQUEST_PAGE,10);

  uint8_t CGMS_REQUEST_PAGE_8[] = { 
    CGMS_REQUEST_PAGE[0],
    CGMS_REQUEST_PAGE[1],
    CGMS_REQUEST_PAGE[2],
    CGMS_REQUEST_PAGE[3],
    CGMS_REQUEST_PAGE[4],
    CGMS_REQUEST_PAGE[5],
    CGMS_REQUEST_PAGE[6],
    CGMS_REQUEST_PAGE[7],
    CGMS_REQUEST_PAGE[8],
    CGMS_REQUEST_PAGE[9],
    CGMS_REQUEST_PAGE[10],
    CGMS_REQUEST_PAGE[11]                                                          };

  CGMS_REQUEST_PAGE_8[10]=lowByte(CRC);
  CGMS_REQUEST_PAGE_8[11]=highByte(CRC);

  Serial.println("Request last page");  
  if (!send_frame(sizeof(CGMS_REQUEST_PAGE_8), CGMS_REQUEST_PAGE_8)) {
    error("ERROR REQ");
    return;
  }
  //
  rcvd = 256;  
  if (!get_frame(buf, &rcvd)) {
    error("ERROR RESP");
    return;
  }
  for (int i = 0; i < 256; i++){
    page[i]=buf[i];
  }
  //
  rcvd = 256;
  if (!get_frame(buf, &rcvd)) {
    error("ERROR RESP");
    return;
  }
  for (int i = 0; i < 256; i++){
    page[i+256]=buf[i];
  }
  //
  rcvd = 256;
  if (!get_frame(buf, &rcvd)) {
    error("ERROR RESP");
    return;
  }
  for (int i = 0; i < 256; i++){
    page[i+512]=buf[i];
  }

  //fill in the array with past values
  if(firstime==0){
    for(int i=10;i>0;i--){
      parseGlucose(page,i);
    }
  }

  //get the glucose and the time since epoch
  parseGlucose(page,0);
  //

}

uint16_t CalculateCrc16(uint16_t* buf,int len)
{
  uint16_t num = 0;
  for (int i = 0; i < len; i++)
  {
    num = ((num << 8) ^ m_crc16Table[((num >> 8) ^ buf[i]) & 0xff]);
  }
  return num;
}

void parseDateIntoGlobals(long c_time){
  time_t current_time;
  char* c_time_string;
  char *dy = (char*) malloc(3);
  char *mnth = (char*) malloc(3);
  char *ddate = (char*) malloc(2);
  char *hourc = (char*) malloc(2);

  /* Obtain current time as seconds elapsed since the Epoch. */
  current_time = c_time;
  c_time_string = ctime(&current_time);

  Serial.println(c_time_string);
  //Sat Feb  1 21:38:17 1975
  //0123456789012345678901234
  //          1         2   
  char msd, lsd;

  Serial.println("Parse Day");

  dy= strndup( c_time_string+0, 3);
  Serial.println(dy);
  if (strncmp(dy,"Sat",3)==0)
    day=6;

  if (strncmp(dy,"Sun",3)==0)
    day=0;

  if (strncmp(dy,"Mon",3)==0)
    day=1;

  if (strncmp(dy,"Tue",3)==0)
    day=2;

  if (strncmp(dy,"Wed",3)==0)
    day=3;

  if (strncmp(dy,"Thu",3)==0)
    day=4;

  if (strncmp(dy,"Fri",3)==0)
    day=5;

  Serial.println("Parse Month");
  mnth= strndup( c_time_string+4, 3);

  if (strncmp(mnth,"Jan",3)==0)
    month=1;

  if (strncmp(mnth,"Feb",3)==0)
    month=2;

  if (strncmp(mnth,"Mar",3)==0)
    month=3;

  if (strncmp(mnth,"Apr",3)==0)
    month=4;

  if (strncmp(mnth,"May",3)==0)
    month=5;

  if (strncmp(mnth,"Jun",3)==0)
    month=6;

  if (strncmp(mnth,"Jul",3)==0)
    month=7;

  if (strncmp(mnth,"Aug",3)==0)
    month=8;

  if (strncmp(mnth,"Sep",3)==0)
    month=9;

  if (strncmp(mnth,"Oct",3)==0)
    month=10;

  if (strncmp(mnth,"Nov",3)==0)
    month=11;

  if (strncmp(mnth,"Dec",3)==0)
    month=12;

  Serial.println("Parse Day number");

  ddate=strndup( c_time_string+8, 2);
  date = atoi(ddate);

  Serial.println("Parse Hour");

  hourc=strndup( c_time_string+11, 2);
  msd = hourc[0];
  lsd = hourc[1];
  hours = ((msd - 0x30) * 10) + (lsd - 0x30);


  minutes = getMinutes(c_time_string);
  seconds = getSeconds(c_time_string);

  free(dy);
  free(mnth);
  free(ddate);
  free(hourc);
}


/* Sometimes there are errors when warm resetting the Arduino. Maybe
 we should power down + up the Maxim chip to cold reboot the AP. But
 for now we just warm-reboot the Arduino, it ends up by working
 after a few times... */
void error(char* text)
{
  Serial.print("### ");
  Serial.println(text);
  delay(1000);
  return;
}
