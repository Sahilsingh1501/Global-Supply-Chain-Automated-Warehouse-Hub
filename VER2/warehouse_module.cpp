#ifndef INVENTORY_MODULE_NO_MAIN
#define INVENTORY_MODULE_NO_MAIN
#include "main.cpp"
#endif

#include <iostream>
#include <limits>
#include <string>
#include "person_d_support.h"
using namespace std;
struct StoredProduct {
    int id;
    string name;
    int quantity;
    StoredProduct* next;

    StoredProduct(int productId, const string& productName, int stock)
        : id(productId), name(productName), quantity(stock), next(nullptr) {}
};
class Warehouse {
private:
    int id;
    string name;
    string location;
    int capacity;
    int usedCapacity;
    StoredProduct* products;
public:
    Warehouse()
        : id(0), capacity(0), usedCapacity(0), products(nullptr) {}
    Warehouse(int warehouseId, const string& warehouseName,
              const string& warehouseLocation, int maxCapacity)
        : id(warehouseId), name(warehouseName), location(warehouseLocation),
          capacity(maxCapacity), usedCapacity(0), products(nullptr) {}
    ~Warehouse() {
        clearProducts();
    }
    Warehouse(const Warehouse&) = delete;
    Warehouse& operator=(const Warehouse&) = delete;
    int getId() const { return id; }
    const string& getName() const { return name; }
    const string& getLocation() const { return location; }
    int getCapacity() const { return capacity; }
    int getUsedCapacity() const { return usedCapacity; }
    bool isEmpty() const { return products == nullptr; }
    void swap(Warehouse& other) {
        using std::swap;
        swap(id, other.id);
        swap(name, other.name);
        swap(location, other.location);
        swap(capacity, other.capacity);
        swap(usedCapacity, other.usedCapacity);
        swap(products, other.products);
    }
    void clearProducts() {
        while (products) {
            StoredProduct* next = products->next;
            delete products;
            products = next;
        }
        usedCapacity = 0;
    }
    int productQuantity(int productId) const {
        for (StoredProduct* current = products; current; current = current->next) {
            if (current->id == productId) return current->quantity;
        }
        return 0;
    }
    size_t storedProductCount() const {
        size_t count = 0;
        for (StoredProduct* current = products; current; current = current->next) ++count;
        return count;
    }
    void saveState(ostream& output) const {
        output << id << ' ' << capacity << '\n';
        PersonD::writeString(output, name);
        PersonD::writeString(output, location);
        output << storedProductCount() << '\n';
        for (StoredProduct* current = products; current; current = current->next) {
            output << current->id << ' ' << current->quantity << '\n';
            PersonD::writeString(output, current->name);
        }
    }

    bool canStoreProduct(int productId, const string& productName, int quantity) const {
        for (StoredProduct* current = products; current; current = current->next) {
            if (current->id == productId && current->name != productName) return false;
        }
        return quantity > 0 && quantity <= capacity - usedCapacity;
    }

    bool receiveProduct(int productId, const string& productName, int quantity) {
        if (quantity <= 0) return false;
        for (StoredProduct* current = products; current; current = current->next) {
            if (current->id == productId) {
                if (current->name != productName) {
                    return false;
                }
                if (quantity > capacity - usedCapacity) {
                    return false;
                }
                current->quantity += quantity;
                usedCapacity += quantity;
                return true;
            }
        }
        if (quantity > capacity - usedCapacity) {
            return false;
        }
        StoredProduct* product = new StoredProduct(productId, productName, quantity);
        product->next = products;
        products = product;
        usedCapacity += quantity;
        return true;
    }

    void addProduct(int productId, const string& productName, int quantity) {
        if (quantity <= 0) {
            cout << "Quantity must be greater than zero.\n";
        } else if (!receiveProduct(productId, productName, quantity)) {
            if (productQuantity(productId) > 0) {
                cout << "Not enough capacity or product name does not match the stored ID.\n";
            } else {
                cout << "Not enough capacity. Available space: "
                     << capacity - usedCapacity << " units.\n";
            }
        } else {
            cout << "Product stored successfully.\n";
        }
    }

