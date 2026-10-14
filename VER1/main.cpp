#include <iostream>
#include <fstream>
#include <limits>
#include <string>

using namespace std;

bool inputFailed() {
    if (cin.good()) {
        return false;
    }

    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "Invalid input. Please enter the requested value.\n";
    return true;
}

// Base Class
class Entity {
protected:
    int id;
    string name;

public:
    Entity() {
        id = 0;
        name = "";
    }

    Entity(int i, string n) {
        id = i;
        name = n;
    }

    int getId() { return id; }
    string getName() { return name; }

    virtual void display() {
        cout << "ID: " << id << " | Name: " << name << endl;
    }

    virtual ~Entity() {}
};

// Derived Class 1: Product
class Product : public Entity {
private:
    float price;
    int quantity;
    int supplierId;

public:
    Product() : Entity() {
        price = 0.0;
        quantity = 0;
        supplierId = 0;
    }

    Product(int i, string n, float p, int q, int sId) : Entity(i, n) {
        price = p;
        quantity = q;
        supplierId = sId;
    }

    float getPrice() { return price; }
    int getQuantity() { return quantity; }
    int getSupplierId() { return supplierId; }
    void setQuantity(int q) { quantity = q; }
    void setPrice(float p) { price = p; }

    void display() override {
        cout << "Product ID: " << id 
             << " | Name: " << name 
             << " | Price: $" << price 
             << " | Stock: " << quantity 
             << " | Supplier ID: " << supplierId << endl;
    }
};

// Derived Class 2: Supplier
class Supplier : public Entity {
private:
    string contact;

public:
    Supplier() : Entity() {
        contact = "";
    }

    Supplier(int i, string n, string c) : Entity(i, n) {
        contact = c;
    }

    string getContact() { return contact; }

    void display() override {
        cout << "Supplier ID: " << id 
             << " | Name: " << name 
             << " | Contact: " << contact << endl;
    }
};

// Linked List Node
struct Node {
    Product data;
    Node* next;
};

// Supply Chain System Class
class SupplyChain {
private:
    Node* head;
    Supplier suppliers[20];
    int supplierCount;

public:
    SupplyChain() {
        head = NULL;
        supplierCount = 0;
        loadData();
    }

    ~SupplyChain() {
        clearListMemory();
    }

    void clearListMemory() {
        Node* current = head;
        while (current != NULL) {
            Node* nextNode = current->next;
            delete current;
            current = nextNode;
        }
        head = NULL;
    }

    bool isIdExists(int id) {
        Node* temp = head;
        while (temp != NULL) {
            if (temp->data.getId() == id) {
                return true;
            }
            temp = temp->next;
        }
        return false;
    }

    Node* findProductByName(string name) {
        Node* temp = head;
        while (temp != NULL) {
            if (temp->data.getName() == name) {
                return temp;
            }
            temp = temp->next;
        }
        return NULL;
    }

    bool isSupplierExists(int sId) {
        for (int i = 0; i < supplierCount; i++) {
            if (suppliers[i].getId() == sId) {
                return true;
            }
        }
        return false;
    }

    void addProduct() {
        int id, qty, suppId;
        string name;
        float price;

        cout << "Enter Product Name: ";
        cin.ignore();
        getline(cin, name);

        Node* existingNode = findProductByName(name);
        if (existingNode != NULL) {
            cout << "\n[!] This product already exists in inventory!\n";
            cout << "Details: Product ID " << existingNode->data.getId() 
                 << " | Current Stock: " << existingNode->data.getQuantity() << endl;
            cout << "Operation cancelled. Use Option 5 to update stock instead.\n";
            return;
        }

        cout << "Enter Product ID: ";
        cin >> id;

        if (inputFailed()) {
            return;
        }

        if (isIdExists(id)) {
            cout << "\n[!] Product with ID " << id << " already exists!\n";
            cout << "Operation cancelled. Please use a unique Product ID.\n";
            return;
        }

        cout << "Enter Price: ";
        cin >> price;
        if (inputFailed()) {
            return;
        }

        cout << "Enter Initial Stock Quantity: ";
        cin >> qty;
        if (inputFailed()) {
            return;
        }

        cout << "Enter Supplier ID for this product: ";
        cin >> suppId;
        if (inputFailed()) {
            return;
        }

        if (!isSupplierExists(suppId)) {
            cout << "[Note] Supplier ID " << suppId << " is not yet registered. You can add this supplier using Option 8.\n";
        }

        Product newProd(id, name, price, qty, suppId);

        Node* newNode = new Node();
        newNode->data = newProd;
        newNode->next = NULL;

        if (head == NULL) {
            head = newNode;
        } else {
            Node* temp = head;
            while (temp->next != NULL) {
                temp = temp->next;
            }
            temp->next = newNode;
        }

        saveData();
        cout << "=> Product added successfully!\n";
    }

    void displayAll() {
        if (head == NULL) {
            cout << "No products in inventory (Linked List is empty).\n";
            return;
        }

        cout << "\n--- Current Inventory (Linked List) ---\n";
        Node* temp = head;
        while (temp != NULL) {
            Entity* e = &(temp->data);
            e->display();
            temp = temp->next;
        }
    }

