// Written on ubuntu environment
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <string.h>
#include <chrono>
#include <ctime>
#include <openssl/hmac.h>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <spawn.h>
#include <thread>
#include <cstdlib>
#include <unistd.h>

using namespace std;
using json = nlohmann::json;
using namespace std::chrono;

string baseUrl = "https://api.binance.com";
static string LogFname = "";
const string apiKey = "hj54s3JROd6ueTICoQh2FdE3ezfUiMhbRVoL15yshrNAxSfow5IRwWEBcWMVBn7M";
const string apiSecret = "gCs0iTJaeXRSOqKcnXSSY4HDJKdCBic3HlU8HmHN0PZwreNcS1OwshB0763zaW5i";

static string gs_strLastResponse;
json JsonForm;
string TempRes;

/*This is our callback function which will be executed by curl on perform.
here we will copy the data received to our struct
ptr : points to the data received by curl.
size : is the count of data items received.
nmemb : number of bytes
data : our pointer to hold the data.
*/
struct error
{
	int typeNULL = 0;
};
error handler;
string getTimestamp()
{
	long long ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	return to_string(ms_since_epoch);
}

string encryptWithHMAC(const char *key, const char *data)
{
	unsigned char *result;
	static char res_hexstring[64];
	int result_len = 32;
	string signature;

	result = HMAC(EVP_sha256(), key, strlen((char *)key), const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(data)), strlen((char *)data), NULL, NULL);
	for (int i = 0; i < result_len; i++)
	{
		sprintf(&(res_hexstring[i * 2]), "%02x", result[i]);
	}

	for (int i = 0; i < 64; i++)
	{
		signature += res_hexstring[i];
	}

	return signature;
}

string getSignature(string query)
{
	string signature = encryptWithHMAC(apiSecret.c_str(), query.c_str());
	return "&signature=" + signature;
}

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

/*Sending the HTTP Request and printing the response*/
void executeHTTPRequest(CURL *curl)
{
	CURLcode res;
	gs_strLastResponse = "";
	/* Perform the request, res will get the return code */
	res = curl_easy_perform(curl);
	/* Check for errors */
	if (res != CURLE_OK)
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
	// cout << gs_strLastResponse << endl;
}

/*Write the server response to string for display*/
size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp)
{
	gs_strLastResponse += (const char *)contents;
	gs_strLastResponse += '\n';
	return size * nmemb;
}

