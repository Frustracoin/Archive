#include <iostream>
#include <fstream>
#include <filesystem>
#include <bitset>
#include <string>

std::string format_file(std::string name, std::string format){
    if (name[name.length() - 4] == '.') {
        return name;
    }
    return name + '.' + format;
}

void printBinary(char character) {
    for (int i = 7; i >= 0; i--) {
        std::cout << ((character >> i) & 1);
    }
    std::cout << std::endl;
}

struct settings {
    bool openfile = false;
    bool create = false;
    bool merge = false;
    std::string file;
    std::string archive;
};

std::bitset<12> hamm_encode(std::bitset<8> data) {
    std::bitset<12> hafcode;
    hafcode[0] = 0;
    hafcode[1] = 0;
    hafcode[3] = 0;
    hafcode[7] = 0;
    hafcode[2] = data[0];
    hafcode[4] = data[1];
    hafcode[5] = data[2];
    hafcode[6] = data[3];
    hafcode[8] = data[4];
    hafcode[9] = data[5];
    hafcode[10] = data[6];
    hafcode[11] = data[7];
    hafcode[7] = hafcode[7] ^ hafcode[8] ^ hafcode[9] ^ hafcode[10] ^ hafcode[11];
    hafcode[3] = hafcode[3] ^ hafcode[4] ^ hafcode[5] ^ hafcode[6] ^ hafcode[11];
    hafcode[1] = hafcode[1] ^ hafcode[2] ^ hafcode[5] ^ hafcode[6] ^ hafcode[9] ^ hafcode[10];
    hafcode[0] = hafcode[0] ^ hafcode[2] ^ hafcode[4] ^ hafcode[6] ^ hafcode[8] ^ hafcode[10];
    return hafcode;
}

std::bitset<8> hamm_decode(std::bitset<12>& encoded) {
    std::bitset<8> ret;
    bool c0 = encoded[0] ^ encoded[2] ^ encoded[4] ^ encoded[6] ^ encoded[8] ^ encoded[10];
    bool c1 = encoded[1] ^ encoded[2] ^ encoded[5] ^ encoded[6] ^ encoded[9] ^ encoded[10];
    bool c3 = encoded[3] ^ encoded[4] ^ encoded[5] ^ encoded[6] ^ encoded[11];
    bool c7 = encoded[7] ^ encoded[8] ^ encoded[9] ^ encoded[10] ^ encoded[11];
    
    int error_position = c0 + c1 * 2 + c3 * 4 + c7 * 8;
    if (0 > error_position || error_position > 12) {
        std::cerr << "Неисправимая ошибка.\n";
        return ret;
    }
    if (error_position != 0) {
        encoded.flip(error_position - 1);
    }
    ret[0] = encoded[2];
    ret[1] = encoded[4];
    ret[2] = encoded[5];
    ret[3] = encoded[6];
    ret[4] = encoded[8];
    ret[5] = encoded[9];
    ret[6] = encoded[10];
    ret[7] = encoded[11];
    
    return ret;
}

int append(settings set) {
    std::ifstream file(set.file, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл" << std::endl;
        return 1;
    }
    std::ofstream archive(set.archive, std::ios::binary | std::ios::app);
    std::string name = "[file:" + set.file + "]";
    archive << name << '\n';
    char ch = 0;
    char temp = 0;
    int count = 0;
    while (file.get(ch)) {
        std::bitset<8> data(ch);
        std::bitset<12> hafcode_data = hamm_encode(data);
        if (count % 2 == 0) {
            for (int i = 0; i < 8; i++){
                ch <<= 1;
                ch |= hafcode_data[i];
            }
            for (int i = 8; i < 12; i++) {
                temp <<= 1;
                temp |= hafcode_data[i];
            }
            temp <<= 4;
            archive << ch;
        } else {
            temp >>= 4;
            for (int i = 0; i < 4; i++) {
                temp <<= 1;
                temp |= hafcode_data[i];
            }
            for (int i = 4; i < 12; i++) {
                ch <<= 1;
                ch |= hafcode_data[i];
            }
            archive << temp;
            archive << ch;
        }
        count++;
    }
    if (count % 2 == 1) {
        archive << temp;
    }
    file.close();
    archive << '\n';
    archive.close();
    std::cout << "Данные успешно записаны" << std::endl;
    return 0;
}

