
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <zlib.h>

#include "common.h"
#include "glog.h"
#include "glog_color.h"
#include "bufchain.h"

#ifdef GLOG_SUPPORT
#include "gzlog.h"
#endif

#define LOG_FILE_PATH		"/home/luotang.me/log/GIPC.log"
#define LOG_FILE_ZIP		"/home/luotang.me/log/GIPC-%ld.zip"
	

#define LOG_BUFFER_SIZE		4096


#define LOG_TITLE	 		"[GLOG_INFO:%d]\n"
#define LOG_MAX_FILE_SIZE 	(10*1024*1024)  /* 10M 可以压缩打包*/
#define LOG_MAX_HEAD_LEN 	32   


static const char* LEVEL_STR[] = {
	"INFO",
	"DEBUG",
	"ERROR",
};

static const char* COLOR_STR[] = {
	 "\e[0m",
	 "\e[0;30m",
	 "\e[1;30m",
	 "\e[0;31m",
	 "\e[1;31m",
	 "\e[0;32m",
	 "\e[1;32m",
	 "\e[0;33m",
	 "\e[1;33m",
	 "\e[0;34m",
	 "\e[1;34m",
	 "\e[0;35m",
	 "\e[1;35m",
	 "\e[0;36m",
	 "\e[1;36m",
	 "\e[0;37m",
	 "\e[1;37m",
};

#define BOLD                 "\e[1m"
#define UNDERLINE            "\e[4m"
#define BLINK                "\e[5m"
#define REVERSE              "\e[7m"
#define HIDE                 "\e[8m"
#define CLEAR                "\e[2J"
#define CLRLINE              "\r\e[K" /* or "\e[1K\r" */


typedef struct log_param {
	unsigned char 	enable;			/* log enable */
	unsigned char	blogout;		/* log printf enable */
	unsigned char	blogfile;		/* log file enable*/
	unsigned char	blogredirect; 	/* system srceen output */

	unsigned char	logname[64];
	unsigned int	logsize;		/* log limit size or log zip size */

	unsigned char 	bzip;			/* log zip enable */
	unsigned char 	zipalg;			/* log zip algorithm, lzma,tar,zip etc */
	unsigned char	bencrypt;		/* log zip encrypt enable */
	unsigned char	encryptalg;		/* log zip encrypt algorithm, md5,sha1,hash etc*/
	unsigned char	bupload;		/* log zip upload to remote data center*/
	unsigned char	bdownload;		/* log zip download from local system */
	unsigned char 	res1[2+16];		
	/*unsigned char	uploadurl[32];	   log zip upload url */

	unsigned char 	bpcolor;		/* log printf color enable*/
	unsigned char 	bpfunc;			/* log printf func  enable */
	unsigned char 	bpfile;			/* log printf file  enable */
	unsigned char 	bpline;			/* log printf line  number */

	unsigned char	logstat;		/* log running state */
	unsigned char 	logver;			/* log version using bit set */
	unsigned char 	logmlevel;		/* min level of log */
	unsigned char	res2[1+16];
	
	int			  	logfd;			/* log thread/process pid */
	int			  	logpid; 		/* to avoid duplicates over a fork */
	
	
	
#ifdef 	GLOG_SUPPORT
	unsigned char	bglogfile;
	unsigned char	res1[3];
	unsigned int	glogsize;
	gzlog*  		lgzlog;
#endif
	
	bufchain 		queue;			/* log tmp buff chain quene */
	pthread_mutex_t log_mutex;
}LogParam;


static LogParam  log;
static LogParam* lplog = &log;


static int glog_current_time(char* localtime, unsigned int len) {
	struct tm tm;
	time_t t;

	if( !localtime || len > 32) {
		return -1;
	}
	memset(&tm,    0, sizeof(tm));

	time( &t );
	localtime_r( &t, &tm );
	snprintf(localtime, len - 1 , 
			"%d-%d-%2d-%02d:%02d:%02d",
			tm.tm_year + 1900,
			tm.tm_mon + 1, 
			tm.tm_mday,
			tm.tm_hour, 
			tm.tm_min, 
			tm.tm_sec);
	
	return 0;
}

int glog_generate_name(char* name, unsigned int len) {
	
	if( ! name || len > 128 ) {
		return -1;
	}
	
	time_t t;
	time( &t );

	snprintf(name, len - 1, LOG_FILE_ZIP, t);
	
	return 0;
}

