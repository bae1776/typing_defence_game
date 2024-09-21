// Creator: Jinsung
// 문자열을 빠르게 검색하기 위해 해싱하여 저장하는 프로그램
// 1. 단순하게 문자열 맨 앞, 맨 뒤의 문자로 해싱합니다. (충돌되는 경우 체이닝)
// 2. 키에 상응하는 값은 없습니다.
// 3. 테이블은 오직 1개이며, 문자열을 깊은 복사(동적할당)하기 때문에 사용 후 해제해야 누수가 없습니다.

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TABLE_SIZE 36

typedef struct TableElement {
    char* key;
    struct TableElement* next;
} TableElement;

TableElement hashTable[TABLE_SIZE][TABLE_SIZE];

int hashIndex(char ch)
{
    if (ch >= 'a' && ch <= 'z') return ch - 'a';
    else if (ch >= '0' && ch <= '9') return ch - '0' + 26;
    else return -1;
}

void hashInit() {
    for (int i = 0; i < TABLE_SIZE; i++) {
        for (int j = 0; j < TABLE_SIZE; j++) {
            hashTable[i][j].key = NULL;
            hashTable[i][j].next = NULL;
        }
    }
}


bool hashInsert(char* key, int len)
{
    TableElement* selected = &hashTable[hashIndex(key[0])][hashIndex(key[len-1])];

    if (selected->key == NULL) {
        selected->key = (char*)malloc(sizeof(char) * (len + 1));
        strcpy(selected->key, key);
        return true;
    }
    else {
        while (selected->next != NULL) {
            if (strcmp(selected->key, key) == 0) 
                return false;
            selected = selected->next;
        }
        if (strcmp(selected->key, key) == 0) 
            return false;
        else {
            selected->next = (TableElement*)malloc(sizeof(TableElement));
            selected = selected->next;
            selected->key = (char*)malloc(sizeof(char) * (len + 1));
            strcpy(selected->key, key);
            selected->next = NULL;
            return true;
        }
    }
}

bool hashSearch(char* key, int len)
{
    TableElement* selected = &hashTable[hashIndex(key[0])][hashIndex(key[len-1])];

    if (selected->key == NULL) 
        return false;
    else {
        while (selected != NULL) {
            if (strcmp(selected->key, key) == 0) 
                return true;
            selected = selected->next;
        }
        return false;
    }
}


void hashFree()
{
    TableElement* selected, *next;

    for (int i = 0; i < TABLE_SIZE; i++) {
        for (int j = 0; j < TABLE_SIZE; j++) {
            selected = &hashTable[i][j];
            
            if (selected->key != NULL) {
                free(selected->key);
                selected->key = NULL;
                while (selected->next != NULL) {
                    selected = selected->next;
                    next = selected->next;
                    free(selected->key);
                    free(selected);
                    selected = next;
                }
            }
        }
    }
}
