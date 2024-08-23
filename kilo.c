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
#define TAB_STOP 8
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
    char *render;
    int rlen;
} erow;
struct editorConf{//it'll be easier to store global state this way instead of random vars
    struct termios og_termios;//it's really making me mad that termios doesn't have an n...
    int cursorx;//position of curor in file
    int cursory;//position of curor in file
    int screenwidth;//tracks the number of columns/width of the editor
    int screenheight;//tracks the height of the editor
    int numrows;//tracks the number of rows in the text
    int rowoffset;//used for vertical scrolling purposes
    int coloffset;//used for horizontal scrolling purposes
    erow *rows; //stores all the rows on the editor.
    int renderx;//stores the cursor's position in the render's line (diverts from cursorx because of tabs)
    int debug_mode;//displays the debug line in refreshscreen()
    //NOTE: for the position of the cursor in the editor
    //x=cursorx-coloffset+1
    //y=cursory-rowoffset+1 
};
struct buffer{//this will be a dynamic string that we use as a buffer.
    char *string;
    int len;
};
/* function prototypes */ //man i hate using C purely because of function prototypes... I planned not to use these but the dependencies are getting too confusing....
int CursortoRender(int cursorx);
void movecursor(int key);
void updateRow(erow *row);
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
    Editor.debug_mode=0;//automatically not in debug mode
    Editor.renderx=0;
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

int CursortoRender(int cursorx){
    int renderx=0;
    for (int i=0; i<cursorx; i++){
        char current = Editor.rows[Editor.cursory].text[i];
        switch(current){
            case '\t':
                renderx+=(TAB_STOP-1)-(renderx%TAB_STOP);
                break;
        }
        renderx++;//either way, we'll go one index forward
    }
    return renderx;
}
void editorscroll(){
    Editor.renderx=CursortoRender(Editor.cursorx);
    //NOTE: the bug is here. I'm trying to access a non-allocated cursor x, 
    //and im getting a bufferoverflow. since my cursor x value is too high
    //going up or too high going down. it seems I need to pass in a value 
    //that is definitively in the file. I'll do that by just editing it 
    //in the movecursor(). Like I did for the horizontal movement skipping 
    //to the next line.
    if (Editor.cursory<Editor.rowoffset){//the cursor is above the window
        Editor.rowoffset=Editor.cursory;
        editorscroll();
    } else if (Editor.cursory>Editor.rowoffset+Editor.screenheight-1){//y cursor below window
        Editor.rowoffset=Editor.cursory-Editor.screenheight+1;
        editorscroll();
    }
    if (Editor.renderx<Editor.coloffset){//x cursor left of window
        Editor.coloffset=Editor.renderx;
        editorscroll();
    } else if (Editor.renderx>Editor.coloffset+Editor.screenwidth-1){//x cursor right of window
        Editor.coloffset=Editor.renderx-Editor.screenwidth+1;
        editorscroll();
    }
    if (Editor.renderx>Editor.rows[Editor.cursory].rlen-1){//x cursor too far right, outside file
        Editor.renderx=Editor.rows[Editor.cursory].rlen-1;
        editorscroll();
    }
}

