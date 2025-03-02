/*
 *
 * CSEE 4840 Lab 2 for 2025
 *
 * Name/UNI: Amanda Jenkins (alj2155); Swapnil Banerjee ()
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000



#define BUFFER_SIZE 128

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);

/*
* Chat History (Rows 0–19): Displays messages received from the chat server.
* User Input Area (Rows 22–23): Where the user types messages before sending them.
*/


//Row index for the last line of the message display area (rows 0-19; can be edited though)
int row_idx = 19;

//Column index for the next character to be printed in the output area
int col_idx = 0;

/* 2D character array to store the chat messages displayed on the screen
*  20 lines of text; each line can hold up to 64 chars (CAN BE CHANGED)
*/ 
char display[20][64];


/*
* Func for the display of chat messages on the screen.
* Upward scrolling to making room for new messages at the bottom of the screen.
* message[] is the buffer to store user's input before it is sent; may not need to be this large
*/

void fbdisplay(char message[2][64]) {
 int rows, cols;
 int counter=1;
  for (rows = 0; rows < 18; rows++) {
    for (cols = 0; cols < 64; cols++) {
        display[rows][cols] = display[rows+2][cols];  // Move row (r+2) to row (r)
    }
  }
    memset(display[18],' ',64);
    memset(display[19],' ',64);

  for (rows = 18; rows < 20; rows++) {
    for (cols = 0; cols < 64; cols++) {
      display[rows][cols] = message[rows-18][cols]; // message[0] goes to row 18, message[1] to row 19
    }
  }

  // redraws the entire display on the framebuffer
  for (rows = 0; rows < 20; rows++) {
      for (cols = 0; cols < 64; cols++){
          fbputchar(display[rows][cols], rows+1, cols); // displays each character at updated row & column
      }
//if(counter=1){

//memset(display[18],' ',64);
//memset(display[19],' ',64);
//counter++;  }
}
}

//Sends the message to the chat server over a TCP socket.
void server_send(char *sent_msg) {
    if (write(sockfd, sent_msg, strlen(sent_msg)) > 0) {
        printf("SENT: %s\n", sent_msg);
    } else {
        perror("Error sending message to server");
    }
}



int main()
{
  int err, col;
  int counter = 1; 
  struct sockaddr_in serv_addr;
  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];

  //20 lines; 64 char buffer, we can change the buffer size
  char msg[2][64];

  /* We want to track which row the user is currently typing in. So start at row 21/22
  * then track column, so where the cursor should be in the 64 length buffer for user input, so 0.
  */
  int cursor_place = 0;
  int the_rows = 22;
  int columns = 0;


  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('*', 0, col);
    fbputchar('*', 23, col);
  }

  //To be commented out!
  //fbputs("Hello CSEE 4840 World!", 4, 10);


    memset(display, ' ', sizeof(display));
  // memset(display[14], ' ', 64);
  // memset(display[15], ' ', 64);
    memset(display[18], ' ', 64);
  // memset(display[17], ' ', 64);
    memset(display[19], ' ', 64);
  // memset(display[20], ' ', 64);



  /*
  * Clearing the screen, adding the horizontal line at row 20 or 21? (center of screen), and setting cursor 
  * Can make this a function in itself if needed
  */
  clear();

  // line across row 20 or 21?, from column 0 to 63 (the whole length of text allowed)
  for (col = 0; col < 64; col++) {
    fbputchar('-', 21, col);
  }
for(col=0;col<64;col++){
fbputchar('*',19,col);
fbputchar('*',20,col);
}


  // horizontal line at the top (row 0) as long as 60 columns.
  for (col = 0; col < 64; col++) {
    fbputchar('-', 0, col);
  }
  //this will set the cursor at row 22 with the text and then column 0 (beginning)
  fbputchar('_', 22, 0);

  /* Open the keyboard */
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }

  /* Create a TCP communications socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  /* Look for and handle keypresses */
  for (;;) {
    libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);
    if (transferred == sizeof(packet)) {
      sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
	      packet.keycode[1]);
      printf("%s\n", keystate);
      fbputs(keystate, 6, 0);
      if (packet.keycode[0] == 0x29) { /* ESC pressed? */
	break;
      }
      /*
      * To handle the keyboard input; can also write this in a seperate function
      */
      if (packet.keycode[0] == 0x28) { //Enter is PRESSED    
	      int rows, cols;

        //memset(display, ' ', sizeof(display));
        //memset(display[17],' ',64);
        fbdisplay(msg);
        server_send(msg[0]); 
        //server_send(msg[1]);
        memset(msg, ' ', sizeof(msg));
	      memset(display[19],' ',64);
  //       clear();
  //       fbdisplay(msg);

  //       // redraws the entire display on the framebuffer
  //       for (rows = 0; rows < 20; rows++) {
  //           for (cols = 0; cols < 64; cols++){
  //           fbputchar(display[rows][cols], rows+1, cols); // displays each character at updated row & column
  //       }
  // }
  //       counter++;
  //       }
  //       else{
  //         fbdisplay(msg);
  //       }

      // This clears the message buffer
      for (cols = 0; cols < 64; cols++) {
        for (rows = 0; rows < 2; rows++) {
          msg[rows][cols] = ' ';
        }
      }
      // This clears the input area on the screen
      for (rows = 22; rows < 24; rows++) {
        for (cols = 0; cols < 64; cols++) {
          fbputchar(' ', rows, cols);
        }
      }
      //resets the cursor back to row 22
      fbputchar('_', 22, 0);
      cursor_place = 0;
	    columns = 0;
	    the_rows = 22;
    }
    //memset(display,' ',sizeof(display));
