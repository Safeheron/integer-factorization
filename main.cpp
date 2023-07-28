#include <chrono>
#include <iostream>
#include <fstream>
#include <boost/thread.hpp>
#include <boost/process.hpp>
#include "crypto-bn/rand.h"

using safeheron::bignum::BN;
namespace bp = boost::process;

int counter = 0;
int targetCounter = 453;
std::string successfulLine;
boost::mutex mutex;
boost::atomic<bool> hasTrueResult(false);
using FunctionPtr = void (*)(const char *, const BN &);

void generate_safe_prime(int nbits, int pbits, BN &p, BN &q, BN &N){
    p = safeheron::rand::RandomSafePrime(pbits);
    q = safeheron::rand::RandomSafePrime(nbits - pbits);
    N = p * q;
}

int get_round(std::string &line){
    std::istringstream iss(line);
    std::string word;
    int number;
    iss >> word;
    if (word == "Run" && iss >> number) {
        return number;
    } else {
        return 0;
    }
}

int call_ecm(const char* programPath, const BN &N) {
    bp::ipstream pipeOut;
    bp::opstream pipeIn;
    bp::child c(programPath, bp::std_out > pipeOut, bp::std_in < pipeIn);

    std::string input = N.Inspect(10);
    pipeIn << input << std::endl;
    pipeIn.close();

    std::string line;
    int curr_round = 1;
    //execute ecm program and get its output
    while (std::getline(pipeOut, line)) {
        //std::cout <<"ecm: " << line << std::endl;
        if(line.find("Run") != std::string::npos) {
            curr_round = get_round(line);
            if(curr_round == 2) break;
        }
        if (line.find(" Factor found ") != std::string::npos) {
            boost::unique_lock<boost::mutex> lock(mutex);
            if (!hasTrueResult) {
                hasTrueResult = true;
                std::cout << line << " " << counter << std::endl;
                successfulLine = line;
            }
            lock.unlock();
            pipeOut.close();
            return curr_round;
        }
    }
    pipeOut.close();
    c.terminate();
    return 0;
}

void threadFunction_find(const char* programPath, const BN &N){
    int curr_round = 0;
    while(curr_round == 0 && !hasTrueResult){
        boost::unique_lock<boost::mutex> lock(mutex);
        counter++;
        lock.unlock();
        curr_round = call_ecm(programPath, N);
    }
}

void threadFunction_judge(const char* programPath, const BN &N){
    int curr_round = 0;
    while(curr_round == 0 && !hasTrueResult) {
        boost::unique_lock<boost::mutex> lock(mutex);
        if (counter >= targetCounter) break;
        counter++;
        lock.unlock();
        curr_round = call_ecm(programPath, N);
    }
}

void execute_multi_threads(int &threadCount, const char* programPath, const BN &N, const int &mod){
    boost::thread threads[threadCount];
    FunctionPtr tfunction = &threadFunction_find;
    if(mod == 2){
        tfunction = &threadFunction_judge;
    }

    for (int i = 0; i < threadCount; ++i) {
        threads[i] = boost::thread(tfunction, programPath, N);
    }
    for (int i = 0; i < threadCount; ++i) {
        threads[i].join();
    }
}

bool processArguments(int argc, char* argv[], std::string &ecmPath, std::string &filePath, int &nNum, int &nbits, int &pbits, int &threads, int &mod) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-ecm") {
            if (i + 1 < argc) {
                ecmPath = argv[++i];
            }
        } else if (arg == "-f") {
            if (i + 1 < argc) {
                filePath = argv[++i];
            }
        } else if (arg == "-nn") {
            if (i + 1 < argc) {
                nNum = std::stoi(argv[++i]);
                if (nNum <= 0) {
                    std::cerr << "The number of integers cannot be negative or zero.\n";
                    return false;
                }
            }
        } else if (arg == "-nbits") {
            if (i + 1 < argc) {
                nbits = std::stoi(argv[++i]);
                if (nbits <= 0) {
                    std::cerr << "Bits of the integer cannot be negative or zero.\n" ;
                    return false;
                }
            }
        } else if (arg == "-pbits") {
            if (i + 1 < argc) {
                pbits = std::stoi(argv[++i]);
                if (pbits <= 0) {
                    std::cerr << "Bits of the factor cannot be negative or zero.\n" ;
                    return false;
                }
            }
        } else if (arg == "-t") {
            if (i + 1 < argc) {
                threads = std::stoi(argv[++i]);
                if (threads <= 0) {
                    std::cerr << "The number of threads cannot be negative or zero.\n";
                    return false;
                }
            }
        } else if (arg == "-m") {
            if (i + 1 < argc) {
                mod = std::stoi(argv[++i]);
                if (!(mod == 1 || mod == 2)) {
                    std::cerr << "The mod can only be 1 or 2.\n";
                    return false;
                }
            }
        } else if (arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [-ecm ecm path] [-f integer file] [-nn number of integers] [-nbits bits of the integer] [-pbits bits of the factor] [-t number of threads] [-m function mod].\n";
            std::cout << "Arguments:\n";
            std::cout << "  -ecm: ecm path" << std::endl;
            std::cout << "  -f: integer file path" << std::endl;
            std::cout << "  -nn: number of integers" << std::endl;
            std::cout << "  -nbits: bits of the integer" << std::endl;
            std::cout << "  -pbits: bits of the factor" << std::endl;
            std::cout << "  -t: number of threads" << std::endl;
            std::cout << "  -m: function mod. 1 : factorize; 2 : judge" << std::endl;
        } else {
            std::cerr << "Error arguments: type \"" << argv[0] << " -h\" to get help.\n";
            return false;
        }
    }
    return true;
}

