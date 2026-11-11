#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define INVENTORY_MODULE_NO_MAIN
#include "../../VER2/main.cpp"
#define WAREHOUSE_MODULE_NO_MAIN
#include "../../VER2/warehouse_module.cpp"
#define SUPPLY_CHAIN_MODULE_NO_MAIN
#include "../../VER2/supply_chain_module.cpp"
#include "../../VER2/person_d_persistence.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifdef __MINGW32__
#include <io.h>
#define _access_s _access
#endif
#include "vendor/httplib.h"
#include "vendor/json.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using json = nlohmann::json;

namespace {

const int MAX_BODY_SIZE = 1024 * 1024;
const int PBKDF2_ITERATIONS = 210000;
const std::size_t MAX_AUDIT_ENTRIES = 2000;

struct ApiError : public std::runtime_error {
    int status;
    ApiError(int statusCode, const std::string& message)
        : std::runtime_error(message), status(statusCode) {}
};

struct UserRecord {
    std::string id;
    std::string name;
    std::string email;
    std::string role;
    std::string passwordHash;
    bool active;
    std::string createdAt;
};

struct SupplierRecord {
    std::string id;
    std::string code;
    std::string name;
    std::string email;
    std::string phone;
    std::string address;
    bool active;
    std::string createdAt;
};

struct PurchaseItemRecord {
    std::string id;
    int productId;
    int quantityOrdered;
    int quantityReceived;
};

struct PurchaseRecord {
    std::string id;
    std::string orderNumber;
    std::string supplierId;
    int warehouseId;
    std::string status;
    std::string expectedAt;
    std::string notes;
    std::string createdAt;
    std::vector<PurchaseItemRecord> items;
};

struct MovementRecord {
    std::string id;
    int productId;
    int warehouseId;
    std::string userId;
    std::string kind;
    int delta;
    std::string note;
    std::string reference;
    std::string createdAt;
};

struct RouteRecord {
    int fromId;
    int toId;
    int distanceKm;
    int transitHours;
    bool active;
};

struct ShipmentEventRecord {
    std::string status;
    std::string note;
    std::string createdAt;
};

struct ShipmentMeta {
    int id;
    std::string trackingNumber;
    std::string expectedAt;
    std::string delayReason;
    std::string createdAt;
    std::string deliveredAt;
    std::vector<ShipmentEventRecord> events;
};

struct ProductMeta {
    int id;
    std::string sku;
    std::string category;
    std::string unit;
    std::string description;
    int reorderLevel;
};

struct WarehouseMeta {
    int id;
    std::string code;
    std::string address;
};

struct SessionRecord {
    std::string userId;
    std::time_t expiresAt;
};

struct AuditRecord {
    std::string id;
    std::string userId;
    std::string action;
    std::string entity;
    std::string entityId;
    std::string details;
    std::string createdAt;
};

class OutputCapture {
    std::streambuf* previous;
    std::ostringstream ignored;
public:
    OutputCapture() : previous(std::cout.rdbuf(ignored.rdbuf())) {}
    ~OutputCapture() { std::cout.rdbuf(previous); }
};

std::tm utcTime(std::time_t timestamp) {
    static std::mutex timeMutex;
    std::tm value;
    std::lock_guard<std::mutex> lock(timeMutex);
    const std::tm* utc = std::gmtime(&timestamp);
    if (!utc) throw std::runtime_error("Failed to format UTC timestamp.");
    value = *utc;
    return value;
}

std::string nowIso() {
    const std::time_t now = std::time(nullptr);
    const std::tm value = utcTime(now);
    std::ostringstream result;
    result << std::put_time(&value, "%Y-%m-%dT%H:%M:%SZ");
    return result.str();
}

std::string randomHex(std::size_t byteCount) {
    typedef LONG NTSTATUS;
    typedef void* BCRYPT_ALG_HANDLE;
    typedef NTSTATUS (WINAPI * BCryptGenRandomFunction)(
        BCRYPT_ALG_HANDLE, unsigned char*, ULONG, ULONG);
    HMODULE library = LoadLibraryA("bcrypt.dll");
    if (!library) throw std::runtime_error("Windows cryptographic provider is unavailable.");
    BCryptGenRandomFunction generate = reinterpret_cast<BCryptGenRandomFunction>(
        GetProcAddress(library, "BCryptGenRandom"));
    if (!generate) {
        FreeLibrary(library);
        throw std::runtime_error("Secure random number generation is unavailable.");
    }
    std::vector<unsigned char> bytes(byteCount);
    const NTSTATUS result = generate(nullptr, bytes.data(),
        static_cast<ULONG>(bytes.size()), 0x00000002);
    FreeLibrary(library);
    if (result < 0) throw std::runtime_error("Secure random number generation failed.");
    static const char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(byteCount * 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        output.push_back(digits[bytes[i] >> 4]);
        output.push_back(digits[bytes[i] & 15]);
    }
    return output;
}

std::string derivePasswordHash(const std::string& password, const std::string& saltHex) {
    typedef LONG NTSTATUS;
    typedef void* BCRYPT_ALG_HANDLE;
    typedef NTSTATUS (WINAPI * BCryptOpenAlgorithmProviderFunction)(
        BCRYPT_ALG_HANDLE*, LPCWSTR, LPCWSTR, ULONG);
    typedef NTSTATUS (WINAPI * BCryptDeriveKeyPBKDF2Function)(
        BCRYPT_ALG_HANDLE, unsigned char*, ULONG, unsigned char*, ULONG,
        unsigned long long, unsigned char*, ULONG, ULONG);
    typedef NTSTATUS (WINAPI * BCryptCloseAlgorithmProviderFunction)(
        BCRYPT_ALG_HANDLE, ULONG);
    HMODULE library = LoadLibraryA("bcrypt.dll");
    if (!library) throw std::runtime_error("Windows password hashing provider is unavailable.");
    BCryptOpenAlgorithmProviderFunction openProvider =
        reinterpret_cast<BCryptOpenAlgorithmProviderFunction>(GetProcAddress(library, "BCryptOpenAlgorithmProvider"));
    BCryptDeriveKeyPBKDF2Function derive =
        reinterpret_cast<BCryptDeriveKeyPBKDF2Function>(GetProcAddress(library, "BCryptDeriveKeyPBKDF2"));
    BCryptCloseAlgorithmProviderFunction closeProvider =
        reinterpret_cast<BCryptCloseAlgorithmProviderFunction>(GetProcAddress(library, "BCryptCloseAlgorithmProvider"));
    if (!openProvider || !derive || !closeProvider) {
        FreeLibrary(library);
        throw std::runtime_error("Windows PBKDF2 password hashing is unavailable.");
    }
    BCRYPT_ALG_HANDLE provider = nullptr;
    const NTSTATUS opened = openProvider(&provider, L"SHA256", nullptr, 0x00000008);
    if (opened < 0) {
        FreeLibrary(library);
        throw std::runtime_error("Could not initialize password hashing.");
    }
    std::vector<unsigned char> salt;
    for (std::size_t i = 0; i + 1 < saltHex.size(); i += 2) {
        salt.push_back(static_cast<unsigned char>(
            std::strtoul(saltHex.substr(i, 2).c_str(), nullptr, 16)));
    }
    std::vector<unsigned char> derived(32);
    const NTSTATUS result = derive(
        provider,
        reinterpret_cast<unsigned char*>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        salt.data(),
        static_cast<ULONG>(salt.size()),
        PBKDF2_ITERATIONS,
        derived.data(),
        static_cast<ULONG>(derived.size()),
        0);
    closeProvider(provider, 0);
    FreeLibrary(library);
    if (result < 0) throw std::runtime_error("Password hashing failed.");
    static const char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(derived.size() * 2);
    for (std::size_t i = 0; i < derived.size(); ++i) {
        output.push_back(digits[derived[i] >> 4]);
        output.push_back(digits[derived[i] & 15]);
    }
    return output;
}

std::string makePasswordHash(const std::string& password) {
    const std::string salt = randomHex(16);
    return "pbkdf2-sha256$" + std::to_string(PBKDF2_ITERATIONS) + "$" + salt + "$" +
           derivePasswordHash(password, salt);
}

bool verifyPassword(const std::string& password, const std::string& stored) {
    const std::string prefix = "pbkdf2-sha256$" + std::to_string(PBKDF2_ITERATIONS) + "$";
    if (stored.compare(0, prefix.size(), prefix) != 0) return false;
    const std::size_t saltEnd = stored.find('$', prefix.size());
    if (saltEnd == std::string::npos) return false;
    const std::string salt = stored.substr(prefix.size(), saltEnd - prefix.size());
    const std::string expected = stored.substr(saltEnd + 1);
    const std::string actual = derivePasswordHash(password, salt);
    if (actual.size() != expected.size()) return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < actual.size(); ++i) difference |= actual[i] ^ expected[i];
    return difference == 0;
}

std::string roleName(const std::string& value) {
    static const char* roles[] = {"ADMIN", "INVENTORY", "LOGISTICS", "PROCUREMENT", "VIEWER"};
    for (int i = 0; i < 5; ++i) if (value == roles[i]) return value;
    throw ApiError(400, "Role must be ADMIN, INVENTORY, LOGISTICS, PROCUREMENT, or VIEWER.");
}

int parseId(const std::string& value, const std::string& label) {
    if (value.empty()) throw ApiError(400, label + " is required.");
    char* end = nullptr;
    errno = 0;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (errno || !end || *end || parsed <= 0 || parsed > 2147483647L)
        throw ApiError(400, label + " must be a positive whole number.");
    return static_cast<int>(parsed);
}

std::string getString(const json& object, const std::string& key,
                      const std::string& fallback = "", std::size_t maxLength = 240) {
    if (!object.is_object() || object.find(key) == object.end() || object[key].is_null()) return fallback;
    if (!object[key].is_string()) throw ApiError(400, key + " must be text.");
    const std::string value = object[key].get<std::string>();
    if (value.size() > maxLength) throw ApiError(400, key + " is too long.");
    return value;
}

int getInteger(const json& object, const std::string& key, int fallback = -1,
               int minimum = 1, int maximum = 1000000000) {
    if (!object.is_object() || object.find(key) == object.end()) {
        if (fallback >= 0) return fallback;
        throw ApiError(400, key + " is required.");
    }
    if (!object[key].is_number_integer()) throw ApiError(400, key + " must be a whole number.");
    const long long value = object[key].get<long long>();
    if (value < minimum || value > maximum) throw ApiError(400, key + " is outside the allowed range.");
    return static_cast<int>(value);
}

