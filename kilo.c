/** includes **/
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
/* defines */
#define CTRL_KEY(k) ((k) & 0x1f)
/* terminal stuff */
struct termios og_termios;
void die(const char *s){
    perror(s);
    exit(1);
}
void closekilo(){
    if (tcsetattr(STDERR_FILENO,TCSAFLUSH,&og_termios)==-1){die("tcsetarr");}
    // just resetting the terminal
}

void removeFlags(struct termios *old){
    old->c_lflag&= ~(ECHO | IEXTEN | ICANON | ISIG);
    old->c_oflag&= ~(OPOST);
    old->c_iflag&= ~(IXON | BRKINT | INPCK | ISTRIP | ICRNL);
    old->c_cflag&= ~(CS8); 
    //turns off a bunch of flags, and sets charset to CS8
    if ((tcsetattr(STDIN_FILENO,TCSAFLUSH,old))==-1){
        die("tcsetattr");
    } //modify state of terminal, die if fails.
}

/* init */
int main() {
    if ((tcgetattr(STDERR_FILENO,&og_termios))==-1){die("tcsetattr");} //get terminal state, die if fails
    atexit(closekilo);//stdlib.h function, forces exit function at exit no matter what.
    struct termios now_termios=og_termios; //maintains current state of terminal
    removeFlags(&now_termios); //enable raw input
    char c;
    printf("Type a key to find it's ascii code.\r\n");
    while (1){
        if (read(STDIN_FILENO, &c, 1)==-1 && errno!=EAGAIN){die("read");}
        //apparently errno is how C does error checking... weird...
        if (iscntrl(c)){
            printf("%d\r\n", c);
        } else{
            printf("%c (%d)\r\n", c, c);
        }
        if (c==CTRL_KEY('q')){ break;} //exit with ctrl+q
    }
    return 0;
}