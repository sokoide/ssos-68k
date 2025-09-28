#include <stddef.h>
#include <string.h>

// 簡易的なstrtokの実装
static char* strtok_ptr = NULL;

char* strtok(char* str, const char* delim) {
    if (str != NULL) {
        strtok_ptr = str;
    } else if (strtok_ptr == NULL) {
        return NULL;
    }

    // デリミタをスキップ
    while (*strtok_ptr && strchr(delim, *strtok_ptr)) {
        strtok_ptr++;
    }

    if (*strtok_ptr == '\0') {
        strtok_ptr = NULL;
        return NULL;
    }

    char* token = strtok_ptr;

    // 次のデリミタを探す
    while (*strtok_ptr && !strchr(delim, *strtok_ptr)) {
        strtok_ptr++;
    }

    if (*strtok_ptr) {
        *strtok_ptr = '\0';
        strtok_ptr++;
    } else {
        strtok_ptr = NULL;
    }

    return token;
}