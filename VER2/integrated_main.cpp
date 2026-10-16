#define INVENTORY_MODULE_NO_MAIN
#include "main.cpp"

#define WAREHOUSE_MODULE_NO_MAIN
#include "warehouse_module.cpp"

#include "supply_chain_module.cpp"
#include "person_d_persistence.h"

void recordChange(PersonD::AuditLog& audit, const SystemPersistence& persistence,
                  const InventoryManager& inventory, const WarehouseManager& warehouses,
                  const SupplyChainManager& supplyChain, const string& action,
                  const string& details) {
    if (!audit.record(action, details))
        cout << "Warning: this change was not recorded in the audit log.\n";
    string error;
    if (!persistence.save(inventory, warehouses, supplyChain, error))
        cout << "Save error: " << error << "\nThe change remains in memory for this run.\n";
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

void displaySystemReport(const InventoryManager& inventory,
                         const WarehouseManager& warehouses,
                         const SupplyChainManager& supplyChain) {
    ostringstream stockValue;
    stockValue << fixed << setprecision(2) << inventory.totalStockValue();
    cout << "\n--- System Operations Report ---\n"
         << "Catalog products: " << inventory.productCount() << '\n'
         << "Catalog stock value: $" << stockValue.str() << '\n'
         << "Warehouses: " << warehouses.warehouseCount() << '\n'
         << "Warehouse capacity used: " << warehouses.totalUsedCapacity()
         << " / " << warehouses.totalCapacity() << " units\n"
         << "Active routes: " << supplyChain.routeCount() << '\n'
         << "Shipments: " << supplyChain.shipmentCount()
         << " (in transit: " << supplyChain.shipmentCount("In Transit")
         << ", delayed: " << supplyChain.shipmentCount("Delayed")
         << ", delivered: " << supplyChain.shipmentCount("Delivered") << ")\n";
}

void runInventoryManager(InventoryManager& inventory, const WarehouseManager& warehouses,
                         const SupplyChainManager& supplyChain, PersonD::AuditLog& audit,
                         const SystemPersistence& persistence) {
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Inventory Manager (Person A) ---\n"
             << "1. Add product\n"
             << "2. Delete product\n"
             << "3. Update product stock\n"
             << "4. Search product by ID\n"
             << "5. Display all products\n"
             << "6. Display low-stock products\n"
             << "0. Return to main menu\n";
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) return;
            continue;
        }

        if (choice == 1) {
            Product product;
            cout << "Enter product as ID|name|price|quantity (example: 101|Laptop Pro|899.99|12):\n";
            if (!(cin >> product)) {
                cout << "Invalid product. Use a positive ID, non-negative price and stock, "
                        "and a non-empty name without '|'.\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            } else if (inventory.addProduct(product)) {
                cout << "Product added successfully.\n";
                recordChange(audit, persistence, inventory, warehouses, supplyChain,
                             "PRODUCT_ADDED", "Product ID " + to_string(product.getId()));
            } else {
                cout << "That product ID already exists.\n";
            }
        } else if (choice == 2) {
            int id;
            if (!readInteger("Product ID to delete: ", id)) continue;
            if (warehouses.containsProduct(id) || supplyChain.usesProduct(id)) {
                cout << "Cannot delete a product that is stored or in an active shipment.\n";
            } else if (inventory.deleteProduct(id)) {
                cout << "Product deleted successfully.\n";
                recordChange(audit, persistence, inventory, warehouses, supplyChain,
                             "PRODUCT_DELETED", "Product ID " + to_string(id));
            } else {
                cout << "Product not found.\n";
            }
        } else if (choice == 3) {
            int id;
            if (!readInteger("Product ID to update: ", id)) continue;
            Product* product = inventory.searchProduct(id);
            if (!product) {
                cout << "Product not found.\n";
                continue;
            }
            int quantity;
            if (readInteger("Enter new stock quantity: ", quantity)) {
                if (quantity < 0) {
                    cout << "Stock cannot be negative.\n";
                } else if (quantity < warehouses.storedProductQuantity(id) +
                                      supplyChain.inTransitProductQuantity(id)) {
                    cout << "Stock cannot be less than the quantity stored or in transit.\n";
                } else {
                    product->setQuantity(quantity);
                    cout << "Stock updated successfully.\n";
                    recordChange(audit, persistence, inventory, warehouses, supplyChain,
                                 "PRODUCT_STOCK_UPDATED",
                                 "Product ID " + to_string(id) + ", quantity " +
                                 to_string(quantity));
                }
            }
        } else if (choice == 4) {
            int id;
            if (!readInteger("Product ID to search: ", id)) continue;
            Product* product = inventory.searchProduct(id);
            if (product) cout << *product << '\n';
            else cout << "Product not found.\n";
        } else if (choice == 5) {
            inventory.displayProducts();
        } else if (choice == 6) {
            int threshold;
            if (readInteger("Show products with stock below: ", threshold)) {
                if (threshold < 0) cout << "Threshold cannot be negative.\n";
                else inventory.displayLowStock(threshold);
            }
        } else if (choice != 0) {
            cout << "Invalid choice. Select an option from the menu.\n";
        }
    }
}

