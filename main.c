#include<sys/socket.h>          // socket
#include<sys/stat.h>                // file station
#include<string.h>                    // deal with string   
//#include<netdb.h>                   // get ip address
#include<pthread.h>              //  create thread
#include<stdio.h>                    //  print
#include<arpa/inet.h>

#define ISDEBUG 1
#define BUFFER_SIZE 1024

int AcceptClient();
void* DealWithClient(void* pclient_socket);
int getlines(int client_socket,char* buf,int size);
char* LineHeap(int client_socket,int* plength);
char** ParseReportFirstLine(char* first_line,int* nums,int len);
void bad_request(int client_socket);
void inner_error(int client_socket);
void get_response(int client_socket,char* url);
void not_found(int client_socket);
void unimplemented(int client_socket);
void post_response(int client_socket,char* content_type,char* boundary,int content_length);

int main()
{
    AcceptClient(11451);
    return 0;
}

int AcceptClient(int port)
{
    // create socket
    int server_socket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(server_socket==-1)
    {
        if(ISDEBUG)printf("socket error\n");
        return -2;
    }

    // bind
    struct sockaddr_in server_address = { 0 };
    server_address.sin_family=AF_INET;
    server_address.sin_port=htons(port);
    server_address.sin_addr.s_addr=htonl(INADDR_ANY);               // 0.0.0.0
    if(bind(server_socket,(struct sockaddr*)&server_address,sizeof(server_address))!=0)
    {
        if(ISDEBUG)printf("bind error\n");
        close(server_socket);
        return -1;
    } 

    // listen
    if(listen(server_socket, 5)==-1)
    {
        if(ISDEBUG)printf("listen error\n");
        close(server_socket);
        return -1;
    }

    while(1)
    {
        // accept
        struct sockaddr_in client_address={0};
        int client_socket=0;
        int client_length=sizeof(client_address);
        client_socket=accept(server_socket,(struct sockaddr*)&client_address,&client_length);

        struct in_addr ipaddr;
        memcpy(&ipaddr,&client_address.sin_addr.s_addr,4);
        printf("get client,address is:%s\n",inet_ntoa(ipaddr));

        pthread_t thread;
        void* param=malloc(sizeof(int));
        *(int*)param=client_socket;
        if(pthread_create(&thread,NULL,DealWithClient,param)!=0)
        {
            if(ISDEBUG)printf("thread error\n");
            close(client_socket);
        }
    }
    close(server_socket);
}

void* DealWithClient(void* pclient_socket)
{
    int client_socket=*(int*)pclient_socket;
    int length=0;
    int nums=0;
    char* report_line=LineHeap(client_socket,&length);
    if(report_line==NULL)return NULL;
    if(ISDEBUG)printf("FirstLine:%s\n",report_line);
    char** parse_result=ParseReportFirstLine(report_line,&nums,length);
    if(nums==0||parse_result==NULL)
    {
        bad_request(client_socket);
    }
    else if(strncasecmp(parse_result[0],"GET",strlen("GET"))==0)
    {
        int temp=0;
        while(1)
        {
            char* tempChar=LineHeap(client_socket,&temp);
            if(tempChar!=NULL)
            {
                if(ISDEBUG)
                printf("%s\n",tempChar);
                free(tempChar);
            }
            if(temp<=0)break;
        }
        get_response(client_socket,parse_result[1]);
    }
    else if(strncasecmp(parse_result[0],"POST",strlen("POST"))==0)
    {
        int temp=0;
        char* content_type=NULL;
        char* boundary=NULL;
        int content_length;
        //Content-Type, Content-Length
        while(1)
        {
            char* tempChar=LineHeap(client_socket,&temp);
            if(tempChar!=NULL)
            {
                if(ISDEBUG)
                printf("%s\n",tempChar);
                int tempNum=0;
                char** tempParse=ParseReportFirstLine(tempChar,&tempNum,temp);
                if(strncasecmp(tempParse[0],"Content-Type",strlen("Content-Type"))==0)
                {
                    content_type=(char*)malloc(sizeof(char)*strlen(tempParse[1])+1);
                    memset(content_type,0,sizeof(char)*strlen(tempParse[1])+1);
                    strcpy(content_type,tempParse[1]);
                    if(tempNum>=3)
                    {
                        char* t_boundary=strchr(tempParse[2],'=')+1;
                        boundary=(char*)malloc(sizeof(char)*strlen(t_boundary)+1);
                        memset(boundary,0,sizeof(char)*strlen(t_boundary)+1);
                        strcpy(boundary,t_boundary);
                    }
                }
                else if(strncasecmp(tempParse[0],"Content-Length",strlen("Content-Length"))==0)
                {
                    content_length = atoi(tempParse[1]);
                }
                free(tempChar);
                free(tempParse);
            }
            if(temp<=0)break;
        }
        // finnally it doesn't have \r\n
        if(ISDEBUG)printf("Content-Type:%s\nContent-Length:%d\n",content_type,content_length);
        post_response(client_socket,content_type,boundary,content_length);
        if(content_type!=NULL)free(content_type);
        if(boundary!=NULL)free(boundary);
    }
    else
    {
        int temp=0;
        while(1)
        {
            char* tempChar=LineHeap(client_socket,&temp);
            if(tempChar!=NULL)
            {
                if(ISDEBUG)
                printf("%s\n",tempChar);
                free(tempChar);
            }
            if(temp<=0)break;
        }
        unimplemented(client_socket);
    }
    free(report_line);
    free(parse_result);
    return NULL;
}

