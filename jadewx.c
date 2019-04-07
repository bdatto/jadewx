#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <curl/curl.h>
#include <mysql/mysql.h>
#include <pthread.h>

char *mysql_username=NULL,*mysql_password=NULL;
char *WU_station=NULL,*WU_password=NULL;
int WU_upload=0;
CURL *curl=NULL;
int device_id;
int comm_interval=10;
int config_set,config_requested;
unsigned char cfg_cs[2];
struct timespec first_sleep,next_sleep;
int latest_haddr;
float rain_day,rain_total_base;
time_t last_WU_upload_time;
time_t last_history_print_time;
char *compass[361]={
"??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"NNE","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"NE","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"ENE","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"E","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"ESE","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"SE","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"ESE","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"S","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"SSW","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"SW","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"WSW","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"W","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"WNW","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"NW","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"NNW","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??","??",
"N"
};
struct {
  float temp_out_max,temp_out_min,wspd_max,wgust_max,barom_min,barom_max;
} extremes;
pthread_t tid=0xffffffff;

int timestamp(char *datetime)
{
  struct tm tm;
  strptime(datetime,"%Y-%m-%d %H:%M:%S",&tm);
  return mktime(&tm);
}

int now(time_t epoch)
{
  return mktime(localtime(&epoch));
}

void initialize_transceiver(libusb_device_handle *handle,char *serial_num)
{
  printf("initializing transceiver...\n");
  unsigned char *buffer=(unsigned char *)malloc(21*sizeof(unsigned char));
  for (size_t n=0; n < 21; ++n) {
    buffer[n]=0xcc;
  }
  buffer[0]=0xdd;
  buffer[1]=0x0a;
  buffer[2]=0x1;
  buffer[3]=0xf5;
  int status=libusb_control_transfer(handle,0x21,0x9,0x3dd,0,buffer,21,1000);
  status=libusb_control_transfer(handle,0xa1,0x1,0x3dc,0,buffer,21,1000);
for (size_t n=0; n < 21; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  int corr_val=(((buffer[4] << 8) | buffer[5]) << 8 | buffer[6]) << 8 | buffer[7];
  int adj_freq=(905000000./16000000.*16777216.)+corr_val;
  if ( (adj_freq % 2) == 0) {
    ++adj_freq;
  }
printf("corr vals: %d %d %d %d  adj: %d  adj freq: %d\n",(int)buffer[4],(int)buffer[5],(int)buffer[6],(int)buffer[7],corr_val,adj_freq);

  buffer[2]=0x1;
  buffer[3]=0xf9;
  status=libusb_control_transfer(handle,0x21,0x9,0x3dd,0,buffer,21,1000);
  status=libusb_control_transfer(handle,0xa1,0x1,0x3dc,0,buffer,21,1000);
for (size_t n=0; n < 21; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  sprintf(serial_num,"%02d%02d%02d%02d%02d%02d%02d",(int)buffer[4],(int)buffer[5],(int)buffer[6],(int)buffer[7],(int)buffer[8],(int)buffer[9],(int)buffer[10]);
  device_id=(buffer[9] << 8) | buffer[10];
printf("serial num: %s  device_id: %d (%04x)\n",serial_num,device_id,device_id);

  int addr[50],rval[50];
// IFMODE
  addr[0]=0x8;
  rval[0]=0x0;
// MODULATION
  addr[1]=0x10;
  rval[1]=0x41;
// ENCODING
  addr[2]=0x11;
  rval[2]=0x7;
// FRAMING
  addr[3]=0x12;
  rval[3]=0x84;
// CRCINIT0
  addr[4]=0x17;
  rval[4]=0xff;
// CRCINIT1
  addr[5]=0x16;
  rval[5]=0xff;
// CRCINIT2
  addr[6]=0x15;
  rval[6]=0xff;
// CRCINIT3
  addr[7]=0x14;
  rval[7]=0xff;
// FREQ0
  addr[8]=0x23;
  rval[8]=(adj_freq >> 0) & 0xff;
// FREQ1
  addr[9]=0x22;
  rval[9]=(adj_freq >> 8) & 0xff;
// FREQ2
  addr[10]=0x21;
  rval[10]=(adj_freq >> 16) & 0xff;
// FREQ3
  addr[11]=0x20;
  rval[11]=(adj_freq >> 24) & 0xff;
printf("%x %x %x %x\n",(int)rval[8],(int)rval[9],(int)rval[10],(int)rval[11]);
// FSKDEV2
  addr[12]=0x25;
  rval[12]=0x0;
// FSKDEV1
  addr[13]=0x26;
  rval[13]=0x31;
// FSKDEV0
  addr[14]=0x27;
  rval[14]=0x27;
// IFFREQHI
  addr[15]=0x28;
  rval[15]=0x20;
// IFFREQLO
  addr[16]=0x29;
  rval[16]=0x0;
// PLLLOOP
  addr[17]=0x2c;
  rval[17]=0x1d;
// PLLRANGING
  addr[18]=0x2d;
  rval[18]=0x8;
// PLLRNGCLK
  addr[19]=0x2e;
  rval[19]=0x3;
// TXPWR
  addr[20]=0x30;
  rval[20]=0x3;
// TXRATEHI
  addr[21]=0x31;
  rval[21]=0x0;
// TXRATEMID
  addr[22]=0x32;
  rval[22]=0x51;
// TXRATELO
  addr[23]=0x33;
  rval[23]=0xec;
// MODMISC
  addr[24]=0x34;
  rval[24]=0x3;
// ADCMISC
  addr[25]=0x38;
  rval[25]=0x1;
// AGCTARGET
  addr[26]=0x39;
  rval[26]=0xe;
// AGCATTACK
  addr[27]=0x3a;
  rval[27]=0x11;
// AGCDECAY
  addr[28]=0x3b;
  rval[28]=0xe;
// CICDEC
  addr[29]=0x3f;
  rval[29]=0x3f;
// DATARATEHI
  addr[30]=0x40;
  rval[30]=0x19;
// DATARATELO
  addr[31]=0x41;
  rval[31]=0x66;
// TMGGAINHI
  addr[32]=0x42;
  rval[32]=0x1;
// TMGGAINLO
  addr[33]=0x43;
  rval[33]=0x96;
// PHASEGAIN
  addr[34]=0x44;
  rval[34]=0x3;
// FREQGAIN
  addr[35]=0x45;
  rval[35]=0x4;
// FREQGAIN2
  addr[36]=0x46;
  rval[36]=0xa;
// AMPLGAIN
  addr[37]=0x47;
  rval[37]=0x6;
// SPAREOUT
  addr[38]=0x60;
  rval[38]=0x0;
// TESTOBS
  addr[39]=0x68;
  rval[39]=0x0;
// APEOVER
  addr[40]=0x70;
  rval[40]=0x0;
// TMMUX
  addr[41]=0x71;
  rval[41]=0x0;
// PLLVCOI
  addr[42]=0x72;
  rval[42]=0x1;
// PLLCPEN
  addr[43]=0x73;
  rval[43]=0x1;
// AGCMANUAL
  addr[44]=0x78;
  rval[44]=0x0;
// ADCDCLEVEL
  addr[45]=0x79;
  rval[45]=0x10;
// RFMISC
  addr[46]=0x7a;
  rval[46]=0xb0;
// TXDRIVER
  addr[47]=0x7b;
  rval[47]=0x88;
// REF
  addr[48]=0x7c;
  rval[48]=0x23;
// RXMISC
  addr[49]=0x7d;
  rval[49]=0x35;
printf("%d %d %d %d\n",rval[11],rval[10],rval[9],rval[8]);
  unsigned char *reg_buffer=(unsigned char *)malloc(5*sizeof(unsigned char));
  reg_buffer[0]=0xf0;
  reg_buffer[2]=0x1;
  reg_buffer[4]=0;
  for (size_t n=0; n < 50; ++n) {
// address
    reg_buffer[1]=addr[n] & 0x7f;
// data to write
    reg_buffer[3]=rval[n];
    int status=libusb_control_transfer(handle,0x21,0x9,0x3f0,0,reg_buffer,5,1000);
  }
  printf("...transceiver initialization done\n");
}

const size_t RX_BUFFER_LENGTH=21;
unsigned char *RX_buffer=NULL;
void setRX(libusb_device_handle *handle)
{
  if (RX_buffer == NULL) {
    RX_buffer=(unsigned char *)malloc(RX_BUFFER_LENGTH*sizeof(unsigned char));
    RX_buffer[0]=0xd0;
    for (size_t n=1; n < RX_BUFFER_LENGTH; ++n) {
	RX_buffer[n]=0x0;
    }
  }
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d0,0,RX_buffer,21,1000);
//  printf("set RX status: %d\n",status);
//  for (size_t n=0; n < RX_BUFFER_LENGTH; ++n) {
//    printf(" %02x",RX_buffer[n]);
//  }
//  printf("\n");
}

void do_setup(libusb_device_handle *handle)
{
  unsigned char *buffer=(unsigned char *)malloc(21*sizeof(unsigned char));
  for (size_t n=0; n < 21; ++n) {
    buffer[n]=0x0;
  }
// execute
  buffer[0]=0xd9;
  buffer[1]=0x5;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d9,0,buffer,15,1000);
printf("execute status: %d\n",status);
// set preamble pattern
  buffer[0]=0xd8;
  buffer[1]=0xaa;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  status=libusb_control_transfer(handle,0x21,0x9,0x3d8,0,buffer,21,1000);
printf("set preamble status: %d\n",status);
// set state
  buffer[0]=0xd7;
  buffer[1]=0x0;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  status=libusb_control_transfer(handle,0x21,0x9,0x3d7,0,buffer,21,1000);
printf("set state status: %d\n",status);
  sleep(1);
// setRX
  setRX(handle);
// set preamble pattern
  buffer[0]=0xd8;
  buffer[1]=0xaa;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  status=libusb_control_transfer(handle,0x21,0x9,0x3d8,0,buffer,21,1000);
printf("set preamble status: %d\n",status);
// set state
  buffer[0]=0xd7;
  buffer[1]=0x1e;
for (size_t n=0; n < 15; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  status=libusb_control_transfer(handle,0x21,0x9,0x3d7,0,buffer,21,1000);
printf("set state status: %d\n",status);
  sleep(1);
// setRX
  setRX(handle);
  first_sleep.tv_sec=0;
  first_sleep.tv_nsec=85000000;
  next_sleep.tv_sec=0;
  next_sleep.tv_nsec=5000000;
  config_set=0;
  config_requested=0;
  cfg_cs[0]=0;
  cfg_cs[1]=0;
  latest_haddr=0xfffff;
  rain_day=-999.;
  rain_total_base=-999.;
  last_WU_upload_time=0;
  last_history_print_time=0;
}

const size_t TX_BUFFER_LENGTH=21;
unsigned char *TX_buffer=NULL;
void setTX(libusb_device_handle *handle)
{
// setTX
  if (TX_buffer == NULL) {
    TX_buffer=(unsigned char *)malloc(TX_BUFFER_LENGTH*sizeof(unsigned char));
    TX_buffer[0]=0xd1;
    for (size_t n=1; n < TX_BUFFER_LENGTH; ++n) {
	TX_buffer[n]=0x0;
    }
  }
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d1,0,TX_buffer,21,1000);
//  printf("setTX status %d \n",status);
//  for (size_t n=0; n < TX_BUFFER_LENGTH; ++n) {
//    printf(" %02x",TX_buffer[n]);
//  }
//  printf("\n");
}

