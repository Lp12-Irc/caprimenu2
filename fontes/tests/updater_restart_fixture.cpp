#include <Windows.h>
#include <fstream>
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){std::ofstream file("started.txt");file<<"started";return 0;}
