// CSharpNet8Test — Cross-language DatasetProxy Interop Tests
using System;
using System.Runtime.InteropServices;

class Program
{
    static int passed = 0;
    static int failed = 0;

    static void Assert(bool condition, string testName, string detail = "")
    {
        if (condition)
        {
            Console.WriteLine($"  PASS: {testName}");
            passed++;
        }
        else
        {
            Console.WriteLine($"  FAIL: {testName} {detail}");
            failed++;
        }
    }

    static void Main()
    {
        Console.WriteLine("=== C# .NET 8 DatasetProxy Tests ===\n");

        // --- Test 1: Producer Flow ---
        Console.WriteLine("--- Producer Flow ---");
        try
        {
            dynamic proxy = Activator.CreateInstance(
                Type.GetTypeFromProgID("ATLProjectcomserverExe.DatasetProxy")!)!;
            
            for (int i = 0; i < 100; i++)
                proxy.AddRow($"user:{i}", $"name:User{i}|dept:IT|level:{i % 5}");
            
            int count = proxy.GetRecordCount();
            Assert(count == 100, "Producer: 100 rijen toegevoegd", $"Got {count}");

            // Park in SharedValue
            dynamic shared = Activator.CreateInstance(
                Type.GetTypeFromProgID("ATLProjectcomserverExe.SharedValue")!)!;
            shared.SetValue(proxy);
            
            Assert(true, "Producer: Proxy geparkeerd in SharedValue");
        }
        catch (Exception ex)
        {
            Assert(false, "Producer Flow", ex.Message);
        }

        // --- Test 2: Consumer Flow ---
        Console.WriteLine("\n--- Consumer Flow ---");
        try
        {
            dynamic shared = Activator.CreateInstance(
                Type.GetTypeFromProgID("ATLProjectcomserverExe.SharedValue")!)!;
            dynamic proxy = shared.GetValue();

            int count = proxy.GetRecordCount();
            Assert(count == 100, "Consumer: 100 rijen gelezen", $"Got {count}");

            string data = proxy.GetRowData("user:0");
            Assert(data.Contains("name:User0"), "Consumer: Data-integriteit OK", data);

            // Paginering
            object keysObj = proxy.FetchPageKeys(0, 10);
            if (keysObj is Array keysArr)
                Assert(keysArr.Length == 10, "Consumer: Paginering (10 keys)", $"Got {keysArr.Length}");
            else
                Assert(false, "Consumer: Paginering", "Geen array retour");
        }
        catch (Exception ex)
        {
            Assert(false, "Consumer Flow", ex.Message);
        }

        // --- Test 3: Bidirectionele Mutatie ---
        Console.WriteLine("\n--- Bidirectionele Mutatie ---");
        try
        {
            dynamic proxy = Activator.CreateInstance(
                Type.GetTypeFromProgID("ATLProjectcomserverExe.DatasetProxy")!)!;
            proxy.AddRow("mut_1", "original");
            proxy.AddRow("mut_2", "original");
            proxy.AddRow("mut_3", "original");

            proxy.UpdateRow("mut_1", "updated");
            proxy.RemoveRow("mut_2");

            int count = proxy.GetRecordCount();
            Assert(count == 2, "Mutatie: 2 records na update+remove", $"Got {count}");
            
            string val = proxy.GetRowData("mut_1");
            Assert(val == "updated", "Mutatie: UpdateRow werkt", val);
            
            bool exists = proxy.HasKey("mut_2");
            Assert(!exists, "Mutatie: RemoveRow werkt");
        }
        catch (Exception ex)
        {
            Assert(false, "Bidirectionele Mutatie", ex.Message);
        }

        // --- Test 4: Error Handling ---
        Console.WriteLine("\n--- Error Handling ---");
        try
        {
            dynamic proxy = Activator.CreateInstance(
                Type.GetTypeFromProgID("ATLProjectcomserverExe.DatasetProxy")!)!;
            
            // Test: niet-bestaande key
            try
            {
                proxy.GetRowData("onbestaandeKey");
                Assert(false, "Error: KeyNotFound exception verwacht");
            }
            catch (COMException ex)
            {
                Assert(ex.Message.Contains("not found"), "Error: KeyNotFound met beschrijving", ex.Message);
            }

            // Test: duplicate key
            proxy.AddRow("dup", "val");
            try
            {
                proxy.AddRow("dup", "val2");
                Assert(false, "Error: DuplicateKey exception verwacht");
            }
            catch (COMException ex)
            {
                Assert(ex.Message.Contains("already exists"), "Error: DuplicateKey met beschrijving", ex.Message);
            }

            // Test: storage mode op gevulde store
            try
            {
                proxy.SetStorageMode(1);
                Assert(false, "Error: StoreModeNotEmpty exception verwacht");
            }
            catch (COMException ex)
            {
                Assert(ex.Message.Contains("not empty"), "Error: StoreModeNotEmpty met beschrijving", ex.Message);
            }
        }
        catch (Exception ex)
        {
            Assert(false, "Error Handling", ex.Message);
        }

        // --- Test 5: Storage Mode ---
        Console.WriteLine("\n--- Storage Mode ---");
        try
        {
            dynamic proxy = Activator.CreateInstance(
                Type.GetTypeFromProgID("ATLProjectcomserverExe.DatasetProxy")!)!;
            
            proxy.SetStorageMode(1); // Unordered
            int mode = proxy.GetStorageMode();
            Assert(mode == 1, "StorageMode: Unordered (1)", $"Got {mode}");

            proxy.AddRow("perf_key", "perf_val");
            string val = proxy.GetRowData("perf_key");
            Assert(val == "perf_val", "StorageMode: CRUD werkt met Unordered");
        }
        catch (Exception ex)
        {
            Assert(false, "Storage Mode", ex.Message);
        }

        // --- Results ---
        Console.WriteLine($"\n========================================");
        Console.WriteLine($"Results: {passed} passed, {failed} failed");
        Environment.ExitCode = failed > 0 ? 1 : 0;
    }
}
