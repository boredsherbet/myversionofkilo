/** includes **/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <sys/types.h>
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
/* Editor keys enum */
enum editorKey{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_DOWN, 
    ARROW_UP,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

/* structs*/
typedef struct erow{
    int len;
    char *text;
} erow;
struct editorConf{//it'll be easier to store global state this way instead of random vars
    struct termios og_termios;//it's really making me mad that termios doesn't have an n...
    int cursorx;//position of curor in file
    int cursory;//position of curor in file
    // NOTE: cursor position in a file requires use of rowoffset or columnoffset.
    // the cursorx and cursory variables are the position in the editor's window pane
    // the goal now is to make it that the offset deterines the window's coordinates (top, left)
    // and then the cursorx and cursory deterine the cursor's position in the file
    // then we can use the cursor's position to enter in stuff to the file.
    //
    int screenwidth;//tracks the number of columns/width of the editor
    int screenheight;//tracks the height of the editor
    int numrows;//tracks the number of rows in the text
    int rowoffset;//used for vertical scrolling purposes
    int coloffset;//used for horizontal scrolling purposes
    erow *rows; //stores all the rows on the editor.
    //NOTE: for the position of the cursor in the editor
    //x=cursorx-coloffset
    //y=cursory-rowoffset
};
struct buffer{//this will be a dynamic string that we use as a buffer.
    char *string;
    int len;
};
/* function prototypes */ //man i hate using C purely because of function prototypes... I planned not to use these but the dependencies are getting too confusing....
void movecursor(int key);
void appendRow(char *s, ssize_t len);
void initEditor();//initializes editor object that saves stuff about terminal
void die(const char *s);//everything went wrong, print message for info and kys (the program)
void closeKilo();//closes everything
void removeFlags(struct termios *old);//resets terminal
int getCursorPos(int *rows, int *columns);//gets the position of the cursor
int readKeys();//reads the key presses
int getWindowSize(int *rows, int *columns);//gets the size of the window
void processKeys();//processes key presses (and calls readkeys to read them)
void drawrows(struct buffer *buff);//draws the tildes, for now
void refreshScreen();//working on this one
void setFlags();//sets terminal flags to the values we need to build and run text editor
void bufferAppend(struct buffer *buffer, const char *string,int len ); // dynamic string append
void freeBuffer(struct buffer *buffer); //frees buffer memory
void openEditor(char *filename);
//opens an editor. no file input yet, but that will probably be a parameter later
struct editorConf Editor;

void initEditor(){
    if(getWindowSize(&Editor.screenheight,&Editor.screenwidth)==-1){die("getWindowSize");}
    //cursor is at position cursorx columns to the right 
    //and cursor y rows down when initialized
    Editor.cursorx=0;
    Editor.cursory=0;
    Editor.numrows=0;
    Editor.rowoffset=0;
    Editor.coloffset=0;
    Editor.rows=NULL; //npd if we fork up.
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

int readKeys(){
    char c;
    while (read(STDIN_FILENO, &c, 1)==-1){ //enters the while loop if no character is read
        if (errno!=EAGAIN){die("read");}
        //apparently errno is how C does error checking... weird...
    }
    if (c=='\x1b'){
        char seq[4];//we'll have this be the [A or [B or [C or [D for arrow keys
        if(read(STDIN_FILENO, &seq[0], 1)!=1){return c;}//it's a 3 char sequence, 
        if(read(STDIN_FILENO, &seq[1], 1)!=1){return c;}//again, 3 char sequence...
        if (seq[0]=='['){//it's a properly formatted escape sequence
            if (seq[1]>='0' &&seq[1]<='9'){ //there's a number in second position
                if(read(STDIN_FILENO, &seq[2], 1)!=1){return c;}//third position?
                if(seq[2]=='~'){//will be none if nothing is read...
                    switch(seq[1]){
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '1': return HOME_KEY;
                        case '8': return END_KEY;
                        case '3': return DEL_KEY;
                        case '7': return HOME_KEY;
                        case '4': return END_KEY;
                    }
                }
            } else{//there's no number in second position (aka there's a letter there)
                switch(seq[1]){//single character escape sequences
                    case 'A': return ARROW_UP;//up arrow key
                    case 'B': return ARROW_DOWN;//down arrow key
                    case 'C': return ARROW_RIGHT;//right arrow key
                    case 'D': return ARROW_LEFT;//left arrow key
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
            return '\x1b';// bracket read in on accident
        } else if(seq[0]=='O'){//does not start with bracket
            switch(seq[1]){
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
            return '\x1b';//O got read in on accident
        } else {
            return c;
        }
    }
    return c;
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

void editorscroll(){
    // TODO:utilizing the editor config, scroll the editor to place the cursor as necessary.
    // if it goes off in the y, adjus tthe rowoffset
    // if it goes off in the x, adjust the colofset as necessary.
    if (Editor.cursory<Editor.rowoffset){//the cursor is above the window
        Editor.rowoffset=Editor.cursory;
    } else if (Editor.cursory>Editor.rowoffset+Editor.screenheight-1){//y cursor below window
        Editor.rowoffset=Editor.cursory-Editor.screenheight+1;
    }
    if (Editor.coloffset<Editor.cursorx){//x cursor left of window
        Editor.coloffset=Editor.cursorx;
    }
    if (Editor.cursorx>Editor.coloffset+Editor.screenwidth-1){//x cursor right of window
        Editor.coloffset=Editor.cursorx-Editor.screenwidth;
    }
}

void processKeys(){
    int c=readKeys();
    switch (c) {
        case CTRL_KEY('q'): 
            write(STDOUT_FILENO,"\x1b[2J",4);
            write(STDOUT_FILENO,"\x1b[H",3);
            exit(0);
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case PAGE_UP:
        case PAGE_DOWN:
        case HOME_KEY:
        case END_KEY:
            movecursor(c);
            break;
    }
}
void movecursor(int key){
    switch(key) {
        case ARROW_LEFT://cursor left
            if (Editor.cursorx!=0){
                Editor.cursorx--;//move left
            }
            break;
        case ARROW_UP://cursor up
            if (Editor.cursory!=0){ 
                Editor.cursory--;//move up
            }
            break;
        case ARROW_DOWN: 
            if (Editor.cursory<Editor.numrows){//cursory is 0 indexed, but screenheight is 1 indexed
                Editor.cursory++;//move down
            }
            break;
        case ARROW_RIGHT:
            if (Editor.cursorx<Editor.rows[Editor.cursory].len-1){
                Editor.cursorx++;
            }
            break;
        case PAGE_DOWN:
        case PAGE_UP:{
            int times = Editor.screenheight;
            while (times--){
                movecursor(key==PAGE_UP?ARROW_UP:ARROW_DOWN);
            }
            break;
        }
        case HOME_KEY:
            Editor.cursorx=0;
            break;
        case END_KEY:
            Editor.cursorx=Editor.rows[Editor.cursory].len-1;
            break;
    }
    editorscroll();
}

void drawrows(struct buffer *buff){
    for (int y=0; y<Editor.screenheight; y++){//for every line in the height
        //we need to start at rowoffset, so...
        int linenumber=y+Editor.rowoffset;
        if (y==5){//FOR DEBUGGING PURPOSES.
            char debugLine[100];//for the debugging line
            int debuglen=snprintf(debugLine,sizeof(debugLine),"Editor at %d,%d. Pointer at %d,%d. Screen of %d,%d. %d rows in file.",Editor.coloffset,Editor.rowoffset,Editor.cursorx,Editor.cursory,Editor.screenwidth,Editor.screenheight,Editor.numrows);
            bufferAppend(buff, debugLine, debuglen);
        } else if (linenumber<Editor.numrows){//draws lines from the file
            //need to make sure it fits the size of the line in the terminal
            int len=Editor.rows[linenumber].len;
            len=len-Editor.coloffset;//our length decreases by however much our column offset is
            if (len>Editor.screenwidth-1){len=Editor.screenwidth;}//and truncate to fit screen
            if (len>0){
                //start printing from the coloffset point...
                bufferAppend(buff, &Editor.rows[linenumber].text[Editor.coloffset],len);
            }
        } else if (Editor.numrows!=0){//lines after file's completion
            bufferAppend(buff,"\x1b[2K~",5);//clear the line (and add a tilde)
        } else if (y != Editor.screenheight/3){//Editor.numrows==0
            bufferAppend(buff,"\x1b[2K~",5);//clear the line (and add a tilde)
        } else {//print the welcome message
            char welcomeMessage[80];
            int welcomelen=snprintf(welcomeMessage,sizeof(welcomeMessage),"Welcome to Kilo v.%s",VERSION);
            if (welcomelen>Editor.screenwidth){welcomelen=Editor.screenwidth;}//screen too small, truncate
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
            bufferAppend(buff,"\r\n",2);//move cursor to next line
        }
    }
}
void refreshScreen(){
    struct buffer buff = BUFFER_INIT;//dynamic string for buffer
    bufferAppend(&buff, "\x1b[2J", 4);//clear the screen
    bufferAppend(&buff,"\x1b[?25l",6);//hide cursor, remember bytes are by character
    bufferAppend(&buff,"\x1b[H",3 );//curor to home
    drawrows(&buff);
    //instead of moving back home, we're going to move to the x and y
    char buf_cursorpos[32];
    int EditorCursorPosy = Editor.cursory-Editor.rowoffset+1;
    int EditorCursorPosx = Editor.cursorx-Editor.coloffset+1;
    snprintf(buf_cursorpos,sizeof(buf_cursorpos),"\x1b[%d;%dH",EditorCursorPosy,EditorCursorPosx);
    //the terminal is indexed from 1 for some reason...
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
int main(int argc, char *argv[]) {
    setFlags();//sets the flags of terminal settings to disable terminal defaults
    initEditor();//screen width & height saved
    if (argc==2){
        openEditor(argv[1]);
    }
    while (1){
        refreshScreen();
        processKeys();
    }
    return 0;
}
/*append buffer (aka dynamic string)*/
void bufferAppend(struct buffer *buffer, const char *string,int len){//len here is the len of s
    char *new=realloc(buffer->string, buffer->len+len);
    if (new==NULL) return;//just error checking
    memcpy(&new[buffer->len],string, len);
    buffer->string=new;
    buffer->len+=len;
}
void freeBuffer(struct buffer *buffer){
    free(buffer->string);
}
/*** file i/o ***/
void openEditor(char *filename){
    FILE *fp =fopen(filename,"r");
    if (!fp) die("open");
    size_t linebuffsize=0;
    char *linebuff=NULL;
    ssize_t linelen;
    while ((linelen=getline(&linebuff,&linebuffsize,fp))!=-1){
        while (linelen>0 && (linebuff[linelen-1]=='\n' || linebuff[linelen-1]=='\r')){
            //we're only doing till the newline, so ignore anything after the newline
            linelen--;
        }
        appendRow(linebuff,linelen);
    }
    free (linebuff);
    fclose(fp);
}
void appendRow(char *s, ssize_t len){
    Editor.rows=realloc(Editor.rows, sizeof(erow) *(Editor.numrows+1));
    int at=Editor.numrows;
    Editor.rows[at].text=malloc(len+1);
    Editor.rows[at].len=len+1;
    memcpy(Editor.rows[at].text,s, len);
    Editor.rows[at].text[len]='\0';
    Editor.numrows++;
}

