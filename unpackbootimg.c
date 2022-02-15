#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mincrypt/sha.h"
#include "bootimg.h"

typedef unsigned char byte;

int a = 0, b = 0, c = 0, y = 0, m = 0; // header.os_version component calculation variables

int read_padding(FILE* f, unsigned itemsize, int pagesize)
{
    byte* buf = (byte*)malloc(sizeof(byte) * pagesize);
    unsigned pagemask = pagesize - 1;
    unsigned count;

    if((itemsize & pagemask) == 0) {
        free(buf);
        return 0;
    }

    count = pagesize - (itemsize & pagemask);

    if(fread(buf, count, 1, f)){};
    free(buf);
    return count;
}

void write_string_to_file(char* file, char* string)
{
    FILE* f = fopen(file, "w");
    fwrite(string, strlen(string), 1, f);
    fwrite("\n", 1, 1, f);
    fclose(f);
}

int print_os_version(uint32_t hdr_os_ver)
{
    if(hdr_os_ver != 0) {
        int os_version = 0, os_patch_level = 0;
        os_version = hdr_os_ver >> 11;
        os_patch_level = hdr_os_ver&0x7ff;

        a = (os_version >> 14)&0x7f;
        b = (os_version >> 7)&0x7f;
        c = os_version&0x7f;

        y = (os_patch_level >> 4) + 2000;
        m = os_patch_level&0xf;
    }
    if((a < 128) && (b < 128) && (c < 128) && (y >= 2000) && (y < 2128) && (m > 0) && (m <= 12)) {
        printf("BOARD_OS_VERSION %d.%d.%d\n", a, b, c);
        printf("BOARD_OS_PATCH_LEVEL %d-%02d\n", y, m);
        return 0;
    }
    return 1;
}

int usage() {
    printf("usage: unpackbootimg\n");
    printf("\t-i|--input boot.img\n");
    printf("\t[ -o|--output output_directory]\n");
    printf("\t[ -p|--pagesize <size-in-hexadecimal> ]\n");
    return 0;
}

