// Written on mac environment
#include <iostream>
#include <curl/curl.h>
#include <string>
#include <fstream>
#include <string.h>
#include <chrono>
#include <openssl/hmac.h>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <unistd.h>
#include <regex>

using json = nlohmann::json;
using namespace std;

const string baseUrl = "https://api.telegram.org/bot";
const string token = "5842114288:AAGMs2IbneIU-3BP0YHEfWAzxD_RjrS3wHM";
const string chatID = "1010606030";
bool FirstRun = true;

struct error
{
	int typeNULL = 0;
};
error handler;

static string gs_strLastResponse;
json TempJForm;
/* concatenate the query parameters to &<key>=<value> */
string joinQueryParameters(const unordered_map<string, string> &parameters)
{
	string queryString = "";
	for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
	{
		if (it == parameters.cbegin())
		{
			queryString += it->first + '=' + it->second;
		}

		else
		{
			queryString += '&' + it->first + '=' + it->second;
		}
	}

	return queryString;
}
bool ErrorHandler(string message)
{
	bool error = true;
	json data = message;

	if (!json::accept(message)) // prevents wierd nlohmann parse error due to binance reaspones
	{
		error = true;
		cout << "parse error\n";
		try
		{
			json j = json::parse(message);
		}
		catch (json::parse_error &e)
		{
			std::cout << e.what() << std::endl;
		}
		exit(3);
	}
	else if (data.is_null()) // is null
	{
		error = true;
		cout << "null detected. Reinitializing request...." << endl;
	}

	/*is only square brackets returned ?*/
	else if (message[0] == '[' && message[1] == ']' && handler.typeNULL < 20)
	{
		error = true;
		cout << "empty square bracket detected. Reinitializing request...." << endl;
		handler.typeNULL++;
		cout << handler.typeNULL << endl;
	}
	/* sqaure bracket handler ended here*/
	else
	{
		error = false;
	}
	return error;
}
/*Sending the HTTP Request and printing the response*/
void executeHTTPRequest(CURL *curl)
{
	CURLcode res;
	gs_strLastResponse = "";
	/* Perform the request, res will get the return code */
	do
	{
		res = curl_easy_perform(curl);
		ofstream logger("tbot_log.txt", ios::app);
		logger << "dumping " << gs_strLastResponse << "\n";
		logger.close();
	} while (ErrorHandler(gs_strLastResponse));
	/* Check for errors */
	if (res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
}

/*Write the server response to string for display*/
size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp)
{
	gs_strLastResponse = (const char *)contents;
	gs_strLastResponse += '\n';
	return size * nmemb;
}

void sendPublicRequest(CURL *curl, string urlPath, unordered_map<string, string> &parameters)
{
	string url = baseUrl + token + urlPath + '?';
	if (!parameters.empty())
	{
		url += joinQueryParameters(parameters);
	}
	// cout << "url:" << url << endl;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	parameters.clear();
	executeHTTPRequest(curl);
}

static string PrevMessage = "-1";
static int PrevID = -1;
static string command = "";
bool Listner()
{
	CURL *curl;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	bool NewMessage = false;
	unordered_map<string, string> parameters;
	if (curl)
	{
		parameters.insert(
			{"offset", "-1"});
		sendPublicRequest(curl, "/getUpdates", parameters);

		/* always cleanup */
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();

	TempJForm = json::parse(gs_strLastResponse);
	int i = 0;
	string message;
	auto TempMessage = TempJForm["result"][0]["message"]["text"];
	auto TempID = TempJForm["result"][0]["update_id"];
	message = TempMessage;
	int ID = TempID;
	command = message;

	if ((!FirstRun && PrevMessage != command) || (PrevID != ID && PrevMessage == command))
	{
		NewMessage = true;
		PrevMessage = command;
	}
	else if (PrevMessage == "-1" || PrevID == -1)
	{
		PrevMessage = command;
		FirstRun = false;
	}
	PrevID = ID;

	return NewMessage;
}

bool SendMessage(string message)
{
	bool MessageStatus = false;
	CURL *curl;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	bool NewMessage = false;
	unordered_map<string, string> parameters;
	if (curl)
	{
		parameters.insert(
			{{"chat_id", chatID},
			 {"text", message}});
		sendPublicRequest(curl, "/sendMessage", parameters);
		MessageStatus = true;
		/* always cleanup */

		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();

	return MessageStatus;
}

string ToEscape(string text)
{
	string escaped = "";
	for (int i = 0; i < text.length(); i++)
	{
		if (text[i] == ' ')
			escaped += "+";
		else if (text[i] == '\n')
			escaped += "%0A";
		else
			escaped += text[i];
	}
	return escaped;
}

void CommandExe()
{
	usleep(2000000);
	cout << "\t\tstarted" << endl;
	while (true)
	{
		if (Listner())
		{
			if (command == "/stat")
			{
				int res = system("ps -C spot");
				if (res == 0)
				{
					SendMessage("running");
				}
				else
					SendMessage("Stopped");
			}
			else if (command == "/startcrypto")
			{
				system("nohup ./spot>log.txt & ");
				int res = system("ps -C spot");
				if (res == 0)
				{
					SendMessage(ToEscape("executed succussfuly"));
				}
				else
					SendMessage(ToEscape("not executed succussfuly"));
			}
			else if (command == "/stopcrypto")
			{
				system("kill 18");
			}
			else if (command == "/profits")
			{
				double total;
				int counter = 0;
				string temp;
				ifstream profit("profits.arb");
				while (profit >> temp)
				{
					total += stod(temp);
					counter++;
				}
				string pass = ToEscape("gained profit is " + to_string(total) + "\ntotal num of opportunities are " + to_string(counter));
				SendMessage(pass);
				profit.close();
			}
			else if (command == "/founds")
			{
				ifstream profit("profits.arb");
				string total;
				string temp;
				while (profit >> temp)
				{
					total += temp + "\n";
				}
				if (total.empty())
				{
					total = "no opportunity found";
				}
				SendMessage(ToEscape(total));
				profit.close();
			}
			else
			{
				SendMessage(ToEscape("unknown command"));
			}
		}
	}
}
int main()
{
	CommandExe();
}