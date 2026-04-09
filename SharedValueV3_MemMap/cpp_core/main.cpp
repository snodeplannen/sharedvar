#include <iostream>
#include <thread>
#include "SharedValueEngine.hpp"
#include "dataset_generated.h" // FlatBuffers generated file

using namespace SharedMemMap;

int main() {
    try {
        std::cout << "[Producer] Starting SharedValueEngine Producer..." << std::endl;
        SharedValueEngine engine(L"MyGlobalDataset", 10 * 1024 * 1024); // 10MB
        
        flatbuffers::FlatBufferBuilder builder(1024);
        int counter = 1;

        while (true) {
            builder.Clear();

            // Create some NestedDetails
            auto d1 = CreateNestedDetails(builder, 24.5 + counter, 60.0, 200);
            auto d2 = CreateNestedDetails(builder, 22.1, 55.5, 200);
            std::vector<flatbuffers::Offset<NestedDetails>> details_vector;
            details_vector.push_back(d1);
            details_vector.push_back(d2);
            auto details_offset = builder.CreateVector(details_vector);

            // Create a DataRow
            auto row_id = builder.CreateString("Sensor_" + std::to_string(counter));
            auto row = CreateDataRow(builder, row_id, true, details_offset);

            // Create rows vector
            std::vector<flatbuffers::Offset<DataRow>> rows_vector;
            rows_vector.push_back(row);
            auto rows_offset = builder.CreateVector(rows_vector);

            // Create Root SharedDataset
            auto api_version = builder.CreateString("1.0.0");
            auto utc = builder.CreateString("2026-04-09T20:00:00Z");
            
            auto dataset = CreateSharedDataset(builder, api_version, utc, rows_offset);
            
            builder.Finish(dataset); // Set root table

            // Write to memory mapped file
            uint8_t* buf = builder.GetBufferPointer();
            size_t size = builder.GetSize();

            engine.WriteData(buf, size);
            std::cout << "[Producer] Wrote SharedDataset with FlatBuffer size: " << size << " bytes." << std::endl;

            // Wait 2 seconds before next update
            std::this_thread::sleep_for(std::chrono::seconds(2));
            counter++;
        }
    } 
    catch (const std::exception& e) {
        std::cerr << "[Producer Error] " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
