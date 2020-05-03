#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <filesystem>
#include <cstring>
#include <string>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
extern "C"{
    #include <libavformat/avformat.h>
    #include <libavutil/dict.h>
}

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
int clid = 0;

void *connectionHandler(void *socket);
void streamFile(const char * filename, int *sock_desc, bool *streaming);

void signal_callback_handler(int signum){
        printf("Caught signal SIGPIPE %d\n",signum);
}

namespace fs = std::filesystem;

int main(int argc, char *argv[]){
    int socket_desc, new_socket, c, *new_sock;
    struct sockaddr_in server, client;
    char *message;

    signal(SIGPIPE, signal_callback_handler);

    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8890);

    if(bind(socket_desc, (struct sockaddr*)&server , sizeof(server)) < 0) {
        puts("bind failed");
        return 1;
    }
    puts("Socket Created");

    listen(socket_desc, 3);

    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    while( (new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {
        puts("Connection accepted");

        pthread_t sniffer_thread;
        new_sock = (int*)malloc(1);
        *new_sock = new_socket;
         
        if(pthread_create( &sniffer_thread, NULL,  connectionHandler, (void*) new_sock) < 0) {
            perror("could not create thread");
            return 1;
        }
    }
     
    if (new_socket<0) {
        perror("accept failed");
        return 1;
    }
     
    return 0;
}

void *connectionHandler(void *socket){
    int sock = *(int *)socket;
    int *new_sock;
    new_sock = (int*)malloc(1);
    *new_sock = sock;
	char *message, client_msg[2000];
	int recv_size;
	int clientID = clid;
	clid++;

	//Reply to the client
	message = (char*)"Connection Recieved\n\0";
	write(sock, message, strlen(message));

	while ((recv_size = recv(sock, client_msg, 2000, 0)) > 0) {
        client_msg[recv_size] = '\0';
    	char *fileTitle, *filebuff, *buff, *title, *album, *artist;
        std::string names;
        int count, offset, lentmp;
        std::ifstream sendFile;
        std::ofstream outFile, dbfile;
        std::stringstream fileName, lenstr;
        fs::path fspath;
        long dbsize;
        AVFormatContext *fmt_ctx = NULL;
        AVDictionary* metadata;
        AVDictionaryEntry *tag = NULL;

        switch((int)client_msg[0]){
            case '0':       //request files///////////////////////////////////////////////////////////////////
            printf("recieved 0\n");
            //names = " ,";
			count = 0;
            for(auto& dir_entry : fs::directory_iterator("music")){
                //printf("%s\n", dir_entry.path().filename().string().c_str());
				//names.append(dir_entry.path().filename().string());
				//names.append(",");
				count++;
            }
            char fileCount[7];
			sprintf(fileCount, "%i", count);
			write(sock, fileCount, strlen(fileCount));
            recv_size = recv(sock, client_msg, sizeof(fileCount), 0);
			fileCount[recv_size] = '\0';
			if(strcmp(fileCount, client_msg) != 0){
				printf("Information transfer corrupted");
				return 0;
			}

            sendFile = std::ifstream("database", std::ifstream::in);
            if(!sendFile.is_open()){
                printf("Invalid File\n");
                write(sock, "0", 1);
                break;
            }
            write(sock, "1", 1);
            fspath = fs::path("database");
            filebuff = new char[fs::file_size(fspath)];
            sendFile.read(filebuff, fs::file_size(fspath));

            //begin sending file
            buff = strtok(filebuff, ",");
	        for(int i = 0; i < count; i++){
	            title = strtok(NULL, ".");
                write(sock, title, strlen(title));
                recv(sock, client_msg, 1, 0);
                buff = strtok(NULL, ",");
	            album = strtok(NULL, ",");
                write(sock, album, strlen(album));
                recv(sock, client_msg, 1, 0);
                artist = strtok(NULL, ",");
                write(sock, artist, strlen(artist));
                recv(sock, client_msg, 1, 0);
            }
            //printf("sending:\n%s\n", filebuff);
            //while(offset < fs::file_size(fspath)){
            //    //printf("Current offset: %i\n", offset);
            //    int send_size = write(sock, filebuff + offset, fs::file_size(fspath) - offset);
            //    offset += send_size;
            //    printf("Sent %i bytes\n", offset);
            //}

            printf("Sent List\n");
            break;

            case '1':       //upload file//////////////////////////////////////////////////////////////////////////////////////////
            printf("Recieved 1\n");
            write(sock, "Ready", 5);
            recv_size = recv(sock, client_msg, 2000, 0);
            client_msg[recv_size] = '\0';
            fileName << "music/" << client_msg;
            fileTitle = new char[strlen(client_msg)];
            memcpy(fileTitle, client_msg, strlen(client_msg));
            printf("File name: %s\n", fileName.str().c_str());

            recv_size = recv(sock, client_msg, 2000, 0);
            client_msg[recv_size] = '\0';
            int fileSize; 
            fileSize = atoi(client_msg);
            printf("File size: %i\n", fileSize);
            if(fs::exists(fileName.str().c_str())){
                write(sock, "decline", 7);
                printf("file exists\n");
                break;
            }
            write(sock, "Ready", 5);
            outFile = std::ofstream(fileName.str().c_str(), std::ofstream::out|std::ofstream::binary);
            filebuff = (char *)malloc(fileSize*sizeof(char));
            offset = 0;
            while(offset < fileSize){
                recv_size = recv(sock, filebuff + offset, fileSize - offset, 0);
                offset += recv_size;
                //printf("Recieved %i out of %i\n", offset, fileSize);
            }
            outFile.write(filebuff, fileSize);
            write(sock, "1", 1);

            outFile.close();
            free(filebuff);
            printf("Recieved File %s\n", fileName.str().c_str());

            if(avformat_open_input(&fmt_ctx, fileName.str().c_str(), NULL, NULL) != 0){ 
                printf("Failed to open file format\n");
                close(sock);
                return NULL;
            }
            avformat_find_stream_info(fmt_ctx, NULL);
            dbfile = std::ofstream("database", std::ofstream::out|std::ofstream::app);
            metadata = fmt_ctx->metadata;
            while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
                printf("%s=%s\n", tag->key, tag->value);
            //album = av_dict_get(metadata, "album", NULL, 0)->value;
            //artist = av_dict_get(metadata, "artist", NULL, 0)->value;
            //printf("Album: %s\nArtist%s\n", album, artist);
            //dbfile << fileTitle << "," << album << "," << artist << ",";
            dbfile.close();
            avformat_close_input(&fmt_ctx);
            fileName.str("");
            break;

            case '2':       //Download file////////////////////////////////////////////////////////////////////////////////////////////
            //Recieve name of file
            printf("recieved 2\n");
            write(sock, "Ready", 5);
            recv_size = recv(sock, client_msg, 2000, 0);
            client_msg[recv_size] = '\0';
            fileName << "music/" << client_msg << ".mp3";
            sendFile = std::ifstream(fileName.str().c_str(), std::ifstream::in|std::ifstream::binary);
            if(!sendFile.is_open()){
                printf("Invalid File\n");
                write(sock, "0", 1);
                break;
            }
            //Send file size
            fspath = fs::path(fileName.str().c_str());
            filebuff = new char[fs::file_size(fspath)];
            lenstr << fs::file_size(fspath);
            printf("%s byte to send\n", lenstr.str().c_str());
            write(sock, lenstr.str().c_str(), sizeof(lenstr.str().c_str()));
            recv_size = recv(sock, client_msg, 2000, 0);
            client_msg[recv_size] = '\0';
            lentmp = atoi(client_msg);
            if(lentmp != fs::file_size(fspath)){
                printf("Error: miscommunication\n");
            }
            //begin sending file
            offset = 0;
            sendFile.read(filebuff, fs::file_size(fspath));
            while(offset < fs::file_size(fspath)){
                printf("Current offset: %i\n", offset);
                int send_size = write(sock, filebuff + offset, fs::file_size(fspath) - offset);
                offset += send_size;
            }
            printf("Sent file %s\n", fileName.str().c_str());
            //Cleanup
            sendFile.close();
            fileName.str("");
            free(filebuff);
            break;

            default:
            break;
        }
	}
    printf("Client %i disconnected\n", clientID);
    close(sock);

    return 0;
}