#include <iostream>
#include <limits>
#include <cmath>
#include <sstream>
#include <string>
#include "person_d_support.h"
using namespace std;
class Product {
private:
    int id;
    string name;
    double price;
    int quantity;
public:
    Product() : id(0), price(0), quantity(0) {}
    Product(int productId, const string& productName, double productPrice, int stock)
        : id(productId), name(productName), price(productPrice), quantity(stock) {}
    int getId() const { return id; }
    const string& getName() const { return name; }
    double getPrice() const { return price; }
    int getQuantity() const { return quantity; }
    void setQuantity(int stock) { quantity = stock; }

    friend istream& operator>>(istream& input, Product& product);
    friend ostream& operator<<(ostream& output, const Product& product);
};
istream& operator>>(istream& input, Product& product) {
    string line;
    if (!getline(input >> ws, line)) {
        return input;
    }
  stringstream fields(line);
    string idText;
    string name;
    string priceText;
    string quantityText;
    string extra;
    if (!getline(fields, idText, '|') ||
        !getline(fields, name, '|') ||
        !getline(fields, priceText, '|') ||
        !getline(fields, quantityText, '|') ||
        getline(fields, extra) ||
        name.empty()) {
        input.setstate(ios::failbit);
        return input;
    }
    stringstream idInput(idText);
    stringstream priceInput(priceText);
    stringstream quantityInput(quantityText);
    int idValue;
    double priceValue;
    int quantityValue;
    char trailing;
    if (!(idInput >> idValue) || (idInput >> trailing) ||
        !(priceInput >> priceValue) || (priceInput >> trailing) ||
        !(quantityInput >> quantityValue) || (quantityInput >> trailing) ||
        idValue <= 0 || !(priceValue >= 0) || !std::isfinite(priceValue) || quantityValue < 0) {
        input.setstate(ios::failbit);
        return input;
    }
    product = Product(idValue, name, priceValue, quantityValue);
    return input;
}
ostream& operator<<(ostream& output, const Product& product) {
    output << "ID: " << product.id
           << " | Name: " << product.name
           << " | Price: $" << product.price
           << " | Stock: " << product.quantity;
    return output;
}
struct ProductNode {
    Product product;
    ProductNode* next;