void runWarehouseManager(WarehouseManager& warehouses, InventoryManager& inventory,
                         SupplyChainManager& supplyChain, PersonD::AuditLog& audit,
                         const SystemPersistence& persistence) {
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Warehouse Manager (Person B) ---\n"
             << "1. Add warehouse\n"
             << "2. Delete warehouse\n"
             << "3. Search warehouse\n"
             << "4. Display all warehouses\n"
             << "5. Store a catalog product\n"
             << "6. Remove product quantity\n"
             << "7. Display capacity report\n"
             << "0. Return to main menu\n";
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) return;
            continue;
        }

        if (choice == 1) {
            int id;
            int capacity;
            string name;
            string location;
            if (!readInteger("Warehouse ID: ", id) ||
                !readText("Warehouse name: ", name) ||
                !readText("Location: ", location) ||
                !readInteger("Maximum capacity (units): ", capacity)) continue;
            if (id <= 0 || capacity <= 0) {
                cout << "Warehouse ID and capacity must be greater than zero.\n";
            } else if (!warehouses.addWarehouse(id, name, location, capacity)) {
                cout << "A warehouse with that ID already exists.\n";
            } else {
                cout << "Warehouse added successfully.\n";
                recordChange(audit, persistence, inventory, warehouses, supplyChain,
                             "WAREHOUSE_ADDED",
                             "Warehouse ID " + to_string(id) + " (" + name + ")");
            }
        } else if (choice == 2) {
            int id;
            if (!readInteger("Warehouse ID to delete: ", id)) continue;
            if (supplyChain.usesWarehouse(id)) {
                cout << "Remove its routes and finish its active shipments before deleting it.\n";
            } else if (warehouses.deleteWarehouse(id)) {
                cout << "Warehouse deleted successfully.\n";
                recordChange(audit, persistence, inventory, warehouses, supplyChain,
                             "WAREHOUSE_DELETED", "Warehouse ID " + to_string(id));
            }
        } else if (choice == 3) {
            int id;
            if (!readInteger("Warehouse ID to search: ", id)) continue;
            Warehouse* warehouse = warehouses.searchWarehouse(id);
            if (warehouse) warehouse->display();
            else cout << "Warehouse not found.\n";
        } else if (choice == 4) {
            warehouses.displayWarehouses();
        } else if (choice == 5) {
            int warehouseId;
            int productId;
            int quantity;
            if (!readInteger("Warehouse ID: ", warehouseId)) continue;
            Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
            if (!warehouse) {
                cout << "Warehouse not found.\n";
                continue;
            }
            if (!readInteger("Catalog product ID: ", productId)) continue;
            Product* product = inventory.searchProduct(productId);
            if (!product) {
                cout << "Add the product to the inventory catalog first.\n";
                continue;
            }
            if (!readInteger("Quantity to store: ", quantity)) continue;
            if (quantity <= 0) {
                cout << "Quantity must be greater than zero.\n";
            } else if (quantity > product->getQuantity() -
                                  warehouses.storedProductQuantity(productId) -
                                  supplyChain.inTransitProductQuantity(productId)) {
                cout << "The requested quantity exceeds total catalog stock not already stored or in transit.\n";
            } else {
                int previousQuantity = warehouse->productQuantity(productId);
                warehouse->addProduct(productId, product->getName(), quantity);
                if (warehouse->productQuantity(productId) == previousQuantity + quantity)
                    recordChange(audit, persistence, inventory, warehouses, supplyChain,
                                 "WAREHOUSE_STOCK_ADDED",
                                 "Warehouse ID " + to_string(warehouseId) +
                                 ", product ID " + to_string(productId) +
                                 ", quantity " + to_string(quantity));
            }
        } else if (choice == 6) {
            int warehouseId;
            int productId;
            int quantity;
            if (!readInteger("Warehouse ID: ", warehouseId) ||
                !readInteger("Product ID: ", productId) ||
                !readInteger("Quantity to remove: ", quantity)) continue;
            Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
            if (!warehouse) cout << "Warehouse not found.\n";
            else if (quantity <= 0) cout << "Quantity must be greater than zero.\n";
            else if (warehouse->removeProduct(productId, quantity))
                recordChange(audit, persistence, inventory, warehouses, supplyChain,
                             "WAREHOUSE_STOCK_REMOVED",
                             "Warehouse ID " + to_string(warehouseId) +
                             ", product ID " + to_string(productId) +
                             ", quantity " + to_string(quantity));
        } else if (choice == 7) {
            warehouses.displayCapacityReport();
        } else if (choice != 0) {
            cout << "Invalid choice. Select an option from the menu.\n";
        }
    }
}

