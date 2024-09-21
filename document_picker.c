#include "document_picker.h"
#include "simplehash.h"
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>


int write_wordList(const char* path)
{
    //1. usr/bin 으로부터 명령어 목록을 받아옴 (일반적인 파일만)
    int cmdCount;
    char** cmdList = get_cmdList(&cmdCount);


    //2. 명령어를 무작위로 선택한 다음, 매뉴얼 문서에서 단어를 추출해서 path에 저장
    //(부족한 경우 다른 명령어도 선택하기)
    int wordCount = 0;
    hashInit();     //단어 중복 방지용 해시 테이블
    int failStreak = 0, result = 0;

    int fd;
    SYSCALL_CHECK(
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666), -1, "open");
    srand(time(NULL)); //Random seed
    
    
    while (wordCount < WORDCOUNT_MAX)
    {
        const char* cmd = cmdList[rand() % cmdCount];
        // man 호출에 10번 연속 실패하면 프로그램 종료
        if ((result = extract_words(cmd, fd, &wordCount)) != 0
            && ++failStreak == 10)
        {
            fprintf(stderr, "Failed to extract words from manual\n");
            exit(1);
        } else {
            failStreak = 0;
        }
    }
    
    for (int i = 0; i < cmdCount; i++)
        free(cmdList[i]);
    free(cmdList);
    hashFree();
    close(fd);

    return 0;
}




char** get_cmdList(int* cmdCountp)
{
    DIR* commandDir;
    struct dirent *fileEntry;
    struct stat fileStat;
    int commandCount = 0, commandMax = 200;
    char** commandList;
    
    SYSCALL_CHECK(
    commandList = (char**)malloc(sizeof(char*) * commandMax), NULL, "malloc");

    //1. 디렉토리 열기
    SYSCALL_CHECK(commandDir = opendir("/usr/bin"), NULL, "opendir");
    
    //2. 파일 엔트리에서 일반 파일만 명령어 목록(commandList)에 추가
    while ((fileEntry = readdir(commandDir)) != NULL)
    {
        char path[1024];
        sprintf(path, "/usr/bin/%s", fileEntry->d_name);
        SYSCALL_CHECK(stat(path, &fileStat), -1, "stat");

        if (S_ISREG(fileStat.st_mode))
        {
            int len = strlen(fileEntry->d_name);
            SYSCALL_CHECK(
                commandList[commandCount] = (char*)malloc(sizeof(char) * (len + 1)), NULL, "malloc");
            strncpy(commandList[commandCount++], fileEntry->d_name, len + 1);
            if (commandCount == commandMax)
            {
                commandMax *= 2;
                SYSCALL_CHECK(
                    commandList = (char**)
                        realloc(commandList, sizeof(char*) * commandMax), NULL, "realloc");
            }
        }
        
    }
    closedir(commandDir);

    *cmdCountp = commandCount;
    return commandList;
}



int extract_words(const char* cmd, int fdout, int* wordCountp)
{
    int pid;
    SYSCALL_CHECK(pid = fork(), -1, "fork");

    if (pid == 0)
    {
        //child process : Load manual to file
        //메뉴얼 내용은 manual.txt에 저장된다.
        int fd, newfd;

        SYSCALL_CHECK(
            fd = open("./manual.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666), -1, "open");
        SYSCALL_CHECK(
            newfd = dup2(fd, 1), -1, "dup2"); //stdout to file
        if (newfd != 1)
        {
            fprintf(stderr, "dup2 error\n");
            exit(1);
        }
        close(2);  //stderr to null

        execlp("man", "man", cmd, NULL);
        perror("execlp");
        exit(1);
    }

    //parent process 
    
    //1. child 정상 수행 여부 검사
    int childStatus;
    wait(&childStatus);
    if ((childStatus >> 8) != 0)
    //child process가 0 이외의 값으로 종료되었다면
    //시스템 콜이 실패했거나, man 명령이 본래 기능을 수행하지 못하는 상황이었음
        return childStatus >> 8;
    

    //2. manual.txt에서 단어 추출
    int fdin; 
    SYSCALL_CHECK(
        fdin = open("./manual.txt", O_RDONLY), -1, "open2");
    
    
    char buf[BUFSIZ];
    char plainWord[WORDLEN_MAX + 1] = {'\0'};
    while (read(fdin, buf, BUFSIZ) > 0)
    {
        char* word = strtok(buf, " /-\n\t");  //tokenize
        while (word != NULL)
        {
            //단어에서 알파벳(소문자로), 숫자만 추출
            int plainLen = 0;
            for (int i = 0; plainLen < WORDLEN_MAX && word[i] != '\0'; i++)
            {
                if (isdigit(word[i]))
                    plainWord[plainLen++] = word[i];
                else if (isalpha(word[i]))
                    plainWord[plainLen++] = tolower(word[i]);
            }
            plainWord[plainLen] = '\0';
            
            if (WORDLEN_MIN <= plainLen && hashInsert(plainWord, plainLen) == true)
            //길이가 적당하고, 중복되지 않은 단어를 테이블에 추가
            {
                SYSCALL_CHECK(write(fdout, plainWord, plainLen), -1, "write");
                SYSCALL_CHECK(write(fdout, "\n", 1), -1, "write");

                if (++(*wordCountp) == WORDCOUNT_MAX)
                {
                    close(fdin);
                    return 0;   //단어 목록이 다 찼으면 종료
                }
            }
            word = strtok(NULL, " /-\n\t");
        }
    }

    close(fdin);
    return 0; //메뉴얼 문서를 모두 읽고 단어 목록은 다 안찬 경우
}