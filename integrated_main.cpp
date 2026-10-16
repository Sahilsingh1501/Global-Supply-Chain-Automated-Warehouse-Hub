#define INVENTORY_MODULE_NO_MAIN
#include "main.cpp"

#define WAREHOUSE_MODULE_NO_MAIN
#include "warehouse_module.cpp"

#include "supply_chain_module.cpp"

void runInventoryManager(InventoryManager& inventory, const WarehouseManager& warehouses,
                         const SupplyChainManager& supplyChain) {
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
                         SupplyChainManager& supplyChain) {
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
            }
        } else if (choice == 2) {
            int id;
            if (!readInteger("Warehouse ID to delete: ", id)) continue;
            if (supplyChain.usesWarehouse(id)) {
                cout << "Remove its routes and finish its active shipments before deleting it.\n";
            } else if (warehouses.deleteWarehouse(id)) {
                cout << "Warehouse deleted successfully.\n";
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
                warehouse->addProduct(productId, product->getName(), quantity);
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
            else warehouse->removeProduct(productId, quantity);
        } else if (choice == 7) {
            warehouses.displayCapacityReport();
        } else if (choice != 0) {
            cout << "Invalid choice. Select an option from the menu.\n";
        }
    }
}

void runSupplyChainManager(SupplyChainManager& supplyChain) {
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
            if (choice == 1) supplyChain.addRoute(fromId, toId);
            else if (choice == 2) supplyChain.removeRoute(fromId, toId);
            else supplyChain.findAndDisplayRoute(fromId, toId);
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
            supplyChain.createShipment(originId, destinationId, productId, quantity);
        } else if (choice == 6) {
            int shipmentId;
            if (readInteger("Shipment ID to move: ", shipmentId))
                supplyChain.advanceShipment(shipmentId);
        } else if (choice == 7 || choice == 8) {
            int shipmentId;
            if (readInteger("Shipment ID: ", shipmentId))
                supplyChain.updateShipmentStatus(shipmentId, choice == 7);
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

int main() {
    InventoryManager inventory;
    WarehouseManager warehouses;
    SupplyChainManager supplyChain(warehouses, inventory);
    int choice = -1;

    while (choice != 0) {
        cout << "\n===========================================\n"
             << " Global Supply Chain and Warehouse System\n"
             << "===========================================\n"
             << "1. Inventory Manager (Person A)\n"
             << "2. Warehouse Manager (Person B)\n"
             << "3. Supply Chain Manager (Person C)\n"
             << "4. Display inventory and warehouses\n"
             << "0. Save & exit (data currently in memory only)\n";
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) break;
            continue;
        }
        if (choice == 1) {
            runInventoryManager(inventory, warehouses, supplyChain);
        } else if (choice == 2) {
            runWarehouseManager(warehouses, inventory, supplyChain);
        } else if (choice == 3) {
            runSupplyChainManager(supplyChain);
        } else if (choice == 4) {
            inventory.displayProducts();
            warehouses.displayWarehouses();
            warehouses.displayCapacityReport();
            supplyChain.displayRoutes();
            supplyChain.displayShipments();
        } else if (choice == 0) {
            cout << "Exiting. Role D's file persistence is not implemented yet.\n";
        } else {
            cout << "Invalid choice. Select an option from the menu.\n";
        }
    }
    return 0;
}
