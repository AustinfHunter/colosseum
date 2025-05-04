#include <criterion/criterion.h>
#include <criterion/internal/assert.h>
#include <criterion/internal/test.h>
#include <stdio.h>
#include "../src/colosseum.h"

const int size = 10;
int *arr = NULL;

void suiteup(void) {
	arr = col_alloc(size*sizeof(int));
}

void suitedown(void) {
	col_free(arr);
}

TestSuite(colosseumtests, .init=suiteup, .fini=suitedown);

Test(colosseumtests, alloc) {
	cr_expect(arr != NULL, "cal_alloc should not return NULL.");
}

Test(colosseumtests, free) {
	col_free(arr);
}

Test(colosseumtests, correct_vals) {
	for (int i = 0; i < size; i++) {
		arr[i] = i+1;
	}
	int exp_arr[10] = {1,2,3,4,5,6,7,8,9,10};
	cr_expect_arr_eq(arr, exp_arr, size*sizeof(int), "Heap allocated array values should equal [1,2,3,4,5,6,7,8,9,10]");
}
