#include <iostream>
#include <queue>
#include <vector>
#include <functional>
#include <random>
#include <ctime>
#include <map>
#include <fstream>
#include <string>

// Event structure to hold event time, type, and action
struct Event {
    double time;
    std::string type;
    std::function<void()> action;

    bool operator>(const Event& other) const {
        return time > other.time;
    }
};

struct Product {
    std::string type;
    int intermediateStage;
};

class ManufacturingSystem {
private:
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> eventQueue;
    std::map<std::string, int> resources;
    std::map<std::string, int> availableResources;
    std::map<std::string, double> resourceUsageTime;
    std::map<std::string, double> resourceWaitingTime;
    int rawMaterialCount = 0;
    int finishedProducts = 0;
    double currentTime = 0.0;
    std::default_random_engine generator;
    std::exponential_distribution<double> rawMaterialArrivalDist;
    std::uniform_real_distribution<double> breakdownDist;
    std::map<std::string, std::vector<double>> processingTimes;
    std::map<std::string, int> productTypes;
    std::map<std::string, int> finishedProductsPerType;
    std::queue<Product> productQueue;

    // New fields for shifts and setup times
    std::map<std::string, double> machineSetupTimes;
    int shiftLength = 8; // 8-hour shifts

public:
    ManufacturingSystem()
        : rawMaterialArrivalDist(1.0), breakdownDist(0.0, 1.0) {
        // Initialize resources and machines
        resources["machines"] = 10;
        resources["operators"] = 5;
        availableResources = resources;
        resourceUsageTime["machines"] = 0.0;
        resourceUsageTime["operators"] = 0.0;
        resourceWaitingTime["machines"] = 0.0;
        resourceWaitingTime["operators"] = 0.0;

        generator.seed(static_cast<unsigned>(std::time(nullptr)));

        // Initialize processing times for different product types
        processingTimes["ProductA"] = { 2.0, 1.5, 1.0, 1.0 };
        processingTimes["ProductB"] = { 3.0, 2.0, 1.5, 1.5 };
        productTypes["ProductA"] = 0;
        productTypes["ProductB"] = 0;
        finishedProductsPerType["ProductA"] = 0;
        finishedProductsPerType["ProductB"] = 0;

        // Initialize machine setup times
        machineSetupTimes["ProductA"] = 0.5;
        machineSetupTimes["ProductB"] = 0.75;
    }

    void scheduleEvent(double time, const std::string& type, std::function<void()> action) {
        eventQueue.push({ time, type, action });
    }

    void runSimulation(double runTime = 1000.0) {
        // Schedule the first raw material arrival
        scheduleEvent(rawMaterialArrivalDist(generator), "raw_material_arrival", [this] { handleRawMaterialArrival("ProductA"); });

        // Schedule shift changes
        scheduleEvent(shiftLength, "shift_change", [this] { handleShiftChange(); });

        while (!eventQueue.empty() && currentTime < runTime) {
            Event currentEvent = eventQueue.top();
            eventQueue.pop();
            currentTime = currentEvent.time;
            currentEvent.action();
        }

        // Log data after simulation
        logData("simulation_log.txt");
    }

    void handleRawMaterialArrival(const std::string& productType) {
        rawMaterialCount++;
        Product newProduct = { productType, 0 };
        productQueue.push(newProduct);
        std::cout << "Raw material for " << productType << " arrived at time " << currentTime << std::endl;

        // Schedule the next raw material arrival
        scheduleEvent(currentTime + rawMaterialArrivalDist(generator), "raw_material_arrival", [this, productType] { handleRawMaterialArrival(productType); });

        handleNextStage(newProduct);
    }

