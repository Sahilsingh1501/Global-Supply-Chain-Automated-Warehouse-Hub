#define SUPPLY_CHAIN_MODULE_NO_MAIN
#include "supply_chain_module.cpp"

int main() {
    InventoryManager inventory;
    WarehouseManager warehouses;
    SupplyChainManager supplyChain(warehouses, inventory);
    int choice = -1;

    while (choice != 0) {
        cout << "\n--- Person C: Supply Chain ---\n"
             << "1 Add product  2 Add warehouse  3 Store product\n"
             << "4 Add route    5 Remove route   6 Display routes\n"
             << "7 Find route   8 Create shipment 9 Move shipment\n"
             << "10 Delay       11 Resume        12 Track shipment\n"
             << "13 List shipments  0 Exit\n";
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) break;
            continue;
        }

        if (choice == 1) {
            Product product;
            cout << "Enter ID|name|price|quantity:\n";
            if (!(cin >> product)) {
                cout << "Invalid product input.\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            } else if (!inventory.addProduct(product)) {
                cout << "Product ID already exists.\n";
            }
        } else if (choice == 2) {
            int id, capacity;
            string name, location;
            if (readInteger("Warehouse ID: ", id) && readText("Name: ", name) &&
                readText("Location: ", location) && readInteger("Capacity: ", capacity)) {
                if (id <= 0 || capacity <= 0 ||
                    !warehouses.addWarehouse(id, name, location, capacity))
                    cout << "Invalid details or warehouse ID already exists.\n";
            }
        } else if (choice == 3) {
            int warehouseId, productId, quantity;
            if (!readInteger("Warehouse ID: ", warehouseId) ||
                !readInteger("Product ID: ", productId) ||
                !readInteger("Quantity: ", quantity)) continue;
            Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
            Product* product = inventory.searchProduct(productId);
            if (!warehouse || !product) {
                cout << "Create the warehouse and catalog product first.\n";
            } else if (quantity <= 0 || quantity > product->getQuantity() -
                       warehouses.storedProductQuantity(productId) -
                       supplyChain.inTransitProductQuantity(productId)) {
                cout << "Quantity exceeds available catalog stock.\n";
            } else {
                warehouse->addProduct(productId, product->getName(), quantity);
            }
        } else if (choice == 6) {
            supplyChain.displayRoutes();
        } else if (choice == 13) {
            supplyChain.displayShipments();
        } else if (choice >= 4 && choice <= 8) {
            int from, to;
            if (!readInteger("From/origin warehouse ID: ", from) ||
                !readInteger("To/destination warehouse ID: ", to)) continue;
            if (choice == 4) supplyChain.addRoute(from, to);
            else if (choice == 5) supplyChain.removeRoute(from, to);
            else if (choice == 7) supplyChain.findAndDisplayRoute(from, to);
            else {
                int productId, quantity;
                if (readInteger("Product ID: ", productId) &&
                    readInteger("Quantity: ", quantity))
                    supplyChain.createShipment(from, to, productId, quantity);
            }
        } else if (choice >= 9 && choice <= 12) {
            int id;
            if (!readInteger("Shipment ID: ", id)) continue;
            if (choice == 9) supplyChain.advanceShipment(id);
            else if (choice == 10) supplyChain.updateShipmentStatus(id, true);
            else if (choice == 11) supplyChain.updateShipmentStatus(id, false);
            else supplyChain.displayShipment(id);
        } else if (choice != 0) {
            cout << "Invalid choice.\n";
        }
    }
    return 0;
}