MYSQL mysql;
void open_mysql()
{
  if (mysql_init(&mysql) == NULL) {
    fprintf(stderr,"Error initializing MySQL\n");
    exit(1);
  }
  if (!mysql_real_connect(&mysql,NULL,mysql_username,mysql_password,NULL,0,NULL,0)) {
    fprintf(stderr,"Error connecting to the database\n");
    exit(1);
  }
}

void close_mysql()
{
  mysql_close(&mysql);
}

unsigned char *state_buffer=NULL;
int get_state(libusb_device_handle *handle)
{
  if (state_buffer == NULL) {
    state_buffer=(unsigned char *)malloc(10*sizeof(unsigned char));
  }
  state_buffer[1]=0x0;
  int status=libusb_control_transfer(handle,0xa1,0x1,0x3de,0,state_buffer,10,1000);
//printf("getstate status: %d %x %x %x %x %x %x\n",status,state_buffer[0],state_buffer[1],state_buffer[2],state_buffer[3],state_buffer[4],state_buffer[5]);
  return state_buffer[1];
}

void request_set_config(libusb_device_handle *handle,unsigned char *buffer,int first)
{
  buffer[0]=0xd5;
  buffer[1]=0x0;
  buffer[2]=0x9;
  if (first) {
    buffer[3]=0xf0;
    buffer[4]=0xf0;
  }
  else {
    buffer[3]=((device_id >> 8) & 0xff);
    buffer[4]=(device_id & 0xff);
  }
  buffer[5]=0x2;
  buffer[6]=cfg_cs[0];
  buffer[7]=cfg_cs[1];
  buffer[8]=(comm_interval >> 4) & 0xff;
  int history_addr=0xffffff;
  buffer[9]=(history_addr >> 16) & 0x0f | 16*(comm_interval & 0xf);
  buffer[10]=(history_addr >> 8) & 0xff;
  buffer[11]=(history_addr >> 0) & 0xff;
  printf("request-set-config");
  for (size_t n=0; n < buffer[2]+3; ++n) {
    printf(" %02x",buffer[n]);
  }
  printf("\n");
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,buffer,12,1000);
}

