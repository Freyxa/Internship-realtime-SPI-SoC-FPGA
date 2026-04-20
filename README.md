# Internship-realtime-SPI-SoC-FPGA
## Timeline:
### 1. Get familiar with the Zynq UltraScale+ using Xilinx Vivado and PYNQ with support from "The FPGA Programming Handbook" by Frank Bruno and Guy Eschemann
### 2. Pynq introduction
### 3. AXI stream 
### 4. Communication Zynq <-> Host PC
Setup FPGA communication to PC:

First step on Powershell PC:

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

Second step on PYNQ:

```python
from pynq import Overlay, allocate
import socket
import numpy as np
import time

ol = Overlay('design_tcp.bit')
dma = ol.axi_dma_0

data_size = 1000
output_buffer = allocate(shape=(data_size,), dtype=np.uint32)

PC_IP = '10.31.104.206'
PC_PORT = 65432

def run_speed_test(iterations=1000):
    print(f"Connecting to PC at {PC_IP}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        sock.connect((PC_IP, PC_PORT))
        print("Connected! Starting stream...")
        
        start_time = time.time()
        
        for i in range(iterations):
            # Hardware Transfer (PL -> DDR)
            dma.recvchannel.transfer(output_buffer)
            dma.recvchannel.wait()
            
            # Network Transfer (DDR -> PC)
            # .tobytes() avoids making a copy of the data
            sock.sendall(output_buffer.tobytes())
            
        end_time = time.time()
        
        # --- Statistics ---
        duration = end_time - start_time
        total_bits = iterations * output_buffer.nbytes * 8
        mbps = total_bits / (duration * 1e6)
        
        print(f"Done!")
        print(f"Sent {iterations} packets ({iterations * output_buffer.nbytes / 1e6:.2f} MB)")
        print(f"Total Time: {duration:.4f} seconds")
        print(f"Average Throughput: {mbps:.2f} Mbps")

    except Exception as e:
        print(f"Error during transfer: {e}")
    finally:
        sock.close()
        

run_speed_test(iterations=2000)
```
Output:

```python
Connecting to PC at 10.31.104.206...
Connected! Starting stream...
Done!
Sent 2000 packets (8.00 MB)
Total Time: 0.4944 seconds
Average Throughput: 129.44 Mbps
```
