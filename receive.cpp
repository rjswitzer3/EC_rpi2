#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <iostream>

int uart0_filestream = -1;

void init();
void recept();

int main(){

  init();
  //puts("Initialized");		//__TEST__

  char c = getchar();
  recept();

  //printf("Loop #: %d\n",cc);
  //close(uart0_filestream);

  return 0;
}

void init(){
  uart0_filestream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (uart0_filestream == -1){
		//ERROR - CAN'T OPEN SERIAL PORT
		printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
	}

	struct termios options;
	tcgetattr(uart0_filestream, &options);
	options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(uart0_filestream, TCIFLUSH);
	tcsetattr(uart0_filestream, TCSANOW, &options);

	puts("UART Initialized");	//__TEST__
}

void recept(){
	int cc;
	while(true){
  	if (uart0_filestream != -1){
		// Read up to 255 characters from the port if they are there
		unsigned char rx_buffer[256];
		int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)
		if (rx_length < 0){
			//An error occured (will occur if there are no bytes)
		}else if (rx_length == 0){
			printf("No Data.£");//No data waiting
		}
		else{
			//Bytes received
			rx_buffer[rx_length] = '\0';
			printf("%i bytes read : %s\n", rx_length, rx_buffer);
			char c = getchar();
		}
	}
	cc++; //printf("Loop #: %d",cc);
	}
}
