#define INVENTORY_MODULE_NO_MAIN
#include "main.cpp"

#define WAREHOUSE_MODULE_NO_MAIN
#include "warehouse_module.cpp"

#include "supply_chain_module.cpp"
#include "person_d_persistence.h"

#include <ctime>
#include <fstream>
#include <cstring>
using namespace std;

// ===== TIMESTAMP =====
string getTimestamp() {
    time_t now = time(nullptr);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

// ===== AUDIT LOG =====
void auditLog(const string& action, const string& detail) {
    ofstream log("audit.log", ios::app);
    if (log) log << getTimestamp() << " | " << action << " | " << detail << '\n';
}

// ===== SUPPLIER MANAGEMENT =====
struct Supplier {
    int id;
    string name;
    string contact;
    double rating;
    Supplier* next;
    Supplier(int i, const string& n, const string& c, double r)
        : id(i), name(n), contact(c), rating(r), next(nullptr) {}
};

class SupplierManager {
    Supplier* head;
public:
    SupplierManager() : head(nullptr) {}
    ~SupplierManager() {
        while (head) { Supplier* n = head->next; delete head; head = n; }
    }
    bool addSupplier(int id, const string& name, const string& contact, double rating) {
        for (Supplier* s = head; s; s = s->next)
            if (s->id == id) return false;
        Supplier* s = new Supplier(id, name, contact, rating);
        s->next = head; head = s;
        return true;
    }
    bool deleteSupplier(int id) {
        Supplier* cur = head, *prev = nullptr;
        while (cur && cur->id != id) { prev = cur; cur = cur->next; }
        if (!cur) return false;
        if (prev) prev->next = cur->next; else head = cur->next;
        delete cur; return true;
    }
    Supplier* searchSupplier(int id) {
        for (Supplier* s = head; s; s = s->next)
            if (s->id == id) return s;
        return nullptr;
    }
    bool updateSupplier(int id, const string& contact, double rating) {
        Supplier* s = searchSupplier(id);
        if (!s) return false;
        s->contact = contact; s->rating = rating; return true;
    }
    void displayAll() const {
        if (!head) { cout << "No suppliers registered.\n"; return; }
        cout << "\n--- Suppliers ---\n";
        for (Supplier* s = head; s; s = s->next)
            cout << "ID: " << s->id << " | Name: " << s->name
                 << " | Contact: " << s->contact
                 << " | Rating: " << s->rating << "/5\n";
    }
    void saveToFile() const {
        ofstream f("suppliers.txt", ios::trunc);
        for (Supplier* s = head; s; s = s->next)
            f << s->id << '\n' << s->name << '\n' << s->contact << '\n'
              << s->rating << "\n---\n";
    }
    void loadFromFile() {
        ifstream f("suppliers.txt");
        if (!f) return;
        string line;
        while (getline(f, line)) {
            if (line.empty() || line == "---") continue;
            bool isNum = !line.empty();
            for (size_t ci = 0; ci < line.size(); ++ci)
                if (line[ci] < '0' || line[ci] > '9') { isNum = false; break; }
            if (!isNum) continue;
            int id = 0;
            for (size_t ci = 0; ci < line.size(); ++ci) id = id * 10 + (line[ci] - '0');
            string name, contact, ratingStr;
            if (!getline(f, name) || !getline(f, contact) || !getline(f, ratingStr)) break;
            double rating = 0;
            // simple string-to-double without stod to avoid exception handling overhead
            bool neg = false;
            size_t ri = 0;
            if (ri < ratingStr.size() && ratingStr[ri] == '-') { neg = true; ++ri; }
            double intPart = 0, fracPart = 0, fracDiv = 1;
            for (; ri < ratingStr.size() && ratingStr[ri] >= '0' && ratingStr[ri] <= '9'; ++ri)
                intPart = intPart * 10 + (ratingStr[ri] - '0');
            if (ri < ratingStr.size() && ratingStr[ri] == '.') {
                ++ri;
                for (; ri < ratingStr.size() && ratingStr[ri] >= '0' && ratingStr[ri] <= '9'; ++ri) {
                    fracPart = fracPart * 10 + (ratingStr[ri] - '0');
                    fracDiv *= 10;
                }
            }
            rating = intPart + fracPart / fracDiv;
            if (neg) rating = -rating;
            string sep; getline(f, sep);
            addSupplier(id, name, contact, rating);
        }
    }
};

// ===== REPORT GENERATOR =====
void generateReport(const InventoryManager& inventory, const WarehouseManager& warehouses,
                    const SupplyChainManager& supplyChain) {
    string ts = getTimestamp();
    ofstream rpt("report.txt", ios::app);
    rpt << "\n========================================\n";
    rpt << "SUPPLY CHAIN REPORT -- " << ts << "\n";
    rpt << "========================================\n";

    cout << "\n--- Report generated at " << ts << " ---\n";

    // Low-stock alerts
    rpt << "\n[LOW STOCK ALERTS -- qty < 10]\n";
    cout << "[LOW STOCK ALERTS -- qty < 10]\n";
    inventory.forEachProduct([&](const Product& p) {
        if (p.getQuantity() < 10) {
            rpt  << "  ALERT: " << p.getName() << " [ID:" << p.getId()
                 << "] qty=" << p.getQuantity() << '\n';
            cout << "  ALERT: " << p.getName() << " [ID:" << p.getId()
                 << "] qty=" << p.getQuantity() << '\n';
        }
    });

    // Inventory summary
    rpt  << "\n[INVENTORY SUMMARY]\n";
    cout << "[INVENTORY SUMMARY]\n";
    inventory.forEachProduct([&](const Product& p) {
        int stored  = warehouses.storedProductQuantity(p.getId());
        int transit = supplyChain.inTransitProductQuantity(p.getId());
        rpt  << "  " << p.getName() << " [ID:" << p.getId() << "]"
             << " catalog=" << p.getQuantity()
             << " stored=" << stored
             << " in-transit=" << transit << '\n';
        cout << "  " << p.getName() << " [ID:" << p.getId() << "]"
             << " catalog=" << p.getQuantity()
             << " stored=" << stored
             << " in-transit=" << transit << '\n';
    });

    // Shipment summary
    rpt  << "\n[SHIPMENT SUMMARY]\n";
    cout << "[SHIPMENT SUMMARY]\n";
    int sc = supplyChain.shipmentCount();
    rpt  << "  Total shipments: " << sc << '\n';
    cout << "  Total shipments: " << sc << '\n';

    rpt << "\n[END OF REPORT]\n";
    cout << "Report saved to report.txt\n";
    auditLog("REPORT_GENERATED", "report.txt written at " + ts);
}

// ===== SAVE HELPER =====
void saveChanges(const SystemPersistence& persistence, const InventoryManager& inventory,
                 const WarehouseManager& warehouses, const SupplyChainManager& supplyChain) {
    string error;
    if (!persistence.save(inventory, warehouses, supplyChain, error))
        cout << "Could not save changes: " << error
             << "\nChanges are still in memory.\n";
}

bool allocatedStockIsValid(const InventoryManager& inventory,
                           const WarehouseManager& warehouses,
                           const SupplyChainManager& supplyChain) {
    bool valid = true;
    inventory.forEachProduct([&](const Product& product) {
        if (product.getQuantity() < warehouses.storedProductQuantity(product.getId()) +
                                   supplyChain.inTransitProductQuantity(product.getId()))
            valid = false;
    });
    return valid;
}

// ===== INVENTORY MANAGER MENU =====
void runInventoryManager(InventoryManager& inventory, const WarehouseManager& warehouses,
                         const SupplyChainManager& supplyChain,
                         const SystemPersistence& persistence) {
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Inventory Manager ---\n"
             << "1. Add product\n"
             << "2. Delete product\n"
             << "3. Update product stock\n"
             << "4. Display all products\n"
             << "0. Return to main menu\n";
        if (!readInteger("Choice: ", choice)) { if (cin.eof()) return; continue; }

        if (choice == 1) {
            Product product;
            cout << "Enter product as ID|name|price|quantity "
                    "(example: 101|Laptop Pro|899.99|12):\n";
            if (!(cin >> product)) {
                cout << "Invalid product details. Use a positive ID, non-negative "
                        "price/quantity, name without '|'.\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            } else if (inventory.addProduct(product)) {
                cout << "Product added successfully.\n";
                auditLog("PRODUCT_ADDED",
                         "ID=" + to_string(product.getId()) + " name=" + product.getName());
                saveChanges(persistence, inventory, warehouses, supplyChain);
            } else {
                cout << "A product with that ID already exists.\n";
            }
        } else if (choice == 2) {
            int id; if (!readInteger("Product ID to delete: ", id)) continue;
            if (warehouses.containsProduct(id) || supplyChain.usesProduct(id)) {
                cout << "Product is stored or in an active shipment -- cannot delete.\n";
            } else if (inventory.deleteProduct(id)) {
                cout << "Product deleted successfully.\n";
                auditLog("PRODUCT_DELETED", "ID=" + to_string(id));
                saveChanges(persistence, inventory, warehouses, supplyChain);
            } else {
                cout << "Product not found.\n";
            }
        } else if (choice == 3) {
            int id; if (!readInteger("Product ID to update: ", id)) continue;
            Product* product = inventory.searchProduct(id);
            if (!product) { cout << "Product not found.\n"; continue; }
            int quantity;
            if (readInteger("Enter new stock quantity: ", quantity)) {
                if (quantity < 0) {
                    cout << "Stock cannot be negative.\n";
                } else if (quantity < warehouses.storedProductQuantity(id) +
                                      supplyChain.inTransitProductQuantity(id)) {
                    cout << "Stock can't be lower than the amount stored or in transit.\n";
                } else {
                    product->setQuantity(quantity);
                    cout << "Stock updated successfully.\n";
                    auditLog("STOCK_UPDATED",
                             "ID=" + to_string(id) + " new_qty=" + to_string(quantity));
                    saveChanges(persistence, inventory, warehouses, supplyChain);
                }
            }
        } else if (choice == 4) {
            inventory.displayProducts();
        } else if (choice != 0) {
            cout << "Please choose one of the listed options.\n";
        }
    }
}

