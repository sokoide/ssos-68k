#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

extern void ssosmain();
extern void set_interrupts();
extern void restore_interrupts();

char local_info[256];
void* local_app_memory_base;
uint32_t local_app_memory_size = 8 * 1024 * 1024;

int main(int argc, char** argv) {
    uint32_t text_size, data_size, bss_size;

    int ssp = _iocs_b_super(0); // enter supervisor mode

    set_interrupts();

    // get local info
    strcpy(local_info, argv[0]);

    FILE* fp = fopen(argv[0], "rb");
    fseek(fp, 0xc, SEEK_SET);
    fread(&text_size, 4, 1, fp);
    fread(&data_size, 4, 1, fp);
    fread(&bss_size, 4, 1, fp);
    fclose(fp);
    sprintf(local_info, "text size:%d, data size:%d, bss size:%d", text_size,
            data_size, bss_size);

    local_app_memory_base = malloc(local_app_memory_size);
    assert(local_app_memory_base);

    ssosmain();

    restore_interrupts();

    _iocs_b_super(ssp); // leave supervisor mode
    return 0;
}
