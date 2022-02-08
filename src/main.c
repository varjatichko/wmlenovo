/*
 *    wmlenovo - A dockapp to monitor ACPI status of one or two batteries,
 *    fan, temperatures and cpu speed.
 *    Copyright (C) 2008  Alexey Ivaniuk aka Varjat <varjat@gmail.com>
 *    Based on work by Florian Krohs <krohs@uni.de>
 *    Copyright (C) 2003  Florian Krohs <krohs@uni.de>

 *    Based on work by Thomas Nemeth <tnemeth@free.fr>
 *    Copyright (C) 2002  Thomas Nemeth <tnemeth@free.fr>
 *    and on work by Seiichi SATO <ssato@sh.rim.or.jp>
 *    Copyright (C) 2001,2002  Seiichi SATO <ssato@sh.rim.or.jp>
 *    and on work by Mark Staggs <me@markstaggs.net>
 *    Copyright (C) 2002  Mark Staggs <me@markstaggs.net>

 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.

 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.

 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "files.h"
#include <signal.h>
#include "dockapp.h"
#include "backlight_on.xpm"
#include "backlight_off.xpm"
#include "parts.xpm"
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifdef linux
#include <sys/stat.h>
#ifdef HAVE_NVML
#include <nvml.h>
#endif
#endif

#ifdef VERSION
#define WMLENOVO_VERSION VERSION
#else
#define WMLENOVO_VERSION "0.1.9"
#endif

#define FREE(data) {if (data) free (data); data = NULL;}

#define RATE_HISTORY 10
#define BLINK_ONLOADING_TIMEOUT 500
#define SIZE	    58
#define MAXSTRLEN   512
#define WINDOWED_BG ". c #AEAAAE"
#define MAX_HISTORY 16
#define CPUNUM_NONE -1

#define CHARGING 3
#define DISCHARGING 1
#define UNKNOWN 0

#define TOGGLEMODE 1
#define TOGGLESPEED 1

#define DEFAULT_UPDATE_INTERVAL 5

#define RATE 0
#define TEMP 1
#define TEMP_V 2
#define TEMP_M 3
#ifdef HAVE_NVML
#define PERF 5
#define TEMP_D 4
#else
#define PERF 4
#endif

#define NONE     0
#define STATE_OK 1
#define INFO_OK  2
#define BAT_OK   3

#ifdef DEBUG
#define DEBUGSTRING(STRING) printf("DEBUG: %s\n",STRING);
#endif

#ifndef DEBUG
#define DEBUGSTRING(STRING)
#endif

# ifdef linux
#  define ACPIDEV "/proc/acpi/info"
# endif

typedef struct AcpiInfos {
  const char	driver_version[10];
  int					ac_line_status;
  int					battery_status[2];
  int					battery_percentage[2];
  long				rate[2];
  long				remain[2];
  long				currcap[2];
  int					thermal_temp;
  int					thermal_state;
  int					hours_left;
  int					minutes_left;
  int					low;
  int					ibm_ctemp;
  int					ibm_vtemp;
  int					ibm_mtemp;
#ifdef HAVE_NVML
  int					ibm_dtemp;
#endif
  int					ibm_fan;
  int					ibm_fan2;
  int					i8k_fan;
  int					i8k_fan2;
  int					speed;
} AcpiInfos;

typedef struct RateListElem {
  long rate[2];
  struct RateListElem *next;	
} RateListElem;

typedef enum { LIGHTOFF, LIGHTON } light;


Pixmap pixmap;
Pixmap backdrop_on;
Pixmap backdrop_off;
Pixmap *parts;
Pixmap bat_backdrop_on;
Pixmap bat_backdrop_off;
Pixmap bat_parts;
Pixmap ac_parts;
Pixmap mask;
static char	*display_name     = "";
static char	light_color[256]  = "#ffaa00";	/* back-light color */
static char	sign_light_color[256]  = "#996633";	/* back-light sign color */
static char	foreground_color[256]  = "#00ff00";	/* foreground color */
static char	background_color[256]  = "#222222";	/* background color */
static char	sign_color[256]  = "#227722";	/* sign color */
static char	foreground_b_color[256]  = "#aa0000";	/* foreground back-light color */
static char	bat_light_color[256]  = "#ff9999";	/* back-light color */
static char	bat_sign_light_color[256]  = "#aa7777";	/* back-light sign color */
static char	bat_foreground_color[256]  = "#ff0000";	/* foreground color */
static char	bat_background_color[256]  = "#222222";	/* background color */
static char	bat_sign_color[256]  = "#772222";	/* sign color */
static char	bat_foreground_b_color[256]  = "#ff0000";	/* foreground back-light color */
char            tmp_string[256];
static char	*config_file      = NULL;	/* name of configfile */
static unsigned update_interval   = DEFAULT_UPDATE_INTERVAL;
static light    backlight         = LIGHTOFF;
static unsigned switch_authorized = True;
static unsigned alarm_level       = 20;
static unsigned alarm_level_temp  = 70;
static char     *notif_cmd        = NULL;
static char     *on_ac_cmd        = NULL;
static char     *on_bat_cmd        = NULL;
static int 	mode			  = TEMP;
static int 	togglemode		  = TOGGLEMODE;
static int 	togglespeed		  = TOGGLESPEED;
static int 	animationspeed    = 500;
static AcpiInfos cur_acpi_infos;
static short int number_of_batteries = 2;
static char state_files[2][256]={BAT0_STATE_FILE,BAT1_STATE_FILE};
static char sysfs_state_files[2][256]={SYS_BAT0_STATE_FILE,SYS_BAT1_STATE_FILE};
static char sysfs_charge_files[2][256]={SYS_BAT0_CHARGE_NOW_FILE,SYS_BAT1_CHARGE_NOW_FILE};
static char sysfs_current_files[2][256]={SYS_BAT0_CURRENT_NOW_FILE,SYS_BAT1_CURRENT_NOW_FILE};
static char sysfs_charge_full_files[2][256]={SYS_BAT0_CHARGE_FULL_FILE,SYS_BAT1_CHARGE_FULL_FILE};
static char info_files[2][256]={BAT0_INFO_FILE,BAT1_INFO_FILE};
static char thermal[256]=THERMAL_FILE;
static char sysfs_thermal[256]=SYS_THERMAL_FILE;
static char sysfs_thermal_v[256]=SYS_THERMAL_V_FILE;
static char sysfs_thermal_m[256]=SYS_THERMAL_M_FILE;
static char ibm_thermal[256]=IBM_TEMP_FILE;
static char ibm_fan[256]=IBM_FAN_FILE;
static char sysfs_ibm_cpu_thermal[256]=SYS_IBM_CPU_TEMP_FILE;
static char sysfs_ibm_gpu_thermal[256]=SYS_IBM_GPU_TEMP_FILE;
static char sysfs_ibm_mb_thermal[256]=SYS_IBM_MB_TEMP_FILE;
static char sysfs_ibm_fan[256]=SYS_IBM_FAN_FILE;
static char i8k[256]=I8K_FILE;
static char ac_state[256]=AC_STATE_FILE;
static char sysfs_ac_state[256]=SYS_AC_STATE_FILE;
static char cpu_speed[256]=CPU_PERF_FILE;
static char sysfs_cpu_speed[256]=SYS_CPU_PERF_FILE;
static int      history_size      = RATE_HISTORY;
static RateListElem *rateElements;
static RateListElem *firstRateElem;
#ifdef HAVE_NVML
static short int max_mode	 = 5;
static short int use_nvml = 0;
static short int alert_nvml = 0;
#else
	static short int max_mode	 = 4;
#endif
static short int has_ibm_fan = 1;
static short int has_i8k = 0;
static short int has_ibm_temp = 1;
static short int has_cpu_file = 1;
static short int use_proc = 0;
static short int use_bat_colors = 0;

#ifdef linux
# ifndef ACPI_32_BIT_SUPPORT
#  define ACPI_32_BIT_SUPPORT      0x0002
# endif
#endif


/* prototypes */
static void parse_config_file(char *config);
static void update();
static void switch_light();
static void draw_remaining_time(AcpiInfos infos);
static void draw_batt(AcpiInfos infos);
static void draw_low();
static void draw_rate(AcpiInfos infos);
static void draw_temp(AcpiInfos infos);
static void draw_statusdigit(AcpiInfos infos);
static void draw_pcgraph(AcpiInfos infos);
static void parse_arguments(int argc, char **argv);
static void print_help(char *prog);
int acpi_getinfos(AcpiInfos *infos);
static void sysfs_getinfos(AcpiInfos *infos);
static int  acpi_exists();
static int  my_system (char *cmd);
static void blink_batt();
static void draw_all();
static void draw_fan();
static void draw_vtemp(AcpiInfos infos);
static void draw_mtemp(AcpiInfos infos);
static void draw_dtemp(AcpiInfos infos);
static void draw_speed(AcpiInfos infos);
static void check_alarm();
static void sig_handler(int s);

static void debug(char *debug_string);
#ifdef linux
int acpi_read(AcpiInfos *i);
int init_stats(AcpiInfos *k);
int ibm_acpi_read(AcpiInfos *i);
int ibm_sysfs_read(AcpiInfos *i);
int ibm_fan_read(AcpiInfos *i);
int sysfs_ibm_fan_read(AcpiInfos *i);
int acpi_speed_read(AcpiInfos *i);
int sysfs_speed_read(AcpiInfos *i);
int i8k_read(AcpiInfos *i);
int sysfs_read(AcpiInfos *i);
#endif

#ifdef HAVE_NVML
static void read_d_temp(AcpiInfos *i);
#endif

int count;
int blink_pos=0;


static void debug(char *debug_string){
  printf("DEBUG: %s\n",debug_string);
}


static void sig_handler(int s){
	printf("Caught signal %d, terminating gracefully\n",s);
#ifdef HAVE_NVML
	nvmlShutdown();
#endif
	exit(0); 
}


