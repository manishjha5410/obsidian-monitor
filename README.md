# Low-Latency Metrics Processor (monitor)

## Overview

`monitor` is a specialized low-latency metrics processor designed to handle high-throughput streaming data with minimal overhead. It focuses on real-time quantile estimation and peak exponentially weighted statistics (EPS), making it ideal for monitoring applications that require instant insights and adaptive metrics.

## Features

- Real-time quantile estimation for streaming data.
- Rolling peak EPS computation for dynamic rate tracking.
- Zero dynamic memory allocation to ensure predictable latency.
- Compact memory footprint suitable for embedded and resource-constrained environments.
- Simple interface for integration into existing systems.
- Due to entire execution in hot path the current system can easily scale to >=25k tokens/sec

## Design Choices

- **Memory & Allocation Strategy:** Uses fixed-size buffers and stack allocation exclusively to avoid runtime heap allocations. This design eliminates memory fragmentation and ensures consistent processing times.
- **Quantiles:** Implements efficient online algorithms tailored for streaming quantile estimation, enabling accurate percentile calculations without storing the entire dataset.
- **Peak EPS:** Incorporates rolling exponentially weighted peak statistics to capture dynamic rate changes with responsiveness and stability.

## Build

To build the `monitor` component:
```sh
g++ -O3 -std=c++17 main.cpp -o monitor
```
Make sure your compiler supports C++17 features.

## Usage

Run the `monitor` executable with streaming input data:
```sh
./monitor --input orders.monbin --out metrics.json
```
Input format and command-line options are documented within `main.cpp`.

## Division

The code starts with main.cpp

### Main.cpp

This is the entry point of the Low-Latency Metrics Processor (monitor).

What it does
-	Parses MON1 binary files containing a header + fixed-size records (72B each).
-	Processes events: Tracks total events, total errors.
-   Creates a Resorce class which reads a binary file and internally calls rollingeps and fixedhashmap
-	Measures NEW → CONFIRM latency distribution (p50, p90, p99) using Quantile class.
-	Writes results to a JSON file (metrics.json) with deterministic output.

### FixedHashMap.h

This file implements a custom fixed-size hash map designed specifically to manage in-flight NEW orders efficiently. The hash map uses a fixed capacity determined at initialization and handles collisions through linear probing.

#### Structure

- **MapEntry**: Represents an individual entry in the hash map. It contains:
  - `key`: A 16-byte key uniquely identifying an order.
  - `ts_ns`: A timestamp in nanoseconds associated with the order.
  - `state`: An indicator of the entry's status:
    - `0`: Empty slot
    - `1`: Occupied slot with valid data
    - `2`: Deleted slot (tombstone)

- **FixedHashMap**: The main hash map structure containing:
  - A vector of `MapEntry` objects.
  - A mask used to efficiently compute indices within the fixed capacity.

#### Operations

- **put(key, ts)**: Inserts a new entry with the given key and timestamp or updates the timestamp if the key already exists. It returns `true` if the insertion or update is successful, and `false` if the map is full.

- **erase_get(key, ts)**: Searches for the entry with the given key, retrieves its timestamp, marks the entry as deleted, and returns `true`. Returns `false` if the key is not found.

This design provides a lightweight and efficient mechanism for tracking orders with minimal overhead.

#### Objective

- Data is added via the resources class new data is puted while the old confirmed data is erased via resource object in main.cpp

### helper.h

The `helper.h` file provides several helper utilities essential for working with binary data and hashing in the project:

- **Key16**: A struct representing a 16-byte array, commonly used as a hash key for `client_order_id`.
- **Endianness conversion functions**: Includes `be16toh_u`, `be32toh_u`, and `be64toh_u` functions that safely decode big-endian binary fields into host byte order, ensuring correct interpretation of network or file data.
- **hash_key**: Implements the FNV-1a hash function specifically for `Key16`, producing a 64-bit hash value. This hash is utilized in the `FixedHashMap` for efficient key lookups.

### RollingEPS.h

`RollingEPS.h` implements a rolling events-per-second (EPS) counter. It divides one second into 10 buckets, each representing 100 milliseconds. When an event is added via the `add(ts)` method, the event is placed into the correct time bucket corresponding to its timestamp, and the rolling window advances if necessary. The class maintains a `window_sum` which is the total count of events in the current 1-second window, and it also tracks the peak events-per-second observed so far.

This implementation avoids storing every individual timestamp by using fixed-size arrays and a circular buffer to efficiently manage counts per bucket, allowing for constant-time updates and queries.

#### Objective

- Each of the data is added timestamp recorded in eps.h added via the resorces class

### Quantile.h


The `Quantile.h` file implements the P² (Jain–Chlamtac) streaming quantile estimator. This algorithm is designed to estimate quantiles such as the median (p50), 90th percentile (p90), or 99th percentile (p99) without the need to store all the samples in memory.

#### How It Works
The estimator maintains five markers that represent positions and heights corresponding to the desired quantile. On each new data insertion, these markers are updated based on the incoming value. The algorithm uses parabolic and linear interpolation methods to adjust the marker heights, ensuring the quantile estimate adapts dynamically as more data is processed.

#### Benefits
- **O(1) Memory Usage:** Only five markers are stored regardless of the number of samples.
- **O(1) Time per Update:** Each new sample requires a fixed amount of computation.
- **Deterministic Output:** Unlike randomized algorithms, this approach yields consistent quantile estimates for the same input sequence.

#### Objective

- 50,70,90 Quantile is calculated via the resource class