bool getBoolean(const json& object, const std::string& key, bool fallback) {
    if (!object.is_object() || object.find(key) == object.end()) return fallback;
    if (!object[key].is_boolean()) throw ApiError(400, key + " must be true or false.");
    return object[key].get<bool>();
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::string apiStatus(const std::string& value) {
    if (value == "In Transit") return "IN_TRANSIT";
    if (value == "Delivered") return "DELIVERED";
    if (value == "Delayed") return "DELAYED";
    return "IN_TRANSIT";
}

std::string dateKey(const std::string& iso) {
    return iso.size() >= 10 ? iso.substr(0, 10) : "";
}

class Application {
    InventoryManager inventory;
    WarehouseManager warehouses;
    SupplyChainManager supplyChain;
    SystemPersistence corePersistence;
    httplib::Server server;
    std::mutex stateMutex;
    std::vector<UserRecord> users;
    std::vector<SupplierRecord> suppliers;
    std::vector<PurchaseRecord> purchaseOrders;
    std::vector<MovementRecord> movements;
    std::vector<RouteRecord> routeMetadata;
    std::vector<ShipmentMeta> shipmentMetadata;
    std::vector<ProductMeta> productMetadata;
    std::vector<WarehouseMeta> warehouseMetadata;
    std::vector<AuditRecord> auditRecords;
    std::map<std::string, SessionRecord> sessions;
    std::map<std::string, int> loginFailures;
    unsigned long long nextSupplierId;
    unsigned long long nextOrderId;
    unsigned long long nextMovementId;
    unsigned long long nextAuditId;
    int port;
    std::string webRoot;

    template <typename Handler>
    void addGet(const std::string& path, Handler handler) {
        server.Get(path.c_str(), [this, handler](const httplib::Request& request, httplib::Response& response) {
            execute(request, response, handler);
        });
    }

    template <typename Handler>
    void addPost(const std::string& path, Handler handler) {
        server.Post(path.c_str(), [this, handler](const httplib::Request& request, httplib::Response& response) {
            execute(request, response, handler);
        });
    }

    template <typename Handler>
    void addPatch(const std::string& path, Handler handler) {
        server.Patch(path.c_str(), [this, handler](const httplib::Request& request, httplib::Response& response) {
            execute(request, response, handler);
        });
    }

    template <typename Handler>
    void execute(const httplib::Request& request, httplib::Response& response, Handler handler) {
        std::lock_guard<std::mutex> lock(stateMutex);
        OutputCapture outputCapture;
        response.set_header("X-Content-Type-Options", "nosniff");
        response.set_header("X-Frame-Options", "DENY");
        response.set_header("Referrer-Policy", "strict-origin-when-cross-origin");
        response.set_header("Cache-Control", "no-store");
        try {
            handler(request, response);
        } catch (const ApiError& error) {
            sendJson(response, error.status, json{{"error", error.what()}});
        } catch (const json::exception&) {
            sendJson(response, 400, json{{"error", "Request contains invalid JSON data."}});
        } catch (const std::exception& error) {
            std::cerr << "Unhandled API error: " << error.what() << '\n';
            sendJson(response, 500, json{{"error", "Something went wrong. Try again, and contact an administrator if this continues."}});
        }
    }

    static void sendJson(httplib::Response& response, int status, const json& payload) {
        response.status = status;
        response.set_content(payload.dump(), "application/json; charset=utf-8");
    }

    static json parseBody(const httplib::Request& request) {
        if (request.body.empty()) return json::object();
        try {
            json parsed = json::parse(request.body);
            if (!parsed.is_object()) throw ApiError(400, "The request body must be a JSON object.");
            return parsed;
        } catch (const ApiError&) {
            throw;
        } catch (const json::exception&) {
            throw ApiError(400, "Request body is not valid JSON.");
        }
    }

    json sidecarJson() const {
        json state;
        state["version"] = 1;
        state["users"] = json::array();
        for (std::size_t i = 0; i < users.size(); ++i) {
            const UserRecord& value = users[i];
            state["users"].push_back({{"id", value.id}, {"name", value.name}, {"email", value.email},
                {"role", value.role}, {"passwordHash", value.passwordHash},
                {"active", value.active}, {"createdAt", value.createdAt}});
        }
        state["suppliers"] = json::array();
        for (std::size_t i = 0; i < suppliers.size(); ++i) {
            const SupplierRecord& value = suppliers[i];
            state["suppliers"].push_back({{"id", value.id}, {"code", value.code}, {"name", value.name},
                {"email", value.email}, {"phone", value.phone}, {"address", value.address},
                {"active", value.active}, {"createdAt", value.createdAt}});
        }
        state["orders"] = json::array();
        for (std::size_t i = 0; i < purchaseOrders.size(); ++i) {
            const PurchaseRecord& value = purchaseOrders[i];
            json items = json::array();
            for (std::size_t j = 0; j < value.items.size(); ++j) {
                items.push_back({{"id", value.items[j].id}, {"productId", value.items[j].productId},
                    {"quantityOrdered", value.items[j].quantityOrdered},
                    {"quantityReceived", value.items[j].quantityReceived}});
            }
            state["orders"].push_back({{"id", value.id}, {"orderNumber", value.orderNumber},
                {"supplierId", value.supplierId}, {"warehouseId", value.warehouseId},
                {"status", value.status}, {"expectedAt", value.expectedAt}, {"notes", value.notes},
                {"createdAt", value.createdAt}, {"items", items}});
        }
        state["movements"] = json::array();
        for (std::size_t i = 0; i < movements.size(); ++i) {
            const MovementRecord& value = movements[i];
            state["movements"].push_back({{"id", value.id}, {"productId", value.productId},
                {"warehouseId", value.warehouseId}, {"userId", value.userId}, {"kind", value.kind},
                {"delta", value.delta}, {"note", value.note}, {"reference", value.reference},
                {"createdAt", value.createdAt}});
        }
        state["routes"] = json::array();
        for (std::size_t i = 0; i < routeMetadata.size(); ++i) {
            const RouteRecord& value = routeMetadata[i];
            state["routes"].push_back({{"fromId", value.fromId}, {"toId", value.toId},
                {"distanceKm", value.distanceKm}, {"transitHours", value.transitHours},
                {"active", value.active}});
        }
        state["shipments"] = json::array();
        for (std::size_t i = 0; i < shipmentMetadata.size(); ++i) {
            const ShipmentMeta& value = shipmentMetadata[i];
            json events = json::array();
            for (std::size_t j = 0; j < value.events.size(); ++j) {
                events.push_back({{"status", value.events[j].status}, {"note", value.events[j].note},
                    {"createdAt", value.events[j].createdAt}});
            }
            state["shipments"].push_back({{"id", value.id}, {"trackingNumber", value.trackingNumber},
                {"expectedAt", value.expectedAt}, {"delayReason", value.delayReason},
                {"createdAt", value.createdAt}, {"deliveredAt", value.deliveredAt}, {"events", events}});
        }
        state["products"] = json::array();
        for (std::size_t i = 0; i < productMetadata.size(); ++i) {
            const ProductMeta& value = productMetadata[i];
            state["products"].push_back({{"id", value.id}, {"sku", value.sku}, {"category", value.category},
                {"unit", value.unit}, {"description", value.description}, {"reorderLevel", value.reorderLevel}});
        }
        state["warehouses"] = json::array();
        for (std::size_t i = 0; i < warehouseMetadata.size(); ++i) {
            state["warehouses"].push_back({{"id", warehouseMetadata[i].id},
                {"code", warehouseMetadata[i].code}, {"address", warehouseMetadata[i].address}});
        }
        state["audit"] = json::array();
        for (std::size_t i = 0; i < auditRecords.size(); ++i) {
            const AuditRecord& value = auditRecords[i];
            state["audit"].push_back({{"id", value.id}, {"userId", value.userId}, {"action", value.action},
                {"entity", value.entity}, {"entityId", value.entityId}, {"details", value.details},
                {"createdAt", value.createdAt}});
        }
        state["nextSupplierId"] = nextSupplierId;
        state["nextOrderId"] = nextOrderId;
        state["nextMovementId"] = nextMovementId;
        state["nextAuditId"] = nextAuditId;
        return state;
    }

    static std::string jsonString(const json& object, const char* key, const std::string& fallback = "") {
        return object.contains(key) && object[key].is_string() ? object[key].get<std::string>() : fallback;
    }

    void loadSidecar() {
        std::ifstream input("warehouse_web_state.json", std::ios::binary);
        if (!input) return;
        json state;
        try {
            input >> state;
        } catch (const json::exception&) {
            throw std::runtime_error("Saved web application data is invalid JSON.");
        }
        if (!state.is_object() || state.value("version", 0) != 1)
            throw std::runtime_error("Saved web application data has an unsupported format.");
        if (state.contains("users") && state["users"].is_array()) {
            for (std::size_t i = 0; i < state["users"].size(); ++i) {
                const json& value = state["users"][i];
                users.push_back({jsonString(value, "id"), jsonString(value, "name"),
                    lower(jsonString(value, "email")), jsonString(value, "role"),
                    jsonString(value, "passwordHash"), value.value("active", false),
                    jsonString(value, "createdAt")});
            }
        }
        if (state.contains("suppliers") && state["suppliers"].is_array()) {
            for (std::size_t i = 0; i < state["suppliers"].size(); ++i) {
                const json& value = state["suppliers"][i];
                suppliers.push_back({jsonString(value, "id"), jsonString(value, "code"),
                    jsonString(value, "name"), jsonString(value, "email"), jsonString(value, "phone"),
                    jsonString(value, "address"), value.value("active", false),
                    jsonString(value, "createdAt")});
            }
        }
        if (state.contains("orders") && state["orders"].is_array()) {
            for (std::size_t i = 0; i < state["orders"].size(); ++i) {
                const json& value = state["orders"][i];
                PurchaseRecord order;
                order.id = jsonString(value, "id");
                order.orderNumber = jsonString(value, "orderNumber");
                order.supplierId = jsonString(value, "supplierId");
                order.warehouseId = value.value("warehouseId", 0);
                order.status = jsonString(value, "status");
                order.expectedAt = jsonString(value, "expectedAt");
                order.notes = jsonString(value, "notes");
                order.createdAt = jsonString(value, "createdAt");
                if (value.contains("items") && value["items"].is_array()) {
                    for (std::size_t j = 0; j < value["items"].size(); ++j) {
                        const json& item = value["items"][j];
                        order.items.push_back({jsonString(item, "id"), item.value("productId", 0),
                            item.value("quantityOrdered", 0), item.value("quantityReceived", 0)});
                    }
                }
                purchaseOrders.push_back(order);
            }
        }
        if (state.contains("movements") && state["movements"].is_array()) {
            for (std::size_t i = 0; i < state["movements"].size(); ++i) {
                const json& value = state["movements"][i];
                movements.push_back({jsonString(value, "id"), value.value("productId", 0),
                    value.value("warehouseId", 0), jsonString(value, "userId"), jsonString(value, "kind"),
                    value.value("delta", 0), jsonString(value, "note"), jsonString(value, "reference"),
                    jsonString(value, "createdAt")});
            }
        }
        if (state.contains("routes") && state["routes"].is_array()) {
            for (std::size_t i = 0; i < state["routes"].size(); ++i) {
                const json& value = state["routes"][i];
                routeMetadata.push_back({value.value("fromId", 0), value.value("toId", 0),
                    value.value("distanceKm", 0), value.value("transitHours", 1),
                    value.value("active", true)});
            }
        }
        if (state.contains("shipments") && state["shipments"].is_array()) {
            for (std::size_t i = 0; i < state["shipments"].size(); ++i) {
                const json& value = state["shipments"][i];
                ShipmentMeta shipment;
                shipment.id = value.value("id", 0);
                shipment.trackingNumber = jsonString(value, "trackingNumber");
                shipment.expectedAt = jsonString(value, "expectedAt");
                shipment.delayReason = jsonString(value, "delayReason");
                shipment.createdAt = jsonString(value, "createdAt");
                shipment.deliveredAt = jsonString(value, "deliveredAt");
                if (value.contains("events") && value["events"].is_array()) {
                    for (std::size_t j = 0; j < value["events"].size(); ++j) {
                        const json& event = value["events"][j];
                        shipment.events.push_back({jsonString(event, "status"), jsonString(event, "note"),
                            jsonString(event, "createdAt")});
                    }
                }
                shipmentMetadata.push_back(shipment);
            }
        }
        if (state.contains("products") && state["products"].is_array()) {
            for (std::size_t i = 0; i < state["products"].size(); ++i) {
                const json& value = state["products"][i];
                productMetadata.push_back({value.value("id", 0), jsonString(value, "sku"),
                    jsonString(value, "category", "General"), jsonString(value, "unit", "pcs"),
                    jsonString(value, "description"), value.value("reorderLevel", 10)});
            }
        }
        if (state.contains("warehouses") && state["warehouses"].is_array()) {
            for (std::size_t i = 0; i < state["warehouses"].size(); ++i) {
                const json& value = state["warehouses"][i];
                warehouseMetadata.push_back({value.value("id", 0), jsonString(value, "code"),
                    jsonString(value, "address")});
            }
        }
        if (state.contains("audit") && state["audit"].is_array()) {
            for (std::size_t i = 0; i < state["audit"].size(); ++i) {
                const json& value = state["audit"][i];
                auditRecords.push_back({jsonString(value, "id"), jsonString(value, "userId"),
                    jsonString(value, "action"), jsonString(value, "entity"),
                    jsonString(value, "entityId"), jsonString(value, "details"),
                    jsonString(value, "createdAt")});
            }
        }
        nextSupplierId = state.value("nextSupplierId", 1ULL);
        nextOrderId = state.value("nextOrderId", 1ULL);
        nextMovementId = state.value("nextMovementId", 1ULL);
        nextAuditId = state.value("nextAuditId", 1ULL);
    }

    void saveSidecar() const {
        const std::string temporary = "warehouse_web_state.json.tmp";
        const std::string backup = "warehouse_web_state.json.bak";
        {
            std::ofstream output(temporary.c_str(), std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("Could not create the temporary web data file.");
            output << sidecarJson().dump(2) << '\n';
            output.flush();
            if (!output) {
                output.close();
                std::remove(temporary.c_str());
                throw std::runtime_error("Could not write web application data.");
            }
        }
        const bool hasOriginal = std::ifstream("warehouse_web_state.json").good();
        if (hasOriginal) {
            std::remove(backup.c_str());
            if (std::rename("warehouse_web_state.json", backup.c_str()) != 0) {
                std::remove(temporary.c_str());
                throw std::runtime_error("Could not preserve the previous web data file.");
            }
        }
        if (std::rename(temporary.c_str(), "warehouse_web_state.json") != 0) {
            if (hasOriginal) std::rename(backup.c_str(), "warehouse_web_state.json");
            std::remove(temporary.c_str());
            throw std::runtime_error("Could not replace the web application data file.");
        }
        if (hasOriginal) std::remove(backup.c_str());
    }

    void saveCore() {
        std::string error;
        if (!corePersistence.save(inventory, warehouses, supplyChain, error))
            throw std::runtime_error("Could not save C++ warehouse data: " + error);
    }

    void saveAll() {
        saveCore();
        saveSidecar();
    }

    void ensureMetadata() {
        inventory.forEachProduct([this](const Product& product) {
            if (!findProductMeta(product.getId())) {
                std::ostringstream sku;
                sku << "SKU-" << std::setfill('0') << std::setw(4) << product.getId();
                productMetadata.push_back({product.getId(), sku.str(), "General", "pcs", "", 10});
            }
        });
        warehouses.forEachWarehouse([this](const Warehouse& warehouse) {
            if (!findWarehouseMeta(warehouse.getId())) {
                std::ostringstream code;
                code << "WH-" << std::setfill('0') << std::setw(3) << warehouse.getId();
                warehouseMetadata.push_back({warehouse.getId(), code.str(), ""});
            }
        });
        supplyChain.forEachRoute([this](int from, int to) {
            if (!findRoute(from, to)) routeMetadata.push_back({from, to, 0, 1, true});
        });
        supplyChain.forEachShipment([this](const Shipment& shipment) {
            if (!findShipmentMeta(shipment.id)) {
                std::ostringstream tracking;
                tracking << "NS-" << shipment.id;
                ShipmentMeta meta;
                meta.id = shipment.id;
                meta.trackingNumber = tracking.str();
                meta.createdAt = nowIso();
                meta.expectedAt = "";
                meta.delayReason = "";
                meta.deliveredAt = shipment.status == "Delivered" ? meta.createdAt : "";
                meta.events.push_back({apiStatus(shipment.status), "Existing shipment restored from the C++ system.", meta.createdAt});
                shipmentMetadata.push_back(meta);
            }
        });
    }

    void seedIfEmpty() {
        if (inventory.productCount() == 0) {
            inventory.addProduct(Product(101, "Wireless Headphones", 79.99, 120));
            inventory.addProduct(Product(102, "USB-C Charging Cable", 12.50, 250));
            inventory.addProduct(Product(103, "Insulated Travel Mug", 24.00, 88));
            inventory.addProduct(Product(104, "Premium Notebook Set", 18.75, 64));
            inventory.addProduct(Product(105, "Laptop Sleeve 14-inch", 32.00, 42));
            warehouses.addWarehouse(1, "Northstar Distribution", "Chicago", 12000);
            warehouses.addWarehouse(2, "East Coast Fulfillment", "New York", 8500);
            warehouses.addWarehouse(3, "Pacific Logistics Center", "Los Angeles", 10000);
            warehouses.searchWarehouse(1)->receiveProduct(101, "Wireless Headphones", 58);
            warehouses.searchWarehouse(2)->receiveProduct(101, "Wireless Headphones", 38);
            warehouses.searchWarehouse(3)->receiveProduct(101, "Wireless Headphones", 24);
            warehouses.searchWarehouse(1)->receiveProduct(102, "USB-C Charging Cable", 120);
            warehouses.searchWarehouse(2)->receiveProduct(102, "USB-C Charging Cable", 78);
            warehouses.searchWarehouse(3)->receiveProduct(102, "USB-C Charging Cable", 52);
            warehouses.searchWarehouse(1)->receiveProduct(103, "Insulated Travel Mug", 42);
            warehouses.searchWarehouse(2)->receiveProduct(103, "Insulated Travel Mug", 28);
            warehouses.searchWarehouse(3)->receiveProduct(103, "Insulated Travel Mug", 18);
            warehouses.searchWarehouse(1)->receiveProduct(104, "Premium Notebook Set", 31);
            warehouses.searchWarehouse(2)->receiveProduct(104, "Premium Notebook Set", 20);
            warehouses.searchWarehouse(3)->receiveProduct(104, "Premium Notebook Set", 13);
            warehouses.searchWarehouse(1)->receiveProduct(105, "Laptop Sleeve 14-inch", 20);
            warehouses.searchWarehouse(2)->receiveProduct(105, "Laptop Sleeve 14-inch", 13);
            warehouses.searchWarehouse(3)->receiveProduct(105, "Laptop Sleeve 14-inch", 9);
            const int routes[][2] = {{1, 2}, {2, 3}, {1, 3}};
            for (std::size_t i = 0; i < 3; ++i) supplyChain.addRoute(routes[i][0], routes[i][1]);
            productMetadata.clear();
            productMetadata.push_back({101, "EL-1001", "Electronics", "pcs", "", 40});
            productMetadata.push_back({102, "EL-1002", "Electronics", "pcs", "", 120});
            productMetadata.push_back({103, "HM-2001", "Home & Living", "pcs", "", 50});
            productMetadata.push_back({104, "OF-3001", "Office", "sets", "", 35});
            productMetadata.push_back({105, "AC-4001", "Accessories", "pcs", "", 30});
            warehouseMetadata.clear();
            warehouseMetadata.push_back({1, "WH-NORTH", "Chicago"});
            warehouseMetadata.push_back({2, "WH-EAST", "New York"});
            warehouseMetadata.push_back({3, "WH-WEST", "Los Angeles"});
            routeMetadata.clear();
            routeMetadata.push_back({1, 2, 1145, 22, true});
            routeMetadata.push_back({2, 3, 3940, 70, true});
            routeMetadata.push_back({1, 3, 2800, 49, true});
            routeMetadata.push_back({2, 1, 1145, 22, true});
            routeMetadata.push_back({3, 2, 3940, 70, true});
            routeMetadata.push_back({3, 1, 2800, 49, true});
            const std::string timestamp = nowIso();
            inventory.forEachProduct([this, timestamp](const Product& product) {
                warehouses.forEachWarehouse([this, &product, &timestamp](const Warehouse& warehouse) {
                    const int quantity = warehouse.productQuantity(product.getId());
                    if (quantity > 0) movements.push_back({std::to_string(nextMovementId++), product.getId(),
                        warehouse.getId(), "", "OPENING_BALANCE", quantity, "Demo opening inventory", "", timestamp});
                });
            });
        }
        if (suppliers.empty()) {
            const std::string timestamp = nowIso();
            suppliers.push_back({"1", "SUP-001", "Acme Components", "orders@acme.example",
                "+1 312 555 0142", "Chicago, IL", true, timestamp});
            suppliers.push_back({"2", "SUP-002", "Pacific Manufacturing Co.", "supply@pacific.example",
                "+1 213 555 0187", "Los Angeles, CA", true, timestamp});
            suppliers.push_back({"3", "SUP-003", "Meridian Goods", "hello@meridian.example",
                "+1 212 555 0121", "New York, NY", true, timestamp});
            nextSupplierId = std::max(nextSupplierId, 4ULL);
        }
    }

    ProductMeta* findProductMeta(int id) {
        for (std::size_t i = 0; i < productMetadata.size(); ++i)
            if (productMetadata[i].id == id) return &productMetadata[i];
        return nullptr;
    }
    const ProductMeta* findProductMeta(int id) const {
        for (std::size_t i = 0; i < productMetadata.size(); ++i)
            if (productMetadata[i].id == id) return &productMetadata[i];
        return nullptr;
    }
    WarehouseMeta* findWarehouseMeta(int id) {
        for (std::size_t i = 0; i < warehouseMetadata.size(); ++i)
            if (warehouseMetadata[i].id == id) return &warehouseMetadata[i];
        return nullptr;
    }
    const WarehouseMeta* findWarehouseMeta(int id) const {
        for (std::size_t i = 0; i < warehouseMetadata.size(); ++i)
            if (warehouseMetadata[i].id == id) return &warehouseMetadata[i];
        return nullptr;
    }
    RouteRecord* findRoute(int from, int to) {
        for (std::size_t i = 0; i < routeMetadata.size(); ++i)
            if ((routeMetadata[i].fromId == from && routeMetadata[i].toId == to) ||
                (routeMetadata[i].fromId == to && routeMetadata[i].toId == from)) return &routeMetadata[i];
        return nullptr;
    }
    RouteRecord* findRouteExact(int from, int to) {
        for (std::size_t i = 0; i < routeMetadata.size(); ++i)
            if (routeMetadata[i].fromId == from && routeMetadata[i].toId == to)
                return &routeMetadata[i];
        return nullptr;
    }
    ShipmentMeta* findShipmentMeta(int id) {
        for (std::size_t i = 0; i < shipmentMetadata.size(); ++i)
            if (shipmentMetadata[i].id == id) return &shipmentMetadata[i];
        return nullptr;
    }
    SupplierRecord* findSupplier(const std::string& id) {
        for (std::size_t i = 0; i < suppliers.size(); ++i)
            if (suppliers[i].id == id && suppliers[i].active) return &suppliers[i];
        return nullptr;
    }
    PurchaseRecord* findOrder(const std::string& id) {
        for (std::size_t i = 0; i < purchaseOrders.size(); ++i)
            if (purchaseOrders[i].id == id) return &purchaseOrders[i];
        return nullptr;
    }
    UserRecord* findUser(const std::string& id) {
        for (std::size_t i = 0; i < users.size(); ++i)
            if (users[i].id == id) return &users[i];
        return nullptr;
    }
    ShipmentMeta& ensureShipmentMeta(const Shipment& shipment) {
        ShipmentMeta* meta = findShipmentMeta(shipment.id);
        if (meta) return *meta;
        std::ostringstream tracking;
        tracking << "NS-" << shipment.id;
        shipmentMetadata.push_back({shipment.id, tracking.str(), "", "",
            nowIso(), "", std::vector<ShipmentEventRecord>()});
        return shipmentMetadata.back();
    }

    const UserRecord& requireUser(const httplib::Request& request) {
        const std::string header = request.get_header_value("Authorization");
        if (header.compare(0, 7, "Bearer ") != 0) throw ApiError(401, "Sign in to continue.");
        const std::string token = header.substr(7);
        std::map<std::string, SessionRecord>::iterator session = sessions.find(token);
        if (session == sessions.end() || session->second.expiresAt <= std::time(nullptr)) {
            if (session != sessions.end()) sessions.erase(session);
            throw ApiError(401, "Your session has expired. Please sign in again.");
        }
        UserRecord* user = findUser(session->second.userId);
        if (!user || !user->active) {
            sessions.erase(session);
            throw ApiError(401, "This account is no longer active.");
        }
        return *user;
    }

    void requireRole(const UserRecord& user, const std::vector<std::string>& allowed) {
        if (std::find(allowed.begin(), allowed.end(), user.role) == allowed.end())
            throw ApiError(403, "You do not have permission to perform this action.");
    }

    void recordAudit(const UserRecord& user, const std::string& action,
                     const std::string& entity, const std::string& entityId,
                     const std::string& details = "") {
        auditRecords.insert(auditRecords.begin(), {std::to_string(nextAuditId++), user.id,
            action, entity, entityId, details, nowIso()});
        if (auditRecords.size() > MAX_AUDIT_ENTRIES) auditRecords.resize(MAX_AUDIT_ENTRIES);
    }

    void recordMovement(const UserRecord& user, int productId, int warehouseId,
                        const std::string& kind, int delta, const std::string& note,
                        const std::string& reference = "") {
        movements.insert(movements.begin(), {std::to_string(nextMovementId++), productId,
            warehouseId, user.id, kind, delta, note, reference, nowIso()});
        if (movements.size() > MAX_AUDIT_ENTRIES) movements.resize(MAX_AUDIT_ENTRIES);
    }

    json userJson(const UserRecord& user) const {
        return {{"id", user.id}, {"name", user.name}, {"email", user.email},
            {"role", user.role}, {"active", user.active}, {"createdAt", user.createdAt}};
    }

    json productJson(const Product& product) const {
        const ProductMeta* meta = findProductMeta(product.getId());
        json stock = json::array();
        warehouses.forEachWarehouse([&stock, &product, this](const Warehouse& warehouse) {
            const int quantity = warehouse.productQuantity(product.getId());
            if (quantity <= 0) return;
            const WarehouseMeta* warehouseInfo = findWarehouseMeta(warehouse.getId());
            stock.push_back({{"id", std::to_string(warehouse.getId()) + "-" + std::to_string(product.getId())},
                {"productId", product.getId()}, {"warehouseId", std::to_string(warehouse.getId())},
                {"quantity", quantity}, {"warehouse", {{"id", std::to_string(warehouse.getId())},
                    {"name", warehouse.getName()}, {"code", warehouseInfo ? warehouseInfo->code : ""}}}});
        });
        return {{"id", std::to_string(product.getId())},
            {"sku", meta ? meta->sku : "SKU-" + std::to_string(product.getId())},
            {"name", product.getName()}, {"category", meta ? meta->category : "General"},
            {"unit", meta ? meta->unit : "pcs"}, {"description", meta ? meta->description : ""},
            {"reorderLevel", meta ? meta->reorderLevel : 10}, {"active", true},
            {"price", product.getPrice()}, {"quantity", product.getQuantity()},
            {"stock", stock}, {"totalQuantity", std::accumulate(stock.begin(), stock.end(), 0,
                [](int sum, const json& row) { return sum + row.value("quantity", 0); })}};
    }

    json warehouseJson(const Warehouse& warehouse) const {
        const WarehouseMeta* meta = findWarehouseMeta(warehouse.getId());
        json stock = json::array();
        warehouse.forEachStoredProduct([this, &stock, &warehouse](int productId, const std::string&, int quantity) {
            const Product* product = inventory.searchProduct(productId);
            if (!product) return;
            stock.push_back({{"id", std::to_string(warehouse.getId()) + "-" + std::to_string(productId)},
                {"productId", std::to_string(productId)}, {"warehouseId", std::to_string(warehouse.getId())},
                {"quantity", quantity}, {"product", {{"id", std::to_string(productId)},
                    {"name", product->getName()}, {"sku", findProductMeta(productId) ?
                        findProductMeta(productId)->sku : ""}, {"unit", findProductMeta(productId) ?
                        findProductMeta(productId)->unit : "pcs"}}}});
        });
        std::set<int> connectedWarehouses;
        supplyChain.forEachRoute([&](int from, int to) {
            if (from == warehouse.getId()) connectedWarehouses.insert(to);
            else if (to == warehouse.getId()) connectedWarehouses.insert(from);
        });
        const int routeCount = static_cast<int>(connectedWarehouses.size());
        return {{"id", std::to_string(warehouse.getId())},
            {"code", meta ? meta->code : "WH-" + std::to_string(warehouse.getId())},
            {"name", warehouse.getName()}, {"address", meta && !meta->address.empty() ?
                meta->address : warehouse.getLocation()},
            {"city", warehouse.getLocation()}, {"capacity", warehouse.getCapacity()},
            {"usedCapacity", warehouse.getUsedCapacity()}, {"active", true}, {"stock", stock},
            {"_count", {{"outboundRoutes", routeCount}, {"inboundRoutes", routeCount}}}};
    }

    json supplierJson(const SupplierRecord& supplier) const {
        int orderCount = 0;
        for (std::size_t i = 0; i < purchaseOrders.size(); ++i)
            if (purchaseOrders[i].supplierId == supplier.id) ++orderCount;
        return {{"id", supplier.id}, {"code", supplier.code}, {"name", supplier.name},
            {"email", supplier.email}, {"phone", supplier.phone}, {"address", supplier.address},
            {"active", supplier.active}, {"createdAt", supplier.createdAt},
            {"_count", {{"orders", orderCount}}}};
    }

    json orderJson(const PurchaseRecord& order) const {
        const SupplierRecord* supplier = nullptr;
        for (std::size_t i = 0; i < suppliers.size(); ++i)
            if (suppliers[i].id == order.supplierId) supplier = &suppliers[i];
        const Warehouse* warehouse = warehouses.searchWarehouse(order.warehouseId);
        json items = json::array();
        for (std::size_t i = 0; i < order.items.size(); ++i) {
            const PurchaseItemRecord& item = order.items[i];
            const Product* product = inventory.searchProduct(item.productId);
            items.push_back({{"id", item.id}, {"productId", std::to_string(item.productId)},
                {"quantityOrdered", item.quantityOrdered}, {"quantityReceived", item.quantityReceived},
                {"product", product ? productJson(*product) : json::object()}});
        }
        return {{"id", order.id}, {"orderNumber", order.orderNumber},
            {"supplierId", order.supplierId}, {"warehouseId", std::to_string(order.warehouseId)},
            {"status", order.status}, {"expectedAt", order.expectedAt.empty() ? json(nullptr) : json(order.expectedAt)},
            {"notes", order.notes}, {"createdAt", order.createdAt},
            {"supplier", supplier ? supplierJson(*supplier) : json::object()},
            {"warehouse", warehouse ? warehouseJson(*warehouse) : json::object()}, {"items", items}};
    }

    json shipmentJson(const Shipment& shipment) {
        ShipmentMeta& meta = ensureShipmentMeta(shipment);
        Warehouse* origin = warehouses.searchWarehouse(shipment.path[0]);
        Warehouse* destination = warehouses.searchWarehouse(shipment.path[shipment.length - 1]);
        const Product* product = inventory.searchProduct(shipment.productId);
        json path = json::array();
        for (int i = 0; i < shipment.length; ++i) path.push_back(std::to_string(shipment.path[i]));
        json events = json::array();
        for (std::size_t i = meta.events.size(); i > 0; --i) {
            const ShipmentEventRecord& event = meta.events[i - 1];
            events.push_back({{"id", meta.trackingNumber + "-" + std::to_string(i)},
                {"status", event.status}, {"note", event.note}, {"createdAt", event.createdAt}});
        }
        json item = {{"productId", std::to_string(shipment.productId)}, {"quantity", shipment.quantity},
            {"product", product ? productJson(*product) : json::object()}};
        return {{"id", std::to_string(shipment.id)}, {"trackingNumber", meta.trackingNumber},
            {"originId", std::to_string(shipment.path[0])},
            {"destinationId", std::to_string(shipment.path[shipment.length - 1])},
            {"origin", origin ? warehouseJson(*origin) : json::object()},
            {"destination", destination ? warehouseJson(*destination) : json::object()},
            {"status", apiStatus(shipment.status)}, {"routePath", path},
            {"currentLeg", shipment.step},
            {"expectedAt", meta.expectedAt.empty() ? json(nullptr) : json(meta.expectedAt)},
            {"delayReason", meta.delayReason},
            {"createdAt", meta.createdAt},
            {"deliveredAt", meta.deliveredAt.empty() ? json(nullptr) : json(meta.deliveredAt)},
            {"items", json::array({item})}, {"events", events}};
    }

    json movementJson(const MovementRecord& movement) const {
        const Product* product = inventory.searchProduct(movement.productId);
        const Warehouse* warehouse = warehouses.searchWarehouse(movement.warehouseId);
        const UserRecord* user = nullptr;
        for (std::size_t i = 0; i < users.size(); ++i)
            if (users[i].id == movement.userId) user = &users[i];
        return {{"id", movement.id}, {"productId", std::to_string(movement.productId)},
            {"warehouseId", std::to_string(movement.warehouseId)}, {"kind", movement.kind},
            {"delta", movement.delta}, {"note", movement.note}, {"reference", movement.reference},
            {"createdAt", movement.createdAt},
            {"product", product ? json{{"id", std::to_string(product->getId())},
                {"name", product->getName()}, {"sku", findProductMeta(product->getId()) ?
                    findProductMeta(product->getId())->sku : ""}} : json::object()},
            {"warehouse", warehouse ? json{{"id", std::to_string(warehouse->getId())},
                {"name", warehouse->getName()}, {"city", warehouse->getLocation()}} : json::object()},
            {"user", user ? json{{"id", user->id}, {"name", user->name}} : json{{"name", "System"}}}};
    }

    json routeJson(const RouteRecord& route) const {
        const Warehouse* from = warehouses.searchWarehouse(route.fromId);
        const Warehouse* to = warehouses.searchWarehouse(route.toId);
        return {{"id", std::to_string(route.fromId) + "-" + std::to_string(route.toId)},
            {"fromWarehouseId", std::to_string(route.fromId)},
            {"toWarehouseId", std::to_string(route.toId)},
            {"distanceKm", route.distanceKm}, {"transitHours", route.transitHours},
            {"active", route.active},
            {"fromWarehouse", from ? json{{"id", std::to_string(from->getId())},
                {"code", findWarehouseMeta(from->getId()) ? findWarehouseMeta(from->getId())->code : ""},
                {"name", from->getName()}, {"city", from->getLocation()}} : json::object()},
            {"toWarehouse", to ? json{{"id", std::to_string(to->getId())},
                {"code", findWarehouseMeta(to->getId()) ? findWarehouseMeta(to->getId())->code : ""},
                {"name", to->getName()}, {"city", to->getLocation()}} : json::object()}};
    }

    json auditJson(const AuditRecord& audit) const {
        const UserRecord* user = nullptr;
        for (std::size_t i = 0; i < users.size(); ++i)
            if (users[i].id == audit.userId) user = &users[i];
        return {{"id", audit.id}, {"action", audit.action}, {"entity", audit.entity},
            {"entityId", audit.entityId}, {"details", audit.details}, {"createdAt", audit.createdAt},
            {"user", user ? json{{"name", user->name}, {"email", user->email}} :
                json{{"name", "System"}, {"email", ""}}}};
    }

    json stockRows() const {
        json result = json::array();
        warehouses.forEachWarehouse([this, &result](const Warehouse& warehouse) {
            warehouse.forEachStoredProduct([this, &result, &warehouse](int productId, const std::string&, int quantity) {
                const Product* product = inventory.searchProduct(productId);
                if (!product) return;
                const ProductMeta* productInfo = findProductMeta(productId);
                const WarehouseMeta* warehouseInfo = findWarehouseMeta(warehouse.getId());
                result.push_back({{"id", std::to_string(warehouse.getId()) + "-" + std::to_string(productId)},
                    {"quantity", quantity},
                    {"product", {{"id", std::to_string(productId)}, {"sku", productInfo ? productInfo->sku : ""},
                        {"name", product->getName()}, {"unit", productInfo ? productInfo->unit : "pcs"},
                        {"reorderLevel", productInfo ? productInfo->reorderLevel : 10}}},
                    {"warehouse", {{"id", std::to_string(warehouse.getId())},
                        {"code", warehouseInfo ? warehouseInfo->code : ""},
                        {"name", warehouse.getName()}, {"city", warehouse.getLocation()}}}});
            });
        });
        return result;
    }

    int newWarehouseId() const {
        int largest = 0;
        warehouses.forEachWarehouse([&largest](const Warehouse& warehouse) {
            largest = std::max(largest, warehouse.getId());
        });
        if (largest == 2147483647) throw ApiError(409, "No additional warehouse IDs are available.");
        return largest + 1;
    }

    int newProductId() const {
        int largest = 0;
        inventory.forEachProduct([&largest](const Product& product) {
            largest = std::max(largest, product.getId());
        });
        if (largest == 2147483647) throw ApiError(409, "No additional product IDs are available.");
        return largest + 1;
    }

    void initializeUsers() {
        if (!users.empty()) return;
        const char* configuredEmail = std::getenv("GSC_ADMIN_EMAIL");
        const char* configuredPassword = std::getenv("GSC_ADMIN_PASSWORD");
        const std::string email = lower(configuredEmail ? configuredEmail : "admin@example.com");
        const std::string password = configuredPassword ? configuredPassword : "ChangeMe123!";
        if (password.size() < 10) throw std::runtime_error("GSC_ADMIN_PASSWORD must be at least 10 characters.");
        users.push_back({"admin-1", "Operations Admin", email, "ADMIN",
            makePasswordHash(password), true, nowIso()});
        if (!configuredPassword)
            std::cerr << "Development admin created: " << email
                      << " / ChangeMe123! (set GSC_ADMIN_PASSWORD before deployment)\n";
    }

    void configureRoutes() {
        httplib::Headers defaultHeaders;
        defaultHeaders.insert(std::make_pair("X-Content-Type-Options", "nosniff"));
        defaultHeaders.insert(std::make_pair("X-Frame-Options", "DENY"));
        defaultHeaders.insert(std::make_pair("Referrer-Policy", "strict-origin-when-cross-origin"));
        defaultHeaders.insert(std::make_pair("Content-Security-Policy",
            "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; "
            "font-src 'self' https://fonts.gstatic.com data:; img-src 'self' data:; connect-src 'self'; "
            "object-src 'none'; base-uri 'self'; frame-ancestors 'none'"));
        server.set_default_headers(defaultHeaders);

        addGet("/api/health", [this](const httplib::Request&, httplib::Response& response) {
            sendJson(response, 200, json{{"status", "ok"}});
        });

        addPost("/api/auth/login", [this](const httplib::Request& request, httplib::Response& response) {
            const json body = parseBody(request);
            const std::string email = lower(getString(body, "email", "", 254));
            const std::string password = getString(body, "password", "", 200);
            if (email.empty() || email.find('@') == std::string::npos)
                throw ApiError(400, "Enter a valid email address.");
            const int failures = loginFailures[request.remote_addr];
            if (failures >= 10) throw ApiError(429, "Too many sign-in attempts. Wait and try again.");
            UserRecord* user = nullptr;
            for (std::size_t i = 0; i < users.size(); ++i)
                if (users[i].email == email && users[i].active) user = &users[i];
            if (!user || !verifyPassword(password, user->passwordHash)) {
                loginFailures[request.remote_addr] = failures + 1;
                throw ApiError(401, "Email or password is incorrect.");
            }
            loginFailures.erase(request.remote_addr);
            const std::string token = randomHex(32);
            sessions[token] = {user->id, std::time(nullptr) + 12 * 60 * 60};
            sendJson(response, 200, json{{"token", token}, {"user", userJson(*user)}});
        });

        addGet("/api/auth/me", [this](const httplib::Request& request, httplib::Response& response) {
            sendJson(response, 200, userJson(requireUser(request)));
        });

        addGet("/api/users", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord& actor = requireUser(request);
            requireRole(actor, {"ADMIN"});
            json rows = json::array();
            for (std::size_t i = 0; i < users.size(); ++i) rows.push_back(userJson(users[i]));
            sendJson(response, 200, rows);
        });

        addPost("/api/users", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN"});
            const json body = parseBody(request);
            const std::string name = getString(body, "name", "", 160);
            const std::string email = lower(getString(body, "email", "", 254));
            const std::string password = getString(body, "password", "", 100);
            const std::string role = roleName(getString(body, "role", "INVENTORY", 30));
            if (name.empty() || email.find('@') == std::string::npos || password.size() < 10)
                throw ApiError(400, "Enter a name, valid email, and password with at least 10 characters.");
            for (std::size_t i = 0; i < users.size(); ++i)
                if (users[i].email == email) throw ApiError(409, "That email is already registered.");
            UserRecord user = {"user-" + randomHex(8), name, email, role,
                makePasswordHash(password), true, nowIso()};
            users.push_back(user);
            recordAudit(actor, "created", "user", user.id, user.email);
            saveSidecar();
            sendJson(response, 201, userJson(user));
        });

        addPatch("/api/users/([A-Za-z0-9_-]+)", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN"});
            const std::string id = request.matches[1];
            UserRecord* user = findUser(id);
            if (!user) throw ApiError(404, "User not found.");
            const json body = parseBody(request);
            const std::string name = getString(body, "name", user->name, 160);
            const std::string role = roleName(getString(body, "role", user->role, 30));
            const bool active = getBoolean(body, "active", user->active);
            if (name.empty()) throw ApiError(400, "A user name is required.");
            if (id == actor.id && !active) throw ApiError(409, "You cannot deactivate your own account.");
            if (user->active && user->role == "ADMIN" && (!active || role != "ADMIN")) {
                int activeAdmins = 0;
                for (std::size_t i = 0; i < users.size(); ++i)
                    if (users[i].active && users[i].role == "ADMIN") ++activeAdmins;
                if (activeAdmins <= 1) throw ApiError(409, "At least one active administrator must remain.");
            }
            user->name = name;
            user->role = role;
            user->active = active;
            recordAudit(actor, "updated", "user", user->id, user->email);
            saveSidecar();
            sendJson(response, 200, userJson(*user));
        });

        addGet("/api/dashboard", [this](const httplib::Request& request, httplib::Response& response) {
            requireUser(request);
            long long totalUnits = 0;
            inventory.forEachProduct([&totalUnits](const Product& product) {
                totalUnits += product.getQuantity();
            });
            std::map<int, int> shipmentCounts;
            supplyChain.forEachShipment([&shipmentCounts](const Shipment& shipment) {
                ++shipmentCounts[shipment.status == "In Transit" ? 0 : shipment.status == "Delayed" ? 1 : 2];
            });
            int openOrders = 0;
            for (std::size_t i = 0; i < purchaseOrders.size(); ++i)
                if (purchaseOrders[i].status == "OPEN" || purchaseOrders[i].status == "PARTIAL") ++openOrders;
            json productRows = json::array();
            inventory.forEachProduct([&](const Product& product) {
                const json item = productJson(product);
                if (item["totalQuantity"].get<int>() <= item["reorderLevel"].get<int>())
                    productRows.push_back(item);
            });
            std::sort(productRows.begin(), productRows.end(), [](const json& a, const json& b) {
                return a["totalQuantity"].get<int>() < b["totalQuantity"].get<int>();
            });
            const std::time_t now = std::time(nullptr);
            const std::time_t dayStart = now - (now % (24 * 60 * 60));
            json movementByDay = json::array();
            for (int offset = 6; offset >= 0; --offset) {
                const std::time_t at = dayStart - offset * 24 * 60 * 60;
                const std::tm day = utcTime(at);
                char label[8];
                std::strftime(label, sizeof(label), "%a", &day);
                char date[16];
                std::strftime(date, sizeof(date), "%Y-%m-%d", &day);
                long long received = 0, dispatched = 0;
                for (std::size_t i = 0; i < movements.size(); ++i) {
                    if (dateKey(movements[i].createdAt) != date) continue;
                    if (movements[i].delta > 0) received += movements[i].delta;
                    else dispatched += -movements[i].delta;
                }
                movementByDay.push_back({{"day", label}, {"received", received}, {"dispatched", dispatched}});
            }
            json latestShipments = json::array();
            supplyChain.forEachShipment([&](const Shipment& shipment) {
                if (latestShipments.size() < 6) latestShipments.push_back(shipmentJson(shipment));
            });
            json latestMovements = json::array();
            for (std::size_t i = 0; i < movements.size() && latestMovements.size() < 6; ++i)
                latestMovements.push_back(movementJson(movements[i]));
            sendJson(response, 200, json{
                {"metrics", {{"products", inventory.productCount()}, {"warehouses", warehouses.warehouseCount()},
                    {"totalUnits", totalUnits}, {"inTransit", shipmentCounts[0] + shipmentCounts[1]},
                    {"delayed", shipmentCounts[1]}, {"openOrders", openOrders},
                    {"lowStock", productRows.size()}}},
                {"shipmentCounts", {{"in_transit", shipmentCounts[0]}, {"delayed", shipmentCounts[1]},
                    {"delivered", shipmentCounts[2]}}},
                {"movementByDay", movementByDay},
                {"lowStock", productRows.size() > 5 ? json(productRows.begin(), productRows.begin() + 5) : productRows},
                {"recentShipments", latestShipments}, {"recentMovements", latestMovements}});
        });

        addGet("/api/products", [this](const httplib::Request& request, httplib::Response& response) {
            requireUser(request);
            const std::string search = lower(request.has_param("search") ? request.get_param_value("search") : "");
            json rows = json::array();
            inventory.forEachProduct([&](const Product& product) {
                const json row = productJson(product);
                const std::string index = lower(row["name"].get<std::string>() + " " +
                    row["sku"].get<std::string>() + " " + row["category"].get<std::string>());
                if (search.empty() || index.find(search) != std::string::npos) rows.push_back(row);
            });
            sendJson(response, 200, rows);
        });

        addPost("/api/products", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "INVENTORY"});
            const json body = parseBody(request);
            const std::string sku = getString(body, "sku", "", 80);
            const std::string name = getString(body, "name", "", 160);
            const std::string category = getString(body, "category", "General", 80);
            const std::string unit = getString(body, "unit", "pcs", 30);
            const std::string description = getString(body, "description", "", 1000);
            const int reorder = getInteger(body, "reorderLevel", 10, 0, 1000000);
            if (sku.empty() || name.empty()) throw ApiError(400, "SKU and product name are required.");
            for (std::size_t i = 0; i < productMetadata.size(); ++i)
                if (lower(productMetadata[i].sku) == lower(sku)) throw ApiError(409, "A product with that SKU already exists.");
            double price = 0.0;
            if (body.contains("price")) {
                if (!body["price"].is_number()) throw ApiError(400, "Price must be a number.");
                price = body["price"].get<double>();
            }
            if (!(price >= 0) || price > 1000000000.0) throw ApiError(400, "Price must be a non-negative number.");
            const int id = newProductId();
            if (!inventory.addProduct(Product(id, name, price, 0)))
                throw ApiError(409, "A product with that ID already exists.");
            productMetadata.push_back({id, sku, category, unit, description, reorder});
            recordAudit(actor, "created", "product", std::to_string(id), sku);
            saveAll();
            sendJson(response, 201, productJson(*inventory.searchProduct(id)));
        });

        addPatch("/api/products/([0-9]+)", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "INVENTORY"});
            const int id = parseId(request.matches[1], "Product ID");
            Product* product = inventory.searchProduct(id);
            ProductMeta* meta = findProductMeta(id);
            if (!product || !meta) throw ApiError(404, "Product not found.");
            const json body = parseBody(request);
            const std::string name = getString(body, "name", product->getName(), 160);
            const std::string category = getString(body, "category", meta->category, 80);
            const std::string unit = getString(body, "unit", meta->unit, 30);
            const std::string description = getString(body, "description", meta->description, 1000);
            const int reorderLevel = getInteger(body, "reorderLevel", meta->reorderLevel, 0, 1000000);
            if (name.empty() || category.empty() || unit.empty())
                throw ApiError(400, "Product name, category, and unit are required.");
            double price = product->getPrice();
            if (body.contains("price")) {
                if (!body["price"].is_number()) throw ApiError(400, "Price must be a number.");
                price = body["price"].get<double>();
                if (!(price >= 0) || price > 1000000000.0) throw ApiError(400, "Price is outside the allowed range.");
            }
            product->setName(name);
            product->setPrice(price);
            meta->category = category;
            meta->unit = unit;
            meta->description = description;
            meta->reorderLevel = reorderLevel;
            recordAudit(actor, "updated", "product", std::to_string(id), meta->sku);
            saveAll();
            sendJson(response, 200, productJson(*product));
        });

        addGet("/api/warehouses", [this](const httplib::Request& request, httplib::Response& response) {
            requireUser(request);
            json rows = json::array();
            warehouses.forEachWarehouse([&](const Warehouse& warehouse) {
                rows.push_back(warehouseJson(warehouse));
            });
            sendJson(response, 200, rows);
        });

        addPost("/api/warehouses", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "INVENTORY"});
            const json body = parseBody(request);
            const std::string code = getString(body, "code", "", 80);
            const std::string name = getString(body, "name", "", 160);
            const std::string address = getString(body, "address", "", 240);
            const std::string city = getString(body, "city", "", 120);
            const int capacity = getInteger(body, "capacity", -1, 1, 1000000000);
            if (code.empty() || name.empty() || city.empty()) throw ApiError(400, "Warehouse code, name, and city are required.");
            for (std::size_t i = 0; i < warehouseMetadata.size(); ++i)
                if (lower(warehouseMetadata[i].code) == lower(code)) throw ApiError(409, "A warehouse with that code already exists.");
            const int id = newWarehouseId();
            if (!warehouses.addWarehouse(id, name, city, capacity))
                throw ApiError(409, "Could not create the warehouse.");
            warehouseMetadata.push_back({id, code, address});
            recordAudit(actor, "created", "warehouse", std::to_string(id), code);
            saveAll();
            sendJson(response, 201, warehouseJson(*warehouses.searchWarehouse(id)));
        });

        addPatch("/api/warehouses/([0-9]+)", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "INVENTORY"});
            const int id = parseId(request.matches[1], "Warehouse ID");
            Warehouse* warehouse = warehouses.searchWarehouse(id);
            if (!warehouse) throw ApiError(404, "Warehouse not found.");
            const json body = parseBody(request);
            const int capacity = getInteger(body, "capacity", warehouse->getCapacity(), 1, 1000000000);
            if (capacity < warehouse->getUsedCapacity()) throw ApiError(409, "Capacity cannot be lower than the stock already held here.");
            const std::string name = getString(body, "name", warehouse->getName(), 160);
            const std::string city = getString(body, "city", warehouse->getLocation(), 120);
            WarehouseMeta* meta = findWarehouseMeta(id);
            if (!meta) throw ApiError(500, "Warehouse metadata is unavailable.");
            const std::string address = getString(body, "address", meta->address, 240);
            if (name.empty() || city.empty()) throw ApiError(400, "Warehouse name and city are required.");
            warehouse->setCapacity(capacity);
            warehouse->setName(name);
            warehouse->setLocation(city);
            meta->address = address;
            recordAudit(actor, "updated", "warehouse", std::to_string(id), meta->code);
            saveAll();
            sendJson(response, 200, warehouseJson(*warehouse));
        });

        addPost("/api/inventory/adjustments", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "INVENTORY"});
            const json body = parseBody(request);
            const int productId = parseId(getString(body, "productId", "", 40), "Product ID");
            const int warehouseId = parseId(getString(body, "warehouseId", "", 40), "Warehouse ID");
            const int delta = getInteger(body, "delta", -1, -1000000, 1000000);
            if (delta == 0) throw ApiError(400, "Stock change cannot be zero.");
            const std::string note = getString(body, "note", "", 240);
            Product* product = inventory.searchProduct(productId);
            Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
            if (!product) throw ApiError(404, "Product not found.");
            if (!warehouse) throw ApiError(404, "Warehouse not found.");
            if (delta > 0) {
                if (product->getQuantity() > 1000000000 - delta)
                    throw ApiError(409, "Inventory quantity exceeds the supported limit.");
                if (!warehouse->receiveProduct(productId, product->getName(), delta))
                    throw ApiError(409, "This adjustment would exceed warehouse capacity.");
            } else {
                if (-delta > warehouse->productQuantity(productId))
                    throw ApiError(409, "This adjustment would make warehouse stock negative.");
                if (product->getQuantity() < warehouses.storedProductQuantity(productId) +
                        supplyChain.inTransitProductQuantity(productId))
                    throw ApiError(409, "This adjustment would reduce total stock below allocated stock.");
                if (!warehouse->removeProduct(productId, -delta))
                    throw ApiError(409, "Could not remove that amount of stock.");
            }
            product->setQuantity(product->getQuantity() + delta);
            recordMovement(actor, productId, warehouseId, "ADJUSTMENT", delta, note);
            recordAudit(actor, "adjusted", "inventory", std::to_string(productId),
                findProductMeta(productId)->sku + ": " + std::to_string(delta));
            saveAll();
            sendJson(response, 201, json{{"id", movements.front().id}, {"delta", delta},
                {"kind", "ADJUSTMENT"}, {"createdAt", movements.front().createdAt}});
        });

        addGet("/api/inventory/movements", [this](const httplib::Request& request, httplib::Response& response) {
            requireUser(request);
            json rows = json::array();
            for (std::size_t i = 0; i < movements.size() && i < 100; ++i)
                rows.push_back(movementJson(movements[i]));
            sendJson(response, 200, rows);
        });

        addGet("/api/suppliers", [this](const httplib::Request& request, httplib::Response& response) {
            requireUser(request);
            json rows = json::array();
            for (std::size_t i = 0; i < suppliers.size(); ++i)
                if (suppliers[i].active) rows.push_back(supplierJson(suppliers[i]));
            sendJson(response, 200, rows);
        });

        addPost("/api/suppliers", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "PROCUREMENT"});
            const json body = parseBody(request);
            const std::string code = getString(body, "code", "", 80);
            const std::string name = getString(body, "name", "", 160);
            const std::string email = lower(getString(body, "email", "", 254));
            const std::string phone = getString(body, "phone", "", 60);
            const std::string address = getString(body, "address", "", 240);
            if (code.empty() || name.empty()) throw ApiError(400, "Supplier code and company name are required.");
            if (!email.empty() && email.find('@') == std::string::npos) throw ApiError(400, "Enter a valid supplier email.");
            for (std::size_t i = 0; i < suppliers.size(); ++i)
                if (lower(suppliers[i].code) == lower(code)) throw ApiError(409, "A supplier with that code already exists.");
            SupplierRecord supplier = {std::to_string(nextSupplierId++), code, name, email,
                phone, address, true, nowIso()};
            suppliers.push_back(supplier);
            recordAudit(actor, "created", "supplier", supplier.id, supplier.code);
            saveSidecar();
            sendJson(response, 201, supplierJson(supplier));
        });

        addPatch("/api/suppliers/([0-9]+)", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "PROCUREMENT"});
            const std::string id = request.matches[1];
            SupplierRecord* supplier = findSupplier(id);
            if (!supplier) throw ApiError(404, "Supplier not found.");
            const json body = parseBody(request);
            const std::string name = getString(body, "name", supplier->name, 160);
            const std::string email = lower(getString(body, "email", supplier->email, 254));
            const std::string phone = getString(body, "phone", supplier->phone, 60);
            const std::string address = getString(body, "address", supplier->address, 240);
            if (name.empty()) throw ApiError(400, "A supplier name is required.");
            if (!email.empty() && email.find('@') == std::string::npos)
                throw ApiError(400, "Enter a valid supplier email.");
            supplier->name = name;
            supplier->email = email;
            supplier->phone = phone;
            supplier->address = address;
            recordAudit(actor, "updated", "supplier", id, supplier->code);
            saveSidecar();
            sendJson(response, 200, supplierJson(*supplier));
        });

        addGet("/api/purchase-orders", [this](const httplib::Request& request, httplib::Response& response) {
            requireUser(request);
            json rows = json::array();
            for (std::size_t i = purchaseOrders.size(); i > 0; --i)
                rows.push_back(orderJson(purchaseOrders[i - 1]));
            sendJson(response, 200, rows);
        });

        addPost("/api/purchase-orders", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "PROCUREMENT"});
            const json body = parseBody(request);
            const std::string supplierId = getString(body, "supplierId", "", 80);
            const int warehouseId = parseId(getString(body, "warehouseId", "", 40), "Warehouse ID");
            const std::string expectedAt = getString(body, "expectedAt", "", 40);
            const std::string notes = getString(body, "notes", "", 500);
            SupplierRecord* supplier = findSupplier(supplierId);
            if (!supplier) throw ApiError(404, "Supplier not found.");
            Warehouse* warehouse = warehouses.searchWarehouse(warehouseId);
            if (!warehouse) throw ApiError(404, "Warehouse not found.");
            if (!body.contains("items") || !body["items"].is_array() ||
                body["items"].empty() || body["items"].size() > 100)
                throw ApiError(400, "Add at least one product to the purchase order.");
            PurchaseRecord order;
            order.id = "po-" + std::to_string(nextOrderId++);
            std::ostringstream number;
            number << "PO-" << std::time(nullptr) << "-" << std::setfill('0') << std::setw(4) << nextOrderId;
            order.orderNumber = number.str();
            order.supplierId = supplierId;
            order.warehouseId = warehouseId;
            order.status = "OPEN";
            order.expectedAt = expectedAt;
            order.notes = notes;
            order.createdAt = nowIso();
            for (std::size_t i = 0; i < body["items"].size(); ++i) {
                const json& item = body["items"][i];
                const int productId = parseId(getString(item, "productId", "", 40), "Product ID");
                const int quantity = getInteger(item, "quantityOrdered", -1, 1, 1000000000);
                if (!inventory.searchProduct(productId)) throw ApiError(404, "One of the selected products was not found.");
                for (std::size_t j = 0; j < order.items.size(); ++j)
                    if (order.items[j].productId == productId)
                        throw ApiError(400, "Each product can appear only once in a purchase order.");
                order.items.push_back({"line-" + randomHex(6), productId, quantity, 0});
            }
            purchaseOrders.push_back(order);
            recordAudit(actor, "created", "purchase order", order.id, order.orderNumber);
            saveSidecar();
            sendJson(response, 201, orderJson(order));
        });

        addPost("/api/purchase-orders/([A-Za-z0-9_-]+)/receive", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "INVENTORY", "PROCUREMENT"});
            PurchaseRecord* order = findOrder(request.matches[1]);
            if (!order) throw ApiError(404, "Purchase order not found.");
            if (order->status == "RECEIVED" || order->status == "CANCELLED")
                throw ApiError(409, "This purchase order is already closed.");
            const json body = parseBody(request);
            if (!body.contains("items") || !body["items"].is_array() || body["items"].empty())
                throw ApiError(400, "Select at least one order line to receive.");
            Warehouse* warehouse = warehouses.searchWarehouse(order->warehouseId);
            if (!warehouse) throw ApiError(404, "Receiving warehouse not found.");
            long long addCapacity = 0;
            std::set<std::string> receivedItems;
            for (std::size_t i = 0; i < body["items"].size(); ++i) {
                const std::string itemId = getString(body["items"][i], "itemId", "", 80);
                if (!receivedItems.insert(itemId).second)
                    throw ApiError(400, "Each order line can appear only once in a receipt.");
                const int quantity = getInteger(body["items"][i], "quantity", -1, 1, 1000000000);
                PurchaseItemRecord* line = nullptr;
                for (std::size_t j = 0; j < order->items.size(); ++j)
                    if (order->items[j].id == itemId) line = &order->items[j];
                if (!line) throw ApiError(400, "A receipt line does not belong to this purchase order.");
                if (line->quantityReceived + quantity > line->quantityOrdered)
                    throw ApiError(409, "Received quantity cannot exceed the quantity ordered.");
                Product* product = inventory.searchProduct(line->productId);
                if (!product) throw ApiError(409, "A product on this order is no longer available.");
                if (product->getQuantity() > 1000000000 - quantity)
                    throw ApiError(409, "The received quantity would exceed the supported inventory limit.");
                if (!warehouse->canStoreProduct(product->getId(), product->getName(), quantity))
                    throw ApiError(409, "This order cannot be stored in the selected warehouse.");
                addCapacity += quantity;
            }
            if (static_cast<long long>(warehouse->getUsedCapacity()) + addCapacity > warehouse->getCapacity())
                throw ApiError(409, "Receiving this order would exceed warehouse capacity.");
            std::vector<std::pair<int, int> > receipt;
            for (std::size_t i = 0; i < body["items"].size(); ++i) {
                const std::string itemId = getString(body["items"][i], "itemId", "", 80);
                const int quantity = getInteger(body["items"][i], "quantity", -1, 1, 1000000000);
                for (std::size_t j = 0; j < order->items.size(); ++j) if (order->items[j].id == itemId)
                    receipt.push_back(std::make_pair(static_cast<int>(j), quantity));
            }
            for (std::size_t i = 0; i < receipt.size(); ++i) {
                PurchaseItemRecord& line = order->items[receipt[i].first];
                Product* product = inventory.searchProduct(line.productId);
                if (!warehouse->receiveProduct(product->getId(), product->getName(), receipt[i].second))
                    throw ApiError(409, "Could not add received items to warehouse stock.");
                product->setQuantity(product->getQuantity() + receipt[i].second);
                line.quantityReceived += receipt[i].second;
                recordMovement(actor, product->getId(), warehouse->getId(),
                    "PURCHASE_RECEIPT", receipt[i].second, "", order->orderNumber);
            }
            bool complete = true;
            for (std::size_t i = 0; i < order->items.size(); ++i)
                if (order->items[i].quantityReceived != order->items[i].quantityOrdered) complete = false;
            order->status = complete ? "RECEIVED" : "PARTIAL";
            recordAudit(actor, "received", "purchase order", order->id, order->orderNumber);
            saveAll();
            sendJson(response, 200, orderJson(*order));
        });

        addPost("/api/purchase-orders/([A-Za-z0-9_-]+)/cancel", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "PROCUREMENT"});
            PurchaseRecord* order = findOrder(request.matches[1]);
            if (!order) throw ApiError(404, "Purchase order not found.");
            if (order->status == "RECEIVED" || order->status == "CANCELLED")
                throw ApiError(409, "This purchase order is already closed.");
            order->status = "CANCELLED";
            recordAudit(actor, "cancelled", "purchase order", order->id, order->orderNumber);
            saveSidecar();
            sendJson(response, 200, orderJson(*order));
        });

        addGet("/api/routes", [this](const httplib::Request& request, httplib::Response& response) {
            requireUser(request);
            json rows = json::array();
            for (std::size_t i = 0; i < routeMetadata.size(); ++i)
                rows.push_back(routeJson(routeMetadata[i]));
            sendJson(response, 200, rows);
        });

        addPost("/api/routes", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "LOGISTICS"});
            const json body = parseBody(request);
            const int from = parseId(getString(body, "fromWarehouseId", "", 40), "Origin warehouse ID");
            const int to = parseId(getString(body, "toWarehouseId", "", 40), "Destination warehouse ID");
            const int distance = getInteger(body, "distanceKm", 0, 0, 1000000);
            const int hours = getInteger(body, "transitHours", -1, 1, 1000000);
            if (from == to) throw ApiError(400, "A route must connect two different warehouses.");
            if (!warehouses.searchWarehouse(from) || !warehouses.searchWarehouse(to))
                throw ApiError(404, "Both warehouses must exist.");
            RouteRecord* existing = findRoute(from, to);
            if (existing && existing->active) throw ApiError(409, "That route already exists.");
            if (!supplyChain.addRoute(from, to)) throw ApiError(409, "Could not add the route.");
            if (existing) {
                for (std::size_t i = 0; i < routeMetadata.size(); ++i) {
                    RouteRecord& route = routeMetadata[i];
                    if ((route.fromId == from && route.toId == to) ||
                        (route.fromId == to && route.toId == from)) {
                        route.active = true;
                        route.distanceKm = distance;
                        route.transitHours = hours;
                    }
                }
            } else routeMetadata.push_back({from, to, distance, hours, true});
            recordAudit(actor, "created", "route", std::to_string(from) + "-" + std::to_string(to));
            saveAll();
            const RouteRecord result = {from, to, distance, hours, true};
            sendJson(response, 201, routeJson(result));
        });

        addPatch("/api/routes/([0-9]+)-([0-9]+)", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "LOGISTICS"});
            const int from = parseId(request.matches[1], "Origin warehouse ID");
            const int to = parseId(request.matches[2], "Destination warehouse ID");
            RouteRecord* route = findRouteExact(from, to);
            if (!route) route = findRoute(from, to);
            if (!route) throw ApiError(404, "Route not found.");
            const json body = parseBody(request);
            const int distance = getInteger(body, "distanceKm", route->distanceKm, 0, 1000000);
            const int hours = getInteger(body, "transitHours", route->transitHours, 1, 1000000);
            const bool active = getBoolean(body, "active", route->active);
            if (active && !route->active) {
                if (!supplyChain.addRoute(from, to)) throw ApiError(409, "Could not re-enable the route.");
            } else if (!active && route->active) {
                if (!supplyChain.removeRoute(from, to)) throw ApiError(409, "An active shipment is using that route.");
            }
            for (std::size_t i = 0; i < routeMetadata.size(); ++i) {
                RouteRecord& matching = routeMetadata[i];
                if ((matching.fromId == from && matching.toId == to) ||
                    (matching.fromId == to && matching.toId == from)) {
                    matching.active = active;
                    matching.distanceKm = distance;
                    matching.transitHours = hours;
                }
            }
            recordAudit(actor, active ? "updated" : "deactivated", "route",
                std::to_string(from) + "-" + std::to_string(to));
            saveAll();
            const RouteRecord result = {from, to, distance, hours, active};
            sendJson(response, 200, routeJson(result));
        });

        addGet("/api/shipments", [this](const httplib::Request& request, httplib::Response& response) {
            requireUser(request);
            json rows = json::array();
            supplyChain.forEachShipment([this, &rows](const Shipment& shipment) {
                rows.push_back(shipmentJson(shipment));
            });
            sendJson(response, 200, rows);
        });

        addPost("/api/shipments", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "LOGISTICS"});
            const json body = parseBody(request);
            const int origin = parseId(getString(body, "originId", "", 40), "Origin warehouse ID");
            const int destination = parseId(getString(body, "destinationId", "", 40), "Destination warehouse ID");
            const std::string expectedAt = getString(body, "expectedAt", "", 40);
            if (origin == destination) throw ApiError(400, "Origin and destination must be different warehouses.");
            if (!body.contains("items") || !body["items"].is_array() ||
                body["items"].size() != 1)
                throw ApiError(400, "A shipment must contain exactly one product in this version.");
            const json& item = body["items"][0];
            const int productId = parseId(getString(item, "productId", "", 40), "Product ID");
            const int quantity = getInteger(item, "quantity", -1, 1, 1000000000);
            int shipmentId = supplyChain.nextShipmentId();
            if (!supplyChain.createShipment(origin, destination, productId, quantity))
                throw ApiError(409, "Could not create shipment. Check origin stock and the active warehouse route.");
            const Shipment* shipment = supplyChain.getShipment(shipmentId);
            if (!shipment) throw ApiError(500, "Shipment was created but could not be retrieved.");
            ShipmentMeta& meta = ensureShipmentMeta(*shipment);
            meta.expectedAt = expectedAt;
            meta.events.push_back({"IN_TRANSIT", "Dispatched from " +
                warehouses.searchWarehouse(origin)->getName(), nowIso()});
            recordMovement(actor, productId, origin, "SHIPMENT_DISPATCH", -quantity, "", meta.trackingNumber);
            recordAudit(actor, "dispatched", "shipment", std::to_string(shipmentId), meta.trackingNumber);
            saveAll();
            sendJson(response, 201, shipmentJson(*shipment));
        });

        addPatch("/api/shipments/([0-9]+)/advance", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "LOGISTICS"});
            const int id = parseId(request.matches[1], "Shipment ID");
            Shipment* shipment = supplyChain.getShipment(id);
            if (!shipment) throw ApiError(404, "Shipment not found.");
            const bool wasDelayed = shipment->status == "Delayed";
            if (wasDelayed)
                supplyChain.updateShipmentStatus(id, false);
            if (!supplyChain.advanceShipment(id)) {
                if (wasDelayed) supplyChain.updateShipmentStatus(id, true);
                throw ApiError(409, "Shipment could not advance. Check its route and destination capacity.");
            }
            ShipmentMeta& meta = ensureShipmentMeta(*shipment);
            const bool delivered = shipment->status == "Delivered";
            const Warehouse* at = warehouses.searchWarehouse(shipment->path[shipment->step]);
            const std::string note = delivered ? "Delivered to " + at->getName() :
                "Arrived at route stop " + std::to_string(shipment->step + 1) +
                " of " + std::to_string(shipment->length);
            meta.events.push_back({apiStatus(shipment->status), note, nowIso()});
            if (delivered) {
                meta.deliveredAt = nowIso();
                recordMovement(actor, shipment->productId, shipment->path[shipment->length - 1],
                    "SHIPMENT_DELIVERY", shipment->quantity, "", meta.trackingNumber);
            }
            recordAudit(actor, delivered ? "delivered" : "advanced", "shipment",
                std::to_string(id), meta.trackingNumber);
            saveAll();
            sendJson(response, 200, shipmentJson(*shipment));
        });

        addPatch("/api/shipments/([0-9]+)/delay", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord actor = requireUser(request);
            requireRole(actor, {"ADMIN", "LOGISTICS"});
            const int id = parseId(request.matches[1], "Shipment ID");
            const json body = parseBody(request);
            const std::string reason = getString(body, "reason", "", 240);
            if (reason.empty()) throw ApiError(400, "Enter a reason for the delay.");
            Shipment* shipment = supplyChain.getShipment(id);
            if (!shipment) throw ApiError(404, "Shipment not found.");
            if (!supplyChain.updateShipmentStatus(id, true))
                throw ApiError(409, "A delivered shipment cannot be delayed.");
            ShipmentMeta& meta = ensureShipmentMeta(*shipment);
            meta.delayReason = reason;
            meta.events.push_back({"DELAYED", reason, nowIso()});
            recordAudit(actor, "delayed", "shipment", std::to_string(id), reason);
            saveAll();
            sendJson(response, 200, shipmentJson(*shipment));
        });

        addGet("/api/reports/stock", [this](const httplib::Request& request, httplib::Response& response) {
            requireUser(request);
            sendJson(response, 200, stockRows());
        });

        addGet("/api/audit", [this](const httplib::Request& request, httplib::Response& response) {
            const UserRecord& actor = requireUser(request);
            requireRole(actor, {"ADMIN"});
            json rows = json::array();
            for (std::size_t i = 0; i < auditRecords.size() && i < 100; ++i)
                rows.push_back(auditJson(auditRecords[i]));
            sendJson(response, 200, rows);
        });

        server.set_payload_max_length(MAX_BODY_SIZE);
        server.set_error_handler([this](const httplib::Request& request, httplib::Response& response) {
            if (request.path.compare(0, 4, "/api") == 0) {
                sendJson(response, response.status == 404 ? 404 : response.status,
                    json{{"error", "API endpoint not found."}});
            } else if (!webRoot.empty()) {
                response.status = 200;
                std::ifstream index(webRoot + "/index.html", std::ios::binary);
                if (!index) {
                    std::cerr << "Could not open the built frontend index file.\n";
                    response.status = 500;
                    response.set_content("The built frontend is unavailable.", "text/plain; charset=utf-8");
                    return;
                }
                std::ostringstream content;
                content << index.rdbuf();
                response.set_content(content.str(), "text/html; charset=utf-8");
            }
        });
    }

