#include <iostream>

class Lamp{
    private:
        std::string name;
        int brightness;
        std::string color;
    
    public:
        Lamp(std::string Name, int Brightness, std::string Color){
            name = Name;
            brightness = Brightness;
            color = Color;
        };

        void turnOn(){

        }

        void turnOff(){

        }

        void setBrightness(){

        }

        void setColor(){

        }

        void getStatus(){
            return info;
        }

};

void addLamp(std::string name, int brightness, std::string color){
    Lamp lamp(name, brightness, color);
}

void printMenu(){
    std::cout<<"Что хотите сделать?"<<std::endl;
    std::cout<<"1. Создать лампу"<<std::endl;
    std::cout<<"2. Вывести статус лампы"<<std::endl;
    std::cout<<"3. Поменять яркость лампы"<<std::endl;
    std::cout<<"4. Поменять цвет лампы"<<std::endl;
}

int main(){
    std::string color;
    std::cout<<"Привествую!"<<std::endl;
    bool stop = false;
    int choose;
    std::string name;
    int brightness;
    std::string color;
    while (stop){
        printMenu();
        if (choose == 1){
            std::cout<<"Введите название лампы: ";
            std::cin >> name;
            std::cout<<"Введите яркость лампы: ";
            std::cin >> brightness;
            std::cout<<"Введите цвет лампы: ";
            std::cin >> color;
            addLamp(name, brightness, color);
            
        }
        
        if (choose == 2){
            getStatus();
        }

        if (choose == 3){
            
        }

        if (choose == 4){
            
        }

        if (choose == 5){
            
        }

        if (choose == 0){
            stop = true;
        }

        continue;
    }


    return 0;
}