#pragma once

#ifndef TEST_IDT_H
#define TEST_IDT_H

#include <stdint.h>

#include "lib/util.h"

void test_divide_by_zero();
void test_debug_exception();
void test_nmi_exception();

#endif  // TEST_IDT_H 