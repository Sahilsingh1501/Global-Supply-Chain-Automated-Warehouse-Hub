/*
 * ======================================================================
 *  Global Supply Chain & Automated Warehouse Hub
 *  Person C - Global Network Graph & Traversals (Tier 1)
 * ======================================================================
 *
 *  Core Modules Implemented:
 *      1. Display Connected Warehouses (Graph & Adjacency List)
 *      2. Connect Warehouses (Add Supply Route / Edge)
 *      3. Add a Shipment (Add / Receive Inventory at a Warehouse)
 *      4. Search Product across Warehouses (BFS Traversal)
 *
 *  Constraints Followed:
 *      - Built from scratch using raw pointers (No vector, queue, list, etc.)
 *      - OOP: Abstract Base Class, Inheritance, Polymorphism (Virtual Functions)
 *      - Standard ASCII output (no terminal encoding issues)
 *
 *  Compile:  g++ warehouse_network.cpp
 *  Run:      .\a.exe
 * ======================================================================
 */

#include <iostream>
#include <string>

using namespace std;

// ----------------------------------------------------------------------
// 1. PRODUCT RECORD & INVENTORY LINKED LIST (Built from scratch)
// ----------------------------------------------------------------------
struct ProductRecord {
    string sku;
    string name;
    int quantity;

    ProductRecord() : sku(""), name(""), quantity(0) {}
    ProductRecord(string s, string n, int q) : sku(s), name(n), quantity(q) {}
};

struct ProductNode {
    ProductRecord data;
    ProductNode* next;

    ProductNode(ProductRecord d) : data(d), next(nullptr) {}
};

class ProductList {
private:
    ProductNode* head;
    int count;

public:
    ProductList() : head(nullptr), count(0) {}

    ~ProductList() {
        ProductNode* cur = head;
        while (cur) {
            ProductNode* tmp = cur;
            cur = cur->next;
            delete tmp;
        }
        head = nullptr;
        count = 0;
    }

    // Add shipment: if product already exists, add quantity; otherwise insert new node
    void addShipment(const string& sku, const string& name, int qty, bool verbose = true) {
        ProductNode* cur = head;
        while (cur) {
            if (cur->data.sku == sku) {
                cur->data.quantity += qty;
                if (verbose) {
                    cout << "  [UPDATED] Existing product found. Added " << qty
                         << " units. Total stock: " << cur->data.quantity << "\n";
                }
                return;
            }
            cur = cur->next;
        }

        // New product insertion at front
        ProductNode* newNode = new ProductNode(ProductRecord(sku, name, qty));
        newNode->next = head;
        head = newNode;
        count++;
        if (verbose) {
            cout << "  [SUCCESS] New shipment recorded: " << name
                 << " (SKU: " << sku << ", Qty: " << qty << ")\n";
        }
    }

    // Search product by name (case-sensitive substring match)
    ProductNode* searchByName(const string& name) const {
        ProductNode* cur = head;
        while (cur) {
            if (cur->data.name.find(name) != string::npos) {
                return cur;
            }
            cur = cur->next;
        }
        return nullptr;
    }

    int getCount() const { return count; }
    bool isEmpty() const { return head == nullptr; }

    void display() const {
        ProductNode* cur = head;
        while (cur) {
            cout << "        SKU: " << cur->data.sku
                 << "  |  Name: "   << cur->data.name
                 << "  |  Stock Qty: " << cur->data.quantity << "\n";
            cur = cur->next;
        }
    }
};

// ----------------------------------------------------------------------
// 2. OOP CLASS HIERARCHY (Inheritance & Polymorphism)
// ----------------------------------------------------------------------
class NetworkNode {
protected:
    int id;
    string name;
    string location;
    ProductList inventory;

public:
    NetworkNode(int id, string name, string location)
        : id(id), name(name), location(location) {}

    virtual ~NetworkNode() {}

    // Pure virtual function
    virtual void displayInfo() const = 0;

    int getId() const { return id; }
    const string& getName() const { return name; }
    const string& getLocation() const { return location; }
    ProductList& getInventory() { return inventory; }
    const ProductList& getInventoryConst() const { return inventory; }
};

// Derived: Central Hub
class CentralHub : public NetworkNode {
private:
    int maxCapacity;

public:
    CentralHub(int id, string name, string location, int capacity)
        : NetworkNode(id, name, location), maxCapacity(capacity) {}

