#include <unistd.h>
#include <termios.h>

void enableRaw(struct termios *old){
    old->c_lflag&= ~(ECHO);
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&old);
}
int main() {
    struct termios initial;
    tcgetattr(STDERR_FILENO,&initial);
    struct termios new=initial;
    enableRaw(&new);
    char c;
    while ((read(STDIN_FILENO, &c, 1)==1) && c!='q'){
    }
    tcsetattr(STDERR_FILENO,TCSANOW,&initial); // just resetting the terminal
    return 0;
}