# Global Supply Chain and Warehouse Management System

The project now has a single integrated console application for Persons A, B, and C. Inventory products, warehouses, routes, and shipments share the same in-memory managers during one run.

## Implemented modules

- `main.cpp` (Person A): inventory stored in a separate-chaining hash table, with add, delete, update, search, display, low-stock reports, and `<<` / `>>` product operators.
- `warehouse_module.cpp` (Person B): warehouses stored in a binary search tree ordered by warehouse ID. Supports adding, deleting, searching, displaying, capacity reports, and storing/removing product quantities. Products stored in a warehouse must first exist in the inventory catalog.
- `supply_chain_module.cpp` (Person C): warehouse routes represented by a graph, BFS route finding, route add/remove, shipment creation and tracking, shipment status updates, and step-by-step movement. Dispatch removes stock from the origin; delivery adds it to the destination, subject to its capacity.
- `integrated_main.cpp`: connects all three modules under one main menu. It prevents deleting catalog products that are stored in a warehouse, deleting warehouses referenced by routes or active shipments, and removing routes required by active shipments.

The standalone Person A and Person B programs remain available. Data is currently in memory only; Person D's file persistence, audit log, reporting, and exception-handling integration are still to come. Data resets when an application exits.

## Run the project on Windows

Double-click `run_project.bat` and choose **Start system**. The launcher builds and starts the integrated application, where inventory, warehouse, and supply-chain operations share the same data. A C++ compiler must be installed and available on `PATH`.

The integrated application can also be built directly:

```text
g++ -std=c++11 -Wall -Wextra -pedantic integrated_main.cpp -o supply_chain.exe
```

Build Person C's standalone demo with:

```text
g++ -std=c++11 -Wall -Wextra -pedantic person_c_demo.cpp -o person_c_demo.exe
```

The standalone Person A and Person B programs are still available for development, but the launcher intentionally starts only the integrated system. The standalone Person C demo is also available by compiling `person_c_demo.cpp`; it starts with empty catalog and warehouse data.

## Quick integrated test

1. Open **Inventory Manager** and add `101|Widget|2.50|20`.
2. Open **Warehouse Manager** and add warehouse `1` (North, capacity 10) and warehouse `2` (South, capacity 10). Store 8 units of catalog product `101` in warehouse 1.
3. Open **Supply Chain Manager**, add route `1` to `2`, then create a shipment from `1` to `2` for product `101`, quantity `3`.
4. Move the shipment once and track it. It should arrive at South; the warehouse capacities should show 5 units at North and 3 units used at South.

Choose **Display inventory and warehouses** from the main menu to see shared data and shipment summaries.