unsigned char *msg_buffer=NULL;
void request_weather_message(libusb_device_handle *handle,int action,int history_addr)
{
  if (msg_buffer == NULL) {
    msg_buffer=(unsigned char *)malloc(12*sizeof(unsigned char));
  }
  if (history_addr != 0xfffff) {
    printf("requesting history message with address: %d\n",history_addr);
  }
  msg_buffer[0]=0xd5;
  msg_buffer[1]=0x0;
  msg_buffer[2]=0x9;
  msg_buffer[3]=((device_id >> 8) & 0xff);
  msg_buffer[4]=(device_id & 0xff);
  msg_buffer[5]=action;
  msg_buffer[6]=cfg_cs[0];
  msg_buffer[7]=cfg_cs[1];
  msg_buffer[8]=(comm_interval >> 4) & 0xff;
  msg_buffer[9]=(history_addr >> 16) & 0x0f | 16*(comm_interval & 0xf);
  msg_buffer[10]=(history_addr >> 8) & 0xff;
  msg_buffer[11]=(history_addr >> 0) & 0xff;
/*
  for (size_t n=0; n < 12; ++n) {
    printf(" %02x",(int)msg_buffer[n]);
  }
  printf("\n");
*/
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,msg_buffer,12,1000);
}

void swap(unsigned char *buffer,int start,int stop)
{
  size_t mid=(stop-start+1)/2;
  size_t m=stop;
  for (size_t n=0; n < mid; ++n) {
    unsigned char temp=buffer[start+n];
    buffer[start+n]=buffer[m];
    buffer[m]=temp;
    --m;
  }
}

const size_t CONFIG_BUFFER_LENGTH=51;
unsigned char *config_buffer=NULL;
void store_config(libusb_device_handle *handle,unsigned char *buffer)
{
  printf("***GETCONFIG***\n");
buffer[9]=(buffer[9] & 0xf);
  int ocs=7;
  for (size_t n=0; n < CONFIG_BUFFER_LENGTH; ++n) {
    printf(" %02x",buffer[n]);
    if (n >= 7 && n < 47) {
	ocs+=buffer[n];
    }
  }
  printf("\nold checksum: %x\n",ocs);
  if (!config_set) {
    if (config_buffer == NULL) {
	config_buffer=(unsigned char *)malloc(CONFIG_BUFFER_LENGTH*sizeof(unsigned char));
    }
    memcpy(config_buffer,buffer,CONFIG_BUFFER_LENGTH);
    config_buffer[0]=0xd5;
    swap(config_buffer,14,18);
    swap(config_buffer,19,23);
    swap(config_buffer,24,25);
    swap(config_buffer,26,27);
    swap(config_buffer,28,31);
    swap(config_buffer,33,35);
    swap(config_buffer,36,40);
    swap(config_buffer,41,45);
    config_buffer[9]=0x2;
// set history interval to 1 minute
    config_buffer[32]=0x0;
    config_buffer[46]=0x10;
    int cs=7;
    for (size_t n=7; n < 46; ++n) {
	cs+=config_buffer[n];
    }
    printf("new checksum: %x\n",cs);
    cfg_cs[0]=cs >> 8;
    cfg_cs[1]=(cs & 0xff);
    config_buffer[CONFIG_BUFFER_LENGTH-2]=cfg_cs[0];
    config_buffer[CONFIG_BUFFER_LENGTH-1]=cfg_cs[1];
    for (size_t n=0; n < CONFIG_BUFFER_LENGTH; ++n) {
	printf(" %02x",config_buffer[n]);
    }
    printf("\n");
    request_set_config(handle,buffer,0);
//    config_set=1;
  }
  else {
    request_weather_message(handle,0,latest_haddr);
  }
  first_sleep.tv_nsec=300000000;
  next_sleep.tv_nsec=10000000;
  config_requested=1;
}

void set_config(libusb_device_handle *handle)
{
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,config_buffer,CONFIG_BUFFER_LENGTH,1000);
  printf("***SETCONFIG*** %d\n",status);
  first_sleep.tv_nsec=85000000;
  next_sleep.tv_nsec=5000000;
config_set=1;
}

void set_time(libusb_device_handle *handle,unsigned char *buffer)
{
  time_t t=time(NULL);
  struct tm *tm_result=localtime(&t);
  ++tm_result->tm_mon;
  tm_result->tm_year-=100;
  printf("***TIMESET*** 20%02d-%02d-%02d %02d:%02d:%02d %d\n",tm_result->tm_year,tm_result->tm_mon,tm_result->tm_mday,tm_result->tm_hour,tm_result->tm_min,tm_result->tm_sec,tm_result->tm_wday);
  buffer[0]=0xd5;
  buffer[1]=0x0;
  buffer[2]=0xc;
  buffer[3]=((device_id >> 8) & 0xff);
  buffer[4]=(device_id & 0xff);
  buffer[5]=0xc0;
  cfg_cs[0]=buffer[7];
  cfg_cs[1]=buffer[8];
  buffer[6]=cfg_cs[0];
  buffer[7]=cfg_cs[1];
  int i=tm_result->tm_sec/10;
  buffer[8]=(i*16)+(tm_result->tm_sec % 10);
  i=tm_result->tm_min/10;
  buffer[9]=(i*16)+(tm_result->tm_min % 10);
  i=tm_result->tm_hour/10;
  buffer[10]=(i*16)+(tm_result->tm_hour % 10);
  buffer[11]=((tm_result->tm_mday % 10) << 4) | tm_result->tm_wday;
  i=tm_result->tm_mday/10;
  buffer[12]=((tm_result->tm_mon % 10) << 4) | ((i*16) >> 4);
  i=tm_result->tm_mon/10;
  buffer[13]=((tm_result->tm_year % 10) << 4) | ((i*16) >> 4);
  i=tm_result->tm_year/10;
  buffer[14]=((i*16) >> 4);
//  for (size_t n=0; n < 15; ++n) {
//    printf(" %02x",buffer[n]);
//  }
//  printf("\n");
  libusb_control_transfer(handle,0x21,0x9,0x3d5,0,buffer,15,1000);
  first_sleep.tv_nsec=85000000;
  next_sleep.tv_nsec=5000000;
}

typedef struct {
  float temp_out,dewp_out,wspd,wgust,rain_1hr,rain_total,barom;
  int rh_out,wdir;
} CurrentWeather;
CurrentWeather wx[2];
int cwx_idx=0;

