/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
#ifndef __UHID_H_
#define __UHID_H_

/*
 * User-space I/O driver support for HID subsystem
 * Copyright (c) 2012 David Herrmann
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

/*
 * Public header for user-space communication. We try to keep every structure
 * aligned but to be safe we also use __attribute__((__packed__)). Therefore,
 * the communication should be ABI compatible even between architectures.
 */

#include <stdint.h>

#define BUS_PCI			0x01
#define BUS_ISAPNP		0x02
#define BUS_USB			0x03
#define BUS_HIL			0x04
#define BUS_BLUETOOTH		0x05
#define BUS_VIRTUAL		0x06

#define HID_MAX_DESCRIPTOR_SIZE		4096

enum uhid_event_type {
	__UHID_LEGACY_CREATE,
	UHID_DESTROY,
	UHID_START,
	UHID_STOP,
	UHID_OPEN,
	UHID_CLOSE,
	UHID_OUTPUT,
	__UHID_LEGACY_OUTPUT_EV,
	__UHID_LEGACY_INPUT,
	UHID_GET_REPORT,
	UHID_GET_REPORT_REPLY,
	UHID_CREATE2,
	UHID_INPUT2,
	UHID_SET_REPORT,
	UHID_SET_REPORT_REPLY,
};

struct uhid_create2_req {
	uint8_t name[128];
	uint8_t phys[64];
	uint8_t uniq[64];
	uint16_t rd_size;
	uint16_t bus;
	uint32_t vendor;
	uint32_t product;
	uint32_t version;
	uint32_t country;
	uint8_t rd_data[HID_MAX_DESCRIPTOR_SIZE];
} __attribute__((__packed__));

enum uhid_dev_flag {
	UHID_DEV_NUMBERED_FEATURE_REPORTS			= (1ULL << 0),
	UHID_DEV_NUMBERED_OUTPUT_REPORTS			= (1ULL << 1),
	UHID_DEV_NUMBERED_INPUT_REPORTS				= (1ULL << 2),
};

struct uhid_start_req {
	uint64_t dev_flags;
};

#define UHID_DATA_MAX 4096

enum uhid_report_type {
	UHID_FEATURE_REPORT,
	UHID_OUTPUT_REPORT,
	UHID_INPUT_REPORT,
};

struct uhid_input2_req {
	uint16_t size;
	uint8_t data[UHID_DATA_MAX];
} __attribute__((__packed__));

struct uhid_output_req {
	uint8_t data[UHID_DATA_MAX];
	uint16_t size;
	uint8_t rtype;
} __attribute__((__packed__));

struct uhid_get_report_req {
	uint32_t id;
	uint8_t rnum;
	uint8_t rtype;
} __attribute__((__packed__));

struct uhid_get_report_reply_req {
	uint32_t id;
	uint16_t err;
	uint16_t size;
	uint8_t data[UHID_DATA_MAX];
} __attribute__((__packed__));

struct uhid_set_report_req {
	uint32_t id;
	uint8_t rnum;
	uint8_t rtype;
	uint16_t size;
	uint8_t data[UHID_DATA_MAX];
} __attribute__((__packed__));

struct uhid_set_report_reply_req {
	uint32_t id;
	uint16_t err;
} __attribute__((__packed__));

/*
 * Compat Layer
 * All these commands and requests are obsolete. You should avoid using them in
 * new code. We support them for backwards-compatibility, but you might not get
 * access to new feature in case you use them.
 */

enum uhid_legacy_event_type {
	UHID_CREATE			= __UHID_LEGACY_CREATE,
	UHID_OUTPUT_EV			= __UHID_LEGACY_OUTPUT_EV,
	UHID_INPUT			= __UHID_LEGACY_INPUT,
	UHID_FEATURE			= UHID_GET_REPORT,
	UHID_FEATURE_ANSWER		= UHID_GET_REPORT_REPLY,
};

/* Obsolete! Use UHID_CREATE2. */
struct uhid_create_req {
	uint8_t name[128];
	uint8_t phys[64];
	uint8_t uniq[64];
	uint8_t *rd_data;
	uint16_t rd_size;

	uint16_t bus;
	uint32_t vendor;
	uint32_t product;
	uint32_t version;
	uint32_t country;
} __attribute__((__packed__));

/* Obsolete! Use UHID_INPUT2. */
struct uhid_input_req {
	uint8_t data[UHID_DATA_MAX];
	uint16_t size;
} __attribute__((__packed__));

/* Obsolete! Kernel uses UHID_OUTPUT exclusively now. */
struct uhid_output_ev_req {
	uint16_t type;
	uint16_t code;
	int32_t value;
} __attribute__((__packed__));

/* Obsolete! Kernel uses ABI compatible UHID_GET_REPORT. */
struct uhid_feature_req {
	uint32_t id;
	uint8_t rnum;
	uint8_t rtype;
} __attribute__((__packed__));

/* Obsolete! Use ABI compatible UHID_GET_REPORT_REPLY. */
struct uhid_feature_answer_req {
	uint32_t id;
	uint16_t err;
	uint16_t size;
	uint8_t data[UHID_DATA_MAX];
} __attribute__((__packed__));

/*
 * UHID Events
 * All UHID events from and to the kernel are encoded as "struct uhid_event".
 * The "type" field contains a UHID_* type identifier. All payload depends on
 * that type and can be accessed via ev->u.XYZ accordingly.
 * If user-space writes short events, they're extended with 0s by the kernel. If
 * the kernel writes short events, user-space shall extend them with 0s.
 */

struct uhid_event {
	uint32_t type;

	union {
		struct uhid_create_req create;
		struct uhid_input_req input;
		struct uhid_output_req output;
		struct uhid_output_ev_req output_ev;
		struct uhid_feature_req feature;
		struct uhid_get_report_req get_report;
		struct uhid_feature_answer_req feature_answer;
		struct uhid_get_report_reply_req get_report_reply;
		struct uhid_create2_req create2;
		struct uhid_input2_req input2;
		struct uhid_set_report_req set_report;
		struct uhid_set_report_reply_req set_report_reply;
		struct uhid_start_req start;
	} u;
} __attribute__((__packed__));

#endif /* __UHID_H_ */
