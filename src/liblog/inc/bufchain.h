
struct bufchain_granule;

#define BUFFER_MIN_GRANULE  512

struct bufchain_granule {
    struct bufchain_granule *next;
    char* bufpos;
	char* bufend;
	char* bufmax;
};

typedef struct bufchain_tag {
    struct bufchain_granule* head;
	struct bufchain_granule* tail;
    int buffersize;/* current amount of buffered data */
} bufchain;

int bufchain_init(bufchain *ch);
int bufchain_clear(bufchain *ch);
int bufchain_size(bufchain *ch);
int bufchain_add(bufchain *ch, const void *data, int len);
int bufchain_prefix(bufchain *ch, void **data, int *len);
int bufchain_consume(bufchain *ch, int len);
int bufchain_fetch(bufchain *ch, void *data, int len);