static char* glog_dupvprintf(const char *fmt, va_list ap) {
	char*	buf 	= NULL;
	char* 	prebuf 	= NULL;
    int len, size 	= 0;

    buf = NEW( char, 256);
	if( !buf ) {
		return NULL;
	}
    size = 256;

    while (1) {

#ifdef WIN32
#define vsnprintf _vsnprintf
#endif
#ifdef va_copy
	/* Use the `va_copy' macro mandated by C99, if present.
     * XXX some environments may have this as __va_copy() */
	va_list aq;
	va_copy(aq, ap);
	len = vsnprintf(buf, size, fmt, aq);
	va_end(aq); 
#else
  /* Ugh. No va_copy macro, so do something nasty.
   * Technically, you can't reuse a va_list like this: it is left
   * unspecified whether advancing a va_list pointer modifies its
   * value or something it points to, so on some platforms calling
   * vsnprintf twice on the same va_list might fail hideously
   * (indeed, it has been observed to).
   * XXX the autoconf manual suggests that using memcpy() will give
   *	 "maximum portability". */

	len = vsnprintf(buf, size, fmt, ap);
#endif
	if (len >= 0 && len < size) { 
	   /* This is the C99-specified criterion for snprintf to have
        * been completely successful. */
	    return buf;
	} else if (len > 0) { 
		/* This is the C99 error condition: the returned length is
         * the required buffer size not counting the NUL. */
	    size = len + 1;
	} else {
	    /* This is the pre-C99 glibc error condition: <0 means the
	     * buffer wasn't big enough, so we enlarge it a bit and hope. */
	    size += 256;
	}

	/* 	If realloc failed, a null pointer is returned, and the memory 
	 *	block pointed to by argument buf is not deallocated(not free or move!!!). 
	 *	(it is still valid, and with its contents unchanged)
	 *	ref: http://www.cplusplus.com/reference/cstdlib/realloc */
	prebuf = buf;
	buf = RENEW(buf, char, size);
	if( !buf ) {
		sfree(prebuf);
		return NULL;
	}

	}

}

static int glog_update_file(LogParam* ctx, int last_write_pos) {
	int 	ret = -1;
	char 	log_info[LOG_MAX_HEAD_LEN];

	if( !ctx || last_write_pos < 0 ) {
		return -1;
	}

	memset(log_info, 0, sizeof(log_info));
	
	ret = lseek(ctx->logfd, 0, SEEK_SET); 
	if(ret < 0) {
		printf("lseek file start error:%s\n", strerror(errno));
		return -1;
	} 
	snprintf(log_info, LOG_MAX_HEAD_LEN, LOG_TITLE, last_write_pos);
	ret  = write(ctx->logfd, log_info, strlen(log_info)); 
	if(ret != (int)strlen(log_info)) {
		printf("write file header error:%s\n", strerror(errno));
		return -1;
	}
	lplog->logsize += last_write_pos;
	return 0;
	
}

