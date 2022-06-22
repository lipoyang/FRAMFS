#include "FRAMFS.h"

FRAMFS fram1(256);
FRAMFS fram2(256);

// Get FRAM Size
void get_fram_size(FRAMFS &fram)
{
    uint32_t total = fram.totalBytes();
    uint32_t used  = fram.usedBytes();
    Serial.printf("Total Size: %7d Bytes\n", total);
    Serial.printf("Used  Size: %7d Bytes\n", used);
}

// Open and Write File
void open_write_file(FRAMFS &fram, char *line)
{
    const char path[] = "/test.txt";

    File file = fram.open(path);
    
    if(!file){
        Serial.println("File not found");
        
        // Create and Write File
        file = fram.open(path, FILE_WRITE);
        if(!file){
            Serial.println("Failed to create file");
        }else{
            Serial.println("Create a file: test.txt");
            file.println(line);
            file.close();
        }
        
    }else{
        // Read File
        Serial.println("Read file: test.txt");
        while(file.available()){
            Serial.write(file.read());
        }
        file.close();
        Serial.println("[EOF]");
        
        // Append File
        file = fram.open(path, FILE_APPEND);
        if(!file){
            Serial.println("Failed to open file for appending");
        }else{
            Serial.println("Append a line: test.txt");
            file.println(line);
            file.close();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    while(!Serial){;}
    Serial.println("FRAM Test (2 Devices)");
    
    // Begin FRAM
    if(!fram1.begin(4, SPI, 4000000, "/fram1", 10, true))
    {
        Serial.println("FRAM Mount Failed!");
        return;
    }
    if(!fram2.begin(32, SPI, 4000000, "/fram2", 10, true))
    {
        Serial.println("FRAM Mount Failed!");
        return;
    }
    
    // Get FRAM Size
    Serial.println("FRAM1:");
    get_fram_size(fram1);
    Serial.println("FRAM2:");
    get_fram_size(fram2);
    
    // Open and Write File
    Serial.println("FRAM1:");
    open_write_file(fram1, "abcdefghijklmn");
    Serial.println("FRAM2:");
    open_write_file(fram2, "ABCDEFGHIJKLMN");
}

void loop()
{
    if(Serial.read())
    {
        char c = Serial.read();

        // Delete File
        if(c == 'd'){
            if(fram1.remove("/test.txt")){
                Serial.println("FRAM1: Delete a file: test.txt");
            }else{
                Serial.println("FRAM1: Failed to delete file");
            }
        }
        if(c == 'D'){
            if(fram2.remove("/test.txt")){
                Serial.println("FRAM2: Delete a file: test.txt");
            }else{
                Serial.println("FRAM2: Failed to delete file");
            }
        }

        // Unformat (just for debug)
        if(c == 'u'){
            fram1.unformat(4);
            Serial.println("FRAM1: Unformatted");
        }
        if(c == 'U'){
            fram2.unformat(32);
            Serial.println("FRAM2: Unformatted");
        }
    }
}
