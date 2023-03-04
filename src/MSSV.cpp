#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fstream>

using namespace std;
int main(int argc, char* argv[]){
    // pair url 
    string url = argv[1];
    string server, path;
    if (url.find("http://") != -1){
        url = url.substr(7);
    }
    size_t index_flash = url.find("/");
    if (index_flash != -1) {
        server = url.substr(0,index_flash);
        path = url.substr(index_flash);
    }else {
        server = url;
        path = "/";
    }

    // create socket
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd == -1) {
        cerr << "Failed to create socket";
        return 1;
    }
    
    // find server
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo(server.c_str(), "http", &hints, &servinfo);

    // connect server
    int res = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (res == -1) {
        cerr << "Failed to connect to server." << endl;
        return 1;
    }

    // send request http1.1
    string request = "GET " + path +" HTTP/1.1\r\nHost: " + server + "\r\n\r\n";
    send(sockfd, request.c_str(), request.length(), 0);
    

    char buffer[1024];
    size_t bytesReceived;

    string response;
    string data = "";
    
    bool contentLength = false;
    bool tranferEncodingChunked = false;
    
    size_t lengthData; 
    size_t lengthChunk = -1;
    string chunked;


    while ((bytesReceived=recv(sockfd,buffer,1024,0)) && bytesReceived !=-1){
        response += string(buffer,bytesReceived);
        if (contentLength){
            data += string(buffer,bytesReceived);
            if (data.length() == lengthData) break;
        }else if (tranferEncodingChunked){
            chunked += string(buffer,bytesReceived);
            get_enough_data: 
            if (lengthChunk == -1){
                if (chunked.find("\r\n")!=-1){
                    size_t i = 0;
                    while (chunked[i] != '\r') i++;
                    lengthChunk = stoul(chunked.substr(0,i),nullptr,16);
                    chunked = chunked.substr(i+2);
                }
                else continue;
            }
            if (lengthChunk == 0) break;
            if (lengthChunk <= chunked.length()){
                data += chunked.substr(0,lengthChunk+2);
                chunked = chunked.substr(lengthChunk+2); 
                lengthChunk = -1;
                if (chunked.find("0\r\n")!=-1){
                    goto get_enough_data;
                }
            } 
        }else{
            if (response.find("Content-Length: ") != -1){
                size_t indexLent = response.find("Content-Length: ") + 16; 
                size_t i = indexLent;
                while (response[i] != '\r') i++;
                lengthData = stoi(response.substr(indexLent,i-indexLent));
                data = response.substr(response.find("\r\n\r\n")+4);
                if (data.length() == contentLength) break;
                contentLength = true;
            }
            if (response.find("Transfer-Encoding: chunked")!= -1){
                chunked = response.substr(response.find("\r\n\r\n")+4);
                size_t i = 0;
                while (chunked[i] != '\r') i++;
                lengthChunk = stoul(chunked.substr(0,i),nullptr,16);
                chunked = chunked.substr(i+2);
                tranferEncodingChunked = true;
            }
        }
    }
    close(sockfd);



    ofstream file;
    if (path == "/"){
        file.open(server+"_index.html");
    } else {
        file.open(argv[2]);
    }
    file << data;
    file.close();

    return 0;
}