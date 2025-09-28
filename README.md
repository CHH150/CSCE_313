# PA1 – Named Pipes

For this assignment, we had to build a client program that communicates with a server using named pipes. The server contains ECG data for 15 patients, and the client connects to it to perform several tasks:

- Request a single ECG data point from the BIMDC dataset.
- Collect the first 1000 data points for a patient and save them to x1.csv.
- Request an entire file from the server, which is sent back in chunks if it’s too large for one transfer.
- Create a new communication channel when needed.
- Launch the server as a child process of the client using fork and exec, so both run from a single terminal.

## GitHub Repository Link
https://github.com/CHH150/CSCE_313
