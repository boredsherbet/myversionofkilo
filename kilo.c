/** includes **/
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

/* defines */
#define CTRL_KEY(k) ((k) & 0x1f)
#define BUFFER_INIT {NULL,0}
#define VERSION "0.0.1"

/* structs*/
struct editorConf{//it'll be easier to store global state this way instead of random vars
    struct termios og_termios;//it's really making me mad that termios doesn't have an n...
    int cursorx;
    int cursory;
    int screenwidth; 
    int screenheight;
};
struct buffer{//this will be a dynamic string that we use as a buffer.
    char *string;
    int len;
};
/* function prototypes */ //man i hate using C purely because of function prototypes... I planned not to use these but the dependencies are getting too confusing....
void movecursor(char key);
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
void bufferAppend(struct buffer *buffer, const char *string,int len ); // dynamic string append
void freeBuffer(struct buffer *buffer); //frees buffer memory
struct editorConf Editor;

void initEditor(){
    if(getWindowSize(&Editor.screenheight,&Editor.screenwidth)==-1){die("getWindowSize");}
    //cursor is at position cursorx columns to the right 
    //and cursor y rows down when initialized
    Editor.cursorx=0;
    Editor.cursory=0;
}
void die(const char *s){
    write(STDOUT_FILENO,"\x1b[2J",4);
    write(STDOUT_FILENO,"\x1b[H",3);
    printf("I died.\r\n");
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
    if (write(STDOUT_FILENO, "\x1b[6n",4)==-1) return -1;
    printf("\r\n");
    char buffer[32];
    unsigned int i=0;
    while (i<sizeof(buffer)){//reads from standard input
        if (read(STDIN_FILENO,&buffer[i],1)!=1){break;}//get out if we're done reading stuff
        if (buffer[i]=='R'){break;}//or if we hit the R
        i++;//next character
    }
    buffer[i]='\0';//install the null character at the end of the buffer-- C string!
    if (buffer[0]!='\x1b' || buffer[1]!='[') {die("getCursorPos");}
    if (sscanf(&buffer[1],"[%d;%d",rows,columns)!=2){//first char is escape, so skip it
        return -1;//some kind of error with scanf
    }
    return 0;
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
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&windowsize)==-1 || windowsize.ws_col==0){
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
        case 'w':
        case 'a':
        case 's':
        case 'd':
            movecursor(c);
            break;
    }
}
void movecursor(char key){
    switch(key) {
        case 'a'://cursor left
            if (Editor.cursorx!=0){
                Editor.cursorx--;
            }
            break;
        case 'w'://cursor up
            if (Editor.cursory!=0){
                Editor.cursory--;
            }
            break;
        case 's': 
            if (Editor.cursory<Editor.screenheight){
                Editor.cursory++;
            }
            break;
        case 'd':
            if (Editor.cursorx<Editor.screenwidth){
                Editor.cursorx++;
            }
            break;
    }
}
void drawrows(struct buffer *buff){
    for (int y=0; y<Editor.screenheight; y++){//for every line in the height
        if (y != Editor.screenheight/3){//when you're not a third of the way down
            bufferAppend(buff,"\x1b[2K~",5);//clear the line (and add a tilde)
        } else {//print the welcome message
            char welcomeMessage[80];
            int welcomelen=snprintf(welcomeMessage,sizeof(welcomeMessage),"Welcome to Kilo v.%s",VERSION);
            if (welcomelen>Editor.screenwidth){welcomelen=Editor.screenwidth;}//snprintf will prolly return 80 (but we can't be sure)
            else {
                int padding=(Editor.screenwidth-welcomelen)/2;
                if (padding){ //if there is padding (aka padding!=0)
                    bufferAppend(buff,"~",1);
                    padding--;
                }
                while (padding--){ //and fill out the rest of the padding
                    //note: you can use while(increment--) to decrement to 0 from increment
                    //this is only possible when increment>0, which is guaranteed here
                    //since we checked that Editor.screenwidth<=welcomelen before we entered this block
                    bufferAppend(buff," ",1);
                }
            }
            bufferAppend(buff, welcomeMessage,welcomelen);
        }
        if (y!=Editor.screenheight-1){//no extra newline at the end of the terminal
            bufferAppend(buff,"\r\n",2);
        }
    }
}
void refreshScreen(){
    struct buffer buff = BUFFER_INIT;//dynamic string for buffer
    bufferAppend(&buff,"\x1b[?25l",6);//hide cursor, remember bytes are by character
    bufferAppend(&buff,"\x1b[H",3 );//curor to home
    drawrows(&buff);
    bufferAppend(&buff,"\x1b[H",3 );//back to home
    //instead of moving back home, we're going to move to the x and y
    char buf_cursorpos[32];
    snprintf(buf_cursorpos,sizeof(buf_cursorpos),"\x1b[%d;%dH",Editor.cursory+1,Editor.cursorx+1);//the terminal is indexed from 1 for some reason...
    bufferAppend(&buff,buf_cursorpos,strlen(buf_cursorpos));
    bufferAppend(&buff,"\x1b[?25h",6);//show the cursor
    write(STDOUT_FILENO,buff.string,buff.len);//flush the buffer to STDOUT_FILENO
    freeBuffer(&buff);//frees buff memory!
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
/*append buffer (aka dynamic string)*/
void bufferAppend(struct buffer *buffer, const char *string,int len ){//len here is the len of s
    char *new=realloc(buffer->string, buffer->len+len);
    if (new==NULL) return;//just error checking
    memcpy(&new[buffer->len],string, len);
    buffer->string=new;
    buffer->len+=len;
    /*
    Notes:
    realloc is a little weird. in this function, it could create a new block of memory
    or it could just extend the current one that buffer->string points to.
    because of this, it essentially gets rid of the original memory location that
    buffer->string pointed to. Accessing that before the reassignment to new would
    mean accessing memory from outside the program's perview. The memory at buffer->string
    was deleted by realloc by then, so it's just random bits. 
    When we use memcpy, the contents from string get copied at the location starting
    from where the null pointer of the string we copied into new (via realloc). 
    We then reassign this new "*new" value to our buffer->string, and blammo, 
    our buffer->string pointer is now the *new pointer and points to the new string.
     */
}
void freeBuffer(struct buffer *buffer){
    free(buffer->string);
}
