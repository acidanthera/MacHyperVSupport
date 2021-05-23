

#ifndef _HV_VMBUS_REGISTERS_H_
#define _HV_VMBUS_REGISTERS_H_

/*
 * NOTE: DO NOT CHANGE THIS.
 */
#define VMBUS_SINT_MESSAGE  2
/*
 * NOTE:
 * - DO NOT set it to the same value as VMBUS_SINT_MESSAGE.
 * - DO NOT set it to 0.
 */
#define VMBUS_SINT_TIMER  4

/*
 * NOTE: DO NOT CHANGE THESE
 */
#define VMBUS_CONNID_MESSAGE    1
#define VMBUS_CONNID_EVENT    2

#define VMBUS_MSG_DSIZE_MAX    240
#define VMBUS_MSG_SIZE      256


/*
 * Channel messages
 * - Embedded in vmbus_message.msg_data, e.g. response and notification.
 * - Embedded in hypercall_postmsg_in.hc_data, e.g. request.
 */

#define VMBUS_CHANMSG_TYPE_CHOFFER    1  /* NOTE */
#define VMBUS_CHANMSG_TYPE_CHRESCIND    2  /* NOTE */
#define VMBUS_CHANMSG_TYPE_CHREQUEST    3  /* REQ */
#define VMBUS_CHANMSG_TYPE_CHOFFER_DONE    4  /* NOTE */
#define VMBUS_CHANMSG_TYPE_CHOPEN    5  /* REQ */
#define VMBUS_CHANMSG_TYPE_CHOPEN_RESP    6  /* RESP */
#define VMBUS_CHANMSG_TYPE_CHCLOSE    7  /* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_CONN    8  /* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_SUBCONN  9  /* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_CONNRESP  10  /* RESP */
#define VMBUS_CHANMSG_TYPE_GPADL_DISCONN  11  /* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_DISCONNRESP  12  /* RESP */
#define VMBUS_CHANMSG_TYPE_CHFREE    13  /* REQ */
#define VMBUS_CHANMSG_TYPE_CONNECT    14  /* REQ */
#define VMBUS_CHANMSG_TYPE_CONNECT_RESP    15  /* RESP */
#define VMBUS_CHANMSG_TYPE_DISCONNECT    16  /* REQ */
#define VMBUS_CHANMSG_TYPE_17      17
#define VMBUS_CHANMSG_TYPE_18      18
#define VMBUS_CHANMSG_TYPE_19      19
#define VMBUS_CHANMSG_TYPE_20      20
#define VMBUS_CHANMSG_TYPE_TL_CONN    21  /* REQ */
#define VMBUS_CHANMSG_TYPE_22      22
#define VMBUS_CHANMSG_TYPE_TL_RESULT    23  /* RESP */
#define VMBUS_CHANMSG_TYPE_MAX      24

/*typedef struct {
  UInt32 type;   VMBUS_CHANMSG_TYPE_ */ /*
  UInt32 reserved;
} VMBusChannelMessageHeader;

typedef struct {
  VMBusChannelMessageHeader header;
  HyperVGuid                channelType;
  HyperVGuid                channelInstance;
} VMBusChannelMssageOffer;

//
// VMBUS_CHANMSG_TYPE_CHREQUEST
//
typedef struct {
  VMBusChannelMessageHeader header;
} VMBusChannelMessageChannelRequest;

//
// VMBUS_CHANMSG_TYPE_CONNECT
//
typedef struct {
  VMBusChannelMessageHeader header;
  UInt32                    version;
  UInt32                    reserved;
  UInt64                    eventFlags;
  UInt64                    mnf1;
  UInt64                    mnf2;
} VMBusChannelMessageConnect;

//
// VMBUS_CHANMSG_TYPE_CONNECT_RESP
//
typedef struct {
  VMBusChannelMessageHeader header;
  UInt8                     done;
} VMBusChannelMessageConnectResponse;*/

#endif