    void handleNextStage(Product product) {
        if (product.intermediateStage < processingTimes[product.type].size()) {
            double processTime = processingTimes[product.type][product.intermediateStage];
            std::string stage = getStageName(product.intermediateStage);
            if (availableResources[stage] > 0) {
                // Schedule machine setup if needed
                double setupTime = product.intermediateStage == 0 ? machineSetupTimes[product.type] : 0.0;
                availableResources[stage]--;
                scheduleEvent(currentTime + setupTime, "setup", [this, product, processTime, stage] {
                    resourceUsageTime[stage] += processTime;
                    scheduleEvent(currentTime + processTime, stage, [this, product] { completeStage(product); });
                    });
                resourceUsageTime[stage] += setupTime;
            }
            else {
                resourceWaitingTime[stage] += processTime;
            }
        }
    }

    void completeStage(Product product) {
        std::string stage = getStageName(product.intermediateStage);
        std::cout << stage << " for " << product.type << " completed at time " << currentTime << std::endl;
        availableResources[stage]++;
        product.intermediateStage++;
        if (product.intermediateStage >= processingTimes[product.type].size()) {
            finishedProducts++;
            finishedProductsPerType[product.type]++;
        }
        else {
            handleNextStage(product);
        }
    }

    std::string getStageName(int stageIndex) {
        switch (stageIndex) {
        case 0: return "machining";
        case 1: return "assembly";
        case 2: return "quality_control";
        case 3: return "packaging";
        default: return "unknown";
        }
    }

    void handleBreakdown(const std::string& resource) {
        std::cout << "Breakdown occurred on " << resource << " at time " << currentTime << std::endl;
        scheduleEvent(currentTime + 5.0, "maintenance", [this, resource] { handleMaintenance(resource); });
    }

    void handleMaintenance(const std::string& resource) {
        std::cout << "Maintenance completed on " << resource << " at time " << currentTime << std::endl;
        availableResources[resource]++;
    }

    void handleShiftChange() {
        std::cout << "Shift change at time " << currentTime << std::endl;

        // Reset available resources for the new shift
        availableResources = resources;

        // Schedule the next shift change
        scheduleEvent(currentTime + shiftLength, "shift_change", [this] { handleShiftChange(); });
    }

    void logData(const std::string& filename) {
        std::ofstream logFile(filename);
        if (logFile.is_open()) {
            logFile << "Resource Usage Times:\n";
            for (const auto& entry : resourceUsageTime) {
                logFile << entry.first << ": " << entry.second << " time units\n";
            }
            logFile << "Resource Waiting Times:\n";
            for (const auto& entry : resourceWaitingTime) {
                logFile << entry.first << ": " << entry.second << " time units\n";
            }
            logFile << "Total finished products: " << finishedProducts << "\n";
            for (const auto& entry : finishedProductsPerType) {
                logFile << entry.first << ": " << entry.second << " units\n";
            }
            logFile.close();
        }
    }

    // Getter for available resources
    std::map<std::string, int> getAvailableResources() const {
        return availableResources;
    }

    // Setter for available resources
    void setAvailableResources(const std::map<std::string, int>& newAvailableResources) {
        availableResources = newAvailableResources;
    }

    // Getter for resources
    std::map<std::string, int> getResources() const {
        return resources;
    }

    // Setter for resources
    void setResources(const std::map<std::string, int>& newResources) {
        resources = newResources;
        availableResources = newResources; // Reset available resources as well
    }
};

void runScenario(const std::string& productType, int machineCount, int operatorCount, double runTime) {
    ManufacturingSystem system;
    std::map<std::string, int> resources = {
        {"machines", machineCount},
        {"operators", operatorCount}
    };
    system.setResources(resources);
    system.runSimulation(runTime);
    system.logData("scenario_" + productType + "_machines_" + std::to_string(machineCount) + "_operators_" + std::to_string(operatorCount) + ".txt");
}

int main() {
    // Run different scenarios
    runScenario("ProductA", 10, 5, 1000.0);
    runScenario("ProductB", 8, 6, 1000.0);
    runScenario("ProductA", 12, 7, 1000.0); // New Scenario
    return 0;
}
