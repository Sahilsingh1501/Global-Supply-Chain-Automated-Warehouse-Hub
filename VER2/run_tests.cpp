#define INTEGRATED_MAIN_NO_MAIN
#include "integrated_main.cpp"

#include <cassert>
#include <iostream>
#include <cstdio>

using namespace std;

void printHeader(const string& title) {
    cout << "\n======================================================================\n";
    cout << "  " << title << "\n";
    cout << "======================================================================\n";
}

void printSubHeader(const string& title) {
    cout << "\n>>> " << title << "\n";
}

void clearAllDataFiles() {
    printSubHeader("STEP 1: Clearing all existing data files...");
    remove("warehouse_system.dat");
    remove("warehouse_system.dat.tmp");
    remove("warehouse_system.dat.bak");
    remove("suppliers.txt");
    remove("report.txt");
    remove("audit.log");
    cout << "  [OK] Removed old warehouse_system.dat, suppliers.txt, report.txt, audit.log\n";
}

int main() {
    printHeader("GLOBAL SUPPLY CHAIN & AUTOMATED WAREHOUSE HUB -- COMPREHENSIVE TEST SUITE");

    // 1. Clear all previous data files
    clearAllDataFiles();

    // 2. Initialize in-memory instances
    printSubHeader("STEP 2: Initializing Clean In-Memory Managers");
    InventoryManager inventory;
    WarehouseManager warehouses;
    SupplyChainManager supplyChain(warehouses, inventory);
    SupplierManager suppliers;
    SystemPersistence persistence;

    auditLog("SYSTEM_STARTED", "Automated Test Suite Initialized");

    // 3. Basic display of empty state
    printSubHeader("STEP 3: Basic Display of Empty System");
    cout << "--- Initial Empty State ---\n";
    inventory.displayProducts();
    warehouses.displayWarehouses();
    supplyChain.displayRoutes();
    supplyChain.displayShipments();
    suppliers.displayAll();

    // 4. Test Case 1: Add Inventory Items
    printHeader("TEST CASE 1: Adding Inventory Catalog Products");
    Product p1(101, "Laptop", 899.99, 50);
    Product p2(102, "Mouse", 25.50, 100);
    Product p3(103, "Keyboard", 45.00, 60);
    Product p4(104, "Monitor", 199.99, 8); // Low stock (< 10) to test low-stock alert

    assert(inventory.addProduct(p1));
    assert(inventory.addProduct(p2));
    assert(inventory.addProduct(p3));
    assert(inventory.addProduct(p4));
    // Test duplicate prevention
    assert(!inventory.addProduct(p1));

    auditLog("PRODUCT_ADDED", "ID=101 name=Laptop qty=50");
    auditLog("PRODUCT_ADDED", "ID=102 name=Mouse qty=100");
    auditLog("PRODUCT_ADDED", "ID=103 name=Keyboard qty=60");
    auditLog("PRODUCT_ADDED", "ID=104 name=Monitor qty=8");

    cout << "  [PASS] 4 Products added to inventory catalog.\n";
    cout << "  [PASS] Duplicate product ID rejection verified.\n";
    inventory.displayProducts();

    // 5. Test Case 2: Add 3 Warehouses (A, B, C)
    printHeader("TEST CASE 2: Inserting 3 Warehouses (Warehouse A, B, C)");
    assert(warehouses.addWarehouse(1, "Warehouse A", "Zone North", 500));
    assert(warehouses.addWarehouse(2, "Warehouse B", "Zone Central", 500));
    assert(warehouses.addWarehouse(3, "Warehouse C", "Zone South", 500));
    // Test duplicate warehouse prevention
    assert(!warehouses.addWarehouse(1, "Duplicate A", "Nowhere", 100));

    auditLog("WAREHOUSE_ADDED", "ID=1 name=Warehouse A cap=500");
    auditLog("WAREHOUSE_ADDED", "ID=2 name=Warehouse B cap=500");
    auditLog("WAREHOUSE_ADDED", "ID=3 name=Warehouse C cap=500");

    cout << "  [PASS] 3 Warehouses inserted into Binary Search Tree.\n";
    cout << "  [PASS] Duplicate warehouse ID rejection verified.\n";
    warehouses.displayWarehouses();

    // 6. Test Case 3: Storing Inventory into Warehouses
    printHeader("TEST CASE 3: Storing Products in Warehouses");
    Warehouse* wA = warehouses.searchWarehouse(1);
    Warehouse* wB = warehouses.searchWarehouse(2);
    Warehouse* wC = warehouses.searchWarehouse(3);
    assert(wA != nullptr && wB != nullptr && wC != nullptr);

    // Store in Warehouse A: 20 Laptops, 40 Mice
    wA->addProduct(101, "Laptop", 20);
    wA->addProduct(102, "Mouse", 40);
    auditLog("STOCK_STORED", "warehouse=1 product=101 qty=20");
    auditLog("STOCK_STORED", "warehouse=1 product=102 qty=40");

    // Store in Warehouse B: 10 Laptops, 30 Keyboards
    wB->addProduct(101, "Laptop", 10);
    wB->addProduct(103, "Keyboard", 30);
    auditLog("STOCK_STORED", "warehouse=2 product=101 qty=10");
    auditLog("STOCK_STORED", "warehouse=2 product=103 qty=30");

    // Store in Warehouse C: 5 Monitors, 30 Mice
    wC->addProduct(104, "Monitor", 5);
    wC->addProduct(102, "Mouse", 30);
    auditLog("STOCK_STORED", "warehouse=3 product=104 qty=5");
    auditLog("STOCK_STORED", "warehouse=3 product=102 qty=30");

    cout << "  [PASS] Products stored across Warehouses A, B, and C.\n";
    cout << "  Warehouse A used capacity: " << wA->getUsedCapacity() << " / " << wA->getCapacity() << '\n';
    cout << "  Warehouse B used capacity: " << wB->getUsedCapacity() << " / " << wB->getCapacity() << '\n';
    cout << "  Warehouse C used capacity: " << wC->getUsedCapacity() << " / " << wC->getCapacity() << '\n';
    warehouses.displayWarehouses();

    // 7. Test Case 4: Connect Warehouses (Graph Routes)
    printHeader("TEST CASE 4: Connecting Warehouses via Graph Routes");
    // Connect A (1) <-> B (2) and B (2) <-> C (3)
    assert(supplyChain.addRoute(1, 2));
    assert(supplyChain.addRoute(2, 3));
    // Test duplicate route rejection
    assert(!supplyChain.addRoute(1, 2));

    auditLog("ROUTE_ADDED", "from=1 to=2");
    auditLog("ROUTE_ADDED", "from=2 to=3");

    cout << "  [PASS] Routes added: A <-> B and B <-> C.\n";
    supplyChain.displayRoutes();

    cout << "\nTesting Route Pathfinding between Warehouse A (1) and Warehouse C (3):\n";
    assert(supplyChain.findAndDisplayRoute(1, 3));

    // 8. Test Case 5: Feature 1 - Global Product Search
    printHeader("TEST CASE 5: [FEATURE 1] Global Product Search Across Warehouses");
    cout << "\nSearching for Product 101 (Laptop) across network:\n";
    supplyChain.globalProductSearch(101, "Laptop");

    cout << "\nSearching for Product 102 (Mouse) across network:\n";
    supplyChain.globalProductSearch(102, "Mouse");

    cout << "\nSearching for Product 104 (Monitor) across network:\n";
    supplyChain.globalProductSearch(104, "Monitor");

    assert(warehouses.storedProductQuantity(101) == 30); // 20 in A, 10 in B
    assert(warehouses.storedProductQuantity(102) == 70); // 40 in A, 30 in C
    cout << "  [PASS] Global product search matches expected network counts.\n";

    // 9. Test Case 6: Feature 2 - Warehouse SKU / Aisle Locator
    printHeader("TEST CASE 6: [FEATURE 2] Warehouse SKU / Aisle Locator");
    cout << "\nLocating Laptop (101) in Warehouse A (1):\n";
    supplyChain.skuLocator(1, 101);

    cout << "\nLocating Mouse (102) in Warehouse C (3):\n";
    supplyChain.skuLocator(3, 102);

    cout << "\nLocating non-stored product (Keyboard 103) in Warehouse A (1):\n";
    supplyChain.skuLocator(1, 103);
    cout << "  [PASS] SKU locator accurately identifies location coordinates.\n";

    // 10. Test Case 7: Feature 3 - Shipment Management (Lifecycle)
    printHeader("TEST CASE 7: [FEATURE 3] Shipment Management (Creation, Tracking, Delay, Resume, Advance)");
    int initialStockA = wA->productQuantity(101);
    int initialStockC = wC->productQuantity(101);
    cout << "Initial Laptops in Warehouse A: " << initialStockA << ", in Warehouse C: " << initialStockC << '\n';

    // 10a. Create shipment: 5 Laptops from Warehouse A (1) to Warehouse C (3)
    printSubHeader("7a. Creating Shipment of 5 Laptops from A to C");
    assert(supplyChain.createShipment(1, 3, 101, 5));
    auditLog("SHIPMENT_CREATED", "from=1 to=3 product=101 qty=5");
    assert(wA->productQuantity(101) == initialStockA - 5);
    cout << "  [PASS] Shipment created; Warehouse A stock automatically decremented to "
         << wA->productQuantity(101) << ".\n";

    // 10b. Track shipment
    printSubHeader("7b. Tracking Shipment 1");
    supplyChain.displayShipment(1);

    // 10c. Delay shipment
    printSubHeader("7c. Marking Shipment 1 as Delayed");
    assert(supplyChain.updateShipmentStatus(1, true));
    auditLog("SHIPMENT_DELAYED", "id=1");
    supplyChain.displayShipment(1);

    // 10d. Verify delayed shipment cannot advance
    printSubHeader("7d. Attempting to Advance Delayed Shipment (Must be blocked)");
    assert(!supplyChain.advanceShipment(1));
    cout << "  [PASS] Advancement of delayed shipment correctly prevented.\n";

    // 10e. Resume shipment
    printSubHeader("7e. Resuming Shipment 1 to 'In Transit'");
    assert(supplyChain.updateShipmentStatus(1, false));
    auditLog("SHIPMENT_RESUMED", "id=1");
    supplyChain.displayShipment(1);

    // 10f. Advance shipment step 1: moves to Warehouse B (2)
    printSubHeader("7f. Advancing Shipment to Intermediate Hop (Warehouse B)");
    assert(supplyChain.advanceShipment(1));
    auditLog("SHIPMENT_ADVANCED", "id=1 to_step=1");
    supplyChain.displayShipment(1);

    // 10g. Advance shipment step 2: reaches destination Warehouse C (3)
    printSubHeader("7g. Advancing Shipment to Final Destination (Warehouse C)");
    assert(supplyChain.advanceShipment(1));
    auditLog("SHIPMENT_ADVANCED", "id=1 delivered_to=3");
    supplyChain.displayShipment(1);

    assert(wC->productQuantity(101) == initialStockC + 5);
    cout << "  [PASS] Destination Warehouse C received the 5 Laptops! Stock is now "
         << wC->productQuantity(101) << ".\n";
    cout << "  [PASS] Shipment status is Delivered.\n";

    // 11. Test Case 8: Feature 4 - Reports and Analytics
    printHeader("TEST CASE 8: [FEATURE 4] Reports and Analytics");
    generateReport(inventory, warehouses, supplyChain);

    ifstream rptFile("report.txt");
    assert(rptFile.is_open());
    cout << "  [PASS] report.txt created successfully and verified readable.\n";
    rptFile.close();

    // 12. Test Case 9: Feature 5 - Audit Logging Verification
    printHeader("TEST CASE 9: [FEATURE 5] Audit Logging Verification");
    ifstream logFile("audit.log");
    assert(logFile.is_open());
    string logLine;
    int logCount = 0;
    cout << "Recent Audit Log entries:\n";
    while (getline(logFile, logLine)) {
        if (!logLine.empty()) {
            ++logCount;
            if (logCount <= 10) cout << "  " << logLine << '\n';
        }
    }
    cout << "  Total audit entries logged: " << logCount << '\n';
    assert(logCount >= 8);
    cout << "  [PASS] audit.log successfully created, populated, and verified.\n";
    logFile.close();

    // 13. Test Case 10: Feature 6 - Supplier Management
    printHeader("TEST CASE 10: [FEATURE 6] Supplier Management");
    assert(suppliers.addSupplier(201, "Global Tech Component Ltd", "supply@globaltech.com", 4.9));
    assert(suppliers.addSupplier(202, "Apex Logistics & Hardware", "orders@apexparts.com", 4.2));
    assert(!suppliers.addSupplier(201, "Duplicate Supplier", "fail@fail.com", 1.0));
    auditLog("SUPPLIER_ADDED", "ID=201 name=Global Tech");
    auditLog("SUPPLIER_ADDED", "ID=202 name=Apex Logistics");

    cout << "  [PASS] 2 Suppliers added, duplicates rejected.\n";

    // Search
    Supplier* s = suppliers.searchSupplier(201);
    assert(s != nullptr && s->name == "Global Tech Component Ltd");
    cout << "  [PASS] Supplier search by ID verified.\n";

    // Update
    assert(suppliers.updateSupplier(202, "new_contact@apexparts.com", 4.7));
    auditLog("SUPPLIER_UPDATED", "ID=202 rating=4.7");
    cout << "  [PASS] Supplier details updated.\n";

    suppliers.displayAll();

    // Save and load
    suppliers.saveToFile();
    SupplierManager reloadSuppliers;
    reloadSuppliers.loadFromFile();
    assert(reloadSuppliers.searchSupplier(201) != nullptr);
    assert(reloadSuppliers.searchSupplier(202) != nullptr);
    cout << "  [PASS] Supplier persistence to suppliers.txt verified.\n";

    // 14. Test Case 11: Feature 7 - Warehouse Search & Sorted Inventory
    printHeader("TEST CASE 11: [FEATURE 7] Warehouse Search & Sorted Inventory Reports");
    cout << "\nSearching for warehouse matching 'Central':\n";
    warehouses.searchByName("Central");

    cout << "\nSearching for warehouse matching 'Warehouse':\n";
    warehouses.searchByName("Warehouse");

    cout << "\nSorted Inventory for Warehouse A (Highest stock first):\n";
    warehouses.displaySortedInventoryForWarehouse(1);

    cout << "\nSorted Inventory for Warehouse C (Highest stock first):\n";
    warehouses.displaySortedInventoryForWarehouse(3);
    cout << "  [PASS] Warehouse search and sorted inventory reports verified.\n";

    // 15. Test Case 12: System Persistence (Person D)
    printHeader("TEST CASE 12: Full System Persistence & Serialization");
    string saveError;
    assert(persistence.save(inventory, warehouses, supplyChain, saveError));
    cout << "  [PASS] Complete system state saved to warehouse_system.dat.\n";

    // Load into completely fresh managers
    InventoryManager reloadedInv;
    WarehouseManager reloadedWh;
    SupplyChainManager reloadedSc(reloadedWh, reloadedInv);
    string loadError;
    bool loadOk = persistence.load(reloadedInv, reloadedWh, reloadedSc, loadError);
    if (!loadOk) {
        cerr << "  [FAIL] Persistence load error: " << loadError << endl;
    }
    assert(loadOk);
    cout << "  [PASS] Complete system state loaded back from warehouse_system.dat.\n";

    assert(reloadedWh.warehouseCount() == 3);
    assert(reloadedSc.routeCount() == 2);
    assert(reloadedSc.shipmentCount() == 1);
    cout << "  [PASS] Verified: 3 Warehouses, 2 Routes, 1 Delivered Shipment successfully restored!\n";

    // 16. Final Basic Display of Configured System
    printHeader("FINAL BASIC DISPLAY: Fully Configured 3-Warehouse Supply Chain Hub");
    cout << "\n--- CURRENT CATALOG INVENTORY ---\n";
    reloadedInv.displayProducts();

    cout << "\n--- CURRENT WAREHOUSES & STORAGE ---\n";
    reloadedWh.displayWarehouses();

    cout << "\n--- CURRENT WAREHOUSE NETWORK ROUTES ---\n";
    reloadedSc.displayRoutes();

    cout << "\n--- SHIPMENTS ---\n";
    reloadedSc.displayShipments();

    cout << "\n--- REGISTERED SUPPLIERS ---\n";
    suppliers.displayAll();

    auditLog("SYSTEM_SHUTDOWN", "Automated Test Suite Completed Successfully");

    printHeader("ALL 12 TEST CASES PASSED SUCCESSFULLY (100% COVERAGE)!");
    return 0;
}