    explicit ProductNode(const Product& value) : product(value), next(nullptr) {}
};
class InventoryManager {
private:
    static const int TABLE_SIZE = 101;
    ProductNode* buckets[TABLE_SIZE];
    int getBucket(int id) const {
        return id % TABLE_SIZE;
    }
public:
    InventoryManager() {
        for (int i = 0; i < TABLE_SIZE; ++i) {
            buckets[i] = nullptr;
        }
    }
    ~InventoryManager() {
        for (int i = 0; i < TABLE_SIZE; ++i) {
            while (buckets[i]) {
                ProductNode* next = buckets[i]->next;
                delete buckets[i];
                buckets[i] = next;
            }
        }
    }
    InventoryManager(const InventoryManager&) = delete;
    InventoryManager& operator=(const InventoryManager&) = delete;
    size_t productCount() const {
        size_t total = 0;
        for (int i = 0; i < TABLE_SIZE; ++i)
            for (ProductNode* current = buckets[i]; current; current = current->next)
                ++total;
        return total;
    }
    double totalStockValue() const {
        double total = 0;
        for (int i = 0; i < TABLE_SIZE; ++i)
            for (ProductNode* current = buckets[i]; current; current = current->next)
                total += current->product.getPrice() * current->product.getQuantity();
        return total;
    }
    template <typename Callback>
    void forEachProduct(Callback callback) const {
        for (int i = 0; i < TABLE_SIZE; ++i)
            for (ProductNode* current = buckets[i]; current; current = current->next)
                callback(current->product);
    }
    void saveState(ostream& output) const {
        output << productCount() << '\n' << setprecision(17);
        for (int i = 0; i < TABLE_SIZE; ++i) {
            for (ProductNode* current = buckets[i]; current; current = current->next) {
                const Product& product = current->product;
                output << product.getId() << ' ' << product.getPrice() << ' '
                       << product.getQuantity() << '\n';
                PersonD::writeString(output, product.getName());
            }
        }
    }
    bool loadState(istream& input) {
        size_t count;
        if (!PersonD::readCount(input, count)) return false;
        for (size_t i = 0; i < count; ++i) {
            int id, quantity;
            double price;
            string name;
            if (!(input >> id >> price >> quantity) || id <= 0 || quantity < 0 ||
                !(price >= 0) || !std::isfinite(price) ||
                !PersonD::readString(input, name) || name.empty() ||
                name.find('|') != string::npos ||
                !addProduct(Product(id, name, price, quantity))) return false;
        }
        return static_cast<bool>(input);
    }
    bool addProduct(const Product& product) {
        const int bucket = getBucket(product.getId());
        for (ProductNode* current = buckets[bucket]; current; current = current->next) {
            if (current->product.getId() == product.getId()) {
                return false;
            }
        }
        ProductNode* node = new ProductNode(product);
        node->next = buckets[bucket];
        buckets[bucket] = node;
        return true;
    }
    Product* searchProduct(int id) {
        if (id <= 0) {
            return nullptr;
        }
        for (ProductNode* current = buckets[getBucket(id)]; current; current = current->next) {
            if (current->product.getId() == id) {
                return &current->product;
            }
        }
        return nullptr;
    }
    const Product* searchProduct(int id) const {
        if (id <= 0) return nullptr;
        for (ProductNode* current = buckets[getBucket(id)]; current; current = current->next)
            if (current->product.getId() == id) return &current->product;
        return nullptr;
    }
    bool deleteProduct(int id) {
        if (id <= 0) {
            return false;
        }
        ProductNode*& bucketHead = buckets[getBucket(id)];
        ProductNode* current = bucketHead;
        ProductNode* previous = nullptr;
        while (current && current->product.getId() != id) {
            previous = current;
            current = current->next;
        }
        if (!current) {
            return false;
        }
        if (previous) {
            previous->next = current->next;
        } else {
            bucketHead = current->next;
        }
        delete current;
        return true;
    }
    void displayProducts() const {
        bool found = false;
        cout << "\n--- Product Inventory (Hash Table) ---\n";
        for (int i = 0; i < TABLE_SIZE; ++i) {
            for (ProductNode* current = buckets[i]; current; current = current->next) {
                cout << current->product << '\n';
                found = true;
            }
        }
        if (!found) {
            cout << "Inventory is empty.\n";
        }
    }
    void displayLowStock(int threshold) const {
        bool found = false;
        cout << "\n--- Low Stock Products (below " << threshold << ") ---\n";
        for (int i = 0; i < TABLE_SIZE; ++i) {
            for (ProductNode* current = buckets[i]; current; current = current->next) {
                if (current->product.getQuantity() < threshold) {
                    cout << current->product << '\n';
                    found = true;
                }
            }
        }
        if (!found) {
            cout << "No low-stock products.\n";
        }
    }
};
bool readInteger(const string& prompt, int& value) {
    cout << prompt;
    if (cin >> value) {
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return true;
    }
    if (cin.eof()) {
        return false;
    }
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "Invalid input. Please enter a whole number.\n";
    return false;
}
#ifndef INVENTORY_MODULE_NO_MAIN
int main() {
    InventoryManager inventory;
    int choice = -1;
    while (choice != 0) {
        cout << "\n--- Inventory Manager (Person A) ---\n"
             << "1. Add product\n"
             << "2. Delete product\n"
             << "3. Update product stock\n"
             << "4. Search product by ID\n"
             << "5. Display all products\n"
             << "6. Display low-stock products\n"
             << "0. Exit\n";
        if (!readInteger("Choice: ", choice)) {
            if (cin.eof()) break;
            continue;
        }
        if (choice == 1) {
            Product product;
            cout << "Enter product as ID|name|price|quantity (example: 101|Laptop Pro|899.99|12):\n";
            if (!(cin >> product)) {
                cout << "Invalid product. Use a positive ID, non-negative price and stock, and a non-empty name without '|'.\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            } else if (inventory.addProduct(product)) {
                cout << "Product added successfully.\n";
            } else {
                cout << "That product ID already exists.\n";
            }
        } else if (choice == 2) {
            int id;
            if (!readInteger("Product ID to delete: ", id)) {
                continue;
            }
            if (inventory.deleteProduct(id)) {
                cout << "Product deleted successfully.\n";
            } else {
                cout << "Product not found.\n";
            }
        } else if (choice == 3) {
            int id;
            if (!readInteger("Product ID to update: ", id)) {
                continue;
            }
            Product* product = inventory.searchProduct(id);
            if (!product) {
                cout << "Product not found.\n";
                continue;
            }
            int quantity;
            if (readInteger("Enter new stock quantity: ", quantity)) {
                if (quantity < 0) {
                    cout << "Stock cannot be negative.\n";
                } else {
                    product->setQuantity(quantity);
                    cout << "Stock updated successfully.\n";
                }
            }
        } else if (choice == 4) {
            int id;
            if (!readInteger("Product ID to search: ", id)) {
                continue;
            }
            Product* product = inventory.searchProduct(id);
            if (product) {
                cout << *product << '\n';
            } else {
                cout << "Product not found.\n";
            }
        } else if (choice == 5) {
            inventory.displayProducts();
        } else if (choice == 6) {
            int threshold;
            if (readInteger("Show products with stock below: ", threshold)) {
                if (threshold < 0) {
                    cout << "Threshold cannot be negative.\n";
                } else {
                    inventory.displayLowStock(threshold);
                }
            }
        } else if (choice == 0) {
            cout << "Inventory manager closed. Data is currently in memory only.\n";
        } else {
            cout << "Invalid choice. Select an option from the menu.\n";
        }
    }
    return 0;
}
#endif
