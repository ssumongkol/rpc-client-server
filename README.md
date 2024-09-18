# RPC Client-Server in C

This repository contains an implementation of a simple Remote Procedure Call (RPC) client-server model in C. This project was created as a side project inspired by concepts learned in my computer systems and networking courses during my Master's in Software Engineering.

## Overview

Remote Procedure Call (RPC) is a protocol that allows a program to request a service or function execution on a remote server in a way similar to calling a local procedure. This project demonstrates a basic RPC mechanism using C to establish communication between a client and server, simulating how networked applications can invoke functions remotely.

## Features

- **Client-Server Architecture:** Implements a simple client-server model using socket programming in C.
- **RPC Mechanism:** The client can remotely invoke specific functions on the server, with arguments being passed over the network.
- **Synchronous Communication:** The client waits for the server's response after sending a request, demonstrating synchronous RPC calls.
- **Error Handling:** Basic error handling to manage network communication issues and invalid requests.

## Why RPC?

RPC is a fundamental concept in distributed systems, enabling inter-process communication across different machines. Implementing an RPC mechanism in C provides a deeper understanding of low-level networking concepts, socket programming, and remote service execution.

## Getting Started

### Prerequisites

- GCC or any standard C compiler.
- Basic knowledge of socket programming in C.

### Compilation

To compile the client and server programs, use the following commands:

```bash
make all
