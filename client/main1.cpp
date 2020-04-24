#include <stdio.h>
#include <winsock2.h>
#include <SFML/Audio.hpp>

int getFileList(SOCKET *sock, char ***f_list);

int main(int argc , char *argv[])
{
	WSADATA wsa;
	SOCKET s;
	struct sockaddr_in server;
	char *message , server_reply[2000];
	int recv_size;

	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		return 1;
	}
	
	printf("Initialised.\n");
	
	if((s = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d" ,  WSAGetLastError());
	}

	printf("Socket created.\n");
	
	server.sin_addr.s_addr = inet_addr("192.168.1.251");
	server.sin_family = AF_INET;
	server.sin_port = htons( 8888 );

	//Connect to remote server
	if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		puts("connect error");
		return 1;
	}
	puts("Connected");
	
	if((recv_size = recv(s, server_reply, 2000, 0)) == SOCKET_ERROR)
	{
		puts("recv failed");
	}
	server_reply[recv_size] = '\0';
	puts(server_reply);  

	char **f_list;
	int count = getFileList(&s, &f_list);
	for(int i = 0; i < count; i++){
		printf("%i: %s\n", i, f_list[i]);
	}  

    closesocket(s);
    WSACleanup();

	return 0;
}

int getFileList(SOCKET *sock, char ***fileList){
	SOCKET s = *sock;
	int recv_size;
	char countChar[6], **f_list;

	if(send(s, "0", 1, 0) < 0) {
		puts("Send failed");
		return 1;
	}
	
	printf("File list:\n");
	if((recv_size = recv(s, countChar, 2000, 0)) == SOCKET_ERROR) {
		puts("recv failed");
	}
	countChar[recv_size] = '\0';
	int fileCount = atoi(countChar);
	f_list = (char**)malloc(fileCount*sizeof(char*));
	for(int i = 0; i < fileCount; i++){
		f_list[i] = (char*)malloc(50*sizeof(char));
	}
	char tmp[500000];
	char *buff;
	if((recv_size = recv(s, tmp, 500000, 0)) == SOCKET_ERROR) {
		puts("recv failed");
	}
	tmp[recv_size] = '\0';
	buff = strtok(tmp, ",");
	for(int i = 0; i < fileCount; i++){
		buff = strtok(NULL, ".");
		strcpy(f_list[i], buff);
		buff = strtok(NULL,",");
	}

	*fileList = f_list;
	closesocket(s);
	return fileCount;
}