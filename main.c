#include <stdio.h>
//Thread
#include <pthread.h>

// include LINUX API
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

struct pool_thread_info {
    int job_ev;           
    int done_ev;
    int port;          // дескриптор подключенного клиента
    pthread_mutex_t lock; // мьютекс для определения статуса потока и синхронизации данных
};

const unsigned int workers_count = 4;
//const int magic = 42;
//захват потока
int pick_thread(struct pool_thread_info *info) {
    // в зависимости от занятости потока возвращаем true (1) или false (0)
    // код функции рассмотрим позже
    int ret = pthread_mutex_trylock(&(info->lock));
    if (ret == 0) {
        // нам удалось выбрать поток
        return 1;
    } else {
        // рассматриваем случай EBUSY
        return 0;
    }
}

void wake_up_thread(struct pool_thread_info *info, int port) {
    info->port = port;
    (void)eventfd_write(info->job_ev, 1);
    (void)pthread_mutex_unlock(&(info->lock));
}

void *control_thread(void *arg) {
    // структуры данных для потоков в пуле
    struct pool_thread_info threads[workers_count];
    int srv_sock = -1;

    // подразумеваем, что управляющий поток уже проделал необходимую работу для старта пула:
    // 1. Дескрипторы событий созданы и разложены по структурам потоков
    // 2. Потоки пула созданы, каждому потоку передана структура с его дескрипторами и прочими данными
    // 3. Серверный сокет был успешно забинжен

    while (magic) {
        int client_sock = accept4(srv_sock, NULL, NULL, SOCK_CLOEXEC);
        unsigned int iter = 0;

        for (; iter < workers_count; iter++) {
            if (pick_thread(&threads[iter])) {
                wake_thread(&threads[iter], client_sock);
            }
        }
    }
    (void)pthread_exit(NULL);
}


int net_dial(const char *ip, int port)
{
    /*
    return int sockfd - open port
    return 0 - close post :(
    return -1 - error or close post :(
    */


    /*
    #include <sys/socket.h>
    int socket(int domain, int type, int protocol);
    Возвращает дескриптор файла (сокета) в случае успеха,
    –1 — в случае ошибки
    */
    // open socket 
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd < 0){
        perror("error Socket fd");
        //exit(-1);
        return(-1);
    }

    //fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK); 

    /*
        Формат представления адресов Интернета определен в заголовочном файле
    <netinet/in.h>. Адреса сокетов из домена IPv4 (AF_INET) представлены структу-
    рой sockaddr_in:
    struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port; 
    struct in_addr sin_addr; 
    };
    */
    //configure IP:port server for conneection on TCP  list ports[1..1024]
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    /*
    uint16_t htons(uint16_t hostint16);
Возвращает 16-разрядное целое с сетевым порядком байтов
    */
    addr.sin_port = htons(port);

    /*
    int inet_pton(int domain, const char *restrict str, void *restrict addr);
Возвращает 1 в случае успеха, 0 — при неверном формате,
–1 — в случае ошибк

    c помощью этой функции:
    - преобразовываем в двоичный формат  в адрес сервера
    - с указываем в какую ячейку памяти сохранить двоичное представление адреса
    */
    if (inet_pton(AF_INET,ip, &addr.sin_addr) < 1) {
        perror("not support format");
        return(-1);
    }    
    void* addr_ptr;
    addr_ptr = (struct sockaddr*)&addr;
    //connect 
    /*
        include <sys/socket.h>
        int connect(int sockfd, const struct sockaddr *addr, socklen_t len);
        Возвращает 0 в случае успеха, –1 — в случае ошибки
    */
    if (connect(sockfd,addr_ptr,sizeof(addr) ) == 0){
                // shutdown(sockfd,SHUT_RDWR);
                return(sockfd);

    }else{
        // close(sockfd);
        shutdown(sockfd,SHUT_RDWR);
        return -1;
    }

}

int main(void){
    
    const char *scanme = "45.33.32.156";// scanme.nmap.org = 45.33.32.1561
    for (int i = 1; i <=1024; i++){
        int conn = net_dial(scanme,i);
        if (conn == -1){
            continue;
        }
        // close(conn);
        shutdown(conn,SHUT_RDWR);
        printf("%d open\n",i);
    }
    return 0;
}