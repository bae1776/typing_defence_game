
/*   
 *   리눅스 내 명령어 메뉴얼에서 영어 단어를 검색해서 리스트로 반환하는 프로그램
 *   외부 오픈소스코드 : 해시 테이블 (단어 중복 방지용)
 *   https://github.com/zfletch/zhash-c
 */

#define WORDLEN_MIN 2
#define WORDLEN_MAX 32
#define WORDCOUNT_MAX 1000

//시스템 콜 호출 시 에러 처리 매크로
#define SYSCALL_CHECK(func, errorReturn, msg) \
    if ((func) == (errorReturn)) { perror(msg); perror(" system call!"); exit(1); }


//외부에서 유일하게 접근하는 함수
int write_wordList(const char* path);

//usr/bin 으로부터 명령어 목록을 받아오는 함수(일반 파일만)
char** get_cmdList(int* cmdCountp);

//메뉴얼 문서를 열고 단어를 추출하는 함수
int extract_words(const char* cmd, int fd, int* wordCountp);