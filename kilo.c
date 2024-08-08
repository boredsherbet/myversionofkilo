/** includes **/
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
/* defines */
#define CTRL_KEY(k) ((k) & 0x1f)
/* function prototypes */ //man i hate using C purely because of function prototypes... I planned not to use these but the dependencies are getting too confusing....

void initEditor();//initializes editor object that saves stuff about terminal
void die(const char *s);//everything went wrong, print message for info and kys (the program)
void closeKilo();//closes everything
void removeFlags(struct termios *old);//resets terminal
int getCursorPos(int *rows, int *columns);//gets the position of the cursor
char readKeys();//reads the key presses
int getWindowSize(int *rows, int *columns);//gets the size of the window
void processKeys();//processes key presses (and calls readkeys to read them)
void drawRows();//draws the tildes, for now
void refreshScreen();//working on this one
void setFlags();//sets terminal flags to the values we need to build and run text editor
/* structs and stuffs*/
struct editorConf{//it'll be easier to store global state this way instead of random vars
    struct termios og_termios;//it's really making me mad that termios doesn't have an n...
    int screenwidth; 
    int screenheight;
};
struct editorConf Editor;

void initEditor(){
    if(getWindowSize(&Editor.screenheight,&Editor.screenwidth)==-1){die("getWindowSize");}
}
void die(const char *s){
    write(STDOUT_FILENO,"\x1b[2J",4);
    write(STDOUT_FILENO,"\x1b[H",3);
    perror(s);
    exit(1);
}
void closekilo(){
    if (tcsetattr(STDERR_FILENO,TCSAFLUSH,&Editor.og_termios)==-1){die("tcsetarr");}
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
int getCursorPos(int *rows, int *columns){
    if (write(STDOUT_FILENO, "\x1b[6n",3)==-1) return -1;
    printf("\r\n");
    char buffer[32];
    unsigned int i=1;
    while (i<sizeof(buffer)){//reads from standard input
        if (read(STDIN_FILENO,&buffer[i],1)!=1){break;}//get out if we're done reading stuff
        if (buffer[i]=='R'){break;}//or if we hit the R
        i++;//next character
    }
    buffer[i]='\0';//install the null character at the end of the buffer-- C string!
    printf("\r\nPointer at: %s",&buffer[1]);//remember first character is escape, so we skip it.
    readKeys();
    return -1;
}

char readKeys(){
    char c;
    while (1){
        if (read(STDIN_FILENO, &c, 1)==-1 && errno!=EAGAIN){die("read");}
        //apparently errno is how C does error checking... weird...
        return c;
    }
}

int getWindowSize(int *rows, int *columns){//this way we can get rows and cols to work with directly, instead of having to deal with the window struct
    struct winsize windowsize;
    if(1 || ioctl(STDOUT_FILENO,TIOCGWINSZ,&windowsize)==-1 || windowsize.ws_col==0){
        //maybe ioctl doesn't work on this system, try manually...
        if (write(STDOUT_FILENO,"\x1b[999B\x1b[999C",12)!=12) return -1;//an error occurred
        return getCursorPos(rows, columns);
    }
    *columns=windowsize.ws_col;
    *rows=windowsize.ws_row;
    return 0;
    //but sometimes ioctl doesn't work (eg. with windows...)
}
void processKeys(){
    char c=readKeys();
    switch (c) {
        case CTRL_KEY('q'): 
            write(STDOUT_FILENO,"\x1b[2J",4);
            write(STDOUT_FILENO,"\x1b[H",3);
            exit(0);
            break;
        case 'b':
            break;
    }
}
void drawrows(){
    for (int y=0; y<Editor.screenheight; y++){
        write(STDOUT_FILENO,"~\r\n",3);
    }
}
void refreshScreen(){
    write(STDOUT_FILENO,"\x1b[2J",4);//clears the whole screen--/x1b is escape for operator [2J which clears screen (4 bytes)
    write(STDOUT_FILENO,"\x1b[H",3);//unlike J, H (cursor to home) is just using its default 
    drawrows();
    write(STDOUT_FILENO,"\x1b[H",3);
}
void setFlags(){
    if ((tcgetattr(STDERR_FILENO,&Editor.og_termios))==-1){die("tcsetattr");} //get terminal state, die if fails
    atexit(closekilo);//stdlib.h function, forces exit function at exit no matter what.
    struct termios now_termios=Editor.og_termios; //maintains current state of terminal
    removeFlags(&now_termios); //change current state of terminal
}
/* main */
int main() {
    setFlags();//sets the flags of terminal settings to disable terminal defaults
    initEditor();//screen width & height saved
    while (1){
        refreshScreen();
        processKeys();
    }
    return 0;
}