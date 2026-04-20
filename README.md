# Internship-realtime-SPI-SoC-FPGA
## Timeline:
### 1. Get familiar with the Zynq UltraScale+ using Xilinx Vivado and PYNQ with support from "The FPGA Programming Handbook" by Frank Bruno and Guy Eschemann
### 2. Pynq introduction
### 3. AXI stream 
### 4. Communication Zynq <-> Host PC
Setup FPGA communication to PC:

Powershell PC:

```python
import socket
import time

HOST = '0.0.0.0' # Listen on all interfaces
PORT = 65432
BUFFER_SIZE = 4000 # Matches your PYNQ buffer size (1000 * uint32)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen(1)
    print(f"Waiting for PYNQ to connect on port {PORT}...")
    conn, addr = s.accept()
    with conn:
        print(f"Connected by {addr}")
        total_bytes = 0
        start_time = time.time()
        try:
            while True:
                data = conn.recv(BUFFER_SIZE)
                if not data:
                    break
                total_bytes += len(data)
        except ConnectionResetError:
            pass 
        end_time = time.time()
        duration = end_time - start_time
        print(f"Received: {total_bytes / 1e6:.2f} MB")
        print(f"Throughput: {(total_bytes * 8) / (duration * 1e6):.2f} Mbps")
 ```
   
