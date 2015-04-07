/*
  MoveMouse.c
  
  Copyright 2014 Madhusudhan <mrameshk@buffalo.edu>
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
  MA 02110-1301, USA.
  
  
 */


#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <termios.h>

/* Used for select() */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <time.h>
#include <X11/Xlib.h>
#include <wiringPi.h>
/* DEBUG_FLAG set to 0 will disable all LOGS
 * DEBUG_FLAG set to 1 will enable all LOGS 
 * */
#define DEBUG_FLAG 1

#if DEBUG_FLAG == 1
#define LOG(...) 	printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

/* Handy MACRO to die gracefully */
#define die(str, args...) do { \
        printf(str); \
        exit(EXIT_FAILURE); \
    } while(0)
    

void move(int x,int y);
void init_uevent();
void init_uart();
void destroy_and_clean_up();
void recv_glint_location();
void test();
int get_mouse_pointer_position(int *x, int *y);
void set_mousePointer();

int uevent_fd;
int uart_fd;
	
static int prev_x, prev_y;
struct uinput_user_dev uidev;
struct input_event     ev;

static int _XlibErrorHandler(Display *display, XErrorEvent *event) {
    LOG("An error occured detecting the mouse position\n");
    return True;
}

wiringPiSetup () ;
pinMode (0, OUTPUT);

digitalWrite (0, HIGH) ;
delay(50);

digitalWrite (0, LOW);
delay(50);
	
int main(int argc, char argv)
{	
	
	int retval;
	fd_set read_fds, master_fds;
	struct timeval tv;
	
	/* Wait up to one second. */
    tv.tv_sec 	= 1;
    tv.tv_usec 	= 0;
		
	init_uevent();
	
	init_uart();
	
	/*Setup file descriptor set*/    
    FD_ZERO(&read_fds);
    FD_ZERO(&master_fds);
    FD_SET(uart_fd, &read_fds);
    
    /* This is done because every time select() returns, the read_fds
     * is cleared
     */
    master_fds = read_fds;
    
    /* Select() always takes in the highest file descriptor plus one
     * and passing NULL in the last parameter signifies that there is
     * no timeout and we will wait indefinitely for an I/O event to occur
     * on the UART fil descriptor
     */
	while(1)
	{
		FD_SET(uart_fd, &read_fds);
		
		retval = select(uart_fd + 1, &read_fds, NULL, NULL, &tv);
		
		if(retval == -1)
		{
			/* Something went wrong with select() */
			LOG("\n Something went wrong with select() \n");
			LOG("\n Error : %s", strerror(errno));
			destroy_and_clean_up();
			exit(EXIT_FAILURE);
		}
		
		if(retval == 0)
		{
			LOG("Time out has occured! \n");
		}
		
		if(FD_ISSET(uart_fd, &read_fds))
		{
			/* We have data from uart_fd, go ahead and read it */
			recv_glint_location();
		}
		
		/* Reseting the read_fds from master_fds */
		read_fds = master_fds;
	}

	LOG("Exiting \n");
	destroy_and_clean_up();
	
	return 0;
}

void move(int x,int y)
{
	int dx,dy;
	
	dx = x - prev_x;
	prev_x = x;
	
	dy = y - prev_y;
	prev_y = y;
	
	LOG("\nStart - move \n");
	int size = sizeof(ev);
	int i;
	
	memset(&ev, 0, sizeof(struct input_event));
	ev.type = EV_REL;
	ev.code = REL_X;
	ev.value = dx;
	if(write(uevent_fd, &ev, sizeof(struct input_event)) < 0)
		die("error: write");

	memset(&ev, 0, sizeof(struct input_event));
	ev.type = EV_REL;
	ev.code = REL_Y;
	ev.value = dy;
	if(write(uevent_fd, &ev, sizeof(struct input_event)) < 0)
		die("error: write");

	memset(&ev, 0, sizeof(struct input_event));
	ev.type = EV_SYN;
	ev.code = 0;
	ev.value = 0;
	if(write(uevent_fd, &ev, sizeof(struct input_event)) < 0)
		die("error: write");
		
	LOG("\nEnd - move \n");
}

