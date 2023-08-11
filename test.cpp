#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
using namespace std;
using json = nlohmann::json;
static string LogFname="";
string SpaceRemover(string text)
{
	string ClearedText = "";
	for (int i = 0; i < text.length(); i++)
	{
		if (text[i] == ' ')
			ClearedText += '/';
		else
			ClearedText += text[i];
	}
	return ClearedText;
}
int main(int, char *[])
{
	json a= [{"asset":"TLM","free":"5736.0568787","locked":"0","freeze":"0","withdrawing":"0","ipoable":"0","btcValuation":"0.00206498"}];
}