#ifndef INVENTORY_MODULE_NO_MAIN
#define INVENTORY_MODULE_NO_MAIN
#include "main.cpp"
#endif

#ifndef WAREHOUSE_MODULE_NO_MAIN
#define WAREHOUSE_MODULE_NO_MAIN
#include "warehouse_module.cpp"
#endif

#include <string>
#include <limits>
#include "person_d_support.h"
using namespace std;

struct Shipment {
    int id, productId, quantity, path[100], length, step;
    string status;
    Shipment* next;
    Shipment() : id(0), productId(0), quantity(0), length(0), step(0),
                 status("In Transit"), next(nullptr) {}
};

class SupplyChainManager {
    static const int LIMIT = 100;
    WarehouseManager& warehouses;
    InventoryManager& inventory;
    int ids[LIMIT], count, nextId;
    bool routes[LIMIT][LIMIT];
    Shipment* shipments;

    int indexOf(int id) const {
        for (int i = 0; i < count; ++i) if (ids[i] == id) return i;
        return -1;
    }
    int addVertex(int id) {
        int i = indexOf(id);
        if (i >= 0) return i;
        if (count == LIMIT) return -1;
        i = count++;
        ids[i] = id;
        for (int j = 0; j < LIMIT; ++j) routes[i][j] = routes[j][i] = false;
        return i;
    }
    Shipment* findShipment(int id) const {
        for (Shipment* s = shipments; s; s = s->next) if (s->id == id) return s;
        return nullptr;
    }
    bool findPath(int fromId, int toId, int path[], int& length) const {
        int start = indexOf(fromId), end = indexOf(toId);
        if (start < 0 || end < 0) return false;
        int queue[LIMIT], previous[LIMIT], reverse[LIMIT];
        bool visited[LIMIT] = {};
        for (int i = 0; i < count; ++i) previous[i] = -1;
        int front = 0, back = 0;
        queue[back++] = start;
        visited[start] = true;
        while (front < back && !visited[end]) {
            int at = queue[front++];
            for (int i = 0; i < count; ++i) {
                if (routes[at][i] && !visited[i]) {
                    visited[i] = true;
                    previous[i] = at;
                    queue[back++] = i;
                }
            }
        }
        if (!visited[end]) return false;
        int size = 0;
        for (int i = end; i >= 0; i = previous[i]) reverse[size++] = ids[i];
        length = size;
        for (int i = 0; i < size; ++i) path[i] = reverse[size - i - 1];
        return true;
    }

public:
    SupplyChainManager(WarehouseManager& w, InventoryManager& i)
        : warehouses(w), inventory(i), count(0), nextId(1), shipments(nullptr) {
        for (int r = 0; r < LIMIT; ++r)
            for (int c = 0; c < LIMIT; ++c) routes[r][c] = false;
    }
    ~SupplyChainManager() {
        while (shipments) {
            Shipment* next = shipments->next;
            delete shipments;
            shipments = next;
        }
    }
    SupplyChainManager(const SupplyChainManager&) = delete;
    SupplyChainManager& operator=(const SupplyChainManager&) = delete;

