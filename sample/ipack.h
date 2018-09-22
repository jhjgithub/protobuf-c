#ifndef __IPACK_HEADER_H
#define __IPACK_HEADER_H


#include <protobuf-c.h>
typedef ProtobufCMessage pbmsg_t;

#define STATIC              static
#define member_sizeof(type, member) sizeof(((type *)0)->member)

#define IPACK_MAGIC          "iPACK"
#define IPACK_MAGIC_SIZE     5
#define IPACK_VER            1

/*
 *** iPack format ***
   iPack Format : Header + Record1 + Record2 + ... + Recordn

   Header: magic(5) + version(1) + reserve(2) + total_size(4) + number_of_record(4)
   Record : type_name_size(1) + type_name + data_name(1) + data_name + data_size(4) + data

   If the length of data is more than a byte,
   it will be converted to Network Order
   before saving it.

 */

typedef struct _ipack_hdr_s {
	char		magic[IPACK_MAGIC_SIZE];
	uint8_t		version;
	uint8_t		reserve[2];
	uint32_t	sz_total;   // not include header data, saved Network Order
	uint32_t	n_record;   // saved Network Order
} ipack_hdr_t;

typedef struct _ipack_name_s {
	uint8_t		len;		// max length is 256
	char		*name;
} ipack_name_t;

typedef struct _ipack_buf_s {
	uint32_t	len;
	char		*buf;
	uint32_t	used;
} ipack_buf_t;

#define IPACK_RECORD_DECODED    0x01

#define IPACK_MAX_REC_NAME		2

typedef struct _ipack_record_s {
	// not saved members
	struct _ipack_record_s	*next;
	uint32_t				flags;


	// saved members
	ipack_name_t	names[IPACK_MAX_REC_NAME];	// 0:type name, 1: data name
	uint32_t		dlen;						// saved Network Order
	uint8_t			*data;
	uint16_t		crc;

} ipack_record_t;

typedef struct _ipack_s {
	uint32_t	sz_total;
	uint32_t	n_record;

	ipack_record_t *first, *last;
} ipack_t;

/////////////////////////////////////////////

typedef int32_t (*CB_IPACK_IO)(void *arg, void *buf, uint32_t len);

/////////////////////////////////////////////////////


ipack_t* ipack_init(void);
void     ipack_clean(ipack_t *ipack);
void*    ipack_dup(ipack_t *ipack);
void*    ipack_alloc(uint32_t len);
void*	 ipack_realloc(void *mem, uint32_t len);
void     ipack_free(void *mem);
void	 ipack_free_record(ipack_record_t *record);
char*	 ipack_strdup(const char *str);

int32_t ipack_add_record(ipack_t *ipack, char *data_name, void *data);
void*   ipack_get_record(ipack_t *ipack, char *data_name, char *type_name);
void*   ipack_get_record_ext(ipack_t *ipack, char *data_name, void *data_type);

ipack_t* ipack_convert(ipack_t *ipack);

int32_t ipack_mod_record(ipack_t *ipack, char *data_name, char *type_name, void *data);
void    ipack_pop_record(ipack_t *ipack, char *data_name, char *type_name);

int32_t ipack_read_from_file(ulong arg, void *buf, uint32_t len);
int32_t ipack_read_from_buffer(void *arg, void *buf, uint32_t len);
int32_t read_to_dummy(void *arg, void *buf, uint32_t len);
int32_t ipack_deserialize(ipack_t *ipack, CB_IPACK_IO cb_ipack_read, void *arg);

int32_t ipack_write_to_file(ulong arg, void *buf, uint32_t len);
int32_t ipack_write_to_buffer(void *arg, void *buf, uint32_t len);
int32_t write_to_dummy(void *arg, void *buf, uint32_t len);
int32_t ipack_serialize(ipack_t *ipack, CB_IPACK_IO cb_ipack_write, void *arg);

int32_t ipack_delete_record_data(void *data);

int32_t ipack_write_byte(CB_IPACK_IO cb_ipack_write, void *arg, void *buf, uint32_t len, uint32_t *written);
int32_t ipack_read_byte(CB_IPACK_IO cb_ipack_read, void *arg, void *buf, uint32_t len, uint32_t *nread);

int32_t  ipack_decode_record(ipack_record_t *record);
void     ipack_put_last(ipack_t *ipack, ipack_record_t *record);
uint16_t ipack_make_crc(uint8_t *data, uint32_t len);

void* ipack_encode_data(void *_msg, uint32_t *packed_size);
void* ipack_decode_data(void *data, uint32_t len, const char *desc_name);

void* ipack_dup_record(ipack_record_t *r);

ProtobufCMessageDescriptor* ipack_find_desc(const char *type_name);

#endif