int main(int argc, char **argv) {
	
  XEvent   event;
  XpmColorSymbol colors[6] = { {"Back0", NULL, 0}, {"Back1", NULL, 0}, {"Back2", NULL, 0},{"Back3", NULL, 0}, {"Back4", NULL, 0}, {"Back5", NULL, 0}  };
  int      ncolor = 6;
  struct   sigaction sa;
  long counter=0;
  long timeout;
  int charging;
  long togglecounter=0;
  long animationcounter=0;

  sa.sa_handler = SIG_IGN;
#ifdef SA_NOCLDWAIT
  sa.sa_flags = SA_NOCLDWAIT;
#else
  sa.sa_flags = 0;
#endif

	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sig_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;



  printf("wmlenovo %s  (c) Alexey  Varjat\n"
         "<varjat@gmail.com>\n\n"
         "Based on wmbatteries of \n"
         "<florian.krohs@informatik.uni-oldenburg.de>\n\n"
         "This Software comes with absolut no warranty.\n"
         "Use at your own risk!\n\n",WMLENOVO_VERSION);

  sigemptyset(&sa.sa_mask);
  sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGINT, &sigIntHandler, NULL);
	sigaction(SIGTERM, &sigIntHandler, NULL);
	sigaction(SIGQUIT, &sigIntHandler, NULL);

  /* Parse CommandLine */
  parse_arguments(argc, argv);

  /* Check for ACPI support */
  if (!acpi_exists() && use_proc) {
#ifdef linux
    fprintf(stderr, "No ACPI support in kernel\n");
#else
    fprintf(stderr, "Unable to access ACPI info\n");
#endif
    exit(1);
  }

  if (!access(i8k, R_OK)) {
		has_i8k = 1;
	}
	else {
		has_i8k = 0;
	}

#ifdef HAVE_NVML
	if (use_nvml) {
		// Init libnvidia-ml
		nvmlReturn_t res = nvmlInit_v2();
		if (res){
			printf("Unable to init NVML: %s\n",nvmlErrorString(res));
			use_nvml = 0;
		}
	}

#endif

  /* Initialize Application */

  init_stats(&cur_acpi_infos);
  //acpi_getinfos(&cur_acpi_infos);
  //update();
  dockapp_open_window(display_name, PACKAGE, SIZE, SIZE, argc, argv);
  dockapp_set_eventmask(ButtonPressMask);

  colors[0].pixel = dockapp_getcolor(light_color);
   
  colors[1].pixel = dockapp_getcolor(sign_light_color);

  colors[2].pixel = dockapp_getcolor(foreground_color);
  
  colors[3].pixel = dockapp_getcolor(background_color);
  
  colors[4].pixel = dockapp_getcolor(sign_color);
  
	colors[5].pixel = dockapp_getcolor(foreground_b_color);

  /* change raw xpm data to pixmap */
  if (dockapp_iswindowed)
    backlight_on_xpm[1] = backlight_off_xpm[1] = WINDOWED_BG;

  if (!dockapp_xpm2pixmap(backlight_on_xpm, &backdrop_on, &mask, colors, ncolor)) {
    fprintf(stderr, "Error initializing backlit background image.\n");
    exit(1);
  }
  if (!dockapp_xpm2pixmap(backlight_off_xpm, &backdrop_off, &mask, colors, ncolor)) {
    fprintf(stderr, "Error initializing background image.\n");
    exit(1);
  }
  if (!dockapp_xpm2pixmap(parts_xpm, &ac_parts, &mask, colors, ncolor)) {
    fprintf(stderr, "Error initializing parts image.\n");
    exit(1);
  }
	
	if (use_bat_colors) {
  	colors[0].pixel = dockapp_getcolor(bat_light_color);
  	 
  	colors[1].pixel = dockapp_getcolor(bat_sign_light_color);

  	colors[2].pixel = dockapp_getcolor(bat_foreground_color);
  	
  	colors[3].pixel = dockapp_getcolor(bat_background_color);
  	
  	colors[4].pixel = dockapp_getcolor(bat_sign_color);
  	
		colors[5].pixel = dockapp_getcolor(bat_foreground_b_color);


  	if (!dockapp_xpm2pixmap(backlight_on_xpm, &bat_backdrop_on, &mask, colors, ncolor)) {
  	  fprintf(stderr, "Error initializing backlit background image.\n");
  	  exit(1);
  	}
  	if (!dockapp_xpm2pixmap(backlight_off_xpm, &bat_backdrop_off, &mask, colors, ncolor)) {
  	  fprintf(stderr, "Error initializing background image.\n");
  	  exit(1);
  	}
  	if (!dockapp_xpm2pixmap(parts_xpm, &bat_parts, &mask, colors, ncolor)) {
  	  fprintf(stderr, "Error initializing parts image.\n");
  	  exit(1);
		}
	}

	parts = &ac_parts;

  /* shape window */
  if (!dockapp_iswindowed) dockapp_setshape(mask, 0, 0);
  if (mask) XFreePixmap(display, mask);

  /* pixmap : draw area */
  pixmap = dockapp_XCreatePixmap(SIZE, SIZE);

  /* Initialize pixmap */
  if (backlight == LIGHTON) 
    dockapp_copyarea(backdrop_on, pixmap, 0, 0, SIZE, SIZE, 0, 0);
  else
    dockapp_copyarea(backdrop_off, pixmap, 0, 0, SIZE, SIZE, 0, 0);

  dockapp_set_background(pixmap);
  update();
  dockapp_show();
  long update_timeout = update_interval*1000;
  long animation_timeout = animationspeed;
  long toggle_timeout = togglespeed*1000;
  int show = 0;
  /* Main loop */
  while (1) {
    if (cur_acpi_infos.battery_status[0]==CHARGING || cur_acpi_infos.battery_status[1]==CHARGING)
      charging = 1;
    else 
      charging = 0;
    timeout = update_timeout;
    if( charging && animation_timeout<update_timeout){
      if(animation_timeout<toggle_timeout)
	timeout = animation_timeout;
      else if(togglemode) timeout = toggle_timeout;	
    } else if(update_timeout<toggle_timeout)
      timeout = update_timeout;
    else if(togglemode) timeout = toggle_timeout;
    if (dockapp_nextevent_or_timeout(&event, timeout)) {
      /* Next Event */
      switch (event.type) {
      case ButtonPress:
	switch (event.xbutton.button) {
	case 1: switch_light(); break;
	case 3: mode == max_mode ? mode=0: mode++; draw_all();dockapp_copy2window(pixmap);break;
	default: break;
	}
	break;
      default: break;
      }
    } else {
      /* Time Out */
      update_timeout -= timeout;
      animation_timeout -= timeout;
      toggle_timeout -= timeout; 
      if(toggle_timeout<=0){
	toggle_timeout = togglespeed*1000;
	if(togglemode){
	  mode == max_mode ? mode=0 : mode++;
	  show = 1;
	}
      }
      if(animation_timeout<=0){
	animation_timeout = animationspeed;	
	if(charging){
	  blink_batt();
	  show = 1;
	}
      }
      if(update_timeout<=0){
	update();
	show = 1;
	update_timeout = update_interval*1000;
      }            		
      if(show) {
	/* show */
	if (mode==RATE && (charging || !(cur_acpi_infos.battery_status[0] ||  cur_acpi_infos.battery_status[1])) && !(cur_acpi_infos.rate[0] || cur_acpi_infos.rate[1])) {
		mode == max_mode ? mode=0 : mode++;
	}
	check_alarm();
	draw_all();
	if(charging) {
	  blink_pos--;
	  blink_batt();
	}
	dockapp_copy2window(pixmap);
	show = 0;
      }
    }
  }

  return 0;
}


int init_stats(AcpiInfos *k) {
  int bat_status[2]={NONE,NONE};
  FILE *fd;
  char *buf;
  char *ptr;
  char present;
  int bat;
  int hist;
  int i;
	long raw;

  buf=(char *)malloc(sizeof(char)*512);
  if(buf == NULL)
    exit(-1);
  /* get info about existing batteries */
  number_of_batteries=0;
  for(i=0;i<2;i++){
		if (use_proc) {
    	if((fd = fopen(state_files[i], "r"))){
    	  fread(buf,512,1,fd);
    	  fclose(fd);
    	  if(ptr = strstr(buf,"present:")) {
					present=*(ptr+25);
					if(present == 'y'){
	  				bat_status[i]|=STATE_OK;
					}
    	  }
    	  if(ptr = strstr(buf,"present rate:")) {
					present=*(ptr+25);
					sscanf(ptr,"%d",&k->rate[bat]);
    	  }
    	}
    	if((fd = fopen(info_files[i], "r"))){
    	  fread(buf,512,1,fd);
    	  fclose(fd);
    	  if(ptr = strstr(buf,"present:")) {
					present=*(ptr+25);
					if(present == 'y'){
	  				bat_status[i]|=INFO_OK;
					}
    	  }
    	  if(ptr = strstr(buf,"last full capacity:")) {
					present=*(ptr+25);
					sscanf(ptr,"%d",&k->currcap[bat]);
    	  }
    	}
		}
		else {
  		if (!access(sysfs_current_files[i], R_OK)) {
    		if((fd = fopen(sysfs_current_files[i], "r"))){
					fscanf(fd,"%ld",&raw);
					if (raw <= 1000) raw = 0;
					k->rate[i] = raw / 1000;
    	  	fclose(fd);
	  			bat_status[i]|=STATE_OK;
				}
    		if((fd = fopen(sysfs_charge_full_files[i], "r"))){
					fscanf(fd,"%ld",&raw);
					k->currcap[i] = raw / 1000;
    	  	fclose(fd);
	  			bat_status[i]|=INFO_OK;
    	  }
    	}
		}
  }
  if(bat_status[0]==BAT_OK && bat_status[1]==BAT_OK){
    printf("BAT0 and BAT1 ok\n");
    number_of_batteries=2;
  } else if(bat_status[0]==BAT_OK) {
    	printf("BAT0 ok\n");
    	number_of_batteries=1;
  } else if(bat_status[1]==BAT_OK) {
   		printf("BAT1 ok\n");
   		number_of_batteries=1;
   		strcpy(state_files[0],state_files[1]);
   		strcpy(info_files[0],info_files[1]);
   		k->currcap[0] = k->currcap[1];
   		k->rate[0] = k->rate[1];
  }

  printf("%i batter%s found in system\n",number_of_batteries,number_of_batteries==1 ? "y" : "ies");

  if (!number_of_batteries)
  {
    printf("We have no battery information, Skip battery monitoring. :-( ");
		k->currcap[0] = 0;
		k->rate[0] = 0;
		//exit (1);
  }

  // initialize buffer
  if ((rateElements = (RateListElem *) malloc(history_size * sizeof(RateListElem))) == NULL)
    exit(-1);
      
  firstRateElem = rateElements;


  /* get info about full battery charge */

  if (number_of_batteries) for(bat=0;bat<number_of_batteries;bat++){
		if (use_proc) {
    	if ((fd = fopen(info_files[bat], "r"))) {
    	  fread(buf,512,1,fd);
    	  fclose(fd);
    	  if(ptr = strstr(buf,"last full capacity:")) {
					ptr += 25;
					sscanf(ptr,"%d",&k->currcap[bat]);
    	  }
    	} 
    	if ((fd = fopen(state_files[bat], "r"))) {
    	  fread(buf,512,1,fd);
    	  fclose(fd);
    	  if(ptr = strstr(buf,"present rate:")) {
					ptr += 25;
					sscanf(ptr,"%d",&k->rate[bat]);
    	  }
    	}
		}
		else {
    	if((fd = fopen(sysfs_charge_full_files[bat], "r"))){
				fscanf(fd,"%ld",&raw);
				k->currcap[bat] = raw / 1000;
    		fclose(fd);
			}
    	if((fd = fopen(sysfs_current_files[bat], "r"))){
				fscanf(fd,"%ld",&raw);
				if (raw <= 1000) raw = 0;
				k->rate[bat] = raw / 1000;
    		fclose(fd);
    	}
		}
  }
  for(i=0;i<2;i++){
    /* link rateElements */
    for(hist=0;hist<(history_size-1);hist++){
      (*(rateElements+hist)).next = rateElements+hist+1;
      (*(rateElements+hist)).rate[i] = k->rate[i];
    }
    (*(rateElements+history_size-1)).next = rateElements;
    (*(rateElements+history_size-1)).rate[i] = k->rate[i];    
  }
  free(buf);
  k->ac_line_status = 1;
  k->battery_status[0] = 0;
  k->battery_percentage[0] = 0;
  k->remain[0] = 0;
  k->battery_status[1] = 0;
  k->battery_percentage[1] = 0;
  k->remain[1] = 0;
  k->thermal_temp = 0;
  k->thermal_state = 0;
  k->ibm_fan= 0;
  k->ibm_fan2= 0;
  k->i8k_fan= -1;
  k->i8k_fan2= -1;
  k->ibm_ctemp= -1024;
  k->ibm_mtemp= -1024;
#ifdef HAVE_NVML
  k->ibm_dtemp= -1024;
#endif
  k->ibm_vtemp= -1024;
  k->speed=0;
  DEBUGSTRING("end of init_stats()");
}

