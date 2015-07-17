#ifndef __CONSOLE_STUFF_H__
#define __CONSOLE_STUFF_H__
#ifndef _WIN32_
#include <termios.h>
#include <stropts.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
//getch in linux ( a little tricky )
static struct termios old, theNew;

/* Initialize theNew terminal i/o settings */
void initTermios(int echo)
{
  tcgetattr(0, &old); /* grab old terminal i/o settings */
  theNew = old; /* make theNew settings same as old settings */
  theNew.c_lflag &= ~ICANON; /* disable buffered i/o */
  theNew.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
  tcsetattr(0, TCSANOW, &theNew); /* use these theNew terminal i/o settings now */
}
/* Read 1 character - echo defines echo mode */
char getch_(int echo)
{
  char ch;
  initTermios(echo);
  ch = getchar();
  tcsetattr(0, TCSANOW, &old); //reset termios
  return ch;
}
/* Read 1 character without echo */
char getch(void){return getch_(0);}
/* Read 1 character with echo */
char getche(void){return getch_(1);}

//no kbhit either!
int kbhit (void)
{
  initTermios(0);
  struct timeval tv;
  fd_set rdfs;

  tv.tv_sec = 0;
  tv.tv_usec = 20000; //20ms

  FD_ZERO(&rdfs);
  FD_SET (STDIN_FILENO, &rdfs);

  select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
  tcsetattr(0, TCSANOW, &old); //reset termios
  return FD_ISSET(STDIN_FILENO, &rdfs);

}
#endif
#endif