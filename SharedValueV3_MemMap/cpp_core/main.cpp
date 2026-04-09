#include <iostream>
#include <thread>
#include <string>
#include <cstdlib>
#include "SharedValueEngine.hpp"
#include "dataset_generated.h" // FlatBuffers generated file

using namespace SharedMemMap;

void PrintUsage() {
    std::cout << "Usage: MemMapProducer.exe [options]\n"
              << "Options:\n"
              << "  --count N       Send N updates then exit (default: infinite)\n"
              << "  --interval MS   Milliseconds between updates (default: 2000)\n"
              << "  --rows N        Number of DataRows per update (default: 1)\n"
              << "  --name NAME     Shared memory name (default: MyGlobalDataset)\n"
              << "  --linger MS     Milliseconds to keep alive after last write (default: 5000)\n"
              << "  --help          Show this help\n";
}

int main(int argc, char* argv[]) {
    int maxCount = 0;       // 0 = infinite
    int intervalMs = 2000;
    int rowCount = 1;
    int lingerMs = 5000;    // keep alive after last write
    std::string name = "MyGlobalDataset";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--count" && i + 1 < argc) {
            maxCount = std::atoi(argv[++i]);
        } else if (arg == "--interval" && i + 1 < argc) {
            intervalMs = std::atoi(argv[++i]);
        } else if (arg == "--rows" && i + 1 < argc) {
            rowCount = std::atoi(argv[++i]);
        } else if (arg == "--name" && i + 1 < argc) {
            name = argv[++i];
        } else if (arg == "--linger" && i + 1 < argc) {
            lingerMs = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            PrintUsage();
            return 0;
        }
    }

    try {
        // Convert name to wide string
        std::wstring wname(name.begin(), name.end());

        std::cout << "[Producer] Starting SharedValueEngine Producer..." << std::endl;
        std::cout << "[Producer] Config: name=" << name 
                  << " interval=" << intervalMs << "ms"
                  << " rows=" << rowCount
                  << " count=" << (maxCount == 0 ? "infinite" : std::to_string(maxCount))
                  << std::endl;

        SharedValueEngine engine(wname, 10 * 1024 * 1024); // 10MB
        std::cout << "[Producer] Engine initialized. Shared memory and kernel objects created." << std::endl;
        
        flatbuffers::FlatBufferBuilder builder(4096);
        int counter = 1;

        while (maxCount == 0 || counter <= maxCount) {
            builder.Clear();

            // Build rows
            std::vector<flatbuffers::Offset<DataRow>> rows_vector;

            for (int r = 0; r < rowCount; r++) {
                // Create NestedDetails for each row
                std::vector<flatbuffers::Offset<NestedDetails>> details_vector;
                double temp = 20.0 + counter + (r * 0.5);
                auto d1 = CreateNestedDetails(builder, temp, 60.0 + r, 200);
                auto d2 = CreateNestedDetails(builder, temp - 2.0, 55.5, 201);
                details_vector.push_back(d1);
                details_vector.push_back(d2);
                auto details_offset = builder.CreateVector(details_vector);

                // Create DataRow
                auto row_id = builder.CreateString("Sensor_" + std::to_string(counter) + "_R" + std::to_string(r));
                bool is_active = (counter % 3 != 0); // Toggle every 3rd update
                auto row = CreateDataRow(builder, row_id, is_active, details_offset);
                rows_vector.push_back(row);
            }

            auto rows_offset = builder.CreateVector(rows_vector);

            // Create Root SharedDataset
            auto api_version = builder.CreateString("1.0.0");
            auto utc = builder.CreateString("2026-04-10T00:00:00Z");
            
            auto dataset = CreateSharedDataset(builder, api_version, utc, rows_offset);
            builder.Finish(dataset);

            // Write to shared memory
            uint8_t* buf = builder.GetBufferPointer();
            size_t size = builder.GetSize();

            engine.WriteData(buf, size);
            std::cout << "[Producer] Update #" << counter 
                      << " | Rows: " << rowCount 
                      << " | FlatBuffer: " << size << " bytes" << std::endl;

            if (maxCount > 0 && counter >= maxCount) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            counter++;
        }

        std::cout << "[Producer] Completed " << counter << " updates. Lingering " << lingerMs << "ms before exit." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(lingerMs));
        std::cout << "[Producer] Exiting." << std::endl;
    } 
    catch (const std::exception& e) {
        std::cerr << "[Producer Error] " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
