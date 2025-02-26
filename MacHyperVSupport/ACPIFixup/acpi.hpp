//
//  acpi.hpp
//  ACPI definitions from acpica
//
//  Copyright © 1999-2008, Intel Corp.
//

#ifndef acpi_hpp
#define acpi_hpp

#include <IOKit/IOLib.h>

/*
 * Miscellaneous types
 */
typedef UInt32                          ACPI_STATUS;    /* All ACPI Exceptions */
typedef UInt32                          ACPI_NAME;      /* 4-byte ACPI name */
typedef char *                          ACPI_STRING;    /* Null terminated ASCII string */
typedef void *                          ACPI_HANDLE;    /* Actually a ptr to a NS Node */

#define ACPI_SUCCESS(a)                 (!(a))
#define ACPI_FAILURE(a)                 (a)

#define AE_OK                           (ACPI_STATUS) 0x0000

/* Length of _HID, _UID, _CID, and UUID values */
#define ACPI_DEVICE_ID_LENGTH           0x09
#define ACPI_MAX_CID_LENGTH             48
#define ACPI_UUID_LENGTH                16

/*
 * Types associated with ACPI names and objects.  The first group of
 * values (up to ACPI_TYPE_EXTERNAL_MAX) correspond to the definition
 * of the ACPI ObjectType() operator (See the ACPI Spec).  Therefore,
 * only add to the first group if the spec changes.
 *
 * NOTE: Types must be kept in sync with the global AcpiNsProperties
 * and AcpiNsTypeNames arrays.
 */
typedef UInt32                          ACPI_OBJECT_TYPE;

#define ACPI_TYPE_ANY                   0x00
#define ACPI_TYPE_INTEGER               0x01  /* Byte/Word/Dword/Zero/One/Ones */
#define ACPI_TYPE_STRING                0x02
#define ACPI_TYPE_BUFFER                0x03
#define ACPI_TYPE_PACKAGE               0x04  /* ByteConst, multiple DataTerm/Constant/SuperName */
#define ACPI_TYPE_FIELD_UNIT            0x05
#define ACPI_TYPE_DEVICE                0x06  /* Name, multiple Node */
#define ACPI_TYPE_EVENT                 0x07
#define ACPI_TYPE_METHOD                0x08  /* Name, ByteConst, multiple Code */
#define ACPI_TYPE_MUTEX                 0x09
#define ACPI_TYPE_REGION                0x0A
#define ACPI_TYPE_POWER                 0x0B  /* Name,ByteConst,WordConst,multi Node */
#define ACPI_TYPE_PROCESSOR             0x0C  /* Name,ByteConst,DWordConst,ByteConst,multi NmO */
#define ACPI_TYPE_THERMAL               0x0D  /* Name, multiple Node */
#define ACPI_TYPE_BUFFER_FIELD          0x0E
#define ACPI_TYPE_DDB_HANDLE            0x0F
#define ACPI_TYPE_DEBUG_OBJECT          0x10

#define ACPI_TYPE_EXTERNAL_MAX          0x10

/* Owner IDs are used to track namespace nodes for selective deletion */
typedef UInt8                           ACPI_OWNER_ID;
#define ACPI_OWNER_ID_MAX               0xFF

typedef union acpi_name_union {
  UInt32 Integer;
  char   Ascii[4];
} ACPI_NAME_UNION;

/*
 * The Namespace Node describes a named object that appears in the AML.
 * DescriptorType is used to differentiate between internal descriptors.
 *
 * The node is optimized for both 32-bit and 64-bit platforms:
 * 20 bytes for the 32-bit case, 32 bytes for the 64-bit case.
 *
 * Note: The DescriptorType and Type fields must appear in the identical
 * position in both the ACPI_NAMESPACE_NODE and ACPI_OPERAND_OBJECT
 * structures.
 */
typedef struct acpi_namespace_node {
  union acpi_operand_object   *Object;        /* Interpreter object */
  UInt8                       DescriptorType; /* Differentiate object descriptor types */
  UInt8                       Type;           /* ACPI Type associated with this name */
  UInt8                       Flags;          /* Miscellaneous flags */
  ACPI_OWNER_ID               OwnerId;        /* Node creator */
  ACPI_NAME_UNION             Name;           /* ACPI Name, always 4 chars per ACPI spec */
  struct acpi_namespace_node  *Child;         /* First child */
  struct acpi_namespace_node  *Peer;          /* Peer. Parent if ANOBJ_END_OF_PEER_LIST set */
} ACPI_NAMESPACE_NODE;

typedef ACPI_STATUS (*ACPI_WALK_CALLBACK) (ACPI_HANDLE ObjHandle, UInt32 NestingLevel, void *Context, void **ReturnValue);

/* Common string version of device HIDs and UIDs */
typedef struct acpi_device_id {
  char Value[ACPI_DEVICE_ID_LENGTH];
} ACPI_DEVICE_ID;

//
// Exported acpica functions from AppleACPIPlatform.
//
extern "C" ACPI_STATUS AcpiWalkNamespace(ACPI_OBJECT_TYPE Type, ACPI_HANDLE StartObject, UInt32 MaxDepth,
                                         ACPI_WALK_CALLBACK UserFunction, void *Context, void **ReturnValue);
extern "C" ACPI_STATUS AcpiUtExecute_HID(ACPI_NAMESPACE_NODE *DeviceNode, ACPI_DEVICE_ID *Hid);

#endif
