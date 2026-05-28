#define _DEFAULT_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>



#define ESC 27
#define ML_STOP			0x20
#define ML_UP			0x02
#define ML_DOWN			0x01
#define ML_FIRE			0x10
#define ML_LEFT			0x04
#define ML_RIGHT		0x08


int max_x, max_y, mid_x, mid_y;

void show_cmd(void){
    mvprintw(20, 20, "Fire!! when you press \"f\"");
    mvprintw(mid_y, mid_x, "@");
    mvprintw(mid_y, mid_x-20, "left turn");
    mvprintw(mid_y, mid_x+10, "right turn");
    mvprintw(mid_y+5, mid_x, "down");
    mvprintw(mid_y-5, mid_x, "up");
    wbkgd(stdscr, COLOR_PAIR(1));
}

void cmd_send(char cmd){
    int fd;
    char buf[1];
    //open file
    fd = open("/dev/ml0", O_RDWR);
    if(fd<0){
        perror("perror output:");
        close(fd);
        exit(1);
    }

    buf[0]=cmd;
    //send cmd for 1 sec period
    if(-1==write(fd, &buf, 1)){
        perror("perror output:");
    }
    if(buf[0]!=ML_FIRE){
        usleep(500000);
    }else{
        sleep(10);
    }

    buf[0]=0x20;//ML_STOP
    if(-1==write(fd, &buf, 1)){
        perror("perror output:");
    }
    close(fd);    
}

int main(){
    
    

    initscr();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    curs_set(FALSE);
    getmaxyx(stdscr, max_y, max_x);
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    


    mid_y=max_y/2;
    mid_x=max_x/2;

    show_cmd();
    
    refresh();

    while(1){
        
        refresh();
        int ch=getch();
        if (ch==KEY_LEFT){
            clear();
            // wbkgd(stdscr, COLOR_PAIR(1));
            show_cmd();
            mvchgat(mid_y, mid_x-20, 9, A_NORMAL, 2, NULL);
            cmd_send(ML_LEFT);
        }
        else if(ch==KEY_RIGHT){
            clear();
            // wbkgd(stdscr, COLOR_PAIR(1));
            show_cmd();
            mvchgat(mid_y, mid_x+10, 10, A_NORMAL, 2, NULL);
            cmd_send(ML_RIGHT);
        }
        else if(ch==KEY_DOWN){
            clear();
            // wbkgd(stdscr, COLOR_PAIR(1));
            show_cmd();
            mvchgat(mid_y+5, mid_x, 4, A_NORMAL, 2, NULL);
            cmd_send(ML_DOWN);
        }
        else if(ch==KEY_UP){
            clear();
            // wbkgd(stdscr, COLOR_PAIR(1));
            show_cmd();
            mvchgat(mid_y-5, mid_x, 2, A_NORMAL, 2, NULL);
            cmd_send(ML_UP);
        }
        else if(ch=='f'){
            cmd_send(ML_FIRE);
        }
        else if(ch==ESC){
            break;
        }
        else{
            //do nothing!!

        }
        
    }
    
    endwin();
    exit(EXIT_SUCCESS);

}
