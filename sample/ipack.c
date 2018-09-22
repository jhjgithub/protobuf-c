//#define _BSD_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "ipack.h"
#include "desclist.h"


/////////////////////////////////////////////////////////
// internal function

STATIC int32_t ipack_write_header(ipack_hdr_t *hdr, uint32_t sz_total, uint32_t n_record)
{
	memset(hdr, 0, sizeof(ipack_hdr_t));

	// magic code: 5bytes
	memcpy(hdr->magic, IPACK_MAGIC, IPACK_MAGIC_SIZE);

	// versoin: 1bytes
	hdr->version = IPACK_VER;
	hdr->sz_total	= htonl(sz_total);
	hdr->n_record	= htonl(n_record);

	return sizeof(ipack_hdr_t);
}

/////////////////////////////////////////////////////////
// external function

void* ipack_alloc(uint32_t len)
{
	return malloc(len);
}

void* ipack_realloc(void *mem, uint32_t len)
{
	return realloc(mem, len);
}

void ipack_free(void *mem)
{
	if (mem) {
		free(mem);
	}
}

char* ipack_strdup(const char *str)
{
	return strdup(str);
}

void ipack_free_record(ipack_record_t *record)
{
	int32_t i;

	if (record == NULL) {
		return;
	}

	for (i=0; i<IPACK_MAX_REC_NAME; i++) {
		if (record->names[i].len > 0) {
			ipack_free(record->names[i].name);
		}
	}

	if (record->dlen > 0) {
		if (record->flags & IPACK_RECORD_DECODED) {
			ipack_delete_record_data(record->data);
		}
		else {
			ipack_free(record->data);
		}
	}

	ipack_free(record);
}

ipack_t* ipack_init(void)
{
	ipack_t *ipack;

	ipack = ipack_alloc(sizeof(ipack_t));

	if (ipack == NULL) {
		return NULL;
	}

	memset(ipack, 0, sizeof(ipack_t));

	return ipack;
}

void ipack_clean(ipack_t *ipack)
{
	ipack_record_t *r, *n;

	r = ipack->first;

	while (r != NULL) {
		n	= r->next;
		ipack_free_record(r);
		r	= n;
	}

	ipack_free(ipack);
}

void* ipack_dup(ipack_t *ipack)
{
	ipack_record_t *r, *n, *rb, *nb;
	ipack_t *ret_ipack;

	if (!(ret_ipack = ipack_init())) {
		return NULL;
	}

	r = ipack->first;

	while (r != NULL) {
		rb = ipack_dup_record(r);

		if (r == ipack->first) {
			ret_ipack->first = rb;
		}
		else {
			nb->next = rb;
		}

		nb = rb;

		n	= r->next;
		r	= n;
	}

	return ret_ipack;
}

char* ipack_copy_name(char *name, ipack_name_t *pname)
{
	if (name == NULL) {
		pname->len = 0;
		return NULL;
	}

	pname->name = ipack_strdup(name);
	pname->len = strlen(name);

	return pname->name;
}

int32_t ipack_add_record(ipack_t *ipack, char *data_name, void *data)
{
	uint32_t len;
	ipack_record_t	*record = NULL;
	const pbmsg_t	*msg = (pbmsg_t *)data;

	len = protobuf_c_message_get_packed_size(msg);

	if (len < 1) {
		return -1;
	}

	record = (ipack_record_t *)ipack_alloc(sizeof(ipack_record_t));
	if (record == NULL) {
		return -1;
	}

	memset(record, 0, sizeof(ipack_record_t));

	record->next = NULL;
	record->data = (uint8_t *)ipack_alloc(len);
	if (record->data == NULL) {
		goto ERR;
	}

	// serialize data to memory
	if (protobuf_c_message_pack(msg, record->data) != len) {
		goto ERR;
	}

	record->dlen = len;

	// type name
	if (ipack_copy_name((char*)msg->descriptor->name, &record->names[0]) == NULL) {
		goto ERR;
	}

	// data name
	ipack_copy_name(data_name, &record->names[1]);

	record->crc = ipack_make_crc(record->data, record->dlen);

	ipack_put_last(ipack, record);

	return 0;

ERR:

	ipack_free_record(record);

	return -1;
}

