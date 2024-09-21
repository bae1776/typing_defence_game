/*   Typing Defence Game - main
 *   이 게임은 쏟아지는 단어들을 꽉 차서 터지기 전에 타이핑하여 없애는 게임입니다.
 *   단어는 무작위로 선택되며, 시간이 갈 수록 빠르게 쳐야 합니다.
 *   최대한 많은 점수를 얻어보세요!
 * 
 */

#include "document_picker.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <curses.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <sys/time.h>

//WordList
char buf[WORDCOUNT_MAX * (WORDLEN_MAX + 1) + 2];
char* wordEntry[WORDCOUNT_MAX + 1];
const char* wordPath = "./wordList.txt";

//Game constants
#define GAMETEXT_ROW 1
#define GAMETEXT "\
*** Typing Defence Game! ***\n\
\n\
type quickly before words fill up the below box.\n\
Rule : \n\
1. Filling speed grows as time goes by.\n\
2. If you type correctly, the letter will be erased and get 1 score.\n\
3. If you type incorrectly, one letter penalty and lose 1 score.\n\
4. If the box is full, you lose.\n\
(only alphabet + number, case-insensitive)"
#define KEYINFO_ROW 11
#define KEYINFO "\
Press SPACEBAR key to (re)start the game. \n\
Press @(Shift+2) key to quit the game."
#define KEYINFO_CLEAR "\
                                           \n\
                                           "
#define BOX_ROW 13
#define BOX "\
*------------------------------------------------------------*\n\
|                                                            |\n\
*------------------------------------------------------------*"
#define BOX_SIZE 60
#define BOXTEXT_ROW (BOX_ROW+1)
#define BOXTEXT_COL 1
#define CURSOR_ROW (BOX_ROW+2)
#define CURSOR_COL 1
#define SCORE_ROW (BOX_ROW+3)
#define MAX_SCORE_ROW (BOX_ROW+4)
#define SPEED_ROW (BOX_ROW+5)
#define TIMER_INTERVAL_MS 686.0
#define SPEEDUP_CHARCNT 10.0
#define SPEEDUP_FACTOR 0.875

//Game functions
void game_reset();      //게임 초기화
int game_init();        //게임 시작 전 메인 메뉴 (+ lcurse init)
void game_progress();   //게임 진행 창
int game_over();        //게임 오버 창

bool addQueue();        //문자를 큐에 추가 (+ 박스스가 꽉 차면 false 반환)
void refreshBox();      //박스 화면 갱신하기
int set_ticker(int);    //타이머 설정
void addBox(int);       //박스에 문자 추가하기 (타이머 핸들러)

//Game variables
char circularQueue[BOX_SIZE + 1];
int front = 0, rear = 0, qSize = 0;       //원형 큐
char* charPtr = NULL;                     //다음 생성될 문자 위치
double speed = TIMER_INTERVAL_MS;         //생성 간격 (ms)
double speedUpInterval = SPEEDUP_CHARCNT; //속도 증가 간격 (문자 개수 단위)
int speedUpRemain;                        //다음 속도 증가까지 남은 문자 개수
int score = 0;              //점수
int maxScore = 0;           //최고 점수

int main(void)
{
    initscr();
    crmode();
    noecho();
    clear();

    //1. 게임 초기화 
    game_reset();

    //2. 게임 시작 전 메인 메뉴
    if (game_init() == 1)
    {
        endwin();
        return 0;
    }
    
    
    //3. 게임 진행 창 <-> 게임 오버 창 -> 끝내기
    while (1)
    {
        game_progress();
        if (game_over() == 1)
            break;
        else {
            game_reset(); //초기화하고 다시 시작
        }
    }

    endwin();
    return 0;
}