char** ParseReportFirstLine(char* first_line,int* nums,int len)
{
    char** result=NULL;
    int num=0;
    char* search=first_line;
    while(1)
    {
        char* space=strchr(search,' ');
        if(space==NULL)break;
        while(*space==' ')
        {
            *space='\0';
            space++;
        }
        char** temp=(char**)malloc(sizeof(char*) * (num+1));
        if(result!=NULL)
        {
            memcpy(temp,result,sizeof(char*)*num);
            free(result);
        }
        result=temp;
        result[num++]=search;
        search=space;
    }
    char** temp=(char**)malloc(sizeof(char*) * (num+1));
    if(result!=NULL)
    {
        memcpy(temp,result,sizeof(char*)*num);
        free(result);
    }
    result=temp;
    result[num++]=search;
    *nums=num;
    return result;
}

char* LineHeap(int client_socket,int* plength)
{
    int length=256;
    char* LineBuffer=NULL;
    int times=0;
    int retVal=0;
    while (1)
    {
        char* tempBuffer=(char*)malloc(sizeof(char)*length);
        retVal=getlines(client_socket,tempBuffer,length);
        if(retVal==-1)
        {
            if(LineBuffer!=NULL)free(LineBuffer);
            free(tempBuffer);
            return NULL;
        }
        if(retVal==-2)
        {
            if(LineBuffer!=NULL)free(LineBuffer);
            free(tempBuffer);
            return NULL;
        }
        if(retVal==-3)
        {
            if(LineBuffer!=NULL)
            {
                char* ano=(char*)malloc(sizeof(char)*length*(times+1));
                strcpy(ano,LineBuffer);
                strcat(ano,tempBuffer);
                free(LineBuffer);
                free(tempBuffer);
                LineBuffer=ano;
            }
            else LineBuffer=tempBuffer;
            times++;
            continue;
        }
        if(LineBuffer!=NULL)
        {
            char* ano=(char*)malloc(sizeof(char)*length*(times+1));
            strcpy(ano,LineBuffer);
            strcpy(ano,tempBuffer);
            free(LineBuffer);
            free(tempBuffer);
            LineBuffer=ano;
        }
        else LineBuffer=tempBuffer;
        retVal=retVal+length*times;
        break;
    } 
    if(plength!=NULL)
    *plength=retVal;
    return LineBuffer;
}

int getlines(int client_socket,char* buf,int size)
{
    int count=0;
    char ch=0;
    while(count<size-1)
    {
        int retVal=recv(client_socket,&ch,1,0);
        if(retVal==0)
        {
            if(ISDEBUG)printf("connect close\n");
            return -1;      // connect close
        }
        else if(retVal==-1)
        {
            if(ISDEBUG)printf("recv error\n");
            return -2;
        }
        else if(retVal==1)
        {
            if(ch=='\r')continue;
            else if(ch=='\n')break;
            buf[count++]=ch;
        }
    }
    if(count>=size-1)
    {
        buf[size-1]='\0';
        return -3;
    }
    buf[count]='\0';
    return count;
}

void bad_request(int client_socket)
{
    char header[256]={0};
    struct stat statBuf;
    stat("FilePath/bad_request.html",&statBuf);
    int size=statBuf.st_size;
    sprintf(header,"HTTP/1.0 400 BAD REQUEST\r\nServer: HXD Server\r\nContent-Type: text/html\r\nConnection: Close\r\nContent-Length: %d\r\n\r\n",size);
    if(ISDEBUG)printf("%s",header);
    if(send(client_socket,header,strlen(header),0)<=0)
    {
        printf("connect close\n");
        return;
    }
    FILE* file=fopen("FilePath/bad_request.html","r");
    if (file == NULL)
    {
        perror("Failed to open file");
        inner_error(client_socket);
    }
    fseek(file,0,SEEK_SET);
    char buffer[BUFFER_SIZE];
    int bytes_read;
    memset(buffer, 0, BUFFER_SIZE);
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0)
    {
      // 发送HTTP响应报头
        int retVal=send(client_socket, buffer, bytes_read, 0);
        if (retVal<=0)
        {
            perror("Failed to send data");
            break;
        }
        memset(buffer, 0, BUFFER_SIZE);
    }
    fclose(file);
}

