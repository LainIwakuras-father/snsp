#include <iostream>          // для вывода в консоль
#include <string>            // для работы со строками
#include <cstring> 
//THREAD 
#include <condition_variable>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
// include LINUX API
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <poll.h>

using namespace std;

class ThreadPool{
    public:
        ThreadPool(size_t num_threads = thread::hardware_concurrency())
        {
            for (size_t i = 0; i < num_threads; i++){
                threads_.emplace_back([this]{
                    while(true){
                        function<void()> task;
                        {
                            unique_lock<mutex> lock(queue_mutex_);
                            cv_.wait(lock,[this]{
                                return !tasks_.empty() || stop_;
                            });

                            if (stop_ && tasks_.empty()){
                                return;
                            }
                            task = move(tasks_.front());
                            tasks_.pop();
                        }
                        task();
                    }
                });
                    
            }   
        }
        ~ThreadPool()
        {
            {
                unique_lock<mutex> lock(queue_mutex_);
                stop_ = true;
            }
            cv_.notify_all();

            for (auto& thread : threads_){
                thread.join();
            }
        }

        void enqueue(function<void()> task)
        {
            {
                unique_lock<mutex> lock(queue_mutex_);
                tasks_.emplace(move(task));
            }
            cv_.notify_one();
        }
    private:
        vector<thread> threads_;
        queue<function<void()>> tasks_;
        mutex queue_mutex_;
        condition_variable cv_;
        bool stop_ = false;

};

int net_dial(const string& ip, int port)
{ 
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd < 0){
        cerr << "error Socket fd:" << strerror(errno) << endl;
        return(-1);
    }

    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK); 

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET,ip.c_str(), &addr.sin_addr) < 1) {
        cerr << "not support format:" << strerror(errno) << endl;
        return(-1);
    }    
    
    if (connect(sockfd,reinterpret_cast<struct sockaddr*>(&addr),sizeof(addr) ) == 0)
        return(sockfd);
    
    if (errno != EINPROGRESS) {
        close(sockfd);
        return -1;
    }

    
    struct pollfd fds;
    fds.fd = sockfd;
    fds.events = POLLOUT;
    
    // ждём до 1 секунды
    int poll_ret = poll( &fds, 1, 1000 );
    
    if ( poll_ret == -1 ){
        cerr << "error polling fd:" << strerror(errno) << endl;
        close(sockfd);
        return(-1);
    }
    else if ( poll_ret == 0 ){
        close(sockfd);
        return(-1);
    }
    else
    {
        if ( fds.revents & POLLOUT ){
            // fds.revents = 0;
            int so_error;
            socklen_t len = sizeof(so_error);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) == 0 && so_error == 0) {
                return(sockfd);
            }else{
                return(-1);
            }
            
        }
    }
}
// Мьютекс для синхронизации вывода
mutex cout_mutex;

void check(const std::string& ip, int port){
    int conn = net_dial(ip,port);
    if (conn > -1){
        lock_guard<mutex> lock(cout_mutex);
        std::cout << "Port " << port << " Is open\n";
        close(conn);
    }
}

int main(int argc, char** argv){

    if (argc == 2){
        const string scanme = argv[1];
        // scanme.nmap.org = 45.33.32.156
        ThreadPool pool(10);
        for (int i = 1; i<=1024;i++){
            pool.enqueue([scanme,i]{
                    check(scanme,i);
                }
            );
        }
        //TODO: реализовать список самых часто используемых портов 
        //TODO: лучше сделать функцию wait()
        this_thread::sleep_for(chrono::seconds(100));
    }   
    else{
         cerr << "Usage: snsp 127.0.0.1" << endl;

    }  
    return 0;
}