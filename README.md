# Global Supply Chain & Automated Warehouse Hub

A high-performance C++ management and logistics system designed to simulate and manage an end-to-end automated global supply chain and distribution network. The platform models interconnected regional distribution centers, global inventory catalogs, localized warehouse capacities, dynamic routing, and shipment tracking with transactional state persistence.

---

## 🌟 Core Concept & Architecture

The system operates on a coordinated two-tier architecture designed to bridge high-speed in-memory data structures with persistent disk storage:


### 1. Global Network & Route Discovery (Graph Layer)
* Models distribution centers and regional hubs as vertices in a connected graph.
* Routes between warehouses are tracked with bidirectional adjacency links.
* Implements **Breadth-First Search (BFS)** for shortest-path route discovery across multi-hop warehouse connections.

### 2. Warehouse Storage & Capacity Engine (Tree Layer)
* Represents individual warehouses stored and retrieved via an organized **Binary Search Tree (BST)**.
* Tracks maximum capacity constraints, utilized volume, and product-level allocations per facility.
* Enforces strict capacity validation before accepting stock deposits or inbound shipment deliveries.

### 3. Inventory Catalog Management (Hash Table Layer)
* High-speed catalog lookups powered by a **Separate-Chaining Hash Table**, enabling $O(1)$ average-time operations for SKU indexing, stock updates, and pricing records.
* Generates real-time low-stock alerts based on configurable inventory thresholds.

### 4. Transactional Persistence & Audit Trail
* **State Persistence**: Serializes the complete state (catalogs, warehouse inventories, route topologies, and active shipment waypoints) to `warehouse_system.dat`.
* **Append-Only Audit Log**: Every state-modifying action (e.g., adding stock, creating routes, dispatching or moving shipments) is stamped and appended to `audit.log` for traceability and error recovery.
* **Startup Validation**: Performs integrity checks on startup to ensure stored or in-transit stock never exceeds total catalog quantities.

---

## 🚀 Key Features

* **Global Catalog Control**: Add, update, search, and delete products with automatic validation rules.
* **Warehouse Registry & Stock Allocation**: Create regional warehouses, monitor capacity limits, and transfer inventory from the global catalog into designated warehouses.
* **Dynamic Route Management**: Establish transport lanes between facilities; routes required by active shipments are protected against premature deletion.
* **End-to-End Shipment Lifecycle**:
  * **Automated Dispatch**: Deducts stock from origin warehouse upon shipment creation.
  * **Multi-hop Routing**: Computes shortest route between origin and destination via BFS.
  * **Step-by-step Movement**: Simulates cargo advancing through waypoints along the route.
  * **Disruption Handling**: Supports marking shipments as `Delayed` and resuming them once clear.
  * **Safe Delivery**: Verifies that the destination warehouse has sufficient capacity before finalizing delivery and restocking inventory.
* **Data Integrity Safeguards**:
  * Cannot delete catalog products currently held in warehouse stock or assigned to active shipments.
  * Cannot delete warehouses actively participating in routes or shipments.
  * Rejects operations exceeding capacity or available inventory.

---

## 🛠️ Data Structures & Algorithms

| Module | Data Structure / Algorithm | Technical Implementation |
| :--- | :--- | :--- |
| **Inventory Catalog** | Hash Table (Separate Chaining) | Dynamic buckets with linked collision resolution; polynomial rolling hash for $O(1)$ lookup. |
| **Warehouse Registry** | Binary Search Tree (BST) | Ordered by numeric Warehouse ID for $O(\log n)$ average search, insertion, and ordered reporting. |
| **Logistics Network** | Adjacency Matrix Graph & Custom Queue | Custom-built queue driving BFS traversal to determine shortest transit routes between hubs. |
| **Shipment Tracking** | Singly Linked List | Tracks active cargo, current waypoint index, remaining route length, and status. |
| **File Persistence** | File Streams (`fstream`) | Custom serialization and deserialization routines ensuring full data survival between runs. |

---
