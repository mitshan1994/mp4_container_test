#include <iostream>
#include <filesystem>
#include <boost/lexical_cast.hpp>

using namespace std;

int main()
{
    int a = boost::lexical_cast<int>(42);

    string cwd = std::filesystem::current_path().string();
    cout << "Current working directory: " << cwd << endl;

	cout << "Hello CMake." << a << endl;
	return 0;
}
