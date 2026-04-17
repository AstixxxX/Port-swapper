#include <iostream>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <chrono>
std::mutex mtx;
std::vector<int> open_ports_list;
std::atomic<int> task_finished_cnt;

int socket_deployment()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1)
    {
        perror("Socket creation failed - open fd limits reached");
        return -1;
    }

    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        perror("Socket modification failed");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

struct range
{
    int begin;
    int end;
};

// get current time from unix epoch in seconds
long get_current_time()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void task(const std::string& scan_ip, range port_range)
{
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;

    if (inet_pton(AF_INET, scan_ip.c_str(), &server_addr.sin_addr) != 1)
    {
        perror("Invalid IP-address");
        return; // add some signalling
    }

    std::vector<pollfd> port_tasks;

    for (int port = port_range.begin; port <= port_range.end; ++port)
    {
        int client_fd = socket_deployment();
        server_addr.sin_port = htons(port);
        connect(client_fd, (const struct sockaddr*)&server_addr, sizeof(server_addr));
        port_tasks.emplace_back(pollfd{client_fd, POLLOUT, 0});
    }

    long finished_time = get_current_time() + 2; // Performance border. 2 seconds of total work for socket proccessing 

    while (finished_time > get_current_time())
    {
        int result = poll(port_tasks.data(), port_tasks.size(), 1000);

        if (result < 1)
        {
            if (result == -1)
                perror("Socket polling failed");

            for (const pollfd& port_task : port_tasks)
                close(port_task.fd);

            break;
        }
        else
        {
            for (int task = 0; task < port_tasks.size() && result > 0; ++task)
            {
                if (port_tasks[task].revents & POLLOUT)
                {
                    int err;
                    socklen_t err_len = sizeof(err);
                    getsockopt(port_tasks[task].fd, SOL_SOCKET, SO_ERROR, &err, &err_len);

                    if (err == 0)
                    {
                        // std::lock_guard<std::mutex> lock(mtx); // if multithreading enable
                        open_ports_list.push_back(port_range.begin + task);
                    }

                    --result;
                    close(port_tasks[task].fd);
                }
            }
        }
    }
}

int main(int argc, char** argv)
{
    std::string scan_ip;
    int port;
    // problems with dns-notes!
    if (argc == 1)
    {
        scan_ip = "127.0.0.1";
    }
    else if (argc >= 2)
    {
        scan_ip = argv[1];
        //inet_network for check the IP-address input

        if (argc == 3)
        {
            port = atoi(argv[2]); // too bad :(

            //check the correctness of port
            if (port < 1 || port > 65535)
            {
                std::cerr << "Invalid port number" << std::endl;
                return -1;
            }

            std::cout << "Scanning IP-address: " << scan_ip << std::endl;
            task(scan_ip, {port, port});
            std::cout << "Port " << port << " is " << (!open_ports_list.empty() ? "open" : "closed")  << std::endl;
            return 0;
        }
    }

    std::cout << "Scanning IP-address: " << scan_ip << std::endl;

    //Single-core mode
    range port_range{1, 512};
    task(scan_ip, port_range);

    // Divide on two groups for awoid limit of open file descriptors for one process
    port_range = {513, 1024};
    task(scan_ip, port_range);

    // Multi-core mode
    // int threads_cnt = std::thread::hardware_concurrency();
    // std::vector<std::thread> thread_pool;
    // range port_range{1, 1024};

    // int thread_port_begin = 1;
    // int thread_port_end = port_range.end / threads_cnt;

    // for (int i = 1; i <= threads_cnt; ++i)
    // {
    //     thread_pool.emplace_back(scan_task, scan_ip, thread_port_begin, thread_port_end);
    //     // std::cout << thread_port_begin << ' ' << thread_port_end << std::endl;

    //     thread_port_begin = thread_port_end + 1;
    //     thread_port_end += port_range.end / threads_cnt;

    //     if (i + 1 == threads_cnt)
    //         thread_port_end += port_range.end % threads_cnt;
    // }

    // for (std::thread& thread : thread_pool)
    //     thread.join();
    
    // Report 
    std::sort(open_ports_list.begin(), open_ports_list.end());

    for (int port : open_ports_list)
        std::cout << "Port " << port << " is open" << std::endl;

    std::cout << "Total: " << open_ports_list.size() << " open, " << port_range.end - open_ports_list.size() << " closed" << std::endl;
    return 0;
}