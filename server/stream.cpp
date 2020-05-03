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
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
}

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
int clid = 0;

void *connectionHandler(void *socket);
void streamFile(const char * filename, int *sock_desc, bool *streaming);

namespace fs = std::filesystem;

void signal_callback_handler(int signum){
        printf("Caught signal SIGPIPE %d\n",signum);
}

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
    server.sin_port = htons(8889);

    if(bind(socket_desc, (struct sockaddr*)&server , sizeof(server)) < 0) {
        puts("bind failed");
        return 1;
    }
    puts("Streaming Socket Created");

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
	char *message, client_msg[2000];
	int recv_size;
	int clientID = clid;
	clid++;
    std::stringstream fileName;
    std::string msg;

	//Reply to the client
	message = (char*)"Connection Recieved\n\0";
	write(sock, message, strlen(message));
    printf("Sent %s", message);

	//Recieve Name of file
    recv_size = recv(sock, client_msg, 2000, 0);
    client_msg[recv_size] = '\0';
    fileName << "/home/powerhouse/Notebooks/MusicServer/music/" << client_msg << ".mp3";
    std::string tmp = fileName.str();
    const char *filename = tmp.c_str();
    printf("Recieved name of file: %s\n", filename);

    AVFormatContext *fctx = NULL;
    if(avformat_open_input(&fctx, filename, NULL, NULL) < 0){
        printf("Could not open file\n");
        write(sock, "0", 1);
        close(sock);
        return NULL;
    }
    write(sock, "1", 1);
    avformat_find_stream_info(fctx, NULL);
    int stream_id;
    for(int i = 0; i < fctx->nb_streams; i++){
        if(fctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream_id = i;
            printf("Stream id: %i\n", stream_id);
            break;
        }
    }
    if(stream_id == -1){
        printf("Failed to find audio stream\n");
        avformat_close_input(&fctx);
        write(sock, "0", 1);
        close(sock);
        return NULL;
    }
    AVCodec *codec = avcodec_find_decoder(fctx->streams[stream_id]->codecpar->codec_id);;
    if(codec == NULL){
        printf("Decoder not found\n");
        avformat_close_input(&fctx);
        write(sock, "0", 1);
        close(sock);
        return NULL;
    }
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if(ctx == NULL){
        avformat_close_input(&fctx);
        write(sock, "0", 1);
        close(sock);
        return NULL;
    }
    if (avcodec_parameters_to_context(ctx, fctx->streams[stream_id]->codecpar) != 0) {
        printf("Error setting codec parameters\n");
        avcodec_close(ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fctx);
        write(sock, "0", 1);
        close(sock);
        return NULL;
    }
    //ctx->request_sample_fmt = av_get_alt_sample_fmt(ctx->sample_fmt, 0);
    if (avcodec_open2(ctx, codec, NULL) != 0) {
        avcodec_close(ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fctx);
        write(sock, "0", 1);
        close(sock);
        return NULL;
    }

    //Sample Size
    int ssize = av_get_bytes_per_sample(ctx->sample_fmt);
    if(ssize != 4){
        printf("incompatable type\n");
        avcodec_close(ctx);
        avcodec_free_context(&ctx);
        avformat_close_input(&fctx);
        write(sock, "0", 1);
        close(sock);
        return NULL;
    }
    msg = std::to_string(ssize);
    write(sock, msg.c_str(), strlen(msg.c_str()));
    recv_size = recv(sock, client_msg, 2000, 0);
    client_msg[recv_size] = '\0';
    printf("Sample Size\t%s\n", client_msg);
    //Sample Rate
    int sampleRate = ctx->sample_rate;
    msg = std::to_string(sampleRate);
    write(sock, msg.c_str(), strlen(msg.c_str()));
    recv_size = recv(sock, client_msg, 2000, 0);
    client_msg[recv_size] = '\0';
    printf("Sample Rate\t%s\n", client_msg);
    //Channels
    int channels = ctx->channels;
    msg = std::to_string(channels);
    write(sock, msg.c_str(), strlen(msg.c_str()));
    recv_size = recv(sock, client_msg, 2000, 0);
    client_msg[recv_size] = '\0';
    printf("Channels\t%s\n", client_msg);
    //duration
    float duration = fctx->duration / AV_TIME_BASE;
    //int frames = fctx->streams[stream_id]->nb_frames;
    //msg = std::to_string(duration);
    //write(sock, msg.c_str(), strlen(msg.c_str()));
    //recv_size = recv(sock, client_msg, 2000, 0);
    //client_msg[recv_size] = '\0';
    printf("Duration: \t%s\n", client_msg);
    
    AVFrame *frame = NULL;
    frame = av_frame_alloc();
    AVPacket *packet;
    packet = av_packet_alloc();
    FILE *rawfile;
    std::stringstream rawfilename;
    rawfilename << "cl" << clientID << ".tmp";
    printf("%s\n", rawfilename.str().c_str());
    rawfile = fopen(rawfilename.str().c_str(), "wb");
    //decode and send data
    printf("Decoding...\n");
    int err = 0;
    int tempsize = 0;
    std::vector<uint8_t> audiobuffer;
    int filesize = 0;
    while((err = av_read_frame(fctx, packet)) != AVERROR_EOF){
        if(err != 0){
            printf("read error\n");
        }
        if(packet->stream_index != stream_id){
            av_packet_unref(packet);
            continue;
        }
        if((err = avcodec_send_packet(ctx, packet)) == 0){
            av_packet_unref(packet);
        }else{
            char errchar[64];
            av_strerror(err, errchar, 64);
            printf("Failed to send packet to encoder; error (%i, %s)\n", err, errchar);
            break;
        }
        while(avcodec_receive_frame(ctx, frame) == 0){
            //handle frame
            //printf("Handling frame\n");
            for(int i = 0; i < frame->nb_samples; i++){
                for(int ch = 0; ch < ctx->channels; ch++){
                    //write to file
                    fwrite(frame->data[ch] + ssize*i, 1, ssize, rawfile);
                    //send
                    //write(sock, "1", 1);
                    //write(sock, (char*)(frame->data[ch] + ssize*i), ssize);
                    //audiobuffer.push
                    tempsize += ssize;
                }
            }
            av_frame_unref(frame);
        }
        //printf("All frames for packet handled\n");
    }

    avcodec_send_packet(ctx, NULL);
    while(avcodec_receive_frame(ctx, frame) == 0){
        for(int i = 0; i < frame->nb_samples; i++){
            for(int ch = 0; ch < ctx->channels; ch++){
                //write to file
                fwrite(frame->data[ch] + ssize*i, 1, ssize, rawfile);
                //send
                tempsize += ssize;
            }
        }
        av_frame_unref(frame);
    }
    printf("finished decoding %i bytes\n", tempsize);

    fclose(rawfile);

    msg = std::to_string(tempsize);
    write(sock, msg.c_str(), strlen(msg.c_str()));
    recv_size = recv(sock, client_msg, 2000, 0);
    client_msg[recv_size] = '\0';

    av_frame_free(&frame);
    avcodec_close(ctx);
    avcodec_free_context(&ctx);
    avformat_close_input(&fctx);

    //send file
    rawfile = fopen(rawfilename.str().c_str(), "rb");
    if(rawfile == NULL){
        printf("File Error\n");
        return 0;
    }
    int count = 0;
    char *buffer = new char[sampleRate*channels*ssize];
    long int offset;
    while(offset < tempsize){
        int count = 0;
        if(offset + sampleRate*channels*4 < tempsize){
            count = fread(buffer, 1, sampleRate*channels*ssize, rawfile);
        }else{
            count = fread(buffer, 1, tempsize - offset, rawfile);
        }
        //rawbuf.load(buffer, count);
        write(sock, buffer, count);
        recv_size = recv(sock, client_msg, 2000, 0);
        client_msg[recv_size] = '\0';
        if(client_msg[0] == '0'){
            printf("Recieved 0\n");
            break;
        }
        offset = ftell(rawfile);
        int timeoffset = offset / (sampleRate * channels * 4);
    }

    fclose(rawfile);    

    printf("Client %i disconnected\n", clientID);
    close(sock);

    return 0;
}

