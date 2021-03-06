#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#define BUFFSIZE 256
#define PACSIZE 1040
#define MAXPAY 1024
#define ACKSIZE 16
#define HSIZE 15

void syserr(char *msg) { perror(msg); exit(-1); }
uint16_t ChkSum(char * packet, int psize);
void ftpcomm(int newsockfd, char* buffer);
void readandsend(int tempfd, int newsockfd, char* buffer);
void recvandwrite(int tempfd, int newsockfd, int size, char* buffer);

int main(int argc, char *argv[])
{
  int sockfd, tempfd, portno, n, numPacketsEmpty;
  uint32_t seqNum, exSeqNum, numPackets;
  uint16_t checksum;
  uint8_t ack;
  struct sockaddr_in serv_addr, clt_addr;
  struct timeval tv;
  fd_set readfds;
  socklen_t addrlen;
  char * recvIP;
  char packet[PACSIZE];
  char ackPac[ACKSIZE];

  if(argc != 3) { 
    fprintf(stderr,"Usage: %s <port> <name of file>\n", argv[0]);
    return 1;
  } 
  else
  { 
  	portno = atoi(argv[1]);
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd < 0) syserr("can't open socket"); 
  	printf("create socket...\n");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  addrlen = sizeof(clt_addr); 

  if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
    syserr("can't bind");
  printf("bind socket to port %d...\n", portno);
  
  //Set up to receive
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  tempfd = open(argv[2], O_CREAT | O_WRONLY, 0666);
  if(tempfd < 0) syserr("failed to open the file");
  memset(packet, 0, PACSIZE);
  memset(ackPac, 0, ACKSIZE);
  exSeqNum = 0;
  numPacketsEmpty = 1;
  printf("Set up to receive\n");
  
  //Receive packet
  while(1){
  	FD_ZERO(&readfds);
  	FD_SET(sockfd, &readfds);
  	select(sockfd+1, &readfds, NULL, NULL, &tv);
  	//If packet is received
  	if(FD_ISSET(sockfd, &readfds)){
	  n = recvfrom(sockfd, &packet, PACSIZE, 0, 
	  	(struct sockaddr*)&clt_addr, &addrlen);
	  if(n < 0) syserr("can't receive from sender"); 
	  if(n > 0) printf("Received from: %d\n", n);
	  printf("debug: %u, %u, %u, %u\n", (uint8_t)packet[2], (uint8_t)packet[3], (uint8_t)packet[4], (uint8_t)packet[5]);
	  
	  seqNum = (uint32_t)((uint16_t)((uint8_t) packet[5] + ((uint8_t) packet[4] << 8)) + ((uint16_t)((uint8_t) packet[3] + ((uint8_t) packet[2] << 8)) << 16));
	  checksum = ChkSum(packet, PACSIZE);
	  if(numPacketsEmpty){
	  	numPacketsEmpty = 0;
	  	numPackets = (uint32_t) packet[10] | (uint32_t) packet[9] << 8 | 
	  			(uint32_t) packet[8] << 16 | (uint32_t) packet[7] << 24;
	  }
	  
	  //Check if packet is correct
	  if((checksum == 0 || checksum == 256) && seqNum == exSeqNum){
	  	ack = 1;
	  	checksum = 0;
	  	//set ack
	  	ackPac[0] = ack & 255;
	  	ackPac[1] = ' ';
	  	//set seq#
	  	ackPac[2] = (seqNum >>  24) & 0xFF;
	  	ackPac[3] = (seqNum >>  16) & 255;
	  	ackPac[4] = (seqNum >>  8) & 255;
	  	ackPac[5] = seqNum & 255;
	  	ackPac[6] = ' ';
	  	//set numPackets
	  	ackPac[7] = (numPackets >>  24) & 255;
	  	ackPac[8] = (numPackets >>  16) & 255;
	  	ackPac[9] = (numPackets >>  8) & 255;
	  	ackPac[10] = numPackets & 255;
	  	ackPac[11] = ' ';
	  	//set checksum
	  	ackPac[12] = (checksum >>  8) & 255;
	  	ackPac[13] = checksum & 255;
	  	ackPac[14] = ' ';
	  	ackPac[15] = ' ';
	  	
	  	//calculate and set checksum
	  	checksum = ChkSum(ackPac, ACKSIZE);
	  	ackPac[12] = (checksum >>  8) & 255;
	  	ackPac[13] = checksum & 255;	
	  		printf("checksum is: %d\n", checksum);
	  	/*
	  	int i;
  		for(i =0 ; i<16; i++){
  			printf("ack acket at %d is: %d\n", i, ackPac[i]);
  		}
	  	*/
	  	n = sendto(sockfd, ackPac, ACKSIZE, 0, 
	  		(struct sockaddr*)&clt_addr, addrlen);
  		if(n < 0) syserr("can't send to receiver");
  		//if(n > 0) printf("sent out %d bytes\n", n);
	  	
	  	exSeqNum++;
	  	//Write 1 KB packet to file
	  	if(seqNum != numPackets){
	  		n = write(tempfd, &packet[HSIZE + 1], MAXPAY);
	  		if(n < 0) syserr("can't write to file");
	  	}
	  	else{
	  		// Find where string terminates and write to file
	  		printf("eof lines running\n");
	  		int i;
	  		char eof;
	  		int eofLoc = MAXPAY;
	  		for(i=0; i <= MAXPAY; i++){
	  			eof = packet[HSIZE + 1 + i];
	  			if(eof == '\0'){
	  				eofLoc  = i;
	  				break;
	  			} 
	  		}
	  		n = write(tempfd, &packet[HSIZE + 1], eofLoc);
	  		if(n < 0) syserr("can't write @ end of file");
	  	}
	  	
	  }
	  }
	  else if(exSeqNum > 0){ 	//packet fault, but past first packet
	  	 n = sendto(sockfd, ackPac, ACKSIZE, 0, 
	  		 (struct sockaddr*)&clt_addr, addrlen);
  		 if(n < 0) syserr("can't send to receiver");
	  }
	/*
	  
	}
	else{
		if(seqNum != numPackets)
			printf("File not received, timeout after 60 secs\n");
		else
			printf("File receieved, timeout after 60 secs\n");
		break;
	}
	*/
  } 
  
  close(sockfd); 
  close(tempfd);
  return 0;
}

uint16_t ChkSum(char * packet, int psize){
	uint16_t checksum = 0, curr = 0, i = 0;
	//sscanf(packet, "%*s %*s %*s %u", &checksum);
	//printf("checksum is: %d. Packet Size is: %d\n", checksum, psize);
	while(psize > 0){
		curr = (uint16_t)((packet[i] << 8) + packet[i+1]) + checksum;
		checksum = curr + 0xFFFF;
		curr = (curr >> 16); //Grab the carryout if it exists
		checksum = curr + checksum;
		psize -= 2;
		i += 2;
	}
	return ~checksum;
}