int extract_file(std::ifstream& archive) {
    std::string line;
    std::getline(archive, line);
    if (line.substr(0, 6) != "[file:") {
        std::cerr << "Ошибка формата архива: ожидались метаданные файла." << std::endl;
        return 1;
    }
    std::string filename = line.substr(6, line.size() - 7);
    std::cout << "Извлекается файл: " << filename << std::endl;
    std::ofstream output(filename.substr(0, filename.size()-4) + "2.txt", std::ios::binary);
    std::string encoded;
    char ch;
    char temp = 0;
    int count = 0;
    while (archive.get(ch) && ch != '\n') {
        std::bitset<12> bits;
        if (count % 2 == 0) {
            for (int i = 0; i < 8; i++) {
                bits[i] = ch & (1 << (7 - i));
            }
            archive.get(temp);
            for (int i = 8; i < 12; i++) {
                bits[i] = temp & (1 << (7 - (i - 8)));
            }
        } else {
            for (int i = 0; i < 4; i++) {
                bits[i] = temp & (1 << (3 - i));
            }
            for (int i = 4; i < 12; i++) {
                bits[i] = ch & (1 << (7 - (i - 4)));
            }
        }
        // for (int i = 0; i < 12; i++) {
        //     std::cout << bits[i];
        // }
        // std::cout << "\n";
        char decoded_char = static_cast<char>(hamm_decode(bits).to_ulong());
        output.put(decoded_char);
        count++;
    }
    output.close();
    std::cout << "Файл успешно извлечен: " << filename << std::endl;
    return 0;
}

int extract_archive(settings set) {
    std::ifstream archive(set.archive, std::ios::binary);
    if (!archive.is_open()) {
        std::cerr << "Не удалось открыть архив" << std::endl;
        return 1;
    }
    while (!archive.eof()) {
        while (extract_file(archive) != 0) {
            std::cerr << "Произошла ошибка при извлечении файла." << std::endl;
            return 1;
        }
    }
    archive.close();
    std::cout << "Все файлы успешно извлечены из архива." << std::endl;
    return 0;
}