void decode_current_wx(unsigned char *buffer,CurrentWeather *current_weather)
{
  current_weather->temp_out=(((buffer[42] & 0xf)*10000.+(buffer[43] >> 4)*1000.+(buffer[43] & 0xf)*100.+(buffer[44] >> 4)*10.+(buffer[44] & 0xf))/1000.-40.)*9./5.+32.;
  current_weather->dewp_out=(((buffer[78] & 0xf)*10000.+(buffer[79] >> 4)*1000.+(buffer[79] & 0xf)*100.+(buffer[80] >> 4)*10.+(buffer[80] & 0xf))/1000.-40.)*9./5.+32.;
  current_weather->rh_out=(buffer[106] >> 4)*10+(buffer[106] & 0xf);
  current_weather->wdir=lroundf((buffer[162] & 0xf)*22.5);
  if (current_weather->wdir == 0) {
    current_weather->wdir=360;
  }
  int idum=((buffer[172] >> 4) << 20) | ((buffer[172] & 0xf) << 16) | ((buffer[173] >> 4) << 12) | ((buffer[173] & 0xf) << 8) | ((buffer[174] >> 4) << 4) | (buffer[174] & 0xf);
  if (idum >= 0x470000) {
    current_weather->wdir=-999;
    current_weather->wspd=-999.;
  }
  else {
    current_weather->wspd=((idum/256.)/100.)/1.60934;
  }
  idum=((buffer[187] >> 4) << 20) | ((buffer[187] & 0xf) << 16) | ((buffer[188] >> 4) << 12) | ((buffer[188] & 0xf) << 8) | ((buffer[189] >> 4) << 4) | (buffer[189] & 0xf);
  if (idum >= 0x470000) {
    current_weather->wgust=-999.;
  }
  else {
    current_weather->wgust=((idum/256.)/100.)/1.60934;
  }
  current_weather->rain_1hr=((buffer[148] >> 4)*100000.+(buffer[148] & 0xf)*10000.+(buffer[149] >> 4)*1000.+(buffer[149] & 0xf)*100.+(buffer[150] >> 4)*10.+(buffer[150] & 0xf))/2540.;
  current_weather->rain_total=((buffer[156] & 0xf)*1000000.+(buffer[157] >> 4)*100000.+(buffer[157] & 0xf)*10000.+(buffer[158] >> 4)*1000.+(buffer[158] & 0xf)*100.+(buffer[159] >> 4)*10.+(buffer[159] & 0xf))/25800.;
  current_weather->barom=((buffer[210] >> 4)*10000.+(buffer[210] & 0xf)*1000.+(buffer[211] >> 4)*100.+(buffer[211] & 0xf)*10.+(buffer[212] >> 4))/100.;
}

typedef struct {
  int this_addr,latest_addr;
  float temp_out,wspd,wgust,rain_raw,barom;
  int rh_out,wdir;
  char datetime[20];
  int day_num;
} History;

int decode_history(unsigned char *buffer,History *history)
{
//  for (size_t n=0; n < 30; ++n) {
//    printf(" %02x",buffer[n]);
//  }
//  printf("\n");
  history->latest_addr=(buffer[6] << 8 | buffer[7]) << 8 | buffer[8];
  if (history->latest_addr > 32744) {
    return 0;
  }
  history->this_addr=(buffer[9] << 8 | buffer[10]) << 8 | buffer[11];
  if (history->this_addr > 32744) {
    return 0;
  }
  latest_haddr=history->latest_addr-18;
  history->temp_out=(((buffer[22] >> 4)*100.+(buffer[22] & 0xf)*10.+(buffer[23] >> 4))/10.-40.)*9./5.+32.;
  history->rh_out=(buffer[17] & 0xf)*10+(buffer[18] >> 4);
  history->wdir=lroundf((buffer[14] >> 4)*22.5);
  if (history->wdir == 0) {
    history->wdir=360;
  }
  int idum=(buffer[14] & 0xf) << 8 | buffer[15];
  history->wspd=idum/10.*2.237;
  idum=(buffer[12] & 0xf) << 8 | buffer[13];
  history->wgust=idum/10.*2.237;
  if (history->wgust < 114. && history->wgust > extremes.wgust_max) {
    extremes.wgust_max=history->wgust;
  }
  history->rain_raw=((buffer[16] >> 4)*100.+(buffer[16] & 0xf)*10.+(buffer[17] >> 4))/100.;
  history->barom=((buffer[19] & 0xf)*10000.+(buffer[20] >> 4)*1000.+(buffer[20] & 0xf)*100.+(buffer[21] >> 4)*10.+(buffer[21] & 0xf))/10.;
  history->day_num=(buffer[27] >> 4)*10+(buffer[27] & 0xf);
  sprintf(history->datetime,"20%02d-%02d-%02d %02d:%02d:00",(buffer[25] >> 4)*10+(buffer[25] & 0xf),(buffer[26] >> 4)*10+(buffer[26] & 0xf),history->day_num,(buffer[28] >> 4)*10+(buffer[28] & 0xf),(buffer[29] >> 4)*10+(buffer[29] & 0xf));
  history->datetime[19]='\0';
  if (history->this_addr <= history->latest_addr) {
    if (history->temp_out > extremes.temp_out_max) {
	extremes.temp_out_max=history->temp_out;
    }
    if (history->temp_out < extremes.temp_out_min) {
	extremes.temp_out_min=history->temp_out;
    }
    if (history->wspd < 114. && history->wspd > extremes.wspd_max) {
	extremes.wspd_max=history->wspd;
    }
    if (history->barom > extremes.barom_max) {
	extremes.barom_max=history->barom;
    }
    if (history->barom < extremes.barom_min) {
	extremes.barom_min=history->barom;
    }
  }
  return 1;
}

void print_history(History *history)
{
  printf("latest: %d  this: %d, %s, %5.1f, %5.1f, %2d, %3d, %4.1f, %4.1f, %4.2f\n",history->latest_addr,history->this_addr,history->datetime,history->barom,history->temp_out,history->rh_out,history->wdir,history->wspd,history->wgust,history->rain_raw);
  last_history_print_time=time(NULL);
}

//const size_t HISTORY_SIZE=300;
const size_t HISTORY_SIZE=60;
History *history_queue=NULL;
size_t curr_hidx=0,last_hidx=0;
const char *HISTORY_INSERT="insert into wx.history values(%d,%d,%d,%d,%d,%d,%d,%d,%d) on duplicate key update haddr = values(haddr)";
char *store_buffer=NULL;
void store_history_records()
{
  if (mysql_username != NULL && mysql_password != NULL) {
    open_mysql();
    if (store_buffer == NULL) {
	store_buffer=(char *)malloc(256*sizeof(char));
    }
    for (size_t n=0; n < HISTORY_SIZE; ++n) {
	for (size_t m=0; m < 256; ++m) {
	  store_buffer[m]=0;
	}
	sprintf(store_buffer,HISTORY_INSERT,timestamp(history_queue[n].datetime),lround(history_queue[n].barom*10.),lround(history_queue[n].temp_out*10.),history_queue[n].rh_out,history_queue[n].wdir,lround(history_queue[n].wspd*10.),lround(history_queue[n].wgust*10.),lround(history_queue[n].rain_raw*100.),history_queue[n].this_addr);
	mysql_query(&mysql,store_buffer);
	printf("stored '%s'\n",store_buffer);
    }
    close_mysql();
  }
}


int wx_changed()
{
  int lwx_idx=1-cwx_idx;
  if (wx[cwx_idx].barom != wx[lwx_idx].barom) {
    return 1;
  }
  if (wx[cwx_idx].temp_out != wx[lwx_idx].temp_out) {
    return 1;
  }
  if (wx[cwx_idx].dewp_out != wx[lwx_idx].dewp_out) {
    return 1;
  }
  if (wx[cwx_idx].rh_out != wx[lwx_idx].rh_out) {
    return 1;
  }
  if (wx[cwx_idx].wdir != wx[lwx_idx].wdir) {
    return 1;
  }
  if (wx[cwx_idx].wspd != wx[lwx_idx].wspd) {
    return 1;
  }
  return 0;
}