    void searchProduct() {
        if (head == NULL) {
            cout << "Inventory is empty.\n";
            return;
        }

        int searchId;
        cout << "Enter Product ID to search: ";
        cin >> searchId;

        Node* temp = head;
        while (temp != NULL) {
            if (temp->data.getId() == searchId) {
                cout << "\n--- PRODUCT DETAILS ---\n";
                temp->data.display();

                int suppId = temp->data.getSupplierId();
                bool foundSupp = false;
                for (int i = 0; i < supplierCount; i++) {
                    if (suppliers[i].getId() == suppId) {
                        cout << "--- SUPPLIER DETAILS ---\n";
                        suppliers[i].display();
                        foundSupp = true;
                        break;
                    }
                }
                if (!foundSupp) {
                    cout << "Supplier details not found for Supplier ID: " << suppId << endl;
                }
                return;
            }
            temp = temp->next;
        }
        cout << "Product ID not found.\n";
    }

    void sortByPrice() {
        if (head == NULL || head->next == NULL) {
            cout << "Not enough products to sort.\n";
            return;
        }

        bool swapped;
        Node* ptr1;
        Node* lptr = NULL;

        do {
            swapped = false;
            ptr1 = head;

            while (ptr1->next != lptr) {
                if (ptr1->data.getPrice() > ptr1->next->data.getPrice()) {
                    Product temp = ptr1->data;
                    ptr1->data = ptr1->next->data;
                    ptr1->next->data = temp;
                    swapped = true;
                }
                ptr1 = ptr1->next;
            }
            lptr = ptr1;
        } while (swapped);

        saveData();
        cout << "Linked List sorted by price (Ascending).\n";
        displayAll();
    }

    void updateStock() {
        if (head == NULL) {
            cout << "Inventory is empty.\n";
            return;
        }

        int id, newQty;
        cout << "Enter Product ID to update stock: ";
        cin >> id;

        Node* temp = head;
        while (temp != NULL) {
            if (temp->data.getId() == id) {
                cout << "Current Stock: " << temp->data.getQuantity() << endl;
                cout << "Enter Quantity to ADD to existing stock: ";
                cin >> newQty;
                temp->data.setQuantity(temp->data.getQuantity() + newQty);
                saveData();
                cout << "Stock updated successfully! New Total Stock: " << temp->data.getQuantity() << endl;
                return;
            }
            temp = temp->next;
        }
        cout << "Product ID not found.\n";
    }

    void deleteProduct() {
        if (head == NULL) {
            cout << "Inventory is empty.\n";
            return;
        }

        int id;
        cout << "Enter Product ID to delete: ";
        cin >> id;

        Node* temp = head;
        Node* prev = NULL;

        if (temp != NULL && temp->data.getId() == id) {
            head = temp->next;
            delete temp;
            saveData();
            cout << "Product deleted from Linked List!\n";
            return;
        }

        while (temp != NULL && temp->data.getId() != id) {
            prev = temp;
            temp = temp->next;
        }

        if (temp == NULL) {
            cout << "Product ID not found.\n";
            return;
        }

        prev->next = temp->next;
        delete temp;

        saveData();
        cout << "Product deleted from Linked List!\n";
    }

    void checkLowStock() {
        if (head == NULL) {
            cout << "Inventory is empty.\n";
            return;
        }

        int threshold = 5;
        cout << "\n--- Low Stock Warning (Less than " << threshold << " items) ---\n";
        Node* temp = head;
        int found = 0;
        while (temp != NULL) {
            if (temp->data.getQuantity() < threshold) {
                temp->data.display();
                found = 1;
            }
            temp = temp->next;
        }

        if (found == 0) {
            cout << "All items have sufficient stock levels.\n";
        }
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

        if (isSupplierExists(id)) {
            cout << "\n[!] Supplier with ID " << id << " already exists!\n";
            return;
        }

        cout << "Enter Supplier Name: ";
        cin.ignore();
        getline(cin, name);
        cout << "Enter Contact Email/Phone: ";
        getline(cin, contact);

        suppliers[supplierCount] = Supplier(id, name, contact);
        supplierCount++;

        saveData();
        cout << "Supplier added successfully!\n";
    }

    void displaySuppliers() {
        if (supplierCount == 0) {
            cout << "No suppliers registered.\n";
            return;
        }

        cout << "\n--- Registered Suppliers (Array) ---\n";
        for (int i = 0; i < supplierCount; i++) {
            Entity* ptr = &suppliers[i];
            ptr->display();
        }
    }

    void clearAllData() {
        char confirm;
        cout << "\n[WARNING] This action will permanently delete ALL products, suppliers, and saved files!\n";
        cout << "Are you sure you want to proceed? (Y/N): ";
        cin >> confirm;

        if (confirm != 'Y' && confirm != 'y') {
            cout << "=> Operation cancelled. No data was deleted.\n";
            return;
        }

        clearListMemory();
        supplierCount = 0;

        ofstream invFile("inventory.txt", ios::trunc);
        if (invFile) invFile.close();

        ofstream suppFile("suppliers.txt", ios::trunc);
        if (suppFile) suppFile.close();

        cout << "\n=> [SUCCESS] All products, suppliers, and saved files have been completely cleared!\n";
    }

