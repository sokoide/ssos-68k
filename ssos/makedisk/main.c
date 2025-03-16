#include <stdio.h>
#include <stdint.h>

void usage () {
    printf("Usage: makedisk {boot-binary-path} {os-binary-path} {target-disk-path}\n");
}

int main(int argc, char** argv) {
    int ret = 0;
    FILE* f_boot;
    FILE* f_os;
    FILE* f_image;
    size_t sz;
    uint8_t buff[1024];
    uint8_t zero[1024] = {0};
    const uint32_t IMAGE_SIZE = 1261568;

    if (argc != 4) {
        usage();
        ret = 1;
        goto END;
    }

    f_boot = fopen(argv[1], "rb");
    if (f_boot == NULL) {
        fprintf(stderr, "Error: boot file not found\n");
        ret = 1;
        goto END;
    }

    f_os = fopen(argv[2], "rb");
    if (f_os == NULL) {
        fprintf(stderr, "Error: os file not found\n");
        ret = 1;
        goto END;
    }

    f_image = fopen(argv[3], "wb");
    if (f_image == NULL) {
        fprintf(stderr, "Error: failed to create the os image file\n");
        ret = 1;
        goto END;
    }

    // get file size
    fseek(f_boot, 0, SEEK_END);
    long sz_boot = ftell(f_boot);
    fseek(f_boot, 0, SEEK_SET);

    fseek(f_os, 0, SEEK_END);
    long sz_os = ftell(f_os);
    fseek(f_os, 0, SEEK_SET);

    printf("boot size: %ld\n", sz_boot);
    printf("os size: %ld\n", sz_os);

    sz = fread(buff, 1, 1024, f_boot);
    fwrite(buff, 1, sz, f_image);
    fwrite(zero, 1, 1024-sz, f_image);

    uint32_t written = 1024;
    while(1) {
        sz = fread(buff, 1, 1024, f_os);
        fwrite(buff, 1, sz, f_image);
        written += sz;

        if (sz < 1024) {
            break;
        }
    }

    printf("Boot (1024) + OS size: %d\n", written);

    while(written <= IMAGE_SIZE - 1024) {
        fwrite(zero, 1, 1024, f_image);
        written += 1024;
    }
    fwrite(zero, 1, IMAGE_SIZE - written, f_image);
    written += IMAGE_SIZE - written;

    printf("Total size: %d\n", written);

END:
    if (f_boot != NULL) {
        fclose(f_boot);
    }
    if (f_os != NULL) {
        fclose(f_os);
    }
    if (f_image != NULL) {
        fclose(f_image);
    }
    return ret;
}