int mdays[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
void get_utc_date(time_t epoch,struct tm **tm_result)
{
  *tm_result=localtime(&epoch);
  (*tm_result)->tm_year+=1900;
  ++(*tm_result)->tm_mon;
  (*tm_result)->tm_hour+=7;
  if ((*tm_result)->tm_hour >= 24) {
    (*tm_result)->tm_hour-=24;
    ++(*tm_result)->tm_mday;
    if ( (*tm_result)->tm_mday > mdays[(*tm_result)->tm_mon]) {
	++(*tm_result)->tm_mon;
	(*tm_result)->tm_mday=1;
	if ( (*tm_result)->tm_mon > 12) {
	  ++(*tm_result)->tm_year;
	  (*tm_result)->tm_mon=1;
	}
    }
  }
}

void request_get_config(libusb_device_handle *handle,unsigned char *buffer)
{
  buffer[0]=0xd5;
  buffer[1]=0x0;
  buffer[2]=0x9;
  buffer[3]=((device_id >> 8) & 0xff);
  buffer[4]=(device_id & 0xff);
  buffer[5]=0x3;
  buffer[6]=cfg_cs[0];
  buffer[7]=cfg_cs[1];
  buffer[8]=(comm_interval >> 4) & 0xff;
  int history_addr=0xffffff;
  buffer[9]=(history_addr >> 16) & 0x0f | 16*(comm_interval & 0xf);
  buffer[10]=(history_addr >> 8) & 0xff;
  buffer[11]=(history_addr >> 0) & 0xff;
printf("request-get-config");
for (size_t n=0; n < 13; ++n) {
printf(" %02x",(int)buffer[n]);
}
printf("\n");
  int status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,buffer,12,1000);
}

void wait_for_message(libusb_device_handle *handle)
{
  time_t t1=time(NULL);
  nanosleep(&first_sleep,NULL);
  while (1) {
    if (get_state(handle) == 0x16) {
	break;
    }
    nanosleep(&next_sleep,NULL);
    time_t t2=time(NULL);
    if ( (t2-t1) > 30) {
	struct tm *tm_t=localtime(&t2);
	printf("***LOSTSYNC?*** 20%2d-%02d-%02d %02d:%02d\n",tm_t->tm_year-100,tm_t->tm_mon+1,tm_t->tm_mday,tm_t->tm_hour,tm_t->tm_min);
	request_weather_message(handle,0,latest_haddr);
	setTX(handle);
	t1=t2;
    }
  }
}

void *thread_scp(void *ts)
{
  system("scp -q /home/wx/current_wx.html pi@10.0.0.15:/home/pi/current_wx/");
  return NULL;
}