void processKeys(){
    int c=readKeys();
    switch (c) {
        case CTRL_KEY('q')://exit the editor
            write(STDOUT_FILENO,"\x1b[2J",4);
            write(STDOUT_FILENO,"\x1b[H",3);
            exit(0);
            break;
        case CTRL_KEY('a'): //turn on/off debug line
            Editor.debug_mode= !Editor.debug_mode;
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
            } else if (Editor.cursory!=0){
                Editor.cursory--;
                Editor.cursorx=Editor.rows[Editor.cursory].len;
            }
            break;

        case ARROW_UP://cursor up
            if (Editor.cursory!=0){ 
                Editor.cursory--;//move up
                if (Editor.cursorx>=Editor.rows[Editor.cursory].len){
                    Editor.cursorx=Editor.rows[Editor.cursory].len-1;
                }
            }

            break;
        case ARROW_DOWN: 
            if (Editor.cursory<Editor.numrows-1){//cursory is 0 indexed, but screenheight is 1 indexed
                Editor.cursory++;//move down
                if (Editor.cursorx>=Editor.rows[Editor.cursory].len){
                    Editor.cursorx=Editor.rows[Editor.cursory].len-1;
                }
            }
            break;
        case ARROW_RIGHT:
            if (Editor.cursorx<Editor.rows[Editor.cursory].len-1){
                Editor.cursorx++;
            } else if (Editor.cursory<Editor.numrows-1){
                Editor.cursorx=0;
                Editor.cursory+=1;
            }
            break;
        case PAGE_DOWN:
        case PAGE_UP:{
            if (key==PAGE_DOWN){
                Editor.cursory=Editor.rowoffset+Editor.screenheight-1;
                if (Editor.cursory>=Editor.numrows) {
                    Editor.cursory=Editor.numrows-1;
                }
            } else if (key==PAGE_UP){
                Editor.cursory=Editor.rowoffset;
            }
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
        if (Editor.debug_mode && y==5){//FOR DEBUGGING PURPOSES, and since I'm dumb.
            char debugLine[100];//for the debugging line
            int debuglen=snprintf(debugLine,sizeof(debugLine),"Editor at %d,%d. Pointer at %d,%d, rendering %d th character. Screen of %d,%d. %d rows in file.",Editor.coloffset,Editor.rowoffset,Editor.cursorx,Editor.cursory,Editor.renderx,Editor.screenwidth,Editor.screenheight,Editor.numrows);
            bufferAppend(buff, debugLine, debuglen);
        } else if (linenumber<Editor.numrows){//draws lines from the file
            //need to make sure it fits the size of the line in the terminal
            int len=Editor.rows[linenumber].rlen;
            len-=Editor.coloffset;//our length decreases by however much our column offset is
            if (len>Editor.screenwidth-1){len=Editor.screenwidth;}//and truncate to fit screen
            if (len>0){//is there anything left to be printed?
                //start printing from the coloffset point...
                bufferAppend(buff, &Editor.rows[linenumber].render[Editor.coloffset],len);
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
                    //NOTE: you can use while(increment--) to decrement to 0 from increment
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
    int EditorCursorPosx = Editor.renderx-Editor.coloffset+1;
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
void updateRow(erow *row){
    int text_index;
    int tabs=0;
    for(text_index=0;text_index<row->len; text_index++){
        //here we'll define the amount of memory extensions we need to make
        char current = row->text[text_index];
        switch(current){
            case '\t':
                tabs++;
                break;
        }
    }
    free(row->render);//no buffer issues
    row->render=malloc(row->len+tabs*(TAB_STOP-1)+1);
    int render_index = 0;
    for (text_index=0; text_index<row->len; text_index++){
        //not using memcpy because different characters render diferently. 
        switch(row->text[text_index]){
            case '\t':
                row->render[render_index++] = ' ';
                while (render_index%TAB_STOP!=0) row->render[render_index++] = ' ';
                //NOTE: Apparently doing this with a for loop might result in misaligned tabs
                //this just allows for the easier creation of tables, for example and really
                //standardizes spacing tbh. I just thought a for loop to do it 8 times would be good
                //but then that happened so... we've come to this.
                break;
            default:
                row->render[render_index++]=row->text[text_index];
                break;
        }
    }
    row->render[render_index++]='\0';
    row->rlen=render_index;
}
void appendRow(char *s, ssize_t len){
    Editor.rows=realloc(Editor.rows, sizeof(erow) *(Editor.numrows+1));
    int at=Editor.numrows;
    Editor.rows[at].text=malloc(len+1);
    Editor.rows[at].len=len+1;
    memcpy(Editor.rows[at].text,s, len);
    Editor.rows[at].text[len]='\0';
    Editor.rows[at].rlen=0;
    Editor.rows[at].render=NULL;
    updateRow(&Editor.rows[at]);//this will give us rendering info for each row of the file.
    Editor.numrows++;
}
