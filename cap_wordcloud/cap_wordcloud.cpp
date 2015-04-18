
#include <cstdio>
#include <thread>

int main(int /*argc*/, char** /*argv*/)
{
    printf("test\n");
    std::this_thread::sleep_for(std::chrono::seconds(10));
}

