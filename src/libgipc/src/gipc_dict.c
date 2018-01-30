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

#include "gipc_dict.h"

static void key_val_free(void *key, void *datum) {    
	sfree(key);    
}

dict* gipc_dict_init(int alg) {
	dict* dct = NULL;
	
	if (dct_hb == alg)
		dct = hb_dict_new((dict_compare_func)strcmp);
	else if (dct_pr == alg)
		dct = pr_dict_new((dict_compare_func)strcmp);
	else if (dct_rb == alg)
		dct = rb_dict_new((dict_compare_func)strcmp);
	else if (dct_tr == alg)
		dct = tr_dict_new((dict_compare_func)strcmp, NULL);
	else if (dct_sp == alg)
		dct = sp_dict_new((dict_compare_func)strcmp);
	else if (dct_wb == alg)
		dct = wb_dict_new((dict_compare_func)strcmp);
	else if (dct_skiplist == alg)
		dct = skiplist_dict_new((dict_compare_func)strcmp, SKIPLINKS);
	else if (dct_hashtable == alg)
		dct = hashtable_dict_new((dict_compare_func)strcmp, 
					dict_str_hash, HSIZE);
	else if (dct_hashtable2 == alg)
		dct = hashtable2_dict_new((dict_compare_func)strcmp, 
					dict_str_hash, HSIZE);
	return dct;
}

void  gipc_dict_clear(dict* dct) {
	dict_clear(dct, key_val_free);
}
void gipc_dict_show(dict* dct) {

	dict_itor *itor = dict_itor_new(dct);
    dict_itor_first(itor);
    for (; dict_itor_valid(itor); dict_itor_next(itor))
		printf("'%s': ''\n",
	       (char *)dict_itor_key(itor));
    dict_itor_free(itor);
}
void gipc_dict_reverse(dict* dct) {
	dict_itor *itor = dict_itor_new(dct);
    dict_itor_last(itor);
    for (; dict_itor_valid(itor); dict_itor_prev(itor))
		printf("'%s': ''\n",
	       (char *)dict_itor_key(itor));
	      // (char *)*dict_itor_datum(itor));
    dict_itor_free(itor);
}

void gipc_dict_remove(dict* dct, long int mseq, CACHE_POOL* cPool) {

	CACHE_POOL_NODE* node = NULL;
	char keyseq[32];
		
	memset(keyseq, 0, sizeof(keyseq));
	snprintf(keyseq, sizeof(keyseq) - 1, "seq_%ld", mseq);

    dict_remove_result result = dict_remove(dct, keyseq);
    if (result.removed) {
		printf("removed '%s' from dict: \n", (char *)result.key);
		free(result.key);
		//free(result.datum);
		node = (CACHE_POOL_NODE*)result.datum;
		cache_pool_node_recycle(cPool, node);
    } else {
		printf("key '%s' not in dict!\n", keyseq);
	}
}

void gipc_dict_insert(dict* dct, long int mseq, CACHE_POOL_NODE* node) {

	char keyseq[32];
	
	memset(keyseq, 0, sizeof(keyseq));
	snprintf(keyseq, sizeof(keyseq) - 1, "seq_%ld", mseq);
	dict_insert_result result = dict_insert(dct, strdup(keyseq));
    if (result.inserted) {
		*result.datum_ptr = node;
		printf("inserted '%s': ''\n", keyseq);
    } else {
		printf("already in dict '%ld': ''\n", mseq);
	}
}

void* gipc_dict_search(dict* dct, int s, long int mseq) {

	void** search = NULL;
	char keyseq[32];
	
	memset(keyseq, 0, sizeof(keyseq));
	snprintf(keyseq, sizeof(keyseq) - 1, "seq_%ld", mseq);

	printf("search '%s': ''\n", keyseq);

	if (dct_search != s) {
		if (!dict_is_sorted(dct)) {
			printf("dict does not support that operation!");
			return NULL;
		}
	}
	if(dct_search == s) {
		search = dict_search(dct, keyseq);
	} else if (dct_searchle  == s) {
		search = dict_search_le(dct, keyseq);
	} else if (dct_searchlt== s) {
		search = dict_search_lt(dct, keyseq);
	} else if (dct_searchge  == s) {
		search = dict_search_ge(dct, keyseq);
	} else if (dct_searchgt  == s) {
		search = dict_search_gt(dct, keyseq);
	}

	return *search;
}


int	gipc_dict_count(dict* dct) {
	return dict_count(dct);
}

