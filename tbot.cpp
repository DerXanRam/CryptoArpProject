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
const string chatID[3] = {"1010606030", "5753875677", "6121768286"};
string static id;
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
	// cout<<TempJForm.dump()<<endl;
	string TempId;
	if (!TempJForm["result"][0]["message"]["chat"]["username"].is_null())
		TempId = TempJForm["result"][0]["message"]["chat"]["username"];
	else
		return false;
	if (TempId == "Junedijoo")
		id = to_string(5753875677);
	else if (TempId == "rbxapi")
		id = to_string(1010606030);
	else if (TempId == "Tapitapin")
		id = to_string(6121768286);

	string message;
	auto TempMessage = TempJForm["result"][0]["message"]["text"];
	auto TempID = TempJForm["result"][0]["update_id"];
	message = TempMessage;
	int ID = TempID;
	command = message;

	if (!FirstRun && PrevID != ID)
	{
		NewMessage = true;
		PrevID = ID;
	}
	else if (FirstRun)
	{
		FirstRun = false;
		NewMessage = false;
		PrevID = ID;
		PrevID = ID;
	}
	else
	{
		NewMessage = false;
		PrevID = PrevID;
	}

	if (id != chatID[0] && id != chatID[1] && id != chatID[3])
		NewMessage = false;

	return NewMessage;
}
double bstod(string string)
{
	std::stringstream s(string);
	double ret = 0;
	s >> ret;
	return ret;
}
bool SendMessage(string message)
{
	bool MessageStatus = false;
	CURL *curl;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	string currentid = id;
	bool NewMessage = false;
	unordered_map<string, string> parameters;
	if (curl)
	{
		parameters.insert(
			{{"chat_id", currentid},
			 {"text", message}});
		sendPublicRequest(curl, "/sendMessage", parameters);
		MessageStatus = true;
		/* always cleanup */

		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();

	return MessageStatus;
}

bool CheckFree()
{
	bool free;
	ifstream transaction("locked.transaction");
	string is_locked;
	transaction >> is_locked;
	if (is_locked.empty())
		free = false;
	else if (is_locked == "true")
		free = false;
	else if (is_locked == "false")
		free = true;
	else
		free = false;
	return free;
}

void transaction(string comm)
{
	if (comm == "lock")
	{
		/*locking transaction*/
		ofstream transaction("locked.transaction");
		string is_locked;
		transaction << "true";
		transaction.close();
		/*------------------*/
	}
	else
	{
		/*unlocking transaction*/
		ofstream end("locked.transaction");
		end << "false";
		end.close();
		/*------------------*/
	}
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

bool is_FileEmpty(ifstream &pFile)
{
	return pFile.peek() == ifstream::traits_type::eof();
}

int PID(string pname)
{
	char buf[512];
	const char *b;
	string c = "pidof -s " + pname;
	b = c.c_str();
	FILE *cmd_pipe = popen(b, "r");
	fgets(buf, 512, cmd_pipe);
	pid_t pid = strtoul(buf, NULL, 10);
	pclose(cmd_pipe);
	int p = pid;
	return p;
}
string ProcessManager(string command, string pname)
{
	string exeResult = "";
	if (command == "stat")
	{
		string p = "ps -C " + pname;
		int res = system(p.c_str());
		if (res == 0)
		{
			exeResult = "working"; // working
		}
		else
			exeResult = "stopped"; // stoped
	}

	else if (command == "stop")
	{
		int id = PID(pname);
		string stop = "kill " + to_string(id);
		if (pname == "spot")
		{
			if (CheckFree())
			{
				transaction("lock");
				int res = -1;
				if (PID(pname) > 0)
				{
					res = system(stop.c_str());
				}

				if (res == 0)
				{
					exeResult = pname + " stopped succsfully"; // killed succsfully
					transaction("unlock");
				}
				else
				{
					exeResult = "cann't kill or died before " + pname; // kill failed
					transaction("unlock");
				}
			}
			else
			{
				exeResult = "cann't stop while on transaction"; // kill failed
			}
		}
		else
		{
			int res = -1;
			if (PID(pname) > 0)
				res = system(stop.c_str());
			if (res == 0)
			{
				exeResult = "killed"; // killed succsfully
			}
			else if (res == -1)
			{
				exeResult = "DiedOrNotExist";
			}
			else
				exeResult = "Unexpected"; // kill failed
		}
	}
	else if (command == "start")
	{
		string start;
		if (pname == "spot")
		{
			start = "nohup ./" + pname + ">log.txt &";
		}
		else if (pname == "tbot")
		{
			start = "nohup ./" + pname + ">tlog.txt &";
		}
		else
		{
			start = "./" + pname;
		}

		int res = system(start.c_str());
		if (res == 0)
		{
			exeResult = "exed"; // excuted succsfully
		}
		else
			exeResult = "failed"; // failed to execute
	}
	return ToEscape(exeResult);
}

void backup(int hour)
{
	usleep(30000000);
	/*file opening here*/
	time_t files = time(0);
	tm *ltm = localtime(&files);
	string month = to_string(ltm->tm_mon);
	string day = to_string(ltm->tm_mday);
	string folderName = month + "_" + day + "_" + to_string(hour);
	string comm = "mkdir " + folderName;
	system(comm.c_str()); // opennig the folder for the day and hour

	/*file opening here*/
	string FilesToLog[4] = {"log.txt", "tlog.txt", "profits.arb", "emergency.message"};
	for (int i = 0; i < 4; i++)
	{
		string cpycomm = "cp " + FilesToLog[i] + " ./" + folderName + "/" + FilesToLog[i];
		fstream check(FilesToLog->c_str());
		if (!check)
			continue;
		system(cpycomm.c_str());
	}
	/*----------------------*/
	transaction("unlock");
}
void CommandExe()
{
	usleep(2000000);
	cout << "\t\tstarted" << endl;
	bool log1, log6, log12, log18;
	log1 = log6 = log12 = log18 = false;

	while (true)
	{
		time_t now = time(0);
		tm *ltm = localtime(&now);
		int hour = ltm->tm_hour;
		if (Listner())
		{
			if (command == "/stat")
			{
				SendMessage(ProcessManager("stat", "spot"));
			}
			else if (command == "/startcrypto")
			{
				SendMessage(ProcessManager("start", "spot"));
			}

			else if (command == "/stopcrypto")
			{
				SendMessage(ProcessManager("stop", "spot"));
			}
			else if (command == "/stoptbot")
			{
				SendMessage(ProcessManager("stop", "tbot"));
			}
			else if (command == "/reward")
			{
				if (CheckFree())
				{
					if (ProcessManager("stop", "spot") == "spot+stopped+succsfully" || ProcessManager("stop", "spot") == "cann't+kill+or+died+before+spot")
					{
						/*reseting swap status from previous condition*/
						ofstream swa("swap.rew");
						swa << "false";
						swa.close();
						/*--------------------------------------------*/
						transaction("lock");
						system("./RAE>slog.txt");
						transaction("lock");
						ifstream ch("swap.rew");
						string t;
						while (ch >> t)
						{
							if (t == "true")
							{
								backup(hour);
								SendMessage(ToEscape("succsussfully collected the swap reward.Please start the spot"));
								transaction("unlock");
							}
							else if (t == "abnormal")
							{
								SendMessage(ToEscape("abnormal halt or shutdown of RAE."));
								transaction("unlock");
							}
							else
							{
								SendMessage(ToEscape("Below minimum amount to collect swap. Can not collect right now.\n please restart the spot."));
								transaction("unlock");
							}
						}
					}
					else
					{
						SendMessage(ToEscape("abnormal message from process manager to stop the spot arbitrager"));
					}
					transaction("unlock");
				}
				else
				{
					SendMessage(ToEscape("can not collect the swap reward. transaction locked by arbitrager"));
				}
			}
			else if (command == "/profits")
			{
				double total = 0;
				int counter = 0;
				string temp, loggedPath, date, by;
				ifstream profit("profits.arb");
				if (!is_FileEmpty(profit))
				{
					while (profit >> temp >> loggedPath >> date >> by)
					{
						total += bstod(temp);
						counter++;
					}
				}
				else
				{
					total = 0.0;
					counter = 0;
				}

				string pass = ToEscape("gained profit is " + to_string(total) + " USDT" + "\ntotal num of opportunities are " + to_string(counter));

				SendMessage(pass);
				profit.close();
			}
			else if (command == "/founds")
			{
				ifstream profit("profits.arb");
				string total;
				string temp, loggedPath, date, by;
				while (profit >> temp >> loggedPath >> date >> by)
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
			else if (command == "/founddetail")
			{
				ifstream profit("profits.arb");
				string total;
				string temp, loggedPath, date, by;
				;
				while (profit >> temp >> loggedPath >> date >> by)
				{
					total += temp + "\n" + loggedPath + "\n" + date + " \n" + by + "\n";
					total += "___________________________\n";
				}
				if (total.empty())
				{
					total = "no opportunity found";
				}
				SendMessage(ToEscape(total));
				profit.close();
			}
			else if (command == "/lockstatus")
			{
				ifstream profit("locked.transaction");
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
			else if (command == "/tlock")
			{
				ofstream profit("locked.transaction");
				string total;
				string temp;

				if (profit)
				{
					profit << "true";
					profit.close();
					SendMessage(ToEscape("transaction is locked now"));
				}
				else
				{
					SendMessage(ToEscape("locked.transaction file openning failed"));
					profit.close();
				}
			}
			else if (command == "/tunlock")
			{
				ofstream profit("locked.transaction");
				string total;
				string temp;

				if (profit)
				{
					profit << "false";
					profit.close();
					SendMessage(ToEscape("transaction is unlocked now"));
				}
				else
				{
					SendMessage(ToEscape("locked.transaction file openning failed"));
					profit.close();
				}
			}
			else if (command == "/mbackup")
			{
				if (CheckFree())
				{
					backup(hour);
					SendMessage(ToEscape("manual backup of log is succsufully finished."));
				}
				else
				{
					SendMessage(ToEscape("can not exeute manual backup because transaction is locked."));
				}
			}
			else
			{
				SendMessage(ToEscape("unknown command or stranger user"));
			}
		}
		if ((hour == 6 && log6 == false) || (hour == 12 && log12 == false) || (hour == 18 && log18 == false || (hour == 1 && log1 == false)))
		{
			if (hour == 1 && log1 == false)
			{
				bool condition = true;
				while (condition)
				{
					if (CheckFree())
					{
						backup(hour);
						log1 = true;
						log6 = false;
						condition = false;
						id = to_string(1010606030);
						SendMessage(ToEscape("hour " + to_string(hour) + "  backup of log is succsufully finished."));
					}
				}
			}
			else if (hour == 6 && log6 == false)
			{
				bool condition = true;
				while (condition)
				{
					if (CheckFree())
					{
						backup(hour);
						log6 = true;
						log12 = false;
						condition = false;
						id = to_string(1010606030);
						SendMessage(ToEscape("hour " + to_string(hour) + "  backup of log is succsufully finished."));
					}
				}
			}
			else if (hour == 12 && log12 == false)
			{
				bool condition = true;
				while (condition)
				{
					if (CheckFree())
					{
						backup(hour);
						log12 = true;
						log18 = false;
						condition = false;
						id = to_string(1010606030);
						SendMessage(ToEscape("hour " + to_string(hour) + "  backup of log is succsufully finished."));
					}
				}
			}
			else if (hour == 18 && log18 == false)
			{

				bool condition = true;
				while (condition)
				{
					if (CheckFree())
					{
						backup(hour);
						log18 = true;
						log1 = false;
						condition = false;
						id = to_string(1010606030);
						SendMessage(ToEscape("hour " + to_string(hour) + "  backup of log is succsufully finished."));
					}
				}
			}
		}
	}
}

int main()
{
	CommandExe();
}