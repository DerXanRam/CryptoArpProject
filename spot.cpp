// Written on ubuntu environment
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <string.h>
// #include <chrono>
#include <thread>
#include <ctime>
#include <openssl/hmac.h>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <spawn.h>
#include <cstdlib>
#include <unistd.h>

using namespace std;
using json = nlohmann::json;
// using namespace std::chrono;

bool TestMode = false;
string baseUrl = "https://api.binance.com";

string apiKey;	  // = "9YpTJujwgAA9HcB167yjhQlXyzOcIqpDLQtJURT617hJvSxwEkKwSnpwmVGY7TfI";
string apiSecret; //= "IWZkv78zFn1BIene4ukwtewW9n4gVVsdVepREE4mgfCdChylCbwv905fvEjqn6ZQ";

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

bool ErrorHandler(string message)
{
	bool error = true;
	json data = message;

	if (!json::accept(message)) // prevents wierd nlohmann parse error due to binance reaspones
	{
		error = true;
		usleep(9000000);
		cout << "parse error\n";
	}
	else if (data.is_null()) // is null
	{
		error = true;
		cout << "null detected. Reinitializing request...." << endl;
	}

	/*is only square brackets returned ?*/
	else if (message[0] == '[' && message[1] == ']' && handler.typeNULL <= 25)
	{
		error = true;
		cout << "empty square bracket detected. Reinitializing request...." << endl;
		handler.typeNULL++;
		cout << handler.typeNULL << endl;
	}
	else if (handler.typeNULL > 25)
	{
		cout << "swap on pending for 19 repeated try. Cannot continue.....exiting" << endl;
		exit(6);
	}
	/* sqaure bracket handler ended here*/
	else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '1' && message[13] == '4') // no data(forbidden)
	{
		error = true;
		cout << "binance throuwn forbidden error....exiting" << endl;
		exit(0);
	}

	else if (message[9] == '2' && message[10] == '0' && message[11] == '1' && message[12] == '5') // invalid api or ip
	{
		error = true;
		exit(0);
	}
	else if (message[9] == '1' && message[10] == '0' && message[11] == '2' && message[12] == '1') // timestamp expired
	{
		error = true;
	}
	else if (message[2] == 'c' && message[3] == 'o' && message[4] == 'd' && message[5] == 'e') // general errors(unknown by me)
	{
		error = true;
		usleep(1000000);
		exit(0);
	}
	else if (message[9] == '9' && message[10] == '0' && message[11] == '0' && message[12] == '0')
	{ // Enter an amount to {0} decimals.
		error = true;
		cout << "Enter an amount to {0} decimals." << endl;
		exit(0);
	}
	else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '0' && message[13] == '1')
	{ // Below the minimum transaction quantity
		error = true;
		cout << "Below the minimum transaction quantity." << endl;
		exit(0);
	}
	else if (message[9] == '9' && message[10] == '0' && message[11] == '0' && message[12] == '0')
	{ // binance system on maintainance
		error = true;
		cout << "System maintenance" << endl;
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
string *APIcall(string protocol, string path, unordered_map<string, string> par, string key[], int size, int round, int iter)
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
		cout << "dumping round " << round << " iter " << iter << " " << time << " " << gs_strLastResponse << endl;
	} while (ErrorHandler(gs_strLastResponse));
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

