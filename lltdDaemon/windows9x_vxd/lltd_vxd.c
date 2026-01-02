/*
 * Minimal Win9x VxD stub used for CI packaging.
 * This does not implement LLTD functionality yet.
 */

unsigned short Device_Init(void) {
    return 1;
}

void Device_Exit(void) {
}
