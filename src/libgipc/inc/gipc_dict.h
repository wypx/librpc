/**************************************************************************
*
* Copyright (c) 2017, luotang.me <wypx520@gmail.com>, China.
* All rights reserved.
*
* Distributed under the terms of the GNU General Public License v2.
*
* This software is provided 'as is' with no explicit or implied warranties
* in respect of its properties, including, but not limited to, correctness
* and/or fitness for purpose.
*
**************************************************************************/
	
#include "common.h"
#include "dict.h"
#include "gipc_cache_pool.h"


	
#define HSIZE		997
#define SKIPLINKS   10


enum dct_alg_t {
	dct_hb,
	dct_pr,
	dct_rb,
	dct_tr,
	dct_sp,
	dct_wb,
	dct_skiplist,
	dct_hashtable,
	dct_hashtable2,
};

enum dct_search_t {
	dct_search,
	dct_searchle,
	dct_searchlt,
	dct_searchge,
	dct_searchgt,
};


dict* gipc_dict_init(int alg);
void  gipc_dict_clear(dict* dct);
void  gipc_dict_show(dict* dct);
void  gipc_dict_reverse(dict* dct);
void  gipc_dict_remove(dict* dct, long int mseq, CACHE_POOL* cPool);
void  gipc_dict_insert(dict* dct, long int mseq, CACHE_POOL_NODE* node);
void* gipc_dict_search(dict* dct, int s, long int mseq);
int	  gipc_dict_count(dict* dct);

