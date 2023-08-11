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
static string LogFname = "";
string apiKey = "hj54s3JROd6ueTICoQh2FdE3ezfUiMhbRVoL15yshrNAxSfow5IRwWEBcWMVBn7M";
string apiSecret = "gCs0iTJaeXRSOqKcnXSSY4HDJKdCBic3HlU8HmHN0PZwreNcS1OwshB0763zaW5i";

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
    int typeMinMax = 0;
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
        ofstream log(LogFname, ios::app);
        error = true;
        log << "parse error\n"
            << "\n";
        log.close();
    }
    else if (data.is_null()) // is null
    {
        error = true;
        ofstream log(LogFname, ios::app);
        error = true;
        log << "null detected. Reinitializing request...."
            << "\n";
        log.close();
    }

    /*is only square brackets returned ?*/
    else if (message[0] == '[' && message[1] == ']' && handler.typeNULL <= 25)
    {
        error = true;
        ofstream log(LogFname, ios::app);
        error = true;
        log << "empty square bracket detected. Reinitializing request...." << handler.typeNULL
            << "\n";
        log.close();
        handler.typeNULL++;
    }
    else if (handler.typeNULL > 25)
    {
        ofstream log(LogFname, ios::app);
        error = true;
        log << "swap on pending for 19 repeated try. Cannot continue.....exiting"
            << "\n";
        log.close();
        error = true;
        ofstream swa("swap.rew");
        swa << "abnormal";
        swa.close();
        exit(6);
    }
    /* sqaure bracket handler ended here*/
    else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '1' && message[13] == '4') // no data(forbidden)
    {
        error = true;
        ofstream log(LogFname, ios::app);
        error = true;
        log << "binance throuwn forbidden error....exiting"
            << "\n";
        log.close();
        ofstream swa("swap.rew");
        swa << "abnormal";
        swa.close();
        exit(0);
    }

    else if (message[9] == '2' && message[10] == '0' && message[11] == '1' && message[12] == '5') // invalid api or ip
    {
        error = true;
        ofstream log(LogFname, ios::app);
        error = true;
        log << "invalid IP or API key.....exiting"
            << "\n";
        log.close();
        ofstream swa("swap.rew");
        swa << "abnormal";
        swa.close();
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
        ofstream log(LogFname, ios::app);
        log << "exiting....General unkown error of " << message
            << "\n";
        log.close();
        ofstream swa("swap.rew");
        swa << "abnormal";
        swa.close();
        exit(0);
    }

    else if (message[9] == '9' && message[10] == '0' && message[11] == '0' && message[12] == '0')
    { // Enter an amount to {0} decimals.
        error = true;
        ofstream log(LogFname, ios::app);
        log << "Enter an amount to {0} decimals.....exiting"
            << "\n";
        log.close();
        ofstream swa("swap.rew");
        swa << "abnormal";
        swa.close();
        exit(0);
    }
    else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '0' && message[13] == '1')
    { // Below the minimum transaction quantity
        error = true;
        ofstream log(LogFname, ios::app);
        log << "Below the minimum transaction quantity......retrying" << handler.typeMinMax
            << "\n";
        log.close();
        if (handler.typeMinMax > 30)
        {
            /*locking transaction*/
            ofstream end("locked.transaction");
            end << "true";
            end.close();
            /*------------------*/
            ofstream log(LogFname, ios::app);
            log << "critical ....can not fix Below the minimum transaction quantity...transaction locked and exiting -1" << handler.typeMinMax
                << "\n";
            log.close();
            exit(-1);
        }
        handler.typeMinMax++;
    }
    else if (message[9] == '1' && message[10] == '2' && message[11] == '0' && message[12] == '0' && message[13] == '0') // Exceeding the maximum transaction quantity
    {
        ofstream log(LogFname, ios::app);
        error = true;
        log << "Exceeding the maximum transaction quantity.....tring again" << handler.typeMinMax
            << "\n";
        log.close();
        if (handler.typeMinMax > 30)
        {
            /*locking transaction*/
            ofstream end("locked.transaction");
            end << "true";
            end.close();
            /*------------------*/
            ofstream log(LogFname, ios::app);
            log << "critical ....can not fix Below the minimum transaction quantity...transaction locked and exiting -1" << handler.typeMinMax
                << "\n";
            log.close();
            exit(-1);
        }
        handler.typeMinMax++;
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
string *APIcall(string protocol, string path, unordered_map<string, string> par, string key[], int size, int round, int iter, bool NoProcessing = false, string initiator = "collect")
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
        ofstream rew(LogFname, ios::app);
        rew << "dumping round " << initiator << " " << round << " iter " << iter << " " << time << " " << gs_strLastResponse << endl;
        rew.close();
    } while (ErrorHandler(gs_strLastResponse));
    handler.typeMinMax=0;
    TempRes = gs_strLastResponse;
    auto APIresponse = json::parse(gs_strLastResponse);
    handler.typeNULL = 0;
    JsonForm = APIresponse;
    if (NoProcessing)
    {
        return SignedResp;
    }
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
double GetUnclaimedReward() //
{
    // get unclaimed reward data
    unordered_map<string, string> rew;
    string *res, key[] = {"totalUnclaimedRewards"};
    /*---------path a-b------------*/
    rew.insert(
        {});
    res = APIcall("GET", "/sapi/v1/bswap/unclaimedRewards", rew, key, 1, 1, 1, true); // RoundCounter, i
    double reward;
    string temp = JsonForm["totalUnclaimedRewards"]["BNB"];

    if (!temp.empty())
        reward = bstod(temp);
    else
    {
        reward = 0.0;
    }

    return reward;
}