    bool removeProduct(int productId, int quantity) {
        if (quantity <= 0) return false;
        StoredProduct* current = products;
        StoredProduct* previous = nullptr;
        while (current && current->id != productId) {
            previous = current;
            current = current->next;
        }
        if (!current) {
            cout << "Product is not stored in this warehouse.\n";
            return false;
        }
        if (quantity > current->quantity) {
            cout << "Cannot remove more units than are currently stored.\n";
            return false;
        }
        current->quantity -= quantity;
        usedCapacity -= quantity;
        if (current->quantity == 0) {
            if (previous) {
                previous->next = current->next;
            } else {
                products = current->next;
            }
            delete current;
        }
        cout << "Product quantity removed; warehouse capacity updated.\n";
        return true;
    }
    void display() const {
        cout << "Warehouse ID: " << id
             << " | Name: " << name
             << " | Location: " << location
             << " | Capacity: " << usedCapacity << " / " << capacity
             << " units used\n";
        cout << "  Products:\n";
        if (!products) {
            cout << "    (none)\n";
            return;
        }
        for (StoredProduct* current = products; current; current = current->next) {
            cout << "    Product ID: " << current->id
                 << " | Name: " << current->name
                 << " | Quantity: " << current->quantity << '\n';
        }
    }
};
struct WarehouseNode {
    Warehouse warehouse;
    WarehouseNode* left;
    WarehouseNode* right;
    WarehouseNode(int id, const string& name, const string& location, int capacity)
        : warehouse(id, name, location, capacity), left(nullptr), right(nullptr) {}
};
class WarehouseManager {
private:
    WarehouseNode* root;
    WarehouseNode* findNode(int id) const {
        WarehouseNode* current = root;
        while (current) {
            if (id == current->warehouse.getId()) return current;
            current = id < current->warehouse.getId() ? current->left : current->right;
        }
        return nullptr;
    }
    WarehouseNode* insert(WarehouseNode* node, int id, const string& name,
                          const string& location, int capacity, bool& added) {
        if (!node) {
            added = true;
            return new WarehouseNode(id, name, location, capacity);
        }
        if (id < node->warehouse.getId()) {
            node->left = insert(node->left, id, name, location, capacity, added);
        } else if (id > node->warehouse.getId()) {
            node->right = insert(node->right, id, name, location, capacity, added);
        }
        return node;
    }
    WarehouseNode* minimumNode(WarehouseNode* node) const {
        while (node && node->left) {
            node = node->left;
        }
        return node;
    }
    WarehouseNode* removeNode(WarehouseNode* node, int id, bool& removed) {
        if (!node) return nullptr;
        if (id < node->warehouse.getId()) {
            node->left = removeNode(node->left, id, removed);
        } else if (id > node->warehouse.getId()) {
            node->right = removeNode(node->right, id, removed);
        } else {
            removed = true;
            if (!node->left || !node->right) {
                WarehouseNode* child = node->left ? node->left : node->right;
                delete node;
                return child;
            }
            WarehouseNode* successor = minimumNode(node->right);
            node->warehouse.swap(successor->warehouse);
            bool successorRemoved = false;
            node->right = removeNode(node->right, successor->warehouse.getId(), successorRemoved);
        }
        return node;
    }
    void destroy(WarehouseNode* node) {
        if (!node) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

    bool containsProduct(WarehouseNode* node, int productId) const {
        if (!node) return false;
        return node->warehouse.productQuantity(productId) > 0 ||
               containsProduct(node->left, productId) ||
               containsProduct(node->right, productId);
    }

    int storedProductQuantity(WarehouseNode* node, int productId) const {
        if (!node) return 0;
        return node->warehouse.productQuantity(productId) +
               storedProductQuantity(node->left, productId) +
               storedProductQuantity(node->right, productId);
    }
    size_t warehouseCount(WarehouseNode* node) const {
        return node ? 1 + warehouseCount(node->left) + warehouseCount(node->right) : 0;
    }
    long long totalCapacity(WarehouseNode* node) const {
        return node ? node->warehouse.getCapacity() +
                      totalCapacity(node->left) + totalCapacity(node->right) : 0;
    }
    long long totalUsedCapacity(WarehouseNode* node) const {
        return node ? node->warehouse.getUsedCapacity() +
                      totalUsedCapacity(node->left) + totalUsedCapacity(node->right) : 0;
    }
    void saveStateInOrder(WarehouseNode* node, ostream& output) const {
        if (!node) return;
        saveStateInOrder(node->left, output);
        node->warehouse.saveState(output);
        saveStateInOrder(node->right, output);
    }
    void displayInOrder(WarehouseNode* node) const {
        if (!node) return;
        displayInOrder(node->left);
        node->warehouse.display();
        displayInOrder(node->right);
    }
    void displayCapacityInOrder(WarehouseNode* node) const {
        if (!node) return;
        displayCapacityInOrder(node->left);
        cout << "ID: " << node->warehouse.getId()
             << " | Capacity: " << node->warehouse.getUsedCapacity()
             << " / " << node->warehouse.getCapacity() << " units\n";
        displayCapacityInOrder(node->right);
    }
public:
    WarehouseManager() : root(nullptr) {}
    ~WarehouseManager() { destroy(root); }
    WarehouseManager(const WarehouseManager&) = delete;
    WarehouseManager& operator=(const WarehouseManager&) = delete;
    bool addWarehouse(int id, const string& name, const string& location, int capacity) {
        bool added = false;
        root = insert(root, id, name, location, capacity, added);
        return added;
    }
    size_t warehouseCount() const { return warehouseCount(root); }
    long long totalCapacity() const { return totalCapacity(root); }
    long long totalUsedCapacity() const { return totalUsedCapacity(root); }
    void saveState(ostream& output) const {
        output << warehouseCount() << '\n';
        saveStateInOrder(root, output);
    }
    bool loadState(istream& input, const InventoryManager& inventory) {
        size_t count;
        if (!PersonD::readCount(input, count)) return false;
        for (size_t i = 0; i < count; ++i) {
            int id, capacity;
            string name, location;
            size_t productCount;
            if (!(input >> id >> capacity) || id <= 0 || capacity <= 0 ||
                !PersonD::readString(input, name) || name.empty() ||
                !PersonD::readString(input, location) || location.empty() ||
                !PersonD::readCount(input, productCount) ||
                !addWarehouse(id, name, location, capacity)) return false;
            Warehouse* warehouse = searchWarehouse(id);
            for (size_t p = 0; p < productCount; ++p) {
                int productId, quantity;
                string productName;
                if (!(input >> productId >> quantity) || productId <= 0 || quantity <= 0 ||
                    !PersonD::readString(input, productName)) return false;
                const Product* catalogProduct = inventory.searchProduct(productId);
                if (!catalogProduct || catalogProduct->getName() != productName ||
                    !warehouse->receiveProduct(productId, productName, quantity)) return false;
            }
        }
        return static_cast<bool>(input);
    }
    bool deleteWarehouse(int id) {
        WarehouseNode* target = findNode(id);
        if (!target) {
            cout << "Warehouse not found.\n";
            return false;
        }
        if (!target->warehouse.isEmpty()) {
            cout << "Warehouse contains products. Remove its products before deleting it.\n";
            return false;
        }
        bool removed = false;
        root = removeNode(root, id, removed);
        return removed;
    }
    Warehouse* searchWarehouse(int id) {
        WarehouseNode* node = findNode(id);
        return node ? &node->warehouse : nullptr;
    }
    bool containsProduct(int productId) const {
        return containsProduct(root, productId);
    }
    int storedProductQuantity(int productId) const {
        return storedProductQuantity(root, productId);
    }
    void displayWarehouses() const {
        if (!root) {
            cout << "No warehouses have been added.\n";
            return;
        }
        cout << "\n--- Warehouses (BST, ordered by ID) ---\n";
        displayInOrder(root);
    }
    void displayCapacityReport() const {
        if (!root) {
            cout << "No warehouses have been added.\n";
            return;
        }
        cout << "\n--- Warehouse Capacity ---\n";
        displayCapacityInOrder(root);
    }
};
bool warehouseReadInteger(const string& prompt, int& value) {
    cout << prompt;
    if (cin >> value) {
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return true;
    }
    if (cin.eof()) return false;
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "Invalid input. Please enter a whole number.\n";
    return false;
}
bool readText(const string& prompt, string& value) {
    cout << prompt;
    if (!getline(cin, value)) return false;
    if (!value.empty()) return true;
    cout << "Value cannot be empty.\n";
    return false;
}
#ifndef WAREHOUSE_MODULE_NO_MAIN
int main() {
    WarehouseManager manager;
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Warehouse Manager (Person B) ---\n"
             << "1. Add warehouse\n"
             << "2. Delete warehouse\n"
             << "3. Search warehouse\n"
             << "4. Display all warehouses\n"
             << "5. Store product in warehouse\n"
             << "6. Remove product quantity from warehouse\n"
             << "7. Display capacity report\n"
             << "0. Exit\n";
        if (!warehouseReadInteger("Choice: ", choice)) {
            if (cin.eof()) break;
            continue;
        }
        if (choice == 1) {
            int id;
            int capacity;
            string name;
            string location;
            if (!warehouseReadInteger("Warehouse ID: ", id) ||
                !readText("Warehouse name: ", name) ||
                !readText("Location: ", location) ||
                !warehouseReadInteger("Maximum capacity (units): ", capacity)) {
                continue;
            }
            if (id <= 0 || capacity <= 0) {
                cout << "Warehouse ID and capacity must be greater than zero.\n";
            } else if (!manager.addWarehouse(id, name, location, capacity)) {
                cout << "A warehouse with that ID already exists.\n";
            } else {
                cout << "Warehouse added successfully.\n";
            }
        } else if (choice == 2) {
            int id;
            if (warehouseReadInteger("Warehouse ID to delete: ", id) &&
                manager.deleteWarehouse(id)) {
                cout << "Warehouse deleted successfully.\n";
            }
        } else if (choice == 3) {
            int id;
            if (!warehouseReadInteger("Warehouse ID to search: ", id)) continue;
            Warehouse* warehouse = manager.searchWarehouse(id);
            if (warehouse) {
                warehouse->display();
            } else {
                cout << "Warehouse not found.\n";
            }
        } else if (choice == 4) {
            manager.displayWarehouses();
        } else if (choice == 5) {
            int warehouseId;
            int productId;
            int quantity;
            string productName;
            if (!warehouseReadInteger("Warehouse ID: ", warehouseId)) continue;
            Warehouse* warehouse = manager.searchWarehouse(warehouseId);
            if (!warehouse) {
                cout << "Warehouse not found.\n";
                continue;
            }
            if (!warehouseReadInteger("Product ID: ", productId) ||
                !readText("Product name: ", productName) ||
                !warehouseReadInteger("Quantity to store: ", quantity)) {
                continue;
            }
            if (productId <= 0 || quantity <= 0) {
                cout << "Product ID and quantity must be greater than zero.\n";
            } else {
                warehouse->addProduct(productId, productName, quantity);
            }
        } else if (choice == 6) {
            int warehouseId;
            int productId;
            int quantity;
            if (!warehouseReadInteger("Warehouse ID: ", warehouseId) ||
                !warehouseReadInteger("Product ID: ", productId) ||
                !warehouseReadInteger("Quantity to remove: ", quantity)) {
                continue;
            }
            Warehouse* warehouse = manager.searchWarehouse(warehouseId);
            if (!warehouse) {
                cout << "Warehouse not found.\n";
            } else if (productId <= 0 || quantity <= 0) {
                cout << "Product ID and quantity must be greater than zero.\n";
            } else {
                warehouse->removeProduct(productId, quantity);
            }
        } else if (choice == 7) {
            manager.displayCapacityReport();
        } else if (choice == 0) {
            cout << "Warehouse manager closed. Data is currently in memory only.\n";
        } else {
            cout << "Invalid choice. Select an option from the menu.\n";
        }
    }
    return 0;
}
#endif
