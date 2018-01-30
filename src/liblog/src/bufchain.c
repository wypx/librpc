#include "common.h"
#include "bufchain.h"

int bufchain_init(bufchain *ch)
{
	if( !ch ) {
		return -1;
	}
    ch->head = ch->tail = NULL;
    ch->buffersize = 0;
	return 0;
}

int bufchain_clear(bufchain *ch) {
	if( !ch ) {
		return -1;
	}
	struct bufchain_granule *b;
    while (ch->head) {
		b = ch->head;
		ch->head = ch->head->next;
		sfree(b);
    }
    ch->tail = NULL;
    ch->buffersize = 0;
	return 0;
}
int bufchain_size(bufchain *ch) {
	if( !ch ) {
		return -1;
	}
	return ch->buffersize;
}
int bufchain_add(bufchain *ch, const void *data, int len) {
	if( !ch ) {
		return -1;
	}
	const char *buf = (const char *)data;

    if (len == 0) return -1;

    ch->buffersize += len;

    while (len > 0) {
	if (ch->tail && ch->tail->bufend < ch->tail->bufmax) {
	    int copylen = MIN(len, ch->tail->bufmax - ch->tail->bufend);
	    memcpy(ch->tail->bufend, buf, copylen);
	    buf += copylen;
	    len -= copylen;
	    ch->tail->bufend += copylen;
	}
	if (len > 0) {
	    int grainlen = MAX(sizeof(struct bufchain_granule) + len, BUFFER_MIN_GRANULE);
	    struct bufchain_granule *newbuf;
	    newbuf = (struct bufchain_granule *)NEW(char, grainlen);
	    newbuf->bufpos = newbuf->bufend =
		(char *)newbuf + sizeof(struct bufchain_granule);
	    newbuf->bufmax = (char *)newbuf + grainlen;
	    newbuf->next = NULL;
	    if (ch->tail)
		ch->tail->next = newbuf;
	    else
		ch->head = newbuf;
	    ch->tail = newbuf;
	}
    }
	return 0;
}

int bufchain_prefix(bufchain *ch, void **data, int *len) {
	if( !ch ) {
		return -1;
	}
	*len = ch->head->bufend - ch->head->bufpos;
    *data = ch->head->bufpos;

	return 0;
}
int bufchain_consume(bufchain *ch, int len) {

	struct bufchain_granule *tmp;

    if(ch->buffersize < len) goto err;
	
    while (len > 0) {
		int remlen = len;
		if( !ch->head ) goto err;
		if (remlen >= ch->head->bufend - ch->head->bufpos) {
		    remlen = ch->head->bufend - ch->head->bufpos;
		    tmp = ch->head;
		    ch->head = tmp->next;
		    if (!ch->head)
			ch->tail = NULL;
		    sfree(tmp);
		} else {
		    ch->head->bufpos += remlen;
		}
		ch->buffersize -= remlen;
		len -= remlen;
    }
	return 0;
err:
	return -1;

}
int bufchain_fetch(bufchain *ch, void *data, int len) {

	struct bufchain_granule *tmp;
    char *data_c = (char *)data;

    tmp = ch->head;

    if(ch->buffersize < len) goto err;
    while (len > 0) {
		int remlen = len;
		if(!tmp) goto err;
		if (remlen >= tmp->bufend - tmp->bufpos)
		    remlen = tmp->bufend - tmp->bufpos;
		memcpy(data_c, tmp->bufpos, remlen);

		tmp = tmp->next;
		len -= remlen;
		data_c += remlen;
    }
	return 0;
err:
	return -1;
}

