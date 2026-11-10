#ifndef WEB_API_H
#define WEB_API_H

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <sstream>

struct WebResult {
    int status;
    std::string body;
};

struct CoutCapture {
    std::ostringstream buffer;
    std::streambuf* previous;
    CoutCapture() : previous(std::cout.rdbuf(buffer.rdbuf())) {}
    ~CoutCapture() { std::cout.rdbuf(previous); }
    std::string str() const { return buffer.str(); }
};

inline std::string jsonEscape(const std::string& value) {
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::sprintf(buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

inline std::string trimRight(std::string text) {
    while (!text.empty()) {
        char c = text[text.size() - 1];
        if (c != '\n' && c != '\r' && c != ' ' && c != '\t') break;
        text.erase(text.size() - 1);
    }
    return text;
}

inline std::string priceText(double price) {
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out << std::setprecision(2) << price;
    return out.str();
}

inline bool jsonSeek(const std::string& body, const std::string& key, size_t& pos) {
    std::string pattern = "\"" + key + "\"";
    size_t found = body.find(pattern);
    if (found == std::string::npos) return false;
    found = body.find(':', found + pattern.size());
    if (found == std::string::npos) return false;
    pos = found + 1;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) ++pos;
    return pos < body.size();
}

inline bool readJsonInt(const std::string& body, const std::string& key, int& value, std::string& error) {
    size_t pos;
    if (!jsonSeek(body, key, pos)) {
        error = "Enter a value for " + key + ".";
        return false;
    }
    char* end = 0;
    long parsed = std::strtol(body.c_str() + pos, &end, 10);
    if (end == body.c_str() + pos || parsed > 2147483647L || parsed < -2147483647L) {
        error = key + " must be a whole number.";
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

inline bool readJsonDouble(const std::string& body, const std::string& key, double& value, std::string& error) {
    size_t pos;
    if (!jsonSeek(body, key, pos)) {
        error = "Enter a value for " + key + ".";
        return false;
    }
    char* end = 0;
    value = std::strtod(body.c_str() + pos, &end);
    if (end == body.c_str() + pos || !std::isfinite(value)) {
        error = key + " must be a number.";
        return false;
    }
    return true;
}

inline bool readJsonString(const std::string& body, const std::string& key, std::string& value, std::string& error) {
    size_t pos;
    if (!jsonSeek(body, key, pos) || body[pos] != '"') {
        error = "Enter a value for " + key + ".";
        return false;
    }
    ++pos;
    value.clear();
    while (pos < body.size()) {
        char c = body[pos++];
        if (c == '"') return true;
        if (c == '\\' && pos < body.size()) {
            char escaped = body[pos++];
            if (escaped == 'n') value += '\n';
            else if (escaped == 'r') value += '\r';
            else if (escaped == 't') value += '\t';
            else value += escaped;
        } else {
            value += c;
        }
    }
    error = key + " is not a complete value.";
    return false;
}

inline std::string buildStateJson(AppState& app) {
    std::ostringstream out;
    out << "{\"products\":[";
    bool first = true;
    app.inventory.forEachProduct([&](const Product& product) {
        if (!first) out << ',';
        first = false;
        out << "{\"id\":" << product.getId()
            << ",\"name\":\"" << jsonEscape(product.getName()) << "\""
            << ",\"price\":" << priceText(product.getPrice())
            << ",\"quantity\":" << product.getQuantity()
            << ",\"stored\":" << app.warehouses.storedProductQuantity(product.getId())
            << ",\"inTransit\":" << app.supplyChain.inTransitProductQuantity(product.getId())
            << "}";
    });
    out << "],\"warehouses\":[";
    first = true;
    app.warehouses.forEachWarehouse([&](const Warehouse& warehouse) {
        if (!first) out << ',';
        first = false;
        out << "{\"id\":" << warehouse.getId()
            << ",\"name\":\"" << jsonEscape(warehouse.getName()) << "\""
            << ",\"location\":\"" << jsonEscape(warehouse.getLocation()) << "\""
            << ",\"capacity\":" << warehouse.getCapacity()
            << ",\"used\":" << warehouse.getUsedCapacity()
            << ",\"products\":[";
        bool productFirst = true;
        warehouse.forEachStoredProduct([&](int id, const std::string& name, int quantity) {
            if (!productFirst) out << ',';
            productFirst = false;
            out << "{\"id\":" << id
                << ",\"name\":\"" << jsonEscape(name) << "\""
                << ",\"quantity\":" << quantity << "}";
        });
        out << "]}";
    });
    out << "],\"routes\":[";
    first = true;
    app.supplyChain.forEachRoute([&](int from, int to) {
        if (!first) out << ',';
        first = false;
        out << "{\"from\":" << from << ",\"to\":" << to << "}";
    });
    out << "],\"shipments\":[";
    first = true;
    app.supplyChain.forEachShipment([&](const Shipment& shipment) {
        if (!first) out << ',';
        first = false;
        const Product* product = app.inventory.searchProduct(shipment.productId);
        std::string productName = product ? product->getName() : "Unknown";
        out << "{\"id\":" << shipment.id
            << ",\"productId\":" << shipment.productId
            << ",\"productName\":\"" << jsonEscape(productName) << "\""
            << ",\"quantity\":" << shipment.quantity
            << ",\"status\":\"" << jsonEscape(shipment.status) << "\""
            << ",\"step\":" << shipment.step
            << ",\"path\":[";
        for (int i = 0; i < shipment.length; ++i) {
            if (i) out << ',';
            Warehouse* warehouse = app.warehouses.searchWarehouse(shipment.path[i]);
            std::string name = warehouse ? warehouse->getName() : "Unknown";
            out << "{\"id\":" << shipment.path[i]
                << ",\"name\":\"" << jsonEscape(name) << "\""
                << ",\"current\":" << (i == shipment.step ? "true" : "false") << "}";
        }
        out << "]}";
    });
    out << "],\"suppliers\":[";
    first = true;
    app.suppliers.forEachSupplier([&](const Supplier& supplier) {
        if (!first) out << ',';
        first = false;
        out << "{\"id\":" << supplier.id
            << ",\"name\":\"" << jsonEscape(supplier.name) << "\""
            << ",\"contact\":\"" << jsonEscape(supplier.contact) << "\""
            << ",\"rating\":" << priceText(supplier.rating) << "}";
    });
    out << "]}";
    return out.str();
}

inline std::string envelope(bool ok, const std::string& message, AppState& app) {
    std::string text = trimRight(message);
    if (text.empty()) text = ok ? "Completed." : "The request could not be completed.";
    return std::string("{\"ok\":") + (ok ? "true" : "false") +
           ",\"message\":\"" + jsonEscape(text) +
           "\",\"state\":" + buildStateJson(app) + "}";
}

inline WebResult apiOk(AppState& app, const std::string& message) {
    WebResult result;
    result.status = 200;
    result.body = envelope(true, message, app);
    return result;
}

inline WebResult apiBad(AppState& app, const std::string& message) {
    WebResult result;
    result.status = 400;
    result.body = envelope(false, message, app);
    return result;
}

inline std::string readAuditJson() {
    std::ifstream input("audit.log");
    std::string line;
    std::string lines[200];
    int count = 0;
    int next = 0;
    while (std::getline(input, line)) {
        lines[next] = line;
        next = (next + 1) % 200;
        if (count < 200) ++count;
    }
    std::ostringstream out;
    out << "{\"ok\":true,\"lines\":[";
    int start = count == 200 ? next : 0;
    for (int i = 0; i < count; ++i) {
        if (i) out << ',';
        out << "\"" << jsonEscape(lines[(start + i) % 200]) << "\"";
    }
    out << "]}";
    return out.str();
}

inline void rememberSave(const SystemPersistence& persistence, const InventoryManager& inventory,
                         const WarehouseManager& warehouses, const SupplyChainManager& supplyChain,
                         std::string& message) {
    CoutCapture capture;
    saveChanges(persistence, inventory, warehouses, supplyChain);
    std::string note = trimRight(capture.str());
    if (!note.empty()) {
        if (!message.empty()) message += "\n";
        message += note;
    }
}

inline WebResult handleWebApi(AppState& app, const std::string& method, const std::string& path,
                              const std::string& body) {
    InventoryManager& inventory = app.inventory;
    WarehouseManager& warehouses = app.warehouses;
    SupplyChainManager& supplyChain = app.supplyChain;
    SupplierManager& suppliers = app.suppliers;
    const SystemPersistence& persistence = app.persistence;

    if (method == "GET" && path == "/api/state") {
        WebResult result;
        result.status = 200;
        result.body = std::string("{\"ok\":true,\"message\":\"\",\"state\":") + buildStateJson(app) + "}";
        return result;
    }
    if (method == "GET" && path == "/api/audit") {
        WebResult result;
        result.status = 200;
        result.body = readAuditJson();
        return result;
    }
    if (method == "POST" && path == "/api/report") {
        CoutCapture capture;
        generateReport(inventory, warehouses, supplyChain);
        return apiOk(app, capture.str());
    }

    std::string error;
    if (method == "POST" && path == "/api/inventory/add") {
        int id = 0, quantity = 0;
        double price = 0;
        std::string name;
        if (!readJsonInt(body, "id", id, error) ||
            !readJsonString(body, "name", name, error) ||
            !readJsonDouble(body, "price", price, error) ||
            !readJsonInt(body, "quantity", quantity, error)) return apiBad(app, error);
        if (id <= 0 || name.empty() || name.find('|') != std::string::npos ||
            !(price >= 0) || !std::isfinite(price) || quantity < 0) {
            return apiBad(app, "Invalid product details. Use a positive ID, non-negative price/quantity, name without '|'.");
        }
        if (!inventory.addProduct(Product(id, name, price, quantity))) {
            return apiBad(app, "A product with that ID already exists.");
        }
        auditLog("PRODUCT_ADDED", "ID=" + to_string(id) + " name=" + name);
        std::string message = "Product added successfully.";
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/inventory/delete") {
        int id = 0;
        if (!readJsonInt(body, "id", id, error)) return apiBad(app, error);
        if (warehouses.containsProduct(id) || supplyChain.usesProduct(id)) {
            return apiBad(app, "Product is stored or in an active shipment -- cannot delete.");
        }
        if (!inventory.deleteProduct(id)) return apiBad(app, "Product not found.");
        auditLog("PRODUCT_DELETED", "ID=" + to_string(id));
        std::string message = "Product deleted successfully.";
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/inventory/stock") {
        int id = 0, quantity = 0;
        if (!readJsonInt(body, "id", id, error) || !readJsonInt(body, "quantity", quantity, error))
            return apiBad(app, error);
        Product* product = inventory.searchProduct(id);
        if (!product) return apiBad(app, "Product not found.");
        if (quantity < 0) return apiBad(app, "Stock cannot be negative.");
        if (quantity < warehouses.storedProductQuantity(id) + supplyChain.inTransitProductQuantity(id)) {
            return apiBad(app, "Stock can't be lower than the amount stored or in transit.");
        }
        product->setQuantity(quantity);
        auditLog("STOCK_UPDATED", "ID=" + to_string(id) + " new_qty=" + to_string(quantity));
        std::string message = "Stock updated successfully.";
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/warehouse/add") {
        int id = 0, capacity = 0;
        std::string name, location;
        if (!readJsonInt(body, "id", id, error) ||
            !readJsonString(body, "name", name, error) ||
            !readJsonString(body, "location", location, error) ||
            !readJsonInt(body, "capacity", capacity, error)) return apiBad(app, error);
        if (name.empty() || location.empty()) return apiBad(app, "Please enter a value.");
        if (id <= 0 || capacity <= 0) return apiBad(app, "ID and capacity must be > 0.");
        if (!warehouses.addWarehouse(id, name, location, capacity)) {
            return apiBad(app, "A warehouse with that ID already exists.");
        }
        auditLog("WAREHOUSE_ADDED", "ID=" + to_string(id) + " name=" + name);
        std::string message = "Warehouse added successfully.";
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/warehouse/delete") {
        int id = 0;
        if (!readJsonInt(body, "id", id, error)) return apiBad(app, error);
        if (supplyChain.usesWarehouse(id)) {
            return apiBad(app, "Remove its routes and complete active shipments before deleting.");
        }
        std::string message;
        bool removed = false;
        {
            CoutCapture capture;
            removed = warehouses.deleteWarehouse(id);
            message = capture.str();
        }
        if (!removed) return apiBad(app, message.empty() ? "Warehouse could not be deleted." : message);
        auditLog("WAREHOUSE_DELETED", "ID=" + to_string(id));
        message = "Warehouse deleted successfully.";
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/warehouse/store") {
        int warehouseId = 0, productId = 0, quantity = 0;
        if (!readJsonInt(body, "warehouseId", warehouseId, error) ||
            !readJsonInt(body, "productId", productId, error) ||
            !readJsonInt(body, "quantity", quantity, error)) return apiBad(app, error);
        Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
        if (!warehouse) return apiBad(app, "Warehouse not found.");
        Product* product = inventory.searchProduct(productId);
        if (!product) return apiBad(app, "Add this product in Inventory Manager first.");
        if (quantity <= 0) return apiBad(app, "Enter a quantity > 0.");
        if (quantity > product->getQuantity() - warehouses.storedProductQuantity(productId) -
                          supplyChain.inTransitProductQuantity(productId)) {
            return apiBad(app, "Not enough stock available after counting items stored or in transit.");
        }
        int previous = warehouse->productQuantity(productId);
        std::string message;
        {
            CoutCapture capture;
            warehouse->addProduct(productId, product->getName(), quantity);
            message = capture.str();
        }
        if (warehouse->productQuantity(productId) != previous + quantity) return apiBad(app, message);
        auditLog("STOCK_STORED", "product=" + to_string(productId) +
                                 " warehouse=" + to_string(warehouseId) +
                                 " qty=" + to_string(quantity));
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/warehouse/remove") {
        int warehouseId = 0, productId = 0, quantity = 0;
        if (!readJsonInt(body, "warehouseId", warehouseId, error) ||
            !readJsonInt(body, "productId", productId, error) ||
            !readJsonInt(body, "quantity", quantity, error)) return apiBad(app, error);
        Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
        if (!warehouse) return apiBad(app, "Warehouse not found.");
        std::string message;
        bool removed = false;
        {
            CoutCapture capture;
            removed = warehouse->removeProduct(productId, quantity);
            message = capture.str();
        }
        if (!removed) return apiBad(app, message.empty() ? "Product could not be removed." : message);
        auditLog("STOCK_REMOVED", "product=" + to_string(productId) +
                                  " warehouse=" + to_string(warehouseId) +
                                  " qty=" + to_string(quantity));
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/route/add") {
        int from = 0, to = 0;
        if (!readJsonInt(body, "from", from, error) || !readJsonInt(body, "to", to, error))
            return apiBad(app, error);
        std::string message;
        bool added = false;
        {
            CoutCapture capture;
            added = supplyChain.addRoute(from, to);
            message = capture.str();
        }
        if (!added) return apiBad(app, message);
        auditLog("ROUTE_ADDED", "from=" + to_string(from) + " to=" + to_string(to));
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/route/remove") {
        int from = 0, to = 0;
        if (!readJsonInt(body, "from", from, error) || !readJsonInt(body, "to", to, error))
            return apiBad(app, error);
        std::string message;
        bool removed = false;
        {
            CoutCapture capture;
            removed = supplyChain.removeRoute(from, to);
            message = capture.str();
        }
        if (!removed) return apiBad(app, message);
        auditLog("ROUTE_REMOVED", "from=" + to_string(from) + " to=" + to_string(to));
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/route/find") {
        int from = 0, to = 0;
        if (!readJsonInt(body, "from", from, error) || !readJsonInt(body, "to", to, error))
            return apiBad(app, error);
        std::string message;
        bool found = false;
        {
            CoutCapture capture;
            found = supplyChain.findAndDisplayRoute(from, to);
            message = capture.str();
        }
        return found ? apiOk(app, message) : apiBad(app, message);
    }
    if (method == "POST" && path == "/api/shipment/create") {
        int originId = 0, destinationId = 0, productId = 0, quantity = 0;
        if (!readJsonInt(body, "originId", originId, error) ||
            !readJsonInt(body, "destinationId", destinationId, error) ||
            !readJsonInt(body, "productId", productId, error) ||
            !readJsonInt(body, "quantity", quantity, error)) return apiBad(app, error);
        std::string message;
        bool created = false;
        {
            CoutCapture capture;
            created = supplyChain.createShipment(originId, destinationId, productId, quantity);
            message = capture.str();
        }
        if (!created) return apiBad(app, message);
        auditLog("SHIPMENT_CREATED", "from=" + to_string(originId) +
                                      " to=" + to_string(destinationId) +
                                      " product=" + to_string(productId) +
                                      " qty=" + to_string(quantity));
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/shipment/advance") {
        int id = 0;
        if (!readJsonInt(body, "id", id, error)) return apiBad(app, error);
        std::string message;
        bool moved = false;
        {
            CoutCapture capture;
            moved = supplyChain.advanceShipment(id);
            message = capture.str();
        }
        if (!moved) return apiBad(app, message);
        auditLog("SHIPMENT_ADVANCED", "id=" + to_string(id));
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if ((method == "POST" && path == "/api/shipment/delay") ||
        (method == "POST" && path == "/api/shipment/resume")) {
        int id = 0;
        if (!readJsonInt(body, "id", id, error)) return apiBad(app, error);
        bool delay = path == "/api/shipment/delay";
        std::string message;
        bool updated = false;
        {
            CoutCapture capture;
            updated = supplyChain.updateShipmentStatus(id, delay);
            message = capture.str();
        }
        if (!updated) return apiBad(app, message);
        auditLog(delay ? "SHIPMENT_DELAYED" : "SHIPMENT_RESUMED", "id=" + to_string(id));
        rememberSave(persistence, inventory, warehouses, supplyChain, message);
        return apiOk(app, message);
    }
    if (method == "POST" && path == "/api/supplier/add") {
        int id = 0;
        double rating = 0;
        std::string name, contact;
        if (!readJsonInt(body, "id", id, error) ||
            !readJsonString(body, "name", name, error) ||
            !readJsonString(body, "contact", contact, error) ||
            !readJsonDouble(body, "rating", rating, error)) return apiBad(app, error);
        if (name.empty() || contact.empty()) return apiBad(app, "Please enter a value.");
        if (rating < 0) rating = 0;
        if (rating > 5) rating = 5;
        if (!suppliers.addSupplier(id, name, contact, rating)) {
            return apiBad(app, "Supplier ID already exists.");
        }
        auditLog("SUPPLIER_ADDED", "ID=" + to_string(id) + " name=" + name);
        suppliers.saveToFile();
        return apiOk(app, "Supplier added.");
    }
    if (method == "POST" && path == "/api/supplier/delete") {
        int id = 0;
        if (!readJsonInt(body, "id", id, error)) return apiBad(app, error);
        if (!suppliers.deleteSupplier(id)) return apiBad(app, "Supplier not found.");
        auditLog("SUPPLIER_DELETED", "ID=" + to_string(id));
        suppliers.saveToFile();
        return apiOk(app, "Supplier deleted.");
    }
    if (method == "POST" && path == "/api/supplier/update") {
        int id = 0;
        double rating = 0;
        std::string contact;
        if (!readJsonInt(body, "id", id, error) ||
            !readJsonString(body, "contact", contact, error) ||
            !readJsonDouble(body, "rating", rating, error)) return apiBad(app, error);
        if (contact.empty()) return apiBad(app, "Please enter a value.");
        if (rating < 0) rating = 0;
        if (rating > 5) rating = 5;
        if (!suppliers.updateSupplier(id, contact, rating)) return apiBad(app, "Supplier not found.");
        auditLog("SUPPLIER_UPDATED", "ID=" + to_string(id));
        suppliers.saveToFile();
        return apiOk(app, "Supplier updated.");
    }

    WebResult missing;
    missing.status = 404;
    missing.body = "{\"ok\":false,\"message\":\"Unknown request.\"}";
    return missing;
}

#endif