// ===== WAREHOUSE MANAGER MENU =====
void runWarehouseManager(WarehouseManager& warehouses, InventoryManager& inventory,
                         SupplyChainManager& supplyChain,
                         const SystemPersistence& persistence) {
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Warehouse Manager ---\n"
             << "1. Add warehouse\n"
             << "2. Delete warehouse\n"
             << "3. Display all warehouses\n"
             << "4. Store an inventory product in warehouse\n"
             << "5. Remove product from warehouse\n"
             << "6. Search warehouse by name\n"
             << "7. View sorted inventory of warehouse\n"
             << "8. Global product search (across all warehouses)\n"
             << "9. SKU / Aisle Locator\n"
             << "0. Return to main menu\n";
        if (!readInteger("Choice: ", choice)) { if (cin.eof()) return; continue; }

        if (choice == 1) {
            int id, capacity; string name, location;
            if (!readInteger("Warehouse ID: ", id) ||
                !readText("Warehouse name: ", name) ||
                !readText("Location: ", location) ||
                !readInteger("Max capacity (units): ", capacity)) continue;
            if (id <= 0 || capacity <= 0) {
                cout << "ID and capacity must be > 0.\n";
            } else if (!warehouses.addWarehouse(id, name, location, capacity)) {
                cout << "A warehouse with that ID already exists.\n";
            } else {
                cout << "Warehouse added successfully.\n";
                auditLog("WAREHOUSE_ADDED",
                         "ID=" + to_string(id) + " name=" + name);
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 2) {
            int id; if (!readInteger("Warehouse ID to delete: ", id)) continue;
            if (supplyChain.usesWarehouse(id)) {
                cout << "Remove its routes and complete active shipments before deleting.\n";
            } else if (warehouses.deleteWarehouse(id)) {
                cout << "Warehouse deleted successfully.\n";
                auditLog("WAREHOUSE_DELETED", "ID=" + to_string(id));
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 3) {
            warehouses.displayWarehouses();
        } else if (choice == 4) {
            int warehouseId, productId, quantity;
            if (!readInteger("Warehouse ID: ", warehouseId)) continue;
            Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
            if (!warehouse) { cout << "Warehouse not found.\n"; continue; }
            if (!readInteger("Inventory product ID: ", productId)) continue;
            Product* product = inventory.searchProduct(productId);
            if (!product) {
                cout << "Add this product in Inventory Manager first.\n"; continue;
            }
            if (!readInteger("Quantity to store: ", quantity)) continue;
            if (quantity <= 0) {
                cout << "Enter a quantity > 0.\n";
            } else if (quantity > product->getQuantity() -
                                  warehouses.storedProductQuantity(productId) -
                                  supplyChain.inTransitProductQuantity(productId)) {
                cout << "Not enough stock available after counting items stored or in transit.\n";
            } else {
                int prev = warehouse->productQuantity(productId);
                warehouse->addProduct(productId, product->getName(), quantity);
                if (warehouse->productQuantity(productId) == prev + quantity) {
                    auditLog("STOCK_STORED",
                             "product=" + to_string(productId) +
                             " warehouse=" + to_string(warehouseId) +
                             " qty=" + to_string(quantity));
                    saveChanges(persistence, inventory, warehouses, supplyChain);
                }
            }
        } else if (choice == 5) {
            int warehouseId, productId, quantity;
            if (!readInteger("Warehouse ID: ", warehouseId)) continue;
            Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
            if (!warehouse) { cout << "Warehouse not found.\n"; continue; }
            if (!readInteger("Product ID: ", productId) ||
                !readInteger("Quantity to remove: ", quantity)) continue;
            if (warehouse->removeProduct(productId, quantity)) {
                auditLog("STOCK_REMOVED",
                         "product=" + to_string(productId) +
                         " warehouse=" + to_string(warehouseId) +
                         " qty=" + to_string(quantity));
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 6) {
            string keyword;
            if (!readText("Search keyword (warehouse name): ", keyword)) continue;
            warehouses.searchByName(keyword);
        } else if (choice == 7) {
            int id; if (!readInteger("Warehouse ID: ", id)) continue;
            warehouses.displaySortedInventoryForWarehouse(id);
        } else if (choice == 8) {
            int productId;
            if (!readInteger("Product ID to search globally: ", productId)) continue;
            Product* p = inventory.searchProduct(productId);
            if (!p) { cout << "Product not found in catalog.\n"; continue; }
            warehouses.searchProductAcrossWarehouses(productId, p->getName());
        } else if (choice == 9) {
            int warehouseId, productId;
            if (!readInteger("Warehouse ID: ", warehouseId) ||
                !readInteger("Product ID: ", productId)) continue;
            supplyChain.skuLocator(warehouseId, productId);
        } else if (choice != 0) {
            cout << "Please choose one of the listed options.\n";
        }
    }
}

// ===== SUPPLY CHAIN MANAGER MENU =====
void runSupplyChainManager(SupplyChainManager& supplyChain, const InventoryManager& inventory,
                           const WarehouseManager& warehouses,
                           const SystemPersistence& persistence) {
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Supply Chain Manager ---\n"
             << "1.  Add route between warehouses\n"
             << "2.  Remove route between warehouses\n"
             << "3.  Display all routes\n"
             << "4.  Find route between two warehouses\n"
             << "5.  Create shipment\n"
             << "6.  Track a shipment\n"
             << "7.  Advance shipment one step\n"
             << "8.  Mark shipment as Delayed\n"
             << "9.  Resume delayed shipment\n"
             << "10. Display all shipments\n"
             << "0.  Return to main menu\n";
        if (!readInteger("Choice: ", choice)) { if (cin.eof()) return; continue; }

        if (choice == 1) {
            int from, to;
            if (!readInteger("Origin warehouse ID: ", from) ||
                !readInteger("Destination warehouse ID: ", to)) continue;
            if (supplyChain.addRoute(from, to)) {
                auditLog("ROUTE_ADDED",
                         "from=" + to_string(from) + " to=" + to_string(to));
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 2) {
            int from, to;
            if (!readInteger("Origin warehouse ID: ", from) ||
                !readInteger("Destination warehouse ID: ", to)) continue;
            if (supplyChain.removeRoute(from, to)) {
                auditLog("ROUTE_REMOVED",
                         "from=" + to_string(from) + " to=" + to_string(to));
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 3) {
            supplyChain.displayRoutes();
        } else if (choice == 4) {
            int from, to;
            if (!readInteger("Origin warehouse ID: ", from) ||
                !readInteger("Destination warehouse ID: ", to)) continue;
            supplyChain.findAndDisplayRoute(from, to);
        } else if (choice == 5) {
            int originId, destinationId, productId, quantity;
            if (!readInteger("Origin warehouse ID: ", originId) ||
                !readInteger("Destination warehouse ID: ", destinationId) ||
                !readInteger("Product ID: ", productId) ||
                !readInteger("Shipment quantity: ", quantity)) continue;
            if (supplyChain.createShipment(originId, destinationId, productId, quantity)) {
                auditLog("SHIPMENT_CREATED",
                         "from=" + to_string(originId) +
                         " to=" + to_string(destinationId) +
                         " product=" + to_string(productId) +
                         " qty=" + to_string(quantity));
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 6) {
            int id; if (!readInteger("Shipment ID: ", id)) continue;
            supplyChain.displayShipment(id);
        } else if (choice == 7) {
            int id; if (!readInteger("Shipment ID to advance: ", id)) continue;
            if (supplyChain.advanceShipment(id)) {
                auditLog("SHIPMENT_ADVANCED", "id=" + to_string(id));
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 8) {
            int id; if (!readInteger("Shipment ID to mark delayed: ", id)) continue;
            if (supplyChain.updateShipmentStatus(id, true)) {
                auditLog("SHIPMENT_DELAYED", "id=" + to_string(id));
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 9) {
            int id; if (!readInteger("Shipment ID to resume: ", id)) continue;
            if (supplyChain.updateShipmentStatus(id, false)) {
                auditLog("SHIPMENT_RESUMED", "id=" + to_string(id));
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 10) {
            supplyChain.displayShipments();
        } else if (choice != 0) {
            cout << "Please choose one of the listed options.\n";
        }
    }
}

// ===== SUPPLIER MANAGER MENU =====
void runSupplierManager(SupplierManager& suppliers) {
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Supplier Manager ---\n"
             << "1. Add supplier\n"
             << "2. Delete supplier\n"
             << "3. Update supplier\n"
             << "4. Search supplier by ID\n"
             << "5. Display all suppliers\n"
             << "0. Return to main menu\n";
        if (!readInteger("Choice: ", choice)) { if (cin.eof()) return; continue; }

        if (choice == 1) {
            int id; string name, contact; double rating;
            if (!readInteger("Supplier ID: ", id)) continue;
            if (!readText("Supplier name: ", name) ||
                !readText("Contact info: ", contact)) continue;
            cout << "Rating (0-5): ";
            if (!(cin >> rating)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            if (rating < 0) rating = 0;
            if (rating > 5) rating = 5;
            if (suppliers.addSupplier(id, name, contact, rating)) {
                cout << "Supplier added.\n";
                auditLog("SUPPLIER_ADDED",
                         "ID=" + to_string(id) + " name=" + name);
                suppliers.saveToFile();
            } else {
                cout << "Supplier ID already exists.\n";
            }
        } else if (choice == 2) {
            int id; if (!readInteger("Supplier ID to delete: ", id)) continue;
            if (suppliers.deleteSupplier(id)) {
                cout << "Supplier deleted.\n";
                auditLog("SUPPLIER_DELETED", "ID=" + to_string(id));
                suppliers.saveToFile();
            } else {
                cout << "Supplier not found.\n";
            }
        } else if (choice == 3) {
            int id; if (!readInteger("Supplier ID to update: ", id)) continue;
            string contact; double rating;
            if (!readText("New contact info: ", contact)) continue;
            cout << "New rating (0-5): ";
            if (!(cin >> rating)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            if (rating < 0) rating = 0;
            if (rating > 5) rating = 5;
            if (suppliers.updateSupplier(id, contact, rating)) {
                cout << "Supplier updated.\n";
                auditLog("SUPPLIER_UPDATED", "ID=" + to_string(id));
                suppliers.saveToFile();
            } else {
                cout << "Supplier not found.\n";
            }
        } else if (choice == 4) {
            int id; if (!readInteger("Supplier ID to search: ", id)) continue;
            Supplier* s = suppliers.searchSupplier(id);
            if (s)
                cout << "ID: " << s->id << " | Name: " << s->name
                     << " | Contact: " << s->contact
                     << " | Rating: " << s->rating << "/5\n";
            else
                cout << "Supplier not found.\n";
        } else if (choice == 5) {
            suppliers.displayAll();
        } else if (choice != 0) {
            cout << "Please choose one of the listed options.\n";
        }
    }
}

// ===== MAIN SYSTEM =====
int runSystem() {
    InventoryManager inventory;
    WarehouseManager warehouses;
    SupplyChainManager supplyChain(warehouses, inventory);
    SystemPersistence persistence;
    SupplierManager suppliers;

    suppliers.loadFromFile();

    string error;
    if (!persistence.load(inventory, warehouses, supplyChain, error)) {
        cerr << "Startup error: " << error << '\n';
        return 1;
    }
    if (!allocatedStockIsValid(inventory, warehouses, supplyChain)) {
        cerr << "Startup error: saved inventory stock is less than amount stored or in transit.\n";
        return 1;
    }

    auditLog("SYSTEM_STARTED", "Application started");

    int choice = -1;
    while (choice != 0) {
        cout << "\n===========================================\n"
             << " Global Supply Chain & Warehouse Hub\n"
             << "===========================================\n"
             << "1. Inventory Manager\n"
             << "2. Warehouse Manager\n"
             << "3. Supply Chain Manager\n"
             << "4. Supplier Manager\n"
             << "5. Reports & Analytics\n"
             << "6. Display all (inventory + warehouses + routes + shipments)\n"
             << "0. Save & Exit\n";
        if (!readInteger("Choice: ", choice)) { if (cin.eof()) break; continue; }

        if (choice == 1) {
            runInventoryManager(inventory, warehouses, supplyChain, persistence);
        } else if (choice == 2) {
            runWarehouseManager(warehouses, inventory, supplyChain, persistence);
        } else if (choice == 3) {
            runSupplyChainManager(supplyChain, inventory, warehouses, persistence);
        } else if (choice == 4) {
            runSupplierManager(suppliers);
        } else if (choice == 5) {
            generateReport(inventory, warehouses, supplyChain);
        } else if (choice == 6) {
            inventory.displayProducts();
            warehouses.displayWarehouses();
            supplyChain.displayRoutes();
            supplyChain.displayShipments();
        } else if (choice == 0) {
            string err;
            if (persistence.save(inventory, warehouses, supplyChain, err))
                cout << "Data saved. Goodbye.\n";
            else
                cerr << "Save error: " << err << '\n';
            auditLog("SYSTEM_SHUTDOWN", "Application exited");
        } else {
            cout << "Please choose one of the listed options.\n";
        }
    }
    return 0;
}

int main() {
    try {
        return runSystem();
    } catch (const exception& error) {
        cerr << "Fatal application error: " << error.what() << '\n';
        return 1;
    }
}
