#include <iostream>
#include <fstream>
#include <string>
using namespace std;
class Entity {
protected:
    int id;
    string name;
public:
    Entity() : id(0), name("") {}
    Entity(int i, const string& n) : id(i), name(n) {}
    int getId() const { return id; }
    const string& getName() const { return name; }
    virtual void display() const {
        cout << "ID: " << id << " | Name: " << name << endl;
    }
    virtual ~Entity() = default;
};
class Product : public Entity {
private:
    float price;
    int quantity;
    int supplierId;
public:
    Product() : Entity(), price(0), quantity(0), supplierId(0) {}
    Product(int i, const string& n, float p, int q, int sId)
        : Entity(i, n), price(p), quantity(q), supplierId(sId) {}

    float getPrice() const { return price; }
    int getQuantity() const { return quantity; }
    int getSupplierId() const { return supplierId; }
    void setQuantity(int q) { quantity = q; }

    void display() const override {
        cout << "Product ID: " << id
             << " | Name: " << name
             << " | Price: $" << price
             << " | Stock: " << quantity
             << " | Supplier ID: " << supplierId << endl;
    }
};
class Supplier : public Entity {
private:
    string contact;
public:
    Supplier() : Entity(), contact("") {}
    Supplier(int i, const string& n, const string& c)
        : Entity(i, n), contact(c) {}
    const string& getContact() const { return contact; }
    void display() const override {
        cout << "Supplier ID: " << id
             << " | Name: " << name
             << " | Contact: " << contact << endl;
    }
};
struct Node {
    Product data;
    Node* next;
};
class SupplyChain {
private:
    Node* head;
    Supplier suppliers[20];
    int supplierCount;
    Node* findProductById(int id) const {
        for (Node* node = head; node; node = node->next)
            if (node->data.getId() == id) return node;
        return nullptr;
    }
    Node* findProductByName(const string& name) const {
        for (Node* node = head; node; node = node->next)
            if (node->data.getName() == name) return node;
        return nullptr;
    }
    Supplier* findSupplierById(int id) {
        for (int i = 0; i < supplierCount; ++i)
            if (suppliers[i].getId() == id) return &suppliers[i];
        return nullptr;
    }
    void appendProduct(const Product& product) {
        Node* node = new Node{product, nullptr};
        if (!head) {
            head = node;
            return;
        }
        Node* last = head;
        while (last->next) last = last->next;
        last->next = node;
    }
public:
    SupplyChain() : head(nullptr), supplierCount(0) { loadData(); }
    ~SupplyChain() { clearListMemory(); }
    void clearListMemory() {
        while (head) {
            Node* next = head->next;
            delete head;
            head = next;
        }
    }
    void addProduct() {
        int id, qty, suppId;
        string name;
        float price;
        cout << "Enter Product Name: ";
        cin.ignore();
        getline(cin, name);
        Node* existingNode = findProductByName(name);
        if (existingNode) {
            cout << "\n[!] This product already exists in inventory!\n";
            cout << "Details: Product ID " << existingNode->data.getId() 
                 << " | Current Stock: " << existingNode->data.getQuantity() << endl;
            cout << "Operation cancelled. Use Option 5 to update stock instead.\n";
            return;
        }
        cout << "Enter Product ID: ";
        cin >> id;
        if (findProductById(id)) {
            cout << "\n[!] Product with ID " << id << " already exists!\n";
            cout << "Operation cancelled. Please use a unique Product ID.\n";
            return;
        }
        cout << "Enter Price: ";
        cin >> price;
        cout << "Enter Initial Stock Quantity: ";
        cin >> qty;
        cout << "Enter Supplier ID for this product: ";
        cin >> suppId;
        if (!findSupplierById(suppId)) {
            cout << "[Note] Supplier ID " << suppId
                 << " is not yet registered. Add it using Option 6.\n";
        }
        appendProduct(Product(id, name, price, qty, suppId));
        saveData();
        cout << "=> Product added successfully!\n";
    }
    void displayAll() {
        if (!head) {
            cout << "No products in inventory (Linked List is empty).\n";
            return;
        }
        cout << "\n--- Current Inventory (Linked List) ---\n";
        for (Node* node = head; node; node = node->next)
            node->data.display();
    }
    void searchProduct() {
        if (!head) {
            cout << "Inventory is empty.\n";
            return;
        }
        int id;
        cout << "Enter Product ID to search: ";
        cin >> id;
        Node* product = findProductById(id);
        if (!product) {
            cout << "Product ID not found.\n";
            return;
        }
        cout << "\n--- PRODUCT DETAILS ---\n";
        product->data.display();
        Supplier* supplier = findSupplierById(product->data.getSupplierId());
        if (supplier) {
            cout << "--- SUPPLIER DETAILS ---\n";
            supplier->display();
        } else {
            cout << "Supplier details not found for Supplier ID: "
                 << product->data.getSupplierId() << endl;
        }
    }
    void updateStock() {
        if (!head) {
            cout << "Inventory is empty.\n";
            return;
        }
        int id, quantity;
        cout << "Enter Product ID to update stock: ";
        cin >> id;
        Node* product = findProductById(id);
        if (!product) {
            cout << "Product ID not found.\n";
            return;
        }
        cout << "Current Stock: " << product->data.getQuantity() << endl;
        cout << "Enter Quantity to ADD to existing stock: ";
        cin >> quantity;
        product->data.setQuantity(product->data.getQuantity() + quantity);
        saveData();
        cout << "Stock updated successfully! New Total Stock: "
             << product->data.getQuantity() << endl;
    }
    void deleteProduct() {
        if (!head) {
            cout << "Inventory is empty.\n";
            return;
        }
        int id;
        cout << "Enter Product ID to delete: ";
        cin >> id;
        Node* current = head;
        Node* previous = nullptr;
        while (current && current->data.getId() != id) {
            previous = current;
            current = current->next;
        }
        if (!current) {
            cout << "Product ID not found.\n";
            return;
        }
        if (previous) previous->next = current->next;
        else head = current->next;
        delete current;
        saveData();
        cout << "Product deleted successfully!\n";
    }
    void addSupplier() {
        if (supplierCount >= 20) {
            cout << "Supplier limit reached!\n";
            return;
        }
        int id;
        string name, contact;
        cout << "Enter Supplier ID: ";
        cin >> id;
        if (findSupplierById(id)) {
            cout << "\n[!] Supplier with ID " << id << " already exists!\n";
            return;
        }
        cout << "Enter Supplier Name: ";
        cin.ignore();
        getline(cin, name);
        cout << "Enter Contact Email/Phone: ";
        getline(cin, contact);
        suppliers[supplierCount++] = Supplier(id, name, contact);
        saveData();
        cout << "Supplier added successfully!\n";
    }
    void displaySuppliers() {
        if (supplierCount == 0) {
            cout << "No suppliers registered.\n";
            return;
        }
        cout << "\n--- Registered Suppliers ---\n";
        for (int i = 0; i < supplierCount; ++i) suppliers[i].display();
    }
    void saveData() {
        int count = 0;
        for (Node* node = head; node; node = node->next) ++count;
        ofstream outFile("inventory.txt");
        if (!outFile) {
            cerr << "Error: Could not save inventory.txt.\n";
            return;
        }
        outFile << "TOTAL_RECORDS: " << count << '\n'
                << "========================================================\n";
        for (Node* node = head; node; node = node->next) {
            outFile << "ID: " << node->data.getId() << '\n'
                    << "Name: " << node->data.getName() << '\n'
                    << "Price: " << node->data.getPrice() << '\n'
                    << "Quantity: " << node->data.getQuantity() << '\n'
                    << "SupplierID: " << node->data.getSupplierId() << '\n'
                    << "--------------------------------------------------------\n";
        }
        ofstream suppFile("suppliers.txt");
        if (!suppFile) {
            cerr << "Error: Could not save suppliers.txt.\n";
            return;
        }
        suppFile << "TOTAL_SUPPLIERS: " << supplierCount << '\n'
                 << "========================================================\n";
        for (int i = 0; i < supplierCount; ++i) {
            suppFile << "ID: " << suppliers[i].getId() << '\n'
                     << "Name: " << suppliers[i].getName() << '\n'
                     << "Contact: " << suppliers[i].getContact() << '\n'
                     << "--------------------------------------------------------\n";
        }
    }
    void loadData() {
        clearListMemory();
        supplierCount = 0;
        ifstream inFile("inventory.txt");
        if (inFile) {
            string label, border, prefix, name;
            int count;
            if (inFile >> label >> count) {
                inFile.ignore();
                getline(inFile, border);
                for (int i = 0; i < count; ++i) {
                    int id, quantity, supplierId;
                    float price;
                    if (!(inFile >> prefix >> id >> prefix)) break;
                    inFile.ignore();
                    if (!getline(inFile, name) ||
                        !(inFile >> prefix >> price >> prefix >> quantity
                                 >> prefix >> supplierId >> prefix))
                        break;
                    appendProduct(Product(id, name, price, quantity, supplierId));
                }
            }
        }
        ifstream suppFile("suppliers.txt");
        if (suppFile) {
            string label, border, prefix, name, contact;
            int count;
            if (suppFile >> label >> count) {
                suppFile.ignore();
                getline(suppFile, border);
                supplierCount = 0;
                for (int i = 0; i < count && i < 20; ++i) {
                    int id;
                    if (!(suppFile >> prefix >> id >> prefix)) break;
                    suppFile.ignore();
                    if (!getline(suppFile, name) || !(suppFile >> prefix)) break;
                    getline(suppFile >> ws, contact);
                    if (!(suppFile >> prefix)) break;
                    suppliers[supplierCount++] = Supplier(id, name, contact);
                }
            }
        }
    }
};
int main() {
    SupplyChain system;
    int choice;
    do {
        cout << "\n=======================================\n";
        cout << "  SUPPLY CHAIN MANAGEMENT SYSTEM   \n";
        cout << "=======================================\n";
        cout << "1. Add Product\n";
        cout << "2. View All Products\n";
        cout << "3. Search Product (with Supplier Info)\n";
        cout << "4. Update Product Stock\n";
        cout << "5. Delete Product\n";
        cout << "6. Add Supplier\n";
        cout << "7. View Suppliers\n";
        cout << "8. Exit\n";
        cout << "Enter choice: ";
        cin >> choice;
        switch (choice) {
            case 1:
                system.addProduct();
                break;
            case 2:
                system.displayAll();
                break;
            case 3:
                system.searchProduct();
                break;
            case 4:
                system.updateStock();
                break;
            case 5:
                system.deleteProduct();
                break;
            case 6:
                system.addSupplier();
                break;
            case 7:
                system.displaySuppliers();
                break;
            case 8:
                cout << "Exiting program. Data saved automatically.\n";
                break;
            default:
                cout << "Invalid choice! Please try again.\n";
        }
    } while (choice != 8);
    return 0;
}