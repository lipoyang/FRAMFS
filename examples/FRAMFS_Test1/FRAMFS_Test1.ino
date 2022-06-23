#include "FRAMFS.h"

FRAMFS fram(256);

void setup()
{
    Serial.begin(115200);
    while(!Serial){;}
    Serial.println("FRAM Test");
    
    // Begin FRAM
#if true
    if(!fram.begin(4)){
        if(fram.isUnformatted()){
            if(fram.format(4)){
                Serial.println("FRAM Formatted!");
            }else{
                Serial.println("FRAM Format Failed!");
                return;
            }
        }else{
            Serial.println("FRAM Mount Failed!");
            return;
        }
    }
#else
    // or just as below
    if(!fram.beginOrFormat(4)){
        Serial.println("FRAM Mount Failed!");
    }
#endif
    
    // Get FRAM Size
    uint32_t total = fram.totalBytes();
    uint32_t used  = fram.usedBytes();
    Serial.printf("Total Size: %7d Bytes\n", total);
    Serial.printf("Used  Size: %7d Bytes\n", used);
    
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
            file.print("Create a file\n");
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
            file.print("Append a line\n");
            file.close();
        }
    }
}

void loop()
{
    if(Serial.available())
    {
        char c = Serial.read();

        // Delete File
        if(c == 'd'){
            if(fram.remove("/test.txt")){
                Serial.println("Delete a file: test.txt");
            }else{
                Serial.println("Failed to delete file");
            }
        }

        // Unformat (just for debug)
        if(c == 'u'){
            fram.unformat(4);
            Serial.println("Unformatted");
        }
    }
}
