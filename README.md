# Port Swapper - primary port scanner
## This tool is not design for offencive tasks!!! Only for education and administration purposes
## Is the younger brother of nmap which show PoC of port scanning
## dns-resolver is the basic script which convert domain name to ip address. Based on nslookup

### Build manually: 
### In project dir : g++ port_swapper.cpp -o port_swapper
### Usage: ./port_swapper <IP> | ./port_swapper <IP> <PORT>
###         chmod +x dns-resolver
###         ./port_swapper `./dns-resolver <domain name>`          