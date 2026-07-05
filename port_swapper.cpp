#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <vector>
#include <algorithm>
#include <netdb.h>

#define DEBUG_PRINT(msg) std::cerr << msg << std::endl // use it? or not?

std::vector<int> open_ports_list;

struct sockaddr_in server_addr;

// return IPv4 address in a string 
std::string dns_resolving(const std::string& domain) 
{
    std::string answer;
    struct addrinfo hints{}, *result, *ptr;
    char ip[INET_ADDRSTRLEN]; 
    
    hints.ai_family = AF_INET;      
    hints.ai_socktype = SOCK_DGRAM;  
    
    // add timeout!
    int status = getaddrinfo(domain.c_str(), NULL, &hints, &result);
    
    if (status != 0 && result != nullptr) 
        return "";
  
    struct sockaddr_in* ipv4 = (struct sockaddr_in*)result->ai_addr;
    void* address = &(ipv4->sin_addr);
    inet_ntop(result->ai_family, address, ip, sizeof(ip));

    freeaddrinfo(result);
    return std::string(ip);
}

bool ip_address_check(const std::string& scan_ip)
{
    server_addr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, scan_ip.c_str(), &server_addr.sin_addr) != 1)
    {
        // perror("Invalid IP-address");
        return false; 
    }

    return true;
}

int socket_deployment()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // TCP-connection with IPv4

    if (sockfd == -1)
    {
        // perror("Socket creation failed (open fd limits reached)");
        return -1;
    }

    // Non-block socket mode enable
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) == -1)
    {
        // perror("Socket modification failed (nonblock socket mode)");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// port-range struct
struct range
{
    int begin;
    int end;
};

bool task(const std::string& scan_ip, range port_range)
{
    server_addr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, scan_ip.c_str(), &server_addr.sin_addr) != 1)
    {
        // perror("Invalid IP-address");
        return false; 
    }

    std::vector<pollfd> port_tasks;

    // think about all port scaning (some socket max limit)

    int max_open_sockets = sysconf(_SC_OPEN_MAX) - 3; // stdin, stdout, stderr open by default 
    int rounds = (port_range.end - port_range.begin + 1) / max_open_sockets + ((port_range.end - port_range.begin + 1) % max_open_sockets != 0 ? 1 : 0);
    int port_shift = (port_range.end - port_range.begin + 1) / rounds;

    for (int round = 0; round < rounds; ++rounds)
    {
        for (int port = port_range.begin + round * port_shift; port <= std::min(port_range.end, port_range.begin + (round + 1) * port_shift); ++port)
        {
            int client_fd = socket_deployment();
            server_addr.sin_port = htons(port);
            
            connect(client_fd, (const struct sockaddr*)&server_addr, sizeof(server_addr));
            port_tasks.emplace_back(pollfd{client_fd, POLLOUT, 0});
        }
        
        int result = 1;

        while (result > 0)
        {   
            result = poll(port_tasks.data(), port_tasks.size(), 1000);
            
            // std::cout << result << std::endl;

            if (result < 1)
            {
                // if (result == -1)
                //     perror("Socket polling failed");

                // close return errno EBADF when fd == -1
                for (const pollfd& port_task : port_tasks)
                    close(port_task.fd);

                // break;
                return false;
            }
            else
            {
                int cur_result = result;

                for (int task = 0; task < port_tasks.size() && cur_result > 0; ++task)
                {
                    if (port_tasks[task].fd == -1)
                        continue;

                    if (port_tasks[task].revents & POLLOUT)
                    {
                        int err;
                        socklen_t err_len = sizeof(err);
                        getsockopt(port_tasks[task].fd, SOL_SOCKET, SO_ERROR, &err, &err_len);

                        if (err == 0)
                        {
                            // std::lock_guard<std::mutex> lock(mtx); // if multithreading enable
                            open_ports_list.emplace_back(port_range.begin + task);
                        }

                        --cur_result;
                        close(port_tasks[task].fd);
                        port_tasks[task].fd = -1;
                    }
                }
            }
        }
    }

    return true;
}

int main(int argc, char** argv)
{
    std::string scan_ip;
    range port_range;

    if (argc == 1)
    {
        scan_ip = "127.0.0.1";
        port_range = range{1, 1024};
    }
    else if (argc >= 2)
    {
        scan_ip = argv[1];
        
        if (!ip_address_check(scan_ip))
        {
            scan_ip = dns_resolving(scan_ip);
            port_range = range{1, 1024};

            if (scan_ip == "")
            {
                std::cerr << "Invalid IP-address or Domain Name" << std::endl;
                return -1;
            }
        }

        if (argc == 3)
        {
            uint16_t port = std::stoi(argv[2]);

            // check the correctness of port
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
        else if (argc > 3)
            return -1; // not implemented
    }

    std::cout << "Scanning IP-address: " << scan_ip << std::endl;
    std::cout << "Scan range: " << port_range.begin << "-" << port_range.end << std::endl;

    //Single-core mode
    task(scan_ip, port_range);
    
    // Report 
    std::sort(open_ports_list.begin(), open_ports_list.end());

    for (int port : open_ports_list)
        std::cout << "Port " << port << " is open" << std::endl;

    std::cout << "Total: " << open_ports_list.size() << " open, " << port_range.end - open_ports_list.size() << " closed" << std::endl;
    return 0;
}