pbmsg_desc_t* ipack_find_desc(const char *type_name)
{
	int32_t i;

	i = 0;

	while (g_desclist[i].name != NULL) {
		if (strcmp(g_desclist[i].name, type_name) == 0) {
			return (pbmsg_desc_t *)g_desclist[i].desc;
		}

		i++;
	}

	return NULL;
}

int32_t ipack_decode_record(ipack_record_t *record)
{
	pbmsg_desc_t *desc;
	pbmsg_t *msg;

	if (record->flags & IPACK_RECORD_DECODED) {
		return 0;
	}

	desc = ipack_find_desc(record->names[0].name);
	if (desc == NULL) {

		//printf("Cannot find desc: type=%s \n", record->tname);
		return -1;
	}

	msg = protobuf_c_message_unpack(desc, NULL, record->dlen, record->data);
	if (msg == NULL) {
		return -1;
	}

	msg->descriptor = desc;
	record->flags	|= IPACK_RECORD_DECODED;
	ipack_free(record->data);
	record->data = (void *)msg;
	record->dlen = desc->sizeof_message;

	return 0;
}

void* ipack_encode_data(void *_msg, uint32_t *packed_size)
{
	pbmsg_t *msg = (pbmsg_t*)_msg;
	uint32_t len;
	unsigned char *data = NULL;

	if (!msg) {
		goto END;
	}

	if ((len = protobuf_c_message_get_packed_size(msg)) < 1) {
		goto END;
	}

	if (!(data = ipack_alloc(len))) {
		goto END;
	}

	// serialize data to memory
	if (protobuf_c_message_pack(msg, data) != len) {
		ipack_free(data);
		data = NULL;
	}

	*packed_size = len;

END:
	return (void*)data;
}

void* ipack_decode_data(void *data, uint32_t len, const char *desc_name)
{
	pbmsg_desc_t *desc;
	pbmsg_t *msg;

	if (!(desc = ipack_find_desc(desc_name))) {
		return NULL;
	}

	if (!(msg = protobuf_c_message_unpack(desc, NULL, len, data))) {
		return NULL;
	}

	msg->descriptor = desc;

	return (void*)msg;
}

void* ipack_dup_record(ipack_record_t *r)
{
	ipack_record_t *rr;

	if (!r) {
		return NULL;
	}

	rr = (ipack_record_t *)ipack_alloc(sizeof(ipack_record_t));
	memcpy(rr, r, sizeof(ipack_record_t));
	rr->data = ipack_alloc(r->dlen);
	memcpy(rr->data, r->data, r->dlen);
	rr->dlen = r->dlen;
	rr->next = NULL;

	return rr;
}

int32_t ipack_write_byte(CB_IPACK_IO cb_ipack_write, void *arg, void *buf, uint32_t len, uint32_t *written)
{
	int32_t wlen;

	wlen = cb_ipack_write(arg, buf, len);
	if (wlen != len) {
		return -1;
	}

	(*written) += len;

	return 0;
}

int32_t ipack_read_byte(CB_IPACK_IO cb_ipack_read, void *arg, void *buf, uint32_t len, uint32_t *nread)
{
	int32_t rlen;

	rlen = cb_ipack_read(arg, buf, len);
	if (rlen != len) {
		return -1;
	}

	(*nread) += len;

	return 0;
}

void ipack_put_last(ipack_t *ipack, ipack_record_t *record)
{
	if (ipack == NULL || record == NULL) {
		return;
	}

	if (ipack->last == NULL) {
		// first time
		ipack->first = record;
	}
	else {
		ipack->last->next = record;
	}

	ipack->last = record;
	ipack->n_record++;

	ipack->sz_total += record->names[0].len;
	ipack->sz_total += record->names[1].len;
	ipack->sz_total += record->dlen;
	ipack->sz_total += member_sizeof(ipack_name_t, len) * 2;
	ipack->sz_total += member_sizeof(ipack_record_t, dlen);
	ipack->sz_total += member_sizeof(ipack_record_t, crc);
}

uint16_t ipack_make_crc(uint8_t *data, uint32_t len)
{
	return 0;
}

/////////////////////////////////////////////////////////////

