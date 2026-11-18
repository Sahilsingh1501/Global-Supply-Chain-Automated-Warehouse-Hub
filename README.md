# Global Supply Chain & Automated Warehouse Hub

A fully integrated **C++ console application** that simulates a real-world global supply chain and warehouse management system. The system manages products, warehouses, routes, and shipments — all persisted to disk between runs — through a single unified menu.

---

## Features

### Inventory Management
- Add, update, and delete products with ID, name, price, and stock quantity
- Hash-table backed storage for O(1) average-case lookups
- Low-stock alerts and full inventory summary reports
- Input/output operator overloading for clean product serialisation

### Warehouse Management
- Add and delete warehouses with capacity limits
- Binary Search Tree (BST) structure for warehouses, ordered by ID
- Store inventory products inside warehouses with capacity enforcement
- Transfer stock between warehouses
- View capacity utilisation per warehouse

### Supply Chain & Routing
- Model warehouse-to-warehouse connections as a graph (adjacency matrix)
- BFS-based shortest-path finding between any two warehouses
- Create and track shipments along computed routes
- Display all active routes and shipment statuses

### Persistence (File I/O)
- Full system state saved to `warehouse_system.dat` on every write and on exit
- State restored automatically on the next startup using `ifstream`
- Atomic save strategy: writes to a temporary file first, then renames — no data loss on crash
- Versioned snapshot format (`GSC_STATE_V1`) with integrity validation

### Audit Log
- Every user action is appended (never overwritten) to `audit.log` using `ofstream` with `ios::app`
- Timestamped entries for: system start/shutdown, product add/delete/update, stock store, warehouse add/delete, route addition, shipment creation, and report generation

### Report Generator
- On-demand summary report written to `report.txt` via `ofstream`
- Sections:
  - **Inventory overview** — total products, total units, total value
  - **Low-stock alerts** — all products at or below the configured threshold
  - **Warehouse utilisation** — capacity used vs. total, with `EMPTY / OK / NEAR FULL` status per warehouse

---

## Project Structure

```
PBL/
├── VER2/
│   ├── integrated_main.cpp       # Main entry point — wires all modules together
│   ├── inventory_module.cpp      # Hash-table based inventory (Product & InventoryManager)
│   ├── warehouse_module.cpp      # BST-based warehouse management (Warehouse & WarehouseManager)
│   ├── supply_chain_module.cpp   # Graph, BFS routing, shipment tracking
│   ├── person_d_persistence.h    # File I/O layer (SystemPersistence, load/save)
│   ├── person_d_support.h        # Low-level stream helpers (readString, writeString)
│   ├── warehouse_system.dat      # Saved system state (auto-managed)
│   ├── audit.log                 # Operation history (append-only)
│   └── run_project.bat           # Build & run launcher
├── webapp/                       # Web interface (React + C++ HTTP server)
├── RUN_PROJECT.bat               # Root launcher
└── README.md                     # This file
```

---

## Getting Started

### Prerequisites
- A C++ compiler supporting **C++11** or later (e.g. GCC / MinGW on Windows)
- Compiler must be available on your system `PATH`

### Run (Windows — easiest)
Double-click **`RUN_PROJECT.bat`** at the root and select **Start system**.  
The launcher compiles and runs the integrated application automatically.

### Build Manually
```bash
cd VER2
g++ -std=c++11 -Wall -Wextra -pedantic integrated_main.cpp -o supply_chain.exe
./supply_chain.exe
```

---

## Main Menu Options

| Option | Action |
|--------|--------|
| `1` | Inventory Manager (add / delete / update / display products) |
| `2` | Warehouse Manager (add / delete / store products / transfer stock) |
| `3` | Supply Chain Manager (add routes / display connections / create shipments) |
| `4` | Display full snapshot (inventory + warehouses + routes + shipments) |
| `5` | Generate summary report → `report.txt` |
| `0` | Save & exit |

---

## Quick Test Walkthrough

1. **Add a product** — open *Inventory Manager* → option 1 → enter `101|Widget|2.50|50`
2. **Add warehouses** — open *Warehouse Manager* → option 1 → add warehouse `1` (North Hub, capacity 100) and warehouse `2` (South Hub, capacity 100)
3. **Store stock** — in *Warehouse Manager* → option 4 → store 20 units of product `101` in warehouse `1`
4. **Connect warehouses** — open *Supply Chain Manager* → option 1 → add route from `1` to `2`
5. **Create a shipment** — in *Supply Chain Manager* → option 3 → shipment from `1` to `2`, product `101`, qty `5`
6. **Generate report** — back in the main menu → option 5 → check `report.txt`
7. **Exit** — option `0` → data saved to `warehouse_system.dat`; `audit.log` updated

Restart the application and all data will be restored automatically.

---

## Data Structures & Concepts Used

| Module | Data Structure | Key Concept |
|--------|---------------|-------------|
| Inventory | Separate-chaining hash table | Hashing, collision resolution, `<<` / `>>` overloading |
| Warehouse | Binary Search Tree (BST) | Tree insertion/deletion, in-order traversal |
| Supply Chain | Adjacency matrix graph | BFS shortest-path traversal |
| Persistence | File streams (`fstream`) | Serialisation, atomic writes, `ifstream` / `ofstream` |
| Audit / Report | Append-mode `ofstream` | File I/O, exception-safe resource handling |

---

## Notes

- `warehouse_system.dat` is auto-created on first save. Keep it in the `VER2/` folder to preserve state between runs.
- `audit.log` grows indefinitely in append mode — archive or clear it periodically if needed.
- `report.txt` is overwritten each time option 5 is selected.
- The web interface (`webapp/`) requires Node.js and runs separately from the console application.
