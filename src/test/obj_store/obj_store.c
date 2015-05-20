/*
 * Copyright (c) 2015, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * obj_store.c -- unit test for root object and object store
 *
 * usage: obj_store file operation:...
 *
 * operations are 'r' or 'a' or 'f' or 'u' or 'n'
 *
 */
#include <stddef.h>
#include <sys/param.h>

#include "unittest.h"
#include "libpmemobj.h"
#include "util.h"
#include "list.h"
#include "obj.h"

#define	LAYOUT_NAME "layout_obj_store"
#define	MAX_ROOT_NAME 128

#define	ROOT_NAME "root object name"
#define	ROOT_VALUE 77

PLIST_HEAD(listhead, struct tobject);

struct root {
	char name[MAX_ROOT_NAME];
	uint8_t value;
	struct listhead lhead;
};

struct root_grown {
	char name[MAX_ROOT_NAME];
	uint8_t value;
	struct listhead lhead;
	char name2[MAX_ROOT_NAME];
};

struct tobject {
	uint8_t value;
	PLIST_ENTRY(struct tobject) next;
};

void
test_root_object(const char *path)
{
	PMEMobjpool *pop = NULL;
	OID_TYPE(struct root) root;
	OID_TYPE(struct root_grown) rootg;

	/* create a pool */
	if ((pop = pmemobj_create(path, LAYOUT_NAME,
					PMEMOBJ_MIN_POOL,
					S_IRWXU)) == NULL)
		FATAL("!pmemobj_create: %s", path);

	/* there should be no root object */
	ASSERTeq(pmemobj_root_size(pop), 0);

	/* create root object */
	OID_ASSIGN(root, pmemobj_root(pop, sizeof (struct root)));
	ASSERT(OID_IS_NULL(root) == 0);
	ASSERTeq(pmemobj_root_size(pop), sizeof (struct root));

	/* fill in root object */
	strncpy(D_RW(root)->name, ROOT_NAME, MAX_ROOT_NAME);
	D_RW(root)->value = ROOT_VALUE;
	pop->persist(D_RW(root), sizeof (struct root));

	/* re-open the pool */
	pmemobj_close(pop);
	if ((pop = pmemobj_open(path, LAYOUT_NAME)) == NULL)
		FATAL("!pmemobj_open: %s", path);

	/* check size and offset of root object */
	ASSERTeq(pmemobj_root_size(pop), sizeof (struct root));
	OID_ASSIGN(root, pmemobj_root(pop, 0));
	ASSERT(OID_IS_NULL(root) == 0);
	ASSERTeq(pmemobj_root_size(pop), sizeof (struct root));

	/* verify content of root object */
	ASSERTeq(strncmp(D_RO(root)->name, ROOT_NAME, MAX_ROOT_NAME), 0);
	ASSERTeq(D_RO(root)->value, ROOT_VALUE);

	/* resize root object */
	OID_ASSIGN(rootg, pmemobj_root(pop, sizeof (struct root_grown)));

	/* check offset and size of resized root object */
	ASSERT(OID_IS_NULL(rootg) == 0);
	ASSERTeq(pmemobj_root_size(pop), sizeof (struct root_grown));

	/* verify old content of resized root object */
	ASSERTeq(strncmp(D_RO(rootg)->name, ROOT_NAME, MAX_ROOT_NAME), 0);
	ASSERTeq(D_RO(rootg)->value, ROOT_VALUE);

	/* fill in new content */
	strncpy(D_RW(rootg)->name2, ROOT_NAME, MAX_ROOT_NAME);
	pop->persist(&D_RW(rootg)->name2, sizeof (D_RW(rootg)->name2));

	/* re-open the pool */
	pmemobj_close(pop);
	if ((pop = pmemobj_open(path, LAYOUT_NAME)) == NULL)
		FATAL("!pmemobj_open: %s", path);

	/* check size and offset of resized root object */
	OID_ASSIGN(rootg, pmemobj_root(pop, 0));
	ASSERT(OID_IS_NULL(rootg) == 0);
	ASSERTeq(pmemobj_root_size(pop), sizeof (struct root_grown));

	/* verify content of resized root object */
	ASSERTeq(strncmp(D_RO(rootg)->name, ROOT_NAME, MAX_ROOT_NAME), 0);
	ASSERTeq(D_RO(rootg)->value, ROOT_VALUE);
	ASSERTeq(strncmp(D_RO(rootg)->name2, ROOT_NAME, MAX_ROOT_NAME), 0);

	pmemobj_close(pop);
}