void init_uevent()
{
	LOG("Start - init\n");
	uevent_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(uevent_fd < 0)
        die("error: open");

    if(ioctl(uevent_fd, UI_SET_EVBIT, EV_KEY) < 0)
        die("error: ioctl");
    if(ioctl(uevent_fd, UI_SET_KEYBIT, BTN_LEFT) < 0)
        die("error: ioctl");

    if(ioctl(uevent_fd, UI_SET_EVBIT, EV_REL) < 0)
        die("error: ioctl");
    if(ioctl(uevent_fd, UI_SET_RELBIT, REL_X) < 0)
        die("error: ioctl");
    if(ioctl(uevent_fd, UI_SET_RELBIT, REL_Y) < 0)
        die("error: ioctl");
        
        
	memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-sample");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1;
    uidev.id.product = 0x1;
    uidev.id.version = 1;

    if(write(uevent_fd, &uidev, sizeof(uidev)) < 0)
        die("error: write");

    if(ioctl(uevent_fd, UI_DEV_CREATE) < 0)
        die("error: ioctl");

	LOG("End - init\n");

}

void destroy_and_clean_up()
{
	if(ioctl(uevent_fd, UI_DEV_DESTROY) < 0)
        die("error: ioctl");
}

void recv_glint_location()
{
	char x_pos[5], y_pos[5];
	char rx_buffer[256];
	int i;
	int x,y;
	
	//Filestream, buffer to store in, number of bytes to read (max)
	int rx_length = read(uart_fd, (void *)rx_buffer, sizeof(char) * 255);
	
	if(rx_length == 8)
	{
		for(i=0; i<4; i++)
		{			
			x_pos[i] = rx_buffer[i];
		}
		x_pos[i] = '\0';
		
		for(i=0; i<4; i++)
		{			
			if(isdigit(rx_buffer[i + 4]))
				y_pos[i] = rx_buffer[i+4];
			else 
				y_pos[i] = 0;
		}
		y_pos[i] = '\0';
		
		x = atoi(x_pos);
		y = atoi(y_pos);
		
		/* Scale X and Y */
		x = x * 2.5;
		y = y * 2.5;
		
		move(x, y);
	}
	else
	{
		LOG("\n Recieved JUNK \n");
		LOG("\n Read length is %d \n",rx_length);
	}
	
}

void test()
{
	move(50, 50);
		sleep(3);
			
	set_mousePointer(150, 150);	
	
	exit(0);
}

void init_uart()
{
	uart_fd = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (uart_fd == -1)
	{
		printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
		printf("ERROR is : %s",strerror(errno));
		assert(0);
	}
 
    struct termios options;
	tcgetattr(uart_fd, &options);
	options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_iflag = IGNPAR | ICRNL;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(uart_fd, TCIFLUSH);
	tcsetattr(uart_fd, TCSANOW, &options);
}

int get_mouse_pointer_position(int *x, int *y) 
{
    int number_of_screens;
    int i;
    Bool result;
    Window *root_windows;
    Window window_returned;
    int root_x, root_y;
    int win_x, win_y;
    unsigned int mask_return;

    Display *display = XOpenDisplay(NULL);
    assert(display);
    XSetErrorHandler(_XlibErrorHandler);
    number_of_screens = XScreenCount(display);
    LOG("There are %d screens available in this X session\n", number_of_screens);
    root_windows = malloc(sizeof(Window) * number_of_screens);
    for (i = 0; i < number_of_screens; i++) {
        root_windows[i] = XRootWindow(display, i);
    }
    for (i = 0; i < number_of_screens; i++) {
        result = XQueryPointer(display, root_windows[i], &window_returned,
                &window_returned, &root_x, &root_y, &win_x, &win_y,
                &mask_return);
        if (result == True) {
            break;
        }
    }
    if (result != True) {
        fprintf(stderr, "No mouse found.\n");
        return -1;
    }
    LOG("Mouse is at (%d,%d)\n", root_x, root_y);
    *x = root_x;
    *y = root_y;

    free(root_windows);
    XCloseDisplay(display);
    
    return 0;
}

void set_mousePointer(int goal_x, int goal_y)
{
	int cur_x, cur_y;

	assert(get_mouse_pointer_position(&cur_x, &cur_y) == 0);
	
	move((goal_x - cur_x), (goal_y - cur_y));
	
}