    void displayInfo() const override {
        cout << "  [CENTRAL HUB]  ID: " << id
             << " | " << name
             << " | Location: " << location
             << " | Max Capacity: " << maxCapacity
             << " | Products: " << inventory.getCount() << "\n";
    }
};

// Derived: Regional Warehouse
class RegionalWarehouse : public NetworkNode {
private:
    string region;

public:
    RegionalWarehouse(int id, string name, string location, string region)
        : NetworkNode(id, name, location), region(region) {}

    void displayInfo() const override {
        cout << "  [REGIONAL WH]  ID: " << id
             << " | " << name
             << " | Location: " << location
             << " | Region: " << region
             << " | Products: " << inventory.getCount() << "\n";
    }
};

// ----------------------------------------------------------------------
// 3. CUSTOM QUEUE (Linked-list based for BFS, no STL)
// ----------------------------------------------------------------------
struct QueueNode {
    int data;
    QueueNode* next;

    QueueNode(int d) : data(d), next(nullptr) {}
};

class Queue {
private:
    QueueNode* front;
    QueueNode* rear;

public:
    Queue() : front(nullptr), rear(nullptr) {}

    ~Queue() {
        while (!isEmpty()) dequeue();
    }

    void enqueue(int val) {
        QueueNode* node = new QueueNode(val);
        if (rear) rear->next = node;
        else front = node;
        rear = node;
    }

    int dequeue() {
        if (isEmpty()) return -1;
        QueueNode* tmp = front;
        int val = tmp->data;
        front = front->next;
        if (!front) rear = nullptr;
        delete tmp;
        return val;
    }

    bool isEmpty() const { return front == nullptr; }
};

// ----------------------------------------------------------------------
// 4. GRAPH DATA STRUCTURE (Adjacency List with raw pointers)
// ----------------------------------------------------------------------
struct AdjNode {
    int destId;
    AdjNode* next;

    AdjNode(int d) : destId(d), next(nullptr) {}
};

struct GraphVertex {
    NetworkNode* warehouse;
    AdjNode* adjHead;
    bool occupied;

    GraphVertex() : warehouse(nullptr), adjHead(nullptr), occupied(false) {}
};

class WarehouseGraph {
private:
    static const int MAX_VERTICES = 100;
    GraphVertex vertices[MAX_VERTICES];
    int vertexCount;

    int findIndex(int id) const {
        for (int i = 0; i < MAX_VERTICES; i++) {
            if (vertices[i].occupied && vertices[i].warehouse->getId() == id)
                return i;
        }
        return -1;
    }

    int findFreeSlot() const {
        for (int i = 0; i < MAX_VERTICES; i++) {
            if (!vertices[i].occupied) return i;
        }
        return -1;
    }

public:
    WarehouseGraph() : vertexCount(0) {}

    ~WarehouseGraph() {
        for (int i = 0; i < MAX_VERTICES; i++) {
            if (vertices[i].occupied) {
                AdjNode* cur = vertices[i].adjHead;
                while (cur) {
                    AdjNode* tmp = cur;
                    cur = cur->next;
                    delete tmp;
                }
                delete vertices[i].warehouse;
                vertices[i].occupied = false;
            }
        }
    }

    // Add a warehouse node to the network
    bool addWarehouse(NetworkNode* node) {
        if (!node) return false;
        if (findIndex(node->getId()) != -1) {
            cout << "  [Error] Warehouse with ID " << node->getId() << " already exists.\n";
            return false;
        }
        int slot = findFreeSlot();
        if (slot == -1) return false;

        vertices[slot].warehouse = node;
        vertices[slot].adjHead = nullptr;
        vertices[slot].occupied = true;
        vertexCount++;
        return true;
    }

    // Connect two warehouses (undirected edge)
    bool connectWarehouses(int id1, int id2, bool verbose = true) {
        int i1 = findIndex(id1);
        int i2 = findIndex(id2);
        if (i1 == -1 || i2 == -1) {
            if (verbose) cout << "  [Error] One or both warehouse IDs do not exist.\n";
            return false;
        }
        // Check if edge already exists
        AdjNode* check = vertices[i1].adjHead;
        while (check) {
            if (check->destId == id2) {
                if (verbose) cout << "  [Info] Connection between " << id1 << " and " << id2 << " already exists.\n";
                return false;
            }
            check = check->next;
        }

        // Add id2 to id1
        AdjNode* n1 = new AdjNode(id2);
        n1->next = vertices[i1].adjHead;
        vertices[i1].adjHead = n1;

        // Add id1 to id2
        AdjNode* n2 = new AdjNode(id1);
        n2->next = vertices[i2].adjHead;
        vertices[i2].adjHead = n2;

        if (verbose) {
            cout << "  [OK] Successfully connected " << vertices[i1].warehouse->getName()
                 << " <-> " << vertices[i2].warehouse->getName() << "\n";
        }
        return true;
    }

