#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>
#include <stdio.h>

#include "sylar/timestamp.h"
#include "plot.h"

int main(int argc, char const *argv[]) {
    std::vector<double> cpu_usage;
    cpu_usage.reserve(300);
    for (int i = 0; i < 300; ++i) {
        cpu_usage.push_back(1.0 + sin(pow(i / 30.0, 2)));
    }

    Plot plot(640, 100, 600, 2);
    sylar::Timestamp start(sylar::Timestamp::now());

    const int N = 10000;
    for (int i = 0; i < N; ++i) {
        std::string png = plot.plotCpu(cpu_usage);
    }

    double elapsed = sylar::timeDifference(sylar::Timestamp::now(), start);
    std::cout << N << " plots in " << elapsed << " seconds, " << N / elapsed << " PNG per second, "
            << elapsed * 1000.0 / N << " ms per PNG" << std::endl;
    std::string png = plot.plotCpu(cpu_usage);

    std::ofstream ofs("test.png", std::ios::binary | std::ios::out);
    if (!ofs) {
        perror("cannot open test.png");
    }
    ofs << png;
    std::cout << "Image saved to test.png" << std::endl;
    return 0;
}

