/* run using ./server <port> */
#include <bits/stdc++.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <limits.h>
#include "http_server.hh"
#include <sys/stat.h>
#include <fstream>
#include <sstream>

#define THREAD_NUM 256
#define MAX_SIZE 8000
#define BUFFER_SIZE 1024

queue<int> client_queue;
int count_var = 0;

pthread_mutex_t mutex_var;
pthread_cond_t condition_var;

vector<string> split(const string &s, char delim){
  vector<string> elems;
  stringstream ss(s);
  string item;
  while (getline(ss, item, delim)){
    if (!item.empty())
      elems.push_back(item);
  }
  return elems;
}

HTTP_Request::HTTP_Request(string request){
  vector<string> lines = split(request, '\n');
  vector<string> first_line = split(lines[0], ' ');

  this->HTTP_version = "1.0"; // We'll be using 1.0 irrespective of the request

  /*
   TODO : extract the request method and URL from first_line here
  */
  method = first_line[0];
  url = first_line[1];
  if (this->method != "GET"){
    cerr << "Method '" << this->method << "' not supported" << endl;
    exit(1);
  }
}

HTTP_Response *handle_request(string req){
  // cout << req << endl;
  HTTP_Request *request = new HTTP_Request(req);

  HTTP_Response *response = new HTTP_Response();

  string url = string("html_files") + request->url;

  response->HTTP_version = "1.0";

  struct stat sb;
  if (stat(url.c_str(), &sb) == 0) // requested path exists
  {
    response->status_code = "200";
    response->status_text = "OK";
    response->content_type = "text/html";

    string body;

    if (S_ISDIR(sb.st_mode)){
      /*
      In this case, requested path is a directory.
      TODO : find the index.html file in that directory (modify the url
      accordingly)
      */
      url = url + "/index.html";
    }

    /*
    TODO : open the file and read its contents
    */
    ifstream ifs(url);
    string data((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

    /*
    TODO : set the remaining fields of response appropriately
    */
    response->body = data;
    response->content_length = to_string(data.size());
  }

  else{
    response->status_code = "404";

    /*
    TODO : set the remaining fields of response appropriately
    */
    response->status_text = "NOT FOUND";
    response->content_type = "text/html";
    response->body = "<h1>404 File Not Found!</h1>";
    response->content_length = to_string(response->body.size());
  }

  delete request;

  return response;
}

string HTTP_Response::get_string(int newsockfd, HTTP_Response *data){
  /*
  TODO : implement this function
  */
  // string alter_response = "HTTP/" + data->HTTP_version + " " + data->status_code + " " + data->status_text +
  // "\nContent-Type:" + data->content_type + "\nContent-Length: " + data->content_length + "\n\n" + data->body;

  // string response = "HTTP/1.1 200 OK\nContent-Type:text/html\nContent-Length: " + data->content_length + "\n\n" + data->body;

  string headers = "HTTP/" + data->HTTP_version + " " + data->status_code + " " + data->status_text;
  string rem = "\nContent-Type:" + data->content_type + "\nContent-Length: " + data->content_length + "\n\n";
  rem = rem + data->body;

  char info[rem.length()];
  strcpy(info, rem.c_str());

  char _data[headers.length() + 1];
  // strcpy(_data, response.c_str());
  strcpy(_data, headers.c_str());
  int n = write(newsockfd, _data, sizeof(_data));
  if (n <= 0)
    printf("ERROR writing to socket");
  int m = write(newsockfd, info, sizeof(info));
  if (m <= 0)
    printf("ERROR writing to socket");
  close(newsockfd);
  // sleep(20);
  return "";
}

void error(string msg)
{
  // perror(msg);
  cout << msg << endl;
  // exit(1);
}

void server_process(void *arg)
{
  int newsockfd = *(int *)arg;
  char buffer[1024];
  int n;

  /* read message from client */
  while(1){
    bzero(buffer, 1024);
    n = read(newsockfd, buffer, 1024);
    // printf("%s\n",buffer);
    if (n <= 0){
      // printf("ERROR reading from socket: Socket Closed\n");
      break;
    }
    // printf("Here is the message: %s", buffer);
    // printf("Client Connected successfully\n");
    int i;
    string s = "";
    for (i = 0; i < sizeof(buffer); i++){
      s = s + buffer[i];
    }
    if (s[0] == 'G'){
        /* send reply to client */
      HTTP_Response *r = handle_request(s);
    // char data[r->body.length()+1];
    // strcpy(data, r->body.c_str());
      r->get_string(newsockfd, r);

    // n = write(newsockfd, "I got your message", 18);
    // if (n <= 0)
    // printf("ERROR writing to socket");
    // close(newsockfd);
      free(r);
    }
  }
  // pthread_exit(NULL);
}

void *startThread(void *args){
  int newsockfd;
  while(1){
    pthread_mutex_lock(&mutex_var);
    while(count_var == 0){
      pthread_cond_wait(&condition_var, &mutex_var);
    }
    newsockfd = client_queue.front();
    client_queue.pop();
    count_var--;
    pthread_mutex_unlock(&mutex_var);
    server_process(&newsockfd);
    // printf("executed\n");
  }
  return NULL;
}


int main(int argc, char *argv[])
{
  pthread_t th[THREAD_NUM];
  int sockfd, newsockfd, portno;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  int n;
  pthread_mutex_init(&mutex_var, NULL);
  pthread_cond_init(&condition_var, NULL);
  if (argc < 2)
  {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(1);
  }

  /* create socket */

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");

  /* fill in port number to listen on. IP address can be anything (INADDR_ANY)
   */

  bzero((char *)&serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  /* bind socket to this port number on this machine */

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");

  /* Create Threads */
  int i;
  for (i = 0; i < THREAD_NUM; i++){
    if (pthread_create(&th[i], NULL, &startThread, NULL) != 0){
      perror("Failed to Create Thread!");
    }
  }

  /* listen for incoming connection requests */

  listen(sockfd, 5);
  clilen = sizeof(cli_addr);

  while (1)
  {
    /* accept a new request, create a newsockfd */
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd <= 0)
      // error("ERROR on accept");
      continue;

    if (client_queue.size() == MAX_SIZE){
      close(newsockfd);
      continue;
    }
    // Insert the accepted client request in Shared Queue
    pthread_mutex_lock(&mutex_var);
    client_queue.push(newsockfd);
    count_var++;
    pthread_mutex_unlock(&mutex_var);
    pthread_cond_signal(&condition_var);
  }
  pthread_mutex_destroy(&mutex_var);
  pthread_cond_destroy(&condition_var);
  return 0;
}
