#include <unistd.h>
#include <cerrno>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listen_fd, (const sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 10);

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    while (1) {
        struct epoll_event events[1024];
        int n = epoll_wait(epoll_fd, events, 1024, -1);

        for(int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                while (1) {
                    int client_fd = accept4(fd, NULL, NULL, SOCK_NONBLOCK);
                    if (client_fd < 0) {
                        if (errno == EAGAIN) break;
                        if (errno == EINTR) continue;
                        perror("accept");
                        break;
                    }

                    struct epoll_event cev;
                    cev.events = EPOLLIN | EPOLLET;
                    cev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &cev);
                }
            }

            else {
                while (1) {

                    char buf[1024];
                    ssize_t n_read = recv(fd, buf, sizeof(buf), 0);
                    if (n_read < 0) {
                        if (errno == EINTR) continue;
                        if (errno == EAGAIN) break;
                        perror("recv");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                        break;
                    }

                    else if (n_read == 0) {
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                        break;
                    }

                    else {
                        printf("[Server] fd=%d recv %ld bytes\n", fd, n_read);
                        ssize_t total = 0;
                        while(total < n_read) {
                            ssize_t n_send = send(fd, buf + total, n_read - total, MSG_NOSIGNAL);
                            if (n_send < 0) {
                                if (errno == EINTR) continue;
                                if (errno == EAGAIN) break;
                                perror("send");
                                break;
                            }
                            total += n_send;
                        }
                    }

                }

            }
        }

    }

    close(listen_fd);
    close(epoll_fd);
}