void sendPublicRequest(CURL *curl, string urlPath, unordered_map<string, string> &parameters)
{
	string url = baseUrl + urlPath + '?';
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

void sendSignedRequest(CURL *curl, string httpMethod, string urlPath, unordered_map<string, string> &parameters)
{
	string url = baseUrl + urlPath + '?';
	string queryString = "";
	string timeStamp = "timestamp=" + getTimestamp();
	if (!parameters.empty())
	{
		string res = joinQueryParameters(parameters) + '&' + timeStamp;
		url += res;
		queryString += res;
	}

	else
	{
		url += timeStamp;
		queryString += timeStamp;
	}

	string signature = getSignature(queryString);
	url += signature;
	queryString += signature;
	// cout << "url:" << url << endl;

	if (httpMethod == "POST")
	{
		curl_easy_setopt(curl, CURLOPT_URL, (baseUrl + urlPath).c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, queryString.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		parameters.clear();
		executeHTTPRequest(curl);
	}

	else
	{
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		executeHTTPRequest(curl);
	}
}

bool ErrorHandler(string message, string prevasset, string round, string iter)
{
	bool error = true;
	json data = message;

	if (!json::accept(message)) // prevents wierd nlohmann parse error due to binance reaspones
	{
		ofstream log(LogFname, ios::app);
		error = true;
		usleep(9000000);
		log << "parse error\n";
		log.close();
	}
	else if (data.is_null()) // is null
	{
		error = true;
		ofstream log(LogFname, ios::app);
		log << "null detected. Reinitializing request...." << endl;
		log.close();
	}

	/*is only square brackets returned ?*/
	else if (message[0] == '[' && message[1] == ']' && handler.typeNULL <= 60)
	{
		error = true;
		ofstream log(LogFname, ios::app);
		log << handler.typeNULL << "empty square bracket detected. Reinitializing request...."
			<< "\n";
		log.close();
		handler.typeNULL++;
	}
	else if (handler.typeNULL > 60 && prevasset!="null")
	{
		ofstream log(LogFname, ios::app);
		log << "swap on pending for 60 repeated try. Cannot continue.....exiting"
			<< "\n";
		log.close();
		string command = "./RAE " + prevasset + " arb " + round + " " + iter + " terminator";
		int res = system(command.c_str());
		if (res == 0)
		{
			/*unlocking transaction*/
			ofstream end("locked.transaction");
			end << "false";
			end.close();
			/*------------------*/
			exit(6);
		}
		else
		{
			/*locking transaction*/
			ofstream end("locked.transaction");
			end << "true";
			end.close();
			/*------------------*/
			exit(6);
		}
	}
	/* sqaure bracket handler ended here*/
	else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '1' && message[13] == '4') // no data(forbidden)
	{
		error = true;
		ofstream log(LogFname, ios::app);
		log << "binance throuwn forbidden error....exiting"
			<< "\n";
		log.close();
		/*unlocking transaction*/
		ofstream end("locked.transaction");
		end << "false";
		end.close();
		/*------------------*/
		exit(0);
	}
	else if (message[9] == '9' && message[10] == '0' && message[11] == '0' && message[12] == '0')
	{ // binance system on maintainance
		error = true;
		ofstream log(LogFname, ios::app);
		log << "System maintenance"
			<< "\n";
		log.close();
		/*unlocking transaction*/
		ofstream end("locked.transaction");
		end << "false";
		end.close();
		/*------------------*/
		exit(0);
	}
	else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '0' && message[13] == '1') // below min transaction
	{
		ofstream log(LogFname, ios::app);
		log << "below min transaction"
			<< "\n";
		log.close();
		error = true;
	}
	else if (message[9] == '2' && message[10] == '0' && message[11] == '1' && message[12] == '5') // invalid api or ip
	{
		ofstream log(LogFname, ios::app);
		error = true;
		log << "inalid api or ip"
			<< "\n";
		log.close();
		/*unlocking transaction*/
		ofstream end("locked.transaction");
		end << "false";
		end.close();
		/*------------------*/

		exit(0);
	}
	else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '1' && message[13] == '0') // balance is not enough
	{
		ofstream log(LogFname, ios::app);
		error = true;
		log << "user asset freeze failed, please check whether your balance is enough"
			<< "\n";
		log.close();
		/*unlocking transaction*/
		ofstream end("locked.transaction");
		end << "false";
		end.close();
		/*------------------*/

		exit(0);
	}
	else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '0' && message[13] == '0') // Exceeding the maximum transaction quantity
	{
		ofstream log(LogFname, ios::app);
		error = true;
		log << "Exceeding the maximum transaction quantity.....tring again"
			<< "\n";
	}
	else if (message[9] == '1' && message[10] == '1' && message[11] == '0' && message[12] == '2') // caused after not enoguh balance response
	{
		ofstream log(LogFname, ios::app);
		error = true;
		log << "Mandatory parameter was not sent, was empty/null, or malformed......errorr....exiting"
			<< "\n";
		log.close();
		/*unlocking transaction*/
		ofstream end("locked.transaction");
		end << "false";
		end.close();
		/*------------------*/

		exit(0);
	}
	else if (message[9] == '1' && message[10] == '0' && message[11] == '2' && message[12] == '1') // timestamp expired
	{
		error = true;
	}
	else if (message[2] == 'c' && message[3] == 'o' && message[4] == 'd' && message[5] == 'e') // general errors(unknown by me)
	{
		ofstream log(LogFname, ios::app);
		error = true;
		log << "general error"
			<< "\n";
		log.close();
		usleep(1000000);
	}
	else if (message[9] == '9' && message[10] == '0' && message[11] == '0' && message[12] == '0')
	{ // Enter an amount to {0} decimals.
		error = true;
		ofstream log(LogFname, ios::app);
		log << "Enter an amount to {0} decimals."
			<< "\n";
		log.close();
		/*unlocking transaction*/
		ofstream end("locked.transaction");
		end << "false";
		end.close();
		/*------------------*/
		exit(0);
	}
	else
	{
		error = false;
	}
	return error;
}
double bstod(std::string string)
{
	std::stringstream s(string);
	double ret = 0;
	s >> ret;
	return ret;
}
string SignedResp[5];
string *APIcall(string protocol, string path, unordered_map<string, string> par, string key[], int size, int round, int iter, string prevasset = "null")
{
	CURL *curl;
	string queryString;

	curl_global_init(CURL_GLOBAL_ALL);

	/* get a curl handle */
	curl = curl_easy_init();
	/* Adding API key to header */
	struct curl_slist *chunk = NULL;
	chunk = curl_slist_append(chunk, ("X-MBX-APIKEY:" + apiKey).c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	int NumOfKey = size; // length calculation
	/*for time log*/
	time_t t;
	struct tm *tt;
	/*-------------*/
	do
	{
		sendSignedRequest(curl, protocol, path, par);
		/*time getter*/
		time(&t);
		tt = localtime(&t);
		string time = asctime(tt);
		// cout << " size is " << gs_strLastResponse.size() << endl;
		ofstream loger(LogFname, ios::app);
		loger << "dumping round " << round << " iter " << iter << " " << time << " " << gs_strLastResponse << "\n";
		loger.close();
	} while (ErrorHandler(gs_strLastResponse, prevasset, to_string(round), to_string(iter)));
	TempRes = gs_strLastResponse;
	auto APIresponse = json::parse(gs_strLastResponse);
	handler.typeNULL = 0;
	JsonForm = APIresponse;

	if (APIresponse.is_array()) // for array response
	{

		// cout << " yes it is array\n";
		for (int i = 0; i < NumOfKey; i++)
		{
			if (APIresponse[0][key[i]].is_number())
			{
				auto p = APIresponse[0][key[i]].get<int>();
				int TempNum = p;
				SignedResp[i] = to_string(TempNum);
			}
			else
			{
				// cout << "\n arrived here\n";
				SignedResp[i] = APIresponse[0][key[i]];
			}
		}
	}
	else if (!APIresponse.is_array()) // for non-array reponse
	{
		// cout << "\n no array\n";
		for (int i = 0; i < NumOfKey; i++)
		{

			if (APIresponse[key[i]].is_number())
			{

				auto p = APIresponse[key[i]].get<int>();
				int TempNum = p;
				SignedResp[i] = to_string(TempNum);
			}
			else
			{
				SignedResp[i] = APIresponse[key[i]];
			}
		}
	}

	/* always cleanup */
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return SignedResp;
}

string dref(string *str, int index)
{
	string value = str[index];
	return value;
}

string ReadBalance(string asset)
{
	unordered_map<string, string> par;
	string *res;
	string balance;
	/*------read "from" asset------*/
	par.insert(
		{
			{"asset", asset} // asset
		});
	string key[] = {"free"};
	res = APIcall("POST", "/sapi/v3/asset/getUserAsset", par, key, 1, 1, 1);
	balance = dref(res, 0);
	// cout << "balance is " << balance << endl;
	return balance;
}

void CombinationMaker()
{
	unordered_map<string, string> par, s;
	string *res, key[] = {"poolId"};
	vector<string> pairs, USDTpairs, SinglePairs, NonUSDT, graph;
	string Lpair, Rpair;
	int separator;

	/*---------read tradabel pair on bswap------------*/
	par.insert({});
	res = APIcall("GET", "/sapi/v1/bswap/pools", par, key, 1, 1, 1);

	/*---------separating USDT pairs only------------*/
	for (const auto &item : JsonForm)
	{
		string tradeSymbol = string(item["poolName"]);
		pairs.push_back(tradeSymbol);
	}

	for (const string &value : pairs)
	{
		for (int i = 0; i < value.size(); i++) // collect all pairs to vector pair
		{
			if (value[i] == '/')
			{
				separator = i;
				break;
			}
		}

		for (int i = 0; i < separator; i++) // constracting left side pair name
		{
			Lpair += value[i];
		}

		for (int i = separator + 1; i < value.size(); i++) // constracting right side pair name
		{
			Rpair += value[i];
		}
		string temp;
		if (Lpair == "USDT" || Rpair == "USDT") // check wether or not it is usdt pair or non usdt pair
		{
			temp = Rpair + "/" + Lpair;
			USDTpairs.push_back(value);
			if (Lpair == "USDT")
				SinglePairs.push_back(Rpair);
			else
				SinglePairs.push_back(Lpair);
		}
		else
		{
			NonUSDT.push_back(value);
			NonUSDT.push_back(temp);
		}
		Lpair.clear();
		Rpair.clear();
	}

	/*arbitrage path builder using only possible paths in Bswap*/
	string TempHolder;
	// cout << "checking singlepairlist\n";
	for (const string &Lside : SinglePairs)
	{
		for (const string &Rside : SinglePairs)
		{
			TempHolder = Lside + "/" + Rside;
			// cout << h << endl;
			for (const string &Source : NonUSDT)
			{
				if (TempHolder == Source)
				{
					string temp = "USDT " + Lside + " " + Rside;
					graph.push_back(temp);
				}
			}
		}
	}

	ofstream file("graph.dat");
	if (file)
	{
		for (const string &data : graph)
		{
			file << data << "\n";
		}
		file.close();
	}
} // end of combination maker

bool order(string from, string to,string prevasset)
{
	unordered_map<string, string> par;
	string *res;
	string key[5];
	bool result;

	string FromBalance = ReadBalance(from);
	// swap order
	if (stod(FromBalance) > 0)
	{
		// cout << "qauantity is " << FromBalance << "\n";
		key[0] = {"swapId"};
		par.insert(
			{{"quoteAsset", from},		// asset to sell
			 {"baseAsset", to},			// asset to buy
			 {"quoteQty", FromBalance}, // quoteAsset
			 {"timeInForce", "GTC"}});

		res = APIcall("POST", "/sapi/v1/bswap/swap", par, key, 1, 1, 1,prevasset);

		string SwapRes = TempRes;
		// cout<<"the string being checked is"<<SwapRes<<endl;
		if (SwapRes[2] == 's' && SwapRes[3] == 'w' && SwapRes[4] == 'a' && SwapRes[5] == 'p' && SwapRes[6] == 'I' && SwapRes[7] == 'd')
		{
			result = true;
			// cout << "yaa contains swap id" << endl;
		}
		else
		{
			result = false;
			// cout << "no swap id" << endl;
		}
	}

	return result;
}
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
void NegarbInitiator(string a, string b, string c, string rou, string iter)
{
	bool result;
	string req = "./NegArb " + a + " " + b + " " + c + " " + rou + " " + iter + " terminator";
	system(req.c_str());
}
int ArbFinder(string a, string b, string c, string rou, string iter) // algo efficiency is -- 20 round of 134 iterration in 14 mins
{

	/*locking transaction*/
	ofstream transaction("locked.transaction");
	string is_locked;
	transaction << "true";
	transaction.close();
	/*------------------*/

	string searched = a + "-" + b + "-" + c;
	string *res;
	double amount = 50;
	int status = 0;
	/*for time log*/
	time_t t;
	struct tm *tt;
	/*-------------*/
	double net;
	int i = stod(iter);
	int RoundCounter = stod(rou);
	do
	{
		/*---------let path a-b-c------------*/
		unordered_map<string, string> pathA, pathB, pathC;
		string *res, key[] = {"baseQty"};
		string amt = to_string(amount);

		/*---------path a-b------------*/
		pathA.insert(
			{{"quoteAsset", a}, // asset to sell
			 {"baseAsset", b},	// asset to buy
			 {"quoteQty", amt}, // quoteAsset
			 {"timeInForce", "GTC"}});
		res = APIcall("GET", "/sapi/v1/bswap/quote", pathA, key, 1, RoundCounter, i);
		string SwapPrice1 = dref(res, 0);

		/*---------path b-c------------*/
		pathB.insert(
			{{"quoteAsset", b},		   // asset to sell
			 {"baseAsset", c},		   // asset to buy
			 {"quoteQty", SwapPrice1}, // quoteAsset
			 {"timeInForce", "GTC"}});
		res = APIcall("GET", "/sapi/v1/bswap/quote", pathB, key, 1, RoundCounter, i);
		string SwapPrice2 = dref(res, 0);

		/*---------path c-a------------*/
		pathC.insert(
			{{"quoteAsset", c},		   // asset to sell
			 {"baseAsset", a},		   // asset to buy
			 {"quoteQty", SwapPrice2}, // quoteAsset
			 {"timeInForce", "GTC"}});
		res = APIcall("GET", "/sapi/v1/bswap/quote", pathC, key, 1, RoundCounter, i);
		string SwapPrice3 = dref(res, 0);

		/*calculation part*/
		double total;
		total = stod(SwapPrice3);
		net = total - amount;

		if (net >= 0)
		{
			if (order(a, b,"null"))
			{
				usleep(2253037);
				if (order(b, c,a))
				{
					usleep(2253037);
					if (order(c, a,b))
					{
						ofstream profit("profits.arb", ios::app);
						/*time getter*/
						time(&t);
						tt = localtime(&t);
						string time = asctime(tt);
						/*---------------t*/
						profit << net << " " << searched << " " << SpaceRemover(time) << "Arb"
							   << "\n";
						profit.close();
						ofstream loger(LogFname, ios::app);
						loger << "---------profitable------------" << endl;
						loger.close();
					}
					else
					{
						status = -3;
					}
				}
				else
				{
					status = -2;
				}
			}
			else
			{
				status = -1;
			}
		}
		else if (net > -0.07 && net < 0)
		{
			std::thread(NegarbInitiator, a, b, c, to_string(RoundCounter), to_string(i)).detach(); // exeute order
			ofstream lo(LogFname);
			lo << "net seen by arb to Neg is " << net << " " << searched << "\n";
			lo.close();
			ofstream arblog("LogFname", ios::app);
			arblog << "---------passed for NegArb------------" << endl;
			arblog.close();
		}
		else
		{
			ofstream loger(LogFname, ios::app);
			loger << "\t\t---------not profitable------------" << endl;
			loger.close();
			break;
		}

	} while (net >= 0);

	/*unlocking transaction*/
	ofstream end("locked.transaction");
	end << "false";
	end.close();
	/*------------------*/
	return status;
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
	else
		free = true;
	return free;
}
int main(int argc, char **argv)
{
	int result;
	bool controller = true;
	int i = 0;
	auto start = chrono::high_resolution_clock::now();
	while (controller)
	{
		if (CheckFree())
		{
			/*for time log*/
			time_t t;
			struct tm *tt;
			/*time getter*/
			time(&t);
			tt = localtime(&t);
			string time = asctime(tt);
			string roundC = argv[4];
			string iterC = argv[5];
			LogFname = "./run_log_arb/arb_" + roundC + " " + iterC + "_" + time + ".txt";
			controller = false;
			result = ArbFinder(argv[1], argv[2], argv[3], argv[4], argv[5]);
		}
		auto end = chrono::high_resolution_clock::now();
		double time_taken = chrono::duration_cast<chrono::microseconds>(end - start).count();
		if (time_taken > 30000000)
		{
			result = -12;
			break;
		}
		i++;
		usleep(3000000);
	}
	return result;
}
