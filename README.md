# Internship-realtime-SPI-SoC-FPGA
## Timeline:
## 1. Get familiar with the Zynq UltraScale+ using Xilinx Vivado and PYNQ with support from "The FPGA Programming Handbook" by Frank Bruno and Guy Eschemann
## 2. Pynq introduction
## 3. AXI stream 
## 4. Communication Zynq <-> Host PC

Setup CPU communication to PC:

First step on Powershell PC:

```python
import socket
# Use 0.0.0.0 to listen on all available network interfaces
server_address = ('0.0.0.0', 65432)
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind(server_address)
server.listen(1)

print("Waiting for Zynq connection...")
conn, addr = server.accept()
print(f"Connected by {addr}")

data = conn.recv(1024)
print(f"Received: {data.decode()}")
conn.close()
```


Second step on PYNQ:

```python
import socket
address = ("10.31.104.206", 65432) 
client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    client.connect(address)
    client.sendall(b"Hello from Zynq CPU!")
    print("Message sent successfully!")
except ConnectionRefusedError:
    print("Connection failed. Is the server running on the PC?")
finally:
    client.close()
```


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
## FFT IP

### FFT IP test on Zynq ultrascale+

1. Vivado bloc design
   
<img width="2759" height="1257" alt="image" src="https://github.com/user-attachments/assets/24e860be-da50-4eea-8e96-1c3aa4474fbb" />


2. FFT IP config


<img width="2373" height="1548" alt="image" src="https://github.com/user-attachments/assets/a98abbfd-fb34-47aa-bf8e-459aac49b1cb" />
<img width="2383" height="1557" alt="image" src="https://github.com/user-attachments/assets/557d590d-8eaa-49f1-80f9-f9c9011b6c77" />


3. Jupyter notebook scripting on PYNQ


```python
import numpy as np
from pynq import allocate, Overlay
import plotly.graph_objects as go
import math

# Load Overlay
overlay = Overlay('design_fft.bit')
dma = overlay.axi_dma_0 

# Parameters 
FFT_Size = 1024
SCALE = 16000  # Q15-like amplitude (safe margin from overflow)

# Buffers 
TX_buffer = allocate(shape=(FFT_Size,), dtype=np.uint32)
RX_buffer = allocate(shape=(FFT_Size,), dtype=np.uint32)

print("TX Buffer Address:", hex(TX_buffer.physical_address))
print("RX Buffer Address:", hex(RX_buffer.physical_address))

# Generate Signals
sinc_LUT = np.zeros(FFT_Size, dtype=np.int16)
rect_LUT = np.zeros(FFT_Size, dtype=np.int16)
cos_LUT  = np.zeros(FFT_Size, dtype=np.int16)

center = FFT_Size // 2

for i in range(FFT_Size):
    #  Proper centered sinc 
    x = i - center
    
    if x == 0:
        sinc_val = 1.0
    else:
        sinc_val = np.sin(np.pi * x / 32) / (np.pi * x / 32)

    sinc_LUT[i] = np.int16(round(SCALE * sinc_val))

    #  Rectangular signal 
    if (center - 12) < i < (center + 12):
        rect_LUT[i] = np.int16(30000)
    else:
        rect_LUT[i] = np.int16(0)

    #  Cosine (for validation) 
    angle = (8 * 2.0 * math.pi * i) / FFT_Size
    cos_LUT[i] = np.int16(round(SCALE * np.cos(angle)))

#  Select signal to test 
signal = sinc_LUT   # <-- change to rect_LUT or cos_LUT for testing

#  Pack into TX buffer 
for i in range(FFT_Size):
    real = np.int16(signal[i])
    imag = np.int16(0)

    TX_buffer[i] = (np.uint32(imag & 0xFFFF) << 16) | (np.uint32(real & 0xFFFF))

#  DMA Transfer
dma.sendchannel.transfer(TX_buffer)
dma.recvchannel.transfer(RX_buffer)

dma.sendchannel.wait()
dma.recvchannel.wait()

#  Unpack FFT output 
real_buffer = np.zeros(FFT_Size, dtype=np.int16)
imag_buffer = np.zeros(FFT_Size, dtype=np.int16)
abs_buffer  = np.zeros(FFT_Size)

for i in range(FFT_Size):
    real_buffer[i] = np.int16(RX_buffer[i] & 0xFFFF)
    imag_buffer[i] = np.int16((RX_buffer[i] >> 16) & 0xFFFF)

    abs_buffer[i] = math.sqrt(
        float(real_buffer[i])**2 + float(imag_buffer[i])**2
    )

# Normalize for visualization
abs_buffer /= np.max(abs_buffer)

#  Reference FFT (NumPy)
ref_fft = np.abs(np.fft.fftshift(np.fft.fft(signal)))
ref_fft /= np.max(ref_fft)

# Plot Time Domain 
fig = go.Figure()
fig.add_trace(go.Scatter(y=signal, mode='lines', name='Input Signal'))
fig.update_layout(title='Time Domain Signal', xaxis_title='Index', yaxis_title='Amplitude')
fig.show()

#  Plot FPGA FFT 
fig = go.Figure()
fig.add_trace(go.Scatter(y=np.fft.fftshift(abs_buffer), mode='lines', name='FPGA FFT'))
fig.update_layout(title='FPGA FFT Output', xaxis_title='Frequency Bin', yaxis_title='Normalized Amplitude')
fig.show()

#  Plot NumPy FFT
fig = go.Figure()
fig.add_trace(go.Scatter(y=ref_fft, mode='lines', name='NumPy FFT (Reference)'))
fig.update_layout(title='Reference FFT (NumPy)', xaxis_title='Frequency Bin', yaxis_title='Normalized Amplitude')
fig.show()
```
<img width="983" height="525" alt="newplot" src="https://github.com/user-attachments/assets/89dcaf8e-8006-4e92-9b1e-7d37508bd6d0" />
<img width="983" height="525" alt="newplot(1)" src="https://github.com/user-attachments/assets/dc7a05d5-a403-4bcf-8ebe-211e392e516e" />
<img width="983" height="525" alt="newplot(2)" src="https://github.com/user-attachments/assets/569e62a5-8c60-4ac4-bc69-1b0bd0c0fc67" />

