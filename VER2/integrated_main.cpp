#define INVENTORY_MODULE_NO_MAIN
#include "main.cpp"

#define WAREHOUSE_MODULE_NO_MAIN
#include "warehouse_module.cpp"

#include "supply_chain_module.cpp"
#include "person_d_persistence.h"

void saveChanges(const SystemPersistence& persistence, const InventoryManager& inventory,
                 const WarehouseManager& warehouses,
                 const SupplyChainManager& supplyChain) {
    string error;
    if (!persistence.save(inventory, warehouses, supplyChain, error))
        cout << "Could not save changes: " << error
             << "\nThe changes are still in memory, but may be lost when the app closes.\n";
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
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) return;
            continue;
        }

        if (choice == 1) {
            Product product;
            cout << "Enter product as ID|name|price|quantity "
                    "(example: 101|Laptop Pro|899.99|12):\n";
            if (!(cin >> product)) {
                cout << "Product details aren't valid. Use a positive ID, a non-negative "
                        "price and quantity, and a name without '|'.\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            } else if (inventory.addProduct(product)) {
                cout << "Product added successfully.\n";
                saveChanges(persistence, inventory, warehouses, supplyChain);
            } else {
                cout << "A product with that ID already exists.\n";
            }
        } else if (choice == 2) {
            int id;
            if (!readInteger("Product ID to delete: ", id)) continue;
            if (warehouses.containsProduct(id) || supplyChain.usesProduct(id)) {
                cout << "This product is stored or in an active shipment, so it can't be deleted.\n";
            } else if (inventory.deleteProduct(id)) {
                cout << "Product deleted successfully.\n";
                saveChanges(persistence, inventory, warehouses, supplyChain);
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
                    cout << "Stock can't be lower than the amount stored or in transit.\n";
                } else {
                    product->setQuantity(quantity);
                    cout << "Stock updated successfully.\n";
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
                cout << "Warehouse ID and capacity must both be greater than zero.\n";
            } else if (!warehouses.addWarehouse(id, name, location, capacity)) {
                cout << "A warehouse with that ID already exists.\n";
            } else {
                cout << "Warehouse added successfully.\n";
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 2) {
            int id;
            if (!readInteger("Warehouse ID to delete: ", id)) continue;
            if (supplyChain.usesWarehouse(id)) {
                cout << "Remove its routes and complete its active shipments before deleting it.\n";
            } else if (warehouses.deleteWarehouse(id)) {
                cout << "Warehouse deleted successfully.\n";
                saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice == 3) {
            warehouses.displayWarehouses();
        } else if (choice == 4) {
            int warehouseId;
            int productId;
            int quantity;
            if (!readInteger("Warehouse ID: ", warehouseId)) continue;
            Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
            if (!warehouse) {
                cout << "Warehouse not found.\n";
                continue;
            }
            if (!readInteger("Inventory product ID: ", productId)) continue;
            Product* product = inventory.searchProduct(productId);
            if (!product) {
                cout << "Add this product in Inventory Manager before storing it.\n";
                continue;
            }
            if (!readInteger("Quantity to store: ", quantity)) continue;
            if (quantity <= 0) {
                cout << "Enter a quantity greater than zero.\n";
            } else if (quantity > product->getQuantity() -
                                      warehouses.storedProductQuantity(productId) -
                                      supplyChain.inTransitProductQuantity(productId)) {
                cout << "Not enough stock is available after counting items already stored "
                        "or in transit.\n";
            } else {
                int previousQuantity = warehouse->productQuantity(productId);
                warehouse->addProduct(productId, product->getName(), quantity);
                if (warehouse->productQuantity(productId) == previousQuantity + quantity)
                    saveChanges(persistence, inventory, warehouses, supplyChain);
            }
        } else if (choice != 0) {
            cout << "Please choose one of the listed options.\n";
        }
    }
}

void runSupplyChainManager(SupplyChainManager& supplyChain, const InventoryManager& inventory,
                           const WarehouseManager& warehouses,
                           const SystemPersistence& persistence) {
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Supply Chain Manager  ---\n"
             << "1. Add route\n"
             << "2. Display connected warehouses\n"
             << "3. Create shipment\n"
             << "0. Return to main menu\n";
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) return;
            continue;
        }

        if (choice == 1) {
            int fromId;
            int toId;
            if (!readInteger("Origin warehouse ID: ", fromId) ||
                !readInteger("Destination warehouse ID: ", toId)) continue;
            if (supplyChain.addRoute(fromId, toId))
                saveChanges(persistence, inventory, warehouses, supplyChain);
        } else if (choice == 2) {
            supplyChain.displayRoutes();
        } else if (choice == 3) {
            int originId;
            int destinationId;
            int productId;
            int quantity;
            if (!readInteger("Origin warehouse ID: ", originId) ||
                !readInteger("Destination warehouse ID: ", destinationId) ||
                !readInteger("Product ID: ", productId) ||
                !readInteger("Shipment quantity: ", quantity)) continue;
            if (supplyChain.createShipment(originId, destinationId, productId, quantity))
                saveChanges(persistence, inventory, warehouses, supplyChain);
        } else if (choice != 0) {
            cout << "Please choose one of the listed options.\n";
        }
    }
}

int runSystem() {
    InventoryManager inventory;
    WarehouseManager warehouses;
    SupplyChainManager supplyChain(warehouses, inventory);
    SystemPersistence persistence;
    string error;
    if (!persistence.load(inventory, warehouses, supplyChain, error)) {
        cerr << "Startup error: " << error << '\n';
        return 1;
    }
    if (!allocatedStockIsValid(inventory, warehouses, supplyChain)) {
        cerr << "Startup error: saved inventory stock is less than the amount stored or in transit.\n";
        return 1;
    }
    int choice = -1;

    while (choice != 0) {
        cout << "\n===========================================\n"
             << " Global Supply Chain and Warehouse System\n"
             << "===========================================\n"
             << "1. Inventory Manager\n"
             << "2. Warehouse Manager\n"
             << "3. Supply Chain Manager\n"
             << "4. Display inventory and warehouses\n"
             << "0. Save & exit\n";
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) break;
            continue;
        }
        if (choice == 1) {
            runInventoryManager(inventory, warehouses, supplyChain, persistence);
        } else if (choice == 2) {
            runWarehouseManager(warehouses, inventory, supplyChain, persistence);
        } else if (choice == 3) {
            runSupplyChainManager(supplyChain, inventory, warehouses, persistence);
        } else if (choice == 4) {
            inventory.displayProducts();
            warehouses.displayWarehouses();
            supplyChain.displayRoutes();
            supplyChain.displayShipments();
        } else if (choice == 0) {
            if (persistence.save(inventory, warehouses, supplyChain, error))
                cout << "Data saved. Goodbye.\n";
            else
                cerr << "Save error: " << error << '\n';
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