static int glog_write_file(LogParam* ctx, const char *info, int len) { 
	if ( !info || len <= 0 ) {
		printf("glog_write_file param is error\n");
		return -1;
	} 
	if( !ctx || ctx->logfd < 0 ) {
		lplog->logstat 	= LS_ERROR;
		return -1;
	}
	
	int 	ret = -1;  
	int 	total_len = 0; 
	int 	last_write_pos = 0;
  	char 	read_buf[LOG_MAX_HEAD_LEN]; 

	memset(read_buf, 0, sizeof(read_buf));
	
	
	total_len = lseek(ctx->logfd, 0, SEEK_END); 
	if(total_len < 0) {
		printf("lseek total_len error:%s\n", strerror(errno));
		goto err;
	}
	else if(0 == total_len) {
		/* write header info */
		ret = glog_update_file(ctx, LOG_MAX_HEAD_LEN);
		if(0 != ret) {
			goto err;
		}
		total_len += LOG_MAX_HEAD_LEN;
		ret = lseek(ctx->logfd, LOG_MAX_HEAD_LEN, SEEK_SET);
		if (ret < 0) {
			printf("lseek log_head_len failed\n");
			goto err;
		}
	}
	else {
		/* jump to file start */
		ret = lseek(ctx->logfd, 0, SEEK_SET); 
		if (ret < 0) {
			printf("[%d][%s]lseek file start error:%s\n", 
					__LINE__, __func__, strerror(errno));
			goto err;
		}
		
		/* check whether the log file size > LOG_FILE_SIZE,
			device maybe reboot*/
		ret = read(ctx->logfd, read_buf, LOG_MAX_HEAD_LEN);
		if ( ret < 0 ) { 
			printf("[%d][%s]read info failed(%d---%d)\n",
					__LINE__, __func__, ret, len);
			goto err;
		}

		sscanf(read_buf, "[TM_LOG_INFO:%d]", &last_write_pos); 	

		//printf("log last_write_pos:%d\n", last_write_pos);
		
		if (last_write_pos + len > LOG_MAX_FILE_SIZE) {
			printf("4g write log roolback\n"); 
			last_write_pos = LOG_MAX_HEAD_LEN;
		}
		/* Skip log header*/
		ret = lseek(ctx->logfd, last_write_pos, SEEK_SET); 
		if (ret < 0) { 
			printf("[%d][%s]lseek last_write_pos error:%s\n", 
					__LINE__, __func__, strerror(errno));
			goto err;
		}
		total_len = last_write_pos;
	}
	
	ret = write(ctx->logfd, info, len); 
	if(ret != len){  
		printf("[%d][%s]lseek write logerror:%s\n", 
					__LINE__, __func__, strerror(errno));
		goto err;
	}

	ret = glog_update_file(ctx, total_len + len);
	if(ret != 0) {
		goto err;
	}
	
	fsync(ctx->logfd);
	return 0;
	
err:
	fsync(ctx->logfd);
	return -1;
}


int glog_init(void* data, unsigned int len) {
	(void)data;
	(void)len;
	char log_name[128];

	memset(log_name, 0, sizeof(log_name));
	memset(lplog, 0, sizeof(LogParam));
	
	lplog->enable		= 1;
	lplog->blogout  	= 1;
	lplog->blogfile 	= 0;
	lplog->blogredirect = 1;
	
	lplog->bzip 		= 0;
	lplog->zipalg		= 0;			
	lplog->bencrypt		= 0;		
	lplog->encryptalg	= 0;		
	lplog->bupload		= 0;		
	lplog->bdownload	= 0;	
	
	lplog->bpcolor		= 1;
	lplog->bpfile		= 1;
	lplog->bpfunc		= 1;
	lplog->bpline		= 1;

	lplog->logfd		= -1;
	lplog->logver		= 1;
	lplog->logpid 		= getpid();
	lplog->logstat 		= LS_CLOSED;
	lplog->logmlevel 	= LV_INFO;
	lplog->logsize 		= LOG_MAX_FILE_SIZE;

#ifdef GLOG_SUPPORT
	lplog->bglogfile= 1;
	lplog->lgzlog   = NULL;
#endif
	pthread_mutex_init( &(lplog->log_mutex), NULL );

	lplog->logstat 	= LS_OPENING;
	
	if(lplog->blogfile) {
		lplog->logfd = open(LOG_FILE_PATH, 
			O_CREAT | O_RDWR | O_APPEND , 0766); 
		if (lplog->logfd < 0) {
			printf("[%d][%s]fail to open %s\n",
					__LINE__, __func__, LOG_FILE_PATH);
			lplog->logstat 	= LS_ERROR;
			return -1;
		} 	
	}
#ifdef GLOG_SUPPORT
	if(lplog->bglogfile) {
		glog_generate_name(log_name, sizeof(log_name));
		lplog->lgzlog = gzlog_open(log_name);
	}
#endif
	bufchain_init(&lplog->queue);

	lplog->logstat 	= LS_OPEN;
	
	return 0;
}


int glog_free(void* data, unsigned int len) {
	(void)data;
	(void)len;

	lplog->logstat = LS_CLOSED;
	sclose(lplog->logfd);
    bufchain_clear(&lplog->queue);

	return 0;
}

int glog_zip(void* data, unsigned int len) {

	FILE* 			file = NULL;
	uLongf     		clen = 0;
	int				ret  = -1;
	
    unsigned char*  cbuf = NULL; 

	char log_name[128];
	char cmd[128];

	memset(cmd, 0, sizeof(cmd));
	memset(log_name, 0, sizeof(log_name));
	glog_generate_name(log_name, sizeof(log_name));

	snprintf(cmd, 128, 
		"zip -q -r -P pass %s %s", log_name, LOG_FILE_PATH);

	system(cmd);

	return 0;
}