int32_t ipack_write_to_file(ulong arg, void *buf, uint32_t len)
{
	return write((int)arg, (const void *)buf, (size_t)len);
}

int32_t ipack_write_to_buffer(void *arg, void *buf, uint32_t len)
{
	ipack_buf_t *b = (ipack_buf_t *)arg;

	// out of buffer ?
	if ((b->used + len) > b->len) {
		return -1;
	}

	memcpy(&b->buf[b->used], buf, len);
	b->used += len;

	return len;
}

int32_t write_to_dummy(void *arg, void *buf, uint32_t len)
{
	return len;
}

int32_t ipack_serialize(ipack_t *ipack, CB_IPACK_IO cb_ipack_write, void *arg)
{
	ipack_hdr_t hdr;
	int32_t		len, i;
	uint32_t	tmp, written = 0;
	ipack_record_t *r;
	ipack_name_t *n;
	uint16_t crc;

	len = ipack_write_header(&hdr, ipack->sz_total, ipack->n_record);

	// 1. header
	if (ipack_write_byte(cb_ipack_write, arg, &hdr, len, &written)) {
		return -1;
	}

	// 2. each record
	r = ipack->first;

	while (r != NULL) {
		// write names
		for (i=0; i<IPACK_MAX_REC_NAME; i++) {
			n = &r->names[i];
			
			// write length of name string
			if (ipack_write_byte(cb_ipack_write, arg, &n->len,
								 member_sizeof(ipack_name_t, len), &written)) {
				return -1;
			}

			// write name string itself
			if (n->len &&
				ipack_write_byte(cb_ipack_write, arg, n->name, n->len, &written)) {
				return -1;
			}
		}

		// write data length
		tmp = htonl(r->dlen);
		if (ipack_write_byte(cb_ipack_write, arg, &tmp, member_sizeof(ipack_record_t, dlen), &written)) {
			return -1;
		}

		// data
		if (r->dlen) {
			if (ipack_write_byte(cb_ipack_write, arg, r->data, r->dlen, &written)) {
				return -1;
			}

			crc = htons(r->crc);

			if (ipack_write_byte(cb_ipack_write, arg, &crc, sizeof(crc), &written)) {
				return -1;
			}
		}

		r = r->next;
	}

	return written;
}

/////////////////////////////////////////////////////////////

int32_t ipack_read_from_file(ulong arg, void *buf, uint32_t len)
{
	return read((int)arg, buf, len);
}

int32_t ipack_read_from_buffer(void *arg, void *buf, uint32_t len)
{
	ipack_buf_t *b = (ipack_buf_t *)arg;

	if ((b->used + len) > b->len) {
		return -1;
	}

	memcpy(buf, &b->buf[b->used], len);
	b->used += len;

	return len;
}

int32_t read_to_dummy(void *arg, void *buf, uint32_t len)
{
	    return len;
}

int32_t ipack_deserialize(ipack_t *ipack, CB_IPACK_IO cb_ipack_read, void *arg)
{
	uint32_t	nread = 0, i, j;
	uint32_t	sz_total, n_record, sz_data;
	ipack_record_t *r = NULL;
	ipack_name_t *n;
	ipack_hdr_t hdr;
	uint16_t crc;

	if (ipack_read_byte(cb_ipack_read, arg, &hdr, sizeof(ipack_hdr_t), &nread)) {
		return -1;
	}

	if (memcmp(hdr.magic, IPACK_MAGIC, IPACK_MAGIC_SIZE) != 0) {
		return -1;
	}

	if (hdr.version != IPACK_VER) {
		return -2;
	}

	sz_total = ntohl(hdr.sz_total);
	n_record = 2;    // iRPC has two records;
	nread = 0;

	for (i = 0; i < n_record; i++) {
		r = ipack_alloc(sizeof(ipack_record_t));
		if (r == NULL) {
			goto ERR;
		}

		memset(r, 0, sizeof(ipack_record_t));
		r->next = NULL;

		// read names
		for (j=0; j<IPACK_MAX_REC_NAME; j++) {
			n = &r->names[j];

			if (ipack_read_byte(cb_ipack_read, arg, &n->len, member_sizeof(ipack_name_t, len), &nread)) {
				goto ERR;
			}

			if (n->len > 0) {
				n->name = ipack_alloc(n->len + 1);

				if (ipack_read_byte(cb_ipack_read, arg, n->name, n->len, &nread)) {
					goto ERR;
				}

				n->name[n->len] = 0;
			}
		}

		// data
		if (ipack_read_byte(cb_ipack_read, arg, &sz_data, sizeof(sz_data), &nread)) {
			goto ERR;
		}

		r->dlen = ntohl(sz_data);

		r->data = ipack_alloc(r->dlen);
		if (r->data == NULL) {
			goto ERR;
		}

		if (ipack_read_byte(cb_ipack_read, arg, r->data, r->dlen, &nread)) {
			goto ERR;
		}

		if (ipack_read_byte(cb_ipack_read, arg, &crc, sizeof(crc), &nread)) {
			goto ERR;
		}

		r->crc = ntohs(crc);
		crc = ipack_make_crc(r->data, r->dlen);

		if (r->crc != crc) {
			goto ERR;
		}

		ipack_decode_record(r);
		ipack_put_last(ipack, r);
	}

	return 0;

ERR:

	ipack_free_record(r);

	return -1;
}