int list(settings set){
    if (set.archive.length() == 0) {
        std::cerr << "Архив не открыт" << std::endl;
        return 1;
    }
    std::ifstream archive(set.archive, std::ios::binary);
    std::string line;
    std::getline(archive, line);
    if (line.substr(0, 6) != "[file:") {
        std::cerr << "Ошибка формата архива: ожидались метаданные файла." << std::endl;
        return 1;
    }
    std::string filename = line.substr(6, line.size() - 7);
    std::cout << "Список файлов в архиве:" << std::endl;
    std::cout << " - файл : " << filename << std::endl;
    char ch;
    while (archive.get(ch)) {
        if (ch == '['){
            if (line.substr(0, 6) != "[file:") {
                std::cerr << "Ошибка формата архива: ожидались метаданные файла." << std::endl;
                return 1;
            }
            std::getline(archive, line);
            std::cout << " - файл : " << line.substr(5, line.size() - 6) << std::endl;
        }
    }
    return 0;
}
void help2(){
    std::cout << "- Вводить файлы можно с расширением и без" << std::endl;
    std::cout << "- При вводе файлов без расширения используются расширения по умолчанию" << std::endl;
    std::cout << "- .haf - для архивов " << std::endl;
    std::cout << "- .txt - для текстовых файлов" << std::endl;
    std::cout << "- команда --end прерывает программу только после завершения последнего процесса" << std::endl;
    std::cout << "- при ошибке в выполнении команды выводится сообщение и прекращается работа программы" << std::endl;
    std::cout << "- Убедитесь что не забыли указать архив через --arch перед другими действиями с архивом" << std::endl;
    std::cout << "- !!Запускайте программу с правами администратора, иначе останутся временные файлы из-за ошибки <permition denied>" << std::endl;
}
void help() {
    std::cout << "-c, --create - создание нового архива" << std::endl;
    std::cout << "-o, --arch=[ARHCNAME] - открыть архив" << std::endl;
    std::cout << "-f, --file=[FILENAME] - имя файла с архивом" << std::endl;
    std::cout << "-l, --list - вывести список файлов в архиве" << std::endl;
    std::cout << "-x, --extract - извлечь файлы из архива" << std::endl;
    std::cout << "-a, --append - добавить файл в архив" << std::endl;
    std::cout << "-d, --delete - удалить файл из архива" << std::endl; 
    std::cout << "-A, --concatenate - смерджить два архива" << std::endl;
    std::cout << "--end - Для прекращения ввода напишите" << std::endl;
    std::cout << "--help - чтобы посмотреть список команд напишите" << std::endl;
    return;
}
int deleter(settings set){
    std::cout<< "Введите название файла который хотите удалить" << std::endl;
    std::string file_to_delete;
    std::cin>>file_to_delete;
    file_to_delete = format_file(file_to_delete, "txt");
    std::ofstream temp("Working_Archive_temp_file.haf", std::ios::binary);
    std::ifstream archive(set.archive, std::ios::binary);
    if (!archive.is_open()) {
        std::cerr << "Не удалось открыть архив" << std::endl;
        return 1;
    }
    while (!archive.eof()){
        char ch;
        while(archive.get(ch)){
            temp << ch;
        }
    }
    archive.close();
    temp.close();
    std::ofstream new_archive(set.archive, std::ios::binary);
    std::ifstream temp2("Working_Archive_temp_file.haf", std::ios::binary);
    while (!temp2.eof()){
        char ch;
        while(temp2.get(ch)){
            if (ch == '['){
                std::string line;
                std::string useless;
                std::getline(temp2, line);
                std::cout << line.substr(5, line.size() - 6) << " " << file_to_delete << std::endl;
                if (line.substr(0, 5) == "file:" && line.substr(5, line.size() - 6) == file_to_delete){ 
                    std::getline(temp2, useless);
                }else{
                    new_archive << ch;
                    new_archive << line;
                    new_archive << "\n";
                }
            }else{
                new_archive << ch;
            }
        }
    }
    new_archive.close();
    temp.close();
    temp2.close();
    std::filesystem::remove("Working_Archive_temp_file.haf");
    return 0;
}
int merge(){
    std::cout<< "Введите название первого архива" << std::endl;
    std::string first;
    std::cin >> first;
    first = format_file(first, "haf");
    std::cout << "Введите название второго архива" << std::endl;
    std::string second;
    std::cin >> second;
    second = format_file(second, "haf");
    std::ifstream first_archive(first, std::ios::binary);
    std::ifstream second_archive(second, std::ios::binary);
    if (!first_archive.is_open() || !second_archive.is_open()) {
        std::cerr << "Не удалось открыть архив" << std::endl;
        return 1;
    }
    if (!second_archive.is_open()) {
        std::cerr << "Не удалось открыть архив" << std::endl;
        return 1;
    }  
    std::cout << "Выберите название результирующего архива" << std::endl;
    std::string result;
    std::cin >> result;
    result = format_file(result, "haf");
    std::ofstream archive(result, std::ios::binary);
    while (!first_archive.eof()){
        char ch;
        while(first_archive.get(ch)){
            archive << ch;
        }
    }
    while (!second_archive.eof()){
        char ch;
        while(second_archive.get(ch)){
            archive << ch;
        }
    }
    first_archive.close();
    second_archive.close();
    archive.close();
    std::cout << "Архив успешно смерджен" << std::endl;
    return 0;
}
int parser() {
    std::cout << "Введите команды" << std::endl;
    std::cout << "Для прекращения ввода напишите --end" << std::endl;
    std::cout << "Чтобы посмотреть список команд напишите --help" << std::endl;
    std::string command;
    settings set;
    while (true) {
        std::cin >> command;
        if (command == "--end") {
            break;
            return 0;
        } else if (command == "--help") {
            help();
            std::cout << " введите 1 для дополнительной информации\n введите 0 чтобы продолжить" << std::endl;
            bool x;
            std::cin>>x;
            if (x){
                help2();
            }
        } else if (command == "-f" || command.substr(0, 6) == "--file") {
            if (command.substr(0, 6) == "--file") {
                set.file = format_file(command.substr(7, command.length()), "txt");
            } else {
                set.file = "file.txt";
            }
            set.openfile = true;
        } else if (command == "-o" || command.substr(0, 6) == "--arch") {
            if (command.substr(0, 6) == "--arch") {
                set.archive = command.substr(7, command.length()) + ".haf";
            } else {
                set.archive = "Archive.haf";
            }
            set.create = true;
        } else if (command == "-c" || command == "--create") {
            set.create = true;
            std::cout<<"Введите название архива" << std::endl;
            std::string name;
            std::cin>>name;
            set.archive = format_file(name, "haf");
        } else if (command == "-a" || command == "--append") {
            if (append(set)) return 1;
        } else if (command == "-x" || command == "--extract") {
            if  (extract_archive(set)) return 1;
        } else if (command == "-l" || command == "--list") {
            if (list(set) == 1){
                return 1;
            }
        } else if( command == "-A" || command == "--concatenate"){
            if (merge() == 1){
                return 1;
            }
        } else if (command == "-d" || command == "--delete") {
            if (deleter(set) == 1){
                return 1;
            }
        } else {
            std::cerr << "Неизвестная команда" << std::endl;
        }
    }
    std::cout << "Работа программы окончена" << std::endl;
    return 0;
}

int main() {
    setlocale(LC_ALL, "ru");
    return parser();
}