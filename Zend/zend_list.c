/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2017 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@zend.com>                                |
   |          Zeev Suraski <zeev@zend.com>                                |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

/* resource lists */

#include "zend.h"
#include "zend_list.h"
#include "zend_API.h"
#include "zend_globals.h"

ZEND_API int le_index_ptr;

/* true global */
static HashTable list_destructors;

ZEND_API zval *zend_list_insert(void *ptr, int type)
{
	int index;
	zval zv;

	index = zend_hash_next_free_element(&EG(regular_list));
	if (index == 0) {
		index = 1;
	}
	ZVAL_NEW_RES(&zv, index, ptr, type);
	return zend_hash_index_add_new(&EG(regular_list), index, &zv);
}

ZEND_API int zend_list_delete(zend_resource *res)
{
	if (GC_DELREF(res) <= 0) {
		return zend_hash_index_del(&EG(regular_list), res->handle);
	} else {
		return SUCCESS;
	}
}

ZEND_API int zend_list_free(zend_resource *res)
{
	if (GC_REFCOUNT(res) <= 0) {
		return zend_hash_index_del(&EG(regular_list), res->handle);
	} else {
		return SUCCESS;
	}
}

static void zend_resource_dtor(zend_resource *res)
{
	zend_rsrc_list_dtors_entry *ld;
	zend_resource r = *res;

	res->type = -1;
	res->ptr = NULL;

	ld = zend_hash_index_find_ptr(&list_destructors, r.type);
	if (ld) {
		if (ld->list_dtor_ex) {
			ld->list_dtor_ex(&r);
		}
	} else {
		zend_error(E_WARNING, "Jenis elemen daftar tidak dikenal (%d)", r.type);
	}
}


ZEND_API int zend_list_close(zend_resource *res)
{
	if (GC_REFCOUNT(res) <= 0) {
		return zend_list_free(res);
	} else if (res->type >= 0) {
		zend_resource_dtor(res);
	}
	return SUCCESS;
}

ZEND_API zend_resource* zend_register_resource(void *rsrc_pointer, int rsrc_type)
{
	zval *zv;

	zv = zend_list_insert(rsrc_pointer, rsrc_type);

	return Z_RES_P(zv);
}

ZEND_API void *zend_fetch_resource2(zend_resource *res, const char *resource_type_name, int resource_type1, int resource_type2)
{
	if (res) {
		if (resource_type1 == res->type) {
			return res->ptr;
		}

		if (resource_type2 == res->type) {
			return res->ptr;
		}
	}

	if (resource_type_name) {
		const char *space;
		const char *class_name = get_active_class_name(&space);
		zend_error(E_WARNING, "%s%s%s(): resource yang diberikan bukan merupakan resource %s yang valid", class_name, space, get_active_function_name(), resource_type_name);
	}

	return NULL;
}

ZEND_API void *zend_fetch_resource(zend_resource *res, const char *resource_type_name, int resource_type)
{
	if (resource_type == res->type) {
		return res->ptr;
	}

	if (resource_type_name) {
		const char *space;
		const char *class_name = get_active_class_name(&space);
		zend_error(E_WARNING, "%s%s%s(): resource yang diberikan bukan merupakan resource %s yang valid", class_name, space, get_active_function_name(), resource_type_name);
	}

	return NULL;
}

ZEND_API void *zend_fetch_resource_ex(zval *res, const char *resource_type_name, int resource_type)
{
	const char *space, *class_name;
	if (res == NULL) {
		if (resource_type_name) {
			class_name = get_active_class_name(&space);
			zend_error(E_WARNING, "%s%s%s(): tidak ada resource %s yang diberikan", class_name, space, get_active_function_name(), resource_type_name);
		}
		return NULL;
	}
	if (Z_TYPE_P(res) != IS_RESOURCE) {
		if (resource_type_name) {
			class_name = get_active_class_name(&space);
			zend_error(E_WARNING, "%s%s%s(): parameter yang diberikan bukan merupakan resource %s yang valid", class_name, space, get_active_function_name(), resource_type_name);
		}
		return NULL;
	}

	return zend_fetch_resource(Z_RES_P(res), resource_type_name, resource_type);
}

ZEND_API void *zend_fetch_resource2_ex(zval *res, const char *resource_type_name, int resource_type1, int resource_type2)
{
	const char *space, *class_name;
	if (res == NULL) {
		if (resource_type_name) {
			class_name = get_active_class_name(&space);
			zend_error(E_WARNING, "%s%s%s(): tidak ada resource %s yang diberikan", class_name, space, get_active_function_name(), resource_type_name);
		}
		return NULL;
	}
	if (Z_TYPE_P(res) != IS_RESOURCE) {
		if (resource_type_name) {
			class_name = get_active_class_name(&space);
			zend_error(E_WARNING, "%s%s%s(): parameter yang diberikan bukan merupakan resource %s yang valid", class_name, space, get_active_function_name(), resource_type_name);
		}
		return NULL;
	}

	return zend_fetch_resource2(Z_RES_P(res), resource_type_name, resource_type1, resource_type2);
}

void list_entry_destructor(zval *zv)
{
	zend_resource *res = Z_RES_P(zv);

	ZVAL_UNDEF(zv);
	if (res->type >= 0) {
		zend_resource_dtor(res);
	}
	efree_size(res, sizeof(zend_resource));
}