void game_reset()       //게임 초기화
{
    int pid;
    SYSCALL_CHECK(pid = fork(), -1, "fork");
    //child process : 새로운 단어 목록 만들기 (백그라운드로 미리 실행)*********
    if (pid == 0) {
        exit(write_wordList(wordPath));
    }
    //***************************************************************

    //parent process
    //1. 게임 값 초기화
    front = rear = score = qSize = 0;
    speed = TIMER_INTERVAL_MS;
    speedUpInterval = SPEEDUP_CHARCNT;
    speedUpRemain = (int)speedUpInterval;
    memset(circularQueue, ' ', sizeof(circularQueue) - 1);
    memset(buf, '\0', sizeof(buf));
    memset(wordEntry, '\0', sizeof(wordEntry));
    charPtr = NULL;
    srand(time(NULL));
    move(BOX_ROW, 0);
    addstr(BOX);

    //커서, 점수 초기화
    move(CURSOR_ROW, CURSOR_COL);
    addch('^');

    move(SCORE_ROW, 0);
    printw("Score     : %-10d", score);

    move(LINES-1, COLS-1);
    refresh();

    int childStatus;
    wait(&childStatus);
    if ((childStatus >> 8) != 0) {
        perror("child(document picker) process error");
        exit(1);
    }

    //2. 단어 목록 가져와서 wordEntry에 보관하기.
    int fd;
    SYSCALL_CHECK(
        fd = open(wordPath, O_RDONLY), -1, "open");
    
    switch (read(fd, buf, sizeof(buf)))
    {
        case -1:
            perror("read");
            exit(1);
        case sizeof(buf) / sizeof(buf[0]):
            fprintf(stderr, "wordList.txt is too big\n");
            exit(1);
    };

    int idx = 0, entryIndex = 0;
    for (int i = 0; buf[i] != '\0'; ++i)
    {
        if (buf[i] == '\n')
        {
            buf[i] = '\0';
            wordEntry[entryIndex++] = &buf[idx];
            idx = i + 1;
        }
    }
    //wordEntry 각 요소에는 단어의 첫 문자를 가리키는 포인터가 들어있다.

    return;
}


int game_init()         //게임 시작 전 메인 메뉴 
{
    signal(SIGQUIT, SIG_IGN);   //ctrl+\ 무시

    //게임 설명
    move(GAMETEXT_ROW, 0);
    addstr(GAMETEXT);

    //키 설명
    move(KEYINFO_ROW, 0);
    addstr(KEYINFO);

    //박스
    move(BOX_ROW, 0);
    addstr(BOX);

    //커서
    move(CURSOR_ROW, CURSOR_COL);
    addch('^');

    //점수
    move(SCORE_ROW, 0);
    printw("Score     : %-10d", score);

    //최고 점수
    move(MAX_SCORE_ROW, 0);
    printw("Max Score : %-10d", maxScore);

    //속도
    move(SPEED_ROW, 0);
    printw("Speed     : %-5.2f word/s   ", 1000.0 / speed);

    //커서 구석으로
    move(LINES-1, COLS-1);
    refresh();
    
    while (1) {
        int c = getch();
        addch(c);
        if (c == ' ') {
            //게임 진행
            return 0;
        }
        else if (c == '@') {
            //게임 종료
            return 1;
        }
    }

    return 0;
}



void game_progress()    //게임 진행 창
{
    //1. 단어 10개 채우고 시작하기
    for (int i = 0; i < 10; ++i)
        addQueue();
    refreshBox();


    move(KEYINFO_ROW, 0); //키 설명 지우기
    addstr(KEYINFO_CLEAR);

    //2. 게임 진행
    signal(SIGALRM, addBox);    //자동 문자 생성 시그널
    timeout(5);                 //getch Non-blocking 설정
    set_ticker((int)speed);     //타이머 시작

    int ch;

    while (qSize < BOX_SIZE) 
    //게임 유지 조건
    {
        ch = getch();

        if (ch != ERR)  //문자 입력이 있는 경우
        {
            //move(21, 0);
            //printw("ch : %c, qSize = %d", ch, qSize);

            if (isalpha(ch))
                ch = tolower(ch);

            if (ch == circularQueue[front] && qSize != 0) //맞는 입력
            {
                score++;
                circularQueue[front] = ' ';
                move(CURSOR_ROW, CURSOR_COL + front);
                addch('-');
                front = (front + 1) % BOX_SIZE;
                move(CURSOR_ROW, CURSOR_COL + front);
                addch('^'); //커서 이동
                qSize--;
                
            } else {                        //틀린 입력
                score--;
                addQueue();                 //(1개 패널티)
            }

            //박스 갱신
            refreshBox();

            //점수 갱신
            move(SCORE_ROW, 0);
            printw("Score     : %-10d", score);
            move(LINES-1, COLS-1);
        }
    }
    

    //Game Over 확정 후 처리
    
    signal(SIGALRM, SIG_DFL);   //자동 문자 생성 시그널 해제
    struct itimerval new_timeset;  //타이머 삭제
    new_timeset.it_interval.tv_sec = 0;
    new_timeset.it_interval.tv_usec = 0;
    new_timeset.it_value.tv_sec = 0;
    new_timeset.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_timeset, NULL);
    timeout(-1);                //getch Blocking 설정


    return;
}