    // FILE PERSISTENCE: SAVE
    void saveData() {
        ofstream outFile("inventory.txt");
        if (outFile) {
            int count = 0;
            Node* temp = head;
            while (temp != NULL) {
                count++;
                temp = temp->next;
            }

            outFile << "TOTAL_RECORDS: " << count << endl;
            outFile << "========================================================\n";
            
            temp = head;
            while (temp != NULL) {
                outFile << "ID: " << temp->data.getId() << endl;
                outFile << "Name: " << temp->data.getName() << endl;
                outFile << "Price: " << temp->data.getPrice() << endl;
                outFile << "Quantity: " << temp->data.getQuantity() << endl;
                outFile << "SupplierID: " << temp->data.getSupplierId() << endl;
                outFile << "--------------------------------------------------------\n";
                temp = temp->next;
            }
            outFile.close();
        }

        ofstream suppFile("suppliers.txt");
        if (suppFile) {
            suppFile << "TOTAL_SUPPLIERS: " << supplierCount << endl;
            suppFile << "========================================================\n";
            
            for (int i = 0; i < supplierCount; i++) {
                suppFile << "ID: " << suppliers[i].getId() << endl;
                suppFile << "Name: " << suppliers[i].getName() << endl;
                suppFile << "Contact: " << suppliers[i].getContact() << endl;
                suppFile << "--------------------------------------------------------\n";
            }
            suppFile.close();
        }
    }

    // FILE PERSISTENCE: LOAD
    void loadData() {
        clearListMemory();

        ifstream inFile("inventory.txt");
        if (inFile) {
            string headerLabel, borderLine;
            int count = 0;

            if (inFile >> headerLabel >> count) {
                inFile.ignore();
                getline(inFile, borderLine);

                for (int i = 0; i < count; i++) {
                    int id, qty, suppId;
                    string name, prefix;
                    float price;

                    inFile >> prefix >> id;
                    inFile >> prefix; 
                    inFile.ignore(); 
                    getline(inFile, name);
                    inFile >> prefix >> price;
                    inFile >> prefix >> qty;
                    inFile >> prefix >> suppId;
                    inFile >> prefix; 

                    Product p(id, name, price, qty, suppId);

                    Node* newNode = new Node();
                    newNode->data = p;
                    newNode->next = NULL;

                    if (head == NULL) {
                        head = newNode;
                    } else {
                        Node* temp = head;
                        while (temp->next != NULL) {
                            temp = temp->next;
                        }
                        temp->next = newNode;
                    }
                }
            }
            inFile.close();
        }

        ifstream suppFile("suppliers.txt");
        if (suppFile) {
            string headerLabel, borderLine;
            if (suppFile >> headerLabel >> supplierCount) {
                suppFile.ignore();
                getline(suppFile, borderLine);

                for (int i = 0; i < supplierCount; i++) {
                    int id;
                    string name, contact, prefix;

                    suppFile >> prefix >> id;
                    suppFile >> prefix; 
                    suppFile.ignore(); 
                    getline(suppFile, name);
                    suppFile >> prefix; 
                    getline(suppFile, contact);
                    suppFile >> prefix;

                    suppliers[i] = Supplier(id, name, contact);
                }
            }
            suppFile.close();
        }
    }
};

int main() {
    SupplyChain system;
    int choice = 0;

    do {
        cout << "\n=======================================\n";
        cout << "  SUPPLY CHAIN MANAGEMENT SYSTEM   \n";
        cout << "=======================================\n";
        cout << "1. Add Product\n";
        cout << "2. View All Products\n";
        cout << "3. Search Product (with Supplier Info)\n";
        cout << "4. Sort Products by Price (Bubble Sort)\n";
        cout << "5. Update Product Stock\n";
        cout << "6. Delete Product\n";
        cout << "7. Check Low Stock Warnings\n";
        cout << "8. Add Supplier\n";
        cout << "9. View Suppliers\n";
        cout << "10. CLEAR ALL DATA (Reset System)\n";
        cout << "11. Exit\n";
        cout << "Enter choice: ";
        cin >> choice;

        if (inputFailed()) {
            choice = 0;
            continue;
        }

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
                system.sortByPrice();
                break;
            case 5:
                system.updateStock();
                break;
            case 6:
                system.deleteProduct();
                break;
            case 7:
                system.checkLowStock();
                break;
            case 8:
                system.addSupplier();
                break;
            case 9:
                system.displaySuppliers();
                break;
            case 10:
                system.clearAllData();
                break;
            case 11:
                cout << "Exiting program. Data saved automatically.\n";
                break;
            default:
                cout << "Invalid choice! Please try again.\n";
        }
    } while (choice != 11);

    return 0;
}