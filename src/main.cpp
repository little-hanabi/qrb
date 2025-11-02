#include <iostream>

#include <qrb/qrb.h>

#ifdef WIN32
int wmain(const int argc, const wchar_t* argv[]) {
#else
int main(const int argc, const char* argv[]) {
#endif
    bool ok = false;
    uint32_t mode = 2;

    if (argc >= 2) {
        const std::string mode_str = fs::path(argv[1]).string(); // 字符串编码转换
        
        if (argc == 8 && (mode_str == "--encode" || mode_str == "-e")) {
            ok = qrb::config(fs::path(argv[2]), fs::path(argv[3]), std::stoi(argv[4]), std::stoi(argv[5]), std::stoi(argv[6]), std::stoi(argv[7]));
            mode = 1;
        } else if (argc == 9 && (mode_str == "--encode" || mode_str == "-e")) {
            ok = qrb::config(fs::path(argv[2]), fs::path(argv[3]), std::stoi(argv[4]), std::stoi(argv[5]), std::stoi(argv[6]), std::stoi(argv[7]), std::stoi(argv[8]));
            mode = 1;
        } else if (argc == 4 && (mode_str == "--decode" || mode_str == "-d")) {
            ok = qrb::config(fs::path(argv[2]), fs::path(argv[3]));
            mode = 0;
        } else if (argc == 5 && (mode_str == "--decode" || mode_str == "-d")) {
            ok = qrb::config(fs::path(argv[2]), fs::path(argv[3]), fs::path(argv[4]));
            mode = 0;
        }
    }

    if (!ok) {
        std::cout << "Version: " << qrb::VERSION << std::endl << std::endl;
        std::cout << "Usage:" << std::endl << std::endl
                  << qrb::NAME << " --encode <input_file> <output_dir> <col> <row> <qr_version> <qr_ecc> [<file_ecc>]" << std::endl
                  << qrb::NAME << " --decode <input_dir>  <output_dir> [<ecc_dir>]" << std::endl;
        
        return 1;
    }

    if (mode == 0) qrb::read();
    else if (mode == 1) qrb::write();
    qrb::clean();

    return 0;
}