void swapper() // algo efficiency is -- 20 round of 134 iterration in 14 mins
{

    double amount = GetUnclaimedReward();
    if (amount > 0.00042)
    {
        unordered_map<string, string> pathC;
        string *res, key[] = {"success"};
        string amt = to_string(amount);
        pathC.insert(
            {{"type", "0"}});
        res = APIcall("POST", "/sapi/v1/bswap/claimRewards", pathC, key, 1, 0, 0, true, "by swapp"); // 0 round and 0 iteration number is for indicating collecting swap reward.
        if (TempRes[2] == 's' || TempRes[2] == 'u' || TempRes[2] == 'c')
        {
            ofstream swa("swap.rew");
            swa << "true";
            swa.close();
        }
        else
        {
            ofstream log(LogFname, ios::app);
            log << "swap failed"
                << "\n";
            log.close();
            exit(-11);
        }
    }
    else
    {

        ofstream swa("swap.rew");
        swa << "false";
        swa.close();
    }
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
    res = APIcall("POST", "/sapi/v3/asset/getUserAsset", par, key, 1, 1, 1, false);
    balance = dref(res, 0);
    // cout << "balance is " << balance << endl;
    return balance;
}
string ReadSpecificBalance(string asset)
{
    string balance;
    unordered_map<string, string> par;
    string *res;

    /*------read "from" asset------*/
    par.insert(
        {});
    string key[] = {"free"};
    res = APIcall("POST", "/sapi/v3/asset/getUserAsset", par, key, 1, 1, 1, true);
    for (auto const &temp : JsonForm)
    {
        if (JsonForm["asset"] == asset)
        {
            balance = JsonForm["free"];
            break;
        }
    }
    double tempbal = bstod(balance);
    if (balance.empty() || tempbal == 0)
    {
        /*locking transaction*/
        ofstream end("locked.transaction");
        end << "true";
        end.close();
        /*------------------*/
        ofstream log(LogFname, ios::app);
        log << "critical condition!! can not read balance for balance reverser function.transaction is locked and exiting with -1...."
            << "\n";
        log.close();
        exit(-1);
    }
    return balance;
}
void BalanceRevereser(string asset, string ArbType, string round, string iter)
{
    unordered_map<string, string> par;
    string *res;
    string key[5];
    string FromBalance = ReadSpecificBalance(asset);
    string initiator = "reverser by " + ArbType;
    int rou = stod(round);
    int ite = stod(iter);

    // swap order
    if (bstod(FromBalance) > 0)
    {
        // cout << "qauantity is " << FromBalance << "\n";
        key[0] = {"swapId"};
        par.insert(
            {{"quoteAsset", asset},     // asset to sell
             {"baseAsset", "USDT"},     // asset to buy
             {"quoteQty", FromBalance}, // quoteAsset
             {"timeInForce", "GTC"}});
        res = APIcall("POST", "/sapi/v1/bswap/swap", par, key, 1, rou, ite, false, initiator);

        string SwapRes = TempRes;
        // cout<<"the string being checked is"<<SwapRes<<endl;
        if (SwapRes[2] == 's' && SwapRes[3] == 'w' && SwapRes[4] == 'a' && SwapRes[5] == 'p' && SwapRes[6] == 'I' && SwapRes[7] == 'd')
        {
            ofstream log(LogFname, ios::app);
            log << "reversed!!"
                << "\n";
            log.close();
            // cout << "yaa contains swap id" << endl;
        }
        else
        {
            ofstream log(LogFname, ios::app);
            log << "Failed to reverse!!"
                << "\n";
            log.close();
            ofstream mes("emergency.message", ios::app);
            mes << "failed to reverse balance from " << asset << " to USDT. Check and take action fastly";
            mes.close();
            // cout << "no swap id" << endl;
        }
    }
    else
    {
        ofstream log(LogFname, ios::app);
        log << "asset amount is zero.....waiting to be unfreezed....."
            << "\n";
        log.close();
    }
}

int main(int argc, char **argv)
{
    /*for time log*/
    time_t t;
    struct tm *tt;
    /*time getter*/
    time(&t);
    tt = localtime(&t);
    string time = asctime(tt);

    if (argc == 1)
    {
        LogFname = "./run_log_RAE/rae_BNB reward_ 0 0 _" + time + ".txt";
        swapper();
    }

    else
    {
        string initiator = argv[2];
        string roundC = argv[3];
        string iterC = argv[4];
        LogFname = "./run_log_RAE/rae_" + initiator + "_" + roundC + " " + iterC + "_" + time + ".txt";
        BalanceRevereser(argv[1], argv[2], argv[3], argv[4]);
    }
}