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
              << "  --name NAME     Shared memory base name (default: MyGlobalDataset)\n"
              << "  --linger MS     Milliseconds to keep alive after last write (default: 5000)\n"
              << "  --help          Show this help\n";
}

void OnDataReceived(const uint8_t* pData, size_t size) {
    if (size == 0) return;
    
    // Parse the data
    auto dataset = GetSharedDataset(pData);
    if (!dataset) return;
    
    std::cout << "\n>>> [Consumer C2P] Received FlatBuffer from C# (" << size << " bytes)\n";
    if (dataset->api_version()) {
        std::cout << ">>>  API Version: " << dataset->api_version()->c_str() << "\n";
    }
    if (dataset->rows()) {
        std::cout << ">>>  Rows Count: " << dataset->rows()->size() << "\n";
        for (auto row : *dataset->rows()) {
             if (row->row_id()) {
                 std::cout << ">>>    Row ID: " << row->row_id()->c_str() << "\n";
             }
        }
    }
    std::cout << "\n";
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
        std::wstring nameP2C = wname + L"_P2C";
        std::wstring nameC2P = wname + L"_C2P";

        std::cout << "[Producer] Starting SharedValueEngine Dual-Channel Host..." << std::endl;
        std::cout << "[Producer] Config: base_name=" << name 
                  << " interval=" << intervalMs << "ms"
                  << " rows=" << rowCount
                  << " count=" << (maxCount == 0 ? "infinite" : std::to_string(maxCount))
                  << std::endl;

        // True because C++ initializes the channels and acts as the primary driver setting the Ready Events.
        SharedValueEngine engineP2C(nameP2C, true); 
        SharedValueEngine engineC2P(nameC2P, true); 

        std::cout << "[Producer] Engine channels initialized. Kernel objects & Ready events created." << std::endl;
        
        // Start listening to C2P channel in a background thread
        engineC2P.StartListening(&OnDataReceived);

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

            // Write to shared memory (P2C)
            uint8_t* buf = builder.GetBufferPointer();
            size_t size = builder.GetSize();

            engineP2C.WriteData(buf, size);
            std::cout << "[Producer] Sent P2C Update #" << counter 
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
        
        engineC2P.StopListening();
        std::cout << "[Producer] Exiting." << std::endl;
    } 
    catch (const std::exception& e) {
        std::cerr << "[Producer Error] " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