void inner_error(int client_socket)
{
    char header[256]={0};
    sprintf(header,"HTTP/1.0 404 Not Found\r\nServer: HXD Server\r\nContent-Type: text/html\r\nConnection: Close\r\n\r\n");
    if(ISDEBUG)printf("%s",header);
    if(send(client_socket,header,strlen(header),0)==-1)
    {
        printf("connect close\n");
        return;
    }
    send(client_socket, "inner_error", strlen("inner_error"), 0); 
}

void get_response(int client_socket,char* url)
{
    char header[256]={0};
    struct stat statBuf;
    char filename[128]={0};
    sprintf(filename,".%s",url);
    if(stat(filename,&statBuf)==-1||!S_ISREG(statBuf.st_mode))
    {
        not_found(client_socket);
        return;
    }
    int size=statBuf.st_size;
    sprintf(header,"HTTP/1.0 200 OK\r\nServer: HXD Server\r\nContent-Type: text/html\r\nConnection: Close\r\nContent-Length: %d\r\n\r\n",size);
    if(ISDEBUG)printf("%s",header);
    FILE* file=fopen(filename,"rb+");
    if (file == NULL) {
      perror("Failed to open file");
      inner_error(client_socket);
      return;
    }
    if(send(client_socket,header,strlen(header),0)==-1)
    {
        printf("connect close\n");
        return;
    }
    fseek(file,0,SEEK_SET);
    char buffer[BUFFER_SIZE];
    int bytes_read;
    memset(buffer, 0, BUFFER_SIZE);
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
      // 发送HTTP响应报头
      if (send(client_socket, buffer, bytes_read, 0) == -1) {
        perror("Failed to send data");
        break;
      }
      memset(buffer, 0, BUFFER_SIZE);
    }
    fclose(file);
}

void not_found(int client_socket)
{
    char header[256]={0};
    struct stat statBuf;
    stat("FilePath/not_found.html",&statBuf);
    int size=statBuf.st_size;
    sprintf(header,"HTTP/1.0 404 Not Found\r\nServer: HXD Server\r\nContent-Type: text/html\r\nConnection: Close\r\nContent-Length: %d\r\n\r\n",size);
    FILE* file=fopen("FilePath/not_found.html","rb+");
    if (file == NULL) {
      perror("Failed to open file");
      inner_error(client_socket);
    }
    if(ISDEBUG)printf("%s",header);
    if(send(client_socket,header,strlen(header),0)<=0)
    {
        printf("connect close\n");
        return;
    }
    fseek(file,0,SEEK_SET);
    char buffer[BUFFER_SIZE];
    int bytes_read;
    memset(buffer, 0, BUFFER_SIZE);
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
      // 发送HTTP响应报头
      if (send(client_socket, buffer, bytes_read, 0) <= 0) {
        perror("Failed to send data");
        break;
      }
      memset(buffer, 0, BUFFER_SIZE);
    }
    fclose(file);
}

void unimplemented(int client_socket)
{
    char header[256]={0};
    struct stat statBuf;
    stat("FilePath/unimplemented.html",&statBuf);
    int size=statBuf.st_size;
    sprintf(header,"HTTP/1.0 501 Method Not Implemented\r\nServer: HXD Server\r\nContent-Type: text/html\r\nConnection: Close\r\nContent-Length: %d\r\n\r\n",size);
    if(ISDEBUG)printf("%s",header);
    if(send(client_socket,header,strlen(header),0)<=0)
    {
        printf("connect close\n");
        return;
    }
    FILE* file=fopen("FilePath/unimplemented.html","rb+");
    if (file == NULL) {
      perror("Failed to open file");
      inner_error(client_socket);
      return;
    }
    fseek(file,0,SEEK_SET);
    char buffer[BUFFER_SIZE];
    int bytes_read;
    memset(buffer, 0, BUFFER_SIZE);
    while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
      if (send(client_socket, buffer, bytes_read, 0) == -1) {
        perror("Failed to send data");
        break;
      }
      memset(buffer, 0, BUFFER_SIZE);
    }
    fclose(file);
}

void post_response(int client_socket,char* content_type,char* boundary,int content_length)
{
    if(content_type==NULL||content_length<=0)bad_request(client_socket);
    // format upload
    if(strncasecmp(content_type,"application/x-www-form-urlencoded",strlen("application/x-www-form-urlencoded"))==0)
    {
        char* recvBuf=(char*)malloc(sizeof(char)*content_length+1);
        recv(client_socket,recvBuf,sizeof(char)*content_length+1,0);
        printf("post message:%s\n",recvBuf);
        not_found(client_socket);
    }
    // multiple upload
    else if(strncasecmp(content_type,"multipart/form-data",strlen("multipart/form-data"))==0)
    {
        not_found(client_socket);
    }
}
