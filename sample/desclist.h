#ifndef __DESCLIST__H_
#define __DESCLIST__H_


typedef ProtobufCMessageDescriptor pbmsg_desc_t;

typedef struct desc_list_s {
	char				*name;
	const pbmsg_desc_t	*desc;

} desc_list_t;



extern desc_list_t g_desclist[];

#endif