char *insert_buffer=NULL,*url_buffer=NULL;
const char *WU_UPLOAD_URL_FORMAT="https://rtupdate.wunderground.com/weatherstation/updateweatherstation.php?ID=%s&PASSWORD=%s&action=updateraw&realtime=1&rtfreq=2.5&softwaretype=jadewx&winddir=%d&windspeedmph=%.1f&windgustmph=%.1f&humidity=%d&dewptf=%.1f&tempf=%.1f&baromin=%.2f&rainin=%.2f&dailyrainin=%.2f&dateutc=%04d-%02d-%02d+%02d%%3A%02d%%3A%02d";
const char *WU_UPLOAD_URL_NO_WIND_FORMAT="https://rtupdate.wunderground.com/weatherstation/updateweatherstation.php?ID=%s&PASSWORD=%s&action=updateraw&realtime=1&rtfreq=2.5&softwaretype=jadewx&humidity=%d&dewptf=%.1f&tempf=%.1f&baromin=%.2f&rainin=%.2f&dailyrainin=%.2f&dateutc=%04d-%02d-%02d+%02d%%3A%02d%%3A%02d";
void handle_frame(libusb_device_handle *handle,unsigned char *buffer,int backfill_history)
{
//  printf("getframe: [%02x]",buffer[5]);
//  for (size_t n=0; n < buffer[2]; ++n) {
//    printf(" %02x",buffer[n]);
//  }
//  printf("\n");
  if (cfg_cs[0] == 0x0 && cfg_cs[1] == 0x0 && buffer[5] == 0x40) {
    cfg_cs[0]=buffer[7];
    cfg_cs[1]=buffer[8];
  }
  switch (buffer[5] >> 4) {
    case 0x2:
    {
	printf("***DATAWRITTEN***\n");
	setRX(handle);
	break;
    }
    case 0x4:
    {
	store_config(handle,buffer);
	break;
    }
    case 0x6:
    {
	if (!backfill_history) {
	  cwx_idx=1-cwx_idx;
	  decode_current_wx(&buffer[3],&wx[cwx_idx]);
	  if (url_buffer == NULL) {
	    url_buffer=(char *)malloc(4096*sizeof(char));
	  }
//	  printf("pressure: %.2fin\n",wx[cwx_idx].barom);
//	  printf("outside temp: %.1fF\n",wx[cwx_idx].temp_out);
//	  printf("outside dew point: %.1fF\n",wx[cwx_idx].dewp_out);
//	  printf("outside RH: %d%%\n",wx[cwx_idx].rh_out);
//	  printf("wind direction: %ddeg\n",wx[cwx_idx].wdir);
//	  printf("wind speed: %.1fmph\n",wx[cwx_idx].wspd);
//	  printf("wind gust: %.1fmph\n",wx[cwx_idx].wgust);
//	  printf("1-hour rain: %.2fin\n",wx[cwx_idx].rain_1hr);
//	  printf("total rain: %.2fin\n",wx[cwx_idx].rain_total);
	  if (WU_station != NULL && WU_password != NULL) {
	    time_t upload_time=time(NULL);
	    if ( (upload_time-last_WU_upload_time) > 3) {
		struct tm *tm_result;
		get_utc_date(upload_time,&tm_result);
		if (rain_total_base < 0.) {
		  rain_total_base=wx[cwx_idx].rain_total;
		  printf("total rain set to: %.2f\n",rain_total_base);
		}
		float computed_rain_day=rain_day+wx[cwx_idx].rain_total-rain_total_base;
//printf("rain report: %f %f %f %f\n",computed_rain_day,rain_day,wx[cwx_idx].rain_total,rain_total_base);
		if (WU_upload == 1) {
/*
struct timespec t1,t2;
clock_gettime(CLOCK_MONOTONIC,&t1);
*/
		  if (wx[cwx_idx].wspd >= 0.) {
		    sprintf(url_buffer,WU_UPLOAD_URL_FORMAT,WU_station,WU_password,wx[cwx_idx].wdir,wx[cwx_idx].wspd,wx[cwx_idx].wgust,wx[cwx_idx].rh_out,wx[cwx_idx].dewp_out,wx[cwx_idx].temp_out,wx[cwx_idx].barom,wx[cwx_idx].rain_1hr,computed_rain_day,tm_result->tm_year,tm_result->tm_mon,tm_result->tm_mday,tm_result->tm_hour,tm_result->tm_min,tm_result->tm_sec);
		  }
		  else {
		    sprintf(url_buffer,WU_UPLOAD_URL_NO_WIND_FORMAT,WU_station,WU_password,wx[cwx_idx].rh_out,wx[cwx_idx].dewp_out,wx[cwx_idx].temp_out,wx[cwx_idx].barom,wx[cwx_idx].rain_1hr,computed_rain_day,tm_result->tm_year,tm_result->tm_mon,tm_result->tm_mday,tm_result->tm_hour,tm_result->tm_min,tm_result->tm_sec);
		  }
		  curl_easy_setopt(curl,CURLOPT_URL,url_buffer);
		  CURLcode ccode;
		  if ( (ccode=curl_easy_perform(curl)) == 0) {
		    if (wx[cwx_idx].wspd >= 0.) {
			printf("WUupload %d,%.1f,%.1f,%d,%.1f,%.1f,%.2f,%.2f,%.2f,%04d-%02d-%02d %02d:%02d:%02d ",wx[cwx_idx].wdir,wx[cwx_idx].wspd,wx[cwx_idx].wgust,wx[cwx_idx].rh_out,wx[cwx_idx].dewp_out,wx[cwx_idx].temp_out,wx[cwx_idx].barom,wx[cwx_idx].rain_1hr,computed_rain_day,tm_result->tm_year,tm_result->tm_mon,tm_result->tm_mday,tm_result->tm_hour,tm_result->tm_min,tm_result->tm_sec);
		    }
		    else {
			printf("WUupload (no wind) %d,%.1f,%.1f,%.2f,%.2f,%.2f,%04d-%02d-%02d %02d:%02d:%02d ",wx[cwx_idx].rh_out,wx[cwx_idx].dewp_out,wx[cwx_idx].temp_out,wx[cwx_idx].barom,wx[cwx_idx].rain_1hr,computed_rain_day,tm_result->tm_year,tm_result->tm_mon,tm_result->tm_mday,tm_result->tm_hour,tm_result->tm_min,tm_result->tm_sec);
		    }
		  }
		  else {
		    printf("***WUupload failed for %04d-%02d-%02d %02d:%02d:%02d with code %d ",tm_result->tm_year,tm_result->tm_mon,tm_result->tm_mday,tm_result->tm_hour,tm_result->tm_min,tm_result->tm_sec,ccode);
//printf("%s\n",url_buffer);
		  }
/*
clock_gettime(CLOCK_MONOTONIC,&t2);
printf("%f %f %d %d %d %d\n",t1.tv_sec+t1.tv_nsec/1000000000.,t2.tv_sec+t2.tv_nsec/1000000000.,t1.tv_sec,t1.tv_nsec,t2.tv_sec,t2.tv_nsec);
*/
		}
		else {
/*
struct timespec t1,t2;
clock_gettime(CLOCK_MONOTONIC,&t1);
*/
		  FILE *fp;
		  if ( (fp=fopen("/home/wx/current_wx.html","w")) != NULL) {
		    fprintf(fp,"<html><head><meta http-equiv=\"refresh\" content=\"8\"></head><body style=\"font-family: arial,sans-serif\"><h2>Current Weather:</h2><ul><strong>Time:</strong> %04d-%02d-%02d %02d:%02d:%02d UTC<br /><strong>Wind:</strong><ul>%s at %.1f mph<br />Gusts to %.1f mph</ul><strong>Temperature:</strong> %.1f&deg;F<br /><strong>Dewpoint:</strong> %.1f&deg;F<br /><strong>Relative humidity:</strong> %d%<br /><strong>Barometer:</strong> %.2f in Hg<br /><strong>Rain:</strong><ul>One hour: %.2f in<br />Daily: %.2f in</ul></ul><h2>Extremes:</h2><ul><strong>Max temperature:</strong> %.1f&deg;F<br /><strong>Min temperature:</strong> %.1f&deg;F<br /><strong>Max wind speed:</strong> %.1f mph<br /><strong>Max wind gust:</strong> %.1f mph<br /><strong>Max barometer:</strong> %.2f in Hg<br /><strong>Min barometer:</strong> %.2f in Hg</ul></body></html>",tm_result->tm_year,tm_result->tm_mon,tm_result->tm_mday,tm_result->tm_hour,tm_result->tm_min,tm_result->tm_sec,compass[wx[cwx_idx].wdir],wx[cwx_idx].wspd,wx[cwx_idx].wgust,wx[cwx_idx].temp_out,wx[cwx_idx].dewp_out,wx[cwx_idx].rh_out,wx[cwx_idx].barom,wx[cwx_idx].rain_1hr,computed_rain_day,extremes.temp_out_max,extremes.temp_out_min,extremes.wspd_max,extremes.wgust_max,extremes.barom_max*0.02953,extremes.barom_min*0.02953);
		    fclose(fp);
		    if (tid != 0xffffffff) {
			pthread_join(tid,NULL);
		    }
		    int status;
		    if ( (status=pthread_create(&tid,NULL,thread_scp,NULL)) != 0) {
			printf("thread creation error: %d\n",status);
		    }
		  }
/*
clock_gettime(CLOCK_MONOTONIC,&t2);
printf("%f %f %d %d %d %d\n",t1.tv_sec+t1.tv_nsec/1000000000.,t2.tv_sec+t2.tv_nsec/1000000000.,t1.tv_sec,t1.tv_nsec,t2.tv_sec,t2.tv_nsec);
*/
		}
	    }
	    last_WU_upload_time=upload_time;
	  }
	}
	request_weather_message(handle,0,latest_haddr);
	first_sleep.tv_nsec=300000000;
	next_sleep.tv_nsec=10000000;
	break;
    }
    case 0x8:
    {
	if (!backfill_history) {
	  if (curr_hidx == HISTORY_SIZE) {
	    store_history_records();
	    curr_hidx=0;
	  }
	}
	if (decode_history(&buffer[3],&history_queue[curr_hidx])) {
	  print_history(&history_queue[curr_hidx]);
	  if ( (buffer[5] & 0xf) != 0) {
	    printf("***LOW BATTERY*** Console: %d  Temp/Hum: %d  Rain: %d  Wind: %d\n",( (buffer[5] & 0x8) == 0x8),( (buffer[5] & 0x4) == 0x4),( (buffer[5] & 0x2) == 0x2),( (buffer[5] & 0x1) == 0x1));
	  }
	  if (!backfill_history) {
	    if ( (history_queue[curr_hidx].latest_addr-history_queue[curr_hidx].this_addr) >= 0) {
		if (history_queue[curr_hidx].this_addr != history_queue[last_hidx].this_addr) {
		  if (history_queue[curr_hidx].day_num != history_queue[last_hidx].day_num) {
// if the day has changed, reset the daily rainfall to zero
		    rain_day=0.;
// reset total rain base to the current total rain (eliminates -0 that happens
//   for computed rain after nightly reset)
		    rain_total_base=wx[cwx_idx].rain_total;
		  }
		  else {
// otherwise, increment the daily rainfall by the rain amount in the last
//  history period
		    rain_day+=history_queue[curr_hidx].rain_raw;
		  }
		  rain_total_base+=history_queue[curr_hidx].rain_raw;
		  last_hidx=curr_hidx;
		}
		else {
		  curr_hidx=last_hidx;
		}
		latest_haddr=history_queue[curr_hidx].this_addr;
		++curr_hidx;
	    }
	    if (!config_requested) {
		request_get_config(handle,buffer);
	    }
else if (!config_set) {
printf("request config?\n");
request_set_config(handle,buffer,0);
}
	    else {
		sleep(2);
		request_weather_message(handle,5,0xfffff);
	    }
	    first_sleep.tv_nsec=300000000;
	    next_sleep.tv_nsec=10000000;
	    break;
	  }
	  else {
	    if (insert_buffer == NULL) {
		insert_buffer=(char *)malloc(256*sizeof(char));
	    }
	    for (size_t n=0; n < 256; ++n) {
		insert_buffer[n]=0;
	    }
	    sprintf(insert_buffer,HISTORY_INSERT,timestamp(history_queue[curr_hidx].datetime),lround(history_queue[curr_hidx].barom*10.),lround(history_queue[curr_hidx].temp_out*10.),history_queue[curr_hidx].rh_out,history_queue[curr_hidx].wdir,lround(history_queue[curr_hidx].wspd*10.),lround(history_queue[curr_hidx].wgust*10.),lround(history_queue[curr_hidx].rain_raw*100.),history_queue[curr_hidx].this_addr);
	    mysql_query(&mysql,insert_buffer);
	    latest_haddr=history_queue[curr_hidx].this_addr;
	  }
	}
    }
    case 0xa:
    {
	switch (buffer[5] & 0xf) {
	  case 0x1:
	  {
/*
// first config
	buffer[0]=0xd5;
	buffer[1]=0x0;
	buffer[2]=0x9;
	buffer[3]=0xf0;
	buffer[4]=0xf0;
// ask for a config message
	buffer[5]=0x3;
//	buffer[6]=data_buffer[7];
//	buffer[7]=data_buffer[8];
buffer[6]=0x0;
buffer[7]=0x1b;
	buffer[8]=(comm_interval >> 4) & 0xff;
	int history_addr=0xffffff;
	buffer[9]=(history_addr >> 16) & 0x0f | 16*(comm_interval & 0xf);
	buffer[10]=(history_addr >> 8) & 0xff;
	buffer[11]=(history_addr >> 0) & 0xff;
	status=libusb_control_transfer(handle,0x21,0x9,0x3d5,0,data_buffer,273,1000);
printf("first config status %d\n",status);
*/
	    first_sleep.tv_nsec=85000000;
	    next_sleep.tv_nsec=5000000;
	    break;
	  }
	  case 0x2:
	  {
	    set_config(handle);
	    break;
	  }
	  case 0x3:
	  {
	    printf("***CLOCKMESSAGE***\n");
	    set_time(handle,buffer);
	    break;
	  }
	}
	break;
    }
    default:
    {
	printf("***unhandled message type");
	for (size_t n=0; n < 7; ++n) {
	  printf(" %02x",buffer[n]);
	}
	printf("\n");
    }
  }
}

