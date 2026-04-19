#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "SharedValueV5Engine.hpp"

using namespace SharedValueV5;

int main(int argc, char* argv[]) {
    std::wstring channelName = L"V5Demo";
    int maxUpdates = 15; // default 15 iterations
    int delayMs = 2000; // Initial delay for consumers to connect

    // Parse args
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--name" && i + 1 < argc) {
            std::string name = argv[++i];
            channelName = std::wstring(name.begin(), name.end());
        } else if (arg == "--count" && i + 1 < argc) {
            maxUpdates = std::stoi(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            delayMs = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: cpp_producer [options]\n"
                      << "  --name NAME    Channel name (default: V5Demo)\n"
                      << "  --count N      Exit after N updates (default: 15)\n"
                      << "  --help         Show this help\n";
            return 0;
        }
    }

    std::wcout << L"[C++ Producer] Starting SharedValueV5 Producer...\n";
    std::wcout << L"[C++ Producer] Channel: " << channelName << L"\n";

    // 1. Define dataset with dynamic schema
    SharedDataSet ds("DemoDataSet");

    auto& sensoren = ds.AddTable("Sensoren");
    sensoren.AddColumn("SensorId",    ColumnType::String);
    sensoren.AddColumn("Temperature", ColumnType::Double);
    sensoren.AddColumn("Humidity",    ColumnType::Double);
    sensoren.AddColumn("IsActive",    ColumnType::Bool);
    sensoren.AddColumn("StatusCode",  ColumnType::Int32);

    auto& alarmen = ds.AddTable("Alarmen");
    alarmen.AddColumn("AlarmId",   ColumnType::Int32);
    alarmen.AddColumn("Bericht",   ColumnType::String);
    alarmen.AddColumn("Severity",  ColumnType::Int32);

    // Lock schemas
    sensoren.LockSchema();
    alarmen.LockSchema();

    std::cout << "[C++ Producer] Schema:\n"
              << "  Sensoren: " << sensoren.ColumnCount() << " columns (locked)\n"
              << "  Alarmen:  " << alarmen.ColumnCount() << " columns (locked)\n";

    // 2. Start engine as host
    SharedValueV5Engine engine(channelName, true);

    // 3. Listen for reverse (C2P) data
    engine.StartListeningReverse([](const SharedDataSet& incoming) {
        std::cout << "\n[C++ Producer] Received reverse data! Tables: " << incoming.TableCount() << "\n";
        for (int t = 0; t < incoming.TableCount(); t++) {
            const auto& table = incoming.Table(t);
            std::cout << "  Table: " << table.Name() << " (" << table.RowCount() << " rows)\n";
            for (int r = 0; r < table.RowCount(); r++) {
                std::cout << "    ";
                for (int c = 0; c < table.ColumnCount(); c++) {
                    const auto& col = table.Column(c);
                    const auto& val = table.Row(r)[c];
                    std::cout << col.name << "=";
                    if (std::holds_alternative<std::string>(val)) std::cout << std::get<std::string>(val);
                    else if (std::holds_alternative<int32_t>(val)) std::cout << std::get<int32_t>(val);
                    else if (std::holds_alternative<int64_t>(val)) std::cout << std::get<int64_t>(val);
                    else if (std::holds_alternative<double>(val)) std::cout << std::get<double>(val);
                    else if (std::holds_alternative<bool>(val)) std::cout << (std::get<bool>(val) ? "true" : "false");
                    else std::cout << "(null)";
                    std::cout << " ";
                }
                std::cout << "\n";
            }
        }
    });

    // 4. Publish loop
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> tempDist(18.0, 33.0);
    std::uniform_real_distribution<double> humDist(40.0, 80.0);
    std::uniform_real_distribution<double> chance(0.0, 1.0);

    int counter = 0;
    std::cout << "[C++ Producer] Waiting " << delayMs << "ms for consumers to connect...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    std::cout << "[C++ Producer] Publishing data. Press Ctrl+C to stop.\n\n";

    while (maxUpdates == 0 || counter < maxUpdates) {
        counter++;
        sensoren.ClearRows();
        alarmen.ClearRows();

        // Generate sensor data
        for (int s = 0; s < 3; s++) {
            auto row = sensoren.NewRow();
            std::ostringstream id;
            id << "Sensor_" << std::setfill('0') << std::setw(2) << (s + 1);
            row.Set(sensoren.ColumnOrdinal("SensorId"), id.str());
            row.Set(sensoren.ColumnOrdinal("Temperature"), tempDist(rng));
            row.Set(sensoren.ColumnOrdinal("Humidity"), humDist(rng));
            row.Set(sensoren.ColumnOrdinal("IsActive"), chance(rng) > 0.2);
            row.Set(sensoren.ColumnOrdinal("StatusCode"), chance(rng) > 0.9 ? 503 : 200);
            sensoren.AddRow(std::move(row));
        }

        // Generate alarm every 3rd iteration
        if (counter % 3 == 0) {
            auto alarm = alarmen.NewRow();
            alarm.Set(alarmen.ColumnOrdinal("AlarmId"), 1000 + counter);
            alarm.Set(alarmen.ColumnOrdinal("Bericht"), std::string("Auto alarm #" + std::to_string(counter)));
            alarm.Set(alarmen.ColumnOrdinal("Severity"), static_cast<int32_t>(1 + (rng() % 4)));
            alarmen.AddRow(std::move(alarm));
        }

        engine.Publish(ds);
        std::cout << "\r[C++ Producer] Published #" << counter
                  << " (" << sensoren.RowCount() << " sensors, " << alarmen.RowCount() << " alarms)   " << std::flush;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n[C++ Producer] Done. Total: " << counter << " updates.\n";
    return 0;
}
