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
        perror("Socket creation failed");
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

    long finished_time = get_current_time() + 3; // Performance border. 3 seconds of total work for socket proccessing 

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

int port_is_open(const std::string& scan_ip, int port)
{
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (client_fd == -1)
    {
        perror("Socket creation failed");
        return -1;
    }

    if (fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        perror("Socket setup failed");
        close(client_fd);
        return -1;
    } 

    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // One check in the first time maybe enouth???
    if (inet_pton(AF_INET, scan_ip.c_str(), &server_addr.sin_addr) != 1)
    {
        perror("Invalid IP-address");
        close(client_fd);
        return -1;
    }

    // We must check errno value to tell, why connect() failed
    connect(client_fd, (const struct sockaddr*)&server_addr, sizeof(server_addr)); // mb check stat?

    if (errno != EINPROGRESS)
    {
        perror("Server connection failed");
        return -1;
    }

    struct pollfd polling;
    polling.fd = client_fd;
    polling.events = POLLOUT;

    int fd_cnt = poll(&polling, 1, 300); // 300ms waiting of one fd

    if (fd_cnt == -1)
    {
        perror("Socket polling failed");
        close(polling.fd);
        return -1;
    }
    else if (fd_cnt == 0)
    {
        close(polling.fd);
        return 0; 
    }
    
    if (polling.revents & POLLOUT)
    {
        int err;
        socklen_t err_len = sizeof(err);
        getsockopt(polling.fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
        close(polling.fd);
        return err == 0 ? 1 : 0;
    }

    close(client_fd);
    return 0;
}

void scan_task(const std::string& scan_ip, const int port_begin, const int port_end)
{
    for (int port = port_begin; port <= port_end; ++port)
    {
        if (port_is_open(scan_ip, port))
        {
            std::lock_guard<std::mutex> lock(mtx);
            open_ports_list.push_back(port);
        }
    }

    ++task_finished_cnt;
    std::this_thread::sleep_for(std::chrono::seconds(3));
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
            std::cout << "Port " << port << " is " << (port_is_open(scan_ip, port) == 1 ? "open" : "closed")  << std::endl;
            return 0;
        }
    }

    std::cout << "Scanning IP-address: " << scan_ip << std::endl;

    // int prev_stat = 0;
    // std::cout << "Total ports processed:   " << prev_stat << '%';
    // fflush(stdout);

    // Some stat utilization
    // for (int port = 1; port <= max_port; ++port)
    // {
    //     if (port_is_open(scan_ip, port) == 1)
    //         open_ports_list.push_back(port);
    //     int stat = int(float(port) / float(max_port) * 100);
    //     if (stat != prev_stat)
    //     {
    //         if (stat < 10)
    //             std::cout << "\b\b" << stat << '%';
    //         else if (stat < 100)
    //             std::cout << "\b\b\b" << stat << '%';
    //         else
    //             std::cout << "\b\b\b\b" << stat << '%' << std::endl;
    //         prev_stat = stat;
    //         fflush(stdout);
    //     }
    // }

    //Single-core mode
    // range port_range{1, 1024};
    // task(scan_ip, port_range);

    // Multi-core mode
    int threads_cnt = std::thread::hardware_concurrency();
    std::vector<std::thread> thread_pool;
    range port_range{1, 1024};

    int thread_port_begin = 1;
    int thread_port_end = port_range.end / threads_cnt;

    for (int i = 1; i <= threads_cnt; ++i)
    {
        thread_pool.emplace_back(scan_task, scan_ip, thread_port_begin, thread_port_end);
        // std::cout << thread_port_begin << ' ' << thread_port_end << std::endl;

        thread_port_begin = thread_port_end + 1;
        thread_port_end += port_range.end / threads_cnt;

        if (i + 1 == threads_cnt)
            thread_port_end += port_range.end % threads_cnt;
    }

    for (std::thread& thread : thread_pool)
        thread.join();
    
    // Report 
    std::sort(open_ports_list.begin(), open_ports_list.end());

    for (int port : open_ports_list)
        std::cout << "Port " << port << " is open" << std::endl;

    std::cout << "Total: " << open_ports_list.size() << " open, " << port_range.end - open_ports_list.size() << " closed" << std::endl;
    return 0;
}