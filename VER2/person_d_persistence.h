#ifndef PERSON_D_PERSISTENCE_H
#define PERSON_D_PERSISTENCE_H

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <string>

#include "person_d_support.h"

class SystemPersistence {
    const std::string fileName;
    const std::string temporaryName;
    const std::string backupName;

    bool exists(bool& found, std::string& error) const {
        errno = 0;
        std::FILE* file = std::fopen(fileName.c_str(), "rb");
        if (file) {
            std::fclose(file);
            found = true;
            return true;
        }
        if (errno == ENOENT) {
            found = false;
            return true;
        }
        error = "Cannot access " + fileName + ".";
        return false;
    }

public:
    SystemPersistence()
        : fileName("warehouse_system.dat"),
          temporaryName("warehouse_system.dat.tmp"),
          backupName("warehouse_system.dat.bak") {}

    bool load(InventoryManager& inventory, WarehouseManager& warehouses,
              SupplyChainManager& supplyChain, std::string& error) const {
        bool found = false;
        if (!exists(found, error)) return false;
        if (!found) return true;

        std::ifstream input(fileName.c_str(), std::ios::binary);
        if (!input) {
            error = "Cannot open " + fileName + " for reading.";
            return false;
        }
        std::string signature;
        if (!(input >> signature) || signature != "GSC_STATE_V1" ||
            !inventory.loadState(input) ||
            !warehouses.loadState(input, inventory) ||
            !supplyChain.loadState(input)) {
            error = "Saved data is invalid or incompatible; no saved state was loaded completely.";
            return false;
        }
        std::string ending;
        if (!(input >> ending) || ending != "END") {
            error = "Saved data is incomplete or has an invalid ending marker.";
            return false;
        }
        input >> std::ws;
        if (!input.eof()) {
            error = "Saved data contains unexpected content after the ending marker.";
            return false;
        }
        return true;
    }

    bool save(const InventoryManager& inventory, const WarehouseManager& warehouses,
              const SupplyChainManager& supplyChain, std::string& error) const {
        {
            std::ofstream output(temporaryName.c_str(),
                                 std::ios::out | std::ios::trunc | std::ios::binary);
            if (!output) {
                error = "Cannot create temporary save file " + temporaryName + ".";
                return false;
            }
            output << "GSC_STATE_V1\n";
            inventory.saveState(output);
            warehouses.saveState(output);
            supplyChain.saveState(output);
            output << "END\n";
            output.flush();
            if (!output) {
                error = "Failed while writing saved data to " + temporaryName + ".";
                output.close();
                std::remove(temporaryName.c_str());
                return false;
            }
        }

        bool hasOriginal = false;
        if (!exists(hasOriginal, error)) {
            std::remove(temporaryName.c_str());
            return false;
        }
        if (hasOriginal) {
            if (std::remove(backupName.c_str()) != 0 && errno != ENOENT) {
                error = "Cannot remove the previous backup file " + backupName + ".";
                std::remove(temporaryName.c_str());
                return false;
            }
            if (std::rename(fileName.c_str(), backupName.c_str()) != 0) {
                error = "Cannot preserve the previous save file before replacing it.";
                std::remove(temporaryName.c_str());
                return false;
            }
        }
        if (std::rename(temporaryName.c_str(), fileName.c_str()) != 0) {
            error = "Cannot replace " + fileName + " with the new saved state.";
            if (hasOriginal && std::rename(backupName.c_str(), fileName.c_str()) != 0)
                error += " The previous save remains in " + backupName + ".";
            std::remove(temporaryName.c_str());
            return false;
        }
        if (hasOriginal && std::remove(backupName.c_str()) != 0) {
            error = "Data was saved, but the old backup file could not be removed.";
            return false;
        }
        return true;
    }
};

#endif