void runSupplyChainManager(SupplyChainManager& supplyChain, const InventoryManager& inventory,
                           const WarehouseManager& warehouses, PersonD::AuditLog& audit,
                           const SystemPersistence& persistence) {
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Supply Chain Manager (Person C) ---\n"
             << "1. Add route\n"
             << "2. Remove route\n"
             << "3. Display connected warehouses\n"
             << "4. Find route between warehouses\n"
             << "5. Create shipment\n"
             << "6. Move shipment to next warehouse\n"
             << "7. Mark shipment delayed\n"
             << "8. Resume shipment\n"
             << "9. Track one shipment\n"
             << "10. Display all shipments\n"
             << "0. Return to main menu\n";
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) return;
            continue;
        }

        if (choice == 1 || choice == 2 || choice == 4) {
            int fromId;
            int toId;
            if (!readInteger("Origin warehouse ID: ", fromId) ||
                !readInteger("Destination warehouse ID: ", toId)) continue;
            if (choice == 1) {
                if (supplyChain.addRoute(fromId, toId))
                    recordChange(audit, persistence, inventory, warehouses, supplyChain,
                                 "ROUTE_ADDED",
                                 to_string(fromId) + " -> " + to_string(toId));
            } else if (choice == 2) {
                if (supplyChain.removeRoute(fromId, toId))
                    recordChange(audit, persistence, inventory, warehouses, supplyChain,
                                 "ROUTE_REMOVED",
                                 to_string(fromId) + " -> " + to_string(toId));
            } else {
                supplyChain.findAndDisplayRoute(fromId, toId);
            }
        } else if (choice == 3) {
            supplyChain.displayRoutes();
        } else if (choice == 5) {
            int originId;
            int destinationId;
            int productId;
            int quantity;
            if (!readInteger("Origin warehouse ID: ", originId) ||
                !readInteger("Destination warehouse ID: ", destinationId) ||
                !readInteger("Product ID: ", productId) ||
                !readInteger("Shipment quantity: ", quantity)) continue;
            if (supplyChain.createShipment(originId, destinationId, productId, quantity))
                recordChange(audit, persistence, inventory, warehouses, supplyChain,
                             "SHIPMENT_CREATED",
                             "Origin " + to_string(originId) + ", destination " +
                             to_string(destinationId) + ", product " +
                             to_string(productId) + ", quantity " + to_string(quantity));
        } else if (choice == 6) {
            int shipmentId;
            if (readInteger("Shipment ID to move: ", shipmentId) &&
                supplyChain.advanceShipment(shipmentId))
                recordChange(audit, persistence, inventory, warehouses, supplyChain,
                             "SHIPMENT_MOVED", "Shipment ID " + to_string(shipmentId));
        } else if (choice == 7 || choice == 8) {
            int shipmentId;
            if (readInteger("Shipment ID: ", shipmentId) &&
                supplyChain.updateShipmentStatus(shipmentId, choice == 7))
                recordChange(audit, persistence, inventory, warehouses, supplyChain,
                             choice == 7 ? "SHIPMENT_DELAYED" : "SHIPMENT_RESUMED",
                             "Shipment ID " + to_string(shipmentId));
        } else if (choice == 9) {
            int shipmentId;
            if (readInteger("Shipment ID to track: ", shipmentId))
                supplyChain.displayShipment(shipmentId);
        } else if (choice == 10) {
            supplyChain.displayShipments();
        } else if (choice != 0) {
            cout << "Invalid choice. Select an option from the menu.\n";
        }
    }
}

