#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <cutils/log.h>
#include <linux/ioctl.h>
#include <fcntl.h>

#define IOCTL_HECI_CONNECT_CLIENT _IOWR('H' , 0x01, struct heci_connect_client_data)
#define SMHI_GET_TIME 8

typedef struct guid {
	unsigned int   data1;
	unsigned short data2;
	unsigned short data3;
	unsigned char  data4[8];
} GUID;

GUID HECI_guid={0xaa1dcd67, 0x308f, 0x4daa, {0xa2, 0xb9, 0x27, 0x82, 0xd4, 0x70, 0x3c, 0x5d}};
GUID SMHI_guid={0xBB579A2E, 0xCC54, 0x4450, {0xB1, 0xD0, 0x5E, 0x75, 0x20, 0xDC, 0xAD, 0x25}};

struct heci_client {
	unsigned int max_msg_length;
	unsigned char protocol_version;
	unsigned char reserved[3];
};

struct heci_connect_client_data {
	union {
		GUID in_client_uuid;
		struct heci_client out_client_properties;
	};
};

typedef struct
{
	uint8_t command : 7;
	uint8_t is_response : 1;
	uint8_t reserved[2];
	uint8_t status;
} SMHI_MSG_HEADER;

typedef struct {
	SMHI_MSG_HEADER header;
	uint64_t time_ms;
} SMHI_GET_TIME_RESPONSE;

int heci_open()
{
	int fd;
	int result;
	struct heci_connect_client_data connect_data;

	fd = open("/dev/ish", O_RDWR);
	if ( fd == -1 )
		printf("can not open /dev/ish\n");

	memcpy(&(connect_data.in_client_uuid),&(SMHI_guid),sizeof(GUID));

	result = ioctl(fd, IOCTL_HECI_CONNECT_CLIENT, &connect_data);
	if (result)
		close(fd);

	return fd;
}

void heci_close(int fd)
{
	if (fd != -1)
		close(fd);
}

int heci_read(int fd, void *buf, int size)
{
	if (fd == -1)
		return -1;

	return read(fd, buf, size);
}

int heci_write(int fd, void *buf, int size)
{
	if (fd == -1)
		return -1;

	return write(fd, buf, size);
}
