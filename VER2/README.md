# Global Supply Chain and Warehouse Management System

The project has a single integrated console application for Persons A, B, C, and D. Inventory products, warehouses, routes, and shipments share the same managers and persist between runs.

## Implemented modules

- `main.cpp` (Person A): inventory stored in a separate-chaining hash table, with add, delete, update, display, and `<<` / `>>` product operators.
- `warehouse_module.cpp` (Person B): warehouses stored in a binary search tree ordered by warehouse ID. Supports adding, deleting, displaying, and storing inventory products. Products must first be added in Inventory Manager.
- `supply_chain_module.cpp` (Person C): warehouse routes represented by a graph, BFS route finding, route creation, and shipment creation. The integrated manager supports adding routes, displaying connected warehouses, and creating shipments; BFS route finding remains available in the standalone Person C demo.
- `integrated_main.cpp`: connects all three modules under one main menu. It prevents deleting inventory products that are stored in a warehouse, deleting warehouses referenced by routes or active shipments, and removing routes required by active shipments.
- Person D integration: saves inventory, warehouse stock, routes, and shipment progress to a versioned snapshot and validates saved data on startup.

The standalone Person A and Person B programs remain available. The integrated system automatically restores its last saved state and writes successful changes immediately, so data persists when the application exits.

## Run the project on Windows

Double-click `run_project.bat` and choose **Start system**. The launcher builds and starts the integrated application, where inventory, warehouse, and supply-chain operations share the same data. A C++ compiler must be installed and available on `PATH`.

The integrated application stores its current snapshot in `warehouse_system.dat` in the project folder. Keep this file with the application to preserve system data.

The integrated application can also be built directly:

```text
g++ -std=c++11 -Wall -Wextra -pedantic integrated_main.cpp -o supply_chain.exe
```

Build Person C's standalone demo with:

```text
g++ -std=c++11 -Wall -Wextra -pedantic person_c_demo.cpp -o person_c_demo.exe
```

The standalone Person A and Person B programs are still available for development, but the launcher intentionally starts only the integrated system. The standalone Person C demo is also available by compiling `person_c_demo.cpp`; it does not use the integrated system's saved data.

## Quick integrated test

1. Open **Inventory Manager** and add `101|Widget|2.50|20`.
2. Open **Warehouse Manager** and add warehouse `1` (North, capacity 10) and warehouse `2` (South, capacity 10). Store 8 units of inventory product `101` in warehouse 1.
3. Open **Supply Chain Manager**, add route `1` to `2`, then create a shipment from `1` to `2` for product `101`, quantity `3`. The shipment is created along the route and its details appear in the system display.

Choose **Display inventory and warehouses** from the main menu to see shared data and shipment summaries.