void
test_store_alloc_free(const char *path)
{
#define	_N_TEST_TYPES 3 /* number of types to test */

	PMEMobjpool *pop = NULL;
	OID_TYPE(struct tobject) tobj;
	uint64_t offsets[_N_TEST_TYPES];
	PMEMoid poid;
	int type_num;

	/* create a pool */
	if ((pop = pmemobj_create(path, LAYOUT_NAME,
					PMEMOBJ_MIN_POOL,
					S_IRWXU)) == NULL)
		FATAL("!pmemobj_create: %s", path);

	/* object store should be empty */
	for (type_num = 0; type_num < PMEMOBJ_NUM_OID_TYPES; type_num++) {
		poid = pmemobj_first(pop, type_num);
		ASSERTeq(poid.off, 0);
	}

	/* write to object store */
	for (type_num = 0; type_num < _N_TEST_TYPES; type_num++) {
		OID_ASSIGN(tobj, pmemobj_alloc(pop,
					sizeof (struct tobject), type_num));
		ASSERT(OID_IS_NULL(tobj) == 0);

		/* save offset to check it later */
		offsets[type_num] = tobj.oid.off;

		D_RW(tobj)->value = type_num;
		pop->persist(&D_RW(tobj)->value, sizeof (uint8_t));
	}

	/* re-open the pool */
	pmemobj_close(pop);
	if ((pop = pmemobj_open(path, LAYOUT_NAME)) == NULL)
		FATAL("!pmemobj_open: %s", path);

	/* verify object store */
	for (type_num = 0; type_num < _N_TEST_TYPES; type_num++) {
		OID_ASSIGN(tobj, pmemobj_first(pop, type_num));
		ASSERTeq(tobj.oid.off, offsets[type_num]);
		ASSERTeq(D_RO(tobj)->value, type_num);

		poid = pmemobj_next(tobj.oid);
		ASSERTeq(poid.off, 0);
	}

	/* free object store */
	for (type_num = 0; type_num < _N_TEST_TYPES; type_num++) {
		poid = pmemobj_first(pop, type_num);
		ASSERTne(poid.off, 0);
		pmemobj_free(poid);
	}

	/* re-open the pool */
	pmemobj_close(pop);
	if ((pop = pmemobj_open(path, LAYOUT_NAME)) == NULL)
		FATAL("!pmemobj_open: %s", path);

	/* check if objects were really freed */
	for (type_num = 0; type_num < _N_TEST_TYPES; type_num++) {
		poid = pmemobj_first(pop, type_num);
		ASSERTeq(poid.off, 0);
	}

	pmemobj_close(pop);

#undef	_N_TEST_TYPES
}

void
test_store_for_each(const char *path)
{
#define	_MAX_TYPES	3	/* number of types to test */
#define	_MAX_ELEMENTS	4	/* number of elements in each type to test */

	char bitmap[32]; /* bitmap of values of type uint8_t (32 = 256/8) */
	PMEMobjpool *pop = NULL;
	OID_TYPE(struct tobject) tobj;
	uint8_t value;
	int i, type;

	memset(bitmap, 0, sizeof (bitmap));

	/* create a pool */
	if ((pop = pmemobj_create(path, LAYOUT_NAME,
					PMEMOBJ_MIN_POOL,
					S_IRWXU)) == NULL)
		FATAL("!pmemobj_create: %s", path);

	/* write to object store */
	for (type = 0; type < _MAX_TYPES; type++)
		for (i = 0; i < _MAX_ELEMENTS; i++) {
			OID_ASSIGN(tobj, pmemobj_alloc(pop,
						sizeof (struct tobject), type));
			ASSERT(OID_IS_NULL(tobj) == 0);
			value = _MAX_ELEMENTS * type + i;
			ASSERT(isclr(bitmap, value));
			setbit(bitmap, value);
			D_RW(tobj)->value = value;
			pop->persist(&D_RW(tobj)->value, sizeof (uint8_t));
		}

	/* re-open the pool */
	pmemobj_close(pop);
	if ((pop = pmemobj_open(path, LAYOUT_NAME)) == NULL)
		FATAL("!pmemobj_open: %s", path);

	/* test POBJ_FOREACH */
	i = 0;
	PMEMoid varoid;
	POBJ_FOREACH(pop, varoid, type) {
		ASSERT(i < _MAX_TYPES * _MAX_ELEMENTS);
		OID_ASSIGN(tobj, varoid);
		ASSERT(OID_IS_NULL(tobj) == 0);
		ASSERT(isset(bitmap, D_RO(tobj)->value));
		i++;
	}
	ASSERTeq(i, _MAX_TYPES * _MAX_ELEMENTS);

	/* test POBJ_FOREACH_TYPE */
	i = 0;
	for (type = 0; type < _MAX_TYPES; type++) {
		POBJ_FOREACH_TYPE(pop, tobj, type) {
			ASSERT(i < (type + 1) * _MAX_ELEMENTS);
			ASSERT(OID_IS_NULL(tobj) == 0);
			ASSERT(isset(bitmap, D_RO(tobj)->value));
			i++;
		}
		ASSERTeq(i, (type + 1) * _MAX_ELEMENTS);
	}
	ASSERTeq(i, _MAX_TYPES * _MAX_ELEMENTS);

	/* test POBJ_FOREACH_SAFE */
	PMEMoid nvaroid;
	i = 0;
	POBJ_FOREACH_SAFE(pop, varoid, nvaroid, type) {
		ASSERTne(varoid.off, 0);
		pmemobj_free(varoid);
		i++;
	}
	ASSERTeq(i, _MAX_TYPES * _MAX_ELEMENTS);

	pmemobj_close(pop);

#undef	_MAX_TYPES
#undef	_MAX_ELEMENTS
}

