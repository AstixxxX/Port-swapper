# Port Swapper - *Primary port scanner*

> ⚠️ **Disclaimer**  
> This tool is **not designed for offensive tasks**. It is intended **only for educational and system administration purposes**.

---

## 📖 About

**Port Swapper** is the "younger brother" of **Nmap** — a lightweight **Proof of Concept** that demonstrates how port scanning works at total.


---

## 🛠 Usage

```bash
# Compile the scanner
g++ port_swapper.cpp -o port_swapper

# Run the scanner
./port_swapper <IP|DNS-name> <PORT>