int glog_write( int level, int color, char* mod, char* fmt, ... ) 
{
	int	ret = -1;
	va_list ap;
	char* data = NULL; /* save the param */
	
	char tmfmt[32];
	char log_name[128];
	char fmt_c[256];
	char fmt_nc[256];

	memset(tmfmt,  	 0, sizeof(tmfmt));
	memset(log_name, 0, sizeof(log_name));
	memset(fmt_c,   0, sizeof(fmt_c));
	memset(fmt_nc,  0, sizeof(fmt_nc));
	

	if( level > LV_ERROR || color > WHITE || !mod || !fmt ) {
		fprintf(stderr, "glog_write  param error\n");
		goto err;
	}
	if(LV_INFO == level) {
		/* nothing to do, printf or file log directly */
	} else {
	
		va_start( ap, fmt );
		
		ret = glog_current_time(tmfmt, 32);
		if(ret != 0) {
			goto err;
		}
		if(lplog->bpcolor && lplog->bpfile && 
			lplog->bpline && lplog->bpfunc ) {
			
			snprintf(fmt_c, sizeof(fmt_c) - 1,
					"%s[%s][%s][%s][%s %s:%d]:%s\n", 
					COLOR_STR[color],
					mod,
					LEVEL_STR[level],
					tmfmt, 
					__func__,
					__FILE__,
					__LINE__,
					fmt );
		} else {
			snprintf(fmt_nc, sizeof(fmt_nc) - 1,
					"[%s][%s][%s][%s %s:%d]: %s\n", 		
					mod,
					LEVEL_STR[level],
					tmfmt, 
					__func__,
					__FILE__,
					__LINE__,
					fmt );

		}

		data = glog_dupvprintf(fmt_c, ap);
		if( !data ) {
			goto err;
		}
		va_end( ap );
		
	}

	 if(lplog->blogout) {
		 fprintf(stderr, "%s", data);
	 }

	 /*
     * In state L_CLOSED, we call logfopen, which will set the state
     * to one of L_OPENING, L_OPEN or L_ERROR. Hence we process all of
     * those three _after_ processing L_CLOSED.
     */
    if (lplog->logstat == LS_CLOSED) {
		//glog_init(NULL, 0);
	}
	
    if (lplog->logstat == LS_OPENING || lplog->logstat == LS_ZIPING) {
		
		bufchain_add(&lplog->queue, data, strlen(data));
		
    } else if (lplog->logstat == LS_OPEN) {
    
		if(lplog->blogfile) {
			while (bufchain_size(&lplog->queue)) {
				void* _data = NULL;
				int   _len = 0;
				bufchain_prefix(&lplog->queue, &_data, &_len);
				glog_write_file(lplog, data, _len);
				bufchain_consume(&lplog->queue, _len);
			}
			
			pthread_mutex_lock( &(lplog->log_mutex));
			ret = glog_write_file(lplog, data, strlen(data));
			pthread_mutex_unlock( &(lplog->log_mutex));
			if(ret != 0) {
				goto err;
			}
		}
		
#ifdef GLOG_SUPPORT
		if(lplog->bglogfile) {
			lplog->glogsize += strlen(data);
			gzlog_write(lplog->lgzlog, data, strlen(data));
			if(lplog->glogsize >= LOG_MAX_FILE_SIZE) {
				gzlog_compress(lplog->lgzlog);
				gzlog_close(lplog->lgzlog);
				lplog->glogsize = 0;
				lplog->lgzlog = NULL;
				log_generate_name(log_name, sizeof(log_name));
				lplog->lgzlog = gzlog_open(log_name);
			}
		}	
#endif
		sfree(data);
		if(lplog->bzip) {
			if(lplog->logsize >= LOG_MAX_FILE_SIZE) {
				lplog->logstat = LS_ZIPING;	

				pthread_mutex_lock( &(lplog->log_mutex));
				glog_zip(NULL, 0);
				pthread_mutex_unlock( &(lplog->log_mutex));
						
				/* empty log file */
			    ftruncate(lplog->logfd, 0);
			    lseek(lplog->logfd, 0, SEEK_SET);
				lplog->logsize = 0;
				lplog->logstat = LS_OPEN;
			}
		}
	} /* else L_ERROR, so ignore the write */
	
	return 0;
err:
	lplog->logstat = LS_ERROR;
	sfree(data);
	return -1;
}

