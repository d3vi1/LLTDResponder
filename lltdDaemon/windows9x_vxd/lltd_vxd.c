/*
 * Minimal Win9x VxD stub used for CI packaging.
 * This does not implement LLTD functionality yet.
 */

/* Device Descriptor Block - required for VxD */
typedef struct {
    unsigned long next_ddb;
    unsigned short sdk_version;
    unsigned short device_number;
    unsigned char major_version;
    unsigned char minor_version;
    unsigned short flags;
    char name[8];
    unsigned long init_order;
    unsigned long control_proc;
    unsigned long v86_api_proc;
    unsigned long pm_api_proc;
    unsigned long v86_api_csip;
    unsigned long pm_api_csip;
    unsigned long ref_data;
    unsigned long service_table;
    unsigned long service_table_size;
    unsigned long win32_service_table;
} DDB;

unsigned long __stdcall Device_Control(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);

/* DDB must be in _LDATA segment for VxD */
#pragma data_seg("_LDATA", "DATA")

DDB LLTD_DDB = {
    0,              /* next_ddb */
    0x30A,          /* sdk_version (3.10) */
    0xFFFF,         /* undefined device number */
    1,              /* major_version */
    0,              /* minor_version */
    0,              /* flags */
    "LLTD",         /* name */
    0x80000000,     /* init_order */
    (unsigned long)Device_Control,
    0, 0, 0, 0, 0, 0, 0, 0
};

#pragma data_seg()

unsigned long __stdcall Device_Control(unsigned long dwService, unsigned long dwDDB,
                                       unsigned long hVM, unsigned long lpRegs, unsigned long dwParam) {
    /* Minimal control handler - just succeed */
    (void)dwService; (void)dwDDB; (void)hVM; (void)lpRegs; (void)dwParam;
    return 0;
}