    // ------------------------------------------------------------------
    // MODULE 1: DISPLAY CONNECTED WAREHOUSES
    // ------------------------------------------------------------------
    void displayConnectedWarehouses() const {
        if (vertexCount == 0) {
            cout << "  [Info] No warehouses in the network.\n";
            return;
        }

        cout << "\n  ===============================================================\n";
        cout << "                   CONNECTED WAREHOUSE NETWORK                   \n";
        cout << "  ===============================================================\n\n";

        for (int i = 0; i < MAX_VERTICES; i++) {
            if (!vertices[i].occupied) continue;

            // Polymorphic display of warehouse details
            vertices[i].warehouse->displayInfo();

            // Display connected neighbouring warehouses
            AdjNode* adj = vertices[i].adjHead;
            if (!adj) {
                cout << "        -> Connected to: (No active connections / Isolated)\n\n";
            } else {
                cout << "        -> Connected to: ";
                while (adj) {
                    int neighborIdx = findIndex(adj->destId);
                    if (neighborIdx != -1) {
                        cout << vertices[neighborIdx].warehouse->getName()
                             << " [ID: " << adj->destId << "]";
                    }
                    if (adj->next) cout << ", ";
                    adj = adj->next;
                }
                cout << "\n\n";
            }
        }
        cout << "  Total active warehouses in network: " << vertexCount << "\n";
        cout << "  ===============================================================\n";
    }

    // ------------------------------------------------------------------
    // MODULE 2: ADD A SHIPMENT TO A WAREHOUSE
    // ------------------------------------------------------------------
    bool addShipmentToWarehouse(int warehouseId, string sku, string name, int qty, bool verbose = true) {
        int idx = findIndex(warehouseId);
        if (idx == -1) {
            if (verbose) cout << "  [Error] Warehouse ID " << warehouseId << " does not exist.\n";
            return false;
        }

        if (verbose) cout << "\n  Processing shipment for " << vertices[idx].warehouse->getName() << "...\n";
        vertices[idx].warehouse->getInventory().addShipment(sku, name, qty, verbose);
        return true;
    }

    // ------------------------------------------------------------------
    // MODULE 3: SEARCH PRODUCT AND FIND WHICH WAREHOUSE HAS IT (BFS)
    // ------------------------------------------------------------------
    void searchProductGlobal(const string& productName) const {
        if (vertexCount == 0) {
            cout << "  [Info] The network is empty.\n";
            return;
        }

        bool visited[MAX_VERTICES];
        for (int i = 0; i < MAX_VERTICES; i++) visited[i] = false;

        int foundCount = 0;

        cout << "\n  +-------------------------------------------------------------+\n";
        cout << "  |  GLOBAL BFS SEARCH FOR PRODUCT: \"" << productName << "\"\n";
        cout << "  +-------------------------------------------------------------+\n\n";

        // BFS traversal covering all connected and disconnected clusters
        for (int start = 0; start < MAX_VERTICES; start++) {
            if (!vertices[start].occupied || visited[start]) continue;

            Queue bfsQueue;
            bfsQueue.enqueue(start);
            visited[start] = true;

            while (!bfsQueue.isEmpty()) {
                int cur = bfsQueue.dequeue();
                NetworkNode* wh = vertices[cur].warehouse;

                // Inspect inventory in this warehouse
                ProductNode* found = wh->getInventoryConst().searchByName(productName);
                if (found) {
                    foundCount++;
                    cout << "  [FOUND] In Warehouse:\n";
                    cout << "    ";
                    wh->displayInfo(); // Polymorphic call
                    cout << "    -> Item Details: " << found->data.name
                         << " | SKU: " << found->data.sku
                         << " | Available Stock: " << found->data.quantity << " units\n\n";
                }

                // Traverse connected neighbors
                AdjNode* adj = vertices[cur].adjHead;
                while (adj) {
                    int neighborIdx = findIndex(adj->destId);
                    if (neighborIdx != -1 && !visited[neighborIdx]) {
                        visited[neighborIdx] = true;
                        bfsQueue.enqueue(neighborIdx);
                    }
                    adj = adj->next;
                }
            }
        }

        if (foundCount == 0) {
            cout << "  [NOT FOUND] Product \"" << productName
                 << "\" is currently unavailable across all warehouses.\n";
        } else {
            cout << "  -- Summary: Product found in " << foundCount << " warehouse(s) --\n";
        }
    }
};

