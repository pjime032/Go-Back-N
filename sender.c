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
#include <arpa/inet.h>
#include <netdb.h>
#define BUFFSIZE 256
#define MAXPAY 1024
#define HSIZE 15 // 1B ACK, 4B seq#, 4B numPackets, 2B checksum, 4B for spaces
#define WINSIZE 100
#define TIMEOUT 10000

void syserr(char *msg) { perror(msg); exit(-1); }
uint16_t ChkSum(char * packet, int psize);

int main(int argc, char *argv[])
{
  int sockfd, tempfd, portno, n;
  uint32_t seqNum, numPackets, size, base;
  uint16_t checksum;
  uint8_t ack;
  struct timeval t1, t2;
  struct sockaddr_in serv_addr, clt_addr; 
  struct hostent* server; 
  struct stat filestats;
  struct timeval tv;
  fd_set readfds;
  socklen_t addrlen;
  char payload[MAXPAY];

  if(argc != 4) { 
    fprintf(stderr,"Usage: %s <IP> <port> <name of file>\n", argv[0]);
    return 1;
  } 
  else
  { 
	server = gethostbyname(argv[1]);
	if(!server) {
		fprintf(stderr, "ERROR: no such host: %s\n", argv[1]);
		return 2;
	}
  	portno = atoi(argv[2]);
  }
  
  //Socket Logic
  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(sockfd < 0) syserr("can't open socket"); 
  	printf("create socket...\n");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr = *((struct in_addr*)server->h_addr);
  serv_addr.sin_port = htons(portno);
  addrlen = sizeof(serv_addr);
  
  //Make array of packet elements to send
  stat(argv[3], &filestats);
  size = filestats.st_size;
  numPackets = size/MAXPAY + 1;
  printf("num of packets: %d\n", numPackets);
  char ** fileArray;
  fileArray = (char **) malloc(sizeof(char*) * numPackets);
  seqNum = 0;
  checksum = 0;
  //Open File
  tempfd = open(argv[3], O_RDWR);
  if(tempfd < 0) syserr("failed to open file");
  //Populate the array with packets to send
  	int k = 0;
  while(1){
  	char * packet;
  	ack = 0;
  	packet = malloc(sizeof(char)*(HSIZE + MAXPAY + 1));
  	char payload[MAXPAY];
  	
  	memset(packet, 0, (HSIZE + MAXPAY + 1));
  	//set ack
  	packet[0] = ack & 255;
  	packet[1] = ' ';
  	//set seq#
  	packet[2] = (seqNum >>  24) & 0xFF;
  	packet[3] = (seqNum >>  16) & 0xFF;
  	packet[4] = (seqNum >>  8) & 0xFF;
  	packet[5] = seqNum & 0xFF;
  	packet[6] = ' ';
  	//set numPackets
  	packet[7] = (numPackets >>  24) & 255;
  	packet[8] = (numPackets >>  16) & 255;
  	packet[9] = (numPackets >>  8) & 255;
  	packet[10] = numPackets & 255;
  	packet[11] = ' ';
  	//set checksum
  	packet[12] = (checksum >>  8) & 255;
  	packet[13] = checksum & 255;
  	packet[14] = ' ';
  	
  	printf("The seqnum is: %d\n", seqNum);
  	/*
  	int i;
  	for(i =0 ; i<16; i++){
  		printf("packet at %d is: %d\n", i, packet[i]);
  	}
  	*/
  	//uint32_t test;
  	//memcpy(&test, &packet[2], sizeof(uint32_t));
  	//printf("The test has: %d\n", test);
  	seqNum = (uint32_t)(((uint16_t)((uint8_t)packet[2]<<8)+(uint8_t)packet[3])<<16)+(uint16_t)((uint8_t)packet[4]<<8)+(uint8_t)packet[5];
  	//seqNum = (uint32_t)((uint16_t)((uint8_t) packet[5] + ((uint8_t) packet[4] << 8)) + ((uint16_t)((uint8_t) packet[3] + ((uint8_t) packet[2] << 8)) << 16));
  	//seqNum = (uint32_t) packet[5] | (uint32_t) packet[4] << 8 | 
	  		 //  (uint32_t) packet[3] << 16 | (uint32_t) packet[2] << 24;
  	
//  	printf("the seqnum bitseq is: %u, %u, %u, %u\n", (uint8_t) packet[2], (uint8_t) packet[3], (uint8_t) packet[4], (uint8_t) packet[5]);
//  	printf("The rebuilt seqnum is: %u\n", seqNum);
  	// Read payload from file, add to packet
  	int bytes_read = read(tempfd, payload, MAXPAY);
	payload[bytes_read] = '\0';
  	//printf("bytes read are: %u. packet size is: %u. payload size: %d\n", bytes_read, sizeof(packet), sizeof(payload));
  	//printf("The string is: %s\n", packet);
	if (bytes_read == 0) // We're done reading from the file
		break;
	if (bytes_read < 0) syserr("error reading file");
	strcat(packet, payload); //Set Payload
	checksum = ChkSum(packet, HSIZE + MAXPAY + 1);
  	packet[12] = (checksum >>  8) & 255;
  	packet[13] = checksum & 255;
  	//checksum = ChkSum(packet, HSIZE + MAXPAY + 1);
  	//checksum = (uint16_t)((uint8_t) packet[13] | (uint8_t) packet[12] << 8);
  	//printf("checksum: %d\n", checksum);
  	fileArray[seqNum] = packet;
  	printf("Size of filearray: %lu\n", sizeof(fileArray[seqNum]));
  	char ppacket[HSIZE + MAXPAY + 1];
  				strcpy(ppacket, fileArray[seqNum]);
  				uint32_t seqNum2 = (uint32_t)((uint16_t)((uint8_t) ppacket[5] + ((uint8_t) ppacket[4] << 8)) + ((uint16_t)((uint8_t) ppacket[3] + ((uint8_t) ppacket[2] << 8)) << 16));
  				printf("The 2nd rebuilt seqnum is: %u\n", seqNum2); 
  	seqNum++; 	
  	//printf("seq num @ end: %d\n", seqNum);
  	if (k == 50) break;
  	k++;
  }
  close(tempfd);
  printf("All packets were created\n");
  
  //Set up Go-Back-N loop
  tv.tv_sec = 0;
  tv.tv_usec = 100;
  seqNum = 0;
  base = 0;
  //char  * ackPac = malloc(sizeof(char)*(HSIZE + 1)); 
  exit(0);
  //Send & Recv packets
  while(1){
  	FD_ZERO(&readfds);
  	FD_SET(sockfd, &readfds);
  	select(sockfd+1, &readfds, NULL, NULL, &tv);
  	if(FD_ISSET (sockfd, &readfds)){ 	//recv acks
  		printf("Got a packet");
  		char ackPac[HSIZE + 1];
  		n = recvfrom(sockfd, ackPac, (HSIZE + 1), 0, 
  		  (struct sockaddr*)&serv_addr, &addrlen); 
  		if(n < 0) syserr("can't receive ack packages");
  		
  		//check if corrupt
  		checksum = ChkSum(ackPac, (HSIZE + 1));
  		if(checksum == 0){				//Not Corrupt
  			base = (uint32_t) ackPac[5] | (uint32_t) ackPac[4] << 8 | 
  			  (uint32_t) ackPac[3] << 16 | (uint32_t) ackPac[2] << 24;
  			if(base == numPackets) break; //Finished sending packets
  			base++; 					//Set base to next seqnum 
  			if (base != seqNum){
  				gettimeofday(&t1, NULL);
  			}
  		}
  		else{							//ack packet was corrupt
	  		gettimeofday(&t2, NULL);
	  		double elaps = (t2.tv_sec - t1.tv_sec) * 1000.0;
	  		elaps += (t2.tv_usec - t2.tv_usec) /1000.0;
	  		while(elaps < TIMEOUT){}	// Loop until timeout occurs
	  		if( elaps > TIMEOUT){		// clock timedout, resend packets
	  			gettimeofday(&t1, NULL);
	  			int i;
	  			for(i = base; i < seqNum; i++){
	  				n = sendto(sockfd, fileArray[i], (HSIZE + MAXPAY + 1), 0, 
	  			  		(struct sockaddr*)&serv_addr, addrlen);
	  				if(n < 0) syserr("can't send to receiver");
	  			}
	  		}
  		}
  	}
  	else{								//send packets
  		printf("Sending a packet\n");
  		if(seqNum < base + WINSIZE){	//window not maxed out
  			n = sendto(sockfd, fileArray[seqNum], (HSIZE + MAXPAY + 1), 0, 
  			  (struct sockaddr*)&serv_addr, addrlen);
  			if(n < 0) syserr("can't send to receiver");
  			
  				char packet[HSIZE + MAXPAY + 1];
  				strcpy(packet, fileArray[seqNum]);
  				uint32_t seqNum2 = (uint32_t)((uint16_t)((uint8_t) packet[5] + ((uint8_t) packet[4] << 8)) + ((uint16_t)((uint8_t) packet[3] + ((uint8_t) packet[2] << 8)) << 16));
  				printf("The 2nd rebuilt seqnum is: %u\n", seqNum2);
  				break;
  				
  			if(base == seqNum){
  				gettimeofday(&t1, NULL);
  			}
  			seqNum++;
  		}
  		gettimeofday(&t2, NULL);
  		double elaps = (t2.tv_sec - t1.tv_sec) * 1000.0;
  		elaps += (t2.tv_usec - t2.tv_usec) /1000.0;
  		if( elaps > TIMEOUT){			// clock timedout, resend packets
  			gettimeofday(&t1, NULL);
  			int i;
  			for(i = base; i < seqNum; i++){
  				n = sendto(sockfd, fileArray[i], (HSIZE + MAXPAY + 1), 0, 
  			  		(struct sockaddr*)&serv_addr, addrlen);
  				if(n < 0) syserr("can't send to receiver");
  			}
  		}
  	}
  	
  }
  close(sockfd);
  return 0;
}

uint16_t ChkSum(char * packet, int psize){
	uint16_t checksum = 0, curr = 0, i = 0;
	//sscanf(packet, "%*s %*s %*s %u", &checksum);
	//printf("checksum is: %d. Packet Size is: %d\n", checksum, psize);
	while(psize > 0){
		curr = ((packet[i] << 8) + packet[i+1]) + checksum;
		checksum = curr + 0x0FFFF;
		curr = (curr >> 16); //Grab the carryout if it exists
		checksum = curr + checksum;
		psize -= 2;
		i += 2;
	}
	return ~checksum;
}