void backfill_history_records(libusb_device_handle *handle,History *history)
{
  if (mysql_username != NULL && mysql_password != NULL) {
    open_mysql();
    time_t t1=time(NULL);
    const char *HISTORY_INSERT="insert into wx.history values(%d,%d,%d,%d,%d,%d,%d,%d,%d) on duplicate key update haddr = values(haddr)";
    char *ibuf=(char *)malloc(256*sizeof(char));
    unsigned char *data_buffer=(unsigned char *)malloc(273*sizeof(unsigned char));
    int status=mysql_query(&mysql,"select haddr from wx.history where timestamp in (select max(timestamp) from wx.history)");
    MYSQL_RES *result=mysql_use_result(&mysql);
    if (result != NULL) {
	MYSQL_ROW row;
	while (row=mysql_fetch_row(result)) {
	  latest_haddr=atoi(row[0]);
	}
    }
    else {
	latest_haddr=0xfffff;
    }
    printf("last history address in the database: %d\n",latest_haddr);
    mysql_free_result(result);
    time_t t=time(NULL);
    struct tm *now=localtime(&t);
    char datetime[20];
    if (now->tm_hour >= 18) {
	sprintf(datetime,"20%02d-%02d-%02d 18:00:00",now->tm_year-100,now->tm_mon+1,now->tm_mday);
    }
    else {
	t-=86400;
	struct tm *yesterday=localtime(&t);
	sprintf(datetime,"20%02d-%02d-%02d 18:00:00",yesterday->tm_year-100,yesterday->tm_mon+1,yesterday->tm_mday);
    }
    datetime[19]='\0';
    for (size_t n=0; n < 256; ++n) {
	ibuf[n]=0;
    }
    sprintf(ibuf,"select max(i_temp_out),min(i_temp_out),max(i_press),min(i_press) from wx.history where timestamp > %d",timestamp(datetime));
printf("%s\n",ibuf);
    status=mysql_query(&mysql,ibuf);
    result=mysql_use_result(&mysql);
    if (result != NULL) {
	MYSQL_ROW row;
	row=mysql_fetch_row(result);
	extremes.temp_out_max=atof(row[0])/10.;
	extremes.temp_out_min=atof(row[1])/10.;
	extremes.barom_max=atof(row[2])/10.;
	extremes.barom_min=atof(row[3])/10.;
    }
    mysql_free_result(result);
    for (size_t n=0; n < 256; ++n) {
	ibuf[n]=0;
    }
    sprintf(ibuf,"select max(i_windspd),max(i_windgust) from wx.history where timestamp > %d and i_windspd != 1141",timestamp(datetime));
printf("%s\n",ibuf);
    status=mysql_query(&mysql,ibuf);
    result=mysql_use_result(&mysql);
    if (result != NULL) {
	MYSQL_ROW row;
	row=mysql_fetch_row(result);
	extremes.wspd_max=atof(row[0])/10.;
	extremes.wgust_max=atof(row[1])/10.;
    }
    mysql_free_result(result);
    size_t num_recs=0;
    history->this_addr=latest_haddr-18;
    history->latest_addr=latest_haddr;
    while (1) {
	wait_for_message(handle);
	int status=libusb_control_transfer(handle,0xa1,0x1,0x3d6,0,data_buffer,273,1000);
	handle_frame(handle,data_buffer,1);
	if ( (data_buffer[5] >> 4) == 0x8) {
	  ++num_recs;
	}
	if (history->this_addr != history->latest_addr) {
	  request_weather_message(handle,0,latest_haddr);
	}
	setTX(handle);
	if (history->this_addr == history->latest_addr) {
	  break;
	}
    }
    latest_haddr=history->latest_addr-18;
    for (size_t n=0; n < 256; ++n) {
	ibuf[n]=0;
    }
    char midnight[20];
    sprintf(midnight,"20%02d-%02d-%02d 00:00:00",now->tm_year-100,now->tm_mon+1,now->tm_mday);
    midnight[19]='\0';
    sprintf(ibuf,"select sum(i_rain)/100. from wx.history where timestamp > %d and timestamp <= %d",timestamp(midnight),t);
printf("query: %s\n",ibuf);
    mysql_query(&mysql,ibuf);
    result=mysql_use_result(&mysql);
    if (result != NULL) {
	MYSQL_ROW row;
	while (row=mysql_fetch_row(result)) {
	  rain_day=atof(row[0]);
	}
    }
    printf("daily rain set to: %.2f\n",rain_day);
    mysql_free_result(result);
    time_t t2=time(NULL);
    printf("%d records processed in %d seconds\n",num_recs,(t2-t1));
    close_mysql();
    free(ibuf);
    ++curr_hidx;
  }
}