int game_over()        //게임 오버 창
{
    //키 설명 창 다시 표시
    move(KEYINFO_ROW, 0);
    addstr(KEYINFO);

    //박스 초기화 하고
    move(BOX_ROW, 0);
    addstr(BOX);

    //박스에 게임오버 메시지 출력
    static char msg[] = "  GAME OVER.. ";
    move(BOXTEXT_ROW, BOXTEXT_COL);
    addstr(msg);

    //최고 점수 갱신 (갱신 시 추가 메시지)
    if (score > maxScore) {
        maxScore = score;
        move(BOXTEXT_ROW, BOXTEXT_COL + sizeof(msg));
        printw("New Record! : %-10d", maxScore);
        move(MAX_SCORE_ROW, 0);
        printw("Max Score : %-10d", maxScore);
    }
    
    //커서 구석으로
    move(LINES-1, COLS-1);

    //게임 종료 여부
    while (1) {
        int c = getch();
        if (c == ' ') {
            //게임 진행
            return 0;
        }
        else if (c == '@') {
            //게임 종료
            return 1;
        }
    }
}




bool addQueue()        //문자를 큐에 추가 (+ 큐가 꽉 차면 false 반환)
{
    if (qSize >= BOX_SIZE) {
        return false;
    }
    else {
        //단어 읽기가 끝났거나 처음이면 새로운 단어 무작위 선택
        if (charPtr == NULL || *charPtr == '\0') {
            charPtr = wordEntry[rand() % WORDCOUNT_MAX];
        }
        //큐에 추가
        circularQueue[rear] = *charPtr;
        rear = (rear + 1) % BOX_SIZE;
        charPtr++;
        qSize++;

        //특정 문자 수 마다 속도 증가
        if (--speedUpRemain == 0) {
            speedUpInterval *= (1.0 / SPEEDUP_FACTOR);
            speedUpRemain = (int)speedUpInterval;
            speed *= SPEEDUP_FACTOR;
            set_ticker((int)speed);
            move(SPEED_ROW, 0);
            printw("Speed     : %-5.2f word/s   ", 1000.0 / speed);
        }
        return true;
    }
}


void refreshBox()      //박스 화면 갱신하기
{
    //박스
    move(BOXTEXT_ROW, BOXTEXT_COL);
    addstr(circularQueue);
    
    //커서 구석으로
    move(LINES-1, COLS-1);
    refresh();
    flushinp();
}


int set_ticker(int n_msecs)
{
    struct itimerval new_timeset;
    long n_sec, n_usecs;

    n_sec = n_msecs / 1000l;                    
    n_usecs = (n_msecs % 1000) * 1000L;         

    new_timeset.it_interval.tv_sec = n_sec;     // set reload
    new_timeset.it_interval.tv_usec = n_usecs;  // new ticker value
    new_timeset.it_value.tv_sec = n_sec;        // store this
    new_timeset.it_value.tv_usec = n_usecs;     // and this

    return setitimer(ITIMER_REAL, &new_timeset, NULL);
}

void addBox(int signum) //박스에 문자 추가하기 (타이머 핸들러)
{
    addQueue();
    refreshBox();
    //move(20, 0);
    //printw("front : %d, rear : %d, %d", front, rear, rand());
}
