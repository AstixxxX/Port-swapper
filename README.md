# Port Swapper

**Primary port scanner**

> ⚠️ **Disclaimer**  
> This tool is **not designed for offensive tasks**. It is intended **only for educational and system administration purposes**.

---

## 📖 About

**Port Swapper** is the "younger brother" of **Nmap** — a lightweight **Proof of Concept (PoC)** that demonstrates how port scanning works at the system level.

The project consists of two main components:
- `port_swapper` – scans TCP ports
- `dns-resolver` – converts a domain name to an IP address (based on `nslookup`)

---

## 🛠 Manual Build

```bash
# Compile the scanner
g++ port_swapper.cpp -o port_swapper
./port_swapper <IP> <PORT>

# Use dns-resolver to use domain names instead of IP-address
chmod +x dns-resolver
./port_swapper `dns-resolver <domain name>` <PORT>