bool order(string from, string to)
{
	unordered_map<string, string> par;
	string *res;
	string key[5];
	bool result;

	string FromBalance = ReadBalance(from);
	// swap order
	if (bstod(FromBalance) > 0)
	{
		// cout << "qauantity is " << FromBalance << "\n";
		key[0] = {"swapId"};
		par.insert(
			{{"quoteAsset", from},		// asset to sell
			 {"baseAsset", to},			// asset to buy
			 {"quoteQty", FromBalance}, // quoteAsset
			 {"timeInForce", "GTC"}});

		res = APIcall("POST", "/sapi/v1/bswap/swap", par, key, 1, 1, 1);

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
	else
	{
		cout << "no asset. amount of asset is 0" << endl;
		exit;
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

void arbInitiator(string a, string b, string c, string rou, string iter)
{
	bool result;
	string req = "./arb " + a + " " + b + " " + c + " " + rou + " " + iter + " terminator";
	system(req.c_str());
}
void NegarbInitiator(string a, string b, string c, string rou, string iter)
{
	bool result;
	string req = "./NegArb " + a + " " + b + " " + c + " " + rou + " " + iter + " terminator";
	system(req.c_str());
}
double price[6];
void ArbFinder() // algo efficiency is -- 20 round of 134 iterration in 14 mins
{

	string a[144], b[144], c[144];
	ifstream input("graph.dat");
	while (!input)
	{
		CombinationMaker();
	}
	string pair1, pair2, pair3;
	int k = 0;
	while (input >> pair1 >> pair2 >> pair3)
	{
		a[k] = pair1;
		b[k] = pair2;
		c[k] = pair3;
		k++;
	}
	input.close();
	cout << "\n\n\t\t Arbitrage bot v2.0\n"
		 << endl;
	string searched;
	ifstream command;
	int RoundCounter = 1;
	double amount = 50;
	int OrderExe = 0;
	/*for time log*/
	time_t t;
	struct tm *tt;
	/*-------------*/
	while (RoundCounter < 1500)
	{
		for (int i = 0; i < 121; i++)
		{
			int foundarb = 0;
		again:
			/*---------let path a-b-c------------*/
			unordered_map<string, string> pathA, pathB, pathC;
			string *res, key[] = {"baseQty"};
			string amt = to_string(amount);

			/*---------path a-b------------*/
			pathA.insert(
				{{"quoteAsset", a[i]}, // asset to sell
				 {"baseAsset", b[i]},  // asset to buy
				 {"quoteQty", amt},	   // quoteAsset
				 {"timeInForce", "GTC"}});
			res = APIcall("GET", "/sapi/v1/bswap/quote", pathA, key, 1, RoundCounter, i);
			string SwapPrice1 = dref(res, 0);

			/*---------path b-c------------*/
			pathB.insert(
				{{"quoteAsset", b[i]},	   // asset to sell
				 {"baseAsset", c[i]},	   // asset to buy
				 {"quoteQty", SwapPrice1}, // quoteAsset
				 {"timeInForce", "GTC"}});
			res = APIcall("GET", "/sapi/v1/bswap/quote", pathB, key, 1, RoundCounter, i);
			string SwapPrice2 = dref(res, 0);

			/*---------path c-a------------*/
			pathC.insert(
				{{"quoteAsset", c[i]},	   // asset to sell
				 {"baseAsset", a[i]},	   // asset to buy
				 {"quoteQty", SwapPrice2}, // quoteAsset
				 {"timeInForce", "GTC"}});
			res = APIcall("GET", "/sapi/v1/bswap/quote", pathC, key, 1, RoundCounter, i);
			string SwapPrice3 = dref(res, 0);

			/*calculation part*/
			double total;
			total = bstod(SwapPrice3); // --swaptoswap with uncertainity of (+-)0.017524065 USDT
			searched = a[i] + "-" + b[i] + "-" + c[i];
			cout << "\t\tgraph is " << searched << endl;
			cout << "\t\ttotal amount is " << total << endl
				 << endl;
			double net = total - amount;
			if (net >= 0) //
			{
				std::thread(arbInitiator, a[i], b[i], c[i], to_string(RoundCounter), to_string(i)).detach(); // exeute order
				cout << "net seen is " << net << endl;
			}
			else if (net > -0.07 && net < 0)
			{
				std::thread(NegarbInitiator, a[i], b[i], c[i], to_string(RoundCounter), to_string(i)).detach(); // exeute order
				cout << "negative net seen is " << net << endl;
			}
		}
		RoundCounter++;
		cout << "\n\n\t\t round----- " << RoundCounter << " ------" << endl;
	}
	cout << "runned the estimation hour of 24...stopped for maintainance or analysis" << endl;
	exit(3);
}

int main()
{
	if (!TestMode) // arb1
	{
		apiKey = "9YpTJujwgAA9HcB167yjhQlXyzOcIqpDLQtJURT617hJvSxwEkKwSnpwmVGY7TfI";
		apiSecret = "IWZkv78zFn1BIene4ukwtewW9n4gVVsdVepREE4mgfCdChylCbwv905fvEjqn6ZQ";
	}
	else // ArbTest
	{
		apiKey = "hj54s3JROd6ueTICoQh2FdE3ezfUiMhbRVoL15yshrNAxSfow5IRwWEBcWMVBn7M";
		apiSecret = "gCs0iTJaeXRSOqKcnXSSY4HDJKdCBic3HlU8HmHN0PZwreNcS1OwshB0763zaW5i";
	}
	ArbFinder();
}
