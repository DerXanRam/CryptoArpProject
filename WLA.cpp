// Written on ubuntu environment
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <string.h>
#include <chrono>
#include <openssl/hmac.h>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <unistd.h>

using namespace std;
using json = nlohmann::json;
using namespace std::chrono;

string baseUrl = "https://api.binance.com";
const string apiKey = "9YpTJujwgAA9HcB167yjhQlXyzOcIqpDLQtJURT617hJvSxwEkKwSnpwmVGY7TfI";
const string apiSecret = "IWZkv78zFn1BIene4ukwtewW9n4gVVsdVepREE4mgfCdChylCbwv905fvEjqn6ZQ";

static string gs_strLastResponse, str4;
json JsonForm;
string TempRes;

/*This is our callback function which will be executed by curl on perform.
here we will copy the data received to our struct
ptr : points to the data received by curl.
size : is the count of data items received.
nmemb : number of bytes
data : our pointer to hold the data.
*/

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

	else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '1' && message[13] == '4') // no data(forbidden)
	{
		error = true;
		cout << "binance throuwn forbidden error....exiting" << endl;
		exit(0);
	}
	else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '0' && message[13] == '1') // below min transaction
	{
		error = true;
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
	else
	{
		error = false;
	}

	return error;
}
string SignedResp[5];
string UnsignedResp[5];
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

	do
	{
		sendSignedRequest(curl, protocol, path, par);
		// cout << " size is " << gs_strLastResponse.size() << endl;
		cout << "dumping round "<<round<<" iter "<<iter<<" "<< gs_strLastResponse << endl;
	} while (ErrorHandler(gs_strLastResponse));
	TempRes = gs_strLastResponse;
	auto APIresponse = json::parse(gs_strLastResponse);

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
	gs_strLastResponse.clear();
	return SignedResp;
}

string *PublicAPI(string path, unordered_map<string, string> par, string key[], int size)
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

	do
	{
		sendPublicRequest(curl, path, par);
		// cout << "dumping  " << gs_strLastResponse << endl;
	} while (ErrorHandler(gs_strLastResponse));
	auto UnResp = json::parse(gs_strLastResponse);
	JsonForm = UnResp;

	if (UnResp.is_array()) // for array response
	{

		// cout << " yes it is array\n"<<endl;
		for (int i = 0; i < NumOfKey; i++)
		{
			if (UnResp[0][key[i]].is_number())
			{
				auto p = UnResp[0][key[i]].get<int>();
				int TempNum = p;
				UnsignedResp[i] = to_string(TempNum);
			}
			else
			{
				UnsignedResp[i] = UnResp[0][key[i]];
			}
		}
	}
	else if (!UnResp.is_array()) // for non-array reponse
	{
		// cout << "" no array\n"<<endl;
		for (int i = 0; i < NumOfKey; i++)
		{

			if (UnResp[key[i]].is_number())
			{
				auto p = UnResp[key[i]].get<int>();
				int TempNum = p;
				UnsignedResp[i] = to_string(TempNum);
			}
			else
			{
				UnsignedResp[i] = UnResp[key[i]];
			}
		}
	}

	/* always cleanup */
	curl_easy_cleanup(curl);
	curl_global_cleanup();

	return UnsignedResp;
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
	res = APIcall("POST", "/sapi/v3/asset/getUserAsset", par, key,1,1,1);
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
	res = APIcall("GET", "/sapi/v1/bswap/pools", par, key, 1,1,1);

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
	if (stod(FromBalance) > 0)
	{
		// cout << "qauantity is " << FromBalance << "\n";
		key[0] = {"swapId"};
		par.insert(
			{{"quoteAsset", from},		// asset to sell
			 {"baseAsset", to},			// asset to buy
			 {"quoteQty", FromBalance}, // quoteAsset
			 {"timeInForce", "GTC"}});
		res = APIcall("POST", "/sapi/v1/bswap/swap", par, key, 1,1,1);
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
	cout << "\n\n\t\tversion 1.3 min 0.18 only 7 orders\n"
		 << endl;
	string searched;
	ifstream command;
	int RoundCounter = 1;
	double amount = 50;
	int OrderExe = 0;

	while (RoundCounter < 1500)
	{
		for (int i = 0; i <= 132; i++)
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
			res = APIcall("GET", "/sapi/v1/bswap/quote", pathA, key, 1,RoundCounter,i);
			string SwapPrice1 = dref(res, 0);

			/*---------path b-c------------*/
			pathB.insert(
				{{"quoteAsset", b[i]},	   // asset to sell
				 {"baseAsset", c[i]},	   // asset to buy
				 {"quoteQty", SwapPrice1}, // quoteAsset
				 {"timeInForce", "GTC"}});
			res = APIcall("GET", "/sapi/v1/bswap/quote", pathB, key, 1,RoundCounter,i);
			string SwapPrice2 = dref(res, 0);

			/*---------path c-a------------*/
			pathC.insert(
				{{"quoteAsset", c[i]},	   // asset to sell
				 {"baseAsset", a[i]},	   // asset to buy
				 {"quoteQty", SwapPrice2}, // quoteAsset
				 {"timeInForce", "GTC"}});
			res = APIcall("GET", "/sapi/v1/bswap/quote", pathC, key, 1,RoundCounter,i);
			string SwapPrice3 = dref(res, 0);

			/*calculation part*/
			double total;
			total = stod(SwapPrice3); // --swaptoswap with uncertainity of (+-)0.017524065 USDT
            searched = a[i] + "-" + b[i] + "-" + c[i];
			cout << "\t\tgraph is " << searched << endl<< endl;
			double net = total - amount;
			if (net > 0)
			{
				if (foundarb > 1)
				{
					if (order(a[i], b[i]))
					{
						usleep(2500000);
						if (order(b[i], c[i]))
						{
							usleep(2500000);
							if (order(c[i], a[i]))
							{
								cout << "\nall succss" << endl;
								OrderExe++;
								cout<<"no of exe orders are "<<OrderExe<<endl;
							}
							else
							{
								cout << "failed at third" << endl;
							}
						}
						else
						{
							cout << "failed at sec" << endl;
						}
						usleep(2100000);
					}
					else
					{
						cout << "failed at first" << endl;
					}
				}
				cout << "\t\tnet is " << net << "\n\n";
				foundarb++;
				goto again;
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
	ArbFinder();
}