    int routeCount() const {
        int total = 0;
        for (int i = 0; i < count; ++i)
            for (int j = i + 1; j < count; ++j)
                if (routes[i][j]) ++total;
        return total;
    }
    int shipmentCount() const {
        int total = 0;
        for (Shipment* s = shipments; s; s = s->next) ++total;
        return total;
    }
    int shipmentCount(const string& status) const {
        int total = 0;
        for (Shipment* s = shipments; s; s = s->next)
            if (s->status == status) ++total;
        return total;
    }
    void saveState(ostream& output) const {
        output << count << '\n';
        for (int i = 0; i < count; ++i) output << ids[i] << '\n';
        output << routeCount() << '\n';
        for (int i = 0; i < count; ++i)
            for (int j = i + 1; j < count; ++j)
                if (routes[i][j]) output << ids[i] << ' ' << ids[j] << '\n';
        output << shipmentCount() << '\n';
        for (Shipment* s = shipments; s; s = s->next) {
            output << s->id << ' ' << s->productId << ' ' << s->quantity << ' '
                   << s->step << ' ' << s->length;
            for (int i = 0; i < s->length; ++i) output << ' ' << s->path[i];
            output << '\n';
            PersonD::writeString(output, s->status);
        }
    }
    bool loadState(istream& input) {
        size_t vertexCount;
        if (!PersonD::readCount(input, vertexCount) || vertexCount > LIMIT) return false;
        for (size_t i = 0; i < vertexCount; ++i) {
            int id;
            if (!(input >> id) || id <= 0 || indexOf(id) >= 0 || addVertex(id) < 0)
                return false;
        }
        size_t edgeCount;
        if (!PersonD::readCount(input, edgeCount) ||
            edgeCount > LIMIT * (LIMIT - 1) / 2) return false;
        for (size_t i = 0; i < edgeCount; ++i) {
            int from, to;
            if (!(input >> from >> to) || from == to) return false;
            int a = indexOf(from), b = indexOf(to);
            if (a < 0 || b < 0 || routes[a][b] ||
                !warehouses.searchWarehouse(from) || !warehouses.searchWarehouse(to))
                return false;
            routes[a][b] = routes[b][a] = true;
        }
        size_t shipmentTotal;
        if (!PersonD::readCount(input, shipmentTotal)) return false;
        int largestId = 0;
        for (size_t i = 0; i < shipmentTotal; ++i) {
            int id, productId, quantity, step, length;
            if (!(input >> id >> productId >> quantity >> step >> length) ||
                id <= 0 || productId <= 0 || quantity <= 0 || length < 2 ||
                length > LIMIT || step < 0 || step >= length ||
                id == numeric_limits<int>::max()) return false;
            Shipment* shipment = new Shipment();
            shipment->id = id;
            shipment->productId = productId;
            shipment->quantity = quantity;
            shipment->step = step;
            shipment->length = length;
            for (int pathIndex = 0; pathIndex < length; ++pathIndex) {
                if (!(input >> shipment->path[pathIndex]) || shipment->path[pathIndex] <= 0) {
                    delete shipment;
                    return false;
                }
            }
            if (!PersonD::readString(input, shipment->status) ||
                (shipment->status != "In Transit" && shipment->status != "Delayed" &&
                 shipment->status != "Delivered") ||
                (shipment->status == "Delivered" && step != length - 1) ||
                (shipment->status != "Delivered" && step == length - 1) ||
                findShipment(id)) {
                delete shipment;
                return false;
            }
            if (shipment->status != "Delivered") {
                if (!inventory.searchProduct(productId)) {
                    delete shipment;
                    return false;
                }
                for (int pathIndex = 0; pathIndex < length; ++pathIndex) {
                    if (!warehouses.searchWarehouse(shipment->path[pathIndex])) {
                        delete shipment;
                        return false;
                    }
                    if (pathIndex > 0) {
                        int previous = indexOf(shipment->path[pathIndex - 1]);
                        int current = indexOf(shipment->path[pathIndex]);
                        if (previous < 0 || current < 0 || !routes[previous][current]) {
                            delete shipment;
                            return false;
                        }
                    }
                }
            }
            shipment->next = shipments;
            shipments = shipment;
            if (id > largestId) largestId = id;
        }
        nextId = largestId + 1;
        return static_cast<bool>(input);
    }