/* called by timer */
static void update() {

  /* get current battery usage in percent */
	if (use_proc) {
  	if (!acpi_getinfos(&cur_acpi_infos)){
			use_proc=0;
		}
	}
	if (has_i8k) {
		i8k_read(&cur_acpi_infos);
		has_ibm_temp =0;
		has_ibm_fan = 0;
	}
	if (use_proc) {
  	if (has_ibm_fan ) {
		  ibm_fan_read(&cur_acpi_infos);
  	}
  	if (has_ibm_temp) {
		  ibm_acpi_read(&cur_acpi_infos);
  	}
  	if (has_cpu_file) {
		  acpi_speed_read(&cur_acpi_infos);
  	}
	}
	else {
  	sysfs_getinfos(&cur_acpi_infos);
  	if (has_ibm_fan ) {
		  sysfs_ibm_fan_read(&cur_acpi_infos);
  	}
  	if (has_ibm_temp) {
		  ibm_sysfs_read(&cur_acpi_infos);
  	}
  	if (has_cpu_file) {
		  sysfs_speed_read(&cur_acpi_infos);
  	}
	}
#ifdef HAVE_NVML
	read_d_temp(&cur_acpi_infos);
#endif

  draw_all();
}

static void parse_config_file(char *config){

  FILE *fd=NULL;
  char *buf;
  char stringbuffer[256];
  char *ptr;
  char line[256] ;
  char *item;
  char *value;
  int linenr=0;
  int tmp;
  char *test;
  buf=(char *)malloc(sizeof(char)*512);
  if(buf == NULL)
    exit(-1);
  if(config != NULL) { //config file by command line
    DEBUGSTRING("using command line given config file name");
    DEBUGSTRING(config);
    if((fd = fopen(config, "r"))){
      DEBUGSTRING("config file found\n");
    } else {
      DEBUGSTRING("config file NOT found\n");
      DEBUGSTRING("falling back to default config file\n");
    }
  }
  if(fd==NULL) { // no config file found yet
      strcpy(stringbuffer,getenv("HOME"));
      strcat(stringbuffer,"/.wmlenovorc");
      DEBUGSTRING("trying config file in your $HOME dir\n");
      DEBUGSTRING(stringbuffer);
      if((fd = fopen(stringbuffer, "r"))){
    	DEBUGSTRING("config file found\n");
      } else {
	DEBUGSTRING("config file in $HOME dir nonexistant\n");
	DEBUGSTRING("trying global one in /etc\n");	
	if((fd = fopen("/etc/wmlenovo", "r"))){
	  DEBUGSTRING("config file found\n");
	}
	else {
	  DEBUGSTRING("no config file found. ignoring\n");
	}
      }
    }

  if(fd!=NULL){ // some config file was found, try parsing
    DEBUGSTRING("begin parsing\n");
    while( fgets( line, 255, fd ) != NULL )
    {
      
        item = strtok( line, "\t =\n\r" ) ;
        if( item != NULL && item[0] != '#' )
        {
            value = strtok( NULL, "\t =\n\r" ) ;
	    if(!strcmp(item,"backlight")){
	      if(strcasecmp(value,"yes") && strcasecmp(value,"true") && strcasecmp(value,"false") && strcasecmp(value,"no")) {
		printf("backlight option wrong in line %i,use yes/no or true/false\n",linenr);
	      } else {
		if(!strcasecmp(value,"true") || !strcasecmp(value,"yes")){
		  backlight = LIGHTON;
		} else {
		  backlight = LIGHTOFF;
		}
	      }	    
	    }
	    if(!strcmp(item,"use-proc")){
				use_proc=1;
			}

	    if(!strcmp(item,"use-bat-color")){
				use_bat_colors=1;
			}

	    if(!strcmp(item,"dell")){
				has_i8k=1;
			}

	    if(!strcmp(item,"ibm")){
				has_ibm_temp = 1;
				has_ibm_fan = 1;
			}

	    if(!strcmp(item,"lightcolor")){
	      strcpy(light_color,value);
	    }
	    
	    if(!strcmp(item,"sign-light-color")){
	      strcpy(sign_light_color,value);
	    }
	    
	    if(!strcmp(item,"foreground-color")){
	      strcpy(foreground_color,value);
	    }
	    
	    if(!strcmp(item,"backlight-foreground-color")){
	      strcpy(foreground_b_color,value); 
	    }
	    
	    if(!strcmp(item,"background-color")){
	      strcpy(background_color,value);
	    }
	    
	    if(!strcmp(item,"sign-color")){
	      strcpy(sign_color,value);
	    }

	    if(!strcmp(item,"bat-lightcolor")){
	      strcpy(bat_light_color,value);
	    }
	    
	    if(!strcmp(item,"bat-sign-light-color")){
	      strcpy(bat_sign_light_color,value);
	    }
	    
	    if(!strcmp(item,"bat-foreground-color")){
	      strcpy(bat_foreground_color,value);
	    }
	    
	    if(!strcmp(item,"bat-backlight-foreground-color")){
	      strcpy(bat_foreground_b_color,value); 
	    }
	    
	    if(!strcmp(item,"bat-background-color")){
	      strcpy(bat_background_color,value);
	    }
	    
	    if(!strcmp(item,"bat-sign-color")){
	      strcpy(bat_sign_color,value);
	    }

	    if(!strcmp(item,"acpi_temperature")){
	      strcpy(thermal,value);
	    }

	    if(!strcmp(item,"acpi_bat0_state")){
	      strcpy(state_files[0],value);
	    }

	    if(!strcmp(item,"acpi_bat1_state")){
	      strcpy(state_files[1],value);
	    }

	    if(!strcmp(item,"acpi_bat0_info")){
	      strcpy(info_files[0],value);
	    }

	    if(!strcmp(item,"acpi_bat1_info")){
	      strcpy(info_files[1],value);
	    }

	    if(!strcmp(item,"acpi_ac_state")){
	      strcpy(ac_state,value);
	    }
	    
			if(!strcmp(item,"i8k")){
	      strcpy(i8k,value);
	    }
			
	    if(!strcmp(item,"sysfs_bat0_state")){
	      strcpy(sysfs_state_files[0],value);
	    }

	    if(!strcmp(item,"sysfs_bat1_state")){
	      strcpy(sysfs_state_files[1],value);
	    }

	    if(!strcmp(item,"sysfs_bat0_charge_now")){
	      strcpy(sysfs_charge_files[0],value);
	    }

	    if(!strcmp(item,"sysfs_bat1_charge_now")){
	      strcpy(sysfs_charge_files[1],value);
	    }
	    
			if(!strcmp(item,"sysfs_bat0_charge_full")){
	      strcpy(sysfs_charge_full_files[0],value);
	    }

	    if(!strcmp(item,"sysfs_bat1_charge_full")){
	      strcpy(sysfs_charge_full_files[1],value);
	    }

	    if(!strcmp(item,"sysfs_bat0_current_now")){
	      strcpy(sysfs_current_files[0],value);
	    }

	    if(!strcmp(item,"sysfs_bat1_current_now")){
	      strcpy(sysfs_current_files[1],value);
	    }

	    if(!strcmp(item,"sysfs_temperature")){
	      strcpy(sysfs_thermal,value);
	    }

	    if(!strcmp(item,"sysfs_video_temperature")){
	      strcpy(sysfs_thermal_v,value);
	    }

	    if(!strcmp(item,"sysfs_mb_temperature")){
	      strcpy(sysfs_thermal_m,value);
	    }
	    
			if(!strcmp(item,"sysfs_ibm_temperature")){
	      strcpy(sysfs_ibm_cpu_thermal,value);
	    }

	    if(!strcmp(item,"sysfs_ibm_video_temperature")){
	      strcpy(sysfs_ibm_gpu_thermal,value);
	    }

	    if(!strcmp(item,"sysfs_ibm_mb_temperature")){
	      strcpy(sysfs_ibm_mb_thermal,value);
	    }

	    if(!strcmp(item,"sysfs_ibm_fan")){
	      strcpy(sysfs_ibm_fan,value);
	    }

	    if(!strcmp(item,"sysfs_ac_state")){
	      strcpy(sysfs_ac_state,value);
	    }
	    
	    if(!strcmp(item,"sysfs_cpu_performance")){
	      strcpy(sysfs_cpu_speed,value);
	    }

	    if(!strcmp(item,"notify")){
  			notif_cmd=(char *)malloc(sizeof(char)*512);
	      strcpy(notif_cmd,value);
	    }

	    if(!strcmp(item,"on-ac")){
  			on_ac_cmd=(char *)malloc(sizeof(char)*512);
	      strcpy(on_ac_cmd,value);
	    }

	    if(!strcmp(item,"on-bat")){
  			on_bat_cmd=(char *)malloc(sizeof(char)*512);
	      strcpy(on_bat_cmd,value);
	    }
#ifdef HAVE_NVML
	    if(!strcmp(item,"use-nvidia")){
				use_nvml=1;
			}
	    if(!strcmp(item,"nvidia-alarm-on-throttling")){
				alert_nvml=1;
			}
#endif



	    if(!strcmp(item,"updateinterval")){
	      tmp=atoi(value);
	      if(tmp<1) {
		printf("update interval is out of range in line %i,must be > 0\n",linenr);
	      } else {
		update_interval=tmp;
	      }
	    }

	    if(!strcmp(item,"alarm")){
	      tmp=atoi(value);
	      if(tmp<1 || tmp>190) {
		printf("alarm is out of range in line %i,must be > 0 and <= 190\n",linenr);
	      } else {
		alarm_level=tmp;
	      }
	    }

	    if(!strcmp(item,"alarm_temp")){
	      tmp=atoi(value);
	      if(tmp<1 || tmp>190) {
		printf("alarm is out of range in line %i,must be > 0 and <= 190\n",linenr);
	      } else {
		alarm_level_temp=tmp;
	      }
	    }

	    if(!strcmp(item,"togglespeed")){
	      tmp=atoi(value);
	      if(tmp<1) {
		printf("togglespeed variable is out of range in line %i,must be > 0\n",linenr);
	      } else {
		togglespeed=tmp;
	      }
	    }

	    if(!strcmp(item,"animationspeed")){
	      tmp=atoi(value);
	      if(tmp<100) {
		printf("animationspeed variable is out of range in line %i,must be >= 100\n",linenr);
	      } else { 
		animationspeed=tmp;
	      }
	    }

	    if(!strcmp(item,"historysize")){
	      tmp=atoi(value);
	      if(tmp<1 || tmp>1000) {
		printf("historysize variable is out of range in line %i,must be >=1 and <=1000\n",linenr);
	      } else { 
		history_size=tmp;
	      }
	    }

	    if(!strcmp(item,"mode")){
#ifdef HAVE_NVML
	      if(strcmp(value,"rate") && strcmp(value,"toggle") && strcmp(value,"temp") && strcmp(value,"v_temp") && strcmp(value,"m_temp") && strcmp(value,"v_temp") && strcmp(value,"d_temp")) {
		printf("mode must be one of rate,temp,v_temp,m_temp,d_temp,speed,toggle in line %i\n",linenr);
#else
	      if(strcmp(value,"rate") && strcmp(value,"toggle") && strcmp(value,"temp") && strcmp(value,"v_temp") && strcmp(value,"m_temp") && strcmp(value,"v_temp")) {
		printf("mode must be one of rate,temp,v_temp,m_temp,speed,toggle in line %i\n",linenr);
#endif
	      } else { 
		if(strcmp(value,"rate")) mode=RATE;
		if(strcmp(value,"temp")) mode=TEMP;
		if(strcmp(value,"v_temp")) mode=TEMP_V;
		if(strcmp(value,"m_temp")) mode=TEMP_M;
#ifdef HAVE_NVML
		if(strcmp(value,"d_temp")) mode=TEMP_D;
#endif
		if(strcmp(value,"speed")) mode=PERF;
		if(strcmp(value,"toggle")) togglemode=1;
	      }
	    }



        }
	linenr++;
    }
    fclose(fd);
    DEBUGSTRING("end parsing\n");
  }
}


static void draw_all(){
  int bat;
  long allremain=0;
  long allcapacity=0;

  /* all clear */
  if (backlight == LIGHTON)
	 	if (use_bat_colors && !cur_acpi_infos.ac_line_status){
    	dockapp_copyarea(bat_backdrop_on, pixmap, 0, 0, 58, 58, 0, 0);
		} else {
    	dockapp_copyarea(backdrop_on, pixmap, 0, 0, 58, 58, 0, 0);
		}
  else 
	 	if (use_bat_colors && !cur_acpi_infos.ac_line_status){
    	dockapp_copyarea(bat_backdrop_off, pixmap, 0, 0, 58, 58, 0, 0);
		} else {
    	dockapp_copyarea(backdrop_off, pixmap, 0, 0, 58, 58, 0, 0);
		}
  /* draw digit */
  draw_remaining_time(cur_acpi_infos);
  switch (mode) {
	case RATE:
		draw_rate(cur_acpi_infos);
		break;
  case TEMP:
		draw_temp(cur_acpi_infos);
		break;
	case TEMP_V:
		if (cur_acpi_infos.ibm_vtemp > -1024) {
			draw_vtemp(cur_acpi_infos);
			break;
		}
		mode++;
	case TEMP_M:
		if (cur_acpi_infos.ibm_mtemp > -1024) {
			draw_mtemp(cur_acpi_infos);
			break;
		}
		mode++;
#ifdef HAVE_NVML
	case TEMP_D:
		if (use_nvml && cur_acpi_infos.ibm_dtemp > -1024) {
			draw_dtemp(cur_acpi_infos);
			break;
		}
		mode++;
#endif
	case PERF:
		draw_speed(cur_acpi_infos);
		break;
  }
  draw_statusdigit(cur_acpi_infos);
  draw_pcgraph(cur_acpi_infos);
  if (cur_acpi_infos.ibm_fan != -1 || cur_acpi_infos.ibm_fan2 != -1) {
		draw_fan(cur_acpi_infos);
	}

  if(cur_acpi_infos.low) draw_low();
  
  draw_batt(cur_acpi_infos);
}


/* called when mouse button pressed */

static void switch_light() {
  switch (backlight) {
  case LIGHTOFF:
    backlight = LIGHTON;
	 	if (use_bat_colors && !cur_acpi_infos.ac_line_status){
    	dockapp_copyarea(bat_backdrop_on, pixmap, 0, 0, 58, 58, 0, 0);
		} else {
    	dockapp_copyarea(backdrop_on, pixmap, 0, 0, 58, 58, 0, 0);
		}
    break;
  case LIGHTON:
    backlight = LIGHTOFF;
	 	if (use_bat_colors && !cur_acpi_infos.ac_line_status){
    	dockapp_copyarea(bat_backdrop_off, pixmap, 0, 0, 58, 58, 0, 0);
		} else {
    	dockapp_copyarea(backdrop_off, pixmap, 0, 0, 58, 58, 0, 0);
		}
    break;
  }

  draw_remaining_time(cur_acpi_infos);
  draw_rate(cur_acpi_infos);
  draw_statusdigit(cur_acpi_infos);
  draw_pcgraph(cur_acpi_infos);
  if(cur_acpi_infos.battery_status[0]==CHARGING || cur_acpi_infos.battery_status[1]==CHARGING){
    blink_batt();
  } else draw_batt(cur_acpi_infos);
  if(cur_acpi_infos.low){
    draw_low();
  }
  /* show */
  dockapp_copy2window(pixmap);
}

static void draw_batt(AcpiInfos infos){
  int y = 0;
  int i=0;
  if (backlight == LIGHTON) y = 28;
  if (number_of_batteries) for(i=0;i<number_of_batteries;i++){
    if(infos.battery_status[i]==DISCHARGING){	
      dockapp_copyarea(*parts, pixmap, 33+y , 63, 9, 5, 16+i*11, 39);
    }
  }
}

static void draw_remaining_time(AcpiInfos infos) {
  int y = 0;
  if (backlight == LIGHTON) y = 20;
  if (infos.ac_line_status == 1 && !(cur_acpi_infos.rate[0] || cur_acpi_infos.rate[1])){
    dockapp_copyarea(*parts, pixmap, 0, 68+68+y, 10, 20,  17, 5);
    dockapp_copyarea(*parts, pixmap, 10, 68+68+y, 10, 20,  32, 5);
  } else {
    dockapp_copyarea(*parts, pixmap, (infos.hours_left / 10) * 10, 68+y, 10, 20,  5, 5);
    dockapp_copyarea(*parts, pixmap, (infos.hours_left % 10) * 10, 68+y, 10, 20, 17, 5);
    dockapp_copyarea(*parts, pixmap, (infos.minutes_left / 10)  * 10, 68+y, 10, 20, 32, 5);
    dockapp_copyarea(*parts, pixmap, (infos.minutes_left % 10)  * 10, 68+y, 10, 20, 44, 5);
  }
}

static void draw_low() {
  int y = 0;
  if (backlight == LIGHTON) y = 28;
  dockapp_copyarea(*parts, pixmap,42+y , 58, 17, 7, 38, 38);
}

static void draw_temp(AcpiInfos infos) {
  int temp = 0;
  int light_offset=0;
  if (infos.ibm_ctemp > -1024 ) temp = infos.ibm_ctemp;
	  else temp = infos.thermal_temp;
  if (backlight == LIGHTON) {
    light_offset=50;
  }

  if (temp < -99 || temp>199)  temp = 0;
  if (temp < 0) dockapp_copyarea(*parts, pixmap, 95 + light_offset/10, 58, 5, 9, 17, 46);
  if (temp > 99) dockapp_copyarea(*parts, pixmap, ((temp/100)%10)*5 + light_offset, 40, 5, 9, 17, 46);
  dockapp_copyarea(*parts, pixmap, ((abs(temp)/10)%10)*5 + light_offset, 40, 5, 9, 23, 46);
  dockapp_copyarea(*parts, pixmap, (abs(temp)%10)*5 + light_offset, 40, 5, 9, 29, 46);

  dockapp_copyarea(*parts, pixmap, 10 + light_offset, 49, 5, 9, 36, 46);  //o
  dockapp_copyarea(*parts, pixmap, 15 + light_offset, 49, 5, 9, 42, 46);  //C
  dockapp_copyarea(*parts, pixmap, 35 + light_offset, 49, 5, 9, 48, 46);  //c
}

static void draw_vtemp(AcpiInfos infos) {
  int temp = infos.ibm_vtemp;
  int light_offset=0;
  if (backlight == LIGHTON) {
    light_offset=50;
  }

  if (temp < -99 || temp>199)  temp = 0;
  if (temp < 0) dockapp_copyarea(*parts, pixmap, 94 + light_offset/10, 58, 5, 9, 17, 46);
  if (temp > 99) dockapp_copyarea(*parts, pixmap, ((temp/100)%10)*5 + light_offset, 40, 5, 9, 17, 46);
  dockapp_copyarea(*parts, pixmap, ((abs(temp)/10)%10)*5 + light_offset, 40, 5, 9, 23, 46);
  dockapp_copyarea(*parts, pixmap, (abs(temp)%10)*5 + light_offset, 40, 5, 9, 29, 46);

  dockapp_copyarea(*parts, pixmap, 10 + light_offset, 49, 5, 9, 36, 46);  //o
  dockapp_copyarea(*parts, pixmap, 15 + light_offset, 49, 5, 9, 42, 46);  //C
  dockapp_copyarea(*parts, pixmap, 40 + light_offset, 49, 5, 9, 48, 46);  //v
}

static void draw_mtemp(AcpiInfos infos) {
  int temp = infos.ibm_mtemp;
  int light_offset=0;
  if (backlight == LIGHTON) {
    light_offset=50;
  }

  if (temp < -99 || temp>199)  temp = 0;
  if (temp < 0) dockapp_copyarea(*parts, pixmap, 94 + light_offset/10, 58, 5, 9, 17, 46);
  if (temp > 99) dockapp_copyarea(*parts, pixmap, ((temp/100)%10)*5 + light_offset, 40, 5, 9, 17, 46);
  dockapp_copyarea(*parts, pixmap, ((abs(temp)/10)%10)*5 + light_offset, 40, 5, 9, 23, 46);
  dockapp_copyarea(*parts, pixmap, (abs(temp)%10)*5 + light_offset, 40, 5, 9, 29, 46);

  dockapp_copyarea(*parts, pixmap, 10 + light_offset, 49, 5, 9, 36, 46);  //o
  dockapp_copyarea(*parts, pixmap, 15 + light_offset, 49, 5, 9, 42, 46);  //C
  dockapp_copyarea(*parts, pixmap, light_offset, 49, 5, 9, 48, 46);  //m
}

#ifdef HAVE_NVML
static void draw_dtemp(AcpiInfos infos) {
  int temp = infos.ibm_dtemp;
  int light_offset=0;
  if (backlight == LIGHTON) {
    light_offset=50;
  }

  if (temp < -99 || temp>199)  temp = 0;
  if (temp < 0) dockapp_copyarea(*parts, pixmap, 94 + light_offset/10, 58, 5, 9, 17, 46);
  if (temp > 99) dockapp_copyarea(*parts, pixmap, ((temp/100)%10)*5 + light_offset, 40, 5, 9, 17, 46);
  dockapp_copyarea(*parts, pixmap, ((abs(temp)/10)%10)*5 + light_offset, 40, 5, 9, 23, 46);
  dockapp_copyarea(*parts, pixmap, (abs(temp)%10)*5 + light_offset, 40, 5, 9, 29, 46);

  dockapp_copyarea(*parts, pixmap, 10 + light_offset, 49, 5, 9, 36, 46);  //o
  dockapp_copyarea(*parts, pixmap, 15 + light_offset, 49, 5, 9, 42, 46);  //C
  dockapp_copyarea(*parts, pixmap, 45 + light_offset, 49, 5, 9, 48, 46);  //d
}
#endif

static void blink_batt(){
  int light_offset=0;
  int bat=0;
  if (backlight == LIGHTON) {
    light_offset=50;
  }
  blink_pos=(blink_pos+1)%5;
  if (number_of_batteries) for(bat=0;bat<number_of_batteries;bat++){
    if(cur_acpi_infos.battery_status[bat]==CHARGING){
      dockapp_copyarea(*parts, pixmap, blink_pos*9+light_offset , 117, 9, 5,  16+bat*11, 39);
    }
  }    
}


static void draw_statusdigit(AcpiInfos infos) {
  int light_offset=0;
  if (backlight == LIGHTON) {
    light_offset=28;
  }
  if (infos.ac_line_status == 1){
    dockapp_copyarea(*parts, pixmap,33+light_offset , 58, 9, 5, 5, 39);
  }
}

static void draw_rate(AcpiInfos infos) {
  int light_offset=0;
  long rate = infos.rate[0]+infos.rate[1];
  if (backlight == LIGHTON) {
    light_offset=50;
  }

  if (rate > 9999) dockapp_copyarea(*parts, pixmap, (rate/10000)*5 + light_offset, 40, 5, 9, 5, 46);
  if (rate > 999) dockapp_copyarea(*parts, pixmap, ((rate/1000)%10)*5 + light_offset, 40, 5, 9, 11, 46);
  if (rate > 99) dockapp_copyarea(*parts, pixmap, ((rate/100)%10)*5 + light_offset, 40, 5, 9, 17, 46);
  if (rate > 9) dockapp_copyarea(*parts, pixmap, ((rate/10)%10)*5 + light_offset, 40, 5, 9, 23, 46);
  dockapp_copyarea(*parts, pixmap, (rate%10)*5 + light_offset, 40, 5, 9, 29, 46);

  dockapp_copyarea(*parts, pixmap, 0 + light_offset, 49, 5, 9, 36, 46);  //m
  dockapp_copyarea(*parts, pixmap, 5 + light_offset, 49, 5, 9, 42, 46);  //W

}

static void draw_speed(AcpiInfos infos) {
  int light_offset=0;
  long rate = infos.speed;
  if (backlight == LIGHTON) {
    light_offset=50;
  }

  if (rate > 999 ) dockapp_copyarea(*parts, pixmap, ((rate/1000)%10)*5 + light_offset, 40, 5, 9, 11, 46);
  if (rate > 99 ) dockapp_copyarea(*parts, pixmap, ((rate/100)%10)*5 + light_offset, 40, 5, 9, 17, 46);
  if (rate > 9 ) dockapp_copyarea(*parts, pixmap, ((rate/10)%10)*5 + light_offset, 40, 5, 9, 23, 46);
  dockapp_copyarea(*parts, pixmap, (rate%10)*5 + light_offset, 40, 5, 9, 29, 46);

  dockapp_copyarea(*parts, pixmap, 20 + light_offset, 49, 5, 9, 36, 46);  //M
  dockapp_copyarea(*parts, pixmap, 25 + light_offset, 49, 5, 9, 42, 46);  //H
  dockapp_copyarea(*parts, pixmap, 30 + light_offset, 49, 5, 9, 48, 46);  //z

}

static void draw_pcgraph(AcpiInfos infos) {
  int num[2];
  int bat;
  int width=0;
  int light_offset=0;
  int percentage=0;
  if (backlight == LIGHTON) {
    light_offset=5;
  }
  if (number_of_batteries){ 
		for(bat=0;bat<number_of_batteries;bat++){
    	percentage+= (infos.battery_percentage[bat]);
  	}
  	width=percentage * 32 / 100 / number_of_batteries;
  	percentage/=number_of_batteries;
	}
		else percentage=0;
    dockapp_copyarea(*parts, pixmap, 0, 58+light_offset, width, 5, 4, 26);
    if(percentage == 100){ // don't display leading 0
    	dockapp_copyarea(*parts, pixmap, 4*(percentage/100), 126+light_offset, 3, 5, 37, 26);
    }
    if(percentage > 9){ //don't display leading 0
     dockapp_copyarea(*parts, pixmap, 4*((percentage%100)/10), 126+light_offset, 3, 5, 41, 26);
    }
	dockapp_copyarea(*parts, pixmap, 4*(percentage%10), 126+light_offset, 3, 5, 45, 26);		
}

static void draw_fan(AcpiInfos infos) {
  int width=0;
  int height=3;
  int light_offset=0;
  if (backlight == LIGHTON) {
    light_offset=5;
  }
	if (infos.ibm_fan2 > 0)
	{
		height=1;
  	width=infos.ibm_fan2 * 30 / 6000;
    if (width < 31 ) dockapp_copyarea(*parts, pixmap, 0, 58+light_offset, width, height, 4, 35);
	}
  width=infos.ibm_fan * 30 / 6000;
    if (width < 31 ) dockapp_copyarea(*parts, pixmap, 0, 58+light_offset, width, height, 4, 33);
  if (backlight == LIGHTON) {
    light_offset=50;
  }
    if(infos.ibm_fan > 999 ){ // don't display leading 0
      dockapp_copyarea(*parts, pixmap, 5*(infos.ibm_fan/1000)+light_offset, 176, 4, 5, 36, 32);
    }
    if(infos.ibm_fan > 99){ //don't display leading 0
      dockapp_copyarea(*parts, pixmap, 5*((infos.ibm_fan%1000)/100)+light_offset, 176, 4, 5, 41, 32);
    }
    if(infos.ibm_fan > 9){ //don't display leading 0
      dockapp_copyarea(*parts, pixmap, 5*((infos.ibm_fan%100)/10)+light_offset, 176, 4, 5, 46, 32);
    }
    dockapp_copyarea(*parts, pixmap, 5*(infos.ibm_fan%10)+light_offset, 176, 4, 5, 51, 32);		
}

static void parse_arguments(int argc, char **argv) {
  int i;
  int integer;
  char character;

  for (i = 1; i < argc; i++) { // first search for config file option
    if (!strcmp(argv[i], "--config") || !strcmp(argv[i], "-c")) {
      config_file = argv[i + 1];
      i++;
    }
  }
  // parse config file before other command line options, to allow overriding
  parse_config_file(config_file);
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
      print_help(argv[0]), exit(0);
    } else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
      printf("%s version %s\n", PACKAGE, VERSION), exit(0);
    } else if (!strcmp(argv[i], "--display") || !strcmp(argv[i], "-d")) {
      display_name = argv[i + 1];
      i++;
    } else if (!strcmp(argv[i], "--backlight") || !strcmp(argv[i], "-bl")) {
      backlight = LIGHTON;
    } else if (!strcmp(argv[i], "--use-proc"))  {
      use_proc = 1;
#ifdef HAVE_NVML
    } else if (!strcmp(argv[i], "--use-nvidia"))  {
      use_nvml = 1;
    } else if (!strcmp(argv[i], "--alarm-nvidia"))  {
      alert_nvml = 1;
#endif
    } else if (!strcmp(argv[i], "--use-bat-color") || !strcmp(argv[i], "-sc")) {
      use_bat_colors = 1;
    } else if (!strcmp(argv[i], "--light-color") || !strcmp(argv[i], "-lc")) {
      strcpy(light_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--sign-light-color") || !strcmp(argv[i], "-nc")) {
      strcpy(sign_light_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--foreground-color") || !strcmp(argv[i], "-fc")) {
      strcpy(foreground_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--backlight-foreground-color") || !strcmp(argv[i], "-rc")) {
      strcpy(foreground_b_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--background-color") || !strcmp(argv[i], "-bc")) {
      strcpy(background_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--sign-color") || !strcmp(argv[i], "-sc")) {
      strcpy(sign_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--bat-light-color") || !strcmp(argv[i], "-lb")) {
      strcpy(bat_light_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--bat-sign-light-color") || !strcmp(argv[i], "-nb")) {
      strcpy(bat_sign_light_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--bat-foreground-color") || !strcmp(argv[i], "-fb")) {
      strcpy(bat_foreground_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--bat-backlight-foreground-color") || !strcmp(argv[i], "-rb")) {
      strcpy(bat_foreground_b_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--bat-background-color") || !strcmp(argv[i], "-bb")) {
      strcpy(bat_background_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--bat-sign-color") || !strcmp(argv[i], "-sb")) {
      strcpy(bat_sign_color,argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "--config") || !strcmp(argv[i], "-c")) {
      config_file = argv[i + 1];
      i++;
    } else if (!strcmp(argv[i], "--interval") || !strcmp(argv[i], "-i")) {
      if (argc == i + 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (sscanf(argv[i + 1], "%i", &integer) != 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (integer < 1)
	fprintf(stderr, "%s: argument %s must be >=1\n",
		argv[0], argv[i]), exit(1);
      update_interval = integer;
      i++;
    } else if (!strcmp(argv[i], "--alarm") || !strcmp(argv[i], "-a")) {
      if (argc == i + 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (sscanf(argv[i + 1], "%i", &integer) != 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if ( (integer < 0) || (integer > 190) )
	fprintf(stderr, "%s: argument %s must be >=0 and <=190\n",
		argv[0], argv[i]), exit(1);
      alarm_level = integer;
      i++;
    } else if (!strcmp(argv[i], "--alarm-temp") || !strcmp(argv[i], "-at")) {
      if (argc == i + 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (sscanf(argv[i + 1], "%i", &integer) != 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if ( (integer < 0) || (integer > 190) )
	fprintf(stderr, "%s: argument %s must be >=0 and <=190\n",
		argv[0], argv[i]), exit(1);
      alarm_level_temp = integer;
      i++;
    } else if (!strcmp(argv[i], "--windowed") || !strcmp(argv[i], "-w")) {
      dockapp_iswindowed = True;
    } else if (!strcmp(argv[i], "--broken-wm") || !strcmp(argv[i], "-bw")) {
      dockapp_isbrokenwm = True;
    } else if (!strcmp(argv[i], "--notify") || !strcmp(argv[i], "-n")) {
      notif_cmd = argv[i + 1];
      i++;
    } else if (!strcmp(argv[i], "--on-bat") || !strcmp(argv[i], "-ob")) {
      on_bat_cmd = argv[i + 1];
      i++;
    } else if (!strcmp(argv[i], "--on-ac") || !strcmp(argv[i], "-oa")) {
      on_ac_cmd = argv[i + 1];
      i++;
    } else if (!strcmp(argv[i], "--thermal_file") || !strcmp(argv[i], "-tc")) {
      strncpy(sysfs_thermal, argv[i + 1], 255);
      i++;
    } else if (!strcmp(argv[i], "--thermal_video_file") || !strcmp(argv[i], "-tv")) {
      strncpy(sysfs_thermal_v, argv[i + 1], 255);
      i++;
    } else if (!strcmp(argv[i], "--thermal_mb_file") || !strcmp(argv[i], "-tm")) {
      strncpy(sysfs_thermal_m, argv[i + 1], 255);
      i++;
    } else if (!strcmp(argv[i], "--togglespeed") || !strcmp(argv[i], "-ts")) {
      if (argc == i + 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (sscanf(argv[i + 1], "%i", &integer) != 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if ( integer < 1)
	fprintf(stderr, "%s: argument %s must be positive integer\n",
		argv[0], argv[i],update_interval), exit(1);
      togglespeed=integer;                        
      i++;                        
    } else if (!strcmp(argv[i], "--animationspeed") || !strcmp(argv[i], "-as")) {
      if (argc == i + 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (sscanf(argv[i + 1], "%i", &integer) != 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (integer < 100)
	fprintf(stderr, "%s: argument %s must be >=100\n",
		argv[0], argv[i]), exit(1);
      animationspeed=integer;            
      i++;                                                                       
    } else if (!strcmp(argv[i], "--historysize") || !strcmp(argv[i], "-hs")) {
      if (argc == i + 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (sscanf(argv[i + 1], "%i", &integer) != 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (integer < 1 || integer > 1000)
	fprintf(stderr, "%s: argument %s must be >=1 && <=1000\n",
		argv[0], argv[i]), exit(1);
      history_size=integer;
      i++;                                                                       
    } else if (!strcmp(argv[i], "--mode") || !strcmp(argv[i], "-m")) {
      if (argc == i + 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
      if (sscanf(argv[i + 1], "%c", &character) != 1)
	fprintf(stderr, "%s: error parsing argument for option %s\n",
		argv[0], argv[i]), exit(1);
#ifdef HAVE_NVML
      if (!(character=='t' || character=='r' || character=='s' || character=='v' || character=='m' || character=='p' || character=='d'))
	fprintf(stderr, "%s: argument %s must be t,r,v,m,d,p or s\n",
		argv[0], argv[i]), exit(1);
#else
      if (!(character=='t' || character=='r' || character=='s' || character=='v' || character=='m' || character=='p'))
	fprintf(stderr, "%s: argument %s must be t,r,v,m,p or s\n",
		argv[0], argv[i]), exit(1);
#endif
      if(character=='s') togglemode=1;
      else if(character=='t') mode=TEMP;
      else if(character=='r') mode=RATE;	
      else if(character=='v') mode=TEMP_V;
      else if(character=='m') mode=TEMP_M;
#ifdef HAVE_NVML
      else if(character=='d') mode=TEMP_D;
#endif
      else if(character=='p') mode=PERF;	
      i++;
    } else {
      fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], argv[i]);
      print_help(argv[0]), exit(1);
    }
  }
}


static void print_help(char *prog)
{
  printf("Usage : %s [OPTIONS]\n"
	 "%s - Window Maker Lenovo / Dell hardware monitor dockapp\n"
	 "  -d,  --display <string>        	display to use\n"
	 "  -bl, --backlight               	turn on back-light\n"
	 "  -cs  --use-bat-color			use separate color scheme when on BAT power\n"  
	 "Note: Possible color notations are name (red), rgb (rgb:FF/00/00) or web in quotation marks (\"#ff0000\")!\n"
	 "  -lc, --light-color <string>    	back-light color (rgb:FF/AA/00 is default)\n"
	 "  -nc, --sign-light-color <string>	back-light color for signs (rgb:99/66/33 is default)\n"
	 "  -bc, --background-color <string>	background color (rgb:22/22/22 is default)\n"
	 "  -fc, --foreground-color <string>	foreground color (rgb:00/FF/00 is default)\n"
	 "  -rc, --backlight-foreground-color <string>	foreground color for backlight (rgb:AA/00/00 is default)\n"
	 "  -sc, --sign-color <string>    	sign color (rgb:22/77/22 is default)\n"
	 "  -lb, --bat-light-color <string>    	back-light color when on BAT and -cs is set (rgb:FF/99/99 is default)\n"
	 "  -nb, --bat-sign-light-color <string>	back-light color for signs when on BAT and -cs is set (rgb:AA/77/77 is default)\n"
	 "  -bb, --bat-background-color <string>	background color when on BAT and -cs is set (rgb:22/22/22 is default)\n"
	 "  -fb, --bat-foreground-color <string>	foreground color when on BAT and -cs is set  (rgb:FF/00/00 is default)\n"
	 "  -rb, --bat-backlight-foreground-color <string>	foreground color for backlight when on BAT and -cs is set (rgb:FF/00/00 is default)\n"
	 "  -sb, --bat-sign-color <string>    	sign color when on BAT and -cs is set (rgb:77/22/22 is default)\n"
	 "  -c,  --config <string>         	set filename of config file\n"
	 "  -i,  --interval <number>       	number of secs between updates (1 is default)\n"
	 "  -a,  --alarm <number>          	low battery level to raise alarm\n"
	 "  -at,  --alarm-term <number>    	CPU temperature to raise alarm\n"
	 "                                 	(90 is default)\n"
	 "  -h,  --help                    	show this help text and exit\n"
	 "  -v,  --version                 	show program version and exit\n"
	 "  -w,  --windowed                	run the application in windowed mode\n"
	 "  -bw, --broken-wm               	activate broken window manager fix\n"
	 "  -n,  --notify <string>         	command to launch in case of alarm is on\n"
	 "  -ob,  --on-bat <string>         	command to launch during laptop's switching to BAT power\n"
	 "  -oa,  --on-ac <string>         	command to launch during laptop's switching to AC power\n"
	 "  -m,  --mode [t|r|v|m|p|s]      	set mode for the lower row , \n"
	 "                                 	t=CPU temperature,r=current rate,\n"
	 "                                 	v=GPU temperature, m=motherboard temperature,\n"
	 "                                 	p=CPU speed,s=toggle\n"
	 "  -ts  --togglespeed <int>       	set toggle speed in seconds\n"           
	 "  -as  --animationspeed <int>    	set speed for charging animation in msec\n"           
	 "  -hs  --historysize <int>       	set size of history for calculating\n"
	 "                                 	average power consumption rate\n"           
	 "  --use-proc                     	use depricated /proc interface instead of sysfs\n"
	 "  --use-nvidia                   	use propoetary NVidia library to collect GPU temperature (if available)\n"
	 "  --alarm-nvidia                 	alarm on GPU throttling (if NVidia library is available)\n",  
	 prog, prog);
  /* OPTIONS SUPP :
   *  ? -f, --file    : configuration file
   */
}


int acpi_getinfos(AcpiInfos *infos) {
  DEBUGSTRING("acpi_getinfos\n")
  if (
#if defined(linux) || defined(solaris)
      (acpi_read(infos))
#else
# ifdef freebsd
      (acpi_read(&temp_info))
# endif
#endif
      ) {
    fprintf(stderr, "Cannot read ACPI information: %i\n");
    return(1);
  }
	return (0);
}

static void sysfs_getinfos(AcpiInfos *infos) {
  DEBUGSTRING("sysfs_getinfos\n")
  if (
#if defined(linux) || defined(solaris)
      (sysfs_read(infos))
#endif
      ) {
    fprintf(stderr, "Cannot read SYSFS information: %i\n");
    exit(1);
  }
}



int acpi_exists() {
  if (access(ACPIDEV, R_OK))
    return 0;
  else
    return 1;
}
static void check_alarm()
{
  static light pre_backlight;
  static Bool in_alarm_mode = False;
  /* alarm mode */
#ifdef HAVE_NVML
  if ((cur_acpi_infos.low && (cur_acpi_infos.battery_status[0] || cur_acpi_infos.battery_status[1])) || (cur_acpi_infos.thermal_temp > alarm_level_temp) || alert_nvml > 1) {
#else
  if ((cur_acpi_infos.low && (cur_acpi_infos.battery_status[0] || cur_acpi_infos.battery_status[1])) || (cur_acpi_infos.thermal_temp > alarm_level_temp)) {
#endif
    if (!in_alarm_mode) {
      in_alarm_mode = True;
      pre_backlight = backlight;
      my_system(notif_cmd);
    }
    if ( (switch_authorized) ||
	 ( (! switch_authorized) && (backlight != pre_backlight) ) ) {
      switch_light();
      return;
    }
  }
  else {
    if (in_alarm_mode) {
      in_alarm_mode = False;
      if (backlight != pre_backlight) {
	switch_light();
	return;
      }
    }
  }
}

static int my_system (char *cmd) {
  int           pid;
  extern char **environ;

  if (cmd == 0) return 1;
  pid = fork ();
  if (pid == -1) return -1;
  if (pid == 0) {
    pid = fork ();
    if (pid == 0) {
      char *argv[4];
      argv[0] = "sh";
      argv[1] = "-c";
      argv[2] = cmd;
      argv[3] = 0;
      execve ("/bin/sh", argv, environ);
      exit (0);
    }
    exit (0);
  }
  return 0;
}


#ifdef linux

int acpi_read(AcpiInfos *i) {
  FILE  *fd;
  int   retcode = 0;
  int 	capacity[2],remain[2];
  int 	bat;
  char 	*buf;
  char 	*ptr;
  char 	stat;
  buf=(char *)malloc(sizeof(char)*512);
  RateListElem currRateElement;
  int		hist;
  long	rate;
  float	time;
  long	allcapacity=0;
  long	allremain=0;

  rate = 0;
  
  
  DEBUGSTRING("acpi_read()\n")

  /* get acpi thermal cpu info */
  if ((fd = fopen(thermal, "r"))) {
    fscanf(fd, "temperature: %d", &i->thermal_temp);
    fclose(fd);
		retcode=1;
		return retcode;
  }
  if ((fd = fopen(ac_state, "r"))) {
    bzero(buf, 512);
    fscanf(fd, "state: %s", buf);
    fclose(fd);
    if(strstr(buf, "on-line") != NULL) {
  		if (!i->ac_line_status){
				parts = &ac_parts;
				printf("Firing on AC command\n");
				my_system(on_ac_cmd);
			}
		 	i->ac_line_status=1;
		}
    if(strstr(buf, "off-line") != NULL) {
			if (i->ac_line_status){
				if (use_bat_colors) parts = &bat_parts;
				printf("Firing on BAT command\n");
				my_system(on_bat_cmd);
			}
			i->ac_line_status=0;
		}
  }
  if (number_of_batteries) {	
		for(bat=0;bat<number_of_batteries;bat++){
  	
  	  if ((fd = fopen(state_files[bat], "r"))) {
  	    bzero(buf, 512);
  	    fread(buf,512,1,fd);
  	    fclose(fd);
  	    if(( ptr = strstr(buf,"charging state:"))) {
					stat = *(ptr + 25);
					switch (stat) 
		  		{
		  			case 'd':
		  			  i->battery_status[bat]=1;
		  			  break;
		  			case 'c':
		  			  i->battery_status[bat]=3;
		  			  break;
		  			case 'u':
		  			  i->battery_status[bat]=0;
		  			  break;
		  		}
  	    }
  	    if ((ptr = strstr (buf, "remaining capacity:"))) {
					ptr += 25;
					sscanf(ptr,"%d",&i->remain[bat]);
  	    }
  	    if ((ptr = strstr (buf, "present rate:"))) {
					ptr += 25;
					sscanf(ptr,"%d",&((*firstRateElem).rate[bat]));
  	    }
  	  }
  	   
  	  i->battery_percentage[bat] = (((float)(i->remain[bat])*100)/cur_acpi_infos.currcap[bat]);



  	  currRateElement = *firstRateElem;
  	  if(currRateElement.rate[bat]!=0){
  	    for(hist=0;hist<history_size;hist++){
		if(currRateElement.rate[bat]!=0){
		  rate += currRateElement.rate[bat];
		} else rate+= (*firstRateElem).rate[bat];
		currRateElement = *currRateElement.next;
  	    }
  	  } else {
  	    rate=0;
  	    i->rate[bat]=0;	
  	  }



  	  /* calc average */
  	  rate = rate / history_size;
  	  i->rate[bat] = rate;
  	}

  	if((i->battery_status[0]==1 || i->battery_status[1]==1) && (i->rate[0]+i->rate[1])>0){
  	  time = (float)(i->remain[0]+i->remain[1])/(float)(i->rate[0]+i->rate[1]);
  	  i->hours_left=(int)time;
  	  i->minutes_left=(int)((time-(int)time)*60);	
  	}
  	if(i->battery_status[0]==0 && i->battery_status[1]==0){
  	  i->hours_left=0;
  	  i->minutes_left=0;	
  	}
  	if((i->battery_status[0]==3||i->battery_status[1]==3) && (i->rate[0]>0 || i->rate[1]>0)){
  	  time = (float)(cur_acpi_infos.currcap[0] - i->remain[0] + cur_acpi_infos.currcap[1] - i->remain[1])/(float)(i->rate[0]+i->rate[1]);
  	  i->hours_left=(int)time;
  	  i->minutes_left=(int)(60*(time-(int)time));
  	}
  	for(bat=0;bat<number_of_batteries;bat++){
  	  allremain += i->remain[bat];
  	  allcapacity += cur_acpi_infos.currcap[bat];
  	}

  	cur_acpi_infos.low=0;
  	if(allcapacity>0){
  	  if(((double)allremain/(double)allcapacity)*100<alarm_level){ 
  	    cur_acpi_infos.low=1;
  	  }
  	} 

  	DEBUGSTRING("MID acpi_read()\n") 
  	firstRateElem = ((*firstRateElem).next);	
	}
	else cur_acpi_infos.low=1;	
  free(buf);
  DEBUGSTRING("END acpi_read()\n")
  return retcode;
}

int ibm_fan_read(AcpiInfos *i) {
  FILE  *fd;
  char 	*buf;
  char 	*ptr;
  buf=(char *)malloc(sizeof(char)*512);
  
  DEBUGSTRING("ibm_fan_read()\n")

  /* get ibm fan info */
  if ((fd = fopen(ibm_fan, "r"))) {
    bzero(buf, 512);
    fread(buf,512,1,fd);
    fclose(fd);
    if ((ptr = strstr (buf, "speed:"))) {
			ptr += 7;
			sscanf(ptr,"%d",&i->ibm_fan);
     }
  	free(buf);
		return (0);
  }
  else {
	  has_ibm_fan=0;
	  i->ibm_fan=8888;
  	free(buf);
	  return (1);
  }
}

int sysfs_ibm_fan_read(AcpiInfos *i) {
  FILE  *fd;
  
  DEBUGSTRING("ibm_fan_read()\n")

  /* get ibm fan info */
  if ((fd = fopen(sysfs_ibm_fan, "r"))) {
		fscanf(fd,"%d",&i->ibm_fan);
    fclose(fd);
		return (0);
  }
  else {
	  has_ibm_fan=0;
	  i->ibm_fan=8888;
	  return (1);
  }
}

int ibm_acpi_read(AcpiInfos *i) {
  FILE  *fd;
  char 	*buf;
  char 	*ptr;
  buf=(char *)malloc(sizeof(char)*512);
  
  DEBUGSTRING("ibm_acpi_read()\n")

  /* get ibm temp info */
  if ((fd = fopen(ibm_thermal, "r"))) {
     bzero(buf, 512);
     fread(buf,512,1,fd);
     fclose(fd);
     if ((ptr = strstr (buf, "temperatures:"))) {
			ptr += 14;
			sscanf(ptr,"%d %d %*d %d",&i->ibm_ctemp,&i->ibm_mtemp,&i->ibm_vtemp);
     }
  	free(buf);
		return (0);
  }
  else {
  	free(buf);
	  has_ibm_temp=0;
	  return (1);
  }
}

int ibm_sysfs_read(AcpiInfos *i) {
  FILE  *fd;
	long	raw=0;
  
  DEBUGSTRING("ibm_sysfs_read()\n")

  /* get ibm temp info via sysfs*/
  if ((fd = fopen(sysfs_ibm_cpu_thermal, "r"))) {
		fscanf(fd,"%ld", &raw);
    fclose(fd);
    if (raw == 0) {
	 		has_ibm_temp=0;
	 		return (1);
		}
	i->ibm_ctemp = raw/1000;
	}
	else {
		has_ibm_temp=0;
	 	return (1);
	}
	raw=0;
  if ((fd = fopen(sysfs_ibm_gpu_thermal, "r"))) {
		fscanf(fd,"%ld", &raw);
	  fclose(fd);
	}
	i->ibm_vtemp = raw/1000;
	raw=0;
  if ((fd = fopen(sysfs_ibm_mb_thermal, "r"))) {
		fscanf(fd,"%ld", &raw);
	  fclose(fd);
	}
	i->ibm_mtemp = raw/1000;
	return (0);
}


int acpi_speed_read(AcpiInfos *i) {
  FILE  *fd;
  char 	*buf;
  char 	*ptr;
  buf=(char *)malloc(sizeof(char)*512);
  
  DEBUGSTRING("cpu_speed_read()\n")

  /* get cpu speed info */
  if ((fd = fopen(cpu_speed, "r"))) {
    bzero(buf, 512);
    fread(buf,512,1,fd);
    fclose(fd);
    if ((ptr = strstr (buf, "*"))) {
			ptr += 22;
			sscanf(ptr,"%d",&i->speed);
      }
  	free(buf);
		return (0);
  }
  else {
 		free(buf);
 		has_cpu_file=0;
  	return (1);
 		}
}

int sysfs_speed_read(AcpiInfos *i) {
  FILE  *fd;
	long	raw=0;
  
  DEBUGSTRING("cpu_speed_read()\n")

  /* get cpu speed info */
  if ((fd = fopen(sysfs_cpu_speed, "r"))) {
		fscanf(fd,"%d",&raw);
    fclose(fd);
		i->speed = raw / 1000;
		return (0);
  }
  else {
	 	has_cpu_file=0;
	 	return (1);
  	}
}

int i8k_read(AcpiInfos *i) {
  FILE  *fd;
  
  DEBUGSTRING("Dell_i8k_read()\n")

  /* get dell info */
  if ((fd = fopen(i8k, "r"))) {
		fscanf(fd,"%*s %*s %*s %d %d %d %d %d", &i->ibm_ctemp, &i->i8k_fan, &i->i8k_fan2, &i->ibm_fan, &i->ibm_fan2);
    fclose(fd);
		return (0);
  }
  else {
	  has_i8k=0;
	  return (1);
  }
}

int sysfs_read(AcpiInfos *i) {
  FILE	*fd;
  int		retcode = 0;
  int		capacity[2],remain[2];
  int		bat;
  char 	*buf;
  char 	*ptr;
  buf=(char *)malloc(sizeof(char)*512);
  RateListElem currRateElement;
  int		hist;
  long	rate;
  float	time;
  long	allcapacity=0;
  long	allremain=0;
	long	raw=0;
	int		prev=1;

  rate = 0;
  prev = i->ac_line_status;
  
  DEBUGSTRING("sysfs_read()\n")

  /* get sysfs thermal cpu info */
	if (!has_i8k){
  	if ((fd = fopen(sysfs_thermal, "r"))) {
  		DEBUGSTRING("sysfs_read(thermal_temp)\n")
    	fscanf(fd, "%ld", &raw);
    	i->thermal_temp = raw / 1000;
    	fclose(fd);
  	}
		else {
			return (1);
		}
	}
  if ((fd = fopen(sysfs_ac_state, "r"))) {
  	DEBUGSTRING("sysfs_read(iac_line_status)\n")
    fscanf(fd, "%d", &i->ac_line_status);
    fclose(fd);
  }
	if (!has_ibm_temp) {
  	if ((fd = fopen(sysfs_thermal_v, "r"))) {
  		DEBUGSTRING("sysfs_read(sysfs_thermal_v)\n")
  		fscanf(fd, "%ld", &raw);
  		fclose(fd);
  		i->ibm_vtemp = raw / 1000;
  	}
  	if ((fd = fopen(sysfs_thermal_m, "r"))) {
  		DEBUGSTRING("sysfs_read(sysfs_thermal_m)\n")
  		fscanf(fd, "%ld", &raw);
  		fclose(fd);
  		i->ibm_mtemp = raw / 1000;
  	}
	}
  if (prev && !i->ac_line_status){
		if (use_bat_colors) parts = &bat_parts;
		printf("Firing on BAT command\n");
		my_system(on_bat_cmd);
	}
	else if (!prev && i->ac_line_status){
		parts = &ac_parts;
		printf("Firing on AC command\n");
		my_system(on_ac_cmd);
	}
  if (number_of_batteries) {	
		for(bat=0;bat<number_of_batteries;bat++){
  	
  	  if ((fd = fopen(sysfs_state_files[bat], "r"))) {
  	    bzero(buf, 512);
  	    fread(buf,sizeof(char),1,fd);
  	    fclose(fd);
				switch (*buf) 
		  	{
		  		case 'D':
		  		  i->battery_status[bat]=1;
		  		  break;
		  		case 'C':
		  		  i->battery_status[bat]=3;
		  		  break;
		  		case 'U':
					case 'F':
		  		  i->battery_status[bat]=0;
		  		  break;
		  		}
  	  	if ((fd = fopen(sysfs_charge_files[bat], "r"))) {
					fscanf(fd,"%ld",&raw);
					i->remain[bat] = raw / 1000;
    			fclose(fd);
				}
  	  	if ((fd = fopen(sysfs_current_files[bat], "r"))) {
					fscanf(fd,"%ld",&raw);
					if (raw <= 1000) raw = 0;
					firstRateElem->rate[bat] = raw / 1000;
    			fclose(fd);
				}
  	  }
  	   
  	  i->battery_percentage[bat] = (((float)(i->remain[bat])*100)/cur_acpi_infos.currcap[bat]);


  	  currRateElement = *firstRateElem;
  	  if(currRateElement.rate[bat]!=0){
  	    for(hist=0;hist<history_size;hist++){
					if(currRateElement.rate[bat]!=0){
		  			rate += currRateElement.rate[bat];
					} else rate+= (*firstRateElem).rate[bat];
					currRateElement = *currRateElement.next;
  	    }
  	  } else {
  	   	 rate=0;
  	   	 i->rate[bat]=0;	
  	  }


  	  /* calc average */
  	  rate = rate / history_size;
  	  i->rate[bat] = rate;
  	}

  	if((i->battery_status[0]==1 || i->battery_status[1]==1) && (i->rate[0]+i->rate[1])>0){
  	  time = (float)(i->remain[0]+i->remain[1])/(float)(i->rate[0]+i->rate[1]);
  	  i->hours_left=(int)time;
  	  i->minutes_left=(int)((time-(int)time)*60);	
  	}
  	if(i->battery_status[0]==0 && i->battery_status[1]==0){
  	  i->hours_left=0;
  	  i->minutes_left=0;	
  	}
  	if((i->battery_status[0]==3||i->battery_status[1]==3) && (i->rate[0]>0 || i->rate[1]>0)){
  	  time = (float)(cur_acpi_infos.currcap[0] - i->remain[0] + cur_acpi_infos.currcap[1] - i->remain[1])/(float)(i->rate[0]+i->rate[1]);
  	  i->hours_left=(int)time;
  	  i->minutes_left=(int)(60*(time-(int)time));
  	}
  	for(bat=0;bat<number_of_batteries;bat++){
  	  allremain += i->remain[bat];
  	  allcapacity += cur_acpi_infos.currcap[bat];
  	}

  	cur_acpi_infos.low=0;
  	if(allcapacity>0){
  	  if(((double)allremain/(double)allcapacity)*100<alarm_level){ 
  	    cur_acpi_infos.low=1;
  	  }
  	} 

  	DEBUGSTRING("MID sysfs_read()\n") 
  	firstRateElem = ((*firstRateElem).next);	
	}
	else cur_acpi_infos.low=1;	
  free(buf);
  DEBUGSTRING("END sysfs_read()\n")
  return retcode;
}

#ifdef HAVE_NVML
static void read_d_temp(AcpiInfos *i) {
	DEBUGSTRING("Reading NVIDIA data\n");
	nvmlDevice_t device;
	unsigned long long clocksThrottleReasons;
	int temp;
	nvmlReturn_t res = nvmlDeviceGetHandleByIndex_v2 (0, &device);
 	if (res){
		printf("Unable to find GPU device: %s\n",nvmlErrorString(res));
		return;
	}
	// check GPU temp
	res = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &(i->ibm_dtemp));
 	if (res){
		printf("Unable to get GPU temperature via NVML: %s\n",nvmlErrorString(res));
	}
	// check throttling
	if (alert_nvml) {
		res = nvmlDeviceGetCurrentClocksThrottleReasons(device, &clocksThrottleReasons);
 		if (res){
			printf("Unable to get GPU throttling status via NVML: %s\n",nvmlErrorString(res));
		}
		else {
			if (clocksThrottleReasons > 4) {
				alert_nvml=2;
				printf("GPU throttling is in effect (reason: %lld)\n", clocksThrottleReasons);
			}
			else {
				res = nvmlDeviceGetTemperatureThreshold(device, NVML_TEMPERATURE_THRESHOLD_SLOWDOWN, &temp); 
 				if (res){
					printf("Unable to get GPU temperature threshold via NVML: %s\n",nvmlErrorString(res));
				}
				if (i->ibm_dtemp >= temp) {
					alert_nvml=4;
					printf("GPU temperature (%d) is higher than threshold %d!\n", i->ibm_dtemp, temp);
				}
				else {
					alert_nvml=1;
				}
			}
		}
	}
}
#endif
#endif