void plist_entry_destructor(zval *zv)
{
	zend_resource *res = Z_RES_P(zv);

	if (res->type >= 0) {
		zend_rsrc_list_dtors_entry *ld;

		ld = zend_hash_index_find_ptr(&list_destructors, res->type);
		if (ld) {
			if (ld->plist_dtor_ex) {
				ld->plist_dtor_ex(res);
			}
		} else {
			zend_error(E_WARNING,"Jenis elemen daftar tidak dikenal (%d)", res->type);
		}
	}
	free(res);
}

int zend_init_rsrc_list(void)
{
	zend_hash_init(&EG(regular_list), 8, NULL, list_entry_destructor, 0);
	return SUCCESS;
}


int zend_init_rsrc_plist(void)
{
	zend_hash_init_ex(&EG(persistent_list), 8, NULL, plist_entry_destructor, 1, 0);
	return SUCCESS;
}


static int zend_close_rsrc(zval *zv)
{
	zend_resource *res = Z_PTR_P(zv);

	if (res->type >= 0) {
		zend_resource_dtor(res);
	}
	return ZEND_HASH_APPLY_KEEP;
}


void zend_close_rsrc_list(HashTable *ht)
{
	zend_hash_reverse_apply(ht, zend_close_rsrc);
}


void zend_destroy_rsrc_list(HashTable *ht)
{
	zend_hash_graceful_reverse_destroy(ht);
}

static int clean_module_resource(zval *zv, void *arg)
{
	int resource_id = *(int *)arg;
	if (Z_RES_TYPE_P(zv) == resource_id) {
		return 1;
	} else {
		return 0;
	}
}


static int zend_clean_module_rsrc_dtors_cb(zval *zv, void *arg)
{
	zend_rsrc_list_dtors_entry *ld = (zend_rsrc_list_dtors_entry *)Z_PTR_P(zv);
	int module_number = *(int *)arg;
	if (ld->module_number == module_number) {
		zend_hash_apply_with_argument(&EG(persistent_list), clean_module_resource, (void *) &(ld->resource_id));
		return 1;
	} else {
		return 0;
	}
}


void zend_clean_module_rsrc_dtors(int module_number)
{
	zend_hash_apply_with_argument(&list_destructors, zend_clean_module_rsrc_dtors_cb, (void *) &module_number);
}


ZEND_API int zend_register_list_destructors_ex(rsrc_dtor_func_t ld, rsrc_dtor_func_t pld, const char *type_name, int module_number)
{
	zend_rsrc_list_dtors_entry *lde;
	zval zv;

	lde = malloc(sizeof(zend_rsrc_list_dtors_entry));
	lde->list_dtor_ex = ld;
	lde->plist_dtor_ex = pld;
	lde->module_number = module_number;
	lde->resource_id = list_destructors.nNextFreeElement;
	lde->type_name = type_name;
	ZVAL_PTR(&zv, lde);

	if (zend_hash_next_index_insert(&list_destructors, &zv) == NULL) {
		return FAILURE;
	}
	return list_destructors.nNextFreeElement-1;
}

ZEND_API int zend_fetch_list_dtor_id(const char *type_name)
{
	zend_rsrc_list_dtors_entry *lde;

	ZEND_HASH_FOREACH_PTR(&list_destructors, lde) {
		if (lde->type_name && (strcmp(type_name, lde->type_name) == 0)) {
			return lde->resource_id;
		}
	} ZEND_HASH_FOREACH_END();

	return 0;
}

static void list_destructors_dtor(zval *zv)
{
	free(Z_PTR_P(zv));
}

int zend_init_rsrc_list_dtors(void)
{
	zend_hash_init(&list_destructors, 64, NULL, list_destructors_dtor, 1);
	list_destructors.nNextFreeElement=1;	/* we don't want resource type 0 */
	return SUCCESS;
}


void zend_destroy_rsrc_list_dtors(void)
{
	zend_hash_destroy(&list_destructors);
}


const char *zend_rsrc_list_get_rsrc_type(zend_resource *res)
{
	zend_rsrc_list_dtors_entry *lde;

	lde = zend_hash_index_find_ptr(&list_destructors, res->type);
	if (lde) {
		return lde->type_name;
	} else {
		return NULL;
	}
}

ZEND_API zend_resource* zend_register_persistent_resource_ex(zend_string *key, void *rsrc_pointer, int rsrc_type)
{
	zval *zv;
	zval tmp;

	ZVAL_NEW_PERSISTENT_RES(&tmp, -1, rsrc_pointer, rsrc_type);
	GC_MAKE_PERSISTENT_LOCAL(Z_COUNTED(tmp));
	GC_MAKE_PERSISTENT_LOCAL(key);

	zv = zend_hash_update(&EG(persistent_list), key, &tmp);
	if (UNEXPECTED(zv == NULL)) {
		free(Z_RES(tmp));
		return NULL;
	}

	return Z_RES_P(zv);
}

ZEND_API zend_resource* zend_register_persistent_resource(const char *key, size_t key_len, void *rsrc_pointer, int rsrc_type)
{
	zend_string *str = zend_string_init(key, key_len, 1);
	zend_resource *ret  = zend_register_persistent_resource_ex(str, rsrc_pointer, rsrc_type);

	zend_string_release(str);
	return ret;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