std::string checkOptimalB1(int pbits) {
    if (pbits <= 66) {
        targetCounter = 221;
        return " 5e4 1.3e7 ";
    } else if (pbits <= 83) {
        targetCounter = 453;
        return " 25e4 1.3e8 ";
    } else if (pbits <= 99) {
        targetCounter = 984;
        return " 1e6 1.0e9 ";
    } else if (pbits <= 116) {
        targetCounter = 2541;
        return " 3e6 5.7e9 ";
    } else if (pbits <= 132) {
        targetCounter = 4949;
        return " 11e6 3.5e10 ";
    } else if (pbits <= 149) {
        targetCounter = 8266;
        return " 43e6 2.4e11 ";
    } else if (pbits <= 166) {
        targetCounter = 20158;
        return " 11e7 7.8e11 ";
    } else if (pbits <= 182) {
        targetCounter = 47173;
        return " 26e7 3.2e12 ";
    } else {
        targetCounter = 77666;
        return " 85e7 1.6e13 ";
    }
}

int main(int argc, char* argv[]){
    if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        std::cout << "Usage: " << argv[0] << " [-ecm ecm path] [-f integer file] [-nn number of integers] [-nbits bits of the integer] [-pbits bits of the factor] [-t number of threads] [-m function mod].\n";
        std::cout << "Arguments:\n";
        std::cout << "  -ecm: ecm path" << std::endl;
        std::cout << "  -f: integer file path" << std::endl;
        std::cout << "  -nn: number of integers" << std::endl;
        std::cout << "  -nbits: bits of the integer" << std::endl;
        std::cout << "  -pbits: bits of the factor" << std::endl;
        std::cout << "  -t: number of threads" << std::endl;
        std::cout << "  -m: function mod. 1 : factorize; 2 : judge" << std::endl;
        return 0;
    }

    int mod = 1;
    int threads= 1;
    int nNum = 1;
    int nbits = 2048;
    int pbits = 80;
    std::string filePath;
    std::string ecmPath = "ecm"; //default
    bool ok = processArguments(argc, argv, ecmPath, filePath, nNum, nbits, pbits, threads, mod);
    if (!ok || nbits <= pbits) {
        std::cerr << "Input arguments is invalid. Please check and type it again.\n";
        return 1;
    }
    if (filePath.empty()) {
        filePath = "./integer.txt";
        std::cout << "****** Generate N: ******\n";
        std::ofstream integerTxt(filePath, std::ios::out | std::ios::trunc);
        for (int i = 0; i < nNum; ++i) {
            BN p, q, N;
            generate_safe_prime(nbits, pbits, p, q, N);
            std::cout << "p: " << p.Inspect(10) << std::endl;
            std::cout << "q: " << q.Inspect(10) << std::endl;
            std::cout << "N: " << N.Inspect(10) << std::endl;
            integerTxt << N.Inspect() << std::endl;
        }
        std::cout << "****** Generation Finished. ******\n\n";
        integerTxt.close();
    }

    std::string ecmCommand = ecmPath + " -c 2 " + checkOptimalB1(pbits) + "-mpzmod";

    std::ifstream infile(filePath);
    if (!infile) {
        std::cerr << "Failed to open the input file." << std::endl;
        return 1;
    }

    std::ofstream outfile("factor.txt", std::ios::out | std::ios::trunc);
    outfile << ecmCommand << " threads: " << threads << " mod: " << mod << std::endl;

    std::cout << "************ Start ecm factor **************" << std::endl;
    std::string line;
    std::chrono::milliseconds sum;
    while (std::getline(infile, line)) {
        std::cout << "ecm: " << ecmCommand << "  threads: " << threads << " mod: " << mod << std::endl;
        //reset
        hasTrueResult = false;
        counter = 0;
        //read N
        BN N = BN(line.c_str(),16);
        std::cout << "Read N: " << N.Inspect(10) << std::endl;
        outfile << N.Inspect() << std::endl;

        auto start = std::chrono::high_resolution_clock::now();
        execute_multi_threads(threads, ecmCommand.c_str(), N, mod);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        sum += duration;
        //write the result
        if (hasTrueResult) {
            //std::cout << successfulLine << " " << counter << std::endl;
            outfile << successfulLine << " " << counter << " " << duration.count() << "ms" << std::endl;
        } else {
            if (mod == 2) {
                std::cout << "********** No factor found in " << counter << " times.\n";
                outfile << "********** No factor found in " << counter << " times.\n";
            } else {
                std::cout << "Unknown error, please check your program.\n";
            }
        }
        std::cout << "Execute time: " << duration.count() << " ms.\n";
    }
    outfile << sum.count() << "ms" << std::endl;
    std::cout << "Total time: " << sum.count() << " ms.\n";
    outfile.close();
    infile.close();
    return 0;
}