void* ipack_get_record(ipack_t *ipack, char *data_name, char *type_name)
{
	ipack_record_t *r;

	r = ipack->first;

	while (r) {
		if (strcmp(r->names[1].name, data_name) == 0 &&
			(type_name ? (strcmp(r->names[0].name, type_name) == 0) : 1)) {

			if (r->flags & IPACK_RECORD_DECODED) {
				return r->data;
			}
			else {
				return NULL;
			}
		}

		r = r->next;
	}

	return NULL;
}

void* ipack_get_record_ext(ipack_t *ipack, char *data_name, void *data_type)
{
	const char *type_name;
	const pbmsg_t *msg_type = (const pbmsg_t *)data_type;

	if (msg_type == NULL || msg_type->descriptor == NULL) {
		return NULL;
	}

	type_name = msg_type->descriptor->name;

	if (type_name == NULL) {
		return NULL;
	}

	return ipack_get_record(ipack, data_name, (char *)type_name);
}

ipack_t* ipack_convert(ipack_t *ipack)
{
	ipack_t *rv;
	ipack_record_t *r;

	if (!(rv = ipack_init())) {
		return NULL;
	}

	r = ipack->first;

	while (r) {
		if (r->flags & IPACK_RECORD_DECODED) {
			ipack_add_record(rv, r->names[1].name, r->data);
		}

		r = r->next;
	}

	return rv;
}

int32_t ipack_mod_record(ipack_t *ipack, char *data_name, char *type_name, void *data)
{
	ipack_record_t *r;

	r = ipack->first;

	while (r) {
		if (strcmp(r->names[1].name, data_name) == 0 &&
			(type_name ? (strcmp(r->names[0].name, type_name) == 0) : 1)) {

			if (r->flags & IPACK_RECORD_DECODED) {
				ipack_delete_record_data(r->data);
				r->data = data;
				r->dlen = ((pbmsg_t*)data)->descriptor->sizeof_message;
			}
			else {
				return -1;
			}

			return 0;
		}

		r = r->next;
	}

	return -1;
}

void ipack_pop_record(ipack_t *ipack, char *data_name, char *type_name)
{
	ipack_record_t *r, *p = NULL, *n = NULL;

	r = ipack->first;

	while (r) {
		n = r->next;

		if (strcmp(r->names[1].name, data_name) == 0 &&
			(type_name ? (strcmp(r->names[0].name, type_name) == 0) : 1)) {

			if (p) {
				p->next = r->next;
			}
			else {
				ipack->first = r->next;
			}

			ipack_free_record(r);
			ipack->n_record--;

			r = n;
			continue;
		}

		p = r;
		r = r->next;
	}

	ipack->last = p;
}

int32_t ipack_delete_record_data(void *data)
{
	pbmsg_t *msg = (pbmsg_t *)data;

	//PROTOBUF_C_ASSERT(msg->descriptor != NULL);

	if (msg->descriptor == NULL) {
		return -1;
	}

	protobuf_c_message_free_unpacked(msg, NULL);

	return 0;
}
