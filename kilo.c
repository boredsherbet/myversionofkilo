#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
struct termios og_termios;

void die(const char *s){
    perror(s);
    exit(1);
}
void closekilo(){
    //this function does exit stuff
    if (tcsetattr(STDERR_FILENO,TCSAFLUSH,&og_termios)==-1){die("tcsetarr");}
    // just resetting the terminal
}

void removeFlags(struct termios *old){
    old->c_lflag&= ~(ECHO | IEXTEN | ICANON | ISIG);
    old->c_oflag&= ~(OPOST);
    old->c_iflag&= ~(IXON | BRKINT | INPCK | ISTRIP | ICRNL);
    old->c_cflag&= ~(CS8); //not a flag, sets to 8 bit charset
    // old->c_lflag&= ~(ECHO); 
    // modifies the c_lflag value, which is a sequenece of bits
    // oneof those bits controls ECHO (ECHO is a bitflag)
    // we pull the value of echo, then invert it
    // which produces a value where it's all 1s, except for ECHO
    // anding that against c_lflag will prouce something that preserves
    // all other values in c_lflag, while flipping ECHO (which became 0 due to inversion)
    // ~(ECHO | ICANON) just lets there be two bits that have 0s after inversion (2 things get turned off)
    if ((tcsetattr(STDIN_FILENO,TCSAFLUSH,old))==-1){
        die("tcsetattr");
    } //add changes to terminal, die if fails.
}


int main() {
    if ((tcgetattr(STDERR_FILENO,&og_termios))==-1){die("tcsetattr");} //get terminal state, die if fails
    atexit(closekilo);//stdlib.h function, forces exit function at exit no matter what.
    struct termios now_termios=og_termios; //maintains current state of terminal
    removeFlags(&now_termios); //enable raw input
    char c;
    printf("Type a key to find it's ascii code.\r\n");
    while (1){
        if (read(STDIN_FILENO, &c, 1)==-1){die("read");}
        if (iscntrl(c)){
            printf("%d\r\n", c);
        } else{
            printf("%c (%d)\r\n", c, c);
        }
        if (c=='q'){break;}
    }
    return 0;
}