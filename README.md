# Deribit Trader

A **fully functional trading system** designed for Deribit’s Testnet, supporting a range of trading operations, including **placing, canceling, and modifying orders**, retrieving order books, and monitoring current positions. This system is built with a focus on **low latency** and **high performance**, optimized for trading **spot, futures, and options** across all supported symbols on Deribit.

## Table of Contents

1. [Features](#features)
2. [Installation](#installation)
3. [Configuration](#configuration)
4. [Usage](#usage)
5. [Project Structure](#project-structure)
6. [Contributing](#contributing)
7. [License](#license)

## Features

- **Order Execution**: Place, modify, and cancel orders.
- **Order Book Retrieval**: Fetch current order books with updates.
- **Position Monitoring**: View current positions in real time.
- **Low Latency**: Optimized for fast and efficient trade execution.
- **Support for Spot, Futures, and Options**: Trade across all Deribit markets.

## Installation

### Prerequisites

- **C++ Compiler** (compatible with C++17 or higher)
- **Boost** library with necessary headers and binaries
- Boost library
- **vcpkg** package manager

### Setup

Follow these steps to set up your environment with required libraries:

1. **Update Package Lists**:
   ```bash
   sudo apt update
   sudo apt install -y build-essential cmake

2. **For compiling and Execution**:
   ```bash
   sudo apt install -y libboost-all-dev libssl-dev
   g++ -o deribit_trader.cpp -lssl -lboost_system -lcrypto -lpthread