public:
    Application()
        : supplyChain(warehouses, inventory), nextSupplierId(1), nextOrderId(1),
          nextMovementId(1), nextAuditId(1), port(4000), webRoot() {
        const char* configuredPort = std::getenv("PORT");
        if (configuredPort && *configuredPort) {
            const int value = std::atoi(configuredPort);
            if (value < 1 || value > 65535) throw std::runtime_error("PORT must be between 1 and 65535.");
            port = value;
        }
        const char* configuredWebRoot = std::getenv("GSC_WEB_ROOT");
        if (configuredWebRoot) webRoot = configuredWebRoot;
        std::string error;
        if (!corePersistence.load(inventory, warehouses, supplyChain, error))
            throw std::runtime_error("C++ data startup error: " + error);
        loadSidecar();
        seedIfEmpty();
        ensureMetadata();
        initializeUsers();
        saveAll();
        configureRoutes();
    }

    bool run() {
        if (!webRoot.empty()) server.set_mount_point("/", webRoot);
        std::cout << "C++ Supply Chain Hub listening on http://127.0.0.1:" << port << '\n';
        if (!server.listen("127.0.0.1", port))
            throw std::runtime_error("Could not listen on 127.0.0.1:" + std::to_string(port) +
                ". Check whether another server is already using this port.");
        return true;
    }
};

}  // namespace

int main() {
    try {
        Application application;
        return application.run() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Fatal application error: " << error.what() << '\n';
        return 1;
    }
}
