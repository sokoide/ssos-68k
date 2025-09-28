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
void* local_ssos_memory_base;

uint32_t local_ssos_memory_size = 10 * 1024 * 1024;
uint32_t local_text_size;
uint32_t local_data_size;
uint32_t local_bss_size;

int main(int argc, char** argv) {
    int ssp = _iocs_b_super(0);  // enter supervisor mode

    set_interrupts();

    // get local info
    strcpy(local_info, argv[0]);

    FILE* fp = fopen(argv[0], "rb");
    fseek(fp, 0xc, SEEK_SET);
    fread(&local_text_size, 4, 1, fp);
    fread(&local_data_size, 4, 1, fp);
    fread(&local_bss_size, 4, 1, fp);
    fclose(fp);
    sprintf(local_info, "text size: %9d\ndata size: %9d\nbss size:  %9d",
            local_text_size, local_data_size, local_bss_size);

    local_ssos_memory_base = malloc(local_ssos_memory_size);
    assert(local_ssos_memory_base);

    ssosmain();

    if (NULL != local_ssos_memory_base)
        free(local_ssos_memory_base);

    restore_interrupts();

    _iocs_b_super(ssp);  // leave supervisor mode
    return 0;
}
