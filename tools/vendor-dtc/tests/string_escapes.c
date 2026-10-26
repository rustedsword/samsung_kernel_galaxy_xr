// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for string escapes in dtc
 * Copyright (C) 2006 David Gibson, IBM Corporation.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

int main(int argc, char *argv[])
{
	const char nul_digit_str[] = "\0000\0001\0002\0003\0004\0005\0006\0007";
	const fdt32_t nul_digit_cells[] = {
		cpu_to_fdt32(0x00324b00), cpu_to_fdt32(0x00334500),
	};
	void *fdt;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	check_getprop(fdt, 0, "escape-str",
		      strlen(TEST_STRING_2)+1, TEST_STRING_2);
	check_getprop(fdt, 0, "escape-str-2",
		      strlen(TEST_STRING_3)+1, TEST_STRING_3);
	check_getprop(fdt, 0, "nul-digit-str",
		      sizeof(nul_digit_str), nul_digit_str);
	check_getprop(fdt, 0, "nul-digit-cells",
		      sizeof(nul_digit_cells), nul_digit_cells);

	PASS();
}
