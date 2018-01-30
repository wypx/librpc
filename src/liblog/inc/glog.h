#include <stdio.h>

enum LOGCOLOR {
	NONE = 0,
	BLACK,
	L_BLACK,
	RED,
	L_RED,
	GREEN,
	L_GREEN,
	BROWN,
	YELLOW,
	BLUE,
	L_BLUE,
	PURPLE,
	L_PURPLE,
	CYAN,
	L_CYAN,
	GRAY,
	WHITE,
	COLOR_MAX,
};


enum LOGLEVEL {
	LV_INFO,
    LV_DEBUG,  
    LV_ERROR,     
    LV_LEVEL_MAX,
};

enum LOGSTAT { 
   LS_CLOSED = 0, 
   LS_OPENING, 
   LS_OPEN, 
   LS_ERROR, 
   LS_ZIPING,
};


enum log_type_t {
    LOG_STDIN   = 0, /*stdin*/
    LOG_STDOUT  = 1, /*stdout*/
    LOG_STDERR  = 2, /*stderr*/
    LOG_FILE    = 3,
    LOG_RSYSLOG = 4,

    LOG_MAX_OUTPUT = 255
} ;


int glog_write(int level, int color, char* mod, char* fmt, ... );
int glog_init(void* data, unsigned int len);
int glog_free(void* data, unsigned int len);

int glog_set_param(void* data, unsigned int len);
int glog_get_param(void* data, unsigned int len);