char *trim(char *s)
{
  size_t len=strlen(s);
  size_t n=0;
  while (n < len && (s[n] == ' ' || s[n] == 0x9 || s[n] == 0xa || s[n] == 0xd)) {
    ++n;
  }
  int m=len-1;
  while (m > 0 && (s[m] == ' ' || s[m] == 0x9 || s[m] == 0xa || s[m] == 0xd)) {
    s[m--]='\0';
  }
  return &s[n];
}

void read_config()
{
  FILE *fp=fopen("/etc/jadewx/jadewx.conf","r");
  if (fp == NULL) {
    fprintf(stderr,"Unable to open /etc/jadewx/jadewx.conf\n");
    exit(1);
  }
  const size_t LINE_LENGTH=256;
  char line[LINE_LENGTH];
  while (fgets(line,LINE_LENGTH,fp) != NULL) {
    char *tline=trim(line);
    size_t tlen=strlen(tline);
    if (tlen > 0 && tline[0] != '#') {
	char section[LINE_LENGTH];
	if (tline[0] == '[' && tline[tlen-1] == ']') {
	  strncpy(&section[0],&tline[1],tlen-2);
	  section[tlen-2]='\0';
	}
	else {
	  size_t n=0;
	  while (tline[n] != '=' && n < tlen) {
	    ++n;
	  }
	  if (n != tlen) {
	    char name[LINE_LENGTH],value[LINE_LENGTH];
	    strncpy(&name[0],&tline[0],n);
	    name[n]='\0';
	    char *tname=trim(name);
	    strncpy(&value[0],&tline[n+1],tlen-n);
	    value[tlen-n]='\0';
	    char *tvalue=trim(value);
	    if (strcmp(tname,"username") == 0) {
		if (strcmp(section,"mysql") == 0) {
		  mysql_username=(char *)malloc(strlen(tvalue)*sizeof(char));
		  strcpy(mysql_username,tvalue);
		}
	    }
	    else if (strcmp(tname,"password") == 0) {
		if (strcmp(section,"mysql") == 0) {
		  mysql_password=(char *)malloc(strlen(tvalue)*sizeof(char));
		  strcpy(mysql_password,tvalue);
		}
		else if (strcmp(section,"Wunderground") == 0) {
		  WU_password=(char *)malloc(strlen(tvalue)*sizeof(char));
		  strcpy(WU_password,tvalue);
		}
	    }
	    else if (strcmp(tname,"station") == 0) {
		if (strcmp(section,"Wunderground") == 0) {
		  WU_station=(char *)malloc(strlen(tvalue)*sizeof(char));
		  strcpy(WU_station,tvalue);
		}
	    }
	    else if (strcmp(tname,"do_upload") == 0) {
		if (strcmp(section,"Wunderground") == 0) {
		  if (strcmp(tvalue,"true") == 0) {
		    WU_upload=1;
		  }
		}
	    }
	  }
	}
    }
  }
  fclose(fp);
}

size_t handle_curl_response(char *ptr,size_t size,size_t nmemb,void *userdata)
{
  return size*nmemb;
}

int main(int argc,char **argv)
{
  read_config();
  CURLcode ccode;
  if ( (ccode=curl_global_init(CURL_GLOBAL_ALL)) != 0) {
    fprintf(stderr,"unable to initialize CURL\n");
    exit(1);
  }
  curl=curl_easy_init();
  curl_easy_setopt(curl,CURLOPT_TIMEOUT,15);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,handle_curl_response);
  while (1) {
    libusb_context *ctx;
    libusb_init(&ctx);
//    libusb_set_debug(ctx,LIBUSB_LOG_LEVEL_DEBUG);
    libusb_device **list;
    size_t num_devices=libusb_get_device_list(ctx,&list);
printf("num devices: %d\n",num_devices);
    size_t n=0;
    for (n=0; n < num_devices; ++n) {
	struct libusb_device_descriptor desc;
	libusb_get_device_descriptor(list[n],&desc);
	if (desc.idVendor == 0x6666 && desc.idProduct == 0x5555) {
	  break;
	}
    }
    if (n != num_devices) {
	libusb_device_handle *handle;
	int status=libusb_open(list[n],&handle);
	if (status != 0) {
	  printf("unable to open: %s\n",libusb_error_name(status));
	  exit(1);
	}
	status=libusb_detach_kernel_driver(handle,0);
	if (status != 0 && status != LIBUSB_ERROR_NOT_FOUND) {
	  printf("unable to detach kernal driver: %s\n",libusb_error_name(status));
	  exit(1);
	}
	status=libusb_claim_interface(handle,0);
	if (status != 0) {
	  printf("unable to claim interface: %s\n",libusb_error_name(status));
	  exit(1);
	}
printf("found! %d %d\n",n,status);
	status=libusb_control_transfer(handle,0x21,0xa,0,0,NULL,0,1000);
printf("setup status: %d\n",status);

	char serial_num[15];
	initialize_transceiver(handle,serial_num);
	do_setup(handle);

	printf("ready to pair...\n");

	history_queue=(History *)malloc(HISTORY_SIZE*sizeof(History));
	backfill_history_records(handle,&history_queue[curr_hidx]);

	unsigned char *data_buffer=(unsigned char *)malloc(512*sizeof(unsigned char));
	data_buffer[1]=0;
	const char *CURRENT_WX_INSERT="insert into wx.current values(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d) on duplicate key update rh = values(rh)";
	while (1) {
	  wait_for_message(handle);
	  status=libusb_control_transfer(handle,0xa1,0x1,0x3d6,0,data_buffer,273,1000);
	  handle_frame(handle,data_buffer,0);
	  if ( (time(NULL)-last_history_print_time) > 300) {
	    printf("***TRYING TO RE-SYNC...***\n");
	    break;
	  }
	  else {
	    setTX(handle);
	  }
	}

	status=libusb_release_interface(handle,0);
	if (status != 0) {
	  printf("unable to release interface: %s\n",libusb_error_name(status));
	  exit(1);
	}
	status=libusb_attach_kernel_driver(handle,0);
	if (status != 0) {
	  printf("unable to attach kernal driver: %s\n",libusb_error_name(status));
	  exit(1);
	}
	libusb_close(handle);
    }
    libusb_free_device_list(list,1);
    libusb_exit(ctx);
  }
  curl_easy_cleanup(curl);
}
