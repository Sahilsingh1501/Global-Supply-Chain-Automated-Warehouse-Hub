# Supply Chain Management System

A C++ project for managing product inventory and suppliers using fundamental Data Structures & Algorithms (DSA) and Object-Oriented Programming (OOP) concepts.

---

## Technical Highlights

* **Linked List**: Primary storage for products with dynamic memory allocation (`Node*`).
* **Array**: Static array (`Supplier suppliers[20]`) for managing supplier records.
* **Algorithms**: **Linear Search** (for finding items) and **Bubble Sort** (for sorting by price).
* **OOP Concepts**: **Inheritance** (`Product` and `Supplier` derive from `Entity`), **Polymorphism** (`virtual void display()`), and **Encapsulation**.
* **File Persistence**: Automatically saves and loads data from `inventory.txt` and `suppliers.txt`.

---

## Core Features

1. **Add Product**: Prevents duplicate names/IDs and links each product to a Supplier ID.
2. **View Inventory**: Traverses the linked list to show all products.
3. **Search Product**: Locates a product and displays its linked supplier details.
4. **Sort by Price**: Orders products from lowest to highest price.
5. **Update Stock**: Adds new inventory quantities to existing stock.
6. **Delete Product**: Safely unlinks and deallocates a product node.
7. **Low Stock Alerts**: Displays items with less than 5 units in stock.
8. **Supplier Management**: Add and view registered suppliers.
9. **Reset System**: Clears all dynamic memory and resets stored data files.

---

## How to Build and Run

### Compilation
```bash
g++ -std=c++11 main.cpp -o app