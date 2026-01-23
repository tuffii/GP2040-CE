/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2023 Brian S. Stephan <bss@incorporeal.org>
 */

#ifndef TYPES_H_
#define TYPES_H_

// common types
#define	Pin_t		int32_t		// signed to accommodate for -1
#define Mask_t		uint32_t

struct usage_def_t {
    uint8_t report_id;
    uint8_t size;
    uint16_t bitpos;
    bool is_relative;
    bool is_array = false;
    bool should_be_scaled = false;
    int32_t logical_minimum;
    int32_t logical_maximum;
    uint32_t index = 0;
    uint32_t count = 0;
    uint32_t usage_maximum;
    int32_t* input_state_0 = NULL;
    int32_t* input_state_n = NULL;
    uint8_t index_mask = 0;
};

#endif