void
test_user_lists(const char *path)
{
#define	_USER_TYPE 7
#define	_N_OBJECTS 5

	char bitmap[32]; /* bitmap of values of type uint8_t (32 = 256/8) */
	PMEMobjpool *pop = NULL;
	OID_TYPE(struct root) root;
	OID_TYPE(struct tobject) tobj;
	uint8_t value;
	int i;

	memset(bitmap, 0, sizeof (bitmap));

	/* create a pool */
	if ((pop = pmemobj_create(path, LAYOUT_NAME,
					PMEMOBJ_MIN_POOL,
					S_IRWXU)) == NULL)
		FATAL("!pmemobj_create: %s", path);

	/* create root object */
	OID_ASSIGN(root, pmemobj_root(pop, sizeof (struct root)));
	ASSERT(OID_IS_NULL(root) == 0);
	ASSERTeq(pmemobj_root_size(pop), sizeof (struct root));

	/* fill in root object */
	strncpy(D_RW(root)->name, ROOT_NAME, MAX_ROOT_NAME);
	D_RW(root)->value = ROOT_VALUE;
	pop->persist(D_RW(root), sizeof (struct root));

	/* add _N_OBJECTS elements to the user list */
	for (i = 0; i < _N_OBJECTS; i++) {
		OID_ASSIGN(tobj, POBJ_LIST_INSERT_NEW_HEAD(pop,
				&D_RW(root)->lhead, _USER_TYPE, next));
		ASSERT(OID_IS_NULL(tobj) == 0);
		value = i + 1;
		ASSERT(isclr(bitmap, value));
		setbit(bitmap, value);
		D_RW(tobj)->value = value;
		pop->persist(&D_RW(tobj)->value, sizeof (uint8_t));
	}

	/* re-open the pool */
	pmemobj_close(pop);
	if ((pop = pmemobj_open(path, LAYOUT_NAME)) == NULL)
		FATAL("!pmemobj_open: %s", path);

	/* test POBJ_FOREACH_TYPE */
	i = 0;
	POBJ_FOREACH_TYPE(pop, tobj, _USER_TYPE) {
		ASSERT(i <= _N_OBJECTS);
		ASSERT(OID_IS_NULL(tobj) == 0);
		ASSERT(isset(bitmap, D_RO(tobj)->value));
		i++;
	}
	ASSERTeq(i, _N_OBJECTS);

	/* get root object */
	OID_ASSIGN(root, pmemobj_root(pop, sizeof (struct root)));
	ASSERT(OID_IS_NULL(root) == 0);
	ASSERTeq(pmemobj_root_size(pop), sizeof (struct root));

	/* test POBJ_LIST_FOREACH_REVERSE */
	i = 0;
	POBJ_LIST_FOREACH_REVERSE(tobj, &D_RW(root)->lhead, next) {
		ASSERT(i <= _N_OBJECTS);
		ASSERT(OID_IS_NULL(tobj) == 0);
		ASSERT(isset(bitmap, D_RO(tobj)->value));
		i++;
	}
	ASSERTeq(i, _N_OBJECTS);

	/* test POBJ_LIST_FOREACH */
	i = _N_OBJECTS - 1;
	POBJ_LIST_FOREACH(tobj, &D_RW(root)->lhead, next) {
		ASSERT(i <= _N_OBJECTS);
		ASSERT(OID_IS_NULL(tobj) == 0);
		ASSERT(isset(bitmap, D_RO(tobj)->value));
		i--;
	}
	ASSERTeq(i, -1);

	pmemobj_close(pop);
}

void
test_null_oids(void)
{
	pmemobj_free(OID_NULL);

	ASSERTeq(pmemobj_alloc_usable_size(OID_NULL), 0);

	PMEMoid next = pmemobj_next(OID_NULL);
	ASSERT(next.off == 0 && next.pool_uuid_lo == 0);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_store");

	if (argc != 3)
		FATAL("usage: %s file-name op:r|a|f|u|n", argv[0]);

	const char *path = argv[1];

	if (strchr("rafun", argv[2][0]) == NULL || argv[2][1] != '\0')
		FATAL("op must be r or a or f or u or n");

	switch (argv[2][0]) {
		case 'r':
			test_root_object(path);
			break;
		case 'a':
			test_store_alloc_free(path);
			break;
		case 'f':
			test_store_for_each(path);
			break;
		case 'u':
			test_user_lists(path);
			break;
		case 'n':
			test_null_oids();
			break;
	}

	DONE(NULL);
}