void displayAuditLog() {
    ifstream input("audit.log");
    if (!input) {
        cout << "Could not open audit.log.\n";
        return;
    }
    cout << "\n--- Audit Log ---\n";
    string line;
    bool found = false;
    while (getline(input, line)) {
        cout << line << '\n';
        found = true;
    }
    if (!input.eof()) cout << "Error while reading audit.log.\n";
    else if (!found) cout << "No audit events have been recorded.\n";
}

int runSystem() {
    InventoryManager inventory;
    WarehouseManager warehouses;
    SupplyChainManager supplyChain(warehouses, inventory);
    PersonD::AuditLog audit;
    if (!audit.isOpen()) {
        cerr << "Startup error: cannot open audit.log for appending.\n";
        return 1;
    }
    SystemPersistence persistence;
    string error;
    if (!persistence.load(inventory, warehouses, supplyChain, error)) {
        cerr << "Startup error: " << error << '\n';
        return 1;
    }
    if (!allocatedStockIsValid(inventory, warehouses, supplyChain)) {
        cerr << "Startup error: saved catalog stock is lower than the amount stored or in transit.\n";
        return 1;
    }
    if (!audit.record("SYSTEM_STARTED", "Application started"))
        return 1;
    int choice = -1;

    while (choice != 0) {
        cout << "\n===========================================\n"
             << " Global Supply Chain and Warehouse System\n"
             << "===========================================\n"
             << "1. Inventory Manager (Person A)\n"
             << "2. Warehouse Manager (Person B)\n"
             << "3. Supply Chain Manager (Person C)\n"
             << "4. Display inventory and warehouses\n"
             << "5. Display operations report\n"
             << "6. Display audit log\n"
             << "0. Save & exit\n";
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) break;
            continue;
        }
        if (choice == 1) {
            runInventoryManager(inventory, warehouses, supplyChain, audit, persistence);
        } else if (choice == 2) {
            runWarehouseManager(warehouses, inventory, supplyChain, audit, persistence);
        } else if (choice == 3) {
            runSupplyChainManager(supplyChain, inventory, warehouses, audit, persistence);
        } else if (choice == 4) {
            inventory.displayProducts();
            warehouses.displayWarehouses();
            warehouses.displayCapacityReport();
            supplyChain.displayRoutes();
            supplyChain.displayShipments();
        } else if (choice == 5) {
            displaySystemReport(inventory, warehouses, supplyChain);
        } else if (choice == 6) {
            displayAuditLog();
        } else if (choice == 0) {
            audit.record("SYSTEM_STOPPED", "Application shut down");
            if (persistence.save(inventory, warehouses, supplyChain, error))
                cout << "Data saved to warehouse_system.dat. Goodbye.\n";
            else
                cerr << "Save error: " << error << '\n';
        } else {
            cout << "Invalid choice. Select an option from the menu.\n";
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