int main(int argc, char** argv)
{
    char tmp[PATH_MAX];
    char* directory = "./";
    char* filename = NULL;
    int pagesize = 0;
    int base = 0;

    argc--;
    argv++;
    while(argc > 0){
        char *arg = argv[0];
        char *val = argv[1];
        argc -= 2;
        argv += 2;
        if(!strcmp(arg, "--input") || !strcmp(arg, "-i")) {
            filename = val;
        } else if(!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            directory = val;
        } else if(!strcmp(arg, "--pagesize") || !strcmp(arg, "-p")) {
            pagesize = strtoul(val, 0, 16);
        } else {
            return usage();
        }
    }
    
    if (filename == NULL) {
        return usage();
    }
    
    struct stat st;
    if (stat(directory, &st) == (-1)) {
        printf("Could not stat %s: %s\n", directory, strerror(errno));
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        printf("%s is not a directory\n", directory);
        return 1;
    }
    
    int total_read = 0;
    FILE* f = fopen(filename, "rb");
    boot_img_hdr header;
    
    if (!f) {
        printf("Could not open input file: %s\n", strerror(errno));
        return (1);
    }
    
    //printf("Reading header...\n");
    int i;
    int seeklimit = 4096;
    for (i = 0; i <= seeklimit; i++) {
        fseek(f, i, SEEK_SET);
        if(fread(tmp, BOOT_MAGIC_SIZE, 1, f)){};
        if (memcmp(tmp, BOOT_MAGIC, BOOT_MAGIC_SIZE) == 0)
            break;
    }
    total_read = i;
    if (i > seeklimit) {
        printf("Android boot magic not found.\n");
        return 1;
    }
    fseek(f, i, SEEK_SET);
    if (i > 0) {
        printf("Android magic found at: %d\n", i);
    }

    if(fread(&header, sizeof(header), 1, f)){};
    base = header.kernel_addr - 0x00008000;
    printf("BOARD_KERNEL_CMDLINE %s\n", header.cmdline);
    printf("BOARD_KERNEL_BASE 0x%08x\n", base);
    printf("BOARD_NAME %s\n", header.name);
    printf("BOARD_PAGE_SIZE %d\n", header.page_size);
    printf("BOARD_KERNEL_OFFSET 0x%08x\n", header.kernel_addr - base);
    printf("BOARD_RAMDISK_OFFSET 0x%08x\n", header.ramdisk_addr - base);
    printf("BOARD_SECOND_OFFSET 0x%08x\n", header.second_addr - base);
    printf("BOARD_TAGS_OFFSET 0x%08x\n",header.tags_addr - base);
    if(print_os_version(header.os_version) == 1) {
        header.os_version = 0;
    }
    if (header.dt_size != 0) {
        printf("BOARD_DT_SIZE %d\n", header.dt_size);
    }
    printf("BOARD_UNKNOWN 0x%08x\n", header.unknown);
    
    if (pagesize == 0) {
        pagesize = header.page_size;
    }
    
    //printf("cmdline...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-cmdline");
    write_string_to_file(tmp, (char*)header.cmdline);
    
    //printf("board...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-board");
    write_string_to_file(tmp, (char*)header.name);
    
    //printf("base...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-base");
    char basetmp[200];
    sprintf(basetmp, "0x%08x", base);
    write_string_to_file(tmp, basetmp);

    //printf("pagesize...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-pagesize");
    char pagesizetmp[200];
    sprintf(pagesizetmp, "%d", header.page_size);
    write_string_to_file(tmp, pagesizetmp);

    //printf("kernel_offset...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-kernel_offset");
    char kerneltmp[200];
    sprintf(kerneltmp, "0x%08x", header.kernel_addr - base);
    write_string_to_file(tmp, kerneltmp);

    //printf("ramdisk_offset...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-ramdisk_offset");
    char ramdisktmp[200];
    sprintf(ramdisktmp, "0x%08x", header.ramdisk_addr - base);
    write_string_to_file(tmp, ramdisktmp);

    //printf("second_offset...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-second_offset");
    char secondtmp[200];
    sprintf(secondtmp, "0x%08x", header.second_addr - base);
    write_string_to_file(tmp, secondtmp);

    //printf("tags_offset...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-tags_offset");
    char tagstmp[200];
    sprintf(tagstmp, "0x%08x", header.tags_addr - base);
    write_string_to_file(tmp, tagstmp);

    if(header.os_version != 0) {
        //printf("os_version...\n");
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-os_version");
        char os_versiontmp[200];
        sprintf(os_versiontmp, "%d.%d.%d", a, b, c);
        write_string_to_file(tmp, os_versiontmp);

        //printf("os_patch_level...\n");
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-os_patch_level");
        char os_patch_leveltmp[200];
        sprintf(os_patch_leveltmp, "%d", y, m);
        write_string_to_file(tmp, os_patch_leveltmp);
    }

    //printf("unknown...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-unknown");
    char unknownstmp[200];
    sprintf(unknownstmp, "0x%08x", header.unknown);
    write_string_to_file(tmp, unknownstmp);

    total_read += sizeof(header);
    //printf("total read: %d\n", total_read);
    total_read += read_padding(f, sizeof(header), pagesize);

    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-kernel");
    FILE *k = fopen(tmp, "wb");
    byte* kernel = (byte*)malloc(header.kernel_size);
    //printf("Reading kernel...\n");
    if(fread(kernel, header.kernel_size, 1, f)){};
    total_read += header.kernel_size;
    fwrite(kernel, header.kernel_size, 1, k);
    fclose(k);

    //printf("total read: %d\n", header.kernel_size);
    total_read += read_padding(f, header.kernel_size, pagesize);

    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-ramdisk");
    FILE *r = fopen(tmp, "wb");
    byte* ramdisk = (byte*)malloc(header.ramdisk_size);
    //printf("Reading ramdisk...\n");
    if(fread(ramdisk, header.ramdisk_size, 1, f)){};
    total_read += header.ramdisk_size;
    fwrite(ramdisk, header.ramdisk_size, 1, r);
    fclose(r);

    //printf("total read: %d\n", header.ramdisk_size);
    total_read += read_padding(f, header.ramdisk_size, pagesize);

    if (header.second_size != 0) {
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-second");
        FILE *s = fopen(tmp, "wb");
        byte* second = (byte*)malloc(header.second_size);
        //printf("Reading second...\n");
        if(fread(second, header.second_size, 1, f)){};
        total_read += header.second_size;
        fwrite(second, header.second_size, 1, r);
        fclose(s);
    }

    //printf("total read: %d\n", header.second_size);
    total_read += read_padding(f, header.second_size, pagesize);

    if (header.dt_size != 0) {
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-dt");
        FILE *d = fopen(tmp, "wb");
        byte* dt = (byte*)malloc(header.dt_size);
        //printf("Reading dt...\n");
        if(fread(dt, header.dt_size, 1, f)){};
        total_read += header.dt_size;
        fwrite(dt, header.dt_size, 1, r);
        fclose(d);
    }

    //printf("total read: %d\n", header.dt_size);
    total_read += read_padding(f, header.dt_size, pagesize);

    stat(filename, &st);
    if (st.st_size >= total_read + 256) {
        int sig_size = 0;
        if(fread(tmp, 16, 1, f)){};
        if (memcmp(tmp, "SEANDROIDENFORCE", 16) == 0) {
            sig_size = 272;
        } else {
            sig_size = 256;
        }
        fseek(f, total_read, SEEK_SET);
        sprintf(tmp, "%s/%s", directory, basename(filename));
        strcat(tmp, "-signature");
        FILE *fsig = fopen(tmp, "wb");
        byte* bsig = (byte*)malloc(sig_size);
        //printf("Reading signature...\n");
        if(fread(bsig, sig_size, 1, f)){};
        total_read += sig_size;
        fwrite(bsig, sig_size, 1, r);
        fclose(fsig);
    }

    fclose(f);

    //printf("Total Read: %d\n", total_read);
    return 0;
}