//backspace
if(packet.keycode[0]==0x2A){
if(columns>0){
columns--;
msg[the_rows-22][columns]=' ';
fbputchar(' ',the_rows,columns);
fbputchar('_',the_rows,columns);
fbputchar(' ',the_rows,columns+1);
}
}

//memset(msg,' ',sizeof(msg));

// Left arrow kpace
if(packet.keycode[0]==0x50){
  if(columns>0){
    char prev_char = msg[the_rows-22][columns];
    fbputchar(prev_char,the_rows,columns);
    columns--;
    printf("Program update, Key pressed");
    fbputchar('_',the_rows,columns);
    fbputchar(prev_char,the_rows,columns-1);
}
  continue;
  //fbputchar(prev_char,the_rows,columns);
}

// Right arrow Key
if (packet.keycode[0] == 0x4F) {
  if (columns < 63 && columns > 0 && msg[the_rows-22][columns + 1] != '\0') { 
      fbputchar(msg[the_rows-22][columns], the_rows, columns); // Restore previous character
      columns++; // Move cursor right
      fbputchar('_', the_rows, columns); // Place cursor at new position
  }
  continue;
}


    /*
    * KEYS ARE PRESSED; cursor is set to correct place
    */

    if (cursor_place == 0) {
      columns = 0;
      cursor_place = 1;
    }
    // this converts keycode to ASCII & store in message buffer
    char input = key_input(keystate);
    if (input != '\0')
 
{
   //checks if it is a valid char
   // if(keystate[1]=='5'){
    // if(keystate[2]!='0'){
      if (columns < 63) {  //check condition that the row has space
        msg[the_rows-22][columns] = input;             // store typed character
        fbputchar(input, the_rows, columns);           // display on screen
        fbputchar('_', the_rows, columns + 1);         // morw cursor forward by 1
        columns++;
//	}
//	}
      }
if(keystate[1]=='5' && keystate[2]=='0')
{
return '\0';
}
      //checks if the row is full (enables text wrapping) -> move to next row in this case
      else if ((columns == 63) & (the_rows == 22)) { 
        msg[0][63] = input;                       // Store the last character in row 22
        fbputchar(input, 22, 63);
        fbputchar('_', 23, 0);                    // Move cursor to row 23
        columns = 0;
        the_rows = 23;
      } 
    }
    fbputchar(key_input(keystate), 0, 54);


	    


    }
  }
  /* Terminate the network thread */
  pthread_cancel(network_thread);

  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

  return 0;
}

//should run concurrently with the main program
void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int data;
  /* Receive data */
  while ((data = read(sockfd, recvBuf, BUFFER_SIZE - 1)) > 0) {
    recvBuf[data] = '\0';  // Null-terminate the received message
    printf("show something"); // Print received message for debugging
    
    // memset(display, ' ', sizeof(display));

    // Shift old messages up to make room for new ones
    int r, c;
    for (r = 0; r < 18; r++) {
        for (c = 0; c < 64; c++) {
            display[r][c] = display[r + 2][c];
        }
    }

    //memset(display[18], ' ', 64);
    // Copy new message into the last two rows
    strncpy(display[18], recvBuf, 64);

    // Redraw framebuffer with new messages
    for (r = 0; r < 20; r++) {
        for (c = 0; c < 64; c++) {
            fbputchar(display[r][c], r + 1, c);
        }
    
}
  }

if (data == 0) {
    printf("## Server disconnected\n");
  } else {
      perror("## Error reading from server");
  }

return NULL;
}

  // FOR CLIENT SERVER SEDNING MESSAGES!!


  // messages recieved from the chat server to display them 
  //strncpy(printBuf[0], recvBuf, BUFFER_SIZE/2);
  //strncpy(printBuf[2], recvBuf, BUFFER_SIZE/2);
  
  //fbdisplay(printBuf);

//   return NULL;
// }
