/* This file is part of MCF Gthread.
 * See LICENSE.TXT for licensing information.
 * Copyleft 2022, LH_Mouse. All wrongs reserved.  */

#include "../src/win32.h"
#include <assert.h>
#include <stdio.h>
#include <ntsecapi.h>

static char comp[100];
static char data[100];

static
void
do_generate_random_bytes(void)
  {
    RtlGenRandom(comp, sizeof(comp));
    RtlMoveMemory(data, comp, sizeof(comp));
  }

int
main(void)
  {
    char* ptr;

    // __MCF_mfill
    do_generate_random_bytes();

    RtlFillMemory(comp + 20, 30, 'b');
    ptr = __MCF_mfill(data + 20, 'b', 30);
    assert(ptr == data + 20);

    RtlFillMemory(comp + 50, 20, 'c');
    ptr = __MCF_mfill(data + 50, 'c', 20);
    assert(ptr == data + 50);

    assert(RtlCompareMemory(comp, data, sizeof(comp)) == sizeof(comp));
    assert(__MCF_mequal(comp, data, sizeof(comp)) != 0);

    // __MCF_mzero
    do_generate_random_bytes();

    RtlFillMemory(comp + 20, 30, 0);
    ptr = __MCF_mzero(data + 20, 30);
    assert(ptr == data + 20);

    RtlFillMemory(comp + 50, 20, 0);
    ptr = __MCF_mzero(data + 50, 20);
    assert(ptr == data + 50);

    assert(RtlCompareMemory(comp, data, sizeof(comp)) == sizeof(comp));
    assert(__MCF_mequal(comp, data, sizeof(comp)) != 0);

    // __MCF_mcopy
    do_generate_random_bytes();

    RtlMoveMemory(comp + 10, comp + 80, 20);
    ptr = __MCF_mmove(data + 10, data + 80, 20);
    assert(ptr == data + 10);

    RtlMoveMemory(comp + 70, comp + 10, 30);
    ptr = __MCF_mmove(data + 70, data + 10, 30);
    assert(ptr == data + 70);

    assert(RtlCompareMemory(comp, data, sizeof(comp)) == sizeof(comp));
    assert(__MCF_mequal(comp, data, sizeof(comp)) != 0);

    // __MCF_mmove (forward)
    do_generate_random_bytes();

    RtlMoveMemory(comp + 10, comp + 20, 40);
    ptr = __MCF_mmove(data + 10, data + 20, 40);
    assert(ptr == data + 10);

    assert(RtlCompareMemory(comp, data, sizeof(comp)) == sizeof(comp));
    assert(__MCF_mequal(comp, data, sizeof(comp)) != 0);

    // __MCF_mmove (backward)
    do_generate_random_bytes();

    RtlMoveMemory(comp + 20, comp + 10, 40);
    ptr = __MCF_mmove(data + 20, data + 10, 40);
    assert(ptr == data + 20);

    assert(RtlCompareMemory(comp, data, sizeof(comp)) == sizeof(comp));
    assert(__MCF_mequal(comp, data, sizeof(comp)) != 0);

    // __MCF_mcomp (equal)
    do_generate_random_bytes();

    assert(RtlCompareMemory(comp, data, sizeof(comp)) == sizeof(comp));
    assert(__MCF_mcomp(comp, data, sizeof(comp)) == 0);

    // __MCF_mcomp (less)
    comp[70] = '1';
    data[70] = '2';

    assert(RtlCompareMemory(comp, data, sizeof(comp)) == 70);
    assert(__MCF_mcomp(comp, data, sizeof(comp)) < 0);

    // __MCF_mcomp (greater)
    comp[50] = '4';
    data[50] = '3';

    assert(RtlCompareMemory(comp, data, sizeof(comp)) == 50);
    assert(__MCF_mcomp(comp, data, sizeof(comp)) > 0);
  }