    bool addRoute(int from, int to) {
        Warehouse* a = warehouses.searchWarehouse(from);
        Warehouse* b = warehouses.searchWarehouse(to);
        if (from == to || !a || !b) {
            cout << "Choose two different warehouse IDs that exist.\n";
            return false;
        }
        int needed = (indexOf(from) < 0) + (indexOf(to) < 0);
        if (count + needed > LIMIT) {
            cout << "Route limit reached (100 warehouses).\n";
            return false;
        }
        int x = addVertex(from), y = addVertex(to);
        if (routes[x][y]) {
            cout << "That route already exists.\n";
            return false;
        }
        routes[x][y] = routes[y][x] = true;
        cout << "Route added between " << a->getName() << " and " << b->getName() << ".\n";
        return true;
    }
    bool removeRoute(int from, int to) {
        int x = indexOf(from), y = indexOf(to);
        if (x < 0 || y < 0 || !routes[x][y]) {
            cout << "Route not found.\n";
            return false;
        }
        for (Shipment* s = shipments; s; s = s->next) {
            if (s->status == "Delivered") continue;
            for (int i = s->step; i + 1 < s->length; ++i) {
                if ((s->path[i] == from && s->path[i + 1] == to) ||
                    (s->path[i] == to && s->path[i + 1] == from)) {
                    cout << "That route is required by an active shipment.\n";
                    return false;
                }
            }
        }
        routes[x][y] = routes[y][x] = false;
        cout << "Route removed.\n";
        return true;
    }
    bool findAndDisplayRoute(int from, int to) const {
        int path[LIMIT], length;
        if (!findPath(from, to, path, length)) {
            cout << "No connected route exists between those warehouses.\n";
            return false;
        }
        cout << "Route: ";
        for (int i = 0; i < length; ++i) {
            Warehouse* w = warehouses.searchWarehouse(path[i]);
            cout << (w ? w->getName() : "Unknown") << " [" << path[i] << "]";
            if (i + 1 < length) cout << " -> ";
        }
        cout << '\n';
        return true;
    }
    void displayRoutes() const {
        if (!count) {
            cout << "No routes have been added.\n";
            return;
        }
        cout << "\n--- Connected Warehouses and Routes ---\n";
        for (int i = 0; i < count; ++i) {
            Warehouse* w = warehouses.searchWarehouse(ids[i]);
            if (!w) continue;
            cout << w->getName() << " [" << ids[i] << "] -> ";
            bool found = false;
            for (int j = 0; j < count; ++j) if (routes[i][j]) {
                Warehouse* neighbor = warehouses.searchWarehouse(ids[j]);
                if (neighbor) {
                    if (found) cout << ", ";
                    cout << neighbor->getName() << " [" << ids[j] << "]";
                    found = true;
                }
            }
            cout << (found ? "" : "(no active routes)") << '\n';
        }
    }
    bool usesWarehouse(int id) const {
        int i = indexOf(id);
        if (i >= 0) for (int j = 0; j < count; ++j) if (routes[i][j]) return true;
        for (Shipment* s = shipments; s; s = s->next) if (s->status != "Delivered")
            for (int j = 0; j < s->length; ++j) if (s->path[j] == id) return true;
        return false;
    }
    bool usesProduct(int id) const {
        for (Shipment* s = shipments; s; s = s->next)
            if (s->productId == id && s->status != "Delivered") return true;
        return false;
    }
    int inTransitProductQuantity(int id) const {
        int total = 0;
        for (Shipment* s = shipments; s; s = s->next)
            if (s->productId == id && s->status != "Delivered") total += s->quantity;
        return total;
    }
    bool createShipment(int from, int to, int productId, int quantity) {
        if (nextId == numeric_limits<int>::max()) {
            cout << "Shipment ID limit reached.\n";
            return false;
        }
        Product* p = inventory.searchProduct(productId);
        Warehouse* origin = warehouses.searchWarehouse(from);
        if (!p || !origin || !warehouses.searchWarehouse(to) ||
            from == to || quantity <= 0) {
            cout << "Shipment needs a product added in Inventory Manager, two different warehouses, "
                    "and a positive quantity.\n";
            return false;
        }
        Shipment* s = new Shipment();
        if (!findPath(from, to, s->path, s->length)) {
            delete s;
            cout << "No route connects the origin and destination.\n";
            return false;
        }
        if (origin->productQuantity(productId) < quantity) {
            delete s;
            cout << "The origin warehouse does not have enough stock.\n";
            return false;
        }
        if (!origin->removeProduct(productId, quantity)) {
            delete s;
            return false;
        }
        s->id = nextId++;
        s->productId = productId;
        s->quantity = quantity;
        s->next = shipments;
        shipments = s;
        cout << "Shipment " << s->id << " created and dispatched. Status: "
             << s->status << ".\n";
        return true;
    }
    bool advanceShipment(int id) {
        Shipment* s = findShipment(id);
        if (!s) {
            cout << "Shipment not found.\n";
            return false;
        }
        if (s->status == "Delivered") {
            cout << "Shipment is already delivered.\n";
            return false;
        }
        if (s->status == "Delayed") {
            cout << "Resume the delayed shipment before moving it.\n";
            return false;
        }
        int next = s->step + 1;
        if (next == s->length - 1) {
            Warehouse* destination = warehouses.searchWarehouse(s->path[next]);
            Product* p = inventory.searchProduct(s->productId);
            if (!destination || !p ||
                !destination->canStoreProduct(s->productId, p->getName(), s->quantity)) {
                cout << "Destination capacity check failed; shipment remains at its current stop.\n";
                return false;
            }
            if (!destination->receiveProduct(s->productId, p->getName(), s->quantity)) {
                cout << "Shipment delivery failed.\n";
                return false;
            }
            s->status = "Delivered";
        }
        s->step = next;
        Warehouse* current = warehouses.searchWarehouse(s->path[next]);
        cout << "Shipment " << id << " moved to "
             << (current ? current->getName() : "Unknown")
             << ". Status: " << s->status << ".\n";
        return true;
    }
    bool updateShipmentStatus(int id, bool delayed) {
        Shipment* s = findShipment(id);
        if (!s) {
            cout << "Shipment not found.\n";
            return false;
        }
        if (s->status == "Delivered") {
            cout << "A delivered shipment cannot be rescheduled.\n";
            return false;
        }
        s->status = delayed ? "Delayed" : "In Transit";
        cout << "Shipment " << id << " status updated to " << s->status << ".\n";
        return true;
    }
    void displayShipment(int id) const {
        Shipment* s = findShipment(id);
        if (!s) {
            cout << "Shipment not found.\n";
            return;
        }
        Product* p = inventory.searchProduct(s->productId);
        cout << "Shipment " << id << " | Product: "
             << (p ? p->getName() : "Unknown") << " [" << s->productId
             << "] | Quantity: " << s->quantity << " | Status: " << s->status
             << "\n  Movement: ";
        for (int i = 0; i < s->length; ++i) {
            Warehouse* w = warehouses.searchWarehouse(s->path[i]);
            cout << (w ? w->getName() : "Unknown") << " [" << s->path[i] << "]";
            if (i == s->step) cout << " (current)";
            if (i + 1 < s->length) cout << " -> ";
        }
        cout << '\n';
    }
    void displayShipments() const {
        if (!shipments) cout << "No shipments have been created.\n";
        for (Shipment* s = shipments; s; s = s->next) displayShipment(s->id);
    }
};