// ----------------------------------------------------------------------
// 5. CONSOLE INTERACTION
// ----------------------------------------------------------------------
void printMenu() {
    cout << "\n  +----------------- MENU -----------------+\n";
    cout << "  | 1. Display Connected Warehouses        |\n";
    cout << "  | 2. Connect Two Warehouses              |\n";
    cout << "  | 3. Add a Shipment to a Warehouse       |\n";
    cout << "  | 4. Search Product in Warehouses (BFS)  |\n";
    cout << "  | 0. Exit                                |\n";
    cout << "  +----------------------------------------+\n";
    cout << "  Enter your choice: ";
}

int main() {
    WarehouseGraph network;

    // Load initial sample network
    CentralHub* hub1 = new CentralHub(1, "Delhi Central Hub", "New Delhi", 5000);
    RegionalWarehouse* wh2 = new RegionalWarehouse(2, "Mumbai Logistics Park", "Mumbai", "West");
    RegionalWarehouse* wh3 = new RegionalWarehouse(3, "Kolkata Hub", "Kolkata", "East");
    RegionalWarehouse* wh4 = new RegionalWarehouse(4, "Chennai Depot", "Chennai", "South");

    network.addWarehouse(hub1);
    network.addWarehouse(wh2);
    network.addWarehouse(wh3);
    network.addWarehouse(wh4);

    // Initial connections (silent load)
    network.connectWarehouses(1, 2, false); // Delhi <-> Mumbai
    network.connectWarehouses(1, 3, false); // Delhi <-> Kolkata
    network.connectWarehouses(2, 4, false); // Mumbai <-> Chennai

    // Initial shipments (silent load)
    network.addShipmentToWarehouse(1, "SKU-101", "Laptop Pro", 120, false);
    network.addShipmentToWarehouse(1, "SKU-102", "Wireless Mouse", 400, false);
    network.addShipmentToWarehouse(2, "SKU-101", "Laptop Pro", 75, false);
    network.addShipmentToWarehouse(2, "SKU-103", "Mechanical Keyboard", 250, false);
    network.addShipmentToWarehouse(3, "SKU-104", "Gaming Monitor", 90, false);
    network.addShipmentToWarehouse(4, "SKU-101", "Laptop Pro", 30, false);
    network.addShipmentToWarehouse(4, "SKU-105", "Noise-Cancelling Headphones", 150, false);

    cout << "\n  +-----------------------------------------------------------+\n";
    cout << "  |  GLOBAL SUPPLY CHAIN - WAREHOUSE NETWORK & DISCOVERY      |\n";
    cout << "  +-----------------------------------------------------------+\n";

    int choice;
    do {
        printMenu();
        cin >> choice;

        switch (choice) {
            case 1:
                network.displayConnectedWarehouses();
                break;

            case 2: {
                int id1, id2;
                cout << "\n  --- Connect Two Warehouses ---\n";
                cout << "  Enter First Warehouse ID: ";
                cin >> id1;
                cout << "  Enter Second Warehouse ID: ";
                cin >> id2;
                network.connectWarehouses(id1, id2);
                break;
            }

            case 3: {
                int whId, qty;
                string sku, name;
                cout << "\n  --- Add a Shipment ---\n";
                cout << "  Target Warehouse ID: ";
                cin >> whId;
                cin.ignore();
                cout << "  Product SKU (e.g. SKU-101): ";
                getline(cin, sku);
                cout << "  Product Name: ";
                getline(cin, name);
                cout << "  Quantity to Add: ";
                cin >> qty;
                network.addShipmentToWarehouse(whId, sku, name, qty);
                break;
            }

            case 4: {
                string productName;
                cout << "\n  --- Search Product Across All Warehouses ---\n";
                cout << "  Enter product name to search: ";
                cin.ignore();
                getline(cin, productName);
                network.searchProductGlobal(productName);
                break;
            }

            case 0:
                cout << "\n  Exiting program. Thank you!\n\n";
                break;

            default:
                cout << "  [Error] Invalid choice. Please try again.\n";
                break;
        }
    } while (choice != 0);

    return 0;
}
